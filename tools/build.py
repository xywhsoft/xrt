#!/usr/bin/env python3

"""按模块编译并运行 XRT 测试。"""

from __future__ import annotations

import argparse
from fnmatch import fnmatchcase
import hashlib
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from amalgamate import generate
from xrt_manifest import (
	ASSET_PLATFORMS,
	MANIFEST,
	dependency_closure,
	expand_manifest_paths,
	load_manifest,
	module_dependencies,
	platform_name,
)


ROOT = Path(__file__).resolve().parents[1]
TEST_ASSET_FIELDS = ("tests", "single_tests", "examples")



def _suite_output_name(suite: str) -> str:
	"""为长模块组合生成可读且稳定的短输出目录名。"""

	if len(suite) <= 64:
		return suite

	requested = [
		name.strip() for name in suite.split(",") if name.strip()
	]
	prefix = requested[0][:32]
	digest = hashlib.sha256(suite.encode("utf-8")).hexdigest()[:12]
	return f"{prefix}+{len(requested) - 1}-{digest}"



def _asset_modules(
	modules: list[dict],
	selected: list[dict],
	platform: str | None = None,
) -> dict[str, list[dict]]:
	"""展开聚合节点声明的同产品测试资产，同时隔离核心与其他扩展。"""

	platform = platform_name() if platform is None else platform
	result = {field: list(selected) for field in TEST_ASSET_FIELDS}
	for root in selected:
		for field in root.get("collect_dependency_assets", []):
			for module in modules:
				if module["_module_prefix"] != root["_module_prefix"]:
					continue
				if module not in result[field]:
					result[field].append(module)
	for field in TEST_ASSET_FIELDS:
		result[field] = [
			module for module in result[field]
			if not module.get("asset_platforms") or
				platform in module["asset_platforms"] or
				(
					("posix" in module["asset_platforms"]) and
					(platform != "windows")
				)
		]
	return result



def _asset_paths(
	modules: list[dict],
	owners: list[dict],
	field: str,
	platform: str | None = None,
) -> list[str]:
	"""汇总资产，并让平台专属所有权覆盖聚合节点中的重复登记。"""

	platform = platform_name() if platform is None else platform
	blocked = {
		asset
		for module in modules
		if module.get("asset_platforms") and
			platform not in module["asset_platforms"] and
			not (
				("posix" in module["asset_platforms"]) and
				(platform != "windows")
			)
		for asset in module.get(field, [])
	}
	return list(dict.fromkeys(
		asset
		for module in owners
		for asset in module.get(field, [])
		if asset not in blocked
	))



def _load_modules(
	suite: str,
	overlays: list[Path] | None = None,
	platform: str | None = None,
) -> tuple[
	list[str], list[str], list[str], list[str], list[str], list[str],
	list[dict], dict[str, dict], list[str], list[str],
]:
	"""读取核心与可选扩展清单，返回测试根模块的完整依赖闭包。"""

	platform = platform_name() if platform is None else platform
	manifest = load_manifest(MANIFEST)
	all_modules = [
		{
			**module,
			"_module_prefix": manifest.get("module_prefix", "XRT_MODULE_"),
		}
		for module in manifest["modules"]
	]
	include_dirs: list[str] = []
	header_roots: list[str] = []
	for path in overlays or []:
		overlay = load_manifest(path)
		all_modules.extend({
			**module,
			"_module_prefix": overlay["module_prefix"],
		} for module in overlay["modules"])
		include_dirs.extend(overlay.get("include_dirs", []))
		header_roots.extend(overlay.get("header_roots", []))
	by_name = {module["name"]: module for module in all_modules}
	if len(by_name) != len(all_modules):
		raise SystemExit("core and extension manifests contain duplicate modules")
	if suite == "all":
		requested = list(by_name)
	else:
		requested = list(dict.fromkeys(
			name.strip() for name in suite.split(",") if name.strip()
		))
	if not requested:
		raise SystemExit("suite cannot be empty")
	unknown = [name for name in requested if name not in by_name]
	if unknown:
		raise SystemExit(f"unknown suite: {','.join(unknown)}")

	modules = dependency_closure(requested, all_modules, platform=platform)
	selected = modules if suite == "all" else [by_name[name] for name in requested]
	asset_modules = _asset_modules(modules, selected, platform=platform)
	test_module_ids = {
		id(module)
		for field in TEST_ASSET_FIELDS
		for module in asset_modules[field]
	}
	test_modules = [
		module
		for module in modules
		if id(module) in test_module_ids
	]

	sources: list[str] = []
	tests: list[str] = []
	single_tests: list[str] = []
	examples: list[str] = []
	defines: list[str] = []
	links: list[str] = []
	for module in modules:
		sources.extend(module["sources"])
		if module.get("feature") is not None:
			defines.append(module["feature"])
		platform_links = module.get("link_libraries", {})
		links.extend(platform_links.get(platform,
			platform_links.get("posix", [])))
	tests = _asset_paths(
		modules, asset_modules["tests"], "tests", platform=platform,
	)
	single_tests = _asset_paths(
		modules,
		asset_modules["single_tests"],
		"single_tests",
		platform=platform,
	)
	examples = _asset_paths(
		modules,
		asset_modules["examples"],
		"examples",
		platform=platform,
	)
	return (
		list(dict.fromkeys(sources)),
		list(dict.fromkeys(tests)),
		list(dict.fromkeys(single_tests)),
		list(dict.fromkeys(examples)),
		list(dict.fromkeys(defines)),
		list(dict.fromkeys(links)),
		test_modules,
		by_name,
		list(dict.fromkeys(include_dirs)),
		list(dict.fromkeys(header_roots)),
	)



