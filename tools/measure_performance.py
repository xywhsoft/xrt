#!/usr/bin/env python3

"""编译、运行并比较 XRT 当前 API 的可复现性能基准。"""

from __future__ import annotations

import argparse
import ctypes
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys

import build as xrt_build
import release_identity
from xrt_manifest import expand_manifest_paths
from xrt_text import write_utf8


ROOT = Path(__file__).resolve().parents[1]
PROFILE_PATH = ROOT / "config" / "performance_profiles.json"
REPORT_SCHEMA = 2
NUMBER_PATTERN = re.compile(
	r"^\s*([A-Za-z][A-Za-z0-9_]*)\s*[:=]\s*"
	r"([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?)\s*$"
)
FACT_PATTERN = re.compile(
	r"^\s*([A-Za-z][A-Za-z0-9_]*)\s*[:=]\s*(\S(?:.*\S)?)\s*$"
)
BASELINE_ENVIRONMENT_KEYS = (
	"platform",
	"machine",
	"platform_release",
	"cpu",
	"cpu_count",
	"affinity",
	"power_policy",
	"compiler_family",
	"compiler_version",
	"arch",
	"optimization",
	"cflags",
	"ldflags",
	"policy",
	"repeats",
	"warmups",
	"max_relative_mad",
	"max_relative_central_range",
)



def _load_profiles(path: Path = PROFILE_PATH) -> dict:
	"""读取并严格验证性能组合、运行参数和指标契约。"""

	data = json.loads(path.read_text(encoding="utf-8"))
	if data.get("schema") != 1:
		raise SystemExit(
			f"unsupported performance profile schema: {data.get('schema')}"
		)
	defaults = data.get("defaults")
	if not isinstance(defaults, dict):
		raise SystemExit("performance defaults are missing")
	for name in ("repeats", "warmups", "timeout_seconds"):
		value = defaults.get(name)
		if not isinstance(value, int) or value < (0 if name == "warmups" else 1):
			raise SystemExit(f"invalid performance default: {name}")
	for name in ("cflags", "ldflags"):
		value = defaults.get(name)
		if not isinstance(value, list) or not all(
			isinstance(item, str) for item in value
		):
			raise SystemExit(f"invalid performance default: {name}")
	noise = defaults.get("max_relative_mad")
	if not isinstance(noise, (int, float)) or not (0 <= noise <= 1):
		raise SystemExit("invalid performance noise limit")
	noise_range = defaults.get("max_relative_central_range")
	if not isinstance(noise_range, (int, float)) or not (0 <= noise_range <= 2):
		raise SystemExit("invalid performance central range limit")
	regression = defaults.get("regression")
	if not isinstance(regression, dict):
		raise SystemExit("performance regression limits are missing")
	for direction in ("higher", "lower"):
		limit = regression.get(direction)
		if not isinstance(limit, (int, float)) or not (0 <= limit <= 1):
			raise SystemExit(f"invalid performance regression limit: {direction}")

	profiles = data.get("profiles")
	if not isinstance(profiles, list) or not profiles:
		raise SystemExit("performance profiles must be a non-empty list")
	profile_names: set[str] = set()
	benchmark_names: set[str] = set()
	for profile_value in profiles:
		name = profile_value.get("name") if isinstance(profile_value, dict) else None
		if not isinstance(name, str) or not name:
			raise SystemExit("performance profile name is missing")
		if name in profile_names:
			raise SystemExit(f"duplicate performance profile: {name}")
		profile_names.add(name)
		benchmarks = profile_value.get("benchmarks")
		if not isinstance(benchmarks, list) or not benchmarks:
			raise SystemExit(f"performance profile is empty: {name}")
		for benchmark in benchmarks:
			_benchmark_contract(benchmark, benchmark_names)
	return data



