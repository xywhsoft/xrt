#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""程序设计教程生成器：docs/book/*.md -> wwwroot/book/ch*.html。

依据 docs/book/BOOK_SPEC.md（源格式与门禁规范）。

命令:
  init-order            扫描 wwwroot 现有章节页生成 docs/book/order.json（迁移期全书排序事实源）
  build [--only 3,52]   渲染章节页（prev/next/进度按 order.json 计算）
  material <slug>       输出某章的素材包（示例源码清单、API 契约、符号表）——写作前必看
"""

import argparse
import glob
import io
import json
import os
import re
import sys

GITEE_BLOB = "https://gitee.com/xywhsoft/xrt/blob/master/"

CANON_SECTIONS = ["导读", "引入", "概念", "示例", "契约", "避坑", "练习", "速查"]
NUMBERED = {"引入", "概念", "示例", "契约"}
SECTION_TITLES = {"导读": "本章导读", "避坑": "常见错误", "练习": "编程练习", "速查": "本章小结"}

C_KEYWORDS = {
    "if", "else", "for", "while", "do", "switch", "case", "default", "break",
    "continue", "return", "goto", "typedef", "struct", "enum", "union", "const",
    "static", "inline", "extern", "sizeof", "volatile", "register", "auto",
    "void", "char", "short", "int", "long", "float", "double", "unsigned",
    "signed", "bool", "true", "false", "NULL", "size_t",
}

FM_RE = re.compile(r"\b(num|slug|title|volume|type|lead|api)\s*:\s*(.+)")


def e(s):
    import html
    return html.escape(s, quote=True)


# ---------------------------------------------------------------------------
# 源解析
# ---------------------------------------------------------------------------

class Fence(object):
    __slots__ = ("kind", "info", "text")

    def __init__(self, kind, info, text):
        self.kind = kind          # c / embed / term / diagram
        self.info = info          # bad / good / state / flow / 参数串
        self.text = text


class Block(object):
    """小节内的一个块：('p', lines) 或 ('fence', Fence) 或 ('h', text) 或 ('tbl', rows)"""
    __slots__ = ("kind", "data")

    def __init__(self, kind, data):
        self.kind = kind
        self.data = data


class SubSec(object):
    __slots__ = ("title", "blocks")

    def __init__(self, title):
        self.title = title
        self.blocks = []


class Chapter(object):
    def __init__(self, path):
        self.path = path
        self.fm = {}
        self.sections = {}        # key -> [Block|SubSec]
        self.section_order = []
        raw = io.open(path, encoding="utf-8").read().replace("\r\n", "\n")
        self._parse(raw)

    def _parse(self, raw):
        lines = raw.split("\n")
        i = 0
        # front matter
        if lines and lines[0].strip() == "---":
            i = 1
            while i < len(lines) and lines[i].strip() != "---":
                m = FM_RE.search(lines[i])
                if m:
                    self.fm[m.group(1)] = m.group(2).strip()
                i += 1
            i += 1
        cur_sec = None
        cur_sub = None
        para = []
        fence = None

        def flush_para():
            nonlocal para
            if para and cur_sec is not None:
                target = cur_sub.blocks if cur_sub is not None else self.sections[cur_sec]
                target.append(Block("p", list(para)))
            para = []

        def flush_fence():
            nonlocal fence
            if fence is not None and cur_sec is not None:
                target = cur_sub.blocks if cur_sub is not None else self.sections[cur_sec]
                target.append(Block("fence", fence))
            fence = None

        while i < len(lines):
            line = lines[i]
            if fence is not None:
                if line.strip().startswith("```"):
                    flush_fence()
                else:
                    fence.text.append(line)
                i += 1
                continue
            if line.strip().startswith("```"):
                flush_para()
                info = line.strip()[3:].strip()
                parts = info.split(None, 1)
                kind = parts[0] if parts else "c"
                rest = parts[1] if len(parts) > 1 else ""
                fence = Fence(kind, rest, [])
                i += 1
                continue
            m = re.match(r"^(#{2,3})\s+(.*?)\s*$", line)
            if m:
                flush_para()
                lvl, text = len(m.group(1)), m.group(2).strip()
                if lvl == 2:
                    cur_sub = None
                    key = text
                    if key not in CANON_SECTIONS:
                        # 非规范节： tolerated in front matter area? 拒绝
                        raise ValueError("%s: 非法 h2 节 %r（规范八节之外）" % (self.path, key))
                    cur_sec = key
                    if key not in self.sections:
                        self.sections[key] = []
                        self.section_order.append(key)
                else:
                    if cur_sec is None:
                        raise ValueError("%s: h3 出现在任何 h2 之前" % self.path)
                    cur_sub = SubSec(text)
                    self.sections[cur_sec].append(cur_sub)
                i += 1
                continue
            # 表格行
            if line.strip().startswith("|"):
                flush_para()
                rows = []
                while i < len(lines) and lines[i].strip().startswith("|"):
                    rows.append(lines[i].strip())
                    i += 1
                if cur_sec is not None:
                    target = cur_sub.blocks if cur_sub is not None else self.sections[cur_sec]
                    target.append(Block("tbl", rows))
                continue
            # 无序列表
            if line.strip().startswith("- "):
                flush_para()
                items = []
                while i < len(lines) and lines[i].strip().startswith("- "):
                    items.append(lines[i].strip()[2:])
                    i += 1
                if cur_sec is not None:
                    target = cur_sub.blocks if cur_sub is not None else self.sections[cur_sec]
                    target.append(Block("ul", items))
                continue
            if not line.strip():
                flush_para()
                i += 1
                continue
            para.append(line)
            i += 1
        flush_para()
        flush_fence()


def chapter_meta(ch):
    fm = ch.fm
    for k in ("num", "slug", "title", "volume", "type", "lead", "api"):
        if k not in fm:
            raise ValueError("%s: front matter 缺字段 %s" % (ch.path, k))
    return fm


# ---------------------------------------------------------------------------
# C 语法高亮（复用站点 code-block 的 span 类）
# ---------------------------------------------------------------------------

def highlight_c(src, known_types):
    out = []
    in_comment = False
    for line in src.split("\n"):
        buf = []
        j = 0
        n = len(line)
        while j < n:
            if in_comment:
                k = line.find("*/", j)
                if k < 0:
                    j = n
                else:
                    buf.append('<span class="cm">%s</span>' % e(line[j:k + 2]))
                    in_comment = False
                    j = k + 2
                continue
            if line.startswith("/*", j):
                in_comment = True
                continue
            if line.startswith("//", j):
                buf.append('<span class="cm">%s</span>' % e(line[j:]))
                j = n
                continue
            c = line[j]
            if c == '"':
                k = j + 1
                while k < n and line[k] != '"':
                    k += 2 if line[k] == "\\" else 1
                k = min(k + 1, n)
                buf.append('<span class="str">%s</span>' % e(line[j:k]))
                j = k
                continue
            if c == "#" and not line[:j].strip():
                buf.append('<span class="pp">%s</span>' % e(line))
                j = n
                continue
            if c.isdigit():
                k = j
                while k < n and (line[k].isalnum() or line[k] in "._xu"):
                    k += 1
                buf.append('<span class="num">%s</span>' % e(line[j:k]))
                j = k
                continue
            if c.isalpha() or c == "_":
                k = j
                while k < n and (line[k].isalnum() or line[k] == "_"):
                    k += 1
                word = line[j:k]
                nxt = line[k:].lstrip()
                if word in C_KEYWORDS:
                    buf.append('<span class="kw">%s</span>' % word)
                elif word in known_types or word.startswith(("xrt", "XRT_")):
                    if word in known_types and not word.startswith("xrt"):
                        buf.append('<span class="type">%s</span>' % word)
                    elif word.startswith("xrt") and nxt.startswith("("):
                        buf.append('<span class="fn">%s</span>' % word)
                    else:
                        buf.append(e(word))
                elif nxt.startswith("("):
                    buf.append('<span class="fn">%s</span>' % word)
                else:
                    buf.append(e(word))
                j = k
                continue
            buf.append(e(c))
            j += 1
        out.append("".join(buf))
    return "\n".join(out)


# ---------------------------------------------------------------------------
# markdown 块 -> HTML
# ---------------------------------------------------------------------------

INLINE_CODE = re.compile(r"`([^`]+)`")
INLINE_BOLD = re.compile(r"\*\*([^*]+)\*\*")
INLINE_LINK = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")


def inline_md(s, order):
    s = e(s)

    def link(m):
        text, url = m.group(1), m.group(2)
        if url.endswith(".md"):
            target = re.sub(r"^ch\d+-", "", url[:-3])
            entry = order_by_slug(order, target)
            if entry:
                url = "ch%s.html" % entry["file"]
                text = "第 %d 章 %s" % (entry["num"], text)
            else:
                url = GITEE_BLOB + "docs/book/" + url
        return '<a href="%s">%s</a>' % (e(url), INLINE_CODE.sub(r"<code>\1</code>", text))

    s = INLINE_LINK.sub(link, s)
    s = INLINE_CODE.sub(r"<code>\1</code>", s)
    s = INLINE_BOLD.sub(r"<strong>\1</strong>", s)
    return s


def order_by_slug(order, slug):
    for it in order:
        if it["slug"] == slug:
            return it
    return None


def render_table(rows, cls, order):
    cells = []
    for r in rows:
        r = r.strip().strip("|")
        cells.append([c.strip() for c in r.split("|")])
    if len(cells) >= 2 and all(re.fullmatch(r":?-{2,}:?", c) for c in cells[1] if c):
        header, body = cells[0], cells[2:]
    else:
        header, body = None, cells
    out = ['<table class="%s">' % cls]
    if header:
        out.append("<tr>%s</tr>" % "".join("<th>%s</th>" % inline_md(c, order) for c in header))
    for row in body:
        out.append("<tr>%s</tr>" % "".join("<td>%s</td>" % inline_md(c, order) for c in row))
    out.append("</table>")
    return "".join(out)


def render_code_block(code, fname, known_types, src_url=None):
    src = '<a class="code-src" href="%s" target="_blank" rel="noopener">源码 ↗</a>' % e(src_url) if src_url else ""
    return ('<div class="code-block"><div class="code-head"><span class="fname">%s</span>%s'
            '<button class="copy-btn" type="button">复制</button></div>'
            '<div class="code-body"><pre>%s</pre></div></div>'
            % (e(fname), src, highlight_c(code, known_types)))


def render_term(lines):
    out = ['<div class="term-block">']
    for l in lines:
        if l.startswith("$ "):
            out.append('<span class="t-cmd">%s</span>' % e(l))
        elif l.strip():
            out.append('<span class="t-out">%s</span>' % e(l))
    out.append("</div>")
    return "".join(out)


def render_diagram(info, lines):
    if info == "state":
        rows = []
        for l in lines:
            m = re.match(r"(\S+)\s*->\s*(\S+)\s*:\s*(.*)", l.strip())
            if not m:
                continue
            rows.append('<div class="dg-row"><span class="dg-node">%s</span>'
                        '<span class="dg-arrow">%s →</span>'
                        '<span class="dg-node">%s</span></div>'
                        % (e(m.group(1)), e(m.group(3)), e(m.group(2))))
        return '<div class="diagram dg-state">%s</div>' % "".join(rows)
    if info == "flow":
        steps = []
        no = 0
        for l in lines:
            m = re.match(r"-\s*([^：:]+)[：:]\s*(.*)", l.strip())
            if not m:
                continue
            no += 1
            steps.append('<div class="dg-step"><span class="dg-no">%d</span>'
                         '<span class="dg-name">%s</span>'
                         '<span class="dg-desc">%s</span></div>'
                         % (no, e(m.group(1)), e(m.group(2))))
        return '<div class="diagram dg-flow">%s</div>' % "".join(steps)
    return ""


def render_fence(f, order, known_types, repo):
    if f.kind == "embed":
        opts = dict(re.findall(r'(\w+)\s*=\s*"([^"]*)"', f.info))
        path = opts.get("path", "")
        full = os.path.join(repo, path.replace("/", os.sep))
        text = io.open(full, encoding="utf-8").read().replace("\r\n", "\n").split("\n")
        if "lines" in opts:
            a, b = opts["lines"].split("-")
            text = text[int(a) - 1:int(b)]
        code = "\n".join(text)
        title = opts.get("title", path)
        return render_code_block(code, title, known_types, GITEE_BLOB + path)
    if f.kind == "term":
        return render_term(f.text)
    if f.kind == "diagram":
        return render_diagram(f.info, f.text)
    # ```c [bad|good]
    if f.info in ("bad", "good"):
        return ('<div class="cp-%s"><div class="cp-tag">%s</div><pre>%s</pre></div>'
                % (f.info, "错误写法" if f.info == "bad" else "正确写法", highlight_c("\n".join(f.text), known_types)))
    return ('<div class="code-block"><div class="code-body"><pre>%s</pre></div></div>'
            % highlight_c("\n".join(f.text), known_types))


def render_blocks(items, order, known_types, repo, sub=False):
    out = []
    i = 0
    while i < len(items):
        it = items[i]
        if isinstance(it, SubSec):
            out.append("<h4>%s</h4>" % inline_md(it.title, order))
            out.append(render_blocks(it.blocks, order, known_types, repo))
            i += 1
            continue
        if it.kind == "fence" and it.data.kind == "c" and it.data.info in ("bad", "good"):
            # 连续的 bad/good 块包进 code-pair 容器（左右对照）
            pair = []
            j = i
            while j < len(items):
                b = items[j]
                if b.kind == "fence" and b.data.kind == "c" and b.data.info in ("bad", "good"):
                    pair.append(render_fence(b.data, order, known_types, repo))
                    j += 1
                else:
                    break
            out.append('<div class="code-pair">%s</div>' % "".join(pair))
            i = j
            continue
        if it.kind == "p":
            text = " ".join(x.strip() for x in it.data if x.strip())
            if text:
                out.append("<p>%s</p>" % inline_md(text, order))
        elif it.kind == "ul":
            out.append("<ul>%s</ul>" % "".join(
                "<li>%s</li>" % inline_md(x, order) for x in it.data if x.strip()))
        elif it.kind == "tbl":
            out.append(render_table(it.data, "book-sum", order))
        elif it.kind == "fence":
            out.append(render_fence(it.data, order, known_types, repo))
        i += 1
    return "".join(out)


# ---------------------------------------------------------------------------
# 页面模板
# ---------------------------------------------------------------------------

NAV_HTML = ('<nav class="nav" aria-label="主导航"><div class="nav-inner">'
            '<a href="../index.html" class="nav-logo" aria-label="XRT 首页"><span class="logo-mark">X</span>XRT<em>v2.0</em></a>'
            '<button type="button" class="nav-toggle" aria-controls="site-nav" aria-expanded="false" aria-label="打开菜单"><span></span></button>'
            '<ul class="nav-links" id="site-nav"><li><a href="../index.html#arch">产品架构</a></li>'
            '<li><a href="../reliability.html">工程品质</a></li><li><a href="../benchmarks.html">性能设计</a></li>'
            '<li><a href="../start.html">快速开始</a></li>'
            '<li><details class="nav-dropdown"><summary>开发文档</summary><div><a href="../book/index.html">程序设计教程</a><a href="../api.html">API 参考与搜索</a></div></details></li>'
            '<li><details class="nav-dropdown"><summary>源码仓库</summary><div><a href="https://github.com/xywhsoft/xrt" target="_blank" rel="noopener">GitHub ↗</a><a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee ↗</a></div></details></li>'
            '</ul></div></nav>')

FOOTER_HTML = ('<footer class="footer">\n  <div class="footer-inner">\n    <div>\n'
               '      <div class="f-brand"><span class="logo-mark">X</span>XRT</div>\n'
               '      <p class="f-desc">力求卓越的互联网 + AI 时代跨平台 C 基础设施库——一整套成体系的基础设施库。</p>\n    </div>\n'
               '    <div>\n      <h3>站点</h3>\n      <ul>\n        <li><a href="../index.html">首页</a></li>\n        <li><a href="index.html">书籍</a></li>\n        <li><a href="../api.html">API 概览</a></li>\n      </ul>\n    </div>\n'
               '    <div>\n      <h3>资源</h3>\n      <ul>\n        <li><a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee 仓库</a></li>\n'
               '        <li><a href="https://gitee.com/xywhsoft/xrt/issues" target="_blank" rel="noopener">问题反馈</a></li>\n'
               '        <li><a href="../api.html#reference">API 参考入口</a></li>\n      </ul>\n    </div>\n  </div>\n'
               '  <div class="footer-bottom">\n    XRT &copy; <span data-year>2026</span> xLeaves (xywhsoft) &middot; MIT License &middot; <a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee</a>\n  </div>\n</footer>')


def build_page(ch, entry, order, known_types, repo, api_counts, total_chapters):
    fm = ch.fm
    num = int(fm["num"])
    items = list(order)
    idx = next(i for i, it in enumerate(items) if it["num"] == num)
    prev_it, next_it = (items[idx - 1] if idx > 0 else None), (items[idx + 1] if idx + 1 < len(items) else None)

    # 侧栏 + 正文
    side_cur, side_fix = [], []
    body = []
    sec_no = 0
    anchor = {"导读": "intro", "避坑": "pits", "练习": "exercise", "速查": "summary"}
    for key in CANON_SECTIONS:
        blocks = ch.sections.get(key)
        if blocks is None:
            continue
        if key == "导读":
            body.append('<section class="book-sec" id="intro"><h3>本章导读</h3>%s</section>'
                        % render_blocks(blocks, order, known_types, repo))
            side_cur.append('<li><a href="#intro">导读</a></li>')
            continue
        if key in NUMBERED:
            sec_no += 1
            sid = "sec%d" % sec_no
            body.append('<section class="book-sec" id="%s"><h3><span class="sec-no">%d.%d</span>%s</h3>%s</section>'
                        % (sid, num, sec_no, key, render_blocks(blocks, order, known_types, repo)))
            side_cur.append('<li><a href="#%s">%d.%d %s</a></li>' % (sid, num, sec_no, key))
            continue
        title = SECTION_TITLES.get(key, key)
        aid = anchor.get(key, key)
        if key == "避坑":
            inner = []
            for it in blocks:
                if isinstance(it, SubSec):
                    m = re.match(r"坑\s*(\d+)[：:]\s*(.*)", it.title)
                    tag = ("坑 %s" % m.group(1)) if m else it.title
                    head = m.group(2) if m else it.title
                    inner.append('<div class="book-pit"><span class="box-tag">%s：%s</span>'
                                 '<div class="pit-body">%s</div></div>'
                                 % (e(tag), e(head), render_blocks(it.blocks, order, known_types, repo)))
                elif it.kind == "p":
                    text = " ".join(x.strip() for x in it.data if x.strip())
                    inner.append("<p>%s</p>" % inline_md(text, order))
            html = "".join(inner)
        elif key == "练习":
            inner = []
            for it in blocks:
                if isinstance(it, SubSec):
                    inner.append('<div class="book-ex"><span class="box-tag">%s</span>%s</div>'
                                 % (e(it.title), render_blocks(it.blocks, order, known_types, repo)))
                elif it.kind == "p":
                    text = " ".join(x.strip() for x in it.data if x.strip())
                    inner.append("<p>%s</p>" % inline_md(text, order))
            html = "".join(inner)
        else:  # 速查
            html = render_blocks(blocks, order, known_types, repo)
        body.append('<section class="book-sec" id="%s"><h3>%s</h3>%s</section>' % (aid, title, html))
        side_fix.append('<li><a href="#%s">%s</a></li>' % (aid, title))

    # 页尾 API 链接区（真实计数）
    api_links = []
    for name in [x.strip() for x in fm["api"].split(",") if x.strip()]:
        cnt = api_counts.get(name)
        label = "%s（%s 个条目）" % (name, str(cnt) if cnt is not None else "?")
        api_links.append('<a href="ref-%s.html" style="display:inline-block;margin:2px 6px;padding:3px 12px;'
                         'border-radius:6px;background:rgba(91,157,255,.1);color:#5b9dff;'
                         'text-decoration:none;font-size:13px;">%s</a>' % (e(name), e(label)))
    api_box = ('<div style="margin:28px 0;padding:16px 20px;border:1px solid rgba(91,157,255,.15);'
               'border-radius:10px;background:rgba(91,157,255,.04);">'
               '<p style="margin:0 0 10px;font-size:14px;color:var(--soft);">'
               '<strong>📖 本章涉及的 API 参考手册：</strong>%s</p>'
               '<p style="margin:10px 0 0;font-size:12px;color:var(--muted);">'
               '点击查看完整签名、参数约束、返回值与错误说明</p></div>' % "".join(api_links))

    prev_html = ('<a href="ch%s.html" class="prev"><span class="pn-dir">上一章</span>'
                 '<span class="pn-title">第 %d 章 %s</span></a>' % (prev_it["file"], prev_it["num"], e(prev_it["title"]))
                 if prev_it else "<span></span>")
    next_html = ('<a href="ch%s.html" class="next"><span class="pn-dir">下一章</span>'
                 '<span class="pn-title">第 %d 章 %s</span></a>' % (next_it["file"], next_it["num"], e(next_it["title"]))
                 if next_it else "<span></span>")

    side = ('<aside class="book-side"><h4>本章</h4><ul>%s</ul>'
            '<h4>巩固</h4><ul>%s</ul></aside>'
            % ("".join(side_cur), "".join(side_fix) or "<li><a href=\"#summary\">小结</a></li>"))

    tpl = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>第 {num} 章 {title} - 《XRT 程序设计》</title>
  <meta name="description" content="XRT 程序设计教程第 {num} 章{lead}">
  <link rel="canonical" href="https://xrt.xywhsoft.com/book/ch{file}.html">
  <link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='8' fill='%235b9dff'/%3E%3C/text%3E%3C/svg%3E">
  <link rel="stylesheet" href="../style.css?v=6bdafbdced">
  <link rel="stylesheet" href="book.css?v=730594b9cc">
  <link rel="stylesheet" href="../refinement.css?v=ad12773fdb">
</head>
<body>
<a class="skip-link" href="#main">跳到主要内容</a>
<div class="bg-scene" aria-hidden="true"></div>
{nav}
<header class="page-header">
  <div class="inner">
    <div class="ch-meta">
      <span class="ch-progress">第 {num} 章 / 共 {total} 章</span>
      <span class="ch-vol">{volume}</span>
    </div>
    <h1>第 {num} 章 {title}</h1>
    <p class="ch-lead">{lead}</p>
  </div>
</header>
<div class="book-layout" id="main" role="main">
{side}
  <div class="book-main">
{body}
    <div class="ch-prevnext">{prev}{next}</div>
{apibox}
  </div>
</div>
{footer}
<script src="../script.js?v=31cac74985" defer></script>
</body>
</html>
"""
    return (tpl
            .replace("{num}", str(num))
            .replace("{file}", entry["file"])
            .replace("{title}", e(fm["title"]))
            .replace("{lead}", e(fm["lead"]))
            .replace("{volume}", e(fm["volume"]))
            .replace("{total}", str(total_chapters))
            .replace("{nav}", NAV_HTML)
            .replace("{side}", side)
            .replace("{body}", "\n".join(body))
            .replace("{prev}", prev_html)
            .replace("{next}", next_html)
            .replace("{apibox}", api_box)
            .replace("{footer}", FOOTER_HTML))