def _test_matches(test: str, selector: str) -> bool:
	"""按清单路径、文件名或不带扩展名的名称匹配一个测试。"""

	normalized = selector.replace("\\", "/")
	path = Path(test)
	return normalized in {
		test.replace("\\", "/"),
		path.name,
		path.stem,
	}



def _select_tests(
	tests: list[str],
	selector: str | None,
	start: str | None,
) -> list[str]:
	"""选择单个测试，或从指定测试起保留后续回归序列。"""

	if (selector is not None) and (start is not None):
		raise SystemExit("--test and --start-test cannot be used together")
	if (selector is None) and (start is None):
		return tests
	target = selector if selector is not None else start
	matches = [
		index for index, test in enumerate(tests)
		if _test_matches(test, target)
	]
	if not matches:
		raise SystemExit(f"unknown test in suite: {target}")
	if len(matches) != 1:
		raise SystemExit(f"ambiguous test in suite: {target}")
	if selector is not None:
		return [tests[matches[0]]]
	return tests[matches[0]:]



def _exclude_tests(tests: list[str], patterns: list[str]) -> list[str]:
	"""按清单路径、文件名或无扩展名名称排除显式测试集合。"""

	if not patterns:
		return tests
	normalized_patterns = [pattern.replace("\\", "/") for pattern in patterns]
	result: list[str] = []
	for test in tests:
		path = Path(test)
		candidates = (
			test.replace("\\", "/"),
			path.name,
			path.stem,
		)
		if any(
			fnmatchcase(candidate, pattern)
			for pattern in normalized_patterns
			for candidate in candidates
		):
			continue
		result.append(test)
	return result



def _single_owner_macros(
	module: dict,
	by_name: dict[str, dict],
	seen: set[str],
	platform: str | None = None,
) -> list[str]:
	"""穿过测试聚合节点，返回真实功能根模块宏。"""

	name = module["name"]
	if name in seen:
		return []
	seen.add(name)
	if module.get("feature") is not None:
		return [module.get("_module_prefix", "XRT_MODULE_") + name.upper()]
	macros: list[str] = []
	for dependency in module_dependencies(module, platform=platform):
		macros.extend(_single_owner_macros(
			by_name[dependency], by_name, seen, platform
		))
	return list(dict.fromkeys(macros))



def _single_test_defines(
	single_tests: list[str],
	test_modules: list[dict],
	by_name: dict[str, dict],
	platform: str | None = None,
) -> dict[str, list[str]]:
	"""为每个单头入口选择清单声明的真实功能根模块宏。"""

	result: dict[str, list[str]] = {}
	for test in single_tests:
		claimed = False
		owners: list[str] = []
		for module in test_modules:
			if test in module.get("single_tests", []):
				claimed = True
				owner_name = module.get(
					"single_test_owners", {}
				).get(test)
				owner = module if owner_name is None else by_name[owner_name]
				owners.extend(_single_owner_macros(
					owner, by_name, set(), platform,
				))
		if not claimed:
			raise ValueError(f"single-header test has no selected owner: {test}")
		source = (ROOT / test).read_text(encoding="utf-8")
		result[test] = [
			owner for owner in dict.fromkeys(owners)
			if f"#define {owner}" not in source
		]
	return result



