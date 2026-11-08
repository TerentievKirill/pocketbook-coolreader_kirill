#!/usr/bin/env python3
"""Build, verify and package one explicit PocketBook target on Linux."""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
from targets import DEFAULT_TARGET, ROOT, load_target, sdk_path, tool, firmware_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', default=DEFAULT_TARGET)
    args = parser.parse_args()
    try:
        target = load_target(args.target)
    except ValueError as error:
        parser.error(str(error))
    revision = os.environ.get('CR3_REVISION', 'dev')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', revision):
        parser.error('CR3_REVISION must be a filename-safe identifier')
    os.chdir(ROOT)
    build = Path(os.environ.get('CR3_BUILD_DIR', ROOT / target['build']['directory'])).resolve()
    sdk = sdk_path(target).resolve()
    os.environ['POCKETBOOK_SDK'] = str(sdk)
    for key, value in {'CR3_TOOLCHAIN_PREFIX': target['sdk']['prefix'],
                       'CR3_SYSROOT': target['sdk']['sysroot'],
                       'CR3_PROCESSOR': target['sdk']['processor'],
                       'CR3_C_FLAGS': shlex.join(target['sdk']['c_flags']),
                       'CR3_CXX_FLAGS': shlex.join(target['sdk']['cxx_flags'])}.items():
        os.environ[key] = value
    host = ROOT / 'work/host/usr'
    if (host / 'bin').is_dir():
        os.environ['PATH'] = str(host / 'bin') + os.pathsep + os.environ['PATH']
        os.environ['LD_LIBRARY_PATH'] = str(host / 'lib/x86_64-linux-gnu') + (
            os.pathsep + os.environ['LD_LIBRARY_PATH'] if os.environ.get('LD_LIBRARY_PATH') else '')
    host_zip = shutil.which('zip')
    if not host_zip:
        parser.error('Install the host zip utility')
    build.mkdir(parents=True, exist_ok=True)
    # Prevent incremental objects from another profile or SDK being reused.
    from targets import target_evidence
    import json
    identity = {'target': target_evidence(target), 'sdk': str(sdk)}
    marker = build / 'build-target.json'
    if marker.exists() and json.loads(marker.read_text()) != identity:
        parser.error('Build directory belongs to a different target/profile/SDK; choose a new CR3_BUILD_DIR')
    cache = build / 'CMakeCache.txt'
    if cache.exists() and not marker.exists():
        parser.error('Build directory predates target profiles; choose a new CR3_BUILD_DIR or remove its old build cache')
    marker.write_text(json.dumps(identity, indent=2) + '\n')

    def run(command):
        subprocess.run([str(part) for part in command], check=True)

    run([sys.executable, ROOT / 'tools/test-targets.py'])
    run([sys.executable, ROOT / 'tools/test-ota-launcher.py'])
    run([sys.executable, ROOT / 'tools/test-shared-launcher.py'])
    run([sys.executable, ROOT / 'tools/test-runtime-entry.py'])
    run([sys.executable, ROOT / 'tools/test-shared-package.py'])
    run([sys.executable, ROOT / 'tools/test-firmware-qualification.py'])
    run(['c++', '-std=c++11', ROOT / 'tests/ota-version.cpp', '-o', build / 'ota-version-host'])
    run([build / 'ota-version-host'])
    configure = ['cmake', '-S', ROOT, '-B', build,
                 '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'tools/toolchain-pocketbook.cmake'),
                 '-DZIP_EXECUTABLE=' + host_zip, '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
                 '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_SKIP_RPATH=ON',
                 '-DCMAKE_C_FLAGS_RELEASE=-O2 -g -DNDEBUG',
                 '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -g -DNDEBUG -fno-strict-aliasing -fno-delete-null-pointer-checks',
                 '-DCR3_RELEASE_VERSION=' + os.environ.get('CR3_RELEASE_VERSION', '1.0.5'),
                 '-DCR3_DEFAULTS_FILE=' + str(ROOT / target['package']['assets'] / 'cr3-defaults.ini')]
    configure += ['-D' + key + '=' + value for key, value in target['build']['cmake'].items()]
    run(configure)
    run(['cmake', '--build', build, '--parallel', os.environ.get('JOBS', str(os.cpu_count() or 1))])
    binary = build / 'cr3gui/cr3-pb.app'
    debug = build / 'cr3gui/cr3-pb.debug'
    staged = build / 'package/cr3-pb.app'
    staged.parent.mkdir(exist_ok=True)
    sections = subprocess.check_output([str(tool(target, 'readelf')), '-S', str(binary)], text=True)
    if '.debug_info' not in sections:
        raise SystemExit('Missing debug information; relink the application')
    run([tool(target, 'objcopy'), '--only-keep-debug', binary, debug])
    shutil.copy2(binary, staged)
    run([tool(target, 'strip'), staged])
    run([tool(target, 'objcopy'), '--add-gnu-debuglink=' + str(debug), staged])
    run([sys.executable, ROOT / 'tools/test-touch-hd3.py', '--target', target['id'], '--build', build, '--application', staged])
    run([sys.executable, ROOT / 'tools/test-build-metadata.py'])
    qualification = Path(os.environ.get('CR3_QUALIFICATION_DIR', ROOT / 'work/qualification'))
    for device in target.get('qualification_candidates', []):
        run([sys.executable, ROOT / 'tools/qualify-pocketbook-firmware.py',
             '--device', device, '--archives', os.environ.get('CR3_ARCHIVE_DIR', str(ROOT / 'work/downloads')),
             '--output', qualification / device, '--baseline-build', build,
             '--baseline-firmware', firmware_path(target), '--readelf', tool(target, 'readelf')])
        reports = build / 'firmware-qualification'
        reports.mkdir(exist_ok=True)
        shutil.copy2(qualification / device / 'qualification.json', reports / (device + '.json'))
    run([sys.executable, ROOT / 'tools/package-pocketbook.py', build, '--target', target['id'], '--binary', staged, '--revision', revision])


if __name__ == '__main__':
    main()