def _benchmark_contract(benchmark: object, names: set[str]) -> None:
	"""验证一个 benchmark 的源码、参数和指标声明。"""

	if not isinstance(benchmark, dict):
		raise SystemExit("performance benchmark must be an object")
	name = benchmark.get("name")
	if not isinstance(name, str) or not name:
		raise SystemExit("performance benchmark name is missing")
	if name in names:
		raise SystemExit(f"duplicate performance benchmark: {name}")
	names.add(name)
	for field in ("source", "suite"):
		if not isinstance(benchmark.get(field), str) or not benchmark[field]:
			raise SystemExit(f"performance benchmark {field} is missing: {name}")
	for field in ("args", "smoke_args"):
		value = benchmark.get(field)
		if not isinstance(value, list) or not all(
			isinstance(item, str) for item in value
		):
			raise SystemExit(f"invalid performance benchmark {field}: {name}")
	facts = benchmark.get("facts", [])
	if not isinstance(facts, list) or not all(
		isinstance(item, str) and item for item in facts
	):
		raise SystemExit(f"invalid performance benchmark facts: {name}")
	if len(facts) != len(set(facts)):
		raise SystemExit(f"duplicate performance benchmark fact: {name}")
	metrics = benchmark.get("metrics")
	if not isinstance(metrics, dict) or not metrics:
		raise SystemExit(f"performance benchmark metrics are missing: {name}")
	if set(facts) & set(metrics):
		raise SystemExit(f"performance fact conflicts with metric: {name}")
	for metric_name, metric in metrics.items():
		if not isinstance(metric_name, str) or not metric_name:
			raise SystemExit(f"invalid performance metric name: {name}")
		if not isinstance(metric, dict) or metric.get("direction") not in {
			"higher", "lower"
		}:
			raise SystemExit(f"invalid performance metric direction: {metric_name}")
		if not isinstance(metric.get("unit"), str) or not metric["unit"]:
			raise SystemExit(f"performance metric unit is missing: {metric_name}")
		if "limit" in metric:
			limit = metric["limit"]
			if not isinstance(limit, (int, float)) or not (0 <= limit <= 1):
				raise SystemExit(f"invalid performance metric limit: {metric_name}")
		for field, maximum in (
			("max_relative_mad", 1),
			("max_relative_central_range", 2),
		):
			if field in metric:
				value = metric[field]
				if not isinstance(value, (int, float)) or not (0 <= value <= maximum):
					raise SystemExit(
						f"invalid performance metric {field}: {metric_name}"
					)



def _selected_profiles(config: dict, selection: str) -> list[dict]:
	"""按名称返回稳定顺序的性能组合；星号表示全部。"""

	profiles = config["profiles"]
	if selection == "*":
		return profiles
	wanted = [item.strip() for item in selection.split(",") if item.strip()]
	if not wanted:
		raise SystemExit("performance profile selection is empty")
	known = {profile_value["name"]: profile_value for profile_value in profiles}
	missing = [name for name in wanted if name not in known]
	if missing:
		raise SystemExit(f"unknown performance profile: {','.join(missing)}")
	if len(wanted) != len(set(wanted)):
		raise SystemExit("duplicate performance profile selection")
	return [known[name] for name in wanted]



def _compiler_family(compiler: str, version: str) -> str:
	"""把当前支持的 C 编译器归一为稳定的报告名称。"""

	name = (Path(compiler).stem + " " + version).lower()
	if "tcc" in name or "tiny c" in name:
		return "tcc"
	if "clang" in name:
		return "clang"
	if "gcc" in name or "gnu" in name:
		return "gnu"
	return Path(compiler).stem.lower()



