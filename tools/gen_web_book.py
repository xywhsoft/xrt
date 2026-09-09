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
# en/ru 译文的 h2 节名 -> 内部中文节 key（I18N_SPEC 第 5 节结构对应）
SECTION_ALIAS = {
    "en": {"Orientation": "导读", "Introduction": "引入", "Concepts": "概念",
           "Examples": "示例", "Contracts": "契约", "Pitfalls": "避坑",
           "Exercises": "练习", "Cheat Sheet": "速查"},
    "ru": {"Ориентация": "导读", "Введение": "引入", "Понятия": "概念",
           "Примеры": "示例", "Контракты": "契约", "Ловушки": "避坑",
           "Упражнения": "练习", "Шпаргалка": "速查"},
}

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
                    for alias_map in SECTION_ALIAS.values():
                        if key in alias_map:
                            key = alias_map[key]
                            break
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

SITE = "https://xrt.xywhsoft.com"
LANG_NAMES = {"zh": "简体中文", "en": "English", "ru": "Русский"}
LANG_ATTR = {"zh": "zh-CN", "en": "en", "ru": "ru"}

# UI 字符串字典：zh 为基线，en/ru 覆盖
I18N = {
    "zh": {
        "site_title": "《XRT 程序设计》",
        "title_fmt": "第 {num} 章 {title} - 《XRT 程序设计》",
        "desc_fmt": "XRT 程序设计教程第 {num} 章{lead}",
        "skip": "跳到主要内容",
        "nav_arch": "产品架构", "nav_quality": "工程品质", "nav_perf": "性能设计",
        "nav_start": "快速开始", "nav_docs": "开发文档", "nav_repos": "源码仓库",
        "nav_toc": "程序设计教程", "nav_api": "API 参考与搜索",
        "nav_home": "XRT 首页", "nav_menu": "打开菜单", "nav_lang": "语言",
        "ch_progress": "第 {num} 章 / 共 {total} 章",
        "h1_fmt": "第 {num} 章 {title}",
        "intro": "本章导读", "mistakes": "常见错误", "exercises_t": "编程练习", "this_ch": "本章", "consolidate": "巩固",
        "prev": "上一章", "next": "下一章", "prev_fmt": "第 {num} 章 {title}", "next_fmt": "第 {num} 章 {title}",
        "api_title": "📖 本章涉及的 API 参考手册：", "api_hint": "点击查看完整签名、参数约束、返回值与错误说明",
        "api_count": "{name}（{cnt} 个条目）",
        "footer_desc": "力求卓越的互联网 + AI 时代跨平台 C 基础设施库——一整套成体系的基础设施库。",
        "f_site": "站点", "f_res": "资源", "f_home": "首页", "f_book": "书籍",
        "f_api": "API 概览", "f_repo": "Gitee 仓库", "f_issue": "问题反馈",
        "f_apientry": "API 参考入口",
        "summary": "小结",
    },
    "en": {
        "site_title": "XRT Programming",
        "title_fmt": "Chapter {num} {title} - XRT Programming",
        "desc_fmt": "XRT Programming tutorial, chapter {num}{lead}",
        "skip": "Skip to main content",
        "nav_arch": "Architecture", "nav_quality": "Engineering", "nav_perf": "Performance",
        "nav_start": "Quick Start", "nav_docs": "Docs", "nav_repos": "Repositories",
        "nav_toc": "Programming Tutorial", "nav_api": "API Reference & Search",
        "nav_home": "XRT home", "nav_menu": "Open menu", "nav_lang": "Language",
        "ch_progress": "Chapter {num} of {total}",
        "h1_fmt": "Chapter {num} {title}",
        "intro": "Chapter Orientation", "mistakes": "Common Mistakes", "exercises_t": "Programming Exercises", "this_ch": "On this page", "consolidate": "Practice",
        "prev": "Previous", "next": "Next", "prev_fmt": "Ch. {num} {title}", "next_fmt": "Ch. {num} {title}",
        "api_title": "📖 API references in this chapter:",
        "api_hint": "Click for full signatures, parameter constraints, return values and error notes",
        "api_count": "{name} ({cnt} entries)",
        "footer_desc": "A cross-platform C infrastructure library striving for excellence in the Internet + AI era.",
        "f_site": "Site", "f_res": "Resources", "f_home": "Home", "f_book": "Book",
        "f_api": "API overview", "f_repo": "Gitee repository", "f_issue": "Issues",
        "f_apientry": "API reference",
        "summary": "Summary",
    },
    "ru": {
        "site_title": "Программирование на XRT",
        "title_fmt": "Глава {num} {title} — Программирование на XRT",
        "desc_fmt": "Учебник по программированию XRT, глава {num}{lead}",
        "skip": "Перейти к основному содержанию",
        "nav_arch": "Архитектура", "nav_quality": "Качество", "nav_perf": "Производительность",
        "nav_start": "Быстрый старт", "nav_docs": "Документация", "nav_repos": "Репозитории",
        "nav_toc": "Учебник по программированию", "nav_api": "Справочник API и поиск",
        "nav_home": "Главная XRT", "nav_menu": "Открыть меню", "nav_lang": "Язык",
        "ch_progress": "Глава {num} из {total}",
        "h1_fmt": "Глава {num} {title}",
        "intro": "Ориентация главы", "mistakes": "Типичные ошибки", "exercises_t": "Практические задания", "this_ch": "На этой странице", "consolidate": "Практика",
        "prev": "Предыдущая", "next": "Следующая", "prev_fmt": "Гл. {num} {title}", "next_fmt": "Гл. {num} {title}",
        "api_title": "📖 Справочники API в этой главе:",
        "api_hint": "Полные сигнатуры, ограничения параметров, возвращаемые значения и ошибки",
        "api_count": "{name} ({cnt} записей)",
        "footer_desc": "Кроссплатформенная библиотека инфраструктуры C для эры интернета и ИИ.",
        "f_site": "Сайт", "f_res": "Ресурсы", "f_home": "Главная", "f_book": "Книга",
        "f_api": "Обзор API", "f_repo": "Репозиторий Gitee", "f_issue": "Вопросы",
        "f_apientry": "Справочник API",
        "summary": "Итоги",
    },
}


