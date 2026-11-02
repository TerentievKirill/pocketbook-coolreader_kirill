#!/usr/bin/env python3
"""Package the FW6 binary with upstream data without modifying source assets."""
from pathlib import Path
import argparse
import re
import os
import hashlib
import json
import shutil
import sys
import subprocess
import zipfile
from build_metadata import source_metadata, sha256, verified_report

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('--revision', required=True, help='Explicit revision, e.g. dev or 6')
parser.add_argument('--binary', type=Path)
parser.add_argument('--dist-dir', type=Path)
args = parser.parse_args()
if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.revision):
    parser.error('Revision must be a filename-safe identifier')
root = Path(__file__).resolve().parent.parent
build = args.build.resolve()
binary = (args.binary or build / 'package/cr3-pb.app').resolve()
report = verified_report(json.loads((build / 'test-results.json').read_text()), binary, root, build)
stage = root / ('work/package-touch-hd3-' + args.revision)
if stage.exists():
    shutil.rmtree(stage)
share = stage / 'system/share/cr3'
share.mkdir(parents=True)
assets = root / 'cr3gui/data'
for stylesheet in sorted(assets.glob('*.css')):
    shutil.copy2(stylesheet, share)
for directory in ('hyph', 'chn', 'manual'):
    shutil.copytree(assets / directory, share / directory)
shutil.copy2(root / 'data/touch-hd3/devices.ini', share)
shutil.copy2(root / 'data/touch-hd3/cr3-defaults.ini', share)
for name in ('bin', 'skins', 'i18n', 'keymaps'):
    (share / name).mkdir(exist_ok=True)
shutil.copy2(binary, share / 'bin/cr3-pb.app')
for skin in ('default', 'pb62x', 'pb626fw5', 'pb631fw5'):
    shutil.copy2(build / f'cr3gui/{skin}.cr3skin', share / f'skins/{skin}.cr3skin')
for mo in sorted((build / 'i18n').glob('*/LC_MESSAGES/cr3.mo')):
    shutil.copy2(mo, share / f'i18n/{mo.parent.parent.name}.mo')
if not list((share / 'i18n').glob('*.mo')):
    raise SystemExit('No compiled translations found')
shutil.copytree(root / 'cr3gui/data/devices/pb360/keymaps', share / 'keymaps', dirs_exist_ok=True)
launcher = stage / 'system/bin/cr3-pb.app'
launcher.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(root / 'data/touch-hd3/cr3-pb.app', launcher)
apps = stage / 'applications'
apps.mkdir(exist_ok=True)
shutil.copy2(root / 'data/touch-hd3/CoolReader.app', apps / 'CoolReader.app')
shutil.copy2(root / 'data/touch-hd3/Welcome.fb2', share / 'Welcome.fb2')
launcher.chmod(0o755)
shutil.copy2(root / 'LICENSE', stage / 'LICENSE')
dependencies = json.loads((root / 'thirdparty/versions.json').read_text())
licenses = stage / 'licenses'
licenses.mkdir()
shutil.copy2(root / 'thirdparty/README.txt', licenses / 'README.txt')
shutil.copy2(root / 'thirdparty/versions.json', licenses / 'versions.json')
(licenses / 'crengine').mkdir()
shutil.copy2(root / 'crengine/xxhash-LICENSE.txt', licenses / 'crengine/xxhash-LICENSE.txt')
for dependency in dependencies['bundled']:
    for name in dependency['licenses']:
        destination = licenses / dependency['directory'] / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / 'thirdparty' / dependency['directory'] / name, destination)

(stage / 'README-touch-hd3.txt').write_text(
    f"Candidate revision: {args.revision}\nBinary SHA-256: {sha256(binary)}\n"
    "Automated ARM tests passed. This candidate has NOT been tested on a physical reader.\n"
    "The R5 verification notes below describe the historical R5 build only.\n\n"
    + (root / 'README-touch-hd3.txt').read_text())
# This is intentionally a template: never overwrite a reader's custom config.
shutil.copy2(root / 'data/touch-hd3/extensions.cfg.example', stage / 'extensions.cfg.example')
sdk = Path(os.environ.get('POCKETBOOK_SDK', root / 'work/sdk/SDK-B288-6.10'))
compiler = sdk / 'usr/bin/arm-obreey-linux-gnueabi-g++'
manifest = {
    'application': 'CoolReader for PocketBook Touch HD 3',
    'device': 'PocketBook Touch HD 3 / PB632 / B288 / ARM Cortex-A7 softfp',
    'target_firmware': 'U632.6.11.2001',
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
subprocess.run([sys.executable, str(root / 'tools/test-package-assets.py'), str(stage)], check=True)
manifest['package_asset_validation'] = 'passed'
(share / 'release-version.txt').write_text(manifest['release_version'] + '\n')
(stage / 'build-info.json').write_text(json.dumps(manifest, indent=2) + '\n')
dest = args.dist_dir or root / 'dist'
dest.mkdir(exist_ok=True)
archive = dest / ('coolreader-touch-hd3-fw6.11.2001-' + args.revision + '.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for p in sorted(stage.rglob('*')):
        if p.is_file():
            z.write(p, p.relative_to(stage))
checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
(archive.with_suffix('.zip.sha256')).write_text(f'{checksum}  {archive.name}\n')
print(archive)
print(checksum)