def _compiler(name: str) -> str:
	"""查找请求的 C 编译器。"""

	path = shutil.which(name)
	if path is None:
		raise SystemExit(f"compiler not found: {name}")
	return path



def _compile_options(
	compiler: str,
	arch: str,
	defines: list[str],
	extra_cflags: list[str] | None = None,
	platform: str | None = None,
) -> list[str]:
	"""生成所有编译单元共用的编译参数。"""

	platform = platform_name() if platform is None else platform
	name = Path(compiler).stem.lower()
	options: list[str] = []
	if "tcc" in name:
		options.append("-Wall")
	else:
		options.extend(["-std=c11", "-Wall", "-Wextra", "-Werror"])
	if arch == "x86":
		options.append("-m32")
	elif arch == "x64":
		options.append("-m64")
	options.extend(f"-D{item}" for item in defines)
	options.extend(["-I", str(ROOT / "include")])
	if platform != "windows":
		options.append("-pthread")
	options.extend(extra_cflags or [])
	return options



def _object_fingerprint(
	compiler: str,
	arch: str,
	sources: list[str],
	defines: list[str],
	extra_cflags: list[str] | None = None,
	header_roots: list[str] | None = None,
	platform: str | None = None,
) -> str:
	"""计算编译器、选项、源文件清单与全部头文件的环境指纹。"""

	digest = hashlib.sha256()
	compiler_path = Path(compiler).resolve()
	compiler_stat = compiler_path.stat()
	platform = platform_name() if platform is None else platform
	settings = [
		sys.platform,
		platform,
		str(compiler_path),
		str(compiler_stat.st_size),
		str(compiler_stat.st_mtime_ns),
		arch,
		*_compile_options(
			compiler, arch, defines, extra_cflags, platform,
		),
	]
	for setting in settings:
		digest.update(setting.encode("utf-8"))
		digest.update(b"\0")
	for source in sources:
		digest.update(source.encode("utf-8"))
		digest.update(b"\0")
	files: list[Path] = []
	roots = [ROOT / "include", ROOT / "src"]
	roots.extend(ROOT / path for path in header_roots or [])
	for root in roots:
		files.extend(sorted(root.rglob("*.h")))
	for path in sorted(set(files)):
		digest.update(str(path.relative_to(ROOT)).encode("utf-8"))
		digest.update(b"\0")
		digest.update(path.read_bytes())
		digest.update(b"\0")
	return digest.hexdigest()



def _object_source_fingerprint(
	environment: str,
	source: Path,
) -> str:
	"""把共享编译环境与单个实现文件折叠为可续编对象指纹。"""

	digest = hashlib.sha256()
	digest.update(environment.encode("ascii"))
	digest.update(b"\0")
	digest.update(source.read_bytes())
	return digest.hexdigest()



def _response_argument(argument: str) -> str:
	"""生成 GCC、Clang 和 TCC 都能读取的响应文件参数。"""

	normalized = argument.replace("\\", "/")
	return '"' + normalized.replace('"', '\\"') + '"'



def _run_compiler(
	command: list[str],
	response_path: Path,
	*,
	check: bool = True,
	capture: bool = False,
) -> subprocess.CompletedProcess[str]:
	"""启动编译器，并用响应文件承载过长的编译参数。"""

	actual = command
	use_response = len(subprocess.list2cmdline(command)) >= 24000
	if use_response:
		response_path.parent.mkdir(parents=True, exist_ok=True)
		response_path.write_text(
			"\n".join(_response_argument(item) for item in command[1:]) + "\n",
			encoding="utf-8",
		)
		actual = [command[0], "@" + str(response_path)]
		print(
			"[build]",
			actual[0],
			"@" + str(response_path.relative_to(ROOT)),
			f"arguments={len(command) - 1}",
		)
	else:
		print("[build]", " ".join(command))

	try:
		return subprocess.run(
			actual,
			cwd=ROOT,
			check=check,
			stdout=subprocess.PIPE if capture else None,
			stderr=subprocess.STDOUT if capture else None,
			text=capture,
		)
	finally:
		if use_response:
			response_path.unlink(missing_ok=True)



