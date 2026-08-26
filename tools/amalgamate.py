#!/usr/bin/env python3

"""根据模块清单生成 XRT 单头文件。"""

from __future__ import annotations

import argparse
import posixpath
import re
from pathlib import Path, PurePosixPath

from generate_extension_features import _content as extension_features_content
from generate_features import check as check_features
from generate_features import generate as generate_features
from xrt_manifest import (
	MANIFEST,
	expand_manifest_paths,
	load_manifest,
	module_dependencies,
	module_map,
	topological_modules,
)
from xrt_text import write_utf8


ROOT = Path(__file__).resolve().parents[1]
LOCAL_INCLUDE = re.compile(
	r'^\s*#\s*include\s*(?P<open>[<"])(?P<target>[^>"]+)[>"]\s*$'
)
SELECTION_MACRO = re.compile(
	r"\b[A-Z][A-Z0-9_]*_(?:MODULE|FEATURE)_[A-Z0-9_]+\b"
)



def _license_banner() -> str:
	"""把根目录许可文本转换为可嵌入 C 源码的注释。"""

	lines = (ROOT / "LICENSE").read_text(encoding="utf-8").splitlines()
	parts = ["/*\n"]
	for line in lines:
		parts.append(f" * {line}\n" if line else " *\n")
	parts.append(" */\n")
	return "".join(parts)



def _read_part(
	path: str,
	known: set[str],
	include_dirs: list[str],
) -> str:
	"""读取并移除单头文件中无效的本地 include。"""

	lines = (ROOT / path).read_text(encoding="utf-8").splitlines()
	kept: list[str] = []
	for line in lines:
		match = LOCAL_INCLUDE.match(line)
		dependency = None if match is None else _local_path(
			path,
			match.group("target"),
			include_dirs,
		)
		if dependency in known:
			continue
		kept.append(line)
	return "\n".join(kept).rstrip() + "\n"




def _local_path(
	path: str,
	target: str,
	include_dirs: list[str],
) -> str | None:
	"""把一个被移除的本地 include 转换为仓库相对路径。"""

	relative = posixpath.normpath(
		str(PurePosixPath(path).parent.joinpath(target))
	)
	if (ROOT / relative).is_file():
		return _bridge_target(relative, include_dirs) or relative
	for directory in include_dirs:
		candidate = posixpath.normpath(
			str(PurePosixPath(directory).joinpath(target))
		)
		if (ROOT / candidate).is_file():
			return _bridge_target(candidate, include_dirs) or candidate
	return None



def _bridge_target(path: str, include_dirs: list[str]) -> str | None:
	"""把只有 include guard 和单个 include 的桥接头折叠到真实头。"""

	lines = [
		line.strip()
		for line in (ROOT / path).read_text(encoding="utf-8").splitlines()
		if line.strip()
	]
	if (
		(len(lines) != 4) or
		not lines[0].startswith("#ifndef ") or
		not lines[1].startswith("#define ") or
		not lines[2].startswith("#include ") or
		(lines[3] != "#endif")
	):
		return None
	match = LOCAL_INCLUDE.match(lines[2])
	if match is None:
		return None
	target = match.group("target")
	relative = posixpath.normpath(
		str(PurePosixPath(path).parent.joinpath(target))
	)
	if (ROOT / relative).is_file():
		return relative
	for directory in include_dirs:
		candidate = posixpath.normpath(
			str(PurePosixPath(directory).joinpath(target))
		)
		if (ROOT / candidate).is_file():
			return candidate
	return None



def _part_dependencies(
	path: str,
	known: set[str],
	include_dirs: list[str],
) -> list[str]:
	"""读取一个头文件在当前拼接集合中的直接本地依赖。"""

	dependencies: list[str] = []
	for line in (ROOT / path).read_text(encoding="utf-8").splitlines():
		match = LOCAL_INCLUDE.match(line)
		if match is None:
			continue
		dependency = _local_path(
			path,
			match.group("target"),
			include_dirs,
		)
		if dependency in known:
			dependencies.append(dependency)
	return list(dict.fromkeys(dependencies))



