#!/usr/bin/env python3

"""验证性能报告解析、噪声统计、环境身份和回归判定。"""

from __future__ import annotations

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import measure_performance



class MeasurePerformanceTest(unittest.TestCase):
	"""覆盖报告工具中不依赖真实计时的严格契约。"""

	def test_parses_only_declared_metrics(self) -> None:
		"""标题和运行参数不能被误当成受控性能指标。"""

		values = measure_performance._parse_metrics(
			"benchmark title\ncount=10 extra=20\nrate: 12.5\nlatency=3\n",
			{"rate", "latency"},
		)
		self.assertEqual(values, {"rate": 12.5, "latency": 3.0})
		with self.assertRaises(SystemExit):
			measure_performance._parse_metrics("rate: 1\nrate: 2\n", {"rate"})
		with self.assertRaises(SystemExit):
			measure_performance._parse_metrics("other: 1\n", {"rate"})

	def test_computes_median_and_relative_mad(self) -> None:
		"""偶发极值不能改变中位数或把噪声统计变成平均偏差。"""

		summary = measure_performance._metric_summary([100.0, 110.0, 1000.0])
		self.assertEqual(summary["median"], 110.0)
		self.assertEqual(summary["mad"], 10.0)
		self.assertAlmostEqual(summary["relative_mad"], 10.0 / 110.0)
		self.assertAlmostEqual(summary["relative_range"], 900.0 / 110.0)
		self.assertAlmostEqual(
			summary["relative_central_range"],
			900.0 / 110.0,
		)
		robust = measure_performance._metric_summary(
			[100.0, 101.0, 102.0, 103.0, 1000.0]
		)
		self.assertAlmostEqual(robust["relative_range"], 900.0 / 102.0)
		self.assertAlmostEqual(robust["relative_central_range"], 2.0 / 102.0)

	def test_parses_only_declared_facts(self) -> None:
		"""离散运行条件必须完整、唯一，并保留可读字符串。"""

		values = measure_performance._parse_facts(
			"title\nnetwork_backend=IOCP\nignored=value\n",
			{"network_backend"},
		)
		self.assertEqual(values, {"network_backend": "IOCP"})
		with self.assertRaises(SystemExit):
			measure_performance._parse_facts("backend=a\nbackend=b\n", {"backend"})
		with self.assertRaises(SystemExit):
			measure_performance._parse_facts("other=a\n", {"backend"})

	def test_detects_both_regression_directions(self) -> None:
		"""吞吐下降与延迟上升必须使用各自方向判断。"""

		baseline = self._report()
		current = copy.deepcopy(baseline)
		metrics = current["profiles"]["core"]["benchmarks"]["bench"]["metrics"]
		metrics["throughput"]["median"] = 89.0
		metrics["latency"]["median"] = 121.0
		failures = measure_performance._check_report(current, baseline)
		self.assertEqual(len(failures), 2)
		self.assertIn("throughput", failures[0])
		self.assertIn("latency", failures[1])

	def test_rejects_environment_and_contract_mismatch(self) -> None:
		"""机器口径、runner 指纹和阈值变化都不得静默比较。"""

		baseline = self._report()
		for key, value in (
			("cpu", "other cpu"),
			("affinity", "0"),
			("compiler_version", "gcc 17"),
			("repeats", 7),
		):
			with self.subTest(key=key):
				current = copy.deepcopy(baseline)
				current[key] = value
				with self.assertRaises(SystemExit):
					measure_performance._check_report(current, baseline)
		current = copy.deepcopy(baseline)
		current["benchmark_fingerprint"] = "other"
		with self.assertRaises(SystemExit):
			measure_performance._check_report(current, baseline)
		current = copy.deepcopy(baseline)
		current["profiles"]["core"]["benchmarks"]["bench"]["metrics"][
			"throughput"
		]["limit"] = 0.20
		with self.assertRaises(SystemExit):
			measure_performance._check_report(current, baseline)
		current = copy.deepcopy(baseline)
		current["profiles"]["core"]["benchmarks"]["bench"]["facts"][
			"backend"
		] = "select"
		with self.assertRaises(SystemExit):
			measure_performance._check_report(current, baseline)

	def test_selects_profiles_without_duplicates(self) -> None:
		"""星号选择全部，显式列表保持顺序并拒绝重复项。"""

		config = {"profiles": [{"name": "a"}, {"name": "b"}]}
		self.assertEqual(
			[p["name"] for p in measure_performance._selected_profiles(config, "*")],
			["a", "b"],
		)
		self.assertEqual(
			[p["name"] for p in measure_performance._selected_profiles(config, "b,a")],
			["b", "a"],
		)
		with self.assertRaises(SystemExit):
			measure_performance._selected_profiles(config, "a,a")

	def test_extension_product_name_comes_from_manifest(self) -> None:
		"""扩展性能产物必须使用独立产品名，不能覆盖核心报告。"""

		manifest = (
			measure_performance.ROOT / "extlibs" / "xruntime" /
			"config" / "modules.json"
		)

		self.assertEqual("xrt", measure_performance._product_name(()))
		self.assertEqual(
			"xruntime",
			measure_performance._product_name((manifest,)),
		)

	def test_checked_in_baseline_summary_matches_json(self) -> None:
		"""评审摘要必须与机器基线的身份和全部受控中位数同步。"""

		baseline_path = (
			measure_performance.ROOT / "dev" / "bench" / "performance" /
			"PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json"
		)
		summary_path = baseline_path.with_suffix(".md")
		baseline = measure_performance.json.loads(
			baseline_path.read_text(encoding="utf-8")
		)
		summary = summary_path.read_text(encoding="utf-8")
		self.assertIn(f"| 报告 schema | {baseline['schema']} |", summary)
		for field in (
			"affinity",
			"source_fingerprint",
			"benchmark_fingerprint",
		):
			self.assertIn(str(baseline[field]), summary)
		backend = baseline["profiles"]["network"]["benchmarks"][
			"network_loopback"
		]["facts"]["network_backend"]
		self.assertIn(f"network_backend={backend}", summary)
		for profile in baseline["profiles"].values():
			for benchmark in profile["benchmarks"].values():
				for name, metric in benchmark["metrics"].items():
					self.assertIn(f"`{name}`", summary)
					decimals = 1 if metric["unit"] == "us" else 0
					value = f"{metric['median']:.{decimals}f} {metric['unit']}"
					self.assertIn(value, summary)

	@staticmethod
	def _report() -> dict:
		"""构造同时含吞吐和延迟的最小合法报告。"""

		report = {
			"schema": measure_performance.REPORT_SCHEMA,
			"benchmark_fingerprint": "bench-v1",
			"profiles": {
				"core": {
					"benchmarks": {
						"bench": {
							"facts": {"backend": "iocp"},
							"metrics": {
								"throughput": {
									"median": 100.0,
									"direction": "higher",
									"unit": "ops/s",
									"limit": 0.10,
									"max_relative_mad": 0.20,
									"max_relative_central_range": 0.30,
								},
								"latency": {
									"median": 100.0,
									"direction": "lower",
									"unit": "ns",
									"limit": 0.20,
									"max_relative_mad": 0.20,
									"max_relative_central_range": 0.30,
								},
							}
						}
					}
				}
			},
		}
		for key in measure_performance.BASELINE_ENVIRONMENT_KEYS:
			report[key] = {
				"cpu_count": 8,
				"repeats": 5,
				"warmups": 1,
				"max_relative_mad": 0.20,
				"max_relative_central_range": 0.30,
				"cflags": ["-O2"],
				"ldflags": [],
			}.get(key, "same")
		return report



if __name__ == "__main__":
	unittest.main()