def _compile_objects(
	compiler: str,
	arch: str,
	sources: list[str],
	defines: list[str],
	object_dir: Path,
	rebuild: bool,
	extra_cflags: list[str] | None = None,
	header_roots: list[str] | None = None,
	platform: str | None = None,
) -> list[Path]:
	"""按环境与单源指纹增量编译，并允许中断后从首个旧对象续建。"""

	options = _compile_options(
		compiler, arch, defines, extra_cflags, platform,
	)
	objects = [
		object_dir / (
			"__".join(Path(source).with_suffix("").parts) + ".o"
		)
		for source in sources
	]
	fingerprint_path = object_dir / ".fingerprint"
	environment = _object_fingerprint(
		compiler,
		arch,
		sources,
		defines,
		extra_cflags,
		header_roots,
		platform,
	)
	object_dir.mkdir(parents=True, exist_ok=True)
	compiled = 0
	for source, output in zip(sources, objects):
		source_path = ROOT / source
		stamp_path = output.with_suffix(output.suffix + ".fingerprint")
		fingerprint = _object_source_fingerprint(
			environment,
			source_path,
		)
		if (
			not rebuild and
			output.is_file() and
			stamp_path.is_file() and
			(stamp_path.read_text(encoding="ascii").strip() ==
			 fingerprint)
		):
			continue
		command = [compiler, *options, "-c", str(ROOT / source), "-o", str(output)]

		_run_compiler(command, output.with_suffix(output.suffix + ".rsp"))
		stamp_path.write_text(fingerprint + "\n", encoding="ascii")
		compiled += 1
	fingerprint_path.write_text(environment + "\n", encoding="ascii")
	if compiled == 0:
		print(f"[reuse] objects={len(objects)} dir={object_dir.relative_to(ROOT)}")
	elif compiled < len(objects):
		print(
			f"[reuse] objects={len(objects) - compiled} "
			f"rebuilt={compiled} dir={object_dir.relative_to(ROOT)}"
		)
	return objects



def _compile_program(
	compiler: str,
	arch: str,
	source: str,
	objects: list[Path],
	defines: list[str],
	links: list[str],
	output: Path,
	extra_cflags: list[str] | None = None,
	extra_ldflags: list[str] | None = None,
	platform: str | None = None,
) -> None:
	"""编译一个入口文件并链接已经生成的套件对象。"""

	command = [
		compiler,
		*_compile_options(
			compiler, arch, defines, extra_cflags, platform,
		),
	]
	command.append(str(ROOT / source))
	command.extend(str(item) for item in objects)
	platform = platform_name() if platform is None else platform
	if platform != "windows":
		command.append("-lm")
	command.extend(f"-l{item}" for item in links)
	command.extend(extra_ldflags or [])
	command.extend(["-o", str(output)])

	_run_compiler(command, output.with_suffix(output.suffix + ".rsp"))



def _run(path: Path, runner: list[str] | None = None) -> None:
	"""运行一个测试程序。"""

	print("[test]", path.relative_to(ROOT))
	command = [str(path)] if not runner else [*runner, str(path)]
	subprocess.run(command, cwd=ROOT, check=True)



def _module_features(
	name: str,
	by_name: dict[str, dict],
	platform: str | None = None,
) -> list[str]:
	"""返回一个模块完整依赖闭包中的裁剪宏。"""

	modules = dependency_closure(
		[name], list(by_name.values()), platform=platform,
	)
	return [
		module["feature"] for module in modules
		if module.get("feature") is not None
	]




def _check_trim_dependencies(
	compiler: str,
	arch: str,
	modules: list[dict],
	by_name: dict[str, dict],
	output_dir: Path,
	extra_cflags: list[str] | None = None,
	probe_header: str = "xrt.h",
	platform: str | None = None,
) -> None:
	"""验证每个模块的完整宏闭包可编译，缺少直接依赖时必须失败。"""

	probe = ROOT / "tests" / "trim" / "test_feature_dependencies.c"
	probe_cflags = list(extra_cflags or [])
	probe_cflags.append(
		f"-DXRT_TRIM_PROBE_HEADER=<{probe_header}>"
	)
	missing_guards: list[str] = []
	output_dir.mkdir(parents=True, exist_ok=True)
	for module in modules:
		feature = module.get("feature")
		if feature is None:
			continue
		features = _module_features(module["name"], by_name, platform)
		positive = output_dir / f"{module['name']}__positive.o"
		command = [
			compiler, *_compile_options(
				compiler,
				arch,
				[
					module.get("_module_prefix", "XRT_MODULE_") +
					module["name"].upper()
				],
				probe_cflags,
				platform,
			),
			"-c", str(probe), "-o", str(positive),
		]
		print(f"[trim-pass] module={module['name']}")
		_run_compiler(command, positive.with_suffix(".o.rsp"))

		for dependency in module_dependencies(module, platform=platform):
			dependency_feature = by_name[dependency].get("feature")
			if dependency_feature is None:
				continue
			broken = [item for item in features if item != dependency_feature]
			negative = output_dir / (
				f"{module['name']}__without__{dependency}.o"
			)
			command = [
				compiler,
				*_compile_options(
					compiler, arch, broken, probe_cflags, platform,
				),
				"-c", str(probe), "-o", str(negative),
			]
			result = _run_compiler(
				command,
				negative.with_suffix(".o.rsp"),
				check=False,
				capture=True,
			)
			if result.returncode == 0:
				missing_guards.append(
					f"{module['name']} -> {dependency}"
				)
				print(
					f"[trim-missing] module={module['name']} "
					f"dependency={dependency}"
				)
				continue
			print(
				f"[trim-reject] module={module['name']} missing={dependency}"
			)
	if missing_guards:
		raise SystemExit(
			"trim dependency guards missing:\n" +
			"\n".join(f"  {item}" for item in missing_guards)
		)