def _topological_parts(
	paths: list[str],
	include_dirs: list[str],
) -> list[str]:
	"""按真实本地 include 图稳定排列一组头文件。"""

	unique = list(dict.fromkeys(paths))
	known = set(unique)
	state: dict[str, int] = {}
	ordered: list[str] = []

	def visit(path: str) -> None:
		"""先输出当前头文件依赖的其他本地头。"""

		if state.get(path) == 2:
			return
		if state.get(path) == 1:
			raise ValueError(f"local include cycle at: {path}")
		state[path] = 1
		for dependency in _part_dependencies(path, known, include_dirs):
			visit(dependency)
		state[path] = 2
		ordered.append(path)

	for path in unique:
		visit(path)
	return ordered



def _validate_local_includes(
	paths: list[str],
	headers: set[str],
	include_dirs: list[str] | None = None,
) -> None:
	"""拒绝会被移除却没有进入单头文件的本地头依赖。"""

	for path in paths:
		for line in (ROOT / path).read_text(encoding="utf-8").splitlines():
			match = LOCAL_INCLUDE.match(line)
			if match is None:
				continue
			dependency = _local_path(
				path,
				match.group("target"),
				include_dirs or ["include"],
			)
			target = match.group("target")
			local_names = {"xrt"}
			for directory in include_dirs or ["include"]:
				root = ROOT / directory
				if not root.is_dir():
					continue
				for child in root.iterdir():
					local_names.add(child.stem if child.is_file() else child.name)
			target_root = target.split("/", 1)[0]
			if target_root.endswith(".h"):
				target_root = target_root[:-2]
			looks_local = (
				target.startswith(".") or
				(target_root in local_names)
			)
			if (
				((dependency is not None) and (dependency not in headers)) or
				((dependency is None) and looks_local)
			):
				raise ValueError(
					f"single header misses local include: {path} -> "
					f"{dependency or target}"
				)




def _selection_macros(paths: list[str]) -> list[str]:
	"""收集完整声明临时启用并在包含结束后恢复的选择宏。"""

	macros = {"XRT_MODULE_ALL"}
	for path in paths:
		text = (ROOT / path).read_text(encoding="utf-8")
		macros.update(SELECTION_MACRO.findall(text))
	return sorted(macros)




def _declaration_selection_begin(
	macros: list[str],
	all_macros: list[str] | None = None,
) -> str:
	"""记录调用方选择状态并临时启用所有公共声明。"""

	parts: list[str] = []
	for macro in macros:
		marker = f"XRT_DECLARATIONS_RESTORE_{macro}"
		parts.extend((
			f"#if !defined({macro})\n",
			f"#define {marker} 1\n",
			"#endif\n",
		))
	for macro in all_macros or ["XRT_MODULE_ALL"]:
		marker = f"XRT_DECLARATIONS_RESTORE_{macro}"
		parts.extend((
			f"#if defined({marker})\n",
			f"#define {macro} 1\n",
			"#endif\n",
		))
	return "".join(parts)




def _declaration_selection_end(macros: list[str]) -> str:
	"""恢复调用方在包含声明头之前的模块和特性宏状态。"""

	parts: list[str] = []
	for macro in macros:
		marker = f"XRT_DECLARATIONS_RESTORE_{macro}"
		parts.extend((
			f"#if defined({marker})\n",
			f"#undef {macro}\n",
			f"#undef {marker}\n",
			"#endif\n",
		))
	return "".join(parts)



def _append_section(
	parts: list[str],
	title: str,
	paths: list[str],
	seen: set[str],
	known: set[str],
	include_dirs: list[str],
	guards: dict[str, tuple[str, ...] | None] | None = None,
) -> None:
	"""向单头文件追加尚未出现且有清晰来源边界的文件。"""

	for path in paths:
		if path in seen:
			continue
		seen.add(path)
		parts.append("\n\n/* ========================================================================== */\n")
		parts.append(f"/* {title}: {path} */\n")
		parts.append("/* ========================================================================== */\n\n")
		guard = None if guards is None else guards[path]
		if guard:
			parts.append(_guard_begin(guard))
		parts.append(_read_part(path, known, include_dirs))
		if guard:
			parts.append("#endif\n")