def translated_nums(repo, lang):
    """返回某语言已翻译的章号集合（依据 docs/book/<lang>/NN-*.md 存在）。"""
    nums = set()
    for f in glob.glob(os.path.join(repo, "docs", "book", lang, "*.md")):
        m = re.match(r"(\d+)-", os.path.basename(f))
        if m:
            nums.add(int(m.group(1)))
    return nums


def lang_switch_html(lang, entry=None, translated=None):
    """语言下拉。entry 为章节条目时按章生成三链接；否则跳各语言首页。
    目标语言章节已译则直链同语言页；未译则回退 zh 页并带 ?lang 触发提示条。"""
    translated = translated or {}
    items = []
    for lg in ("zh", "en", "ru"):
        name = LANG_NAMES[lg]
        if lg == lang:
            items.append('<span class="lang-cur">%s</span>' % name)
        elif entry is None:
            href = {"zh": "../../index.html", "en": "../../en/index.html",
                    "ru": "../../ru/index.html"}[lg]
            items.append('<a href="%s">%s</a>' % (href, name))
        elif lg == "zh":
            items.append('<a href="../../book/ch%s.html">%s</a>' % (entry["file"], name))
        elif entry["num"] in translated.get(lg, set()):
            if lang == "zh":
                items.append('<a href="../%s/book/ch%s.html">%s</a>' % (lg, entry["file"], name))
            else:
                items.append('<a href="ch%s.html">%s</a>' % (entry["file"], name))
        else:
            if lang == "zh":
                items.append('<a href="ch%s.html?lang=%s">%s</a>' % (entry["file"], lg, name))
            else:
                items.append('<a href="../../book/ch%s.html?lang=%s">%s</a>'
                             % (entry["file"], lg, name))
    return ('<li><details class="nav-dropdown nav-lang"><summary>%s</summary>'
            '<div class="nav-lang-menu">%s</div></details></li>'
            % (I18N[lang]["nav_lang"], "".join(items)))


