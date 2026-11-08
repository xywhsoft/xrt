#!/usr/bin/env python3

"""验证性能报告解析、噪声统计、环境身份和回归判定。"""

from __future__ import annotations

import copy
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import measure_performance



class MeasurePerformanceTest(unittest.TestCase):
	"""覆盖报告工具中不依赖真实计时的严格契约。"""

	def test_declared_helper_changes_benchmark_identity(self) -> None:
		"""对端/数据夹具变化必须使旧性能基线不再可比。"""

		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			files = [
				"config/performance_profiles.json", "tools/measure_performance.py",
				"tools/release_identity.py", "tools/build.py", "dev/bench/bench_common.h",
				"dev/bench/network/bench_network_common.h", "dev/bench/tls/bench_tls_fixture.h",
				"tests/fixtures/tls_identity_legacy.h", "tests/fixtures/x509_legacy_cert.h",
				"extlibs/test/bench/main.c", "extlibs/test/tests/peer.h",
			]
			for name in files:
				path = root / name
				path.parent.mkdir(parents=True, exist_ok=True)
				path.write_text("original\n", encoding="utf-8")
			profile = [{"benchmarks": [{"source": files[-2], "inputs": [files[-1]]}]}]
			with patch.object(measure_performance, "ROOT", root):
				before = measure_performance._benchmark_fingerprint(profile, root / files[0])
				(root / files[-1]).write_text("changed peer behavior\n", encoding="utf-8")
				after = measure_performance._benchmark_fingerprint(profile, root / files[0])
				for missing in ("tests/missing.h", "../outside-repository.h", "extlibs/test/tests"):
					profile[0]["benchmarks"][0]["inputs"] = [missing]
					with self.subTest(input=missing), self.assertRaises(SystemExit):
						measure_performance._benchmark_fingerprint(profile, root / files[0])
			self.assertNotEqual(before, after)

	def test_rejects_invalid_input_declarations(self) -> None:
		"""显式夹具必须是唯一且非空的文件名列表。"""

		benchmark = {
			"name": "test", "source": "dev/bench/test.c", "suite": "core",
			"args": [], "smoke_args": [],
			"metrics": {"rate": {"direction": "higher", "unit": "ops/s"}},
		}
		measure_performance._benchmark_contract(benchmark, set())
		for invalid in (None, "file.h", [None], [""], ["file.h", "file.h"]):
			with self.subTest(inputs=invalid), self.assertRaises(SystemExit):
				measure_performance._benchmark_contract({**benchmark, "inputs": invalid}, set())

	def test_rejects_invalid_or_overlapping_observation_declarations(self) -> None:
		"""观测值必须独立声明，不能把受控事实或指标藏入同名字段。"""

		benchmark = {
			"name": "test", "source": "dev/bench/test.c", "suite": "core",
			"args": [], "smoke_args": [], "facts": ["queries"],
			"metrics": {"rate": {"direction": "higher", "unit": "ops/s"}},
		}
		measure_performance._benchmark_contract({**benchmark, "observations": ["steps"]}, set())
		for invalid in (None, "steps", [None], [""], ["steps", "steps"], ["queries"], ["rate"]):
			with self.subTest(observations=invalid), self.assertRaises(SystemExit):
				measure_performance._benchmark_contract({**benchmark, "observations": invalid}, set())

	def test_collects_each_real_child_observation_without_rounding(self) -> None:
		"""实际子进程计数变化与大整数必须完整保留，预热不混入正式样本。"""

		with tempfile.TemporaryDirectory() as directory:
			path = str(Path(directory) / "counter")
			code = (
				"from pathlib import Path;import sys;p=Path(sys.argv[1]);"
				"n=int(p.read_text())+1 if p.exists() else 1;p.write_text(str(n));"
				"print('rate: 100');print('queries: 10');print('steps:',n);"
				"print('large:',2**63+n)"
			)
			samples, facts, observations = measure_performance._collect_benchmark_samples(
				Path(sys.executable), ["-c", code, path], {"rate"}, {"queries"}, {"steps", "large"},
				{"warmups": 1, "repeats": 3, "timeout_seconds": 30}, "observed-child",
			)
		self.assertEqual(samples, {"rate": [100.0, 100.0, 100.0]})
		self.assertEqual(facts, {"queries": "10"})
		self.assertEqual(observations, [{"steps": str(n), "large": str(2**63+n)} for n in (2, 3, 4)])

	def test_observations_do_not_waive_missing_fields_or_changing_facts(self) -> None:
		"""缺少声明字段或稳定查询数漂移仍必须拒绝该次采样。"""

		with self.assertRaises(SystemExit):
			measure_performance._collect_benchmark_samples(
				Path(sys.executable), ["-c", "print('rate: 1');print('queries: 10')"],
				{"rate"}, {"queries"}, {"steps"},
				{"warmups": 0, "repeats": 1, "timeout_seconds": 30}, "missing-observation",
			)
		with patch.object(measure_performance, "_run_benchmark", side_effect=[
			({"rate": 1.0}, {"queries": "10", "steps": "9"}),
			({"rate": 1.0}, {"queries": "11", "steps": "4"}),
		]), self.assertRaisesRegex(SystemExit, "facts changed"):
			measure_performance._collect_benchmark_samples(
				Path("unused"), [], {"rate"}, {"queries"}, {"steps"},
				{"warmups": 0, "repeats": 2, "timeout_seconds": 30}, "changing-fact",
			)

	def test_changed_observations_do_not_change_strict_metric_comparison(self) -> None:
		"""内部工作量可减少；稳定后端和吞吐回归门槛继续严格生效。"""

		baseline = self._report()
		baseline["profiles"]["core"]["benchmarks"]["bench"]["observations"] = [{"steps": "351"}]
		current = copy.deepcopy(baseline)
		current["profiles"]["core"]["benchmarks"]["bench"]["observations"] = [{"steps": "4"}]
		self.assertEqual(measure_performance._check_report(current, baseline), [])
		current["profiles"]["core"]["benchmarks"]["bench"]["metrics"]["throughput"]["median"] = 80.0
		self.assertEqual(len(measure_performance._check_report(current, baseline)), 1)
		current["profiles"]["core"]["benchmarks"]["bench"]["facts"]["backend"] = "epoll"
		with self.assertRaises(SystemExit):
			measure_performance._check_report(current, baseline)

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

	def test_exports_zero_median_spread_without_weakening_gate(self) -> None:
		"""零中位数的真实波动必须保留样本、导出合法 JSON 并继续失败。"""

		samples = [0.0, 0.0, 0.0, 1.0, 2.0]
		summary = measure_performance._metric_summary(samples)
		report = {"profiles": {"test": {"benchmarks": {
			"zero": {"metrics": {"ready": summary}},
		}}}}
		exported = json.loads(measure_performance._report_text(report))
		metric = exported["profiles"]["test"]["benchmarks"]["zero"]["metrics"]["ready"]
		self.assertEqual(metric["samples"], samples)
		self.assertEqual(metric["median"], 0.0)
		self.assertEqual(metric["relative_mad"], 0.0)
		self.assertEqual(metric["relative_range"], "inf")
		self.assertEqual(metric["relative_central_range"], "inf")
		self.assertGreater(float(metric["relative_central_range"]), 0.06)
		self.assertEqual(summary["relative_central_range"], math.inf)
		self.assertEqual(summary["samples"], samples)

	def test_export_keeps_finite_metrics_numeric(self) -> None:
		"""全零和有限指标维持数字，不制造未知值或改变原始统计。"""

		for samples in ([0.0] * 5, [100.0, 101.0, 102.0, 103.0, 104.0]):
			summary = measure_performance._metric_summary(samples)
			report = {"profiles": {"test": {"benchmarks": {
				"finite": {"metrics": {"rate": summary}},
			}}}}
			self.assertEqual(json.loads(measure_performance._report_text(report)), report)

	def test_export_still_rejects_nonfinite_samples(self) -> None:
		"""字符串编码仅用于派生相对比值，不能放行非法原始样本。"""

		for invalid in (math.inf, -math.inf, math.nan):
			report = {"profiles": {"test": {"benchmarks": {
				"invalid": {"metrics": {"rate": {"samples": [invalid]}}},
			}}}}
			with self.subTest(sample=invalid), self.assertRaises(ValueError):
				measure_performance._report_text(report)

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
