#!/usr/bin/env python3
"""Reject misidentified firmware before invoking any filesystem extractor."""
import importlib.util
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
from unittest import TestCase, main
import zipfile
from runtime_compatibility import elf_stack_policy

spec = importlib.util.spec_from_file_location('qualification', Path(__file__).with_name('qualify-pocketbook-firmware.py'))
qualification = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qualification)


class FirmwareIdentity(TestCase):
    def test_stack_flags_for_both_elf_classes(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / 'library.so'
            for wide in (False, True):
                for order, byte_order in (('<', 1), ('>', 2)):
                    for flags, expected in ((6, 'non-executable'), (7, 'executable')):
                        header = bytearray(64)
                        header[:6] = b'\x7fELF' + bytes((2 if wide else 1, byte_order))
                        struct.pack_into(order + ('Q' if wide else 'I'), header, 32 if wide else 28, 64)
                        struct.pack_into(order + 'HH', header, 54 if wide else 42, 56 if wide else 32, 1)
                        program = bytearray(56 if wide else 32)
                        struct.pack_into(order + 'I', program, 0, 0x6474e551)
                        struct.pack_into(order + 'I', program, 4 if wide else 24, flags)
                        path.write_bytes(header + program)
                        self.assertEqual(elf_stack_policy(path), expected)
                        path.write_bytes(header)
                        with self.assertRaisesRegex(ValueError, 'Truncated ELF program header'):
                            elf_stack_policy(path)
            struct.pack_into(order + 'HH', header, 54, 56, 0)
            path.write_bytes(header)
            self.assertEqual(elf_stack_policy(path), 'unspecified')

    def test_vendor_qml_description_does_not_break_version_read(self):
        data = b'<firmware><version>U634.6.10.3425</version><description>if (a < b && c) {}</description></firmware>'
        self.assertEqual(qualification.embedded_version(data), 'U634.6.10.3425')

    def fixture(self, directory, version='U634.6.10.3425', model='634', truncated=False):
        root = Path(directory)
        archive = root / 'firmware.zip'
        header = bytearray(1024)
        header[:16] = b'PocketBookUpdate'
        name = ('PocketBook' + model).encode()
        header[16:16+len(name)] = name
        if truncated:
            struct.pack_into('<4I', header, 256, 0x72, 0, 2048, 100)
            struct.pack_into('<4I', header, 272, 0x65, 0, 4096, 100)
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('fwinfo.xml', '<firmware><version>' + version + '</version></firmware>')
            z.writestr('SWUPDATE.BIN', header)
        output = root / 'output'
        output.mkdir()
        device = {'model': '634', 'software_version': 'U634.6.10.3425',
                  'archive': {'filename': archive.name, 'sha256': qualification.sha256(archive)}}
        return device, root, output

    def test_checksum_mismatch(self):
        with TemporaryDirectory() as directory:
            device, root, output = self.fixture(directory)
            device['archive']['sha256'] = 'wrong'
            with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                qualification.extract(device, root, output)

    def test_version_mismatch(self):
        with TemporaryDirectory() as directory:
            device, root, output = self.fixture(directory, version='U634.6.9.850')
            with self.assertRaisesRegex(ValueError, 'version mismatch'):
                qualification.extract(device, root, output)

    def test_model_mismatch(self):
        with TemporaryDirectory() as directory:
            device, root, output = self.fixture(directory, model='700')
            with self.assertRaisesRegex(ValueError, 'model mismatch'):
                qualification.extract(device, root, output)

    def test_truncated_sections(self):
        with TemporaryDirectory() as directory:
            device, root, output = self.fixture(directory, truncated=True)
            with self.assertRaisesRegex(ValueError, 'section bounds'):
                qualification.extract(device, root, output)


if __name__ == '__main__':
    main()