# ---------------------------------------------------------------------------
# order.json
# ---------------------------------------------------------------------------

def load_order(repo):
    path = os.path.join(repo, "docs", "book", "order.json")
    return json.load(io.open(path, encoding="utf-8"))


def cmd_init_order(args):
    www = args.wwwroot
    entries = []
    for f in glob.glob(os.path.join(www, "book", "ch*.html")):
        base = os.path.basename(f)[:-5]          # ch03-error
        m = re.match(r"ch(\d+)-(.*)", base)
        if not m:
            continue
        txt = io.open(f, encoding="utf-8").read()
        t = re.search(r"<h1>(?:第\s*\d+\s*章\s*)?(.*?)</h1>", txt)
        v = re.search(r'<span class="ch-vol">(.*?)</span>', txt)
        entries.append({
            "num": int(m.group(1)),
            "slug": m.group(2),
            "file": m.group(1) + "-" + m.group(2),
            "title": re.sub(r"<[^>]+>", "", t.group(1)).strip() if t else m.group(2),
            "volume": re.sub(r"<[^>]+>", "", v.group(1)).strip() if v else "",
        })
    entries.sort(key=lambda x: x["num"])
    # 同号去重（如 ch82-regex / ch82-xredirect 历史冲突，保留先扫到的）
    dedup = []
    for it in entries:
        if not dedup or dedup[-1]["num"] != it["num"]:
            dedup.append(it)
    entries = dedup
    out = os.path.join(args.repo, "docs", "book", "order.json")
    json.dump(entries, io.open(out, "w", encoding="utf-8", newline="\n"),
              ensure_ascii=False, indent=1)
    print("order.json: %d 章" % len(entries))


