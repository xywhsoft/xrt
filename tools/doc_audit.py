#!/usr/bin/env python3
"""文档深度审计套件（DOC_SPEC v1.3 配套）。

汇总文档深化任务期间沉淀的全部机检审计，防止一次性脚本流失。
每个子命令对应一项已闭环的审计维度，全部只读、零误改。

用法：
  python tools/doc_audit.py --all            # 运行全部审计
  python tools/doc_audit.py --only ghost,tables  # 运行指定审计
  python tools/doc_audit.py --list           # 列出审计项

审计项（均以公共头全集 = include/xrt/*.h + single/xrt.h 为唯一真值源）：
  ghost-table   表格单元中的幽灵标识符（v1）
  ghost-prose   散文/错误清单中的幽灵标识符（v2，豁免「旧版/删除」历史段）
  ghost-code    非范例代码块中的幽灵标识符（v3，排除 G3 覆盖的 #### 范例）
  ghost-type    字段表「类型」列的幽灵类型（含函数指针 typedef 识别）
  tables        表格结构：列断裂 / 空表 / 表头分隔列数不匹配
  enum-values   枚举节逐值完备（签名块每个值在值表中有行）
  struct-fields 结构节字段完备（签名块每个字段在字段表中有行）
  headings      标题健康：重复 ### 节 / 层级跳级
  snippets      函数节范例片段 ≤12 行
  banned-words  函数节/类型节禁用词（大概/之类的/等等）
  const-values  常量总表值列与头文件 #define 逐行一致
"""
from __future__ import annotations

import argparse
import glob
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, "docs", "api")
TRUTH_HEADERS = sorted(glob.glob(os.path.join(ROOT, "include", "xrt", "*.h"))) + [
	os.path.join(ROOT, "single", "xrt.h"),
]

STD_TYPES = {
	"void", "char", "int", "long", "short", "unsigned", "signed", "float",
	"double", "bool", "size_t", "ssize_t", "ptrdiff_t", "int8_t", "int16_t",
	"int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
	"int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",
	"uintptr_t", "intptr_t", "va_list", "FILE", "ptr", "cstr", "str", "bytes",
	"cbytes", "xtime", "xdeadline", "struct", "回调",
}

SECTION_SPLIT = re.compile(r"(?m)^(?=### |## )")


def load_docs():
	for path in sorted(glob.glob(os.path.join(DOCS, "*.md"))):
		if path.endswith("-reference.md"):
			continue
		yield os.path.basename(path), io.open(path, encoding="utf-8").read()


def truth_ids():
	text = ""
	for h in TRUTH_HEADERS:
		with io.open(h, encoding="utf-8", errors="ignore") as fp:
			text += fp.read()
	return set(re.findall(r"\b((?:X|XRT)[A-Z0-9_]{2,})\b", text))


def truth_typedefs():
	text = ""
	for h in TRUTH_HEADERS:
		with io.open(h, encoding="utf-8", errors="ignore") as fp:
			text += fp.read()
	tds = set(re.findall(r"\btypedef\b[^;{}]*?\b(\w+)\s*;", text))
	tds |= set(re.findall(r"\}\s*(\w+)\s*;", text))
	tds |= set(re.findall(r"\btypedef\b[^;]*?\(\s*\*\s*(\w+)\s*\)", text))
	return tds


def sections(text):
	return SECTION_SPLIT.split(text)


def audit_ghost_table():
	ids = truth_ids()
	bad = []
	for name, text in load_docs():
		for ln in text.split("\n"):
			if ln.startswith("|"):
				for m in re.findall(r"`((?:X|XRT)[A-Z0-9_]{2,})`", ln):
					if m not in ids:
						bad.append(f"{name}: 表格幽灵 {m}")
	return bad


def audit_ghost_prose():
	ids = truth_ids()
	bad = []
	for name, text in load_docs():
		in_code = False
		for ln in text.split("\n"):
			if ln.startswith("```"):
				in_code = not in_code
				continue
			if in_code or ln.startswith("|"):
				continue
			if "旧版" in ln or "删除" in ln:
				continue  # 历史决策段的合法引用
			for m in re.findall(r"`((?:X|XRT)[A-Z0-9_]{2,})`", ln):
				if m not in ids:
					bad.append(f"{name}: 散文幽灵 {m}")
	return bad


def audit_ghost_code():
	ids = truth_ids()
	bad = []
	for name, text in load_docs():
		lines = text.split("\n")
		in_code = in_example = False
		for ln in lines:
			if ln.startswith("#### 范例"):
				in_example = True
				continue
			if re.match(r"^#{3,4} ", ln):
				in_example = False
				continue
			if ln.startswith("```c") or (in_code and ln == "```"):
				in_code = not in_code
				continue
			if in_code and not in_example:
				for m in re.findall(r"\b((?:X|XRT)[A-Z0-9_]{2,})\b", ln):
					if m not in ids:
						bad.append(f"{name}: 代码块幽灵 {m}")
	return bad


