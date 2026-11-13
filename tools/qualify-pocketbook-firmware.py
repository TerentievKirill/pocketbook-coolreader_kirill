#!/usr/bin/env python3
"""Inspect pinned firmware and probe an existing build without enabling a device target.

Run inside the Linux build image. Proprietary extracted files stay in the local
output directory; the JSON report contains hashes, inventories and test results.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile
from runtime_compatibility import check_runtime, elf_identity
from runtime_startup import probe_startup
from targets import ROOT, load_target


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def embedded_version(data):
    # Vendor fwinfo.xml embeds unescaped QML in <description>; the complete
    # document is not necessarily valid XML. Read only its version field.
    prefix = data.split(b'<description', 1)[0]
    versions = re.findall(rb'<version>\s*(U[0-9A-Za-z_.]+)\s*</version>', prefix)
    if len(versions) != 1:
        raise ValueError('Missing or ambiguous embedded firmware version')
    return versions[0].decode('ascii')


def run(command, timeout=60, env=None):
    try:
        result = subprocess.run([str(x) for x in command], capture_output=True,
                                text=True, errors='replace', timeout=timeout, env=env)
        return {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
    except subprocess.TimeoutExpired as error:
        def decode(value):
            return value.decode(errors='replace') if isinstance(value, bytes) else value or ''
        return {'exit_code': None, 'timeout': True,
                'stdout': decode(error.stdout), 'stderr': decode(error.stderr)}


def checked(command, log):
    result = run(command, timeout=300)
    log.write_text(result['stdout'] + result['stderr'])
    if result['exit_code'] != 0:
        raise ValueError('Extraction failed; see ' + str(log))


def extract(device, archive_dir, output):
    archive = archive_dir / device['archive']['filename']
    if sha256(archive) != device['archive']['sha256']:
        raise ValueError('Firmware archive checksum mismatch')
    marker = output / 'extracted.json'
    identity = {'archive_sha256': device['archive']['sha256'], 'extractor_schema': 1}
    if marker.exists():
        if json.loads(marker.read_text()) != identity:
            raise ValueError('Output directory contains another firmware; use a new directory')
        return output / 'runtime', output / 'ebrmain/cramfs'
    raw = output / 'SWUPDATE.BIN'
    with zipfile.ZipFile(archive) as source:
        fwinfo = source.read('fwinfo.xml')
        if embedded_version(fwinfo) != device['software_version']:
            raise ValueError('Embedded firmware version mismatch')
        (output / 'fwinfo.xml').write_bytes(fwinfo)
        if not raw.exists():
            with source.open('SWUPDATE.BIN') as src, raw.open('wb') as dst:
                shutil.copyfileobj(src, dst)
    with raw.open('rb') as stream:
        header = stream.read(1024)
        model = b'PocketBook' + device['model'].encode()
        if header[:16] != b'PocketBookUpdate' or header[16:48].split(b'\0')[0] != model:
            raise ValueError('Firmware header model mismatch')
        sections = {kind: (offset, size) for kind, _, offset, size in struct.iter_unpack('<4I', header[256:])}
        for kind, name in ((0x72, 'rootfs'), (0x65, 'ebrmain')):
            offset, size = sections[kind]
            if not size or offset + 1024 + size > raw.stat().st_size:
                raise ValueError('Invalid firmware section bounds')
            image = output / (name + '.img')
            if image.exists():
                continue
            compressed = output / (name + '.gz')
            stream.seek(offset + 1024)
            with compressed.open('wb') as dst:
                remaining = size
                while remaining:
                    chunk = stream.read(min(remaining, 1024 * 1024))
                    if not chunk:
                        raise ValueError('Truncated firmware section')
                    dst.write(chunk)
                    remaining -= len(chunk)
            temporary = image.with_suffix('.img.part')
            with gzip.open(compressed, 'rb') as src, temporary.open('wb') as dst:
                shutil.copyfileobj(src, dst)
            temporary.replace(image)
    runtime = output / 'runtime'
    ebrmain = output / 'ebrmain'
    runtime.mkdir(exist_ok=True)
    ebrmain.mkdir(exist_ok=True)
    checked(['debugfs', '-R', 'rdump /lib ' + str(runtime), output / 'rootfs.img'], output / 'rootfs-extract.log')
    checked(['debugfs', '-R', 'dump /cramfs.img ' + str(ebrmain / 'cramfs.img'), output / 'ebrmain.img'], output / 'ebrmain-extract.log')
    if not (ebrmain / 'cramfs.img').is_file():
        raise ValueError('Unsupported firmware layout: /cramfs.img not found')
    tree = ebrmain / 'cramfs'
    if not tree.exists():
        checked(['unsquashfs', '-no-progress', '-processors', '2', '-d', tree, ebrmain / 'cramfs.img'], output / 'squashfs-extract.log')
    shutil.copytree(tree / 'lib', runtime / 'lib', dirs_exist_ok=True, symlinks=True)
    listing = run(['debugfs', '-R', 'ls -l /lib', output / 'ebrmain.img'])['stdout']
    for line in listing.splitlines():
        fields = line.split()
        if len(fields) < 9 or fields[1] != '120777':
            continue
        name = fields[-1]
        details = run(['debugfs', '-R', 'stat /lib/' + name, output / 'ebrmain.img'])['stdout']
        link = re.search(r'Fast link dest: "([^"]+)"', details)
        if link:
            candidate = runtime / 'lib' / Path(link[1]).name
            alias = runtime / 'lib' / name
            if candidate.is_file() and not alias.exists():
                alias.unlink(missing_ok=True)
                alias.symlink_to(candidate.name)
    marker.write_text(json.dumps(identity, indent=2) + '\n')
    return runtime, tree


def symbols(readelf, path, undefined=False):
    result = run([readelf, '--dyn-syms', '--wide', path])
    if result['exit_code'] != 0:
        raise ValueError('Cannot inspect symbols: ' + str(path))
    names = set()
    for line in result['stdout'].splitlines():
        fields = line.split()
        if len(fields) < 8 or not fields[0].rstrip(':').isdigit():
            continue
        if undefined:
            selected = fields[6] == 'UND' and fields[4] == 'GLOBAL'
        else:
            selected = fields[6] != 'UND' and fields[4] in ('GLOBAL', 'WEAK')
        if selected:
            names.add(fields[7].split('@', 1)[0])
    return names


def probe_regressions(build, runtime, tree, output, qemu, profile):
    """Reuse baseline executables; these are software probes, not device tests."""
    fixtures = build / 'arm-regressions'
    scratch = output / 'regressions'
    scratch.mkdir(exist_ok=True)
    for pattern in ('*.zip', '*.fb2', '*.epub'):
        for path in fixtures.glob(pattern):
            shutil.copy2(path, scratch / path.name)
    source = Path(__file__).resolve().parent.parent
    fonts = tree / 'fonts'
    regular = fonts / 'DejaVuSans.ttf'
    bold = fonts / 'DejaVuSans-Bold.ttf'
    cases = [
        ('reference-initialization', []), ('css-null-ancestor', []),
        ('dependency-compatibility', [scratch]), ('dictionary-safety', []),
        ('history-persistence', [scratch]), ('archive-boundaries', [scratch]),
        ('atomic-faults', [scratch]),
        ('reading-state-migration', [regular, scratch / 'migration.fb2', scratch / 'migration.zip']),
        ('document-render', [regular, bold, fonts / 'DroidSans.ttf',
                             source / profile['package']['assets'] / 'cr3-defaults.ini', source / 'cr3gui/data/hyph',
                             scratch / 'regression-章节.epub', source / 'cr3gui/data/fb2.css',
                              scratch / 'flibusta-footnotes-regression.epub', source / 'cr3gui/data/epub.css',
                              scratch / 'large-grouped-footnotes.epub']),
        ('skin-language-symbol', [regular, bold, build / 'cr3gui/pb631fw5.cr3skin', scratch]),
        ('frontlight-status', [regular, scratch]), ('standby-frontlight', []),
    ]
    if (fixtures / 'shared-device-profiles').is_file():
        cases.append(('shared-device-profiles', [regular, source / 'data/pocketbook-armv7/cr3-defaults.ini']))
    results = []
    for name, arguments in cases:
        binary = fixtures / name
        result = run([qemu, '-L', runtime, '-E', 'LD_LIBRARY_PATH=' + str(runtime / 'lib'),
                      binary, *arguments], timeout=45)
        (scratch / (name + '.log')).write_text(result['stdout'] + result['stderr'])
        results.append({'name': name, 'status': 'passed' if result['exit_code'] == 0 else 'failed',
                        'exit_code': result['exit_code'], 'binary_sha256': sha256(binary),
                        'log': result['stdout'][-1500:] + result['stderr'][-1500:]})
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=Path(__file__).resolve().parent.parent / 'config/firmware-candidates.json')
    parser.add_argument('--device', required=True)
    parser.add_argument('--archives', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline-build', type=Path, required=True)
    parser.add_argument('--baseline-firmware', type=Path, required=True)
    parser.add_argument('--readelf', type=Path, required=True)
    parser.add_argument('--qemu', default='qemu-arm')
    args = parser.parse_args()
    device = next((x for x in json.loads(args.manifest.read_text())['devices'] if x['id'] == args.device), None)
    if not device:
        parser.error('Unknown firmware candidate')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    print('Extracting ' + device['id'] + ' ' + device['version'], flush=True)
    runtime, tree = extract(device, args.archives, output)
    loaders = sorted((path for path in (runtime / 'lib').glob('ld*.so*')
                      if path.is_file() and 'linux' in path.name),
                     key=lambda path: (path.name != 'ld-linux.so.3', path.name))
    if not loaders:
        raise ValueError('No supported Linux loader found')
    loader = loaders[0]
    identity = elf_identity(loader)
    firmware = {'elf_class': identity['class'], 'elf_machine': identity['machine'],
                'float_abi': identity['float_abi'], 'loader': 'lib/' + loader.name}
    application = args.baseline_build / 'package/cr3-pb.app'
    baseline_report = json.loads((args.baseline_build / 'test-results.json').read_text())
    profile = load_target(baseline_report['target']['id'])
    if sha256(application) != baseline_report['binary_sha256']:
        raise ValueError('Baseline binary does not match its regression report')
    readelf = args.readelf
    report = {'schema': 1, 'device': device, 'baseline_binary_sha256': sha256(application),
              'inspection_tools_sha256': {
                  name: sha256(Path(__file__).with_name(name))
                  for name in ('qualify-pocketbook-firmware.py', 'runtime_compatibility.py', 'runtime_startup.py')},
              'embedded_version': embedded_version((output / 'fwinfo.xml').read_bytes()),
              'baseline_validation': baseline_report, 'firmware_abi': firmware,
              'device_validation': 'not-performed',
              'limits': ['CPU instructions and InkView structure layouts need separate review',
                         'Software UI/defaults and mocked hardware tests do not validate physical controls',
                         'HTTPS transfer and device-specific OTA are not covered by these probes']}
    if report['embedded_version'] != device['software_version']:
        raise ValueError('Embedded firmware version mismatch')
    report['loader_version'] = run([args.qemu, '-L', runtime, loader, '--version'])
    report['libc_version'] = run([args.qemu, '-L', runtime, '-E', 'LD_LIBRARY_PATH=' + str(runtime / 'lib'), runtime / 'lib/libc.so.6'])
    inkview = runtime / 'lib/libinkview.so'
    report['arm_attributes'] = {
        'application': run([readelf, '-A', application])['stdout'],
        'firmware_inkview': run([readelf, '-A', inkview])['stdout']}
    baseline_inkview = args.baseline_firmware / 'runtime/lib/libinkview.so'
    imports = symbols(readelf, application, undefined=True) & symbols(readelf, baseline_inkview)
    missing = imports - symbols(readelf, inkview)
    report['inkview_imports'] = {'required': sorted(imports), 'missing': sorted(missing),
                                 'status': 'passed' if not missing else 'failed',
                                 'library_sha256': sha256(inkview)}
    for name in ('libc.so.6', 'libstdc++.so.6', 'libinkview.so'):
        data = run([readelf, '--version-info', '--wide', runtime / 'lib' / name])['stdout']
        report.setdefault('library_versions', {})[name] = sorted(set(re.findall(r'Name: ((?:GLIBC|GLIBCXX|CXXABI)_[0-9.]+)', data)))
    try:
        report['runtime_compatibility'] = check_runtime(application, runtime, readelf, firmware)
    except (ValueError, subprocess.CalledProcessError) as error:
        report['runtime_compatibility'] = {'status': 'failed', 'error': str(error)}
    env = dict(os.environ, LC_ALL='C')
    # Trace through the application's own PT_INTERP. Executing ld.so --list
    # directly under this QEMU build can SIGILL even when normal linking works.
    report['loader_resolution'] = run([args.qemu, '-L', runtime, '-E', 'LD_LIBRARY_PATH=' + str(runtime / 'lib'),
                                       '-E', 'LD_TRACE_LOADED_OBJECTS=1', application], env=env)
    startup = probe_startup(application, runtime, args.qemu, output,
                            ROOT / profile['package']['assets'] / 'Welcome.fb2',
                            ROOT / profile['package']['assets'] / 'runtime-entry.app'
                            if profile.get('device_registry') else None)
    (output / 'startup.log').write_text(startup['stdout'] + startup['stderr'])
    startup['control_transferred'] = 'transferring control:' in startup['stderr']
    startup['stdout'] = startup['stdout'][-2000:]
    startup['stderr'] = startup['stderr'][-3000:]
    report['startup_probe'] = startup
    report['regression_probes'] = probe_regressions(args.baseline_build, runtime, tree, output, args.qemu, profile)
    report['status'] = 'laboratory-checks-passed' if (
        report['runtime_compatibility']['status'] == 'passed' and not missing
        and report['loader_resolution']['exit_code'] == 0
        and report['startup_probe']['control_transferred']
        and all(x['status'] == 'passed' for x in report['regression_probes'])) else 'incompatible-or-incomplete'
    (output / 'qualification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(device['id'] + ': ' + report['status'], flush=True)


if __name__ == '__main__':
    main()