def _guard_begin(features: tuple[str, ...]) -> str:
	"""生成一个稳定分行的实现功能条件。"""

	conditions = [f"defined({feature})" for feature in features]
	if len(conditions) == 1:
		return f"#if {conditions[0]}\n"
	return "#if " + " || \\\n\t".join(conditions) + "\n"



def _support_features(name: str, modules: list[dict]) -> list[str]:
	"""返回直接使用一个无公开宏支撑模块的最近功能集合。"""

	by_name = module_map(modules)
	reverse: dict[str, list[str]] = {module["name"]: [] for module in modules}
	for module in modules:
		for dependency in module_dependencies(module, all_platforms=True):
			reverse[dependency].append(module["name"])

	result: list[str] = []
	visited = {name}
	queue = list(reverse[name])
	while queue:
		current = queue.pop(0)
		if current in visited:
			continue
		visited.add(current)
		module = by_name[current]
		feature = module.get("feature")
		if feature is not None:
			result.append(feature)
			continue
		queue.extend(reverse[current])
	return result



def _implementation_guards(
	modules: list[dict],
) -> dict[str, tuple[str, ...] | None]:
	"""按实现资产的真实所有者生成精确功能边界。"""

	owners: dict[str, list[dict]] = {}
	for module in modules:
		for key in ("internal_headers", "sources"):
			for path in module[key]:
				owners.setdefault(path, []).append(module)

	guards: dict[str, tuple[str, ...] | None] = {}
	for path, path_owners in owners.items():
		if any(module["name"] == "core" for module in path_owners):
			guards[path] = None
			continue

		features: list[str] = []
		for module in path_owners:
			feature = module.get("feature")
			if feature is not None:
				features.append(feature)
				continue
			features.extend(_support_features(module["name"], modules))
		features = list(dict.fromkeys(features))
		if not features:
			raise ValueError(f"implementation has no feature owner: {path}")
		guards[path] = tuple(features)
	return guards



def _manifests(overlays: list[Path] | None = None) -> list[dict]:
	"""读取核心及扩展清单，并拒绝跨产品模块重名。"""

	overlays = expand_manifest_paths(overlays or [])
	result = [load_manifest(MANIFEST)]
	result.extend(load_manifest(path) for path in overlays or [])
	names = [
		module["name"]
		for manifest in result
		for module in manifest["modules"]
	]
	if len(names) != len(set(names)):
		raise ValueError("core and extension manifests contain duplicate modules")
	return result



def _manifest_setting(
	manifest: dict,
	name: str,
	default: str,
) -> str:
	"""读取生成产物必须使用的非空字符串设置。"""

	value = manifest.get(name, default)
	if not isinstance(value, str) or not value:
		raise ValueError(f"invalid manifest setting: {name}")
	return value