def audit_ghost_type():
	tds = truth_typedefs()
	bad = []
	for name, text in load_docs():
		in_ft = False
		for ln in text.split("\n"):
			if ln.startswith("| 字段 | 类型 |"):
				in_ft = True
				continue
			if in_ft and ln.startswith("|"):
				m = re.match(r"^\| `[^`]+` \| `([^`]+)` \|", ln)
				if m:
					t = m.group(1).replace("const ", "").strip()
					rm = re.match(r"(?:struct\s+)?(\w+)", t)
					root = rm.group(1) if rm else t
					if root not in tds and root not in STD_TYPES:
						bad.append(f"{name}: 字段类型幽灵 {m.group(1)}")
			elif not ln.startswith("|"):
				in_ft = False
	return bad


def audit_tables():
	bad = []
	for name, text in load_docs():
		lines = text.split("\n")
		header_cols = None
		in_table = False
		for i, ln in enumerate(lines):
			s = ln.strip()
			if s.startswith("|") and s.endswith("|"):
				cols = s.count("|") - 1
				if not in_table:
					if re.match(r"^\|[-\s|]+\|$", s):
						continue
					in_table, header_cols = True, cols
				elif cols != header_cols:
					bad.append(f"{name}:{i + 1}: 列断裂 {header_cols}->{cols}")
			else:
				in_table = False
		# 空表（表头+分隔后无数据行）
		for i in range(len(lines) - 2):
			h, s, nxt = lines[i], lines[i + 1], lines[i + 2]
			if h in ("| 字段 | 类型 | 语义 |", "| 参数 | 方向 | 约束 | 说明 |",
					"| 值 | 语义 |", "| 返回 | 含义 | 失败时状态 |",
					"| 常量 | 值 | 语义 |") and re.match(r"^\|[-\s|]+\|$", s) \
					and not nxt.startswith("|"):
				bad.append(f"{name}:{i + 1}: 空表 {h}")
		# 表头/分隔列数不匹配
		for i in range(len(lines) - 1):
			h, s = lines[i], lines[i + 1]
			if h.startswith("|") and s.startswith("|") and \
					re.match(r"^\|[-\s|]+\|$", s):
				if h.count("|") != s.count("|"):
					bad.append(f"{name}:{i + 2}: 分隔列数 {s.count('|') - 1} "
						f"≠ 表头 {h.count('|') - 1}")
	return bad


def audit_enum_values():
	bad = []
	for name, text in load_docs():
		for sec in sections(text):
			m = re.match(r"### `(\w+)`", sec)
			if not m or m.group(1).startswith("xrt"):
				continue
			cb = re.search(r"```c\n(.*?)\n```", sec, re.S)
			if not cb or "typedef enum" not in cb.group(1):
				continue
			vals = re.findall(r"^\t((?:X|XRT)[A-Z0-9_]+)\b", cb.group(1), re.M)
			rows = set(re.findall(r"^\| `((?:X|XRT)[A-Z0-9_]+)` \|", sec, re.M))
			for v in vals:
				if v not in rows:
					bad.append(f"{name}: {m.group(1)} 缺枚举行 {v}")
	return bad


def audit_struct_fields():
	bad = []
	for name, text in load_docs():
		for sec in sections(text):
			m = re.match(r"### `(\w+)`", sec)
			if not m or m.group(1).startswith("xrt"):
				continue
			cb = re.search(r"```c\n(.*?)\n```", sec, re.S)
			if not cb or "typedef struct" not in cb.group(1) \
					or "{" not in cb.group(1):
				continue
			body = cb.group(1).split("{", 1)[1].rsplit("}", 1)[0]
			fields = re.findall(r"^\t\w[\w\s\*]*?\s\**\w+;", body, re.M)
			rows = set(re.findall(r"^\| `(\w+)` \| `", sec, re.M))
			for f in fields:
				fm = re.search(r"(\w+);$", f)
				if fm and fm.group(1) not in rows:
					bad.append(f"{name}: {m.group(1)} 缺字段行 {fm.group(1)}")
	return bad


def audit_headings():
	from collections import Counter
	bad = []
	for name, text in load_docs():
		heads = [m.group(1) for m in
			re.finditer(r"^### `([^`]+)`\s*$", text, re.M)]
		for h, n in Counter(heads).items():
			if n > 1:
				bad.append(f"{name}: 重复节 {h}×{n}")
		prev = None
		for ln in text.split("\n"):
			m = re.match(r"^(#{2,5}) ", ln)
			if m:
				lvl = len(m.group(1))
				if prev and lvl > prev + 1:
					bad.append(f"{name}: 层级跳级 {prev}->{lvl}")
				prev = lvl
	return bad