def _cpu_name() -> str:
	"""读取用于拒绝跨机器比较的处理器型号。"""

	if sys.platform == "win32":
		try:
			import winreg

			with winreg.OpenKey(
				winreg.HKEY_LOCAL_MACHINE,
				r"HARDWARE\DESCRIPTION\System\CentralProcessor\0",
			) as key:
				value, _ = winreg.QueryValueEx(key, "ProcessorNameString")
				return " ".join(str(value).split())
		except OSError:
			pass
	if sys.platform.startswith("linux"):
		try:
			for line in Path("/proc/cpuinfo").read_text(
				encoding="utf-8", errors="replace"
			).splitlines():
				if line.lower().startswith("model name"):
					return " ".join(line.split(":", 1)[1].split())
		except OSError:
			pass
	if sys.platform == "darwin":
		result = subprocess.run(
			["sysctl", "-n", "machdep.cpu.brand_string"],
			stdout=subprocess.PIPE,
			stderr=subprocess.DEVNULL,
			text=True,
			encoding="utf-8",
			errors="replace",
			check=False,
		)
		if result.returncode == 0 and result.stdout.strip():
			return " ".join(result.stdout.split())
	return " ".join((platform.processor() or "unknown").split())



def _affinity() -> str:
	"""记录当前进程的实际 CPU 亲和范围，子进程会继承该范围。"""

	if hasattr(os, "sched_getaffinity"):
		return ",".join(str(cpu) for cpu in sorted(os.sched_getaffinity(0)))
	if sys.platform == "win32":
		process_mask = ctypes.c_size_t()
		system_mask = ctypes.c_size_t()
		kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
		kernel32.GetCurrentProcess.restype = ctypes.c_void_p
		kernel32.GetProcessAffinityMask.argtypes = (
			ctypes.c_void_p,
			ctypes.POINTER(ctypes.c_size_t),
			ctypes.POINTER(ctypes.c_size_t),
		)
		kernel32.GetProcessAffinityMask.restype = ctypes.c_int
		if kernel32.GetProcessAffinityMask(
			kernel32.GetCurrentProcess(),
			ctypes.byref(process_mask),
			ctypes.byref(system_mask),
		):
			return f"0x{process_mask.value:x}/0x{system_mask.value:x}"
	return "system-default"



def _power_policy() -> str:
	"""记录可稳定读取的系统电源计划或 CPU governor。"""

	if sys.platform == "win32":
		result = subprocess.run(
			["powercfg", "/getactivescheme"],
			stdout=subprocess.PIPE,
			stderr=subprocess.DEVNULL,
			text=True,
			encoding="utf-8",
			errors="replace",
			check=False,
		)
		match = re.search(
			r"[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
			r"[0-9a-fA-F]{4}-[0-9a-fA-F]{12}",
			result.stdout,
		)
		return match.group(0).lower() if match else "unknown"
	if sys.platform.startswith("linux"):
		values: set[str] = set()
		for path in Path("/sys/devices/system/cpu").glob(
			"cpu[0-9]*/cpufreq/scaling_governor"
		):
			try:
				values.add(path.read_text(encoding="ascii").strip())
			except OSError:
				continue
		return ",".join(sorted(values)) if values else "unknown"
	return "unknown"



def _source_fingerprint(
	profile_path: Path = PROFILE_PATH,
	overlays: tuple[Path, ...] = (),
) -> str:
	"""散列会影响当前 XRT 实现的全部源码输入。"""

	paths = [
		ROOT / "LICENSE",
		ROOT / "config" / "modules.json",
		profile_path,
		ROOT / "single" / "xrt.h",
	]
	for directory in (ROOT / "include", ROOT / "src"):
		paths.extend(path for path in directory.rglob("*") if path.is_file())
	for overlay in overlays:
		paths.append(overlay)
		manifest = json.loads(overlay.read_text(encoding="utf-8"))
		for root in manifest.get("header_roots", []):
			directory = ROOT / root
			paths.extend(
				path for path in directory.rglob("*") if path.is_file()
			)
		for module in manifest.get("modules", []):
			for source in module.get("sources", []):
				paths.append(ROOT / source)
	return release_identity.fingerprint_paths(ROOT, paths)



