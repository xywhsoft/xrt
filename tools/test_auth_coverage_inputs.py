"""Check authentication coverage provenance using real GCC profiles and changed test inputs."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import measure_auth_coverage as coverage


class AuthenticationCoverageInputTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="auth-coverage-inputs-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        files = {
            "single/xrt.h": "/* fixture runtime */\n",
            "extlibs/xjwt/xjwt.h": "int owned(int value);\n",
            "extlibs/xjwt/xjwt.c": '#include "../../single/xrt.h"\n#include "src/xjwt_main.c"\n',
            "extlibs/xjwt/src/xjwt_main.c": "int owned(int value) {\n    if (value > 0) return value;\n    return 0;\n}\n",
            "extlibs/xjwt/tests/test_keys.h": "#define EXPECTED_VALUE 2\n",
            "extlibs/xjwt/tests/test_jwt.c": '#include "../xjwt.c"\n#include "test_keys.h"\nint main(void) { return owned(2) != EXPECTED_VALUE; }\n',
            "tools/measure_auth_coverage.py": "# fixture measurement orchestration\n",
            "tools/gcov_coverage.py": "# fixture counter aggregation\n",
        }
        for name, content in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
        root_patch = patch.object(coverage, "ROOT", self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)
        self.report = self.root / "out/auth_extensions/coverage" / coverage.sys.platform / "jwt/coverage.json"

    def reject_change(self, relative: str, replacement: str) -> None:
        self.report.parent.mkdir(parents=True)
        self.report.write_text('{"obsolete":true}\n', encoding="utf-8")
        original = coverage.gcov_json
        def read_then_change(*args):
            result = original(*args)
            (self.root / relative).write_text(replacement, encoding="utf-8")
            return result
        with patch.object(coverage, "gcov_json", side_effect=read_then_change):
            with self.assertRaisesRegex(RuntimeError, "changed during coverage measurement"):
                coverage.measure("jwt", "gcc")
        self.assertFalse(self.report.exists(), "failed measurement must not leave a stale or new report")

    def test_changed_key_fixture_rejected_after_actual_execution(self) -> None:
        self.reject_change("extlibs/xjwt/tests/test_keys.h", "#define EXPECTED_VALUE 99\n")

    def test_changed_main_test_rejected_after_actual_execution(self) -> None:
        self.reject_change("extlibs/xjwt/tests/test_jwt.c", "int main(void) { return 1; }\n")

    def test_unchanged_executed_fixture_records_test_provenance(self) -> None:
        coverage.measure("jwt", "gcc")
        report = json.loads(self.report.read_text(encoding="utf-8"))
        self.assertEqual(report["scope"], "main")
        self.assertIn("extlibs/xjwt/tests/test_keys.h", report["dependency_sha256"])
        self.assertIn("extlibs/xjwt/tests/test_jwt.c", report["dependency_sha256"])
        self.assertIn("tools/measure_auth_coverage.py", report["dependency_sha256"])
        self.assertEqual(report["total"]["branches"], 2)
        self.assertEqual(report["total"]["covered_branches"], 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
