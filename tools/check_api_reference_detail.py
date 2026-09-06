#!/usr/bin/env python3

"""按 DOC_SPEC 校验 API 参考文档的完备性与签名一致性（G1/G2/G4）。

G1 签名一致：文档每个 API 节的首个 ```c 签名块与公共头声明
   （剥离 XRT_API 前缀、规范化空白）逐字符一致。
G2 覆盖完备：公共头全部 XRT_API 函数各有 `###` 节；每节有
   `#### 参数`（表格行覆盖全部形参）与 `#### 返回值`；返回
   bool 或指针的函数另有 `#### 错误`；有 `#### 范例`。
G4 范例注册：`#### 范例` 引用的 examples 路径存在于
   config/modules.json 任一 examples 数组。
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "config" / "modules.json"
DOCS_DIR = ROOT / "docs" / "api"


def load_manifest():
	data = json.loads(MANIFEST.read_text(encoding="utf-8"))
	doc_headers = {}
	registered = set()
	for module in data["modules"]:
		for doc in module.get("docs", []):
			doc_headers.setdefault(doc, set()).update(
				module.get("public_headers", [])
			)
		for example in module.get("examples", []):
			registered.add(example)
	return doc_headers, registered


def extract_header_functions(text):
	"""返回 {name: {decl, params, ret}}；decl 为规范化签名文本。"""
	functions = {}
	i = 0
	while True:
		idx = text.find("XRT_API", i)
		if idx < 0:
			break
		depth = 0
		started = False
		j = idx
		while j < len(text):
			ch = text[j]
			if ch == "(":
				depth += 1
				started = True
			elif ch == ")":
				depth -= 1
			elif ch == ";" and started and depth == 0:
				break
			j += 1
		if j >= len(text):
			i = idx + 7
			continue
		decl = text[idx : j + 1]
		match = re.search(r"XRT_API\s+.*?\b(xrt[A-Z]\w*)\s*\(", decl, re.S)
		if match is None:
			i = j + 1
			continue
		name = match.group(1)
		open_at = decl.find("(", match.start(1))
		close_at = decl.find(")", open_at)
		param_text = decl[open_at + 1 : close_at]
		ret = decl[len("XRT_API") : match.start(1)].strip()
		functions[name] = {
			"decl": normalize(decl),
			"params": param_names(param_text),
			"ret": ret,
		}
		i = j + 1
	return functions


def normalize(text):
	text = re.sub(r"^XRT_API\s+", "", text.strip())
	text = text.rstrip(";").strip()
	text = re.sub(r"\s+", " ", text)
	text = text.replace("( ", "(").replace(" )", ")")
	text = re.sub(r" ,", ",", text)
	return text


def param_names(param_text):
	parts = split_top(param_text)
	names = []
	for part in parts:
		part = part.strip()
		if not part or part == "void":
			continue
		callback = re.search(r"\(\s*\*\s*(\w+)\s*\)", part)
		if callback is not None:
			names.append(callback.group(1))
			continue
		identifiers = re.findall(r"[A-Za-z_]\w*", part)
		names.append(identifiers[-1] if identifiers else part)
	return names


def split_top(text):
	parts = []
	depth = 0
	current = []
	for ch in text:
		if ch in "([":
			depth += 1
		elif ch in ")]":
			depth -= 1
		if ch == "," and depth == 0:
			parts.append("".join(current))
			current = []
		else:
			current.append(ch)
	parts.append("".join(current))
	return parts


def parse_doc_sections(text):
	sections = {}
	pattern = re.compile(r"^### `(xrt[A-Z][A-Za-z_0-9]*)`", re.M)
	matches = list(pattern.finditer(text))
	for index, match in enumerate(matches):
		end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
		sections[match.group(1)] = text[match.start() : end]
	return sections


def first_code_block(section):
	match = re.search(r"```c\n(.*?)```", section, re.S)
	return match.group(1) if match else None


def subsection(section, title):
	pattern = re.compile(
		r"^#### " + re.escape(title) + r"\s*\n(.*?)(?=^#### |^### |^## |\Z)", re.S | re.M
	)
	match = pattern.search(section)
	return match.group(1) if match else None


def table_first_column(body):
	names = []
	for line in body.splitlines():
		line = line.strip()
		if not line.startswith("|"):
			continue
		cell = line.split("|")[1].strip() if "|" in line[1:] else ""
		match = re.fullmatch(r"`(\w+|\.\.\.)`", cell)
		if match is not None:
			names.append(match.group(1))
	return names


def example_paths(body):
	return re.findall(r"\]\((\.\./\.\./(examples/[^)]+))\)", body)


def check_doc(doc_path, headers, registered):
	text = doc_path.read_text(encoding="utf-8")
	functions = {}
	for header in headers:
		header_path = ROOT / header
		if header_path.exists():
			functions.update(extract_header_functions(header_path.read_text(encoding="utf-8")))
	sections = parse_doc_sections(text)
	problems = []

	for name, info in sorted(functions.items()):
		section = sections.get(name)
		if section is None:
			problems.append(f"G2 missing-section: {name}")
			continue
		block = first_code_block(section)
		if block is None:
			problems.append(f"G1 no-signature-block: {name}")
		elif normalize(block) != info["decl"]:
			problems.append(f"G1 signature-mismatch: {name}")
		params = subsection(section, "参数")
		if params is None:
			problems.append(f"G2 no-param-section: {name}")
		else:
			documented = table_first_column(params)
			missing = [p for p in info["params"] if p not in documented]
			if missing:
				problems.append(
					f"G2 param-missing: {name} -> {', '.join(missing)}"
				)
		if subsection(section, "返回值") is None:
			problems.append(f"G2 no-return-section: {name}")
		fallible = "bool" in info["ret"] or info["ret"].endswith("*")
		if fallible and subsection(section, "错误") is None:
			problems.append(f"G2 no-error-section: {name}")
		example = subsection(section, "范例")
		if example is None:
			problems.append(f"G2 no-example-section: {name}")
		else:
			paths = example_paths(example)
			if not paths:
				problems.append(f"G2 no-example-link: {name}")
			for _, rel in paths:
				if not (ROOT / rel).exists():
					problems.append(f"G2 example-missing-on-disk: {name} -> {rel}")
				elif rel.replace("\\", "/") not in registered:
					problems.append(f"G4 example-not-registered: {name} -> {rel}")

	orphan = sorted(set(sections) - set(functions))
	for name in orphan:
		problems.append(f"G2 unknown-section: {name}")

	return problems, len(functions)


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	group = parser.add_mutually_exclusive_group(required=True)
	group.add_argument("--doc", help="docs/api 下的文件名，如 array.md")
	group.add_argument("--all", action="store_true")
	args = parser.parse_args()

	doc_headers, registered = load_manifest()
	targets = []
	if args.all:
		for doc in sorted(DOCS_DIR.glob("*.md")):
			if doc.name.endswith("-reference.md"):
				continue
			if "docs/api/" + doc.name in doc_headers:
				targets.append(doc)
	else:
		targets.append(DOCS_DIR / args.doc)

	total_problems = 0
	for doc in targets:
		headers = doc_headers.get("docs/api/" + doc.name, set())
		if not headers:
			print(f"[skip] {doc.name}: manifest 未登记公共头")
			continue
		problems, count = check_doc(doc, headers, registered)
		status = "ok" if not problems else f"{len(problems)} problem(s)"
		print(f"[{status}] {doc.name} ({count} functions)")
		for problem in problems:
			print(f"  - {problem}")
		total_problems += len(problems)

	if total_problems:
		sys.exit(1)


if __name__ == "__main__":
	main()
