"""Validate coverage unions with real GCC profiles and incompatible/corrupt profile inputs."""

from __future__ import annotations

import argparse
import copy
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from gcov_coverage import CoverageUnion, compiler_gcov, gcov_json


class CoverageTests(unittest.TestCase):
    compiler = "gcc"

    @classmethod
    def setUpClass(cls) -> None:
        cls.temp = tempfile.TemporaryDirectory(prefix="gcov-union-test-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name).resolve()
        cls.owned = cls.root / "owned.c"
        cls.owned.write_text("int exercise(int value) {\n"
                             "    int result = 0;\n"
                             "    if (value > 0 && value != 7) result++;\n"
                             "    if (value < 0) result += 2;\n"
                             "    return result;\n}\n", encoding="utf-8")
        gcov = compiler_gcov(cls.compiler)
        cls.documents = []
        for index, value in enumerate((1, -1, 7)):
            source = cls.root / f"probe{index}.c"
            source.write_text('#include "owned.c"\n'
                              f"int main(void) {{ return exercise({value}) < 0; }}\n", encoding="utf-8")
            binary = cls.root / (f"probe{index}" + (".exe" if os.name == "nt" else ""))
            subprocess.run([cls.compiler, "-std=c11", "-O0", "--coverage", "-fprofile-update=atomic",
                            "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)],
                           cwd=cls.root, check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], cwd=cls.root, check=True)
            notes = list(cls.root.glob(f"probe{index}*.gcno"))
            if len(notes) != 1:
                raise RuntimeError(f"unexpected test notes: {notes}")
            cls.documents.append(gcov_json(notes[0], gcov))
        print(f"real profiles: GCC {cls.documents[0]['gcc_version']}, "
              f"gcov JSON {cls.documents[0]['format_version']}", flush=True)
        cls.second = cls.root / "second.c"
        cls.second.write_text("int second(int value) { return value < 0 ? 3 : 4; }\n", encoding="utf-8")
        source = cls.root / "both.c"
        source.write_text('#include "owned.c"\n#include "second.c"\n'
                          'int main(void) { return exercise(1) + second(1) != 5; }\n', encoding="utf-8")
        binary = cls.root / ("both" + (".exe" if os.name == "nt" else ""))
        subprocess.run([cls.compiler, "-std=c11", "-O0", "--coverage", "-fprofile-update=atomic",
                        "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)],
                       cwd=cls.root, check=True, capture_output=True, text=True)
        subprocess.run([str(binary)], cwd=cls.root, check=True)
        notes = list(cls.root.glob("both*.gcno"))
        if len(notes) != 1:
            raise RuntimeError(f"unexpected combined test notes: {notes}")
        cls.both_document = gcov_json(notes[0], gcov)

    def union(self) -> CoverageUnion:
        return CoverageUnion(self.root, [self.owned])

    def owned_file(self, document: dict) -> dict:
        return next(file for file in document["files"]
                    if Path(file["file"]).resolve() == self.owned)

    def test_actual_branch_union_and_replay_do_not_inflate_denominator(self) -> None:
        union = self.union()
        union.add("positive", self.documents[0])
        first = union.report()["total"]
        self.assertEqual(first["branches"], 6)
        self.assertEqual(first["covered_branches"], 3)
        union.add("negative", self.documents[1])
        second = union.report()["total"]
        self.assertEqual(second["branches"], 6)
        self.assertEqual(second["covered_branches"], 5)
        union.add("seven", self.documents[2])
        before_replay = union.report()["total"]
        self.assertEqual(before_replay["covered_branches"], 6)
        union.add("positive replay", self.documents[0])
        report = union.report()
        self.assertEqual(report["total"], before_replay)
        self.assertEqual(report["profiles"][-1]["new_lines"], 0)
        self.assertEqual(report["profiles"][-1]["new_branches"], 0)
        self.assertEqual(report["total"]["lines_percent"], 100.0)

    def test_legacy_ordered_outcomes_and_relative_paths(self) -> None:
        union = self.union()
        for index, original in enumerate(self.documents):
            document = copy.deepcopy(original)
            document["format_version"] = "1"
            file = self.owned_file(document)
            file["file"] = "owned.c"
            document["current_working_directory"] = str(self.root)
            for line in file["lines"]:
                line.pop("block_ids", None)
                for branch in line["branches"]:
                    branch.pop("source_block_id", None)
                    branch.pop("destination_block_id", None)
            union.add(str(index), document)
        self.assertEqual(union.report()["total"]["covered_branches"], 6)

    def test_explicit_partial_profiles_cannot_shrink_owned_denominators(self) -> None:
        union = CoverageUnion(self.root, [self.owned, self.second])
        union.add("partial", self.documents[0], sources=[self.owned])
        with self.assertRaisesRegex(RuntimeError, "missing owned source definitions"):
            union.report()
        union.add("both", self.both_document)
        for index in (1, 2):
            union.add(str(index), self.documents[index], sources=[self.owned])
        report = union.report()
        self.assertEqual(report["total"]["branches"], 8)
        self.assertEqual(report["total"]["covered_branches"], 7)
        self.assertEqual(report["profiles"][0]["compiled_sources"], ["owned.c"])
        self.assertEqual(set(report["files"]), {"owned.c", "second.c"})

    def test_partial_scope_must_exactly_match_the_compiled_owned_files(self) -> None:
        for document, scope in ((self.both_document, [self.owned]),
                                (self.documents[0], [self.owned, self.second]),
                                (self.documents[0], []),
                                (self.documents[0], [self.owned, self.owned]),
                                (self.documents[0], [self.root / "unknown.c"])):
            with self.subTest(scope=scope):
                union = CoverageUnion(self.root, [self.owned, self.second])
                with self.assertRaises(RuntimeError):
                    union.add("invalid scope", document, sources=scope)
                self.assertEqual(union.files, {})
                self.assertEqual(union.profiles, [])

    def reject(self, mutate) -> None:
        union = self.union()
        union.add("base", self.documents[0])
        before = union.report()
        document = copy.deepcopy(self.documents[1])
        mutate(document, self.owned_file(document))
        with self.assertRaises((RuntimeError, KeyError)):
            union.add("corrupt", document)
        self.assertEqual(union.report(), before)

    def test_missing_or_duplicate_sources_rejected_without_partial_merge(self) -> None:
        self.reject(lambda document, file: document["files"].remove(file))
        self.reject(lambda document, file: document["files"].append(file))
        self.reject(lambda document, file: file.update(file=str(self.root / "not-owned.c")))

    def test_incompatible_versions_and_control_flow_rejected(self) -> None:
        self.reject(lambda document, file: document.update(gcc_version="different compiler"))
        self.reject(lambda document, file: document.update(format_version="99"))
        self.reject(lambda document, file: file["functions"][0].update(blocks=999))
        self.reject(lambda document, file: file["functions"][0].update(start_line=999))
        def swap_outcomes(document, file):
            line = next(line for line in file["lines"] if line["branches"])
            line["branches"].reverse()
        self.reject(swap_outcomes)
        if self.documents[0]["format_version"] == "2":
            def change_edge(document, file):
                line = next(line for line in file["lines"] if line["branches"])
                line["branches"][0]["destination_block_id"] = 999
            self.reject(change_edge)

    def test_invalid_counts_duplicate_lines_and_unknown_functions_rejected(self) -> None:
        for count in (-1, 0.5, True, "0"):
            with self.subTest(count=count):
                self.reject(lambda document, file: file["lines"][0].update(count=count))
        self.reject(lambda document, file: file["lines"].append(file["lines"][0]))
        self.reject(lambda document, file: file["lines"][0].update(function_name="unknown"))
        self.reject(lambda document, file: file["functions"].append(file["functions"][0]))
        def negative_branch(document, file):
            line = next(line for line in file["lines"] if line["branches"])
            line["branches"][0]["count"] = -1
        self.reject(negative_branch)

    def test_source_change_and_unexecuted_profile_rejected(self) -> None:
        union = self.union()
        union.add("base", self.documents[0])
        original = self.owned.read_bytes()
        try:
            self.owned.write_bytes(original + b"/* changed after compilation */\n")
            with self.assertRaisesRegex(RuntimeError, "source changed"):
                union.add("changed", self.documents[1])
            with self.assertRaisesRegex(RuntimeError, "source changed"):
                union.report()
        finally:
            self.owned.write_bytes(original)
        note = self.root / "unexecuted.gcno"
        note.touch()
        with self.assertRaisesRegex(RuntimeError, "missing executed"):
            gcov_json(note, compiler_gcov(self.compiler))

    def test_duplicate_labels_and_empty_reports_rejected(self) -> None:
        union = self.union()
        with self.assertRaisesRegex(RuntimeError, "without executed"):
            union.report()
        union.add("main", self.documents[0])
        with self.assertRaisesRegex(RuntimeError, "duplicate.*label"):
            union.add("main", self.documents[1])

    def test_nonfinite_or_out_of_range_gate_cannot_silently_pass(self) -> None:
        gates = (("measure_auth_coverage.py", ("--library", "oauth2", "--min-oauth2")),
                 ("measure_acme_coverage.py", ("--min-lines",)),
                 ("measure_acme_coverage.py", ("--min-branches",)))
        for name, arguments in gates:
            script = Path(__file__).with_name(name)
            for value in ("nan", "inf", "-1", "101"):
                with self.subTest(script=name, arguments=arguments, value=value):
                    result = subprocess.run([sys.executable, str(script), *arguments, value],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 2)
                    self.assertIn("coverage minimum must be finite", result.stderr)
                    self.assertNotIn("[coverage", result.stdout)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    args, remaining = parser.parse_known_args()
    CoverageTests.compiler = args.compiler
    unittest.main(argv=[__file__, *remaining])
