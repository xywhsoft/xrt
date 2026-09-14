#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""全书章节重编号（新增/插入章节后的一次性机械操作）。

order.json 是事实来源：slug 是章节身份，num 是显示位置。
对 order.json 中 num 与磁盘文件不一致的章节执行：
  1. 重命名 wwwroot/book/chOLD-slug.html -> chNEW-slug.html（两阶段防覆盖）
  2. 修补旧页面内的编号（title/h1/ch-progress/canonical/og:url）与 prev/next
  3. 全站 HTML 与 docs/book/*.md 中的 chOLD-slug 引用替换
  4. 重命名 docs/book/NN-slug.md 并同步 front matter num
之后应立即运行 gen_web_book.py build 重建 md 源章节。

用法:
  python tools/renumber_book.py --dry-run   # 预览将发生的重命名
  python tools/renumber_book.py             # 执行
"""

import argparse
import glob
import io
import json
import os
import re

WWW_CH = "book"


def fmt_num(n):
    return "%d" % n if n >= 100 else "%02d" % n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default="D:/GIT/xrt")
    ap.add_argument("--wwwroot", default="D:/GIT/home/host/xrt/wwwroot")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--sweep-refs", type=int, metavar="N",
                    help="独立子步骤：散文编号引用 +1（端点 >= N），不动文件名")
    args = ap.parse_args()
    repo = args.repo.replace("\\", "/")
    www = args.wwwroot.replace("\\", "/")

    if args.sweep_refs is not None:
        sweep_chapter_refs(repo, args.sweep_refs, args.dry_run)
        return

    order_path = os.path.join(repo, "docs", "book", "order.json")
    order = json.load(io.open(order_path, encoding="utf-8"))
    nums = [it["num"] for it in order]
    assert nums == sorted(nums) and len(set(nums)) == len(nums), "order.json num 必须唯一且升序"

    # 磁盘现状: slug -> 旧文件名
    disk = {}
    for f in glob.glob(os.path.join(www, WWW_CH, "ch*.html")):
        base = os.path.basename(f)[:-5]
        m = re.match(r"ch(\d+)-(.+)", base)
        if m:
            disk[m.group(2)] = base

    renames = []          # (slug, old_base, new_base, new_entry)
    for it in order:
        new_base = "ch%s-%s" % (fmt_num(it["num"]), it["slug"])
        old = disk.get(it["slug"])
        if old is None:
            continue  # 新章，尚无文件
        if old != new_base:
            renames.append((it["slug"], old, new_base, it))

    total = len(order)
    print("order: %d 章 | 需重命名 %d 个文件" % (total, len(renames)))
    for slug, old, new, _ in renames:
        print("  %s -> %s" % (old, new))
    if args.dry_run:
        return

    chdir = os.path.join(www, WWW_CH)
    # 1) 两阶段重命名
    for slug, old, new, _ in renames:
        os.rename(os.path.join(chdir, old + ".html"), os.path.join(chdir, "chtmp-" + slug + ".html"))
    for slug, old, new, _ in renames:
        os.rename(os.path.join(chdir, "chtmp-" + slug + ".html"), os.path.join(chdir, new + ".html"))

    by_slug = {it["slug"]: it for it in order}

    # 2) 修补被重命名的旧页面
    for slug, old, new, entry in renames:
        path = os.path.join(chdir, new + ".html")
        txt = io.open(path, encoding="utf-8").read()
        old_num = int(re.match(r"ch(\d+)-", old).group(1))
        n = entry["num"]
        # 标题与进度中的章号
        txt = txt.replace("第 %d 章 " % old_num, "第 %d 章 " % n)
        txt = txt.replace("第%d章 " % old_num, "第%d章 " % n)
        txt = re.sub(r'(ch-progress">第 )\d+( 章 / 共 )\d+( 章)',
                     r"\g<1>%d\g<2>%d\g<3>" % (n, total), txt)
        # 本页文件名（canonical / og:url）
        txt = txt.replace(old + ".html", new + ".html")
        # prev/next
        idx = next(i for i, it in enumerate(order) if it["slug"] == slug)
        for side, pos in (("prev", idx - 1), ("next", idx + 1)):
            if 0 <= pos < len(order):
                nb = order[pos]
                nb_base = "ch%s-%s" % (fmt_num(nb["num"]), nb["slug"])
                blk_re = re.compile(
                    r'(<a href=")[\w-]+(\.html" class="%s"><span class="pn-dir">%s章</span>'
                    r'<span class="pn-title">第 )\d+( 章 )[^<]*(</span></a>)' % (side, "上一" if side == "prev" else "下一"))
                txt = blk_re.sub(r"\g<1>%s\g<2>%d\g<3>%s\g<4>" % (nb_base, nb["num"], nb["title"]), txt)
        io.open(path, "w", encoding="utf-8", newline="\n").write(txt)

    # 3) 全站引用替换（先长后短避免前缀碰撞）
    pairs = sorted([(old, new) for _s, old, new, _e in renames],
                   key=lambda t: -len(t[0]))
    targets = glob.glob(os.path.join(www, "*.html")) + glob.glob(os.path.join(www, WWW_CH, "*.html"))
    for f in targets:
        txt = io.open(f, encoding="utf-8").read()
        orig = txt
        for old, new in pairs:
            if old in txt:
                txt = txt.replace(old, new)
        if txt != orig:
            io.open(f, "w", encoding="utf-8", newline="\n").write(txt)

    # 4) md 源重命名 + front matter num（按旧文件名精确匹配，避免后缀碰撞）
    for slug, old, new, entry in renames:
        old_md = os.path.join(repo, "docs", "book",
                              "%s-%s.md" % (("%d" % int(re.match(r"ch(\d+)-", old).group(1)))
                                            if int(re.match(r"ch(\d+)-", old).group(1)) >= 100
                                            else "%02d" % int(re.match(r"ch(\d+)-", old).group(1)), slug))
        if os.path.exists(old_md):
            txt = io.open(old_md, encoding="utf-8").read()
            txt = re.sub(r"(?m)^num:\s*\d+", "num: %d" % entry["num"], txt)
            dst = os.path.join(os.path.dirname(old_md), "%s-%s.md" % (fmt_num(entry["num"]), slug))
            io.open(dst, "w", encoding="utf-8", newline="\n").write(txt)
            os.remove(old_md)
            print("md: %s -> %s" % (os.path.basename(old_md), os.path.basename(dst)))

    # 4b) en/ru 译文源同步重命名 + front matter num
    for lang in ("en", "ru"):
        for slug, old, new, entry in renames:
            old_md = os.path.join(repo, "docs", "book", lang,
                                  "%s-%s.md" % (old[2:old.index("-", 2)], slug))
            if not os.path.exists(old_md):
                continue
            txt = io.open(old_md, encoding="utf-8").read()
            txt = re.sub(r"(?m)^num:\s*\d+", "num: %d" % entry["num"], txt)
            dst = os.path.join(os.path.dirname(old_md), "%s-%s.md" % (fmt_num(entry["num"]), slug))
            io.open(dst, "w", encoding="utf-8", newline="\n").write(txt)
            os.remove(old_md)
            print("md[%s]: %s -> %s" % (lang, os.path.basename(old_md), os.path.basename(dst)))

    print("完成：重命名 %d 文件 + 全站引用替换" % len(renames))


# ---------------------------------------------------------------------------
# 散文编号引用扫描：第 N 章 / 第 N-M 章 / 第 N/M 章 / （N-M 章）
#                    Chapter(s) N(-M) / глава N(–M) —— 端点 >= 阈值一律 +1
# ---------------------------------------------------------------------------

REF_PATS = [
    # zh：单个、范围（~-—–）、多值（/ 分隔），如 “第 34 章”“第 103-104 章”“第 63/66 章”
    (re.compile(r"(第\s*)((?:\d+\s*[/~～—–-]\s*)*\d+)(\s*章)"), True),
    # zh 卷区间：“（25-35 章：…）”
    (re.compile(r"(（\s*)(\d+\s*[/~～—–-]\s*\d+)(\s*章)"), False),
    # en：Chapter 34 / Chapters 103-104
    (re.compile(r"([Cc]hapters?\s+)((?:\d+\s*[/~～—–-]\s*)*\d+)"), False),
    # ru：глава 30 / главы 103-104
    (re.compile(r"([Гг]лав[аы]*\s+)((?:\d+\s*[/~～—–-]\s*)*\d+)"), False),
]


def _shift_nums(s, threshold):
    def one(m):
        return str(int(m.group(0)) + 1) if int(m.group(0)) >= threshold else m.group(0)
    return re.sub(r"\d+", one, s)


def sweep_chapter_refs(repo, threshold, dry_run=False):
    """对章节 md（zh/en/ru，不含 PROGRESS/PHASES/order 等登记文件）做编号引用 +1。"""
    total = 0
    rows = []
    for sub in ("", "en", "ru"):
        base = os.path.join(repo, "docs", "book", sub)
        for f in sorted(os.listdir(base)) if os.path.isdir(base) else []:
            if not re.match(r"\d{2,3}-[\w-]+\.md$", f):
                continue
            path = os.path.join(base, f)
            txt = io.open(path, encoding="utf-8").read()
            orig, n = txt, 0
            for pat, _zh in REF_PATS:
                txt = pat.sub(lambda m: m.group(1) + _shift_nums(m.group(2), threshold) + m.group(3)
                              if m.lastindex and m.lastindex >= 3 else
                              m.group(1) + _shift_nums(m.group(2), threshold), txt)
            n = sum(a != b for a, b in zip(orig.split("\n"), txt.split("\n")))
            if txt != orig:
                total += 1
                rows.append("%s%s: %d 行" % (sub + "/" if sub else "", f, n))
                if not dry_run:
                    io.open(path, "w", encoding="utf-8", newline="\n").write(txt)
    print("散文引用改号: %d 个文件%s" % (total, "（dry-run）" if dry_run else ""))
    for r in rows[:40]:
        print("  " + r)
    if len(rows) > 40:
        print("  ... 共 %d 文件" % len(rows))
    return total


if __name__ == "__main__":
    main()
