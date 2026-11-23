#!/usr/bin/env python3
"""Guard registry consistency and admission of shared firmware evidence."""
import copy
import importlib.util
import json
import subprocess
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
        self.assertEqual({device['firmware'] for device in registry['devices'] if device['firmware']}, expected)
        self.assertEqual(sum(bool(device['firmware']) for device in registry['devices']), 5)
        self.assertEqual(len(registry['devices']), 22)
        text = generator.generate(registry)
        for version in expected:
            self.assertIn(version, text)
        duplicate = copy.deepcopy(registry)
        duplicate['devices'].append(duplicate['devices'][0])
        with self.assertRaises(ValueError):
            generator.generate(duplicate)

    def test_runtime_admission_for_all_profiles(self):
        registry = json.loads((ROOT / 'config/pocketbook-devices.json').read_text())
        with TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / 'pb_device_data.h').write_text(generator.generate(registry))
            checks = []
            for device in registry['devices']:
                model = device['model']
                firmware = device['firmware'] or ('U' + model + '.6.10.1')
                for description in (model, 'PB' + model, 'PocketBook ' + model,
                                    'PocketBook ' + device['name'] + ' (PB' + model + ')'):
                    checks.append('assert(pbdevice::supported(%s, %s));' %
                                  (json.dumps(description), json.dumps(firmware)))
                checks.append('assert(!pbdevice::supported(%s, "U%s.6.9.1"));' %
                              (json.dumps(model), model))
            descriptor = ('CR3-OTA-SHARED-2\npocketbook-armv7-softfp\n1.0.8\n' + 'a' * 64 + '\n' +
                          ''.join(d['model'] + ' ' + str(d['minimum_api']) + '\n' for d in registry['devices']))
            checks += ['cr3ota::SharedRelease release;',
                       'assert(cr3ota::parseSharedRelease(%s, release));' % json.dumps(descriptor)]
            for device in registry['devices']:
                checks.append('assert(cr3ota::compatible(release, "pocketbook-armv7-softfp", %s, "U.6.10.1"));' % json.dumps(device['model']))
            checks += ['assert(pbdevice::modelNumber("PB740-2") == "7402");',
                       'assert(pbdevice::modelNumber("PocketBook InkPad 3 Pro (PB740-3)") == "7403");',
'assert(!pbdevice::supported("PB1030", "U1030.6.11.1"));',
                       'assert(!pbdevice::supported("PB1042", "U1042.6.11.1"));',
                       'assert(!pbdevice::supported("PB629K3", "U629K3.6.11.1"));',
                       'assert(!pbdevice::supported("PB634K3", nullptr));']
            source = folder / 'profiles.cpp'
            source.write_text('#include <cassert>\n#include "pb_device_profile.h"\n#include "ota_version.h"\nint main() {\n' +
                              '\n'.join(checks) + '\n}\n')
            binary = folder / 'profiles'
            subprocess.run(['c++', '-std=c++11', '-I' + str(folder),
                            '-I' + str(ROOT / 'cr3gui/src'), str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

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