def nav_html(lang, entry=None, translated=None):
    t = I18N[lang]
    if lang == "zh":
        logo = "../res/logo.png"
        home = "../index.html"
        rel, bench, start = "../reliability.html", "../benchmarks.html", "../start.html"
        book_idx, api = "index.html", "../api.html"
        q = ""
    else:
        logo = "../../res/logo.png"
        # 门面页 en（I2）/ru（I27）已上线：直链同语言门面；ref 页仍共用 zh（I28 决策 A）
        home = "../../%s/index.html" % lang
        rel, bench, start = ("../../%s/reliability.html" % lang, "../../%s/benchmarks.html" % lang,
                             "../../%s/start.html" % lang)
        book_idx, api = "index.html", "../../%s/api.html" % lang
    return ('<nav class="nav" aria-label="%s"><div class="nav-inner">'
            '<a href="%s" class="nav-logo" aria-label="%s"><img src="%s" alt="XRT" width="96" height="32"><em>v2.0</em></a>'
            '<button type="button" class="nav-toggle" aria-controls="site-nav" aria-expanded="false" aria-label="%s"><span></span></button>'
            '<ul class="nav-links" id="site-nav"><li><a href="%s#arch">%s</a></li>'
            '<li><a href="%s">%s</a></li><li><a href="%s">%s</a></li>'
            '<li><a href="%s">%s</a></li>'
            '<li><details class="nav-dropdown"><summary>%s</summary><div><a href="%s">%s</a><a href="%s">%s</a></div></details></li>'
            '<li><details class="nav-dropdown"><summary>%s</summary><div><a href="https://github.com/xywhsoft/xrt" target="_blank" rel="noopener">GitHub ↗</a><a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee ↗</a></div></details></li>'
            '%s'
            '</ul></div></nav>'
            % (t["skip"], home, t["nav_home"], logo, t["nav_menu"],
               home, t["nav_arch"], rel, t["nav_quality"], bench, t["nav_perf"],
               start, t["nav_start"],
               t["nav_docs"], book_idx, t["nav_toc"], api, t["nav_api"],
               t["nav_repos"],
               lang_switch_html(lang, entry)))


def footer_html(lang):
    t = I18N[lang]
    if lang == "zh":
        home, book_idx, api = "../index.html", "index.html", "../api.html"
    else:
        home = "../../%s/index.html" % lang
        book_idx, api = "index.html", "../../%s/api.html" % lang
    return ('<footer class="footer">\n  <div class="footer-inner">\n    <div>\n'
            '      <div class="f-brand"><img src="%s" alt="XRT" width="72" height="24"></div>\n'
            '      <p class="f-desc">%s</p>\n    </div>\n'
            '    <div>\n      <h3>%s</h3>\n      <ul>\n        <li><a href="%s">%s</a></li>\n'
            '        <li><a href="%s">%s</a></li>\n        <li><a href="%s">%s</a></li>\n      </ul>\n    </div>\n'
            '    <div>\n      <h3>%s</h3>\n      <ul>\n        <li><a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">%s</a></li>\n'
            '        <li><a href="https://gitee.com/xywhsoft/xrt/issues" target="_blank" rel="noopener">%s</a></li>\n'
            '        <li><a href="%s#reference">%s</a></li>\n      </ul>\n    </div>\n  </div>\n'
            '  <div class="footer-bottom">\n    XRT &copy; <span data-year>2026</span> xLeaves (xywhsoft) &middot; MIT License &middot; <a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee</a>\n  </div>\n</footer>'
            % ("../res/logo.png" if lang == "zh" else "../../res/logo.png",
               t["footer_desc"],
               t["f_site"], home, t["f_home"], book_idx, t["f_book"], api, t["f_api"],
               t["f_res"], t["f_repo"], t["f_issue"], api, t["f_apientry"]))


# 兼容旧引用（zh 缺省）
NAV_HTML = nav_html("zh")
FOOTER_HTML = footer_html("zh")


