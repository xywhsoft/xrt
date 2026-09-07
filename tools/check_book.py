#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""教程质量门禁 + 漂移检测（依据 docs/book/BOOK_SPEC.md）。

用法:
  python tools/check_book.py check [--chapter 3]     # 硬门禁，FAIL 退出码 1
  python tools/check_book.py report                  # 全书度量表 + 漂移标红
"""

import argparse
import glob
import io
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_web_book import Chapter, chapter_meta   # noqa: E402
from gen_web_api import scan_all_headers         # noqa: E402

# 类型 -> 目标（讲解字数, 完整程序, 图示, 避坑）
TARGETS = {
    "practice":     (4000, 2, 1, 2),
    "composition":  (6000, 2, 1, 2),
    "concept":      (6000, 2, 1, 2),
    "project":      (8000, 2, 1, 2),
    "intro":        (2000, 0, 0, 0),
}
FULL_SECTIONS = ["导读", "引入", "概念", "示例", "契约", "避坑", "练习", "速查"]

IDENT_RE = re.compile(r"\b[A-Za-z_]\w*\b")
URL_RE = re.compile(r"\]\(([^)]+)\)|https?://\S+|[\w./-]+\.(?:h|c|md|json)")


def fmt(n):
    return format(n, ",")


def iter_sources(repo):
    for f in sorted(glob.glob(os.path.join(repo, "docs", "book", "*.md"))):
        base = os.path.basename(f)[:-3]
        m = re.match(r"^(\d+)-(.+)$", base)
        if m:
            yield int(m.group(1)), f


# ---------------------------------------------------------------------------
# 度量
# ---------------------------------------------------------------------------

def metrics(ch, repo):
    acc = {"prose": [], "programs": 0, "terms": 0, "diagrams": 0,
           "pits": 0, "pit_pairs": 0, "exercises": set(), "sub_no_term": 0}

    def add_prose(text):
        acc["prose"].append(re.sub(r"[#*`|>\-]", "", text))

    def fence_kind(f):
        if f.kind == "embed":
            acc["programs"] += 1
            return "prog"
        if f.kind == "c" and not f.info and "int main(" in "\n".join(f.text):
            acc["programs"] += 1
            return "prog"
        if f.kind == "term":
            acc["terms"] += 1
            return "term"
        if f.kind == "diagram":
            acc["diagrams"] += 1
        return None

    def container(blocks, key, top=False):
        has_prog = has_term = False
        for it in blocks:
            if hasattr(it, "blocks"):  # SubSec
                if key == "避坑":
                    acc["pits"] += 1
                    kinds = [b.data.info for b in it.blocks
                             if b.kind == "fence" and b.data.kind == "c"]
                    if "bad" in kinds and "good" in kinds:
                        acc["pit_pairs"] += 1
                if key == "练习":
                    for lv in ("基础", "进阶", "挑战"):
                        if lv in it.title:
                            acc["exercises"].add(lv)
                add_prose(it.title)
                hp, ht = container(it.blocks, key)
                if hp and not ht and key == "示例":
                    acc["sub_no_term"] += 1
                has_prog |= hp
                has_term |= ht
            elif it.kind == "p":
                add_prose(" ".join(it.data))
            elif it.kind == "ul":
                for r in it.data:
                    add_prose(r)
            elif it.kind == "tbl":
                for r in it.data:
                    add_prose(r)
            elif it.kind == "fence":
                r = fence_kind(it.data)
                if r == "prog":
                    has_prog = True
                elif r == "term":
                    has_term = True
        return has_prog, has_term

    for key, blocks in ch.sections.items():
        hp, ht = container(blocks, key, top=True)
        if hp and not ht and key == "示例" and not any(
                hasattr(b, "blocks") for b in blocks):
            acc["sub_no_term"] += 1

    return {
        "prose": sum(len(p) for p in acc["prose"]),
        "programs": acc["programs"],
        "terms": acc["terms"],
        "diagrams": acc["diagrams"],
        "pits": acc["pits"],
        "pit_pairs": acc["pit_pairs"],
        "exercises": len(acc["exercises"]),
        "prog_without_term": acc["sub_no_term"],
    }


# ---------------------------------------------------------------------------
# 校验
# ---------------------------------------------------------------------------

def validate(ch, path, repo, sym_map, order, wwwroot, allow=()):
    fails = []
    warns = []
    fm = chapter_meta(ch)  # 缺字段直接抛
    typ = fm["type"]
    if typ not in TARGETS:
        fails.append("type=%s 未定义门禁" % typ)
        typ = "practice"

    # 1. 骨架
    if typ != "intro":
        missing = [s for s in FULL_SECTIONS if s not in ch.sections]
        if missing:
            fails.append("缺少规范节: %s" % ",".join(missing))

    # 2. 深度
    m = metrics(ch, repo)
    t_prose, t_prog, t_dg, t_pit = TARGETS[typ]
    if m["prose"] < t_prose:
        fails.append("讲解字数 %s < %s" % (fmt(m["prose"]), fmt(t_prose)))
    if m["programs"] < t_prog:
        fails.append("完整程序 %d < %d" % (m["programs"], t_prog))
    if m["terms"] < t_prog:
        fails.append("term 输出块 %d < %d" % (m["terms"], t_prog))
    if m["prog_without_term"]:
        fails.append("有 %d 个完整程序所在小节缺 term 输出块" % m["prog_without_term"])
    if m["diagrams"] < t_dg:
        fails.append("图示 %d < %d" % (m["diagrams"], t_dg))
    if m["pits"] < t_pit:
        fails.append("避坑 %d < %d" % (m["pits"], t_pit))
    if m["pit_pairs"] < t_pit:
        fails.append("含 bad/good 对照的坑 %d < %d" % (m["pit_pairs"], t_pit))
    if typ != "intro" and m["exercises"] < 3:
        fails.append("练习三级缺: %s" % ("基础,进阶,挑战" if not m["exercises"] else "不足三级"))

    # 3. api 列表
    for name in [x.strip() for x in fm["api"].split(",") if x.strip()]:
        if not os.path.exists(os.path.join(wwwroot, "book", "ref-%s.html" % name)):
            fails.append("api 页 ref-%s.html 不存在" % name)

    # 4. embed 路径 + term 与示例预期输出比对
    slugs = {it["slug"]: it for it in order}
    for key, blocks in ch.sections.items():
        for it in blocks:
            items = it.blocks if hasattr(it, "blocks") else [it] if it.kind == "fence" else []
            prog_out = None
            terms_here = []
            for b in items:
                if b.kind != "fence":
                    continue
                f = b.data
                if f.kind == "embed":
                    opts = dict(re.findall(r'(\w+)\s*=\s*"([^"]*)"', f.info))
                    p = opts.get("path", "")
                    full = os.path.join(repo, p.replace("/", os.sep))
                    if not os.path.exists(full):
                        fails.append("embed 路径不存在: %s" % p)
                        continue
                    src = io.open(full, encoding="utf-8").read().replace("\r\n", "\n").split("\n")
                    if "lines" in opts:
                        a, bb = (int(x) for x in opts["lines"].split("-"))
                        if a < 1 or bb > len(src):
                            fails.append("embed lines %s 超出 %s 行数 %d" % (opts["lines"], p, len(src)))
                    prog_out = expected_output(src)
                elif f.kind == "term":
                    terms_here.append("\n".join(f.text))
            if prog_out and terms_here:
                want = [normalize_out(x) for x in prog_out if x.strip()]
                got = [normalize_out(x) for x in terms_here[0].split("\n")
                       if x.strip() and not x.strip().startswith("$")]
                if want != got:
                    fails.append("term 输出与示例预期输出不一致:\n    期望 %r\n    实际 %r" % (want, got))

    # 5. 幻觉标识符
    bad_ids = set()
    for key, blocks in ch.sections.items():
        texts = []
        for it in blocks:
            if hasattr(it, "blocks"):
                texts.append(it.title)
                for b in it.blocks:
                    if b.kind == "p":
                        texts.append(" ".join(b.data))
                    elif b.kind == "ul":
                        texts.extend(b.data)
                    elif b.kind == "tbl":
                        texts.extend(b.data)
                    elif b.kind == "fence" and b.data.kind in ("c", "diagram"):
                        texts.append("\n".join(b.data.text))
            else:
                if it.kind == "p":
                    texts.append(" ".join(it.data))
                elif it.kind == "ul":
                    texts.extend(it.data)
                elif it.kind == "tbl":
                    texts.extend(it.data)
                elif it.kind == "fence" and it.data.kind in ("c", "diagram"):
                    texts.append("\n".join(it.data.text))
        for t in texts:
            t = URL_RE.sub(" ", t)
            for tok in IDENT_RE.findall(t):
                if tok in BRAND_TOKENS or tok in allow:
                    continue
                if re.match(r"^xrt[A-Z]", tok) or re.match(r"^X[A-Z0-9_]{2,}$", tok) \
                        or re.match(r"^x[a-z]\w{2,}$", tok):
                    if tok not in sym_map:
                        bad_ids.add(tok)
    if bad_ids:
        fails.append("符号表中不存在的标识符: %s" % ", ".join(sorted(bad_ids)))

    # 6. 跨章引用
    raw = io.open(path, encoding="utf-8").read()
    for url in re.findall(r"\]\((ch[\w-]+\.md)\)", raw):
        slug = re.sub(r"^ch\d+-", "", url[:-3])
        if slug not in slugs:
            fails.append("跨章引用目标不存在: %s" % url)

    return fails, warns, m


def expected_output(src_lines):
    """从示例头部注释提取“预期输出”块。"""
    out = []
    on = False
    for l in src_lines[:40]:
        s = l.strip()
        body = s.lstrip("/*").rstrip("*/").strip()
        if "预期输出" in body:
            on = True
            continue
        if not on:
            continue
        if not body:
            break
        if body.startswith("*"):
            body = body.lstrip("*").strip()
        if not body or re.match(r"^[（(【]|模块宏|编译|演示 API|范例", body):
            break
        out.append(body)
    return out


def normalize_out(s):
    s = s.strip()
    s = re.sub(r"\d+", "N", s)
    s = re.sub(r"N+", "N", s)   # 示例注释里的 NNNNN 占位与真实端口归一
    return s


BRAND_TOKENS = {"XRT", "XSON", "xson", "JSON", "HTTP", "TLS", "SSE", "DNS", "API", "CMake",
                "xhttp", "xws", "xmail", "xssh", "xruntime",
                "XID", "xrtMath"}  # 产品/协议/格式名与数学函数族前缀，非单个 API

# 用户侧集成宏：XRT_IMPLEMENTATION 与被 features.h 消费的 XRT_MODULE_* 选择宏
# （库里只 #if defined(...) 消费、不定义它们，故不在声明符号表中）
USER_MACROS = {"XRT_IMPLEMENTATION", "XRT_MODULE_", "XRT_FEATURE_", "XRT_EXCLUDE_"}


def collect_user_macros(repo):
    import io as _io
    src = _io.open(os.path.join(repo, "include", "xrt", "features.h"),
                   encoding="utf-8", errors="replace").read()
    out = set()
    out |= set(re.findall(r"\bXRT_MODULE_[A-Z0-9_]+", src))
    out |= set(re.findall(r"\bXRT_FEATURE_[A-Z0-9_]+", src))    # 依赖闭包生成
    out |= set(re.findall(r"\bXRT_EXCLUDE_[A-Z0-9_]+", src))    # 排除宏
    return out


# ---------------------------------------------------------------------------
# 命令
# ---------------------------------------------------------------------------

def cmd_check(args, repo, wwwroot):
    order = json.load(io.open(os.path.join(repo, "docs", "book", "order.json"), encoding="utf-8"))
    sym_map, _, _ = scan_all_headers(repo)
    allow = USER_MACROS | collect_user_macros(repo)
    any_fail = False
    rows = []
    for num, path in iter_sources(repo):
        if args.chapter and num != args.chapter:
            continue
        try:
            ch = Chapter(path)
            chapter_meta(ch)
            fails, warns, m = validate(ch, path, repo, sym_map, order, wwwroot, allow)
        except ValueError as ex:
            fails, warns, m = [str(ex)], [], {"prose": 0, "programs": 0, "terms": 0, "diagrams": 0,
                                              "pits": 0, "pit_pairs": 0, "exercises": 0, "prog_without_term": 0}
        tag = "PASS" if not fails else "FAIL"
        any_fail |= bool(fails)
        rows.append((num, os.path.basename(path), tag, m, fails, warns))
    for num, name, tag, m, fails, warns in sorted(rows):
        print("[%s] %3d %-22s 字数%s 程序%d 图示%d 坑%d(%d对照) 练习%d"
              % (tag, num, name, fmt(m["prose"]), m["programs"], m["diagrams"], m["pits"], m["pit_pairs"], m["exercises"]))
        for f in fails:
            print("    ✗ " + f)
        for w in warns:
            print("    ⚠ " + w)
    return 1 if any_fail else 0


def cmd_report(args, repo, wwwroot):
    order = json.load(io.open(os.path.join(repo, "docs", "book", "order.json"), encoding="utf-8"))
    print("%-4s %-26s %-11s %8s %6s %5s %5s %6s" % ("章", "文件", "类型", "讲解字数", "程序", "图示", "坑", "练习"))
    low_streak = 0
    red = False
    for num, path in iter_sources(repo):
        try:
            ch = Chapter(path)
            typ = ch.fm.get("type", "practice")
            m = metrics(ch, repo)
            target = TARGETS.get(typ, TARGETS["practice"])[0]
            pct = m["prose"] * 100 // target if target else 100
            flag = ""
            if pct < 80:
                low_streak += 1
                flag = " ◀ %d%%" % pct
                if low_streak >= 3:
                    red = True
            else:
                low_streak = 0
            print("%-4d %-26s %-11s %8s %6d %5d %5d %6d%s"
                  % (num, os.path.basename(path), typ, fmt(m["prose"]), m["programs"],
                     m["diagrams"], m["pits"], m["exercises"], flag))
        except ValueError as ex:
            print("%-4d %-26s 解析失败: %s" % (num, os.path.basename(path), ex))
            low_streak += 1
    if red:
        print("★ 漂移标红：连续 3 章低于类型目标 80%，停止推进并回金标准校准")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default="D:/GIT/xrt")
    ap.add_argument("--wwwroot", default="D:/GIT/home/host/xrt/wwwroot")
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("check")
    c.add_argument("--chapter", type=int)
    sub.add_parser("report")
    args = ap.parse_args()
    args.repo = args.repo.replace("\\", "/")
    args.wwwroot = args.wwwroot.replace("\\", "/")
    sys.exit(cmd_check(args, args.repo, args.wwwroot) if args.cmd == "check"
             else cmd_report(args, args.repo, args.wwwroot))


if __name__ == "__main__":
    main()