# ---------------------------------------------------------------------------
# material：素材包
# ---------------------------------------------------------------------------

def cmd_material(args):
    repo = args.repo
    modules = json.load(io.open(os.path.join(repo, "config", "modules.json"), encoding="utf-8"))["modules"]
    order = load_order(repo)
    entry = order_by_slug(order, args.slug)
    if entry is None:
        sys.exit("order.json 中没有 slug=%s" % args.slug)
    # 找同章的 md 源（若有）
    api_names = []
    hits = glob.glob(os.path.join(repo, "docs", "book", "*-%s.md" % args.slug))
    if hits:
        ch = Chapter(hits[0])
        api_names = [x.strip() for x in ch.fm.get("api", "").split(",") if x.strip()]
    if not api_names:
        api_names = [args.slug.replace("-", "_")]
    print("== 章节素材包: 第 %d 章 %s ==" % (entry["num"], entry["title"]))
    print("卷: %s" % entry.get("volume", ""))
    print("建议 api 页: %s" % ", ".join(api_names))
    for name in api_names:
        print("\n########## 模块域: %s ##########" % name)
        for m in modules:
            hdrs = [os.path.basename(h)[:-2] for h in m.get("public_headers", [])]
            if name in hdrs or m.get("name") == name:
                if m.get("examples"):
                    print("[示例] %s" % ", ".join(m["examples"]))
                if m.get("docs"):
                    print("[文档] %s" % ", ".join(m["docs"]))
                if m.get("tests"):
                    print("[测试] %s" % ", ".join(m["tests"][:4]))
    print("\n提示: examples 源码与 docs/api 契约以上述路径为准；"
          "正文中出现的每个 API 名都必须能在 include/xrt 符号表中找到。")


# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------

def cmd_build(args):
    repo = args.repo
    www = args.wwwroot
    order = load_order(repo)
    # md 源覆盖 order 条目
    sources = {}
    for f in sorted(glob.glob(os.path.join(repo, "docs", "book", "*.md"))):
        base = os.path.basename(f)[:-3]
        m = re.match(r"(\d+)-(.+)", base)
        if not m or base == "BOOK_SPEC":
            continue
        sources[int(m.group(1))] = f
    only = set(int(x) for x in args.only.split(",")) if args.only else None

    # 符号表（类型名用于高亮）
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from gen_web_api import scan_all_headers
    _, hdr_syms, _ = scan_all_headers(repo)
    known_types = set()
    for syms in hdr_syms.values():
        for name, (kind, _) in syms.items():
            if kind == "type":
                known_types.add(name)

    idx_path = os.path.join(www, "search-index.json")
    api_counts = {}
    if os.path.exists(idx_path):
        for it in json.load(io.open(idx_path, encoding="utf-8")):
            api_counts[it["module"]] = api_counts.get(it["module"], 0) + 1

    total = len(order)
    for num, path in sorted(sources.items()):
        if only and num not in only:
            continue
        ch = Chapter(path)
        fm = chapter_meta(ch)
        if int(fm["num"]) != num:
            raise ValueError("%s: 文件名章号 %d 与 front matter num=%s 不一致" % (path, num, fm["num"]))
        entry = next(it for it in order if it["num"] == num)
        entry = dict(entry)
        entry["file"] = "%d-%s" % (num, fm["slug"]) if num >= 100 else "%02d-%s" % (num, fm["slug"])
        page = build_page(ch, entry, order, known_types, repo, api_counts, total)
        out = os.path.join(www, "book", "ch%s.html" % entry["file"])
        io.open(out, "w", encoding="utf-8", newline="\n").write(page)
        print("生成 ch%s.html" % entry["file"])

    # CSS 追加（幂等）
    css_path = os.path.join(www, "book", "book.css")
    css = io.open(css_path, encoding="utf-8").read()
    if "/* ---- gen_web_book 追加 ---- */" not in css:
        css += CSS_ADDON
        io.open(css_path, "w", encoding="utf-8", newline="\n").write(css)
        print("book.css: 追加教程组件样式")