def _benchmark_fingerprint(
	profiles: list[dict],
	profile_path: Path = PROFILE_PATH,
) -> str:
	"""散列 runner、配置、公共计时实现和本次使用的基准源码。"""

	paths = [
		profile_path,
		ROOT / "tools" / "measure_performance.py",
		ROOT / "tools" / "release_identity.py",
		ROOT / "tools" / "build.py",
		ROOT / "dev" / "bench" / "bench_common.h",
		ROOT / "dev" / "bench" / "network" / "bench_network_common.h",
		ROOT / "dev" / "bench" / "tls" / "bench_tls_fixture.h",
		ROOT / "tests" / "fixtures" / "tls_identity_legacy.h",
		ROOT / "tests" / "fixtures" / "x509_legacy_cert.h",
	]
	for profile_value in profiles:
		for benchmark in profile_value["benchmarks"]:
			paths.append(ROOT / benchmark["source"])
	return release_identity.fingerprint_paths(ROOT, paths)



def _product_name(overlays: tuple[Path, ...]) -> str:
	"""返回报告所属产品名；最后一个扩展清单代表发布产品。"""

	if not overlays:
		return "xrt"
	manifest = json.loads(overlays[-1].read_text(encoding="utf-8"))
	product = manifest.get("product")
	if not isinstance(product, str) or not product:
		product = manifest.get("scope", {}).get("product")
	if not isinstance(product, str) or not product:
		raise SystemExit("extension manifest product is missing")
	return product



def _parse_metrics(text: str, wanted: set[str]) -> dict[str, float]:
	"""从 benchmark 的单值 key-value 行读取声明的有限数值指标。"""

	values: dict[str, float] = {}
	for line in text.splitlines():
		match = NUMBER_PATTERN.fullmatch(line)
		if match is None or match.group(1) not in wanted:
			continue
		name = match.group(1)
		if name in values:
			raise SystemExit(f"duplicate performance metric: {name}")
		value = float(match.group(2))
		if not math.isfinite(value):
			raise SystemExit(f"non-finite performance metric: {name}")
		values[name] = value
	missing = sorted(wanted - values.keys())
	if missing:
		raise SystemExit(f"performance metrics are missing: {','.join(missing)}")
	return values



def _parse_facts(text: str, wanted: set[str]) -> dict[str, str]:
	"""读取声明的非空字符串事实，供后端等离散运行条件做精确比较。"""

	values: dict[str, str] = {}
	for line in text.splitlines():
		match = FACT_PATTERN.fullmatch(line)
		if match is None or match.group(1) not in wanted:
			continue
		name = match.group(1)
		if name in values:
			raise SystemExit(f"duplicate performance fact: {name}")
		values[name] = match.group(2)
	missing = sorted(wanted - values.keys())
	if missing:
		raise SystemExit(f"performance facts are missing: {','.join(missing)}")
	return values



def _metric_summary(samples: list[float]) -> dict[str, float | list[float]]:
	"""保留原始样本并计算稳健中位数和中位绝对偏差。"""

	if not samples:
		raise SystemExit("performance metric has no samples")
	median = float(statistics.median(samples))
	mad = float(statistics.median(abs(value - median) for value in samples))
	relative_mad = 0.0 if mad == 0 else (
		float("inf") if median == 0 else mad / abs(median)
	)
	value_range = max(samples) - min(samples)
	relative_range = 0.0 if value_range == 0 else (
		float("inf") if median == 0 else value_range / abs(median)
	)
	central = sorted(samples)[1:-1] if len(samples) >= 5 else samples
	central_range = max(central) - min(central)
	relative_central_range = 0.0 if central_range == 0 else (
		float("inf") if median == 0 else central_range / abs(median)
	)
	return {
		"samples": samples,
		"median": median,
		"mean": float(statistics.fmean(samples)),
		"min": min(samples),
		"max": max(samples),
		"mad": mad,
		"relative_mad": relative_mad,
		"range": value_range,
		"relative_range": relative_range,
		"central_range": central_range,
		"relative_central_range": relative_central_range,
	}



