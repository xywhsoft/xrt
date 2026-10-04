"""Reject stale and forged header measurements using actual GCC object profiles."""

from __future__ import annotations

import json
import os
import subprocess
import unittest
from pathlib import Path
from unittest.mock import patch

import measure_extension_header_coverage as extension
import test_auth_coverage_inputs as fixtures


class ExtensionHeaderInputTests(unittest.TestCase):
    def setUp(self):
        fixtures.AuthenticationCoverageInputTests.setUp(self)
        for module in (extension, extension.headers):
            root_patch = patch.object(module, 'ROOT', self.root)
            root_patch.start()
            self.addCleanup(root_patch.stop)
        self.header = self.root / 'extlibs/xjwt/src/private.h'
        self.header.write_text('static inline int positive(int value) {\n'
                               '    if (value > 0) return value;\n'
                               '    return 0;\n}\n', encoding='utf-8')
        (self.root / 'extlibs/xjwt/src/xjwt_main.c').write_text(
            '#include "private.h"\nint owned(int value) {\n'
            '    if (value > 0) return positive(value);\n'
            '    return positive(value);\n}\n', encoding='utf-8')
        for name in ('measure_extension_header_coverage.py', 'gcov_header_coverage.py',
                     'measure_acme_coverage.py', 'measure_mail_header_coverage.py', 'measure_mail_coverage.py'):
            (self.root / 'tools' / name).write_text('# postprocessor fixture\n', encoding='utf-8')
        self.secondary = self.report.with_name('header-functions.json')

    def fresh(self):
        fixtures.coverage.measure('jwt', 'gcc')
        return extension.measure('xjwt')

    def test_real_original_profile_verifies_primary_and_keeps_it_unchanged(self):
        report = self.fresh()
        self.assertTrue(report['primary_owned_c_union_verified'])
        self.assertEqual(report['total']['functions'], 1)
        self.assertEqual(report['total']['branches'], 2)
        self.assertEqual(report['total']['covered_branches'], 1)
        self.assertEqual(len(report['object_profile_sha256']), 2)
        original, secondary = self.report.read_bytes(), self.secondary.read_bytes()
        extension.measure('xjwt', report_only=True)
        self.assertEqual(self.report.read_bytes(), original)
        self.assertEqual(self.secondary.read_bytes(), secondary)

    def test_changed_header_is_rejected_before_rebinding_profiles(self):
        self.fresh()
        original = self.secondary.read_bytes()
        self.header.write_text(self.header.read_text() + '/* changed */\n')
        with self.assertRaisesRegex(RuntimeError, 'stale or unbound'):
            extension.measure('xjwt', report_only=True)
        self.assertEqual(self.secondary.read_bytes(), original)

    def test_forged_primary_gaps_are_rejected_from_actual_profiles(self):
        self.fresh()
        primary = json.loads(self.report.read_text())
        primary['files']['extlibs/xjwt/src/xjwt_main.c']['covered_lines'] -= 1
        self.report.write_text(json.dumps(primary))
        with self.assertRaisesRegex(RuntimeError, 'differ from primary.*files'):
            extension.measure('xjwt')
        self.assertFalse(self.secondary.exists())

    def test_reexecuting_different_branch_invalidates_old_primary(self):
        self.fresh()
        source = self.root / 'extlibs/xjwt/tests/test_jwt.c'
        source.write_text(source.read_text().replace('owned(2)', 'owned(-2)').replace('EXPECTED_VALUE', '0'))
        # The replacement must be executed. Merely changing dependencies cannot
        # demonstrate that counts from a different test run are rejected.
        old = self.report.read_bytes()
        fixtures.coverage.measure('jwt', 'gcc')
        replacement = json.loads(old)
        fresh = json.loads(self.report.read_bytes())
        replacement['dependency_sha256'] = fresh['dependency_sha256']
        self.report.write_text(json.dumps(replacement))
        with self.assertRaisesRegex(RuntimeError, 'differ from primary.*files'):
            extension.measure('xjwt')
        self.assertFalse(self.secondary.exists())

    def test_unemitted_definition_cannot_be_credited(self):
        self.header.write_text(self.header.read_text() +
            'static inline int unused(int value) { return value + 1; }\n')
        fixtures.coverage.measure('jwt', 'gcc')
        with self.assertRaisesRegex(RuntimeError, 'not emitted.*unused'):
            extension.measure('xjwt')
        self.assertFalse(self.secondary.exists())

    def test_forged_secondary_counts_are_rejected_and_retained_for_review(self):
        report = self.fresh()
        report['total']['covered_branches'] = 2
        forged = json.dumps(report).encode()
        self.secondary.write_bytes(forged)
        with self.assertRaisesRegex(RuntimeError, 'differs from original'):
            extension.measure('xjwt', report_only=True)
        self.assertEqual(self.secondary.read_bytes(), forged)

    def test_missing_original_profile_does_not_publish_report(self):
        self.fresh()
        next(self.report.parent.glob('*.gcda')).unlink()
        with self.assertRaises(FileNotFoundError):
            extension.measure('xjwt')
        self.assertFalse(self.secondary.exists())

    def test_no_static_definitions_is_explicitly_not_applicable(self):
        self.header.write_text('/* declarations only */\nint declared(int value);\n')
        (self.root / 'extlibs/xjwt/src/xjwt_main.c').write_text(
            '#include "private.h"\nint owned(int value) { return value; }\n')
        report = self.fresh()
        self.assertFalse(report['applicable'])
        self.assertEqual(report['total']['functions'], 0)
        self.assertEqual(report['total']['lines'], 0)

    def test_changed_postprocessor_invalidates_secondary(self):
        self.fresh()
        (self.root / 'tools/measure_extension_header_coverage.py').write_text('# changed\n')
        with self.assertRaisesRegex(RuntimeError, 'differs from original'):
            extension.measure('xjwt', report_only=True)

    def test_boolean_primary_counter_cannot_impersonate_integer(self):
        self.fresh()
        primary = json.loads(self.report.read_text())
        primary['total']['covered_branches'] = True
        self.report.write_text(json.dumps(primary))
        with self.assertRaisesRegex(RuntimeError, 'differ from primary.*total'):
            extension.measure('xjwt')
        self.assertFalse(self.secondary.exists())

    def test_duplicate_and_nonfinite_report_fields_are_rejected(self):
        self.fresh()
        original = self.report.read_text()
        for invalid in ('{"scope":"main","scope":"main"}', '{"total":NaN}'):
            with self.subTest(invalid=invalid):
                self.report.write_text(invalid)
                with self.assertRaises(RuntimeError):
                    extension.measure('xjwt')
                self.assertFalse(self.secondary.exists())
        self.report.write_text(original)

    def test_mutating_original_counts_during_postprocessing_rejected(self):
        self.fresh()
        original = extension.gcov_json
        def read_then_change(*args):
            document = original(*args)
            profile = args[0].with_suffix('.gcda')
            profile.write_bytes(profile.read_bytes() + b'changed')
            return document
        with patch.object(extension, 'gcov_json', side_effect=read_then_change):
            with self.assertRaisesRegex(RuntimeError, 'changed during private header'):
                extension.measure('xjwt')
        self.assertFalse(self.secondary.exists())

    def test_acme_final_module_and_independent_profiles_reproduce_union(self):
        # Real source objects and standalone embedding are distinct profile kinds.
        # Repeated module executions may change contributions, never denominators.
        root_patch = patch.object(extension.acme, 'ROOT', self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)
        manifest = self.root / 'extlibs/xacme/config/modules.json'
        manifest.parent.mkdir(parents=True)
        manifest_patch = patch.object(extension.acme, 'MANIFEST', manifest)
        manifest_patch.start()
        self.addCleanup(manifest_patch.stop)
        source = self.root / 'extlibs/xacme/src/owned.c'
        source.parent.mkdir(parents=True)
        header = source.with_name('private.h')
        header.write_text(self.header.read_text())
        source.write_text('#include "private.h"\nint owned(int value) {\n'
                          '    if (value > 0) return positive(value);\n'
                          '    return positive(value);\n}\n')
        manifest.write_text(json.dumps({'modules': [{'sources': ['extlibs/xacme/src/owned.c']}]}))
        directory = self.root / f'out/acme/coverage/{extension.sys.platform}'
        directory.mkdir(parents=True)
        primary = directory / 'coverage.json'
        primary.write_text(json.dumps({'scope': 'module_mock_independent',
                                       'source_sha256': extension.hashes([source])}))
        probes = self.root / 'extlibs/xacme/tests/acme'
        probes.mkdir(parents=True)
        for name in ('test_http_interop_client.c', 'test_lifecycle_interop_client.c',
                     'test_provider_body_client.c', 'test_provider_wire_client.c'):
            (probes / name).write_text('#include "../../src/owned.c"\n')
        interop = directory / 'interop'
        interop.mkdir()
        for stem in ('acme_http_client', 'acme_ownership_client',
                     'acme_provider_body_client', 'acme_provider_wire_client'):
            (interop / (stem + '-fixture.gcno')).write_bytes(b'placeholder')
        _, profiles = extension.original_profiles('xacme', json.loads(primary.read_text()))
        union = extension.CoverageUnion(self.root, [source])
        for index, (note, selected) in enumerate(profiles):
            note.parent.mkdir(parents=True, exist_ok=True)
            obj = note.with_suffix('.o')
            probe = note.with_suffix('.c')
            probe.write_text('#include "' + source.as_posix() + '"\n'
                             'int main(void) { return owned(' + ('2' if index == 0 else '-2') +
                             ') != ' + ('2' if index == 0 else '0') + '; }\n')
            binary = note.with_suffix('.exe' if os.name == 'nt' else '.bin')
            subprocess.run(['gcc', '-O0', '--coverage', '-c', str(probe), '-o', str(obj)], check=True)
            subprocess.run(['gcc', '--coverage', str(obj), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            document = extension.gcov_json(note, 'gcov')
            union.add(str(index), document, sources=selected)
            if index == 0:
                union.add('repeated module execution', document, sources=selected)
        document = {**union.report(), 'scope': 'module_mock_independent',
                    'dependency_sha256': extension.hashes([source, header, manifest, *probes.glob('*.c')]),
                    'compile_flags': ['-O0', '--coverage']}
        primary.write_text(json.dumps(document))
        original = primary.read_bytes()
        report = extension.measure('xacme')
        self.assertEqual(report['total']['functions'], 1)
        self.assertEqual(report['total']['branches'], 2)
        self.assertEqual(report['total']['covered_branches'], 2)
        self.assertEqual(len(report['object_profile_sha256']), 10)
        extension.measure('xacme', report_only=True)
        self.assertEqual(primary.read_bytes(), original)


if __name__ == '__main__':
    unittest.main(verbosity=2)
