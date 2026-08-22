#!/usr/bin/env python3

"""验证体积报告解析、环境约束和增长判定。"""

from __future__ import annotations

import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import measure_size



class MeasureSizeTest(unittest.TestCase):
	"""覆盖归档汇总、零基线和多指标回归。"""

	def test_parses_archive_members(self) -> None:
		"""归档中的每个对象必须计入节区总量。"""

		values = measure_size._parse_size_output(
			"text data bss dec hex filename\n"
			"10 2 3 15 f a.o\n"
			"20 4 5 29 1d b.o\n"
		)
		self.assertEqual(values["text"], 30)
		self.assertEqual(values["data"], 6)
		self.assertEqual(values["bss"], 8)
		self.assertEqual(values["load"], 36)
		self.assertEqual(values["members"], 2)

	def test_detects_growth_and_environment_mismatch(self) -> None:
		"""同环境超限必须失败，不同环境不得比较。"""

		baseline = {
			"schema": measure_size.REPORT_SCHEMA,
			"platform": "win32",
			"machine": "amd64",
			"compiler_family": "gnu",
			"compiler_version": "gcc 16.0.1",
			"size_tool_version": "GNU size 2.45",
			"arch": "x64",
			"optimization": "O2",
			"stripped": False,
			"growth_limits": {
				"text": 0.10,
				"data": 0.20,
				"bss": 0.20,
				"file": 0.15,
			},
			"profiles": {
				"core": {
					"suite": "core",
					"kinds": {
						"single": {
							"text": 100,
							"data": 10,
							"bss": 0,
							"file": 200,
						}
					}
				}
			},
		}
		current = copy.deepcopy(baseline)
		current["profiles"]["core"]["kinds"]["single"]["text"] = 111
		limits = {"text": 0.10, "data": 0.20, "bss": 0.20, "file": 0.15}
		failures = measure_size._check_report(current, baseline, limits)
		self.assertEqual(len(failures), 1)
		self.assertIn("core/single/text", failures[0])

		for key, value in (
			("machine", "arm64"),
			("compiler_version", "gcc 17.0.0"),
			("size_tool_version", "GNU size 2.46"),
			("optimization", "O3"),
			("stripped", True),
		):
			with self.subTest(key=key):
				mismatched = copy.deepcopy(baseline)
				mismatched[key] = value
				with self.assertRaises(SystemExit):
					measure_size._check_report(
						mismatched, baseline, limits
					)

		current = copy.deepcopy(baseline)
		current["profiles"]["core"]["suite"] = "string"
		with self.assertRaises(SystemExit):
			measure_size._check_report(current, baseline, limits)

		current = copy.deepcopy(baseline)
		current["growth_limits"]["text"] = 0.20
		with self.assertRaises(SystemExit):
			measure_size._check_report(current, baseline, limits)

	def test_selects_full_profile_without_ambiguity(self) -> None:
		"""all 是完整库 profile，星号才表示全部组合。"""

		config = {
			"profiles": [
				{"name": "core", "suite": "core"},
				{"name": "all", "suite": "all"},
			]
		}
		self.assertEqual(
			[
				profile["name"]
				for profile in measure_size._selected_profiles(config, "all")
			],
			["all"],
		)
		self.assertEqual(
			len(measure_size._selected_profiles(config, "*")),
			2,
		)

	def test_forbidden_symbols_match_prefixes(self) -> None:
		"""裁剪门禁必须报告全部命中的未选实现符号。"""

		self.assertEqual(
			measure_size._forbidden_symbols(
				["xrtMalloc", "bbre_init", "tinfl_decompress", "bbre_init"],
				["bbre_", "tinfl_", "xrtNet"],
			),
			["bbre_init", "tinfl_decompress"],
		)

	def test_extension_module_macro_uses_manifest_namespace(self) -> None:
		"""扩展体积单头必须选择 XRUNTIME 宏而不是伪造 XRT 宏。"""

		manifest = (
			measure_size.ROOT / "extlibs" / "xruntime" /
			"config" / "modules.json"
		)
		self.assertEqual(
			measure_size._module_macros("runtime_type", [manifest]),
			["XRUNTIME_MODULE_RUNTIME_TYPE"],
		)

	def test_checked_in_baseline_summary_matches_json(self) -> None:
		"""体积评审摘要必须覆盖机器基线的身份与全部产物数值。"""

		baseline_path = (
			measure_size.ROOT / "dev" / "bench" / "size" /
			"SIZE_BASELINE_WINDOWS_GCC16_X64.json"
		)
		baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
		summary = baseline_path.with_suffix(".md").read_text(encoding="utf-8")
		self.assertIn(f"| 报告 schema | {baseline['schema']} |", summary)
		self.assertIn(baseline["source_fingerprint"], summary)
		kind_labels = {
			"single": "单头对象",
			"static": "静态库",
			"shared": "动态库",
		}
		for profile_name, profile in baseline["profiles"].items():
			for kind_name, values in profile["kinds"].items():
				row = (
					f"| {profile_name} | {kind_labels[kind_name]} | "
					f"{values['text']} | {values['data']} | {values['bss']} | "
					f"{values['file']} |"
				)
				self.assertIn(row, summary)



if __name__ == "__main__":
	unittest.main()
