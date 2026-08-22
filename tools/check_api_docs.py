#!/usr/bin/env python3

"""验证一个模块家族的公共符号都能在登记文档中检索到。"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "config/modules.json"



def _family_files(
	root: Path,
	manifest: Path,
	module_prefix: str,
) -> tuple[list[Path], list[Path]]:
	"""从模块清单收集家族直接登记的公共头和文档。"""

	data = json.loads(manifest.read_text(encoding="utf-8"))
	modules = [
		module for module in data["modules"]
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
	docs = sorted({
		root / path
		for module in modules
		for path in module.get("docs", [])
	})
	missing = [path for path in headers + docs if not path.is_file()]
	if missing:
		raise ValueError(
			"registered API documentation input is missing: " +
			", ".join(str(path.relative_to(root)) for path in missing)
		)
	return headers, docs



def _manifest_files(
	root: Path,
	manifest: Path,
) -> tuple[list[Path], list[Path], dict]:
	"""收集扩展清单登记的全部公共头、文档和符号前缀。"""

	data = json.loads(manifest.read_text(encoding="utf-8"))
	config = data.get("api_reference")
	if not isinstance(config, dict):
		raise ValueError("extension manifest has no api_reference configuration")
	headers = sorted({
		root / path
		for module in data["modules"]
		for path in module.get("public_headers", [])
		if (root / path).is_file() and
		("XRT_API" in (root / path).read_text(encoding="utf-8"))
	})
	docs = sorted({
		root / path
		for module in data["modules"]
		for path in module.get("docs", [])
	})
	missing = [path for path in docs if not path.is_file()]
	if missing:
		raise ValueError(
			"registered API documentation input is missing: " +
			", ".join(str(path.relative_to(root)) for path in missing)
		)
	return headers, docs, config



def _header_symbols(
	text: str,
	function_prefix: str,
	constant_prefix: str,
	type_prefix: str,
) -> tuple[set[str], set[str], set[str]]:
	"""提取导出函数以及家族前缀约束的常量和类型标识符。"""

	functions: set[str] = set()
	for match in re.finditer(r"\bXRT_API\b([^;]+);", text, re.DOTALL):
		declaration = match.group(1)
		name = re.search(
			r"\b(" + re.escape(function_prefix) +
			r"[A-Za-z0-9_]*)\s*\(",
			declaration,
		)
		if name is not None:
			functions.add(name.group(1))

	constants = set(re.findall(
		r"\b" + re.escape(constant_prefix) + r"[A-Z0-9_]+\b",
		text,
	))
	include_guards = set(re.findall(
		r"^\s*#ifndef\s+([A-Z][A-Z0-9_]*)\s*\n\s*#define\s+\1\b",
		text,
		re.MULTILINE,
	))
	constants.difference_update(include_guards)
	types = set(re.findall(
		r"\b" + re.escape(type_prefix) + r"[a-z0-9_]+\b",
		text,
	))
	return functions, constants, types



def _missing(symbols: set[str], documentation: str) -> list[str]:
	"""返回没有以完整标识符形式出现在文档中的符号。"""

	return sorted(
		symbol for symbol in symbols
		if re.search(
			r"(?<![A-Za-z0-9_])" + re.escape(symbol) +
			r"(?![A-Za-z0-9_])",
			documentation,
		) is None
	)



def _arguments() -> argparse.Namespace:
	"""解析一个模块家族及其 C 标识符前缀。"""

	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--manifest")
	parser.add_argument("--module-prefix")
	parser.add_argument("--function-prefix")
	parser.add_argument("--constant-prefix")
	parser.add_argument("--type-prefix")
	return parser.parse_args()



def main() -> int:
	"""检查清单登记范围并输出稳定的门禁摘要。"""

	arguments = _arguments()
	try:
		if arguments.manifest is not None:
			manifest = (ROOT / arguments.manifest).resolve()
			headers, docs, config = _manifest_files(ROOT, manifest)
			function_prefixes = config.get(
				"function_prefixes",
				[config["function_prefix"]],
			)
			constant_prefixes = config.get(
				"constant_prefixes",
				[config["constant_prefix"]],
			)
			type_prefixes = config.get(
				"type_prefixes",
				[config["type_prefix"]],
			)
			family = config["title"]
		else:
			if not all((
				arguments.module_prefix,
				arguments.function_prefix,
				arguments.constant_prefix,
				arguments.type_prefix,
			)):
				raise ValueError(
					"core family check requires all symbol prefixes"
				)
			headers, docs = _family_files(
				ROOT,
				MANIFEST,
				arguments.module_prefix,
			)
			function_prefixes = [arguments.function_prefix]
			constant_prefixes = [arguments.constant_prefix]
			type_prefixes = [arguments.type_prefix]
			family = arguments.module_prefix
	except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
		print(f"[docs-fail] {error}")
		return 1

	header_text = "\n".join(
		path.read_text(encoding="utf-8") for path in headers
	)
	documentation = "\n".join(
		path.read_text(encoding="utf-8") for path in docs
	)
	functions: set[str] = set()
	constants: set[str] = set()
	types: set[str] = set()
	for prefix in function_prefixes:
		functions.update(_header_symbols(header_text, prefix, "\0", "\0")[0])
	for prefix in constant_prefixes:
		constants.update(_header_symbols(header_text, "\0", prefix, "\0")[1])
	for prefix in type_prefixes:
		types.update(_header_symbols(header_text, "\0", "\0", prefix)[2])
	missing = {
		"function": _missing(functions, documentation),
		"constant": _missing(constants, documentation),
		"type": _missing(types, documentation),
	}
	failed = False
	for kind, symbols in missing.items():
		for symbol in symbols:
			failed = True
			print(f"[docs-missing] kind={kind} symbol={symbol}")
	print(
		"[docs-summary] "
		f"family={family} "
		f"functions={len(functions)} "
		f"constants={len(constants)} "
		f"types={len(types)} "
		f"missing={sum(len(value) for value in missing.values())}"
	)
	return 1 if failed else 0



if __name__ == "__main__":
	raise SystemExit(main())
