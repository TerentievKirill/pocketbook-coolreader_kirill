#!/usr/bin/env python3
"""Prepare native PocketBook packs from cached open StarDict sources (Python 3.12+).

Run in the build container with the pinned PyGlossary checkout on PYTHONPATH.
Source archives are retained beside the packs for attribution and reproducibility.
This is a maintainer tool; the reader only downloads the finished .dic files.
"""
import argparse
from dataclasses import dataclass
import gzip
import hashlib
import html
import json
from pathlib import Path
import struct
import zipfile

CONVERTER = '74d8b9cf3dcd5ff4869f6efecbcb33c6c9f507e7'
LANGUAGES = {'en': 'English', 'ru': 'Russian', 'uk': 'Ukrainian', 'cs': 'Czech'}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dictionary_entries(archive):
    with zipfile.ZipFile(archive) as source:
        def member(suffix, optional=False):
            found = [n for n in source.namelist() if n.endswith(suffix)]
            if len(found) != 1:
                if optional and not found:
                    return b''
                raise ValueError('Expected one ' + suffix + ' in ' + str(archive))
            return source.read(found[0])
        metadata = dict(line.split('=', 1) for line in member('.ifo').decode().splitlines() if '=' in line)
        if metadata.get('sametypesequence') != 'h' or metadata.get('idxoffsetbits', '32') != '32':
            raise ValueError('Unsupported StarDict record format')
        compressed = member('.dict.dz', optional=True)
        data = gzip.decompress(compressed) if compressed else member('.dict')
        index = member('.idx')
        synonyms = member('.syn', optional=True)
    entries = []
    offset = 0
    while offset < len(index):
        end = index.index(b'\0', offset)
        word = index[offset:end].decode('utf-8')
        start, length = struct.unpack_from('>II', index, end + 1)
        if start + length > len(data):
            raise ValueError('StarDict definition outside data file')
        entries.append((word, data[start:start+length].decode('utf-8')))
        offset = end + 9
    if len(entries) != int(metadata['wordcount']):
        raise ValueError('StarDict word count mismatch')
    # Expand alternate forms into real SDIC entries. The stock SDIC writer only
    # keeps the first term of a multi-term entry, which would lose inflections.
    primary_count = len(entries)
    offset = 0
    while offset < len(synonyms):
        end = synonyms.index(b'\0', offset)
        word = synonyms[offset:end].decode('utf-8')
        index = struct.unpack_from('>I', synonyms, end + 1)[0]
        if index >= primary_count:
            raise ValueError('Invalid StarDict synonym index')
        entries.append((word, entries[index][1]))
        offset = end + 5
    return entries, metadata


@dataclass
class Entry:
    word: str
    defi: str

    @property
    def l_term(self):
        return [self.word]

    def isData(self):
        return False


