#!/usr/bin/env python3
"""Guard registry consistency and admission of shared firmware evidence."""
import copy
import importlib.util
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import TestCase, main
import build_metadata as metadata
from targets import ROOT, load_target
from package_layout import validate_reader_layout

spec = importlib.util.spec_from_file_location('device_generator', Path(__file__).with_name('generate-device-profiles.py'))
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class SharedPackage(TestCase):
    def test_support_files_stay_out_of_library(self):
        validate_reader_layout([
            'applications/CoolReader.app', 'system/share/cr3/licenses/COPYING',
            'system/share/cr3/doc/README-pocketbook-armv7.txt',
            'system/share/cr3/metadata/build-info.json'])
        for name in ('LICENSE', 'licenses/README.txt', 'README-pocketbook-armv7.txt',
                     'build-info.json', '../system/README.txt', 'system/../README.txt',
                     '/system/README.txt', 'system//README.txt', 'system\\README.txt'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                validate_reader_layout(['system/share/cr3/licenses/COPYING', name])

    def test_registry_matches_qualified_firmware(self):
        profile = load_target('pocketbook-armv7-softfp')
        registry = json.loads((ROOT / profile['device_registry']).read_text())
        candidates = json.loads((ROOT / 'config/firmware-candidates.json').read_text())['devices']
        expected = {candidate['software_version'] for candidate in candidates}
        expected.add(profile['firmware']['version'])
        self.assertEqual({device['firmware'] for device in registry['devices']}, expected)
        self.assertEqual(len(registry['devices']), 4)
        text = generator.generate(registry)
        for version in expected:
            self.assertIn(version, text)
        duplicate = copy.deepcopy(registry)
        duplicate['devices'].append(duplicate['devices'][0])
        with self.assertRaises(ValueError):
            generator.generate(duplicate)

    def test_missing_or_stale_firmware_evidence_blocks_packaging(self):
        profile = load_target('pocketbook-armv7-softfp')
        with TemporaryDirectory() as directory:
            build = Path(directory)
            binary = build / 'app'
            binary.write_bytes(b'new binary')
            (build / 'firmware-qualification').mkdir()
            with self.assertRaises(FileNotFoundError):
                metadata.verified_firmware_reports(binary, ROOT, build, profile, {})
            for name in profile['qualification_candidates']:
                (build / 'firmware-qualification' / (name + '.json')).write_text(json.dumps({
                    'status': 'laboratory-checks-passed', 'baseline_binary_sha256': 'old-binary'}))
            with self.assertRaisesRegex(ValueError, 'mismatched firmware evidence'):
                metadata.verified_firmware_reports(binary, ROOT, build, profile, {})


if __name__ == '__main__':
    main()
