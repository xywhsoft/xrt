"""Exercise mail coverage provenance with actual GCC objects and executed counters."""

from __future__ import annotations

import json
import hashlib
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import measure_mail_coverage as coverage


class MailCoverageInputTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix='mail-coverage-inputs-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        files = {
            'config/modules.json': '{"modules": []}\n',
            'include/xrt/fixture.h': '/* modular runtime */\n',
            'src/core/fixture.c': '/* runtime source */\n',
            'single/xrt.h': '/* single runtime */\n',
            'tests/test.h': '/* harness */\n',
            'tests/fixtures/mail_input.h': '#define EXPECTED 2\n',
            'extlibs/xmail/src/mail/owned.c':
                'int owned(int value) {\n    if (value > 0) return value;\n    return 0;\n}\n',
            'extlibs/xmail/tests/main.c':
                '#include "../../../tests/fixtures/mail_input.h"\nint owned(int);\n'
                'int main(void) { return owned(2) != EXPECTED; }\n',
        }
        for product in coverage.SUITES:
            sources = ['extlibs/xmail/src/mail/owned.c'] if product == 'xmail' else []
            files[f'extlibs/{product}/config/modules.json'] = json.dumps({'modules': [{'sources': sources}]})
        for name in ('measure_mail_coverage.py', 'gcov_coverage.py', 'build.py', 'amalgamate.py',
                     'xrt_manifest.py', 'xrt_text.py', 'generate_features.py', 'generate_extension_features.py',
                     'test_mail_tls_interop.py'):
            files['tools/' + name] = '# build input\n'
        for relative, content in files.items():
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8')
        root_patch = patch.object(coverage, 'ROOT', self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)
        interop_root_patch = patch.object(coverage.interop, 'ROOT', self.root)
        interop_root_patch.start()
        self.addCleanup(interop_root_patch.stop)
        build_patch = patch.object(coverage, '_build', side_effect=self.build_fixture)
        build_patch.start()
        self.addCleanup(build_patch.stop)
        self.report = self.root / 'out/mail/coverage' / sys.platform / 'xmail/coverage.json'

    def build_fixture(self, product, suite, compiler, object_dir):
        object_dir.mkdir(parents=True, exist_ok=True)
        source = self.root / 'extlibs/xmail/src/mail/owned.c'
        obj = object_dir / 'extlibs__xmail__src__mail__owned.o'
        binary = object_dir.parent / ('fixture.exe' if os.name == 'nt' else 'fixture')
        subprocess.run([compiler, '-O0', '--coverage', '-c', str(source), '-o', str(obj)], check=True)
        subprocess.run([compiler, '-O0', '--coverage', str(self.root / 'extlibs/xmail/tests/main.c'),
                        str(obj), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

    def fresh(self):
        coverage.measure('xmail', 'gcc', 'gcov', False, include_interop=False)
        self.assertTrue(self.report.is_file(), 'executed profiles need a persisted provenance report')
        return json.loads(self.report.read_text(encoding='utf-8'))

    def test_unbound_raw_counters_cannot_be_reported_as_current(self):
        object_dir = coverage._object_dir('xmail', coverage.SUITES['xmail'], 'gcc')
        self.build_fixture('xmail', coverage.SUITES['xmail'], 'gcc', object_dir)
        with self.assertRaisesRegex(RuntimeError, 'provenance|report'):
            coverage.measure('xmail', 'gcc', 'gcov', True, include_interop=False)
        self.assertFalse(self.report.exists())

    def test_executed_fixture_has_exact_counts_and_test_hashes(self):
        report = self.fresh()
        self.assertEqual(report['scope'], 'module_suite_object_profiles')
        self.assertEqual(report['total']['branches'], 2)
        self.assertEqual(report['total']['covered_branches'], 1)
        self.assertIn('extlibs/xmail/tests/main.c', report['dependency_sha256'])
        self.assertIn('tests/fixtures/mail_input.h', report['dependency_sha256'])
        self.assertEqual(set(report['source_sha256']), {'extlibs/xmail/src/mail/owned.c'})
        original = self.report.read_bytes()
        self.assertEqual(coverage.measure('xmail', 'gcc', 'gcov', True, include_interop=False),
                         (report['total']['lines_percent'] / 100, report['total']['branches_percent'] / 100))
        self.assertEqual(self.report.read_bytes(), original)

    def test_changed_test_during_actual_counter_read_rejected(self):
        self.report.parent.mkdir(parents=True)
        self.report.write_text('{"obsolete": true}\n', encoding='utf-8')
        name = 'gcov_json' if hasattr(coverage, 'gcov_json') else '_gcov'
        original = getattr(coverage, name)
        def read_then_change(*args):
            result = original(*args)
            (self.root / 'tests/fixtures/mail_input.h').write_text('#define EXPECTED 99\n', encoding='utf-8')
            return result
        with patch.object(coverage, name, side_effect=read_then_change):
            with self.assertRaisesRegex(RuntimeError, 'changed during coverage measurement'):
                coverage.measure('xmail', 'gcc', 'gcov', False, include_interop=False)
        self.assertFalse(self.report.exists(), 'failed measurement must not leave an obsolete report')

    def test_stale_test_fixture_does_not_rebind_old_counters(self):
        self.fresh()
        original = self.report.read_bytes()
        (self.root / 'tests/fixtures/mail_input.h').write_text('#define EXPECTED 99\n', encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'stale|changed|match'):
            coverage.measure('xmail', 'gcc', 'gcov', True, include_interop=False)
        self.assertEqual(self.report.read_bytes(), original)

    def test_stale_owned_source_preserves_original_report(self):
        self.fresh()
        original = self.report.read_bytes()
        source = self.root / 'extlibs/xmail/src/mail/owned.c'
        source.write_text(source.read_text(encoding='utf-8') + '/* changed */\n', encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'stale'):
            coverage.measure('xmail', 'gcc', 'gcov', True, include_interop=False)
        self.assertEqual(self.report.read_bytes(), original)

    def test_changed_executed_counter_is_rejected(self):
        report = self.fresh()
        profile = next(path for path in report['object_profile_sha256'] if path.endswith('.gcda'))
        with (self.root / profile).open('ab') as stream:
            stream.write(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'profiles changed'):
            coverage.measure('xmail', 'gcc', 'gcov', True, include_interop=False)

    def test_forged_report_values_are_rejected_against_actual_profiles(self):
        original = self.fresh()
        for value in (-1, True, float('nan'), float('inf'), 999):
            with self.subTest(value=value):
                report = json.loads(json.dumps(original))
                report['total']['covered_branches'] = value
                self.report.write_text(json.dumps(report), encoding='utf-8')
                with self.assertRaises(RuntimeError):
                    coverage.measure('xmail', 'gcc', 'gcov', True, include_interop=False)

    def test_duplicate_json_fields_are_rejected(self):
        self.fresh()
        text = self.report.read_text(encoding='utf-8')
        self.report.write_text(text.replace('"schema": 2', '"schema": 2, "schema": 2'), encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'duplicate'):
            coverage.measure('xmail', 'gcc', 'gcov', True, include_interop=False)

    def test_missing_owned_manifest_source_is_rejected(self):
        extra = self.root / 'extlibs/xmail/src/mail/missing.c'
        extra.write_text('int missing(void) { return 2; }\n', encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'every owned C source'):
            coverage.measure('xmail', 'gcc', 'gcov', False, include_interop=False)

    def test_missing_executed_profile_is_rejected(self):
        report = self.fresh()
        profile = next(path for path in report['object_profile_sha256'] if path.endswith('.gcda'))
        (self.root / profile).unlink()
        with self.assertRaisesRegex(RuntimeError, 'original executed'):
            coverage.measure('xmail', 'gcc', 'gcov', True, include_interop=False)

    def test_host_profile_directories_are_distinct(self):
        self.assertIn('native-windows' if os.name == 'nt' else 'native-linux',
                      coverage._object_dir('xmail', coverage.SUITES['xmail'], 'gcc').parts)

    def test_default_cli_enforces_product_floor(self):
        self.fresh()
        with patch.object(sys, 'argv', ['measure_mail_coverage.py', '--product', 'xmail', '--report-only', '--module-only']):
            self.assertEqual(coverage.main(), 1, 'fixture coverage is below the product CI floor')

    def test_nan_threshold_is_rejected(self):
        object_dir = coverage._object_dir('xmail', coverage.SUITES['xmail'], 'gcc')
        self.build_fixture('xmail', coverage.SUITES['xmail'], 'gcc', object_dir)
        with patch.object(sys, 'argv', ['measure_mail_coverage.py', '--product', 'xmail',
                                      '--report-only', '--min-lines', 'nan']):
            with self.assertRaises(SystemExit) as rejected:
                coverage.main()
            self.assertEqual(rejected.exception.code, 2)

    def build_independent_fixture(self, product, compiler):
        protocols = coverage.interop.coverage_protocols(product)
        cases = []
        binaries = []
        for protocol in protocols:
            directory = coverage._interop_directory(product, protocol, compiler)
            objects = directory / 'obj'
            objects.mkdir(parents=True)
            source = self.root / 'extlibs/xmail/src/mail/owned.c'
            obj = objects / 'extlibs__xmail__src__mail__owned.o'
            probe = self.root / f'extlibs/xmail/examples/{protocol}.c'
            probe.parent.mkdir(parents=True, exist_ok=True)
            probe.write_text('int owned(int);\nint main(void) { return owned(-1) != 0; }\n', encoding='utf-8')
            binary = directory / (coverage.interop.EXAMPLES[protocol][2] + ('.exe' if os.name == 'nt' else ''))
            subprocess.run([compiler, '-O0', '--coverage', '-c', str(source), '-o', str(obj)], check=True)
            subprocess.run([compiler, '-O0', '--coverage', str(probe), str(obj), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            binaries.append(binary)
            for mode in ('tls', 'starttls'):
                for hostname, reject, truncated, close_notify in [
                        ('localhost', False, False, True), ('localhost', False, False, False),
                        ('localhost', False, True, True), ('127.0.0.1', False, False, True),
                        ('127.0.0.1', True, False, True)]:
                    cases.append({'protocol': protocol, 'mode': mode, 'hostname': hostname,
                                  'reject_identity': reject, 'truncated_response': truncated,
                                  'close_notify': close_notify})
        report = {'schema': 2, 'product': product, 'platform': sys.platform,
                  'compile_flags': coverage.FLAGS, 'ipv6_available': False, 'cases': cases,
                  'binary_sha256': {p.relative_to(self.root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in binaries}}
        path = coverage._execution_path(product)
        path.parent.mkdir(parents=True)
        path.write_text(json.dumps(report), encoding='utf-8')

    def fresh_union(self):
        # Put the example source in the original input set, before measurement.
        for protocol in ('pop3', 'smtp', 'imap'):
            path = self.root / f'extlibs/xmail/examples/{protocol}.c'
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('int owned(int);\nint main(void) { return owned(-1) != 0; }\n', encoding='utf-8')
        with patch.object(coverage, '_build_interop', side_effect=self.build_independent_fixture):
            coverage.measure('xmail', 'gcc', 'gcov')
        return json.loads(self.report.read_text(encoding='utf-8'))

    def test_independent_actual_objects_cover_the_other_outcome_without_denominator_inflation(self):
        report = self.fresh_union()
        self.assertEqual(report['scope'], 'module_and_independent_tls_object_profiles')
        self.assertEqual(report['total']['branches'], 2)
        self.assertEqual(report['total']['covered_branches'], 2)
        self.assertEqual(len(report['profiles']), 4)
        self.assertEqual(report['profiles'][1]['new_branches'], 1)
        self.assertEqual(report['profiles'][2]['new_branches'], 0)
        self.assertEqual(len(report['independent_tls_execution']['cases']), 30)
        self.assertIn('tools/test_mail_tls_interop.py', report['dependency_sha256'])
        self.assertIn('extlibs/xmail/examples/pop3.c', report['dependency_sha256'])
        original = self.report.read_bytes()
        coverage.measure('xmail', 'gcc', 'gcov', True)
        self.assertEqual(self.report.read_bytes(), original)

    def test_changed_independent_executable_is_rejected(self):
        report = self.fresh_union()
        binary = next(iter(report['independent_tls_execution']['binary_sha256']))
        with (self.root / binary).open('ab') as stream:
            stream.write(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'profiles changed'):
            coverage.measure('xmail', 'gcc', 'gcov', True)

    def test_missing_independent_counter_or_case_is_rejected(self):
        report = self.fresh_union()
        counter = next(p for p in report['object_profile_sha256']
                       if ',' in p and p.endswith('.gcda'))
        original = (self.root / counter).read_bytes()
        (self.root / counter).unlink()
        with self.assertRaisesRegex(RuntimeError, 'original executed'):
            coverage.measure('xmail', 'gcc', 'gcov', True)
        (self.root / counter).write_bytes(original)
        path = coverage._execution_path('xmail')
        data = json.loads(path.read_text())
        data['cases'].pop()
        path.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'profiles changed'):
            coverage.measure('xmail', 'gcc', 'gcov', True)

    def test_partial_execution_rejected_before_report_publication(self):
        for protocol in ('pop3', 'smtp', 'imap'):
            path = self.root / f'extlibs/xmail/examples/{protocol}.c'
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('int owned(int);\nint main(void) { return owned(-1) != 0; }\n', encoding='utf-8')
        def partial(product, compiler):
            self.build_independent_fixture(product, compiler)
            path = coverage._execution_path(product)
            data = json.loads(path.read_text())
            data['cases'].pop()
            path.write_text(json.dumps(data), encoding='utf-8')
        with patch.object(coverage, '_build_interop', side_effect=partial):
            with self.assertRaisesRegex(RuntimeError, 'every required case'):
                coverage.measure('xmail', 'gcc', 'gcov')
        self.assertFalse(self.report.exists())

    def test_diagnostic_reports_cannot_be_promoted_to_the_full_scope(self):
        self.fresh()
        original = self.report.read_bytes()
        with self.assertRaisesRegex(RuntimeError, 'scope'):
            coverage.measure('xmail', 'gcc', 'gcov', True)
        self.assertEqual(self.report.read_bytes(), original)

    def test_independent_directories_are_isolated_by_measured_product(self):
        first = coverage._interop_directory('xmail', 'pop3', 'gcc')
        second = coverage._interop_directory('xpop3', 'pop3', 'gcc')
        self.assertNotEqual(first, second)
        self.assertEqual(first.name, 'pop3_client_example,xmail')
        self.assertIn('native-windows' if os.name == 'nt' else 'native-linux', first.parts)

    def test_complete_operation_without_close_notify_is_required_and_typed(self):
        self.build_independent_fixture('xmail', 'gcc')
        path = coverage._execution_path('xmail')
        original = json.loads(path.read_text())
        coverage._execution('xmail', 'gcc')
        for mutation in ('missing', 'duplicate', 'wrong_type', 'old_schema'):
            with self.subTest(mutation=mutation):
                data = json.loads(json.dumps(original))
                index = next(i for i, case in enumerate(data['cases']) if not case['close_notify'])
                if mutation == 'missing':
                    data['cases'].pop(index)
                elif mutation == 'duplicate':
                    data['cases'].append(data['cases'][index])
                elif mutation == 'wrong_type':
                    data['cases'][index]['close_notify'] = 0
                else:
                    data['schema'] = 1
                path.write_text(json.dumps(data), encoding='utf-8')
                with self.assertRaises(RuntimeError):
                    coverage._execution('xmail', 'gcc')


if __name__ == '__main__':
    unittest.main(verbosity=2)