def _content(overlays: list[Path] | None = None) -> tuple[Path, str]:
	"""从当前模块清单和已生成特性头构造单头文件内容。"""

	manifests = _manifests(overlays)
	product = manifests[-1]
	modules = topological_modules([
		module
		for manifest in manifests
		for module in manifest["modules"]
	], all_platforms=True)
	include_dirs = ["include"]
	for manifest in manifests[1:]:
		include_dirs.extend(manifest.get("include_dirs", []))
	include_dirs = list(dict.fromkeys(include_dirs))
	implementation_macro = _manifest_setting(
		product,
		"implementation_macro",
		"XRT_IMPLEMENTATION",
	)
	once_macro = _manifest_setting(
		product,
		"implementation_once_macro",
		implementation_macro + "_ONCE",
	)
	header_guard = _manifest_setting(
		product,
		"single_guard",
		"XRT_SINGLE_HEADER_H",
	)
	single_marker = _manifest_setting(
		product,
		"single_marker",
		"XRT_SINGLE_HEADER",
	)
	implementation_guards = _implementation_guards(modules)
	public_seen: set[str] = set()
	implementation_seen: set[str] = set()
	public_headers: list[str] = []
	internal_headers: list[str] = []
	sources: list[str] = []
	parts = [
		_license_banner(),
		"\n",
		"/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */\n",
		f"#if defined({implementation_macro}) && \\\n\t!defined(_WIN32) && !defined(_WIN64)\n",
		"\t#if defined(__linux__) && !defined(_GNU_SOURCE)\n",
		"\t\t#define _GNU_SOURCE 1\n",
		"\t#endif\n",
		"\t#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)\n",
		"\t\t#define _DARWIN_C_SOURCE 1\n",
		"\t#endif\n",
		"\t#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)\n"
		"\t\t#define _CRT_SECURE_NO_WARNINGS\n"
		"\t#endif\n"
		"\t#if !defined(_POSIX_C_SOURCE)\n",
		"\t\t#define _POSIX_C_SOURCE 200809L\n",
		"\t#endif\n",
		"\t#if !defined(_FILE_OFFSET_BITS)\n",
		"\t\t#define _FILE_OFFSET_BITS 64\n",
		"\t#endif\n",
		"#endif\n",
		f"#ifndef {header_guard}\n",
		f"#define {header_guard}\n",
		"#define XRT_SINGLE_HEADER 1\n",
	]
	if single_marker != "XRT_SINGLE_HEADER":
		parts.append(f"#define {single_marker} 1\n")

	for module in modules:
		public_headers.extend(module["public_headers"])
		internal_headers.extend(module["internal_headers"])
		sources.extend(module["sources"])
	known_headers = set(public_headers + internal_headers)
	known_headers.add("include/xrt.h")
	_validate_local_includes(
		public_headers + internal_headers + sources,
		known_headers,
		include_dirs,
	)
	feature_headers = [
		manifest["features_header"]
		for manifest in reversed(manifests[1:])
		if "features_header" in manifest
	]
	feature_headers.append("include/xrt/features.h")
	feature_headers = [
		path for path in dict.fromkeys(feature_headers)
		if path in known_headers
	]
	_append_section(
		parts,
		"feature selection" if overlays else "public",
		feature_headers,
		public_seen,
		known_headers,
		include_dirs,
	)
	_append_section(
		parts,
		"public",
		_topological_parts(public_headers, include_dirs),
		public_seen,
		known_headers,
		include_dirs,
	)
	parts.append("\n#endif\n")
	parts.append(
		f"\n#if defined({implementation_macro}) && !defined({once_macro})\n"
	)
	parts.append(f"#define {once_macro} 1\n")
	_append_section(
		parts,
		"internal",
		_topological_parts(internal_headers, include_dirs),
		implementation_seen,
		known_headers,
		include_dirs,
		implementation_guards,
	)
	_append_section(
		parts,
		"source",
		sources,
		implementation_seen,
		known_headers,
		include_dirs,
		implementation_guards,
	)
	parts.append("\n#endif\n")

	output = ROOT / _manifest_setting(
		product,
		"single_header",
		"single/xrt.h",
	)
	content = "".join(parts).replace("\r\n", "\n")
	return output, content




