#!/usr/bin/env python3
"""Install/recover shared updates while preserving books, assets and rollback."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temporary:
    storage = Path(temporary)
    share = storage / 'system/share/cr3'
    (share / 'bin').mkdir(parents=True)
    (share / 'Welcome.fb2').write_text('welcome')
    (share / 'package-family.txt').write_text('pocketbook-armv7-softfp\n')
    (share / 'custom-font.ttf').write_text('user font')
    book = storage / 'A book with spaces.fb2'
    book.write_text('book')

    def binary(path, name, status=0):
        path.write_text('#!/bin/sh\nprintf "%s" "$1" > "' + str(storage / 'opened') +
                        '"\nprintf "' + name + '" > "' + str(storage / 'ran') + '"\nexit ' + str(status) + '\n')
        path.chmod(0o755)

    binary(share / 'bin/cr3-pb.app', 'original')
    legacy = share.parent / 'cr3-ota-new'
    (legacy / 'bin').mkdir(parents=True)
    (legacy / '.ready').write_text('9.0.0')
    (legacy / 'bin/cr3-pb.app').write_text('must never run')
    pending = share.parent / 'cr3-ota-shared-new'
    (pending / 'bin').mkdir(parents=True)
    (pending / '.ready').write_text('1.0.4')
    (pending / 'package-family.txt').write_text('pocketbookhf\n')
    binary(pending / 'bin/cr3-pb.app', 'wrong-family')
    launcher = storage / 'launcher'
    launcher.write_text((root / 'data/pocketbook-armv7/cr3-pb.app').read_text()
                        .replace('    /mnt/ext1/system/share/cr3/bin/cr3-pb.app',
                                 '    sh /mnt/ext1/system/share/cr3/bin/cr3-pb.app')
                        .replace('/mnt/ext1', str(storage)))
    subprocess.run(['sh', str(launcher), str(book)], check=True)
    assert (storage / 'opened').read_text() == str(book)
    assert (storage / 'ran').read_text() == 'original'
    assert (legacy / '.ready').exists() and (pending / '.ready').exists()
    (pending / 'package-family.txt').write_text('pocketbook-armv7-softfp\n')
    binary(pending / 'bin/cr3-pb.app', 'updated')
    subprocess.run(['sh', str(launcher)], check=True)
    assert (storage / 'ran').read_text() == 'updated'
    assert (storage / 'opened').read_text() == str(book)
    assert (share / 'custom-font.ttf').read_text() == 'user font'
    assert not pending.exists() and not (share / '.ready').exists()
    (pending / 'bin').mkdir(parents=True)
    (pending / '.ready').write_text('1.0.5')
    (pending / 'package-family.txt').write_text('pocketbook-armv7-softfp\n')
    binary(pending / 'bin/cr3-pb.app', 'failed', 1)
    subprocess.run(['sh', str(launcher)], check=True)
    assert (storage / 'ran').read_text() == 'updated'
    assert not (pending / '.ready').exists()
    backup = share.parent / 'cr3-ota-shared-old'
    share.rename(backup)
    subprocess.run(['sh', str(launcher)], check=True)
    assert (storage / 'opened').read_text() == str(book)
    assert (share / 'custom-font.ttf').read_text() == 'user font'
    book.unlink()
    subprocess.run(['sh', str(launcher)], check=True)
    assert (storage / 'opened').read_text() == str(share / 'Welcome.fb2')
print('PASS: shared installation, family isolation, rollback, interrupted-switch recovery, custom assets and remembered books')
