#!/usr/bin/env python3

"""提供 XRT 模块清单的唯一读取和依赖图实现。"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "config" / "modules.json"
MODULE_NAME = re.compile(r"^[a-z][a-z0-9_]*$")
SCOPE_STATES = {"retained", "review", "internal"}
INTEGRATION_STATES = {"deferred", "integrated"}
COLLECTABLE_ASSET_FIELDS = {"tests", "single_tests", "examples"}
ASSET_PLATFORMS = {
	"windows", "linux", "android", "macos", "bsd", "posix",
}



def platform_name(value: str | None = None) -> str:
	"""把 Python 平台名转换为模块清单使用的平台名。"""

	value = sys.platform if value is None else value
	if value == "win32":
		return "windows"
	if value == "darwin":
		return "macos"
	if value == "android":
		return "android"
	if value.startswith("linux"):
		return "linux"
	if value.startswith(("freebsd", "openbsd", "netbsd", "dragonfly")):
		return "bsd"
	return "posix"



def load_manifest(path: Path = MANIFEST) -> dict:
	"""读取并验证模块清单的基础结构。"""

	with path.open("r", encoding="utf-8") as file:
		manifest = json.load(file)
	modules = manifest.get("modules")
	if not isinstance(modules, list):
		raise ValueError("module manifest must contain a modules array")
	names = [module.get("name") for module in modules]
	if any(not isinstance(name, str) or not MODULE_NAME.fullmatch(name)
		   for name in names):
		raise ValueError("module names must use lowercase C identifier syntax")
	if len(names) != len(set(names)):
		raise ValueError("module names must be unique")
	features = [module.get("feature") for module in modules
				if module.get("feature") is not None]
	if len(features) != len(set(features)):
		raise ValueError("module feature macros must be unique")
	for module in modules:
		fields = module.get("collect_dependency_assets", [])
		if not isinstance(fields, list) or any(
			not isinstance(field, str) or field not in COLLECTABLE_ASSET_FIELDS
			for field in fields
		):
			raise ValueError(
				"collect_dependency_assets only accepts tests, "
				"single_tests, and examples"
			)
		if len(fields) != len(set(fields)):
			raise ValueError("collect_dependency_assets must not contain duplicates")
		platforms = module.get("asset_platforms", [])
		if not isinstance(platforms, list) or any(
			not isinstance(platform, str) or platform not in ASSET_PLATFORMS
			for platform in platforms
		):
			raise ValueError(
				"asset_platforms only accepts windows, linux, android, macos, "
				"bsd, and posix"
			)
		if len(platforms) != len(set(platforms)):
			raise ValueError("asset_platforms must not contain duplicates")
	if "scope" in manifest:
		_validate_scope(manifest["scope"], set(names))
	dependencies = manifest.get("dependency_manifests", [])
	if not isinstance(dependencies, list) or any(
		not isinstance(value, str) or not value
		for value in dependencies
	):
		raise ValueError("dependency_manifests must contain non-empty paths")
	return manifest



def expand_manifest_paths(
	paths: list[Path],
	root: Path = ROOT,
) -> list[Path]:
	"""按依赖优先顺序递归展开扩展清单，并拒绝循环和隐式核心清单。"""

	states: dict[Path, int] = {}
	ordered: list[Path] = []
	core = (root / "config" / "modules.json").resolve()

	def resolve(path: Path) -> Path:
		"""把调用方路径和仓库相对依赖统一为绝对路径。"""

		return (path if path.is_absolute() else root / path).resolve()

	def visit(path: Path) -> None:
		"""先加入一份清单的扩展依赖，再加入清单自身。"""

		path = resolve(path)
		if path == core:
			raise ValueError("core manifest is implicit and cannot be an extension dependency")
		if states.get(path) == 2:
			return
		if states.get(path) == 1:
			raise ValueError(f"extension manifest dependency cycle at: {path}")
		if not path.is_file():
			raise ValueError(f"extension manifest does not exist: {path}")
		states[path] = 1
		manifest = load_manifest(path)
		for dependency in manifest.get("dependency_manifests", []):
			visit(Path(dependency))
		states[path] = 2
		ordered.append(path)

	for path in paths:
		visit(path)
	return ordered



def _validate_scope(scope: object, module_names: set[str]) -> None:
	"""验证产品体系、源码根目录和外部集成边界。"""

	if not isinstance(scope, dict):
		raise ValueError("manifest scope must be an object")
	if not isinstance(scope.get("product"), str) or not scope["product"]:
		raise ValueError("manifest scope must name its product")

	systems = scope.get("systems")
	if not isinstance(systems, list) or not systems:
		raise ValueError("manifest scope must contain systems")
	system_names: set[str] = set()
	source_roots: set[str] = set()
	review_modules: set[str] = set()
	for system in systems:
		if not isinstance(system, dict):
			raise ValueError("scope systems must be objects")
		name = system.get("name")
		if not isinstance(name, str) or not MODULE_NAME.fullmatch(name):
			raise ValueError("scope system names must use identifier syntax")
		if name in system_names:
			raise ValueError(f"duplicate scope system: {name}")
		system_names.add(name)
		state = system.get("state")
		if state not in SCOPE_STATES:
			raise ValueError(f"invalid scope system state: {name} -> {state}")
		roots = system.get("source_roots")
		if not isinstance(roots, list) or not roots:
			raise ValueError(f"scope system has no source roots: {name}")
		for root in roots:
			if not isinstance(root, str) or not MODULE_NAME.fullmatch(root):
				raise ValueError(f"invalid source root in scope system: {name}")
			if root in source_roots:
				raise ValueError(f"source root belongs to multiple systems: {root}")
			source_roots.add(root)
		for module in system.get("review_modules", []):
			if module not in module_names:
				raise ValueError(f"unknown scope review module: {module}")
			if module in review_modules:
				raise ValueError(f"duplicate scope review module: {module}")
			review_modules.add(module)

	integrations = scope.get("external_integrations", [])
	if not isinstance(integrations, list):
		raise ValueError("external integrations must be an array")
	integration_names: set[str] = set()
	for integration in integrations:
		if not isinstance(integration, dict):
			raise ValueError("external integrations must be objects")
		name = integration.get("name")
		if not isinstance(name, str) or not MODULE_NAME.fullmatch(name):
			raise ValueError("external integration names must use identifier syntax")
		if name in integration_names:
			raise ValueError(f"duplicate external integration: {name}")
		integration_names.add(name)
		state = integration.get("state")
		if state not in INTEGRATION_STATES:
			raise ValueError(
				f"invalid external integration state: {name} -> {state}"
			)



def module_map(modules: list[dict]) -> dict[str, dict]:
	"""按名称索引模块，并拒绝未知依赖。"""

	result = {module["name"]: module for module in modules}
	for module in modules:
		for dependency in module_dependencies(module, all_platforms=True):
			if dependency not in result:
				raise ValueError(
					f"unknown module dependency: {module['name']} -> {dependency}"
				)
	return result



def module_dependencies(
	module: dict,
	platform: str | None = None,
	all_platforms: bool = False,
) -> list[str]:
	"""返回模块的公共依赖和指定平台依赖。"""

	dependencies = list(module.get("depends", []))
	platforms = module.get("platform_depends", {})
	if all_platforms:
		for values in platforms.values():
			dependencies.extend(values)
	else:
		platform = platform_name() if platform is None else platform
		dependencies.extend(platforms.get(
			platform,
			platforms.get("posix", []),
		))
	return list(dict.fromkeys(dependencies))



def topological_modules(
	modules: list[dict],
	platform: str | None = None,
	all_platforms: bool = False,
) -> list[dict]:
	"""按清单顺序稳定拓扑排序模块，并拒绝依赖环。"""

	by_name = module_map(modules)
	state: dict[str, int] = {}
	ordered: list[dict] = []

	def visit(name: str) -> None:
		"""先加入一个模块的依赖，再加入模块本身。"""

		if state.get(name) == 2:
			return
		if state.get(name) == 1:
			raise ValueError(f"module dependency cycle at: {name}")
		state[name] = 1
		for dependency in module_dependencies(
			by_name[name],
			platform=platform,
			all_platforms=all_platforms,
		):
			visit(dependency)
		state[name] = 2
		ordered.append(by_name[name])

	for module in modules:
		visit(module["name"])
	return ordered



def dependency_closure(
	names: list[str],
	modules: list[dict],
	platform: str | None = None,
	all_platforms: bool = False,
) -> list[dict]:
	"""按依赖顺序返回一组根模块的完整闭包。"""

	by_name = module_map(modules)
	unknown = [name for name in names if name not in by_name]
	if unknown:
		raise ValueError(f"unknown modules: {','.join(unknown)}")
	needed: set[str] = set()

	def add(name: str) -> None:
		"""递归收集一个根模块。"""

		if name in needed:
			return
		for dependency in module_dependencies(
			by_name[name],
			platform=platform,
			all_platforms=all_platforms,
		):
			add(dependency)
		needed.add(name)

	for name in names:
		add(name)
	return [module for module in modules if module["name"] in needed]



def module_macro(name: str) -> str:
	"""返回公开模块选择宏。"""

	if not MODULE_NAME.fullmatch(name):
		raise ValueError(f"invalid module name: {name}")
	return f"XRT_MODULE_{name.upper()}"