def _declaration_content(
	overlays: list[Path] | None = None,
) -> tuple[Path, str]:
	"""从公共模块头生成不包含实现的单头声明文件。"""

	manifests = _manifests(overlays)
	product = manifests[-1]
	modules = topological_modules([
		module
		for manifest in manifests
		for module in manifest["modules"]
	], all_platforms=True)
	include_dirs = ["include"]
	for manifest in manifests[1:]:
		include_dirs.extend(manifest.get("include_dirs", []))
	include_dirs = list(dict.fromkeys(include_dirs))
	public_headers: list[str] = []
	for module in modules:
		public_headers.extend(module["public_headers"])
	public_headers = list(dict.fromkeys(public_headers))
	feature_headers = [
		manifest["features_header"]
		for manifest in reversed(manifests[1:])
		if "features_header" in manifest
	]
	feature_headers.append("include/xrt/features.h")
	feature_headers = [
		path for path in dict.fromkeys(feature_headers)
		if path in public_headers
	]
	public_headers = _topological_parts(public_headers, include_dirs)
	selection_macros = _selection_macros(public_headers)
	all_macros = [
		manifest.get("module_prefix", "XRT_MODULE_") + "ALL"
		for manifest in manifests
	]
	guard = _manifest_setting(
		product,
		"declaration_guard",
		"XRT_DECLARATIONS_H",
	)
	declaration_markers = list(dict.fromkeys(
		_manifest_setting(
			manifest,
			"declaration_marker",
			"XRT_DECLARATIONS",
		)
		for manifest in manifests
	))
	known_headers = set(public_headers)
	# 扩展声明头已经内嵌核心公共声明；扩展聚合头中的 <xrt.h>
	# 只是模块化布局桥接，不能残留到独立声明单头中。
	known_headers.add("include/xrt.h")

	parts = [
		_license_banner(),
		"\n",
		"/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */\n",
		f"#ifndef {guard}\n",
		f"#define {guard}\n",
		*(f"#define {marker} 1\n" for marker in declaration_markers),
		_declaration_selection_begin(selection_macros, all_macros),
	]
	seen: set[str] = set()
	_append_section(
		parts,
		"feature selection" if overlays else "public",
		feature_headers,
		seen,
		known_headers,
		include_dirs,
	)
	_append_section(
		parts,
		"public",
		public_headers,
		seen,
		known_headers,
		include_dirs,
	)
	parts.append(_declaration_selection_end(selection_macros))
	parts.append("\n#endif\n")

	output = ROOT / _manifest_setting(
		product,
		"declaration_header",
		"single/xrt_decl.h",
	)
	return output, "".join(parts).replace("\r\n", "\n")



def _generate_features(overlays: list[Path] | None = None) -> None:
	"""生成核心与全部扩展的模块选择头。"""

	generate_features()
	for manifest in expand_manifest_paths(overlays or []):
		output, content = extension_features_content(manifest)
		output.parent.mkdir(parents=True, exist_ok=True)
		if not output.exists() or output.read_text(encoding="utf-8") != content:
			write_utf8(output, content)



def _check_features(overlays: list[Path] | None = None) -> bool:
	"""检查核心与全部扩展的模块选择头没有漂移。"""

	if not check_features():
		return False
	for manifest in expand_manifest_paths(overlays or []):
		output, content = extension_features_content(manifest)
		if not output.is_file() or output.read_text(encoding="utf-8") != content:
			return False
	return True



def generate(overlays: list[Path] | None = None) -> Path:
	"""生成特性头和单头文件，仅在内容变化时写入。"""

	_generate_features(overlays)
	output, content = _content(overlays)
	declaration_output, declaration_content = _declaration_content(overlays)
	for path, text in (
		(output, content),
		(declaration_output, declaration_content),
	):
		path.parent.mkdir(parents=True, exist_ok=True)
		if not path.exists() or path.read_text(encoding="utf-8") != text:
			write_utf8(path, text)

	return output



def check(overlays: list[Path] | None = None) -> bool:
	"""只读检查特性头和单头文件是否与清单及源码一致。"""

	if not _check_features(overlays):
		return False
	output, content = _content(overlays)
	declaration_output, declaration_content = _declaration_content(overlays)
	return (
		output.exists() and
		output.read_text(encoding="utf-8") == content and
		declaration_output.exists() and
		declaration_output.read_text(encoding="utf-8") == declaration_content
	)



def main() -> int:
	"""执行生成或只读一致性检查。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("--check", action="store_true")
	parser.add_argument(
		"--manifest",
		action="append",
		default=[],
		help="叠加一个仓库相对路径的扩展模块清单",
	)
	args = parser.parse_args()
	try:
		overlays = expand_manifest_paths([Path(path) for path in args.manifest])
	except (OSError, ValueError) as error:
		parser.error(str(error))
	if args.check:
		if not check(overlays):
			print("single headers are stale")
			return 1
		print("single headers are current")
		return 0
	print(generate(overlays).relative_to(ROOT))
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
