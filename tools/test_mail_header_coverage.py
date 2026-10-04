"""Check private-header coverage against real GCC translation units and reports."""

from __future__ import annotations

import copy
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from gcov_coverage import compiler_gcov, gcov_json
from gcov_header_coverage import HeaderFunctionUnion
import measure_mail_header_coverage as headers
import test_mail_coverage_inputs as fixtures

HEADER = '''static inline int positive(int value) {
    if (value > 0) return value;
    return 0;
}
static inline int negative(int value) {
    if (value < 0) return -value;
    return 0;
}
'''


class HeaderFunctionUnionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='mail-header-union-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.header = self.root / 'private.h'
        self.header.write_text(HEADER, encoding='utf-8')
        self.inventory = {self.header: ['positive', 'negative']}
        self.documents = []
        for index, expression in enumerate(('positive(2) != 2',
                                            'positive(-1) != 0 || negative(-2) != 2',
                                            'negative(1) != 0')):
            source = self.root / f'probe{index}.c'
            source.write_text('#include "private.h"\nint main(void) { return ' + expression + '; }\n', encoding='utf-8')
            obj = source.with_suffix('.o')
            binary = source.with_suffix('.exe' if os.name == 'nt' else '.bin')
            subprocess.run(['gcc', '-O0', '--coverage', '-c', str(source), '-o', str(obj)], check=True)
            subprocess.run(['gcc', '--coverage', str(obj), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            self.documents.append(gcov_json(obj.with_suffix('.gcno'), compiler_gcov('gcc')))

    def union(self):
        return HeaderFunctionUnion(self.root, self.inventory)

    def test_different_emitted_subsets_merge_without_denominator_inflation(self):
        union = self.union()
        for index, document in enumerate(self.documents):
            union.add(str(index), document)
        report = union.report()
        self.assertEqual(report['total']['functions'], 2)
        self.assertEqual(report['total']['lines'], 6)
        self.assertEqual(report['total']['covered_lines'], 6)
        self.assertEqual(report['total']['branches'], 4)
        self.assertEqual(report['total']['covered_branches'], 4)
        self.assertEqual(report['profiles'][0]['compiled_functions'], {'private.h': ['positive']})
        self.assertEqual(report['profiles'][2]['new_branches'], 1)
        union.add('duplicate executions', self.documents[1])
        self.assertEqual(union.report()['total'], report['total'])

    def test_inventory_requires_every_declared_function_to_have_been_emitted(self):
        union = self.union()
        union.add('positive only', self.documents[0])
        with self.assertRaisesRegex(RuntimeError, 'not emitted.*negative'):
            union.report()

    def test_changed_control_flow_is_rejected_without_changing_prior_union(self):
        union = self.union()
        for index, document in enumerate(self.documents):
            union.add(str(index), document)
        original = union.report()
        document = copy.deepcopy(self.documents[1])
        file = next(file for file in document['files'] if file['file'].endswith('private.h'))
        file['functions'][0]['blocks'] += 1
        with self.assertRaisesRegex(RuntimeError, 'incompatible.*control-flow'):
            union.add('changed CFG', document)
        self.assertEqual(union.report(), original)

    def test_changed_header_source_is_rejected(self):
        union = self.union()
        union.add('first', self.documents[0])
        self.header.write_text(HEADER + '/* changed */\n', encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'source changed'):
            union.add('second', self.documents[1])

    def test_different_compiler_version_is_rejected(self):
        union = self.union()
        union.add('first', self.documents[0])
        document = copy.deepcopy(self.documents[1])
        document['gcc_version'] += '.different'
        with self.assertRaisesRegex(RuntimeError, 'different GCC'):
            union.add('second', document)

    def test_unknown_function_and_duplicate_line_and_bad_counter_are_rejected(self):
        for kind in ('unknown function', 'duplicate line', 'boolean counter'):
            with self.subTest(kind=kind):
                document = copy.deepcopy(self.documents[0])
                file = next(file for file in document['files'] if file['file'].endswith('private.h'))
                if kind == 'unknown function':
                    file['functions'][0]['name'] = 'unlisted'
                elif kind == 'duplicate line':
                    file['lines'].append(copy.deepcopy(file['lines'][0]))
                else:
                    file['lines'][0]['count'] = True
                with self.assertRaises(RuntimeError):
                    self.union().add('invalid', document)


class HeaderReportInputTests(unittest.TestCase):
    build_fixture = fixtures.MailCoverageInputTests.build_fixture
    build_independent_fixture = fixtures.MailCoverageInputTests.build_independent_fixture
    fresh_union = fixtures.MailCoverageInputTests.fresh_union

    def setUp(self):
        fixtures.MailCoverageInputTests.setUp(self)
        root_patch = patch.object(headers, 'ROOT', self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)
        self.header = self.root / 'extlibs/xmail/src/internal/private.h'
        self.header.parent.mkdir(parents=True)
        self.header.write_text(HEADER.split('static inline int negative')[0], encoding='utf-8')
        (self.root / 'extlibs/xmail/src/mail/owned.c').write_text(
            '#include "../internal/private.h"\nint owned(int value) { return positive(value); }\n', encoding='utf-8')
        for name in ('measure_mail_header_coverage.py', 'gcov_header_coverage.py'):
            (self.root / 'tools' / name).write_text('# postprocessor fixture\n', encoding='utf-8')
        self.header_report = self.report.with_name('header-functions.json')

    def fresh(self):
        self.fresh_union()
        return headers.measure('xmail', 'gcc', 'gcov')

    def test_verified_original_profiles_cover_header_without_rewriting_primary(self):
        report = self.fresh()
        self.assertEqual(report['scope'], headers.SCOPE)
        self.assertEqual(report['total']['functions'], 1)
        self.assertEqual(report['total']['branches'], 2)
        self.assertEqual(report['total']['covered_branches'], 2)
        primary, original = self.report.read_bytes(), self.header_report.read_bytes()
        headers.measure('xmail', 'gcc', 'gcov', True)
        self.assertEqual(self.report.read_bytes(), primary)
        self.assertEqual(self.header_report.read_bytes(), original)

    def test_stale_header_cannot_rebind_the_primary_profiles(self):
        self.fresh()
        original = self.header_report.read_bytes()
        self.header.write_text(self.header.read_text() + '/* modified */\n', encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'stale'):
            headers.measure('xmail', 'gcc', 'gcov', True)
        self.assertEqual(self.header_report.read_bytes(), original)

    def test_changed_postprocessor_requires_a_new_secondary_report(self):
        self.fresh()
        (self.root / 'tools/gcov_header_coverage.py').write_text('# changed\n', encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'differs'):
            headers.measure('xmail', 'gcc', 'gcov', True)

    def test_forged_header_counts_are_rejected(self):
        report = self.fresh()
        report['total']['covered_branches'] = 0
        self.header_report.write_text(json.dumps(report), encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'differs'):
            headers.measure('xmail', 'gcc', 'gcov', True)

    def test_unbound_primary_report_does_not_publish_header_report(self):
        with self.assertRaises((RuntimeError, FileNotFoundError)):
            headers.measure('xmail', 'gcc', 'gcov')
        self.assertFalse(self.header_report.exists())

    def test_new_definition_that_was_never_emitted_cannot_be_credited(self):
        self.header.write_text(HEADER, encoding='utf-8')
        self.fresh_union()
        with self.assertRaisesRegex(RuntimeError, 'not emitted.*negative'):
            headers.measure('xmail', 'gcc', 'gcov')
        self.assertFalse(self.header_report.exists())

    def test_comment_string_and_static_data_are_not_function_definitions(self):
        self.header.write_text(self.header.read_text() + '\n'
            '/* static int fake(int x) { return x; } */\n'
            'static const char *text = "static int fake2(int x) { return x; }";\n'
            'static int declaration(int value);\n', encoding='utf-8')
        self.assertEqual(headers.header_inventory('xmail')[self.header], ['positive'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