def audit_snippets():
	bad = []
	for name, text in load_docs():
		for sec in sections(text):
			m = re.match(r"### `(xrt\w+)`", sec)
			if not m:
				continue
			ex = re.search(r"#### 范例\n\n.*?```c\n(.*?)```", sec, re.S)
			if ex and ex.group(1).count("\n") >= 12:
				bad.append(f"{name}: {m.group(1)} 片段 "
					f"{ex.group(1).count(chr(10)) + 1} 行 >12")
	return bad


def audit_banned_words():
	# 「之类的」在「不保证 X 之类的 Y」引述句中是准确行为声明，
	# 豁免「不保证」前缀行；其余命中按节报。
	bad = []
	for name, text in load_docs():
		for sec in sections(text):
			head = re.match(r"### `([^`]+)`", sec)
			tag = head.group(1) if head else sec[:20]
			for ln in sec.split("\n"):
				if "不保证" in ln or "不承诺" in ln:
					continue
				if re.search(r"大概|之类的|等等", ln):
					bad.append(f"{name}: {tag} 禁用词")
	return bad


def audit_const_values():
	bad = []
	hdr_text = ""
	for h in TRUTH_HEADERS:
		with io.open(h, encoding="utf-8", errors="ignore") as fp:
			hdr_text += fp.read()

	def norm(s):
		return re.sub(r"\s+", " ", s.strip())

	for name, text in load_docs():
		in_const = False
		for ln in text.split("\n"):
			if ln.startswith("| 常量 | 值 | 语义 |"):
				in_const = True
				continue
			if in_const and ln.startswith("|"):
				m = re.match(r"^\| `((?:X|XRT)[A-Z0-9_]+)` \| `([^`]+)` \|", ln)
				if m:
					nm, val = m.group(1), m.group(2)
					hm = re.search(r"#define\s+" + re.escape(nm)
						+ r"\s+([^\n]+)", hdr_text)
					if hm and norm(hm.group(1)) != norm(val):
						bad.append(f"{name}: {nm} doc={val} hdr={hm.group(1).strip()}")
			elif not ln.startswith("|"):
				in_const = False
	return bad


def audit_bool_rows():
	import check_api_reference_detail as C
	dh, _ = C.load_manifest()
	bad = []
	for name, text in load_docs():
		hdrs = dh.get("docs/api/" + name)
		if not hdrs:
			continue
		funcs = {}
		for h in hdrs:
			hp = C.ROOT / h
			if hp.exists():
				funcs.update(C.extract_header_functions(
					hp.read_text(encoding="utf-8")))
		for sec in sections(text):
			m = re.match(r"### `(xrt\w+)`", sec)
			if not m or m.group(1) not in funcs:
				continue
			if funcs[m.group(1)]["ret"].strip() != "bool":
				continue
			rsec = re.search(r"#### 返回值\n\n(.*?)(?=\n#### |\Z)", sec, re.S)
			if not rsec:
				continue
			body = rsec.group(1)
			if not (re.search(r"^\| `?true`? \|", body, re.M)
					and re.search(r"^\| `?false`? \|", body, re.M)):
				bad.append(f"{name}: {m.group(1)} bool 返回缺两行")
	return bad


AUDITS = {
	"ghost-table": audit_ghost_table,
	"ghost-prose": audit_ghost_prose,
	"ghost-code": audit_ghost_code,
	"ghost-type": audit_ghost_type,
	"tables": audit_tables,
	"enum-values": audit_enum_values,
	"struct-fields": audit_struct_fields,
	"headings": audit_headings,
	"snippets": audit_snippets,
	"banned-words": audit_banned_words,
	"const-values": audit_const_values,
	"bool-rows": audit_bool_rows,
}


def main():
	ap = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("--all", action="store_true", help="运行全部审计")
	ap.add_argument("--only", help="逗号分隔的审计项列表")
	ap.add_argument("--list", action="store_true", help="列出审计项")
	args = ap.parse_args()

	if args.list or not (args.all or args.only):
		for k, v in AUDITS.items():
			doc = (v.__doc__ or "").strip().split("\n")[0]
			print(f"{k:16s} {doc}")
		return 0

	keys = list(AUDITS) if args.all else [k.strip() for k in args.only.split(",")]
	total = 0
	for k in keys:
		if k not in AUDITS:
			print(f"[unknown] {k}")
			continue
		findings = AUDITS[k]()
		status = "ok" if not findings else f"{len(findings)} finding(s)"
		print(f"[{status}] {k}")
		for f in findings[:20]:
			print(f"  - {f}")
		if len(findings) > 20:
			print(f"  ... 及其余 {len(findings) - 20} 项")
		total += len(findings)
	print(f"-- total findings: {total}")
	return 1 if total else 0


if __name__ == "__main__":
	sys.exit(main())