def _compile_benchmark(
	compiler: str,
	arch: str,
	benchmark: dict,
	defaults: dict,
	output: Path,
	overlays: tuple[Path, ...] = (),
) -> None:
	"""以与模块测试一致的警告和链接规则编译单头 benchmark。"""

	source = (ROOT / benchmark["source"]).resolve()
	try:
		relative = source.relative_to(ROOT.resolve())
	except ValueError as error:
		raise SystemExit(
			f"performance source is outside repository: {benchmark['source']}"
		) from error
	if not (
		(relative.parts[:2] == ("dev", "bench")) or
		((len(relative.parts) >= 3) and
		 (relative.parts[0] == "extlibs") and
		 ("bench" in relative.parts[2:]))
	):
		raise SystemExit(
			f"performance source is outside a benchmark root: "
			f"{benchmark['source']}"
		)
	if not source.is_file():
		raise SystemExit(f"performance source is missing: {benchmark['source']}")
	(
		sources, _, _, _, defines, links, _, _, include_dirs, header_roots,
	) = xrt_build._load_modules(benchmark["suite"], list(overlays))
	if overlays:
		extra_cflags = list(defaults["cflags"])
		for path in include_dirs:
			extra_cflags.extend(["-I", str(ROOT / path)])
		objects = xrt_build._compile_objects(
			compiler,
			arch,
			sources,
			defines,
			output.parent / (output.stem + "_obj"),
			False,
			extra_cflags,
			header_roots,
		)
		xrt_build._compile_program(
			compiler,
			arch,
			benchmark["source"],
			objects,
			defines,
			links,
			output,
			extra_cflags,
			list(defaults["ldflags"]),
		)
		return
	options = xrt_build._compile_options(
		compiler,
		arch,
		[],
		list(defaults["cflags"]),
	)
	command = [compiler, *options, str(source)]
	if sys.platform != "win32":
		command.append("-lm")
	command.extend(f"-l{item}" for item in links)
	command.extend(defaults["ldflags"])
	command.extend(["-o", str(output)])
	output.parent.mkdir(parents=True, exist_ok=True)
	xrt_build._run_compiler(
		command,
		output.with_suffix(output.suffix + ".rsp"),
	)



def _run_benchmark(
	executable: Path,
	arguments: list[str],
	metrics: set[str],
	facts: set[str],
	timeout: int,
) -> tuple[dict[str, float], dict[str, str]]:
	"""串行运行一次 benchmark，并把非零退出视为无效样本。"""

	try:
		result = subprocess.run(
			[str(executable), *arguments],
			cwd=ROOT,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			text=True,
			encoding="utf-8",
			errors="replace",
			timeout=timeout,
			check=False,
		)
	except subprocess.TimeoutExpired as error:
		raise SystemExit(f"performance benchmark timed out: {executable.name}") from error
	if result.returncode != 0:
		print(result.stdout, end="")
		raise SystemExit(
			f"performance benchmark failed: {executable.name} "
			f"exit={result.returncode}"
		)
	return (
		_parse_metrics(result.stdout, metrics),
		_parse_facts(result.stdout, facts),
	)



def _metric_limit(metric: dict, defaults: dict) -> float:
	"""读取指标专用阈值，未声明时使用方向默认值。"""

	return float(metric.get("limit", defaults["regression"][metric["direction"]]))



def _check_environment(current: dict, baseline: dict) -> None:
	"""拒绝比较任何会改变执行口径或机器调度条件的环境。"""

	if current.get("schema") != REPORT_SCHEMA:
		raise SystemExit("current performance report schema mismatch")
	if baseline.get("schema") != REPORT_SCHEMA:
		raise SystemExit("performance baseline schema mismatch")
	for key in BASELINE_ENVIRONMENT_KEYS:
		if current.get(key) != baseline.get(key):
			raise SystemExit(f"performance baseline environment mismatch: {key}")
	if current.get("benchmark_fingerprint") != baseline.get(
		"benchmark_fingerprint"
	):
		raise SystemExit("performance benchmark fingerprint mismatch")



