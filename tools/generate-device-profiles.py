#!/usr/bin/env python3
"""Generate the runtime device table from the shared package's single registry."""
import argparse
import json
from pathlib import Path
import re


def generate(registry):
    if registry.get('schema') != 1 or not registry.get('devices'):
        raise ValueError('Unsupported or empty device registry')
    rows, models = [], set()
    for device in registry['devices']:
        model = device['model']
        if not re.fullmatch(r'[0-9]+[A-Z0-9]*', model) or model in models:
            raise ValueError('Invalid or duplicate model: ' + model)
        models.add(model)
        if not re.fullmatch(r'U[0-9A-Za-z]+\.[0-9]+\.[0-9]+\.[0-9]+', device['firmware']):
            raise ValueError('Invalid firmware version')
        for key in ('dpi', 'width', 'height'):
            if type(device[key]) is not int or device[key] <= 0:
                raise ValueError('Invalid device dimension: ' + key)
        rows.append('    {%s, %s, %d, %d, %d, %s, %s},' % (
            json.dumps(model), json.dumps(device['name']), device['dpi'], device['width'], device['height'],
            json.dumps(device['skin']), json.dumps(device['firmware'])))
    return '// Generated from config/pocketbook-devices.json; do not edit.\n' + \
        'static const Profile profiles[] = {\n' + '\n'.join(rows) + '\n};\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('registry', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    text = generate(json.loads(args.registry.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != text:
        args.output.write_text(text)
