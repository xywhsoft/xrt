#!/usr/bin/env python3

"""生成 XRT 代表性裁剪组合的可比较发布体积报告。"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

import build as xrt_build
import package as xrt_package
import release_identity
from xrt_manifest import expand_manifest_paths, load_manifest
from xrt_text import write_utf8


ROOT = Path(__file__).resolve().parents[1]
PROFILE_PATH = ROOT / "config" / "size_profiles.json"
REPORT_SCHEMA = 2
BASELINE_ENVIRONMENT_KEYS = (
	"platform",
	"machine",
	"compiler_family",
	"compiler_version",
	"size_tool_version",
	"arch",
	"optimization",
	"stripped",
)



def _load_profiles(path: Path = PROFILE_PATH) -> dict:
	"""读取并验证体积组合配置。"""

	data = json.loads(path.read_text(encoding="utf-8"))
	if data.get("schema") != 1:
		raise SystemExit(f"unsupported size profile schema: {data.get('schema')}")
	profiles = data.get("profiles")
	if not isinstance(profiles, list) or not profiles:
		raise SystemExit("size profiles must be a non-empty list")
	names: set[str] = set()
	for profile_value in profiles:
		if not isinstance(profile_value, dict):
			raise SystemExit("size profile must be an object")
		name = profile_value.get("name")
		suite = profile_value.get("suite")
		if not isinstance(name, str) or not name:
			raise SystemExit("size profile name is missing")
		if name in names:
			raise SystemExit(f"duplicate size profile: {name}")
		if not isinstance(suite, str) or not suite:
			raise SystemExit(f"size profile suite is missing: {name}")
		for prefix in profile_value.get("forbid_symbols", []):
			if not isinstance(prefix, str) or not prefix:
				raise SystemExit(f"invalid forbidden symbol prefix: {name}")
		names.add(name)
	limits = data.get("default_growth")
	if not isinstance(limits, dict):
		raise SystemExit("size default growth limits are missing")
	for metric in ("text", "data", "bss", "file"):
		value = limits.get(metric)
		if not isinstance(value, (int, float)) or not (0 <= value <= 1):
			raise SystemExit(f"invalid size growth limit: {metric}")
	return data



def _selected_profiles(config: dict, selection: str) -> list[dict]:
	"""按命令行名称返回稳定顺序的体积组合。"""

	profiles = config["profiles"]
	if selection == "*":
		return profiles
	wanted = [value.strip() for value in selection.split(",") if value.strip()]
	if not wanted:
		raise SystemExit("size profile selection is empty")
	known = {profile_value["name"]: profile_value for profile_value in profiles}
	missing = [name for name in wanted if name not in known]
	if missing:
		raise SystemExit(f"unknown size profile: {','.join(missing)}")
	return [known[name] for name in wanted]



def _find_size_tool(compiler: str, explicit: str | None) -> str:
	"""优先使用显式工具，再查找编译器旁的 GNU 或 LLVM size。"""

	if explicit is not None:
		path = shutil.which(explicit)
		if path is None:
			raise SystemExit(f"size tool not found: {explicit}")
		return path
	directory = Path(compiler).resolve().parent
	suffix = ".exe" if sys.platform == "win32" else ""
	for candidate in [
		directory / f"llvm-size{suffix}",
		directory / f"size{suffix}",
		"llvm-size",
		"size",
	]:
		text = str(candidate)
		if Path(text).is_file():
			return text
		path = shutil.which(text)
		if path is not None:
			return path
	raise SystemExit("GNU size or llvm-size is required")



def _find_symbol_tool(compiler: str, size_tool: str) -> str:
	"""查找能够读取 ELF、Mach-O 或 COFF 对象的 nm 工具。"""

	suffix = ".exe" if sys.platform == "win32" else ""
	directories = [
		Path(compiler).resolve().parent,
		Path(size_tool).resolve().parent,
	]
	for directory in directories:
		for name in (f"llvm-nm{suffix}", f"nm{suffix}"):
			candidate = directory / name
			if candidate.is_file():
				return str(candidate)
	for name in ("llvm-nm", "nm"):
		path = shutil.which(name)
		if path is not None:
			return path
	raise SystemExit("GNU nm or llvm-nm is required for trim symbol gates")



def _tool_version(command: str) -> str:
	"""读取工具版本首行，失败时仍保留可复现路径。"""

	return release_identity.tool_version(command, ROOT)



def _source_fingerprint(
	config_path: Path = PROFILE_PATH,
	overlays: list[Path] | None = None,
) -> str:
	"""散列会影响模块化、发布库或单头实现的全部项目输入。"""

	paths = [
		ROOT / "LICENSE",
		ROOT / "config" / "modules.json",
		config_path,
		ROOT / "single" / "xrt.h",
		ROOT / "tools" / "amalgamate.py",
		ROOT / "tools" / "build.py",
		ROOT / "tools" / "package.py",
		ROOT / "tools" / "measure_size.py",
		ROOT / "tools" / "release_identity.py",
	]
	for directory in (ROOT / "include", ROOT / "src"):
		paths.extend(path for path in directory.rglob("*") if path.is_file())
	for manifest_path in overlays or []:
		manifest = load_manifest(manifest_path)
		paths.append(manifest_path)
		for key in ("include_dirs", "header_roots"):
			for directory in manifest.get(key, []):
				paths.extend(
					path for path in (ROOT / directory).rglob("*")
					if path.is_file()
				)
		for module in manifest["modules"]:
			paths.extend(ROOT / source for source in module.get("sources", []))
	return release_identity.fingerprint_paths(ROOT, paths)



def _git_state() -> tuple[str | None, bool | None]:
	"""记录便于定位的 Git 修订；内容指纹仍是源码身份的权威值。"""

	return release_identity.git_state(ROOT)



def _parse_size_output(text: str) -> dict:
	"""汇总 Berkeley 格式 size 输出中的全部对象或归档成员。"""

	totals = {"text": 0, "data": 0, "bss": 0, "members": 0}
	for line in text.splitlines():
		parts = line.split(None, 5)
		if len(parts) < 4:
			continue
		try:
			text_size = int(parts[0], 10)
			data_size = int(parts[1], 10)
			bss_size = int(parts[2], 10)
		except ValueError:
			continue
		totals["text"] += text_size
		totals["data"] += data_size
		totals["bss"] += bss_size
		totals["members"] += 1
	if totals["members"] == 0:
		raise SystemExit("size tool returned no Berkeley-format rows")
	totals["load"] = totals["text"] + totals["data"]
	return totals



def _measure_artifact(size_tool: str, artifact: Path) -> dict:
	"""记录一个对象、库或动态库的文件大小和节区总量。"""

	result = subprocess.run(
		[size_tool, "-B", str(artifact)],
		cwd=ROOT,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		text=True,
		encoding="utf-8",
		errors="replace",
		check=False,
	)
	if result.returncode != 0:
		raise SystemExit(result.stdout.strip() or f"size failed: {artifact}")
	values = _parse_size_output(result.stdout)
	values["file"] = artifact.stat().st_size
	values["path"] = artifact.relative_to(ROOT).as_posix()
	return values



def _module_macros(
	suite: str,
	overlays: list[Path] | None = None,
) -> list[str]:
	"""把 profile 根模块转换成单头文件的公开选择宏。"""

	manifests = [load_manifest(ROOT / "config" / "modules.json")]
	manifests.extend(load_manifest(path) for path in overlays or [])
	if suite == "all":
		return [
			manifest.get("module_prefix", "XRT_MODULE_") + "ALL"
			for manifest in manifests
		]
	modules = {
		module["name"]: (
			module,
			manifest.get("module_prefix", "XRT_MODULE_"),
			manifest.get("product"),
		)
		for manifest in manifests
		for module in manifest["modules"]
	}
	macros: list[str] = []
	for name in [value.strip() for value in suite.split(",") if value.strip()]:
		if name == "core":
			continue
		entry = modules.get(name)
		if entry is None:
			raise SystemExit(f"unknown size suite module: {name}")
		module, prefix, product = entry
		feature = module.get("feature")
		if not isinstance(feature, str):
			if name == product:
				macros.append(prefix + "ALL")
				continue
			raise SystemExit(f"size suite root is not publicly selectable: {name}")
		macros.append(prefix + name.upper())
	return macros



def _write_single_source(
	path: Path,
	suite: str,
	single_header: Path | None = None,
	implementation_macro: str = "XRT_IMPLEMENTATION",
	overlays: list[Path] | None = None,
) -> None:
	"""生成单头实现对象使用的最小翻译单元。"""

	single_header = single_header or (ROOT / "single" / "xrt.h")
	lines = [*(f"#define {macro}" for macro in _module_macros(suite, overlays)),
		f"#define {implementation_macro}",
		f'#include "{single_header.name}"', ""]
	content = "\n".join(lines)
	path.parent.mkdir(parents=True, exist_ok=True)
	if path.is_file() and path.read_text(encoding="utf-8") == content:
		return
	write_utf8(path, content)



def _compile_single(
	compiler: str,
	family: str,
	arch: str,
	suite: str,
	output: Path,
	rebuild: bool,
	single_header: Path | None = None,
	implementation_macro: str = "XRT_IMPLEMENTATION",
	overlays: list[Path] | None = None,
) -> float | None:
	"""按发布优化参数编译单头实现对象。"""

	source = output.with_suffix(".c")
	single_header = single_header or (ROOT / "single" / "xrt.h")
	_write_single_source(
		source,
		suite,
		single_header,
		implementation_macro,
		overlays,
	)
	latest_input = max(
		source.stat().st_mtime_ns,
		single_header.stat().st_mtime_ns,
	)
	if output.is_file() and not rebuild and output.stat().st_mtime_ns >= latest_input:
		return None
	if family == "msvc":
		command = [
			compiler,
			"/nologo",
			"/TC",
			"/std:c11",
			"/utf-8",
			"/W4",
			"/WX",
			"/O2",
			"/Zc:preprocessor",
			"/c",
			str(source),
			f"/I{single_header.parent}",
			f"/Fo{output}",
		]
	else:
		command = [compiler]
		if arch == "x86":
			command.append("-m32")
		elif arch == "x64":
			command.append("-m64")
		command.extend([
			"-std=c11",
			"-O2",
			"-I",
			str(single_header.parent),
			"-c",
			str(source),
			"-o",
			str(output),
		])
		if family == "gnu":
			command[1:1] = ["-Wall", "-Wextra", "-Werror"]
	output.parent.mkdir(parents=True, exist_ok=True)
	started = time.perf_counter()
	xrt_build._run_compiler(command, output.with_suffix(output.suffix + ".rsp"))
	return time.perf_counter() - started



def _defined_symbols(tool: str, artifact: Path) -> list[str]:
	"""读取一个对象中公开和本地的全部已定义符号。"""

	result = subprocess.run(
		[tool, "--defined-only", str(artifact)],
		cwd=ROOT,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		text=True,
		encoding="utf-8",
		errors="replace",
		check=False,
	)
	if result.returncode != 0:
		raise SystemExit(result.stdout.strip() or f"nm failed: {artifact}")
	symbols: list[str] = []
	for line in result.stdout.splitlines():
		parts = line.split()
		if len(parts) >= 2:
			symbols.append(parts[-1])
	return symbols



def _forbidden_symbols(
	symbols: list[str],
	prefixes: list[str],
) -> list[str]:
	"""返回违反当前裁剪 Profile 契约的已定义符号。"""

	return sorted({
		symbol
		for symbol in symbols
		for prefix in prefixes
		if symbol.startswith(prefix)
	})



def _package_artifact(
	compiler: str,
	family: str,
	arch: str,
	suite: str,
	kind: str,
	rebuild: bool,
	overlays: list[Path] | None = None,
	product: str = "xrt",
) -> Path:
	"""调用统一发布工具并返回主产物路径。"""

	command = [
		sys.executable,
		str(ROOT / "tools" / "package.py"),
		"--compiler",
		compiler,
		"--arch",
		arch,
		"--suite",
		suite,
		"--kind",
		kind,
	]
	for manifest in overlays or []:
		command.extend(["--manifest", str(manifest.relative_to(ROOT))])
	if rebuild:
		command.append("--rebuild")
	subprocess.run(command, cwd=ROOT, check=True)
	platform_name = xrt_package._platform()
	main_name, _ = xrt_package._artifact_names(
		kind,
		platform_name,
		family,
		product,
	)
	return (
		ROOT / "release" / family / arch /
		xrt_build._suite_output_name(suite) / kind / main_name
	)



def _growth(current: int, baseline: int) -> float:
	"""计算增长比例；零基线只允许继续保持零。"""

	if baseline == 0:
		return 0.0 if current == 0 else float("inf")
	return (current - baseline) / baseline



def _check_environment(current: dict, baseline: dict) -> None:
	"""拒绝比较任何会改变目标代码或测量口径的环境。"""

	if current.get("schema") != REPORT_SCHEMA:
		raise SystemExit("current size report schema mismatch")
	if baseline.get("schema") != REPORT_SCHEMA:
		raise SystemExit("size baseline schema mismatch")
	for key in BASELINE_ENVIRONMENT_KEYS:
		if current.get(key) != baseline.get(key):
			raise SystemExit(f"size baseline environment mismatch: {key}")
	if current.get("growth_limits") != baseline.get("growth_limits"):
		raise SystemExit("size baseline growth limits mismatch")



def _check_report(current: dict, baseline: dict, limits: dict) -> list[str]:
	"""比较同环境报告并返回所有超限说明。"""

	_check_environment(current, baseline)
	failures: list[str] = []
	for profile_name, profile_value in current["profiles"].items():
		base_profile = baseline.get("profiles", {}).get(profile_name)
		if base_profile is None:
			failures.append(f"{profile_name}: baseline is missing")
			continue
		if profile_value.get("suite") != base_profile.get("suite"):
			raise SystemExit(
				f"size baseline profile mismatch: {profile_name}/suite"
			)
		for kind, values in profile_value["kinds"].items():
			base_values = base_profile.get("kinds", {}).get(kind)
			if base_values is None:
				failures.append(f"{profile_name}/{kind}: baseline is missing")
				continue
			for metric in ("text", "data", "bss", "file"):
				growth = _growth(values[metric], base_values[metric])
				if growth > float(limits[metric]):
					failures.append(
						f"{profile_name}/{kind}/{metric}: "
						f"{values[metric]} > {base_values[metric]} "
						f"({growth:.1%} growth)"
					)
	return failures



def _display_path(path: Path) -> str:
	"""仓库内路径保持简短，仓库外报告使用绝对路径。"""

	try:
		return str(path.relative_to(ROOT))
	except ValueError:
		return str(path)



def main() -> int:
	"""生成报告，并可对同工具链基线执行增长门禁。"""

	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="gcc")
	parser.add_argument("--arch", choices=["native", "x86", "x64"], default="native")
	parser.add_argument(
		"--config",
		type=Path,
		default=PROFILE_PATH,
		help="体积 profile 配置",
	)
	parser.add_argument(
		"--manifest",
		action="append",
		default=[],
		help="叠加一个仓库相对路径的扩展模块清单",
	)
	parser.add_argument(
		"--profiles",
		default="*",
		help="逗号分隔的 profile；* 表示全部",
	)
	parser.add_argument(
		"--kind",
		action="append",
		choices=["single", "static", "shared"],
		help="可重复；默认同时测量三种产物",
	)
	parser.add_argument("--size-tool")
	parser.add_argument("--report", type=Path)
	parser.add_argument("--baseline", type=Path)
	parser.add_argument("--check", action="store_true")
	parser.add_argument("--rebuild", action="store_true")
	arguments = parser.parse_args()
	if arguments.check and arguments.baseline is None:
		parser.error("--check requires --baseline")
	if (not arguments.check) and arguments.baseline is not None:
		parser.error("--baseline requires --check")

	config_path = arguments.config
	if not config_path.is_absolute():
		config_path = ROOT / config_path
	config = _load_profiles(config_path)
	manifest_values = list(arguments.manifest)
	configured_manifest = config.get("manifest")
	if configured_manifest is not None:
		if not isinstance(configured_manifest, str) or not configured_manifest:
			parser.error("size profile manifest 必须是非空路径")
		if configured_manifest not in manifest_values:
			manifest_values.append(configured_manifest)
	try:
		overlays = expand_manifest_paths([
			Path(path) for path in manifest_values
		])
	except (OSError, ValueError) as error:
		parser.error(str(error))
	product_manifest = load_manifest(overlays[-1]) if overlays else {}
	product = product_manifest.get("product", "xrt")
	single_header = ROOT / product_manifest.get(
		"single_header",
		"single/xrt.h",
	)
	implementation_macro = product_manifest.get(
		"implementation_macro",
		"XRT_IMPLEMENTATION",
	)
	if not isinstance(product, str) or not product.isidentifier():
		parser.error("扩展 product 必须是 C 标识符")
	if not isinstance(implementation_macro, str) or not implementation_macro:
		parser.error("implementation_macro 必须是非空字符串")
	xrt_build.generate(overlays)
	profiles = _selected_profiles(config, arguments.profiles)
	kinds = arguments.kind or ["single", "static", "shared"]
	compiler = xrt_build._compiler(arguments.compiler)
	family = xrt_package._compiler_family(compiler)
	size_tool = _find_size_tool(compiler, arguments.size_tool)
	symbol_tool = _find_symbol_tool(compiler, size_tool)
	revision, dirty = _git_state()
	result = {
		"schema": REPORT_SCHEMA,
		"platform": sys.platform,
		"machine": platform.machine().lower(),
		"compiler_family": family,
		"compiler": str(Path(compiler).resolve()),
		"compiler_version": _tool_version(compiler),
		"size_tool": str(Path(size_tool).resolve()),
		"size_tool_version": _tool_version(size_tool),
		"symbol_tool": str(Path(symbol_tool).resolve()),
		"symbol_tool_version": _tool_version(symbol_tool),
		"arch": arguments.arch,
		"optimization": "O2",
		"stripped": False,
		"source_revision": revision,
		"source_dirty": dirty,
		"source_fingerprint": _source_fingerprint(config_path, overlays),
		"growth_limits": config["default_growth"],
		"profiles": {},
	}
	baseline = None
	if arguments.check:
		baseline_path = arguments.baseline
		if not baseline_path.is_absolute():
			baseline_path = ROOT / baseline_path
		baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
		_check_environment(result, baseline)
	for profile_value in profiles:
		name = profile_value["name"]
		suite = profile_value["suite"]
		profile_result = {
			"suite": suite,
			"description": profile_value.get("description", ""),
			"forbid_symbols": profile_value.get("forbid_symbols", []),
			"kinds": {},
		}
		for kind in kinds:
			compile_seconds = None
			if kind == "single":
				suffix = ".obj" if family == "msvc" else ".o"
				artifact = (
					ROOT / "out" / "size" / family / arguments.arch /
					name / "single" / f"{product}_single{suffix}"
				)
				compile_seconds = _compile_single(
					compiler,
					family,
					arguments.arch,
					suite,
					artifact,
					arguments.rebuild,
					single_header,
					implementation_macro,
					overlays,
				)
			else:
				artifact = _package_artifact(
					compiler,
					family,
					arguments.arch,
					suite,
					kind,
					arguments.rebuild,
					overlays,
					product,
				)
			values = _measure_artifact(size_tool, artifact)
			if compile_seconds is not None:
				values["compile_seconds"] = compile_seconds
			if kind == "single" and profile_result["forbid_symbols"]:
				violations = _forbidden_symbols(
					_defined_symbols(symbol_tool, artifact),
					profile_result["forbid_symbols"],
				)
				if violations:
					raise SystemExit(
						f"forbidden symbols in {name}/single: " +
						", ".join(violations[:20])
					)
				values["forbidden_symbol_count"] = 0
			profile_result["kinds"][kind] = values
		result["profiles"][name] = profile_result

	report_path = arguments.report
	if report_path is None:
		report_path = (
			ROOT / "out" / "size" / family / arguments.arch /
			("size-report.json" if product == "xrt" else f"{product}-size-report.json")
		)
	if not report_path.is_absolute():
		report_path = ROOT / report_path
	report_path.parent.mkdir(parents=True, exist_ok=True)
	write_utf8(
		report_path,
		json.dumps(result, ensure_ascii=False, indent=2) + "\n",
	)
	print(f"[pass] size-report={_display_path(report_path)}")
	for name, profile_value in result["profiles"].items():
		for kind, values in profile_value["kinds"].items():
			print(
				f"[size] {name}/{kind} text={values['text']} "
				f"data={values['data']} bss={values['bss']} file={values['file']}"
			)

	if arguments.check:
		failures = _check_report(result, baseline, config["default_growth"])
		if failures:
			for failure in failures:
				print(f"[size-fail] {failure}")
			return 1
		print("[pass] size baseline")
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
