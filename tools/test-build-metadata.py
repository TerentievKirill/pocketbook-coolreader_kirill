#!/usr/bin/env python3
"""Packaging must reject missing, failed or stale validation evidence."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import TestCase, main
from unittest.mock import patch
import copy
import json
from targets import sdk_path
import build_metadata as metadata

class ValidationEvidence(TestCase):
    def test_binding_and_failures(self):
        with TemporaryDirectory() as directory:
            root = Path(directory); binary = root / 'app'; binary.write_bytes(b'binary')
            flags = root / 'crengine/CMakeFiles/crengine.dir/flags.make'
            flags.parent.mkdir(parents=True); flags.write_text('flags')
            firmware = root / 'firmware'
            library = firmware / 'runtime/lib/libc.so.6'
            library.parent.mkdir(parents=True); library.write_bytes(b'firmware libc')
            source = {'commit': 'abc', 'dirty': True, 'tree_sha256': 'tree'}
            (root / 'build-target.json').write_text(json.dumps({
                'target': metadata.target_evidence(metadata.load_target()),
                'sdk': str(sdk_path(metadata.load_target()).resolve())}))
            report = {'status': 'passed', 'passed': sorted(metadata.REQUIRED_TESTS),
                      'binary_sha256': metadata.sha256(binary), 'source': source,
                      'configuration_sha256': metadata.sha256(flags),
                      'target': metadata.target_evidence(metadata.load_target()),
                      'runtime_compatibility': {'status': 'passed', 'files_sha256': {
                          'lib/libc.so.6': metadata.sha256(library)}}}
            with patch.object(metadata, 'source_metadata', return_value=source), \
                    patch.object(metadata, 'firmware_path', return_value=firmware):
                self.assertEqual(metadata.verified_report(report, binary, root, root), report)
                for key, bad in [('status', 'failed'), ('passed', []), ('binary_sha256', 'wrong'),
                                 ('source', {}), ('configuration_sha256', 'wrong'), ('target', {}), ('runtime_compatibility', {})]:
                    invalid = copy.deepcopy(report); invalid[key] = bad
                    with self.subTest(field=key), self.assertRaises(ValueError):
                        metadata.verified_report(invalid, binary, root, root)
                library.write_bytes(b'another firmware')
                with self.assertRaisesRegex(ValueError, 'Firmware runtime changed'):
                    metadata.verified_report(report, binary, root, root)
                library.write_bytes(b'firmware libc')
                binary.write_bytes(b'changed binary')
                with self.assertRaises(ValueError): metadata.verified_report(report, binary, root, root)
if __name__ == '__main__': main()
