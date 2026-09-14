#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""三语言 sitemap.xml 生成器（I28 全站收尾产出，可重复运行）。

策略：
- 门面页（index/start/benchmarks/reliability/api/book 目录）三语言齐全 -> xhtml:link 互指；
- 章节页按实际已译语言互指（zh 恒在 + x-default 指向 zh；en/ru 已译才列入）；
- ref 页（I28 决策 A：保留中文共用）与未译章节仅 zh URL，无 alternate；
- guide.html 为 noindex 重定向页，不入 sitemap。
用法：python tools/i18n/gen_sitemap.py [--wwwroot D:/GIT/home/host/xrt/wwwroot]
"""
import argparse
import io
import os
import re
import time

BASE = "https://xrt.xywhsoft.com"
FACADES = ["index.html", "start.html", "benchmarks.html", "reliability.html", "api.html",
           "book/index.html"]


def lang_url(lang_dir, page):
    return "%s/%s%s" % (BASE, (lang_dir + "/") if lang_dir else "", page)


def exists(wwwroot, lang_dir, page):
    return os.path.exists(os.path.join(wwwroot, lang_dir, page))


def lastmod(path):
    return time.strftime("%Y-%m-%d", time.localtime(os.path.getmtime(path)))


def alternates(pairs):
    """pairs: [(hreflang, url), ...] -> xhtml:link 行。"""
    return "".join(
        '\n    <xhtml:link rel="alternate" hreflang="%s" href="%s"/>' % (h, u)
        for h, u in pairs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wwwroot", default="D:/GIT/home/host/xrt/wwwroot")
    args = ap.parse_args()
    www = args.wwwroot.replace("\\", "/")

    urls = []  # (loc, path_on_disk, alt_pairs)

    # 门面页：三语言互指
    for page in FACADES:
        variants = [(ld, page) for ld in ("", "en", "ru") if exists(www, ld, page)]
        pairs = []
        if ("", page) in variants:
            pairs.append(("zh-CN", lang_url("", page)))
        if ("en", page) in variants:
            pairs.append(("en", lang_url("en", page)))
        if ("ru", page) in variants:
            pairs.append(("ru", lang_url("ru", page)))
        if ("", page) in variants:
            pairs.append(("x-default", lang_url("", page)))
        for ld, pg in variants:
            urls.append((lang_url(ld, pg), os.path.join(www, ld, pg), pairs))

    # 章节页：zh 恒在，en/ru 按已译；重定向页（标题含"重定向"）不入 sitemap
    zh_pages = []
    for f in sorted(os.listdir(os.path.join(www, "book"))):
        if not re.match(r"ch\d+-.*\.html$", f):
            continue
        head = io.open(os.path.join(www, "book", f), encoding="utf-8", errors="replace").read(2000)
        if "http-equiv=\"refresh\"" in head:
            continue
        zh_pages.append(f)
    for f in zh_pages:
        en_pg = os.path.join(www, "en", "book", f)
        ru_pg = os.path.join(www, "ru", "book", f)
        pairs = [("zh-CN", lang_url("", "book/" + f))]
        if os.path.exists(en_pg):
            pairs.append(("en", lang_url("en", "book/" + f)))
        if os.path.exists(ru_pg):
            pairs.append(("ru", lang_url("ru", "book/" + f)))
        pairs.append(("x-default", lang_url("", "book/" + f)))
        urls.append((lang_url("", "book/" + f), os.path.join(www, "book", f), pairs))
        if os.path.exists(en_pg):
            urls.append((lang_url("en", "book/" + f), en_pg, pairs))
        if os.path.exists(ru_pg):
            urls.append((lang_url("ru", "book/" + f), ru_pg, pairs))

    # ref 页：zh 恒在；已有 en/ru 译本的 ref 页按已译语言互指（决策 A 的增量扩展）
    ref_pages = sorted(f for f in os.listdir(os.path.join(www, "book"))
                       if re.match(r"ref-.*\.html$", f))
    for f in ref_pages:
        en_pg = os.path.join(www, "en", "book", f)
        ru_pg = os.path.join(www, "ru", "book", f)
        if not (os.path.exists(en_pg) or os.path.exists(ru_pg)):
            urls.append((lang_url("", "book/" + f), os.path.join(www, "book", f), []))
            continue
        pairs = [("zh-CN", lang_url("", "book/" + f))]
        if os.path.exists(en_pg):
            pairs.append(("en", lang_url("en", "book/" + f)))
        if os.path.exists(ru_pg):
            pairs.append(("ru", lang_url("ru", "book/" + f)))
        pairs.append(("x-default", lang_url("", "book/" + f)))
        urls.append((lang_url("", "book/" + f), os.path.join(www, "book", f), pairs))
        if os.path.exists(en_pg):
            urls.append((lang_url("en", "book/" + f), en_pg, pairs))
        if os.path.exists(ru_pg):
            urls.append((lang_url("ru", "book/" + f), ru_pg, pairs))

    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"',
           '        xmlns:xhtml="http://www.w3.org/1999/xhtml">']
    for loc, path, pairs in urls:
        out.append('  <url>')
        out.append('    <loc>%s</loc>' % loc)
        if pairs:
            out.append('    ' + alternates(pairs).strip())
        out.append('    <lastmod>%s</lastmod>' % lastmod(path))
        out.append('  </url>')
    out.append('</urlset>')
    io.open(os.path.join(www, "sitemap.xml"), "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    n_alt = sum(1 for _, _, p in urls if p)
    print("sitemap.xml: %d URLs（互指 %d / zh-only %d）" % (len(urls), n_alt, len(urls) - n_alt))


if __name__ == "__main__":
    main()
