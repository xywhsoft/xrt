#!/usr/bin/env python3

"""从模块清单和公共头文件生成可检索的 API 符号参考。"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

from check_api_docs import _header_symbols
from xrt_text import write_utf8


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "config" / "modules.json"


REFERENCES = {
	"http": {
		"title": "HTTP 公共符号参考",
		"function_prefix": "xrtHttp",
		"constant_prefix": "XHTTP_",
		"type_prefix": "xhttp",
		"guide": "http.md",
		"output": "docs/api/http-reference.md",
	},
	"net": {
		"title": "网络公共符号参考",
		"function_prefix": "xrtNet",
		"constant_prefix": "XNET_",
		"type_prefix": "xnet",
		"guide": "net.md",
		"output": "docs/api/net-reference.md",
	},
	"regex": {
		"title": "Regex 公共符号参考",
		"function_prefix": "xrtRegex",
		"constant_prefix": "XREGEX_",
		"type_prefix": "xregex",
		"guide": "regex.md",
		"output": "docs/api/regex-reference.md",
	},
	"tls": {
		"title": "TLS 公共符号参考",
		"function_prefix": "xrtTls",
		"constant_prefix": "XTLS_",
		"type_prefix": "xtls",
		"guide": "tls.md",
		"output": "docs/api/tls-reference.md",
	},
	"websocket": {
		"title": "WebSocket 公共符号参考",
		"function_prefix": "xrtWs",
		"constant_prefix": "XWS_",
		"type_prefix": "xws",
		"guide": "websocket.md",
		"output": "docs/api/websocket-reference.md",
	},
}



def _family_headers(
	root: Path,
	manifest_path: Path,
	module_prefix: str,
) -> list[Path]:
	"""收集家族登记的全部公共头，并拒绝清单中的失效路径。"""

	manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
	modules = [
		module for module in manifest["modules"]
		if (module["name"] == module_prefix) or
		module["name"].startswith(module_prefix + "_")
	]
	if not modules:
		raise ValueError(f"unknown module family: {module_prefix}")
	headers = sorted({
		root / path
		for module in modules
		for path in module.get("public_headers", [])
	})
	missing = [path for path in headers if not path.is_file()]
	if missing:
		raise ValueError(
			"registered public header is missing: " +
			", ".join(str(path.relative_to(root)) for path in missing)
		)
	return headers



def _symbol_groups(
	headers: list[Path],
	function_prefix: str | list[str],
	constant_prefix: str | list[str],
	type_prefix: str | list[str],
) -> list[tuple[Path, list[str], list[str], list[str]]]:
	"""按首次声明头文件归组，并消除家族内部重复引用。"""

	function_prefixes = (
		[function_prefix] if isinstance(function_prefix, str) else function_prefix
	)
	constant_prefixes = (
		[constant_prefix] if isinstance(constant_prefix, str) else constant_prefix
	)
	type_prefixes = [type_prefix] if isinstance(type_prefix, str) else type_prefix
	seen_functions: set[str] = set()
	seen_constants: set[str] = set()
	seen_types: set[str] = set()
	groups = []
	for header in headers:
		text = header.read_text(encoding="utf-8")
		functions: set[str] = set()
		constants: set[str] = set()
		types: set[str] = set()
		for prefix in function_prefixes:
			functions.update(_header_symbols(text, prefix, "\0", "\0")[0])
		for prefix in constant_prefixes:
			constants.update(_header_symbols(text, "\0", prefix, "\0")[1])
		for prefix in type_prefixes:
			types.update(_header_symbols(text, "\0", "\0", prefix)[2])
		functions -= seen_functions
		constants -= seen_constants
		types -= seen_types
		seen_functions.update(functions)
		seen_constants.update(constants)
		seen_types.update(types)
		if functions or constants or types:
			groups.append((
				header,
				sorted(functions),
				sorted(constants),
				sorted(types),
			))
	return groups



def _symbol_section(lines: list[str], title: str, symbols: list[str]) -> None:
	"""写出一个稳定、紧凑的符号列表。"""

	if not symbols:
		return
	lines.extend([f"### {title} ({len(symbols)})", ""])
	lines.extend(f"- `{symbol}`" for symbol in symbols)
	lines.append("")



def _render(
	root: Path,
	output: Path,
	title: str,
	guide: str,
	groups: list[tuple[Path, list[str], list[str], list[str]]],
	manifest_label: str = "config/modules.json",
) -> str:
	"""生成面向检索的符号参考，并链接权威公共头契约。"""

	function_count = sum(len(group[1]) for group in groups)
	constant_count = sum(len(group[2]) for group in groups)
	type_count = sum(len(group[3]) for group in groups)
	lines = [
		f"# {title}",
		"",
		f"此文件由 `tools/generate_api_reference.py` 从 `{manifest_label}` 与公共头生成。",
		"不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见",
		f"[{guide}]({guide})；每个声明的精确契约以链接的公共头中文注释为准。",
		"",
		f"当前登记 `{function_count}` 个函数、`{constant_count}` 个常量或宏、",
		f"`{type_count}` 个公共类型。",
		"",
	]
	for header, functions, constants, types in groups:
		relative = Path(os.path.relpath(header, output.parent)).as_posix()
		label = header.relative_to(root).as_posix()
		lines.extend([
			f"## `{label}`",
			"",
			f"[查看带契约注释的公共头]({relative})",
			"",
		])
		_symbol_section(lines, "函数", functions)
		_symbol_section(lines, "常量与宏", constants)
		_symbol_section(lines, "类型", types)
	return "\n".join(lines)



def _generate(reference: str) -> tuple[Path, str]:
	"""读取一个登记配置并返回目标路径与完整内容。"""

	config = REFERENCES[reference]
	output = ROOT / config["output"]
	headers = _family_headers(ROOT, MANIFEST, reference)
	groups = _symbol_groups(
		headers,
		config["function_prefix"],
		config["constant_prefix"],
		config["type_prefix"],
	)
	return output, _render(
		ROOT,
		output,
		config["title"],
		config["guide"],
		groups,
	)



def _generate_manifest(manifest_path: Path) -> tuple[Path, str]:
	"""按扩展清单配置生成覆盖全部扩展公共头的符号参考。"""

	manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
	config = manifest.get("api_reference")
	if not isinstance(config, dict):
		raise ValueError("extension manifest has no api_reference configuration")
	headers = sorted({
		ROOT / path
		for module in manifest["modules"]
		for path in module.get("public_headers", [])
	})
	missing = [path for path in headers if not path.is_file()]
	if missing:
		raise ValueError(
			"registered public header is missing: " +
			", ".join(str(path.relative_to(ROOT)) for path in missing)
		)
	headers = [
		path for path in headers
		if "XRT_API" in path.read_text(encoding="utf-8")
	]
	output = ROOT / config["output"]
	groups = _symbol_groups(
		headers,
		config.get("function_prefixes", config["function_prefix"]),
		config.get("constant_prefixes", config["constant_prefix"]),
		config.get("type_prefixes", config["type_prefix"]),
	)
	return output, _render(
		ROOT,
		output,
		config["title"],
		config["guide"],
		groups,
		manifest_path.relative_to(ROOT).as_posix(),
	)



def main() -> int:
	"""生成全部或指定参考，或检查现有文件没有漂移。"""

	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument(
		"--family",
		choices=["all", *sorted(REFERENCES)],
		default="all",
	)
	parser.add_argument(
		"--manifest",
		help="按扩展清单中的 api_reference 配置生成完整扩展索引",
	)
	parser.add_argument("--check", action="store_true")
	arguments = parser.parse_args()
	if arguments.manifest is not None:
		manifest = (ROOT / arguments.manifest).resolve()
		try:
			output, content = _generate_manifest(manifest)
		except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
			raise SystemExit(str(error)) from error
		if arguments.check:
			if not output.is_file() or output.read_text(encoding="utf-8") != content:
				raise SystemExit(f"{output.relative_to(ROOT)} is out of date")
			print(f"[pass] {output.relative_to(ROOT)} is current")
			return 0
		output.parent.mkdir(parents=True, exist_ok=True)
		write_utf8(output, content)
		print(f"[generate] {output.relative_to(ROOT)}")
		return 0
	families = sorted(REFERENCES) if arguments.family == "all" else [arguments.family]
	for family in families:
		try:
			output, content = _generate(family)
		except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
			raise SystemExit(str(error)) from error
		if arguments.check:
			if not output.is_file() or output.read_text(encoding="utf-8") != content:
				raise SystemExit(f"{output.relative_to(ROOT)} is out of date")
			print(f"[pass] {output.relative_to(ROOT)} is current")
			continue
		output.parent.mkdir(parents=True, exist_ok=True)
		write_utf8(output, content)
		print(f"[generate] {output.relative_to(ROOT)}")
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
