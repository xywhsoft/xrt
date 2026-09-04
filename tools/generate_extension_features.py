#!/usr/bin/env python3

"""根据扩展清单生成扩展模块选择头。"""

from __future__ import annotations

import argparse
from pathlib import Path

from xrt_manifest import (
	ROOT,
	expand_manifest_paths,
	load_manifest,
	module_dependencies,
	module_map,
	topological_modules,
)
from xrt_text import write_utf8



def _macro(prefix: str, name: str) -> str:
	"""返回扩展清单使用的模块选择宏。"""

	return prefix + name.upper()



def _feature_roots(name: str, by_name: dict[str, dict]) -> list[str]:
	"""穿过测试聚合模块，返回最近的功能模块。"""

	result: list[str] = []
	visited: set[str] = set()

	def add(current: str) -> None:
		"""递归收集一个依赖的功能根。"""

		if current in visited:
			return
		visited.add(current)
		module = by_name[current]
		if module.get("feature") is not None:
			result.append(current)
			return
		for dependency in module_dependencies(module, all_platforms=True):
			add(dependency)

	add(name)
	return result



def _selection_test(all_macro: str, module_macro: str, module: dict) -> str:
	"""返回扩展模块显式选择以及 ALL 可选排除条件。"""

	module_test = f"defined({module_macro})"
	all_exclude = module.get("all_exclude_macro")
	if all_exclude is None:
		return f"defined({all_macro}) || {module_test}"
	return (
		f"(defined({all_macro}) && !defined({all_exclude})) || \\\n"
		f"\t{module_test}"
	)



def _content(manifest_path: Path) -> tuple[Path, str]:
	"""构造一个扩展的完整模块选择头。"""

	core = load_manifest()
	extensions = [
		load_manifest(path)
		for path in expand_manifest_paths([manifest_path])
	]
	extension = extensions[-1]
	extension_modules = extension["modules"]
	extension_names = {module["name"] for module in extension_modules}
	modules = list(core["modules"]) + [
		module
		for current in extensions
		for module in current["modules"]
	]
	by_name = module_map(modules)
	owner_prefix = {
		module["name"]: current["module_prefix"]
		for current in extensions
		for module in current["modules"]
	}
	prefix = extension["module_prefix"]
	guard = extension["features_guard"]
	all_macro = prefix + "ALL"
	output = ROOT / extension["features_header"]
	feature_modules = [
		module for module in topological_modules(modules, all_platforms=True)
		if (module["name"] in extension_names) and
		(module.get("feature") is not None)
	]
	parts = [
		"/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */\n",
		f"#ifndef {guard}\n",
		f"#define {guard}\n\n",
	]

	for module in reversed(feature_modules):
		name = module["name"]
		module_macro = _macro(prefix, name)
		parts.extend((
			f"/* {name} 及其直接依赖。 */\n",
			f"#if {_selection_test(all_macro, module_macro, module)}\n",
			f"#ifndef {module['feature']}\n",
			f"#define {module['feature']}\n",
			"#endif\n",
		))
		seen: set[str] = set()
		for dependency in module_dependencies(module, all_platforms=True):
			for root in _feature_roots(dependency, by_name):
				if root in seen:
					continue
				seen.add(root)
				root_prefix = owner_prefix.get(root, "XRT_MODULE_")
				macro = _macro(root_prefix, root)
				parts.extend((
					f"#ifndef {macro}\n",
					f"#define {macro}\n",
					"#endif\n",
				))
		parts.append("#endif\n\n")

	parts.append(f"#endif /* {guard} */\n")
	return output, "".join(parts)



def main() -> int:
	"""生成或检查一个扩展的模块选择头。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("manifest")
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	manifest = (ROOT / args.manifest).resolve()
	output, content = _content(manifest)
	if args.check:
		if not output.is_file() or output.read_text(encoding="utf-8") != content:
			print(f"{output.relative_to(ROOT)} is stale")
			return 1
		print(f"{output.relative_to(ROOT)} is current")
		return 0
	output.parent.mkdir(parents=True, exist_ok=True)
	write_utf8(output, content)
	print(output.relative_to(ROOT))
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
