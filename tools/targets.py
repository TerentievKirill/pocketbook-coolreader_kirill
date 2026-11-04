#!/usr/bin/env python3
"""Load explicit build targets. Device candidates are never implicit build targets."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TARGET = 'pb632-fw6'


def load_target(name=DEFAULT_TARGET):
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]*', name):
        raise ValueError('Invalid target identifier: ' + name)
    path = ROOT / 'config/targets' / (name + '.json')
    if not path.is_file():
        raise ValueError('Unknown build target: ' + name + '; run tools/targets.py --list')
    target = json.loads(path.read_text())
    if target['schema'] != 1 or target['id'] != name:
        raise ValueError('Target schema or identifier mismatch: ' + str(path))
    # Resolve all repository inputs inside the checkout, including symlinks.
    paths = [target['inputs'], target['package']['assets'], target['package']['guide']]
    if target.get('device_registry'):
        paths.append(target['device_registry'])
    for relative in paths:
        candidate = (ROOT / relative).resolve()
        if ROOT not in candidate.parents or not candidate.exists():
            raise ValueError('Missing or external target input: ' + relative)
    return target


def target_evidence(target):
    """Canonical profile content is bound to the regression report and package."""
    encoded = json.dumps(target, sort_keys=True, separators=(',', ':')).encode()
    return {'id': target['id'], 'profile_sha256': hashlib.sha256(encoded).hexdigest(),
            'firmware': target['firmware']['version']}


def sdk_path(target):
    import os
    return Path(os.environ.get('POCKETBOOK_SDK', ROOT / 'work/sdk' / target['sdk']['directory']))


def firmware_path(target):
    import os
    return Path(os.environ.get('CR3_FIRMWARE_DIR', ROOT / target['firmware']['directory']))


def tool(target, name):
    return sdk_path(target) / 'usr/bin' / (target['sdk']['prefix'] + '-' + name)


def verify_build_target(build, target):
    marker = Path(build) / 'build-target.json'
    expected = {'target': target_evidence(target), 'sdk': str(sdk_path(target).resolve())}
    if not marker.is_file() or json.loads(marker.read_text()) != expected:
        raise ValueError('Build directory does not match the selected target/profile/SDK')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--target', default=DEFAULT_TARGET)
    args = parser.parse_args()
    if args.list:
        for path in sorted((ROOT / 'config/targets').glob('*.json')):
            target = load_target(path.stem)
            print(target['id'] + '\t' + target['label'] + '\t' + target['firmware']['version'])
    else:
        try:
            print(json.dumps(load_target(args.target), indent=2))
        except ValueError as error:
            parser.error(str(error))