def main() -> int:
	"""执行模块化和单头文件测试。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("--compiler", default="gcc")
	parser.add_argument("--arch", choices=["native", "x86", "x64"], default="native")
	parser.add_argument(
		"--target-platform",
		choices=sorted(ASSET_PLATFORMS),
		help="覆盖宿主平台，用于选择交叉编译目标的依赖与测试资产",
	)
	parser.add_argument(
		"--runner",
		nargs="+",
		help="外部测试运行器命令；本地可执行文件路径作为最后一个参数传入",
	)
	parser.add_argument(
		"--manifest",
		action="append",
		default=[],
		help="叠加一个仓库相对路径的扩展模块清单",
	)
	parser.add_argument(
		"--suite",
		default="core",
		help="模块名、逗号分隔的组合根模块，或 all",
	)
	parser.add_argument("--no-run", action="store_true")
	parser.add_argument("--no-single", action="store_true")
	parser.add_argument(
		"--no-examples",
		action="store_true",
		help="不编译或运行示例，只执行当前套件的测试门禁",
	)
	parser.add_argument("--trim-only", action="store_true")
	parser.add_argument(
		"--jobs",
		type=int,
		default=1,
		help="并行编译单头文件测试的任务数，测试仍按清单顺序运行",
	)
	parser.add_argument(
		"--rebuild",
		action="store_true",
		help="忽略对象闭包指纹并强制重新编译",
	)
	parser.add_argument(
		"--cflag",
		action="append",
		default=[],
		help="向每个编译和链接命令追加一个编译器参数",
	)
	parser.add_argument(
		"--ldflag",
		action="append",
		default=[],
		help="只向最终链接命令追加一个参数",
	)
	parser.add_argument(
		"--test",
		help="只编译并运行套件中的一个模块化测试",
	)
	parser.add_argument(
		"--start-test",
		help="从指定模块化测试开始继续套件回归",
	)
	parser.add_argument(
		"--single-test",
		help="只编译并运行套件中的一个单头文件测试",
	)
	parser.add_argument(
		"--start-single-test",
		help="从指定单头文件测试开始继续套件回归",
	)
	parser.add_argument(
		"--exclude-test",
		action="append",
		default=[],
		help="按 glob 排除模块化测试；可重复指定",
	)
	parser.add_argument(
		"--exclude-single-test",
		action="append",
		default=[],
		help="按 glob 排除单头文件测试；可重复指定",
	)
	args = parser.parse_args()
	module_selected = (args.test is not None) or (args.start_test is not None)
	single_selected = (
		(args.single_test is not None) or
		(args.start_single_test is not None)
	)
	if module_selected and single_selected:
		parser.error("模块化测试选择器不能与单头文件测试选择器同时使用")
	if args.no_single and single_selected:
		parser.error("--no-single 不能与单头文件测试选择器同时使用")
	if args.trim_only and (module_selected or single_selected):
		parser.error("--trim-only 不能与测试选择器同时使用")
	if args.jobs < 1:
		parser.error("--jobs 必须大于零")

	compiler = _compiler(args.compiler)
	target_platform = (
		platform_name() if args.target_platform is None
		else args.target_platform
	)
	try:
		overlays = expand_manifest_paths([Path(path) for path in args.manifest])
	except (OSError, ValueError) as error:
		parser.error(str(error))
	(
		sources, tests, single_tests, examples, defines, links,
		test_modules, by_name, include_dirs, header_roots,
	) = _load_modules(args.suite, overlays, platform=target_platform)
	extra_cflags = list(args.cflag)
	for path in include_dirs:
		extra_cflags.extend(["-I", str(ROOT / path)])
	tests = _select_tests(tests, args.test, args.start_test)
	single_tests = _select_tests(
		single_tests,
		args.single_test,
		args.start_single_test,
	)
	tests = _exclude_tests(tests, args.exclude_test)
	single_tests = _exclude_tests(single_tests, args.exclude_single_test)
	if args.no_examples:
		examples = []
	single_defines = _single_test_defines(
		single_tests, test_modules, by_name, platform=target_platform,
	)
	if args.test is not None:
		single_tests = []
		examples = []
	if (args.single_test is not None) or (args.start_single_test is not None):
		tests = []
		examples = []
	compiler_name = Path(compiler).stem.lower()
	output_arch = (
		args.arch if args.target_platform is None
		else f"{args.arch}-{target_platform}"
	)
	out_dir = (
		ROOT / "out" / compiler_name / output_arch /
		_suite_output_name(args.suite)
	)
	out_dir.mkdir(parents=True, exist_ok=True)
	if args.trim_only:
		product_manifest = load_manifest(overlays[-1]) if overlays else {}
		product = product_manifest.get("product")
		product_prefix = product_manifest.get("module_prefix")
		trim_modules: list[dict] = []
		for module in test_modules:
			if module["name"] == product:
				closure = dependency_closure(
					[module["name"]], list(by_name.values())
				)
				trim_modules.extend(
					item for item in closure
					if (
						(item.get("feature") is not None) and
						(item.get("_module_prefix") == product_prefix)
					)
				)
				continue
			trim_modules.append(module)
		trim_modules = list({
			module["name"]: module for module in trim_modules
		}.values())
		probe_header = product_manifest.get(
			"umbrella_header",
			product_manifest.get("product", "xrt") + ".h",
		)
		_check_trim_dependencies(
			compiler,
			args.arch,
			trim_modules,
			by_name,
			out_dir / "trim",
			extra_cflags,
			probe_header,
			target_platform,
		)
		print(
			f"[pass] compiler={args.compiler} arch={args.arch} "
			f"platform={target_platform} "
			f"suite={args.suite} trim=dependencies"
		)
		return 0
	objects: list[Path] = []
	if tests or examples:
		objects = _compile_objects(
			compiler,
			args.arch,
			sources,
			defines,
			out_dir / "obj",
			args.rebuild,
			extra_cflags,
			header_roots,
			target_platform,
		)

	for test in tests:
		output = out_dir / (
			Path(test).stem + (".exe" if target_platform == "windows" else "")
		)
		_compile_program(
			compiler, args.arch, test, objects, defines, links, output,
			extra_cflags, args.ldflag, target_platform,
		)
		if not args.no_run:
			_run(output, args.runner)

	for example in examples:
		name = "_".join(Path(example).with_suffix("").parts[-3:])
		output = out_dir / (
			name + (".exe" if target_platform == "windows" else "")
		)
		_compile_program(
			compiler, args.arch, example, objects, defines, links, output,
			extra_cflags, args.ldflag, target_platform,
		)
		if not args.no_run:
			_run(output, args.runner)

	if not args.no_single:
		generate(overlays)
		single_outputs = [
			out_dir / (
				Path(test).stem +
				(".exe" if target_platform == "windows" else "")
			)
			for test in single_tests
		]
		if args.jobs == 1:
			for test, output in zip(single_tests, single_outputs):
				_compile_program(
					compiler, args.arch, test, [],
					single_defines[test], links, output,
					extra_cflags, args.ldflag,
					target_platform,
				)
		else:
			with ThreadPoolExecutor(max_workers=args.jobs) as executor:
				futures = [
						executor.submit(
							_compile_program,
							compiler,
							args.arch,
							test,
							[],
							single_defines[test],
							links,
							output,
							extra_cflags,
							args.ldflag,
							target_platform,
						)
					for test, output in zip(single_tests, single_outputs)
				]
				for future in futures:
					future.result()
		for output in single_outputs:
			if not args.no_run:
				_run(output, args.runner)

	print(
		f"[pass] compiler={args.compiler} arch={args.arch} "
		f"platform={target_platform} suite={args.suite}"
	)
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
