#!/usr/bin/env python3
"""Ensure source cleanup preserves every runtime asset from the tested package."""
from pathlib import Path
import gettext
import hashlib
import json
import sys
import zipfile
import argparse
from targets import DEFAULT_TARGET, load_target
from package_layout import validate_reader_layout

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stage', type=Path)
parser.add_argument('--target', default=DEFAULT_TARGET)
args = parser.parse_args()
stage = args.stage
validate_reader_layout(p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file())
target = load_target(args.target)
package = target['package']
if target.get('device_registry'):
    entry = stage / 'system/share/cr3/bin/cr3-pb.app'
    executable = stage / 'system/share/cr3/bin/cr3-pb-executable.app'
    if entry.read_bytes() != (root / package['assets'] / 'runtime-entry.app').read_bytes():
        raise SystemExit('Missing or changed shared runtime entry point')
    if not executable.read_bytes().startswith(b'\x7fELF') or not entry.stat().st_mode & 0o111:
        raise SystemExit('Missing ELF executable or non-executable runtime entry point')
expected = json.loads((root / 'tests/package-assets.json').read_text())
for name, checksum in expected.items():
    if name in ('system/share/cr3/devices.ini', 'system/share/cr3/Welcome.fb2'):
        continue  # Device defaults are checked against the selected source below.
    path = stage / name
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != checksum:
        raise SystemExit(f'Missing or changed runtime asset: {name}')
translations = stage / 'system/share/cr3/i18n'
languages = {p.stem[4:] for p in (root / 'cr3gui/po').glob('cr3_*.po')}
if {p.stem for p in translations.glob('*.mo')} != languages:
    raise SystemExit('Missing or unexpected compiled translations')
for path in translations.glob('*.mo'):
    with path.open('rb') as stream:
        gettext.GNUTranslations(stream)
for skin in package['skins']:
    sources = root / 'cr3gui/data/devices' / package['keymaps'] / 'skins' / skin
    with zipfile.ZipFile(stage / f'system/share/cr3/skins/{skin}.cr3skin') as archive:
        names = {p.name for p in sources.iterdir() if p.is_file()}
        if set(archive.namelist()) != names:
            raise SystemExit(f'Skin contents differ: {skin}')
        for name in names:
            if archive.read(name) != (sources / name).read_bytes():
                raise SystemExit(f'Skin asset differs: {skin}/{name}')
for relative in ('applications/CoolReader.app', 'system/bin/cr3-pb.app',
                 'system/share/cr3/cr3-defaults.ini', 'system/share/cr3/devices.ini',
                 'system/share/cr3/Welcome.fb2'):
    if (stage / relative).read_bytes() != (root / package['assets'] / Path(relative).name).read_bytes():
        raise SystemExit(f'Packaged defaults or launcher differs: {relative}')
print(f"PASS: {len(expected)} runtime assets, {len(languages)} translations, {len(package['skins'])} skins, launchers and defaults")

dependencies = json.loads((root / 'thirdparty/versions.json').read_text())
licenses = stage / 'system/share/cr3/licenses'
if (licenses / 'COPYING').read_bytes() != (root / 'LICENSE').read_bytes():
    raise SystemExit('Missing or changed GPL license')
if (licenses / 'README.txt').read_bytes() != (root / 'thirdparty/README.txt').read_bytes():
    raise SystemExit('Missing or changed dependency notice')
if (licenses / 'crengine/xxhash-LICENSE.txt').read_bytes() != (root / 'crengine/xxhash-LICENSE.txt').read_bytes():
    raise SystemExit('Missing or changed engine notice')
if json.loads((licenses / 'versions.json').read_text()) != dependencies:
    raise SystemExit('Packaged dependency versions differ')
for dependency in dependencies['bundled']:
    for name in dependency['licenses']:
        original = root / 'thirdparty' / dependency['directory'] / name
        packaged = licenses / dependency['directory'] / name
        if packaged.read_bytes() != original.read_bytes():
            raise SystemExit(f'Missing or changed license: {dependency["name"]}/{name}')
print('PASS: GPL license, pinned dependency manifest and all bundled library notices inside system storage')
