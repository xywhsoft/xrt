#!/usr/bin/env python3

"""按 DOC_SPEC 校验文档范例片段的可追溯性（G3）。

`#### 范例` 小节内的每个 ```c 片段（剥离注释与全部空白后）
必须是该节所引范例源文件（同样规范化）的子串——保证片段
永远来自可编译、已注册的真实代码。
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCS_DIR = ROOT / "docs" / "api"


def strip_comments(text):
	text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
	text = re.sub(r"//[^\n]*", " ", text)
	return text


def normalize(text):
	return re.sub(r"\s+", "", strip_comments(text))


def parse_api_sections(text):
	pattern = re.compile(r"^### `([A-Za-z_]\w*)`", re.M)
	matches = list(pattern.finditer(text))
	sections = []
	for index, match in enumerate(matches):
		end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
		sections.append((match.group(1), text[match.start() : end]))
	return sections


def example_subsection(section):
	pattern = re.compile(
		r"^#### 范例\s*\n(.*?)(?=^#### |^### |^## |\Z)", re.S | re.M
	)
	match = pattern.search(section)
	return match.group(1) if match else None


def check_doc(doc_path):
	text = doc_path.read_text(encoding="utf-8")
	problems = []
	checked = 0
	for name, section in parse_api_sections(text):
		example = example_subsection(section)
		if example is None:
			continue
		links = re.findall(r"\]\((\.\./\.\./(examples/[^)]+))\)", example)
		blocks = re.findall(r"```c\n(.*?)```", example, re.S)
		if not blocks:
			continue
		for block in blocks:
			checked += 1
			snippet = normalize(block)
			if not any(
				snippet in normalize((ROOT / rel).read_text(encoding="utf-8"))
				for _, rel in links
				if (ROOT / rel).exists()
			):
				problems.append(
					f"G3 snippet-not-traceable: {name}"
				)
	return problems, checked


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	group = parser.add_mutually_exclusive_group(required=True)
	group.add_argument("--doc", help="docs/api 下的文件名，如 array.md")
	group.add_argument("--all", action="store_true")
	args = parser.parse_args()

	targets = (
		[DOCS_DIR / args.doc]
		if args.doc
		else sorted(p for p in DOCS_DIR.glob("*.md") if not p.name.endswith("-reference.md"))
	)

	total = 0
	for doc in targets:
		if not doc.exists():
			print(f"[error] 不存在: {doc}")
			total += 1
			continue
		problems, checked = check_doc(doc)
		status = "ok" if not problems else f"{len(problems)} problem(s)"
		print(f"[{status}] {doc.name} ({checked} snippet(s))")
		for problem in problems:
			print(f"  - {problem}")
		total += len(problems)

	if total:
		sys.exit(1)


if __name__ == "__main__":
	main()
