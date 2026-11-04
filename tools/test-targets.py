#!/usr/bin/env python3
"""Regression checks for target selection and firmware incompatibility rejection."""
import copy
import json
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
from unittest import TestCase, main
from unittest.mock import patch
from targets import load_target, target_evidence, verify_build_target, sdk_path
from runtime_compatibility import check_runtime


class TargetSelection(TestCase):
    def test_unknown_or_path_target_rejected(self):
        for name in ('../pb632-fw6', '/tmp/target', 'verse-pro', 'PB632', ''):
            with self.subTest(name=name), self.assertRaises(ValueError):
                load_target(name)

    def test_profile_changes_invalidate_evidence(self):
        target = load_target()
        original = target_evidence(target)
        for section, key, value in [('sdk', 'prefix', 'another-abi'),
                                    ('firmware', 'version', 'another-firmware'),
                                    ('package', 'assets', 'another-defaults')]:
            changed = copy.deepcopy(target)
            changed[section][key] = value
            self.assertNotEqual(original, target_evidence(changed))

    def test_build_cannot_be_relabelled(self):
        target = load_target()
        with TemporaryDirectory() as directory:
            build = Path(directory)
            with self.assertRaises(ValueError):
                verify_build_target(build, target)
            identity = {'target': target_evidence(target), 'sdk': str(sdk_path(target).resolve())}
            (build / 'build-target.json').write_text(json.dumps(identity))
            verify_build_target(build, target)
            changed = copy.deepcopy(target)
            changed['sdk']['c_flags'].append('-mfloat-abi=hard')
            with self.assertRaises(ValueError):
                verify_build_target(build, changed)


class RuntimeCompatibility(TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.runtime = self.root / 'runtime'
        (self.runtime / 'lib').mkdir(parents=True)
        self.application = self.root / 'app'
        self.loader = self.runtime / 'lib/ld-linux.so.3'
        self.libc = self.runtime / 'lib/libc.so.6'
        for path in (self.application, self.loader, self.libc):
            self.write_elf(path)
        self.firmware = load_target()['firmware']
        self.interpreter = '/lib/ld-linux.so.3'
        self.required = 'GLIBC_2.39'
        self.available = 'GLIBC_2.39'
        self.imported_symbol = 'function@GLIBC_2.39'
        self.exported_symbol = 'function@@GLIBC_2.39'

    def write_elf(self, path, machine=40, flags=0x5000200):
        header = bytearray(64)
        header[:7] = b'\x7fELF\x01\x01\x01'
        struct.pack_into('<H', header, 18, machine)
        struct.pack_into('<I', header, 36, flags)
        path.write_bytes(header)

    def readelf(self, command, **kwargs):
        path = Path(command[-1])
        if '-l' in command:
            return '[Requesting program interpreter: ' + self.interpreter + ']'
        if '-d' in command:
            return ' (NEEDED) Shared library: [libc.so.6]' if path == self.application else ''
        if '--dyn-syms' in command:
            if path == self.application:
                return ' 1: 00000000 0 FUNC GLOBAL DEFAULT UND ' + self.imported_symbol
            if path == self.libc:
                return ' 1: 00000100 8 FUNC GLOBAL DEFAULT 12 ' + self.exported_symbol
            return ''
        if path == self.application:
            return 'Version needs section\n File: libc.so.6\n Name: ' + self.required
        if path == self.libc:
            return 'Version definition section\n Name: ' + self.available
        return ''

    def check(self):
        with patch('runtime_compatibility.subprocess.check_output', side_effect=self.readelf):
            return check_runtime(self.application, self.runtime, 'readelf', self.firmware)

    def test_matching_runtime_passes(self):
        self.assertEqual(self.check()['status'], 'passed')

    def test_old_firmware_rejected(self):
        self.available = 'GLIBC_2.17'
        with self.assertRaisesRegex(ValueError, 'lacks versions GLIBC_2.39'):
            self.check()

    def test_version_exists_but_required_symbol_is_missing(self):
        self.exported_symbol = 'another_function@@GLIBC_2.39'
        with self.assertRaisesRegex(ValueError, 'unresolved required symbols: function@GLIBC_2.39'):
            self.check()

    def test_wrong_float_abi_rejected(self):
        self.write_elf(self.application, flags=0x5000400)
        with self.assertRaisesRegex(ValueError, 'ELF ABI mismatch'):
            self.check()

    def test_wrong_library_machine_rejected(self):
        self.write_elf(self.libc, machine=183)
        with self.assertRaisesRegex(ValueError, 'ELF ABI mismatch'):
            self.check()

    def test_wrong_loader_rejected(self):
        self.interpreter = '/lib/ld-linux-armhf.so.3'
        with self.assertRaisesRegex(ValueError, 'loader does not match'):
            self.check()

    def test_missing_dependency_rejected(self):
        self.libc.unlink()
        with self.assertRaisesRegex(ValueError, 'library is missing'):
            self.check()

    def test_external_library_symlink_rejected(self):
        self.libc.unlink()
        self.libc.symlink_to(self.application)
        with self.assertRaisesRegex(ValueError, 'library is missing'):
            self.check()


if __name__ == '__main__':
    main()