def build_page(ch, entry, order, known_types, repo, api_counts, total_chapters,
               lang="zh", translated=None):
    fm = ch.fm
    num = int(fm["num"])
    t = I18N[lang]
    translated = translated or {}
    items = list(order)
    idx = next(i for i, it in enumerate(items) if it["num"] == num)
    prev_it = items[idx - 1] if idx > 0 else None
    next_it = items[idx + 1] if idx + 1 < len(items) else None

    def nav_href(it):
        """章节内导航：目标章在本语言已译直链；未译回退 zh + ?lang 提示条。"""
        if it is None:
            return None
        if lang == "zh" or it["num"] in translated.get(lang, set()):
            return "ch%s.html" % it["file"]
        return "../../book/ch%s.html?lang=%s" % (it["file"], lang)

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
            body.append('<section class="book-sec" id="intro"><h3>%s</h3>%s</section>'
                        % (t["intro"], render_blocks(blocks, order, known_types, repo)))
            side_cur.append('<li><a href="#intro">%s</a></li>' % t["intro"])
            continue
        if key in NUMBERED:
            sec_no += 1
            sid = "sec%d" % sec_no
            body.append('<section class="book-sec" id="%s"><h3><span class="sec-no">%d.%d</span>%s</h3>%s</section>'
                        % (sid, num, sec_no, key, render_blocks(blocks, order, known_types, repo)))
            side_cur.append('<li><a href="#%s">%d.%d %s</a></li>' % (sid, num, sec_no, key))
            continue
        title = (t["intro"] if key == "导读" else
                 t["mistakes"] if key == "避坑" else
                 t["exercises_t"] if key == "练习" else
                 t["summary"] if key == "速查" else key)
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
        label = t["api_count"].format(name=name, cnt=(str(cnt) if cnt is not None else "?"))
        if lang == "zh":
            ref_pre, ref_q = "", ""
        else:
            ref_pre, ref_q = "../../book/", "?lang=%s" % lang
        api_links.append('<a href="%sref-%s.html%s" style="display:inline-block;margin:2px 6px;padding:3px 12px;'
                         'border-radius:6px;background:rgba(91,157,255,.1);color:#5b9dff;'
                         'text-decoration:none;font-size:13px;">%s</a>' % (ref_pre, e(name), ref_q, e(label)))
    api_box = ('<div style="margin:28px 0;padding:16px 20px;border:1px solid rgba(91,157,255,.15);'
               'border-radius:10px;background:rgba(91,157,255,.04);">'
               '<p style="margin:0 0 10px;font-size:14px;color:var(--soft);">'
               '<strong>%s</strong>%s</p>'
               '<p style="margin:10px 0 0;font-size:12px;color:var(--muted);">'
               '%s</p></div>' % (t["api_title"], "".join(api_links), t["api_hint"]))

    prev_html = ('<a href="%s" class="prev"><span class="pn-dir">%s</span>'
                 '<span class="pn-title">%s</span></a>' % (nav_href(prev_it), t["prev"], e(t["prev_fmt"].format(num=prev_it["num"], title=prev_it["title"])))
                 if prev_it else "<span></span>")
    next_html = ('<a href="%s" class="next"><span class="pn-dir">%s</span>'
                 '<span class="pn-title">%s</span></a>' % (nav_href(next_it), t["next"], e(t["next_fmt"].format(num=next_it["num"], title=next_it["title"])))
                 if next_it else "<span></span>")

    side = ('<aside class="book-side"><h4>%s</h4><ul>%s</ul>'
            '<h4>%s</h4><ul>%s</ul></aside>'
            % (t["this_ch"], "".join(side_cur), t["consolidate"],
               "".join(side_fix) or "<li><a href=\"#summary\">%s</a></li>" % t["summary"]))

    up = ".." if lang == "zh" else "../.."
    canonical = "%s/%sbook/ch%s.html" % (SITE, (lang + "/") if lang != "zh" else "", entry["file"])
    alts = ['<link rel="alternate" hreflang="%s" href="%s/book/ch%s.html">' % (LANG_ATTR["zh"], SITE, entry["file"])]
    for lg in ("en", "ru"):
        if num in translated.get(lg, set()):
            alts.append('<link rel="alternate" hreflang="%s" href="%s/%s/book/ch%s.html">' % (LANG_ATTR[lg], SITE, lg, entry["file"]))
    alts.append('<link rel="alternate" hreflang="x-default" href="%s/book/ch%s.html">' % (SITE, entry["file"]))
    alternates = (chr(10) + "  ").join(alts)
    pagetitle = t["title_fmt"].format(num=num, title=e(fm["title"]))
    desc = t["desc_fmt"].format(num=num, lead=e(fm["lead"]))
    progress = t["ch_progress"].format(num=num, total=total_chapters)
    h1 = t["h1_fmt"].format(num=num, title=e(fm["title"]))

    tpl = """<!DOCTYPE html>
<html lang="{htmlang}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{pagetitle}</title>
  <meta name="description" content="{desc}">
  <link rel="canonical" href="{canonical}">
  {alternates}
  <link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='8' fill='%235b9dff'/%3E%3C/text%3E%3C/svg%3E">
  <link rel="stylesheet" href="{up}/style.css?v=i18n001">
  <link rel="stylesheet" href="book.css?v=730594b9cc">
  <link rel="stylesheet" href="{up}/refinement.css?v=ad12773fdb">
</head>
<body>
<a class="skip-link" href="#main">{skiplink}</a>
<div class="bg-scene" aria-hidden="true"></div>
{nav}
<header class="page-header">
  <div class="inner">
    <div class="ch-meta">
      <span class="ch-progress">{progress}</span>
      <span class="ch-vol">{volume}</span>
    </div>
    <h1>{h1}</h1>
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
<script src="{up}/script.js?v=i18n005" defer></script>
</body>
</html>
"""
    return (tpl
            .replace("{htmlang}", LANG_ATTR[lang])
            .replace("{pagetitle}", pagetitle)
            .replace("{desc}", desc)
            .replace("{canonical}", canonical)
            .replace("{alternates}", alternates)
            .replace("{up}", up)
            .replace("{skiplink}", t["skip"])
            .replace("{num}", str(num))
            .replace("{file}", entry["file"])
            .replace("{title}", e(fm["title"]))
            .replace("{lead}", e(fm["lead"]))
            .replace("{volume}", e(fm["volume"]))
            .replace("{total}", str(total_chapters))
            .replace("{progress}", progress)
            .replace("{h1}", h1)
            .replace("{nav}", nav_html(lang, entry, translated))
            .replace("{side}", side)
            .replace("{body}", "\n".join(body))
            .replace("{prev}", prev_html)
            .replace("{next}", next_html)
            .replace("{apibox}", api_box)
            .replace("{footer}", footer_html(lang)))


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

