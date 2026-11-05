#!/usr/bin/env python3
"""Exercise the new payload through the frozen 1.0.3 outer launcher."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
from unittest import TestCase, main

ROOT = Path(__file__).resolve().parents[1]


class RuntimeEntry(TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.storage = Path(self.temporary.name)
        self.share = self.storage / 'system/share/cr3'
        (self.share / 'bin').mkdir(parents=True)
        (self.share / 'Welcome.fb2').write_text('welcome')
        (self.share / 'custom-font.ttf').write_text('preserve')
        self.book = self.storage / 'A book with spaces.fb2'
        self.book.write_text('book')
        text = (ROOT / 'tests/fixtures/shared-launcher-1.0.3.sh').read_text()
        if sys.platform == 'darwin':
            text = text.replace('    /mnt/ext1/system/share/cr3/bin/cr3-pb.app',
                                '    sh /mnt/ext1/system/share/cr3/bin/cr3-pb.app')
        self.outer = self.storage / 'launcher'
        self.outer.write_text(text.replace('/mnt/ext1', str(self.storage)))

    def child(self, path, label, status=0):
        path.write_text('#!/bin/sh\nprintf "%s\\n" "' + label + '" "$GLIBC_TUNABLES" "$1" > "'
                        + str(self.storage / 'observed') + '"\nexit ' + str(status) + '\n')
        path.chmod(0o644)

    def payload(self, folder, status=0):
        (folder / 'bin').mkdir(exist_ok=True, parents=True)
        entry = (ROOT / 'data/pocketbook-armv7/runtime-entry.app').read_text()
        if sys.platform == 'darwin':
            entry = entry.replace('exec "$binary" "$@"', 'exec sh "$binary" "$@"')
        (folder / 'bin/cr3-pb.app').write_text(entry)
        (folder / 'bin/cr3-pb.app').chmod(0o755)
        self.child(folder / 'bin/cr3-pb-executable.app', 'new', status)
        (folder / 'package-family.txt').write_text('pocketbook-armv7-softfp\n')

    def run_outer(self, tunables='', expected=0):
        result = subprocess.run(['sh', str(self.outer), str(self.book)],
                                env={**os.environ, 'GLIBC_TUNABLES': tunables})
        self.assertEqual(result.returncode, expected)
        return (self.storage / 'observed').read_text().splitlines()

    def test_compatibility_environment_permissions_and_arguments(self):
        self.payload(self.share)
        for existing in ('', 'glibc.malloc.trim_threshold=123456', 'glibc.rtld.execstack=0'):
            (self.share / 'bin/cr3-pb-executable.app').chmod(0o644)
            observed = self.run_outer(existing)
            self.assertEqual(observed, ['new', existing + (':' if existing else '') + 'glibc.rtld.execstack=2', str(self.book)])
            self.assertTrue(os.access(self.share / 'bin/cr3-pb-executable.app', os.X_OK))
            self.assertIn('firmware library compatibility enabled', (self.storage / 'system/cr3-pb.crash.log').read_text())

    def test_upgrade_from_old_extractor_and_failed_first_run_rollback(self):
        self.child(self.share / 'bin/cr3-pb.app', 'old')
        (self.share / 'bin/cr3-pb.app').chmod(0o755)
        pending = self.share.parent / 'cr3-ota-shared-new'
        self.payload(pending)
        (pending / '.ready').write_text('1.0.4')
        self.assertEqual(self.run_outer()[0], 'new')
        self.assertEqual((self.share / 'custom-font.ttf').read_text(), 'preserve')
        self.assertFalse(pending.exists())
        # A newly staged payload may fail; 1.0.3 still rolls it back correctly.
        self.payload(pending, status=7)
        (pending / '.ready').write_text('1.0.5')
        self.assertEqual(self.run_outer()[0], 'new')
        self.assertFalse((pending / '.ready').exists())
        self.assertEqual((self.share / 'custom-font.ttf').read_text(), 'preserve')

    def test_child_failure_reaches_outer_launcher(self):
        self.payload(self.share, status=7)
        self.run_outer(expected=7)


if __name__ == '__main__':
    main()