def _check_report(current: dict, baseline: dict) -> list[str]:
	"""按每个指标的方向和固定阈值比较中位数。"""

	_check_environment(current, baseline)
	failures: list[str] = []
	for profile_name, profile_value in current["profiles"].items():
		base_profile = baseline.get("profiles", {}).get(profile_name)
		if base_profile is None:
			failures.append(f"{profile_name}: baseline is missing")
			continue
		for bench_name, benchmark in profile_value["benchmarks"].items():
			base_benchmark = base_profile.get("benchmarks", {}).get(bench_name)
			if base_benchmark is None:
				failures.append(f"{profile_name}/{bench_name}: baseline is missing")
				continue
			if benchmark.get("facts", {}) != base_benchmark.get("facts", {}):
				raise SystemExit(
					"performance benchmark facts mismatch: "
					f"{profile_name}/{bench_name}"
				)
			for metric_name, metric in benchmark["metrics"].items():
				base_metric = base_benchmark.get("metrics", {}).get(metric_name)
				if base_metric is None:
					failures.append(
						f"{profile_name}/{bench_name}/{metric_name}: baseline is missing"
					)
					continue
				for field in (
					"direction",
					"unit",
					"limit",
					"max_relative_mad",
					"max_relative_central_range",
				):
					if metric.get(field) != base_metric.get(field):
						raise SystemExit(
							"performance metric contract mismatch: "
							f"{profile_name}/{bench_name}/{metric_name}/{field}"
						)
				current_value = float(metric["median"])
				base_value = float(base_metric["median"])
				limit = float(metric["limit"])
				if metric["direction"] == "higher":
					failed = current_value < (base_value * (1.0 - limit))
				else:
					failed = current_value > (base_value * (1.0 + limit))
				if failed:
					failures.append(
						f"{profile_name}/{bench_name}/{metric_name}: "
						f"baseline={base_value:.6g} current={current_value:.6g} "
						f"limit={limit:.1%}"
					)
	return failures



def _display_path(path: Path) -> str:
	"""优先显示仓库相对路径，仓库外报告仍可安全输出。"""

	try:
		return str(path.resolve().relative_to(ROOT))
	except ValueError:
		return str(path.resolve())