def cmd_sync_index(args):
    """按 order.json 同步 wwwroot/book/index.html：整卷重建章节列表（章号/顺序/徽章/缺失条目/卷范围）。"""
    order = load_order(args.repo)
    done = set()
    for f in glob.glob(os.path.join(args.repo, "docs", "book", "*.md")):
        m = re.match(r"^(\d+)-(.+)\.md$", os.path.basename(f))
        if m:
            done.add(m.group(2))
    wip = set(x.strip() for x in args.wip.split(",")) if args.wip else set()

    def badge_for(slug):
        if slug in done:
            return "done", "已完成"
        if slug in wip:
            return "wip", "重写中"
        return "plan", "规划中"

    path = os.path.join(args.wwwroot, "book", "index.html")
    txt = io.open(path, encoding="utf-8").read()

    li_re = re.compile(
        r'<li class="book-ch ([\w-]+)"><a href="ch([\w-]+)\.html"><span class="ch-no">\d+</span>'
        r'<span class="ch-title">[^<]*</span><span class="badge badge-[\w-]+">[^<]*</span></a></li>')

    def vol_token(s):
        m = re.match(r"(卷[一二三四五六七八九十]+)", s)
        return m.group(1) if m else None

    def fix_vol(m):
        head = m.group(1)
        vol_key = re.search(r"<h3>(卷[一二三四五六七八九十]+)", head)
        if not vol_key:
            return m.group(0)
        key = vol_key.group(1)
        items = [it for it in order if vol_token(it["volume"]) == key]
        nums = [it["num"] for it in items]
        lis = []
        for it in items:
            cls, label = badge_for(it["slug"])
            lis.append('<li class="book-ch %s"><a href="ch%s.html"><span class="ch-no">%d</span>'
                       '<span class="ch-title">%s</span><span class="badge badge-%s">%s</span></a></li>'
                       % (cls, it["file"], it["num"], it["title"], cls, label))
        head = re.sub(r"第\s*[\d–-]+\s*章", "第 %d–%d 章" % (min(nums), max(nums)), head)
        return head + "\n      " + "\n      ".join(lis) + "\n    </ul>"

    txt = re.sub(r'(<div class="book-vol-head">.*?</div>\s*<ul class="book-ch-list">)(.*?)</ul>',
                 lambda m: fix_vol(m), txt, flags=re.S)
    io.open(path, "w", encoding="utf-8", newline="\n").write(txt)
    print("index.html 已同步（各卷列表按 order.json 重建）")


