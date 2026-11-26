#!/usr/bin/env python3
"""Check Russian catalog coverage against current UI sources and validate gettext formats."""
import ast
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def entries(path):
    result, entry, field = [], {}, None
    for line in path.read_text().splitlines() + ['']:
        if not line.strip():
            if 'msgid' in entry:
                result.append(entry)
            entry, field = {}, None
        elif line.startswith('#,'):
            entry['flags'] = line
        elif line.startswith(('msgid ', 'msgstr ', 'msgctxt ')):
            field, value = line.split(' ', 1)
            entry[field] = ast.literal_eval(value)
        elif line.startswith('"') and field:
            entry[field] += ast.literal_eval(line)
    return result


def main():
    sources = sorted((ROOT / 'cr3gui/src').glob('*.cpp')) + sorted((ROOT / 'cr3gui/src').glob('*.h'))
    sources += [ROOT / 'crengine/src/crgui.cpp', ROOT / 'crengine/src/hyphman.cpp']
    catalog = ROOT / 'cr3gui/po/cr3_ru.po'
    with tempfile.TemporaryDirectory() as directory:
        template = Path(directory) / 'current.pot'
        subprocess.run(['xgettext', '--language=C++', '--from-code=UTF-8', '--keyword=_',
                        '--keyword=_8', '--keyword=_32', '--keyword=C_:1c,2',
                        '-o', str(template)] + [str(p) for p in sources], check=True)
        subprocess.run(['msgfmt', '--check', '--check-format', '-o', directory + '/ru.mo',
                        str(catalog)], check=True)
        translated = {(e.get('msgctxt', ''), e['msgid']): e for e in entries(catalog)}
        errors, count = [], 0
        for entry in entries(template):
            text = entry['msgid']
            if not text:
                continue
            count += 1
            context = entry.get('msgctxt', '')
            value = translated.get((context, text))
            if value is None and context:
                value = translated.get(('', context + '\004' + text))
            if not value or not value.get('msgstr') or 'fuzzy' in value.get('flags', ''):
                errors.append('Missing Russian translation: ' + repr(text))
            elif sorted(re.findall(r'\$[12]', text)) != sorted(re.findall(r'\$[12]', value['msgstr'])):
                errors.append('Changed substitution tokens: ' + repr(text))
        if errors:
            raise SystemExit('\n'.join(errors))
        print('PASS: Russian translations cover all %d current UI keys; formats and substitution tokens are valid' % count)


if __name__ == '__main__':
    main()
