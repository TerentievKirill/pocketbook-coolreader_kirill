#!/usr/bin/env python3
"""Validate the curated download list and generate the application table."""
import json
from pathlib import Path
import re
import sys
from urllib.parse import urlsplit

entries = json.loads(Path(sys.argv[1]).read_text())
seen = set()
rows = []
for entry in entries:
    name = entry['filename']
    format = entry.get('format', 'pocketbook')
    ending = 'stardict' if format == 'stardict' else 'dic'
    if format not in ('stardict', 'pocketbook') or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*[.]' + ending, name) or name in seen:
        raise ValueError('Invalid or duplicate dictionary filename: ' + name)
    seen.add(name)
    url = urlsplit(entry['url'])
    if url.scheme != 'https' or not url.hostname or url.username or url.password:
        raise ValueError('Expected a public HTTPS dictionary URL')
    if not re.fullmatch(r'[0-9a-f]{64}', entry['sha256']) or not 64 <= entry['size'] <= 256*1024*1024:
        raise ValueError('Invalid dictionary checksum or size')
    for key in ('label', 'credit'):
        if not entry[key] or '\x00' in entry[key]:
            raise ValueError('Missing dictionary ' + key)
    installed_size = entry.get('installed_size', 0)
    if not 0 <= installed_size <= 512*1024*1024:
        raise ValueError('Invalid unpacked dictionary size')
    rows.append('    {' + ', '.join(json.dumps(entry[k], ensure_ascii=True)
                for k in ('label', 'filename', 'url', 'sha256', 'credit')) + ', ' + str(entry['size']) + ', ' + json.dumps(format) + ', ' + str(installed_size) + '},')
if not rows:
    raise ValueError('The dictionary catalogue is empty')
Path(sys.argv[2]).write_text('// Generated from config/dictionaries.json.\nstatic const Entry catalog[] = {\n' + '\n'.join(rows) + '\n};\n')
