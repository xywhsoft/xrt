#!/usr/bin/env python3

"""统一检查 XRT 发布成熟度，避免模块状态依赖人工判断。"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from xrt_manifest import (
	dependency_closure,
	expand_manifest_paths,
	load_manifest,
	module_map,
)


ROOT = Path(__file__).resolve().parents[1]
ASSET_FIELDS = (
	"public_headers",
	"internal_headers",
	"bridge_headers",
	"sources",
	"tests",
	"single_tests",
	"fuzz_sources",
	"examples",
	"benchmarks",
	"docs",
)
IMPLEMENTATION_FIELDS = ("public_headers", "sources")
PRODUCTION_FIELDS = (
	"public_headers",
	"internal_headers",
	"bridge_headers",
	"sources",
)
COVERAGE_FIELDS = ("tests", "single_tests", "docs")
MODULE_STATES = {"implemented", "developing"}



def _load_json(path: Path) -> dict:
	"""读取一个 UTF-8 JSON 配置。"""

	with path.open("r", encoding="utf-8") as file:
		return json.load(file)



def _registered_paths(modules: list[dict], fields: tuple[str, ...]) -> set[str]:
	"""收集指定资产字段登记的仓库相对路径。"""

	return {
		path
		for module in modules
		for field in fields
		for path in module.get(field, [])
	}



def _implementation_modules(modules: list[dict]) -> set[str]:
	"""返回直接拥有公共头、内部头或实现源文件的模块。"""

	return {
		module["name"]
		for module in modules
		if any(module.get(field) for field in IMPLEMENTATION_FIELDS)
	}



def _check_product_root(
	manifest: dict,
	product_modules: list[dict],
	modules: list[dict],
	errors: list[str],
) -> None:
	"""验证扩展产品根模块覆盖自身全部公开功能，不留下选择死角。"""

	product = manifest.get("product")
	if product is None:
		return
	by_name = module_map(modules)
	if product not in by_name:
		errors.append(f"扩展产品缺少同名根模块: {product}")
		return
	public = {
		module["name"]
		for module in product_modules
		if module.get("feature") is not None
	}
	closure = {
		module["name"]
		for module in dependency_closure(
			[product], modules, all_platforms=True
		)
	}
	missing = sorted(public - closure)
	if missing:
		errors.append(
			f"扩展产品根模块未覆盖全部公开功能: {product} -> {missing}"
		)



def _production_paths(root: Path, product_root: Path) -> set[str]:
	"""扫描一个产品必须由模块清单登记的生产代码。"""

	paths: set[str] = set()
	include_root = (
		product_root / "include" / "xrt"
		if product_root.resolve() == root.resolve()
		else product_root / "include"
	)
	for base in (include_root, product_root / "src"):
		if not base.is_dir():
			continue
		for path in base.rglob("*"):
			if path.is_file() and path.suffix.lower() in {".c", ".h"}:
				paths.add(path.relative_to(root).as_posix())
	return paths



def _check_assets(
	root: Path,
	modules: list[dict],
	product_root: Path,
	errors: list[str],
) -> None:
	"""检查资产字段类型、路径格式、文件存在性和生产代码登记。"""

	for module in modules:
		for field in ASSET_FIELDS:
			values = module.get(field, [])
			if not isinstance(values, list):
				errors.append(f"{module['name']}.{field} 必须是数组")
				continue
			for value in values:
				if not isinstance(value, str) or not value:
					errors.append(f"{module['name']}.{field} 包含无效路径")
					continue
				path = Path(value)
				if path.is_absolute() or ("\\" in value) or (".." in path.parts):
					errors.append(f"{module['name']}.{field} 路径不规范: {value}")
					continue
				if not (root / path).is_file():
					errors.append(f"{module['name']}.{field} 文件不存在: {value}")

	registered = _registered_paths(modules, PRODUCTION_FIELDS)
	production = _production_paths(root, product_root)
	for path in sorted(production - registered):
		errors.append(f"生产代码未登记: {path}")
	for path in sorted(registered - production):
		if path.startswith(product_root.relative_to(root).as_posix() + "/"):
			errors.append(f"登记的生产代码不在产品源码树中: {path}")



def _source_root(source: str, product_root: Path, root: Path) -> str | None:
	"""返回产品 src 目录下的第一层体系目录。"""

	try:
		parts = (root / source).resolve().relative_to(
			(product_root / "src").resolve()
		).parts
	except ValueError:
		return None
	return parts[0] if parts else None



def _check_profiles(
	root: Path,
	manifest: dict,
	product_modules: list[dict],
	modules: list[dict],
	product_root: Path,
	errors: list[str],
) -> tuple[int, int]:
	"""检查性能和体积配置引用的源文件与裁剪套件。"""

	by_name = module_map(modules)
	names = set(by_name)
	systems = {
		system["name"]: set(system["source_roots"])
		for system in manifest["scope"]["systems"]
	}
	retained = {
		system["name"]
		for system in manifest["scope"]["systems"]
		if system["state"] == "retained"
	}

	def check_system(profile: dict, suites: list[str], kind: str) -> str | None:
		"""验证 profile 标签确实触及所属体系的生产源码。"""

		system = profile.get("system")
		name = profile.get("name")
		if (system == "all") and ("all" in suites):
			return None
		if system not in retained:
			errors.append(f"{kind} profile {name} 的体系无效: {system}")
			return None
		if "all" in suites:
			return system
		valid_suites = [suite for suite in suites if suite in names]
		if not valid_suites:
			return system
		closure = dependency_closure(
			valid_suites,
			modules,
			all_platforms=True,
		)
		roots = {
			root_name
			for module in closure
			for source in module.get("sources", [])
			for root_name in [_source_root(source, product_root, root)]
			if root_name is not None
		}
		if not (roots & systems[system]):
			errors.append(
				f"{kind} profile {name} 未覆盖其声明体系: {system}"
			)
		return system

	performance_path = root / manifest.get(
		"performance_config", "config/performance_profiles.json"
	)
	performance = _load_json(performance_path)
	registered_benchmarks = _registered_paths(
		product_modules, ("benchmarks",)
	)
	performance_count = 0
	profile_names: set[str] = set()
	benchmark_names: set[str] = set()
	performance_systems: set[str] = set()
	for profile in performance.get("profiles", []):
		name = profile.get("name")
		if not isinstance(name, str) or not name:
			errors.append("性能配置包含无效 profile 名称")
		elif name in profile_names:
			errors.append(f"性能 profile 重名: {name}")
		else:
			profile_names.add(name)
		profile_suites: list[str] = []
		for benchmark in profile.get("benchmarks", []):
			performance_count += 1
			benchmark_name = benchmark.get("name")
			if benchmark_name in benchmark_names:
				errors.append(f"性能基准重名: {benchmark_name}")
			benchmark_names.add(benchmark_name)
			source = benchmark.get("source")
			if not isinstance(source, str) or not (root / source).is_file():
				errors.append(f"性能基准源文件不存在: {source}")
			elif source not in registered_benchmarks:
				errors.append(f"性能基准未登记到模块清单: {source}")
			for suite in str(benchmark.get("suite", "")).split(","):
				profile_suites.append(suite)
				if suite and suite not in names:
					errors.append(
					f"性能基准 {benchmark_name} 引用了未知模块: {suite}"
					)
		system = check_system(profile, profile_suites, "性能")
		if system is not None:
			performance_systems.add(system)
	for system in sorted(retained - performance_systems):
		errors.append(f"正式体系缺少性能 profile: {system}")

	size_path = root / manifest.get("size_config", "config/size_profiles.json")
	sizes = _load_json(size_path)
	size_count = 0
	size_names: set[str] = set()
	size_systems: set[str] = set()
	for profile in sizes.get("profiles", []):
		size_count += 1
		name = profile.get("name")
		if not isinstance(name, str) or not name:
			errors.append("体积配置包含无效 profile 名称")
		elif name in size_names:
			errors.append(f"体积 profile 重名: {name}")
		else:
			size_names.add(name)
		suites = str(profile.get("suite", "")).split(",")
		for suite in suites:
			if suite and suite != "all" and suite not in names:
				errors.append(f"体积 profile {name} 引用了未知模块: {suite}")
		system = check_system(profile, suites, "体积")
		if system is not None:
			size_systems.add(system)
	for system in sorted(retained - size_systems):
		errors.append(f"正式体系缺少体积 profile: {system}")

	return performance_count, size_count



def audit_repository(
	root: Path = ROOT,
	release: bool = False,
	manifest_path: Path | None = None,
) -> tuple[list[str], dict]:
	"""执行核心或扩展产品成熟度检查；严格模式拒绝开放状态。"""

	core_path = root / "config" / "modules.json"
	manifest_path = core_path if manifest_path is None else manifest_path
	if not manifest_path.is_absolute():
		manifest_path = root / manifest_path
	manifest = load_manifest(manifest_path)
	product_modules = manifest["modules"]
	if manifest_path.resolve() == core_path.resolve():
		modules = product_modules
	else:
		core = load_manifest(core_path)
		dependencies = [
			load_manifest(path)
			for path in expand_manifest_paths([manifest_path], root)
		][:-1]
		modules = list(core["modules"]) + [
			module
			for dependency in dependencies
			for module in dependency["modules"]
		] + product_modules
		module_map(modules)
	product_root = root / manifest.get("product_root", ".")
	errors: list[str] = []

	for module in product_modules:
		state = module.get("state")
		if state not in MODULE_STATES:
			errors.append(f"模块状态无效: {module['name']} -> {state}")
		if release and state != "implemented":
			errors.append(f"严格发布仍有未完成模块: {module['name']}")

	_check_assets(root, product_modules, product_root, errors)
	_check_product_root(manifest, product_modules, modules, errors)
	performance_count, size_count = _check_profiles(
		root,
		manifest,
		product_modules,
		modules,
		product_root,
		errors,
	)

	owned = _implementation_modules(product_modules)
	coverage: dict[str, set[str]] = {}
	for field in COVERAGE_FIELDS:
		coverage[field] = set()
		for module in product_modules:
			if not module.get(field):
				continue
			coverage[field].update(
				item["name"]
				for item in dependency_closure(
					[module["name"]], modules, all_platforms=True
				)
			)
		for name in sorted(owned - coverage[field]):
			errors.append(f"实现模块缺少 {field} 闭包覆盖: {name}")

	if release:
		for system in manifest["scope"]["systems"]:
			if system["state"] == "review":
				errors.append(f"严格发布仍有待定体系: {system['name']}")

	summary = {
		"product": manifest.get("product", manifest["scope"]["product"]),
		"modules": len(product_modules),
		"implementation_modules": len(owned),
		"test_covered": len(owned & coverage["tests"]),
		"single_covered": len(owned & coverage["single_tests"]),
		"docs_covered": len(owned & coverage["docs"]),
		"developing": sum(
			module.get("state") == "developing" for module in product_modules
		),
		"review_systems": sum(
			system["state"] == "review"
			for system in manifest["scope"]["systems"]
		),
		"performance_benchmarks": performance_count,
		"size_profiles": size_count,
	}
	return errors, summary



def main() -> int:
	"""输出机器可执行的成熟度结论。"""

	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument(
		"--release",
		action="store_true",
		help="拒绝 developing 模块和 review 体系",
	)
	parser.add_argument(
		"--manifest",
		type=Path,
		help="扩展产品模块清单；默认检查核心 XRT",
	)
	args = parser.parse_args()

	try:
		errors, summary = audit_repository(
			ROOT,
			release=args.release,
			manifest_path=args.manifest,
		)
	except (OSError, ValueError, json.JSONDecodeError) as error:
		print(f"release maturity check failed: {error}")
		return 1

	print(
		f"release maturity: product={summary['product']} "
		f"modules={summary['modules']} "
		f"implementation={summary['implementation_modules']} "
		f"tests={summary['test_covered']} "
		f"single={summary['single_covered']} "
		f"docs={summary['docs_covered']} "
		f"benchmarks={summary['performance_benchmarks']} "
		f"size_profiles={summary['size_profiles']} "
		f"developing={summary['developing']} "
		f"review={summary['review_systems']}"
	)
	for error in errors:
		print(f"error: {error}")
	return 1 if errors else 0



if __name__ == "__main__":
	raise SystemExit(main())
