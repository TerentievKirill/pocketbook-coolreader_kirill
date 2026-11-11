"""Small deterministic dictionaries for the real StarDict reader/installer tests."""
import gzip
import random
from pathlib import Path
import shutil
import struct
import zipfile
import zlib


def create(out):
    root = out / 'stardict-fixtures'
    shutil.rmtree(root, ignore_errors=True)
    shutil.rmtree(out / 'stardict-install', ignore_errors=True)
    words = [('duplicate', 'first'), ('duplicate', 'second'), ('house', '<b>a building</b>' + ' content' * 30),
             ('kočka', 'cat'), ('markup', '<tag> & text'), ('дом', 'жилище'), ('слово', 'word')]
    words.sort(key=lambda p: (p[0].lower().encode() if p[0].isascii() else p[0].encode(), p[0].encode()))

    def make(name, compressed=False, wide=False, sequence='h', records=words):
        directory = root / name
        directory.mkdir(parents=True)
        data, index = bytearray(), bytearray()
        for word, body in records:
            raw = body.encode()
            index += word.encode() + b'\0' + struct.pack('>QI' if wide else '>II', len(data), len(raw))
            data += raw
        synonyms = [('houses', 'house'), ('kočky', 'kočka'), ('дома', 'дом'), ('слова', 'слово')]
        syn = bytearray()
        if records == words:
            for alias, target in synonyms:
                syn += alias.encode() + b'\0' + struct.pack('>I', next(i for i, (w, _) in enumerate(records) if w == target))
        if compressed:
            compressor = zlib.compressobj(wbits=-15)
            chunks = []
            for p in range(0, len(data), 64):
                chunks.append(compressor.compress(data[p:p+64]) + compressor.flush(zlib.Z_FINISH if p+64 >= len(data) else zlib.Z_FULL_FLUSH))
            table = struct.pack('<HHH', 1, 64, len(chunks)) + b''.join(struct.pack('<H', len(c)) for c in chunks)
            extra = b'RA' + struct.pack('<H', len(table)) + table
            header = b'\x1f\x8b\x08\x04' + b'\0'*4 + b'\0\xff' + struct.pack('<H', len(extra)) + extra
            (directory / 'test.dict.dz').write_bytes(header + b''.join(chunks) + struct.pack('<II', zlib.crc32(data), len(data)))
            (directory / 'test.idx.gz').write_bytes(gzip.compress(index))
        else:
            (directory / 'test.dict').write_bytes(data)
            (directory / 'test.idx').write_bytes(index)
        if syn:
            (directory / 'test.syn').write_bytes(syn)
        text = ("StarDict's dict ifo file\nversion=3.0.0\nbookname=Test dictionary\n" +
                f'wordcount={len(records)}\nidxfilesize={len(index)}\nidxoffsetbits={64 if wide else 32}\n')
        if sequence:
            text += 'sametypesequence=' + sequence + '\n'
        if syn:
            text += 'synwordcount=' + str(len(synonyms)) + '\n'
        (directory / 'test.ifo').write_text(text)
        return directory

    plain = make('plain')
    make('compressed', compressed=True)
    make('wide', wide=True)
    make('fields', sequence='tm', records=[('word', 'phonetic\0<meaning>')])
    make('typed', sequence='', records=[('word', 'h<b>rich</b>\0mplain\0')])
    for name in ('bad-size', 'bad-offset', 'bad-synonym', 'truncated-index', 'bad-order', 'bad-version'):
        directory = root / name
        shutil.copytree(plain, directory)
        if name == 'bad-size':
            p = directory / 'test.ifo'; p.write_text(p.read_text().replace('wordcount=7', 'wordcount=99'))
        elif name == 'bad-version':
            p = directory / 'test.ifo'; p.write_text(p.read_text().replace('version=3.0.0', 'version=9.0.0'))
        elif name == 'bad-offset':
            p = directory / 'test.idx'; b = bytearray(p.read_bytes()); b[len('duplicate')+1:len('duplicate')+5] = b'\xff'*4; p.write_bytes(b)
        elif name == 'bad-synonym':
            p = directory / 'test.syn'; b = bytearray(p.read_bytes()); b[len('houses')+1:len('houses')+5] = b'\xff'*4; p.write_bytes(b)
        elif name == 'truncated-index':
            p = directory / 'test.idx'; p.write_bytes(p.read_bytes()[:-1])
        else:
            p = directory / 'test.idx'; b = bytearray(p.read_bytes()); b[0] = ord('z'); p.write_bytes(b)
    directory = root / 'truncated-dictzip'; shutil.copytree(root / 'compressed', directory)
    p = directory / 'test.dict.dz'; p.write_bytes(p.read_bytes()[:30])
    for name in ('good', 'traversal', 'duplicate', 'incomplete', 'cancel'):
        with zipfile.ZipFile(out / ('stardict-' + name + '.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
            for p in plain.iterdir():
                if name == 'incomplete' and p.suffix == '.idx':
                    continue
                if name == 'cancel' and p.suffix == '.dict':
                    z.writestr('dictionary/' + p.name, p.read_bytes() + random.Random(42).randbytes(2*1024*1024))
                else:
                    z.write(p, 'dictionary/' + p.name)
            if name == 'traversal':
                z.writestr('../escaped.ifo', 'bad')
            if name == 'duplicate':
                z.write(plain / 'test.ifo', 'other/test.ifo')