CSS_ADDON = """
/* ---- gen_web_book 追加 ---- */

.book-sec .diagram {
  background: rgba(9, 14, 26, 0.72);
  border: 1px solid var(--line);
  border-radius: var(--radius);
  padding: 14px 18px;
  margin: 14px 0;
  overflow-x: auto;
}
.diagram.dg-state .dg-row {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 4px 0;
  white-space: nowrap;
}
.diagram .dg-node {
  font-family: var(--font-mono);
  font-size: 12.5px;
  color: #6fe0f5;
  border: 1px solid rgba(111, 224, 245, 0.35);
  background: rgba(111, 224, 245, 0.06);
  border-radius: 999px;
  padding: 2px 12px;
}
.diagram .dg-arrow {
  font-size: 12px;
  color: var(--muted);
}
.diagram.dg-flow .dg-step {
  display: flex;
  align-items: baseline;
  gap: 12px;
  padding: 5px 0;
}
.diagram .dg-no {
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--blue);
  border: 1px solid rgba(91, 157, 255, 0.4);
  border-radius: 50%;
  width: 20px;
  height: 20px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  flex: none;
}
.diagram .dg-name {
  font-weight: 600;
  font-size: 13.5px;
  color: var(--text);
  white-space: nowrap;
}
.diagram .dg-desc {
  color: var(--soft);
  font-size: 13px;
}

.book-pit .pit-body { margin-top: 6px; }
.book-pit .pit-body p { color: var(--soft); font-size: 14px; margin: 8px 0; }

.code-pair {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  gap: 12px;
  margin: 12px 0;
}
.code-pair .cp-bad, .code-pair .cp-good {
  border-radius: 10px;
  overflow: hidden;
  border: 1px solid var(--line-soft);
}
.code-pair .cp-tag {
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 1px;
  padding: 4px 12px;
}
.code-pair .cp-bad .cp-tag { background: rgba(248, 113, 113, 0.12); color: var(--red); }
.code-pair .cp-good .cp-tag { background: rgba(61, 220, 151, 0.12); color: var(--green); }
.code-pair pre {
  margin: 0;
  padding: 12px 14px;
  background: #060a14;
  font-family: var(--font-mono);
  font-size: 12.5px;
  line-height: 1.7;
  color: #c9d6ee;
  overflow-x: auto;
}

.term-block .t-cmd {
  display: block;
  font-family: var(--font-mono);
  font-size: 12.5px;
  color: #6fe0f5;
}

.code-block .code-src {
  margin-left: auto;
  margin-right: 8px;
  font-size: 12px;
  color: var(--blue);
  text-decoration: none;
}
.code-block .code-src:hover { color: var(--cyan); }

.book-sec h4 {
  font-size: 15px;
  color: var(--text);
  margin: 18px 0 8px;
}
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default="D:/GIT/xrt")
    ap.add_argument("--wwwroot", default="D:/GIT/home/host/xrt/wwwroot")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("init-order")
    b = sub.add_parser("build")
    b.add_argument("--only")
    m = sub.add_parser("material")
    m.add_argument("slug")
    args = ap.parse_args()
    args.repo = args.repo.replace("\\", "/")
    args.wwwroot = args.wwwroot.replace("\\", "/")
    {"init-order": cmd_init_order, "build": cmd_build, "material": cmd_material}[args.cmd](args)


if __name__ == "__main__":
    main()
