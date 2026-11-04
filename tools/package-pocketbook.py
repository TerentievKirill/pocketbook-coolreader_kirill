#!/usr/bin/env python3
"""Package a verified target with shared assets and its declared defaults."""
from pathlib import Path
import argparse
import re
import hashlib
import json
import shutil
import sys
import subprocess
import zipfile
from build_metadata import source_metadata, sha256, verified_report, verified_firmware_reports
from targets import DEFAULT_TARGET, load_target, tool, target_evidence
from package_layout import validate_reader_layout

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('--target', default=DEFAULT_TARGET)
parser.add_argument('--revision', required=True, help='Explicit revision, e.g. dev or 6')
parser.add_argument('--binary', type=Path)
parser.add_argument('--dist-dir', type=Path)
args = parser.parse_args()
if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.revision):
    parser.error('Revision must be a filename-safe identifier')
try:
    target = load_target(args.target)
except ValueError as error:
    parser.error(str(error))
package = target['package']
root = Path(__file__).resolve().parent.parent
profile_assets = root / package['assets']
build = args.build.resolve()
binary = (args.binary or build / 'package/cr3-pb.app').resolve()
report = verified_report(json.loads((build / 'test-results.json').read_text()), binary, root, build, target)
firmware_reports = verified_firmware_reports(binary, root, build, target, report)
stage = root / ('work/package-' + target['id'] + '-' + args.revision)
if stage.exists():
    shutil.rmtree(stage)
share = stage / 'system/share/cr3'
share.mkdir(parents=True)
assets = root / 'cr3gui/data'
for stylesheet in sorted(assets.glob('*.css')):
    shutil.copy2(stylesheet, share)
for directory in ('hyph', 'chn', 'manual'):
    shutil.copytree(assets / directory, share / directory)
shutil.copy2(profile_assets / 'devices.ini', share)
shutil.copy2(profile_assets / 'cr3-defaults.ini', share)
for name in ('bin', 'skins', 'i18n', 'keymaps'):
    (share / name).mkdir(exist_ok=True)
shutil.copy2(binary, share / 'bin/cr3-pb.app')
for skin in package['skins']:
    shutil.copy2(build / f'cr3gui/{skin}.cr3skin', share / f'skins/{skin}.cr3skin')
for mo in sorted((build / 'i18n').glob('*/LC_MESSAGES/cr3.mo')):
    shutil.copy2(mo, share / f'i18n/{mo.parent.parent.name}.mo')
if not list((share / 'i18n').glob('*.mo')):
    raise SystemExit('No compiled translations found')
shutil.copytree(root / 'cr3gui/data/devices' / package['keymaps'] / 'keymaps', share / 'keymaps', dirs_exist_ok=True)
launcher = stage / 'system/bin/cr3-pb.app'
launcher.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(profile_assets / 'cr3-pb.app', launcher)
apps = stage / 'applications'
apps.mkdir(exist_ok=True)
shutil.copy2(profile_assets / 'CoolReader.app', apps / 'CoolReader.app')
shutil.copy2(profile_assets / 'Welcome.fb2', share / 'Welcome.fb2')
launcher.chmod(0o755)
dependencies = json.loads((root / 'thirdparty/versions.json').read_text())
licenses = share / 'licenses'
licenses.mkdir()
shutil.copy2(root / 'LICENSE', licenses / 'COPYING')
shutil.copy2(root / 'thirdparty/README.txt', licenses / 'README.txt')
shutil.copy2(root / 'thirdparty/versions.json', licenses / 'versions.json')
(licenses / 'crengine').mkdir()
shutil.copy2(root / 'crengine/xxhash-LICENSE.txt', licenses / 'crengine/xxhash-LICENSE.txt')
for dependency in dependencies['bundled']:
    for name in dependency['licenses']:
        destination = licenses / dependency['directory'] / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / 'thirdparty' / dependency['directory'] / name, destination)

documentation = share / 'doc'
documentation.mkdir()
metadata = share / 'metadata'
metadata.mkdir()
(documentation / Path(package['guide']).name).write_text(
    f"Candidate revision: {args.revision}\nBinary SHA-256: {sha256(binary)}\n"
    "Automated ARM tests passed. This candidate has NOT been tested on a physical reader.\n"
    "Device notes in the guide describe earlier builds only.\n\n"
    + (root / package['guide']).read_text())
# This is intentionally a template: never overwrite a reader's custom config.
shutil.copy2(profile_assets / 'extensions.cfg.example', documentation / 'extensions.cfg.example')
compiler = tool(target, 'g++')
manifest = {
    'application': package['application'],
    'device': package['device'],
    'target_firmware': target['firmware']['version'],
    'target': target_evidence(target),
    'target_profile': target,
    'revision': args.revision,
    'release_version': next(line.split('=', 1)[1] for line in (build / 'CMakeCache.txt').read_text().splitlines() if line.startswith('CR3_RELEASE_VERSION:STRING=')),
    'source': source_metadata(root),
    'dependencies': dependencies,
    'engine': json.loads((root / 'crengine/UPSTREAM.json').read_text()),
    'compiler': subprocess.check_output([str(compiler), '--version'], text=True).splitlines()[0],
    'build_flags': (build / 'crengine/CMakeFiles/crengine.dir/flags.make').read_text(),
    'binary_sha256': sha256(binary),
    'automated_validation': report,
    'device_validation': {'status': 'not-performed', 'binary_sha256': sha256(binary)},
}
if target.get('device_registry'):
    registry = json.loads((root / target['device_registry']).read_text())
    # Keep primary validation separate from the shared package's compatibility
    # list; PB632-only publishing tools must not mistake this for their package.
    manifest['validation_firmware'] = manifest.pop('target_firmware')
    manifest['qualified_firmware'] = [device['firmware'] for device in registry['devices']]
    manifest['compatibility'] = registry
    manifest['ota'] = {'family': registry['family'], 'channel': 'ota/' + registry['family'] + '.txt',
                       'package': 'coolreader-pocketbook-armv7-softfp.zip',
                       'downloads': 'downloads/coolreader-shared/', 'schema': 'CR3-OTA-SHARED-1'}
    (share / 'package-family.txt').write_text(registry['family'] + '\n')
    (share / 'firmware-compatibility.txt').write_text(''.join(
        device['model'] + ' ' + device['firmware'] + '\n' for device in registry['devices']))
    manifest['firmware_qualification'] = firmware_reports
    shutil.copy2(root / target['device_registry'], metadata / 'device-profiles.json')
    evidence = metadata / 'firmware-qualification'
    evidence.mkdir()
    for name, evidence_report in firmware_reports.items():
        (evidence / (name + '.json')).write_text(json.dumps(evidence_report, indent=2) + '\n')
subprocess.run([sys.executable, str(root / 'tools/test-package-assets.py'), str(stage), '--target', target['id']], check=True)
manifest['package_asset_validation'] = 'passed'
(share / 'release-version.txt').write_text(manifest['release_version'] + '\n')
(metadata / 'build-info.json').write_text(json.dumps(manifest, indent=2) + '\n')
files = sorted(p for p in stage.rglob('*') if p.is_file())
validate_reader_layout(p.relative_to(stage).as_posix() for p in files)
dest = args.dist_dir or root / 'dist'
dest.mkdir(exist_ok=True)
archive = dest / (package['artifact_prefix'] + '-' + args.revision + '.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for p in files:
        z.write(p, p.relative_to(stage))
checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
(archive.with_suffix('.zip.sha256')).write_text(f'{checksum}  {archive.name}\n')
print(archive)
print(checksum)