def main() -> int:
	"""执行性能报告或与同环境固定基线比较。"""

	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="gcc")
	parser.add_argument("--arch", choices=["native", "x86", "x64"], default="native")
	parser.add_argument(
		"--config",
		type=Path,
		default=PROFILE_PATH,
		help="性能 profile 配置；默认使用核心配置",
	)
	parser.add_argument(
		"--manifest",
		type=Path,
		action="append",
		default=[],
		help="可重复指定扩展模块清单",
	)
	parser.add_argument("--profiles", default="*", help="逗号分隔的 profile；* 表示全部")
	parser.add_argument("--report", type=Path)
	parser.add_argument("--baseline", type=Path)
	parser.add_argument("--check", action="store_true")
	parser.add_argument("--smoke", action="store_true")
	parser.add_argument("--repeats", type=int)
	parser.add_argument("--warmups", type=int)
	parser.add_argument("--list", action="store_true")
	arguments = parser.parse_args()
	if arguments.check and arguments.baseline is None:
		parser.error("--check requires --baseline")
	if (not arguments.check) and arguments.baseline is not None:
		parser.error("--baseline requires --check")
	if arguments.smoke and arguments.check:
		parser.error("--smoke cannot compare a release baseline")

	profile_path = arguments.config
	if not profile_path.is_absolute():
		profile_path = ROOT / profile_path
	if not profile_path.is_file():
		parser.error(f"性能 profile 配置不存在: {profile_path}")
	config = _load_profiles(profile_path)
	manifest_paths = list(arguments.manifest)
	configured_manifest = config.get("manifest")
	if configured_manifest is not None:
		if not isinstance(configured_manifest, str) or not configured_manifest:
			parser.error("性能 profile manifest 无效")
		configured_path = Path(configured_manifest)
		known = {
			(path if path.is_absolute() else ROOT / path).resolve()
			for path in manifest_paths
		}
		resolved = (
			configured_path if configured_path.is_absolute()
			else ROOT / configured_path
		).resolve()
		if resolved not in known:
			manifest_paths.append(configured_path)
	try:
		overlays = tuple(expand_manifest_paths([
			Path(path) for path in manifest_paths
		]))
	except (OSError, ValueError) as error:
		parser.error(str(error))
	product = _product_name(overlays)
	profiles = _selected_profiles(config, arguments.profiles)
	if arguments.list:
		for profile_value in profiles:
			print(profile_value["name"])
			for benchmark in profile_value["benchmarks"]:
				print(f"  {benchmark['name']}: {benchmark['source']}")
		return 0
	defaults = dict(config["defaults"])
	if arguments.smoke:
		defaults["repeats"] = 1
		defaults["warmups"] = 0
	if arguments.repeats is not None:
		if arguments.repeats < 1:
			parser.error("--repeats must be positive")
		defaults["repeats"] = arguments.repeats
	if arguments.warmups is not None:
		if arguments.warmups < 0:
			parser.error("--warmups must be non-negative")
		defaults["warmups"] = arguments.warmups

	compiler = xrt_build._compiler(arguments.compiler)
	compiler_version = release_identity.tool_version(compiler, ROOT)
	compiler_family = _compiler_family(compiler, compiler_version)
	optimization = ",".join(
		flag for flag in defaults["cflags"] if flag.startswith("-O")
	) or "default"
	revision, dirty = release_identity.git_state(ROOT)
	result = {
		"schema": REPORT_SCHEMA,
		"product": product,
		"generated_at": datetime.now(timezone.utc).isoformat(),
		"platform": sys.platform,
		"machine": platform.machine().lower(),
		"platform_release": platform.release(),
		"cpu": _cpu_name(),
		"cpu_count": os.cpu_count(),
		"affinity": _affinity(),
		"power_policy": _power_policy(),
		"compiler_family": compiler_family,
		"compiler": str(Path(compiler).resolve()),
		"compiler_version": compiler_version,
		"arch": arguments.arch,
		"optimization": optimization,
		"cflags": defaults["cflags"],
		"ldflags": defaults["ldflags"],
		"policy": defaults["policy"],
		"repeats": defaults["repeats"],
		"warmups": defaults["warmups"],
		"max_relative_mad": defaults["max_relative_mad"],
		"max_relative_central_range": defaults[
			"max_relative_central_range"
		],
		"source_revision": revision,
		"source_dirty": dirty,
		"source_fingerprint": _source_fingerprint(profile_path, overlays),
		"benchmark_fingerprint": _benchmark_fingerprint(
			profiles, profile_path
		),
		"profiles": {},
	}
	baseline = None
	if arguments.check:
		baseline_path = arguments.baseline
		if not baseline_path.is_absolute():
			baseline_path = ROOT / baseline_path
		baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
		_check_environment(result, baseline)

	quality_failures: list[str] = []
	extension = ".exe" if sys.platform == "win32" else ""
	for profile_value in profiles:
		profile_result = {
			"description": profile_value.get("description", ""),
			"benchmarks": {},
		}
		for benchmark in profile_value["benchmarks"]:
			product_output = (
				ROOT / "out" / "performance" / compiler_family /
				arguments.arch
			)
			if product != "xrt":
				product_output /= product
			output = (
				product_output / (benchmark["name"] + extension)
			)
			_compile_benchmark(
				compiler, arguments.arch, benchmark, defaults, output,
				overlays,
			)
			metric_names = set(benchmark["metrics"])
			fact_names = set(benchmark.get("facts", []))
			run_args = benchmark["smoke_args"] if arguments.smoke else benchmark["args"]
			for warmup in range(defaults["warmups"]):
				print(f"[warmup] {benchmark['name']} {warmup + 1}/{defaults['warmups']}")
				_run_benchmark(
					output, run_args, metric_names, fact_names,
					defaults["timeout_seconds"]
				)
			samples = {name: [] for name in metric_names}
			facts_result = None
			for sample in range(defaults["repeats"]):
				print(f"[run] {benchmark['name']} {sample + 1}/{defaults['repeats']}")
				values, facts = _run_benchmark(
					output, run_args, metric_names, fact_names,
					defaults["timeout_seconds"]
				)
				if facts_result is None:
					facts_result = facts
				elif facts != facts_result:
					raise SystemExit(
						f"performance facts changed between samples: {benchmark['name']}"
					)
				for name, value in values.items():
					samples[name].append(value)
			metrics_result = {}
			for name, metric in benchmark["metrics"].items():
				summary = _metric_summary(samples[name])
				summary.update({
					"direction": metric["direction"],
					"unit": metric["unit"],
					"limit": _metric_limit(metric, defaults),
					"max_relative_mad": float(metric.get(
						"max_relative_mad",
						defaults["max_relative_mad"],
					)),
					"max_relative_central_range": float(metric.get(
						"max_relative_central_range",
						defaults["max_relative_central_range"],
					)),
				})
				metrics_result[name] = summary
				if (
					defaults["repeats"] >= 3 and
					float(summary["relative_mad"]) > summary["max_relative_mad"]
				):
					quality_failures.append(
						f"{profile_value['name']}/{benchmark['name']}/{name}: "
						f"relative_mad={summary['relative_mad']:.1%}"
					)
				if (
					defaults["repeats"] >= 3 and
					float(summary["relative_central_range"]) >
					summary["max_relative_central_range"]
				):
					quality_failures.append(
						f"{profile_value['name']}/{benchmark['name']}/{name}: "
						"relative_central_range="
						f"{summary['relative_central_range']:.1%}"
					)
			profile_result["benchmarks"][benchmark["name"]] = {
				"source": benchmark["source"],
				"suite": benchmark["suite"],
				"args": run_args,
				"facts": facts_result or {},
				"metrics": metrics_result,
			}
		result["profiles"][profile_value["name"]] = profile_result

	report_path = arguments.report
	if report_path is None:
		report_name = (
			"performance-report.json"
			if product == "xrt"
			else f"{product}-performance-report.json"
		)
		report_path = (
			ROOT / "out" / "performance" / compiler_family /
			arguments.arch / report_name
		)
	if not report_path.is_absolute():
		report_path = ROOT / report_path
	report_path.parent.mkdir(parents=True, exist_ok=True)
	write_utf8(
		report_path,
		json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
	)
	print(f"[pass] performance-report={_display_path(report_path)}")
	for profile_name, profile_value in result["profiles"].items():
		for bench_name, benchmark in profile_value["benchmarks"].items():
			for metric_name, metric in benchmark["metrics"].items():
				print(
					f"[metric] {profile_name}/{bench_name}/{metric_name} "
					f"median={metric['median']:.6g} {metric['unit']} "
					f"mad={metric['relative_mad']:.1%} "
					f"range={metric['relative_range']:.1%} "
					f"central_range={metric['relative_central_range']:.1%}"
				)
	if quality_failures:
		for failure in quality_failures:
			print(f"[quality-fail] {failure}")
		return 1
	if arguments.check:
		failures = _check_report(result, baseline)
		if failures:
			for failure in failures:
				print(f"[performance-fail] {failure}")
			return 1
		print("[pass] performance baseline")
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