class Info:
    def __init__(self, name):
        self.name = name

    def getInfo(self, key):
        return self.name if key in ('name', 'bookname') else ''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--version', default='20261110')
    parser.add_argument('--only', help='Rebuild one source stem, retaining other prepared packs')
    args = parser.parse_args()
    from pyglossary.plugins.pocketbook_sdic.writer import Writer
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    catalog, manifest = [], []
    if args.only:
        catalog = [entry for entry in json.loads((output / 'catalog.json').read_text()) if entry['filename'] != args.only + '.dic']
        manifest = [entry for entry in json.loads((output / 'sources.json').read_text()) if entry['file'] != args.only + '.dic']
    inputs = [('wikdict-' + a + '-' + b, a, b) for a in ('en', 'ru', 'cs')
              for b in ('en', 'ru', 'cs') if a != b]
    inputs += [('reader-' + a, a, a) for a in LANGUAGES]
    for stem, source, target in inputs:
        if args.only and stem != args.only:
            continue
        archive = args.cache / (stem + '.zip')
        words, original = dictionary_entries(archive)
        label = LANGUAGES[source] + (' definitions' if source == target else ' → ' + LANGUAGES[target])
        name = 'ReaderTools ' + source + '-' + target
        url = ('https://download.wikdict.com/dictionaries/stardict/' + stem + '.zip' if stem.startswith('wikdict') else
               'https://www.reader-dict.com/file/' + source + '/dict-' + source + '-' + source + '-noetym.zip')
        provider = 'WikDict / FreeDict / Wiktionary / DBnary' if stem.startswith('wikdict') else 'reader.dict / Wiktionary'
        credit = (provider + '\nCC BY-SA 4.0\nConverted to PocketBook format by ReaderTools.\n'
                  'Sources and credits: https://readertools.eu/downloads/dictionaries/' + args.version + '/credits.txt')
        credits_html = ('<b>' + html.escape(label) + '</b><br>' + html.escape(credit).replace('\n', '<br>') +
                        '<br>Original description:<br>' + original.get('description', '') +
                        '<br>Source: ' + html.escape(url) +
                        '<br>License: https://creativecommons.org/licenses/by-sa/4.0/')
        words.append(('CoolReader dictionary credits', credits_html))
        # Merge exact headwords before enforcing the native record limit. Several
        # StarDict synonym groups can otherwise create one oversized SDIC record.
        combined = {}
        for word, definition in words:
            if word not in combined:
                combined[word] = definition
            elif definition not in combined[word]:
                combined[word] += '<hr/>' + definition
        words = list(combined.items())
        del combined
        metadata = output / (stem + '.metadata.json')
        metadata.write_text(json.dumps(dict(name=name, localeFrom=source, localeTo=target,
                                            description=label, provider=provider), ensure_ascii=False))
        writer = Writer(Info(name))
        writer._metadata_json_path = str(metadata)
        filename = stem + '.dic'
        writer.open(str(output / filename))
        receiver = writer.write()
        next(receiver)
        shortened = 0
        for word, definition in words:
            encoded = definition.encode('utf-8')
            if len(encoded) > 60000:
                definition = encoded[:59000].decode('utf-8', errors='ignore') + '<p>[Entry shortened for PocketBook. See the source dictionary for the full article.]</p>'
                shortened += 1
            receiver.send(Entry(word, definition))
        try:
            receiver.send(None)
        except StopIteration:
            pass
        writer.finish()
        result = output / filename
        catalog.append(dict(label=label, filename=filename, size=result.stat().st_size,
                            sha256=sha256(result), credit=credit,
                            url='https://readertools.eu/downloads/dictionaries/' + args.version + '/' + filename))
        manifest.append(dict(file=filename, entries=len(words), source=url, source_sha256=sha256(archive),
                             sha256=sha256(result), license='CC BY-SA 4.0', provider=provider,
                             shortened_articles=shortened, original_metadata=original, converter_commit=CONVERTER))
        print(label, len(words), result.stat().st_size, flush=True)
        del words
    catalog.sort(key=lambda entry: entry['label'])
    (output / 'catalog.json').write_text(json.dumps(catalog, indent=2, ensure_ascii=False) + '\n')
    (output / 'sources.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n')
    (output / 'credits.txt').write_text(
        'ReaderTools dictionaries\n\nDictionary data: CC BY-SA 4.0.\n'
        'https://creativecommons.org/licenses/by-sa/4.0/\n\n'
        'WikDict / FreeDict dictionaries: Karl Bartel, Wiktionary contributors, DBnary.\n'
        'https://www.wikdict.com/page/about\n\n'
        'Monolingual dictionaries: reader.dict (Mickaël Schoentgen), Wiktionary contributors.\n'
        'https://www.reader-dict.com/legal-mentions\n\n'
        'Changes: converted StarDict to PocketBook SDIC, expanded alternate word forms, added credits.\n'
        'Articles over the native record limit are shortened with a notice; see sources.json.\n'
        'Conversion: PyGlossary (GPL-3.0-or-later), commit ' + CONVERTER + '\n'
        'https://github.com/ilius/pyglossary/tree/' + CONVERTER + '\n'
        'Full upstream descriptions and exact source URLs/checksums: sources.json\n')


if __name__ == '__main__':
    main()
