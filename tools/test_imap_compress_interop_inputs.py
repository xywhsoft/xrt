"""Regression tests for empty IMAP interop selections and external artifacts."""

import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import test_imap_compress_interop as interop


class InteropInputTests(unittest.TestCase):
    def test_plain_tls_only_selection_is_rejected_before_build_or_write(self):
        for scenario in ('hrr', 'close-truncated', 'close-stall'):
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                output = root / 'uncreated'
                arguments = ['probe', '--case', scenario, '--mode', 'plain', '--output-dir', str(output)]
                with patch.object(interop, 'ROOT', root), patch.object(sys, 'argv', arguments), \
                        patch.object(interop, 'build_clients') as build, contextlib.redirect_stderr(io.StringIO()) as errors:
                    with self.assertRaises(SystemExit) as result:
                        interop.main()
                    self.assertEqual(result.exception.code, 2)
                    self.assertIn('no applicable', errors.getvalue())
                    build.assert_not_called()
                self.assertFalse(output.exists())

    def test_external_output_records_absolute_binary_path_and_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary).resolve()
            root, output = parent / 'source', parent / 'artifacts'
            root.mkdir()
            output.mkdir()
            binary = output / 'modular' / 'client'
            binary.parent.mkdir()
            binary.write_bytes(b'artifact-path-regression')
            arguments = ['probe', '--case', 'dynamic', '--mode', 'plain', '--layout', 'modular',
                         '--output-dir', str(output)]
            case = {'layout': 'modular', 'mode': 'plain', 'scenario': 'dynamic', 'client_exit': 0}
            with patch.object(interop, 'ROOT', root), patch.object(sys, 'argv', arguments), \
                    patch.object(interop, 'build_clients', return_value={'modular': binary}), \
                    patch.object(interop, 'certificates', return_value=(None, None, None, None)), \
                    patch.object(interop, 'run_case', return_value=case) as run, contextlib.redirect_stdout(io.StringIO()):
                interop.main()
                run.assert_called_once()
            record = json.loads((output / 'execution.json').read_text())
            self.assertEqual(record['cases'], [case])
            self.assertEqual(record['actual_runtime_root'], str(root))
            self.assertEqual(record['binary_sha256'],
                             {str(binary): hashlib.sha256(binary.read_bytes()).hexdigest()})


if __name__ == '__main__':
    unittest.main(verbosity=2)
