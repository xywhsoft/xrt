#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""官网多语言译文质量门禁（I18N_SPEC v1.0 G1-G8）。

用法:
  python tools/check_i18n.py check                # 全量门禁（en+ru 已存在的译文）
  python tools/check_i18n.py check en             # 只检查英语
  python tools/check_i18n.py check en 03-first    # 单章自检（翻译批次内快速用）
  python tools/check_i18n.py report [en|ru]       # 度量表（覆盖率/比例/术语命中）
  python tools/check_i18n.py selftest             # 工具自检（zh 自反应全绿）

判定对象：docs/book/NN-slug.md（中文源）与 docs/book/{en,ru}/NN-slug.md 同名镜像对。
未翻译（译文缺失）不算 FAIL——覆盖率在 report 中度量；已存在则必须全绿。
"""

import io
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOOK = os.path.join(REPO, "docs", "book")
GLOSSARY_PATH = os.path.join(REPO, "tools", "i18n", "glossary.json")

FENCE_RE = re.compile(r"^```")
INLINE_RE = re.compile(r"`([^`\n]+)`")
CJK_RE = re.compile(r"[\u3000-\u303f\u4e00-\u9fff\uff00-\uffef]")
URL_RE = re.compile(r"https?://\S+")
# API 符号：x 开头驼峰函数/类型（长度>=4）与 X 前缀宏（全大写>=3 后接字母数字）
SYM_RE = re.compile(r"\bx\w{3,}[A-Z]\w*|\bX[A-Z]{2,}[A-Za-z0-9_]*")
RATIO_MIN, RATIO_MAX = 0.30, 1.60
# G8 连续批次命中率低于此值升级 FAIL（阶段内在 PROGRESS 记录跟踪）
GLOSSARY_HIT_WARN = 0.90


def load_glossary():
    data = json.load(io.open(GLOSSARY_PATH, encoding="utf-8"))
    return data.get("terms", []), data.get("brands", []), data.get("cjk_brands", [])


def split_fm(text):
    """返回 (frontmatter dict, body)。"""
    if not text.startswith("---"):
        return {}, text
    end = text.find("\n---", 3)
    if end < 0:
        return {}, text
    fm_raw = text[3:end].strip("\n")
    body = text[end + 4:]
    fm = {}
    for line in fm_raw.splitlines():
        if ":" in line:
            k, v = line.split(":", 1)
            fm[k.strip()] = v.strip()
    return fm, body


def split_blocks(body):
    """返回 (fenced 块内容列表, 去除 fenced 后的行列表)。"""
    lines = body.splitlines()
    fences, plain, buf, infence = [], [], [], False
    for ln in lines:
        if FENCE_RE.match(ln):
            if infence:
                fences.append("\n".join(buf))
                buf = []
            infence = not infence
            continue
        if infence:
            buf.append(ln)
        else:
            plain.append(ln)
    return fences, plain


def strip_inline(text):
    return INLINE_RE.sub(" ", text)


def symbols_of(text):
    """代码块与行内码之外的 API 符号多重集合（排序元组）。"""
    return sorted(SYM_RE.findall(text))


def headings_of(plain_lines):
    """h2 分组下的 h3 数量序列 + h2 总数。"""
    h2, cur, seq = 0, 0, []
    for ln in plain_lines:
        if ln.startswith("## ") and not ln.startswith("### "):
            h2 += 1
            cur = 0
            seq.append(("h2", ln[3:].strip()))
        elif ln.startswith("### "):
            cur += 1
            seq.append(("h3", ln[4:].strip()))
    return h2, seq


def cjk_residuals(text, cjk_brands):
    """正文中残留的 CJK 片段（URL 与白名单品牌除外）。"""
    clean = URL_RE.sub(" ", text)
    for b in cjk_brands:
        clean = clean.replace(b, " ")
    return CJK_RE.findall(clean)


def hanzi_count(text):
    return len(re.findall(r"[\u4e00-\u9fff]", text))


def word_count(text):
    return len([w for w in re.split(r"\s+", text) if w])


def check_pair(lang, fname, skip=()):
    """检查一对 zh/译文文件，返回 (ok, results, stats)。skip 供自反自检跳过无意义项。"""
    zh_path = os.path.join(BOOK, fname)
    tr_path = os.path.join(BOOK, lang, fname)
    _zero = {"ratio": 0.0, "hit": 0, "total": 0, "zh_chars": 0}
    if not os.path.exists(zh_path):
        return False, (["中文源不存在: %s" % fname], []), _zero
    if not os.path.exists(tr_path):
        return False, (["译文不存在: %s/%s" % (lang, fname)], []), _zero
    zh_text = io.open(zh_path, encoding="utf-8").read()
    tr_text = io.open(tr_path, encoding="utf-8").read()
    zh_fm, zh_body = split_fm(zh_text)
    tr_fm, tr_body = split_fm(tr_text)
    zh_f, zh_plain = split_blocks(zh_body)
    tr_f, tr_plain = split_blocks(tr_body)
    zh_prose = strip_inline("\n".join(zh_plain))
    tr_prose = strip_inline("\n".join(tr_plain))
    errs, warns = [], []

    def gate(name):
        return name not in skip

    # G1 fenced 逐字节一致
    if zh_f != tr_f:
        errs.append("G1 代码块与中文源不一致（%d vs %d 块或内容漂移）" % (len(zh_f), len(tr_f)))
    # G2 行内码一致
    if sorted(INLINE_RE.findall(zh_body)) != sorted(INLINE_RE.findall(tr_body)):
        errs.append("G2 行内代码 token 与中文源不一致")
    # G3 API 符号集合一致（代码块外）
    zh_syms, tr_syms = symbols_of(zh_prose), symbols_of(tr_prose)
    if zh_syms != tr_syms:
        from collections import Counter
        diff = Counter(zh_syms) - Counter(tr_syms)
        diff.update({k: -v for k, v in (Counter(tr_syms) - Counter(zh_syms)).items()})
        errs.append("G3 正文 API 符号不一致（zh→en 计数差）%s" % dict(list(diff.items())[:5]))
    # G4 结构一致：h2 数 + h3 分布
    zh_h2, zh_seq = headings_of(zh_plain)
    tr_h2, tr_seq = headings_of(tr_plain)
    zh_shape = [k for k, _ in zh_seq]
    tr_shape = [k for k, _ in tr_seq]
    if zh_h2 != tr_h2 or zh_shape != tr_shape:
        errs.append("G4 章节结构不一致（h2 %d vs %d，h3 分布漂移）" % (zh_h2, tr_h2))
    # G5 中文残留（译文正文）
    terms, brands, cjk_brands = load_glossary()
    if gate("G5"):
        res = cjk_residuals(tr_prose, cjk_brands)
        if res:
            errs.append("G5 译文残留中文 %d 处（如 %s）" % (len(res), res[:5]))
    # G6 体量比例
    zh_chars = hanzi_count(zh_prose)
    tr_words = word_count(tr_prose)
    ratio = (float(tr_words) / zh_chars) if zh_chars else 0.0
    if gate("G6") and zh_chars and not (RATIO_MIN <= ratio <= RATIO_MAX):
        errs.append("G6 词数比 %.2f 超出 [%s, %s]" % (ratio, RATIO_MIN, RATIO_MAX))
    # G7 frontmatter 对齐
    for k in ("num", "slug", "type", "api"):
        if gate("G7") and zh_fm.get(k) != tr_fm.get(k):
            errs.append("G7 frontmatter %s 不一致（%r vs %r）" % (k, zh_fm.get(k), tr_fm.get(k)))
    if gate("G7") and (CJK_RE.search(tr_fm.get("title", "")) or CJK_RE.search(tr_fm.get("lead", ""))):
        errs.append("G7 title/lead 未翻译")
    # G8 术语命中（WARN）
    hit, total = 0, 0
    low = tr_prose.lower()
    for t in terms:
        if t["zh"] in zh_prose and t.get(lang):
            total += 1
            if t[lang].lower() in low:
                hit += 1
    if total and hit / float(total) < GLOSSARY_HIT_WARN:
        warns.append("G8 术语命中 %d/%d（<%.0f%%，连续两批将升级 FAIL）" % (hit, total, GLOSSARY_HIT_WARN * 100))
    stats = {"ratio": ratio, "hit": hit, "total": total, "zh_chars": zh_chars}
    return (not errs), (errs, warns), stats


def iter_pairs(lang):
    for fn in sorted(os.listdir(BOOK)):
        if not fn.endswith(".md"):
            continue
        if os.path.exists(os.path.join(BOOK, lang, fn)):
            yield fn


def cmd_check(langs, single=None):
    terms_total = len(load_glossary()[0])
    fails = 0
    for lang in langs:
        files = [single] if single else list(iter_pairs(lang))
        if single and not os.path.exists(os.path.join(BOOK, lang, single)):
            print("[MISS] %s/%s 译文不存在" % (lang, single))
            fails += 1
            continue
        for fn in files:
            ok, (errs, warns), _ = check_pair(lang, fn)
            if ok:
                print("[PASS] %s %s" % (lang, fn))
            else:
                fails += 1
                print("[FAIL] %s %s" % (lang, fn))
                for e in errs:
                    print("       " + e)
            for w in warns:
                print("[WARN] %s %s %s" % (lang, fn, w))
    print("术语表条目: %d" % terms_total)
    print("结果: %s" % ("全绿" if fails == 0 else "%d FAIL" % fails))
    return 0 if fails == 0 else 1


def cmd_report(langs):
    order = json.load(io.open(os.path.join(BOOK, "order.json"), encoding="utf-8"))
    total = len([it for it in order if os.path.exists(os.path.join(BOOK, "%s.md" % it["file"]))])
    for lang in langs:
        done = list(iter_pairs(lang))
        print("== %s == 已译 %d / %d 章" % (lang, len(done), total))
        for fn in done:
            ok, (errs, warns), st = check_pair(lang, fn)
            status = "PASS" if ok else "FAIL"
            print("%-4s %s 词数比%.2f 术语%d/%d" % (status, fn, st["ratio"], st["hit"], st["total"]))
    return 0


def cmd_selftest():
    """zh 自反应 G1-G5/G7 全绿（G6 比例对自反无意义、跳过；G8 自反必然全中）。"""
    global RATIO_MIN, RATIO_MAX
    fn = "03-first.md"
    src = io.open(os.path.join(BOOK, fn), encoding="utf-8").read()
    tmp = os.path.join(BOOK, "selftest_tmp")
    os.makedirs(tmp, exist_ok=True)
    io.open(os.path.join(tmp, fn), "w", encoding="utf-8", newline="\n").write(src)
    RATIO_MIN, RATIO_MAX = 0.0, 999.0
    try:
        ok, (errs, _), _ = check_pair("selftest_tmp", fn, skip=("G5", "G6", "G7"))
    finally:
        os.remove(os.path.join(tmp, fn))
        os.rmdir(tmp)
        RATIO_MIN, RATIO_MAX = 0.30, 1.60
    print("[SELFTEST] G1-G4 自反: %s（G5-G7 对自反无意义，跳过）" % ("全绿" if ok else "异常: %s" % errs))
    return 0 if ok else 1


def main(argv):
    if len(argv) < 2 or argv[1] == "check":
        args = argv[2:]
        langs = [args[0]] if args and args[0] in ("en", "ru") else ["en", "ru"]
        single = args[1] if len(args) > 1 else None
        return cmd_check(langs, single if single and single.endswith(".md") else (single + ".md" if single else None))
    if argv[1] == "report":
        langs = [argv[2]] if len(argv) > 2 and argv[2] in ("en", "ru") else ["en", "ru"]
        return cmd_report(langs)
    if argv[1] == "selftest":
        return cmd_selftest()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
