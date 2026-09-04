#!/usr/bin/env python3

"""根据模块清单生成公开的正向模块选择头。"""

from __future__ import annotations

import argparse
from pathlib import Path

from xrt_manifest import (
	ROOT,
	load_manifest,
	module_dependencies,
	module_macro,
	module_map,
	topological_modules,
)
from xrt_text import write_utf8


OUTPUT = ROOT / "include" / "xrt" / "features.h"
PLATFORM_TESTS = {
	"windows": "defined(_WIN32)",
	"linux": "defined(__linux__) && !defined(__ANDROID__)",
	"android": "defined(__ANDROID__)",
	"macos": "defined(__APPLE__) && defined(__MACH__)",
	"bsd": (
		"defined(__FreeBSD__) || defined(__OpenBSD__) || "
		"defined(__NetBSD__) || defined(__DragonFly__)"
	),
	"posix": (
		"!defined(_WIN32) && !defined(__linux__) && !defined(__ANDROID__) && "
		"!(defined(__APPLE__) && defined(__MACH__)) && "
		"!defined(__FreeBSD__) && !defined(__OpenBSD__) && "
		"!defined(__NetBSD__) && !defined(__DragonFly__)"
	),
}



def _platform_test(platform: str, platforms: dict[str, list[str]]) -> str:
	"""返回与清单平台回退语义一致的预处理条件。"""

	test = PLATFORM_TESTS.get(platform)
	if test is None:
		raise ValueError(f"unknown manifest platform: {platform}")
	if platform != "posix":
		return test
	conditions = [
		PLATFORM_TESTS[name]
		for name in ("windows", "linux", "android", "macos", "bsd")
		if name not in platforms
	]
	conditions.append(test)
	return " || \\\n\t".join(f"({condition})" for condition in conditions)



def _feature_roots(name: str, by_name: dict[str, dict]) -> list[str]:
	"""穿过无特性宏的组织模块，返回实际可选择的依赖根。"""

	result: list[str] = []
	visited: set[str] = set()

	def add(current: str) -> None:
		"""加入最近的带特性宏模块。"""

		if current in visited:
			return
		visited.add(current)
		module = by_name[current]
		if module.get("feature") is not None:
			result.append(current)
			return
		for dependency in module.get("depends", []):
			add(dependency)

	add(name)
	return result



def _define_module(parts: list[str], name: str) -> None:
	"""追加一个幂等的模块选择宏定义。"""

	macro = module_macro(name)
	parts.extend((
		f"#ifndef {macro}\n",
		f"#define {macro}\n",
		"#endif\n",
	))



def _append_dependencies(
	parts: list[str],
	dependencies: list[str],
	by_name: dict[str, dict],
) -> None:
	"""追加一组模块依赖定义。"""

	seen: set[str] = set()
	for dependency in dependencies:
		for root in _feature_roots(dependency, by_name):
			if root in seen:
				continue
			seen.add(root)
			_define_module(parts, root)



def _selection_test(module: dict) -> str:
	"""返回模块显式选择以及 MODULE_ALL 可选排除条件。"""

	module_test = f"defined({module_macro(module['name'])})"
	all_exclude = module.get("all_exclude_macro")
	if all_exclude is None:
		return f"defined(XRT_MODULE_ALL) || {module_test}"
	return (
		f"(defined(XRT_MODULE_ALL) && !defined({all_exclude})) || \\\n"
		f"\t{module_test}"
	)



def _content() -> str:
	"""构造完整模块选择头内容。"""

	manifest = load_manifest()
	modules = manifest["modules"]
	by_name = module_map(modules)
	feature_modules = [
		module for module in topological_modules(modules, all_platforms=True)
		if module.get("feature") is not None
	]
	parts = [
		"/* 此文件由 tools/generate_features.py 生成，请勿直接修改。 */\n",
		"#ifndef XRT_FEATURES_H\n",
		"#define XRT_FEATURES_H\n\n",
	]

	for module in reversed(feature_modules):
		name = module["name"]
		feature = module["feature"]
		parts.extend((
			f"/* {name} 及其直接依赖。 */\n",
			f"#if {_selection_test(module)}\n",
			f"#ifndef {feature}\n",
			f"#define {feature}\n",
			"#endif\n",
		))
		_append_dependencies(parts, module.get("depends", []), by_name)
		platforms = module.get("platform_depends", {})
		for platform, dependencies in platforms.items():
			test = _platform_test(platform, platforms)
			parts.append(f"#if {test}\n")
			_append_dependencies(parts, dependencies, by_name)
			parts.append("#endif\n")
		parts.append("#endif\n\n")

	parts.append("#endif\n")
	return "".join(parts)



def generate(output: Path = OUTPUT) -> Path:
	"""生成模块选择头，仅在内容变化时写入。"""

	content = _content().replace("\r\n", "\n")
	output.parent.mkdir(parents=True, exist_ok=True)
	if not output.exists() or output.read_text(encoding="utf-8") != content:
		write_utf8(output, content)
	return output



def check(output: Path = OUTPUT) -> bool:
	"""检查已提交生成物是否与模块清单一致。"""

	return output.exists() and output.read_text(encoding="utf-8") == _content()



def main() -> int:
	"""执行生成或只读一致性检查。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	if args.check:
		if not check():
			print("include/xrt/features.h is stale")
			return 1
		print("include/xrt/features.h is current")
		return 0
	print(generate().relative_to(ROOT))
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