def cmd_build(args):
    repo = args.repo
    www = args.wwwroot
    lang = getattr(args, "lang", "zh") or "zh"
    order = load_order(repo)
    translated = {"en": translated_nums(repo, "en"), "ru": translated_nums(repo, "ru")}
    src_dir = os.path.join(repo, "docs", "book") if lang == "zh" else os.path.join(repo, "docs", "book", lang)
    out_dir = os.path.join(www, "book") if lang == "zh" else os.path.join(www, lang, "book")
    # md 源覆盖 order 条目
    sources = {}
    for f in sorted(glob.glob(os.path.join(src_dir, "*.md"))):
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
        page = build_page(ch, entry, order, known_types, repo, api_counts, total,
                          lang=lang, translated=translated)
        out = os.path.join(out_dir, "ch%s.html" % entry["file"])
        io.open(out, "w", encoding="utf-8", newline="\n").write(page)
        print("生成 ch%s.html" % entry["file"])

    if lang != "zh":
        # 资源镜像：en/ru 站点引用 {up}/style.css 等同级资源
        lang_root = os.path.join(www, lang)
        book_dir = os.path.join(lang_root, "book")
        if not os.path.isdir(book_dir):
            os.makedirs(book_dir)
        for name, dest in [("style.css", "style.css"), ("refinement.css", "refinement.css"),
                           ("script.js", "script.js"),
                           (os.path.join("book", "book.css"), os.path.join("book", "book.css"))]:
            srcf = os.path.join(www, name)
            dstf = os.path.join(lang_root, dest)
            if os.path.exists(srcf) and (
                    not os.path.exists(dstf)
                    or os.path.getmtime(srcf) > os.path.getmtime(dstf)):
                io.open(dstf, "w", encoding="utf-8", newline="").write(
                    io.open(srcf, encoding="utf-8", newline="").read())
        res_src = os.path.join(www, "res")
        res_dst = os.path.join(lang_root, "res")
        if os.path.isdir(res_src) and not os.path.isdir(res_dst):
            import shutil
            shutil.copytree(res_src, res_dst)
        return

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
    b.add_argument("--lang", default="zh", choices=["zh", "en", "ru"])
    b.add_argument("--only")
    s = sub.add_parser("sync-index")
    s.add_argument("--wip", help="逗号分隔的重写中 slug 列表")
    m = sub.add_parser("material")
    m.add_argument("slug")
    args = ap.parse_args()
    args.repo = args.repo.replace("\\", "/")
    args.wwwroot = args.wwwroot.replace("\\", "/")
    {"init-order": cmd_init_order, "build": cmd_build, "material": cmd_material,
     "sync-index": cmd_sync_index}[args.cmd](args)


if __name__ == "__main__":
    main()
