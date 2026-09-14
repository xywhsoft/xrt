#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 docs/api/*.md 生成官网 API 参考页与搜索索引。

输入:
  <repo>/docs/api/*.md          主题文档（结构化 markdown，DOC_SPEC v1.4）
  <repo>/config/modules.json    模块注册表（public_headers -> docs 映射）
  <repo>/include/xrt/*.h        公共头（符号归属 + 未文档化符号的声明文本）

输出（--wwwroot 指向官网目录）:
  wwwroot/book/ref-<hdr>.html   91 个内核参考页（原文件覆盖，扩展库页面不动）
  wwwroot/search-index.json     内核条目重建，扩展库条目原样保留，按模块名全局排序
  wwwroot/api.html              目录页计数（卡片数量 / 汇总行 / meta 描述）
  wwwroot/book/book.css         追加生成页所需的样式块（幂等）

用法:
  python tools/gen_web_api.py --repo D:/GIT/xrt --wwwroot D:/GIT/home/host/xrt/wwwroot
  python tools/gen_web_api.py ... --only xid,tls_client    # 只重生成指定页
  python tools/gen_web_api.py ... --dry-run                # 只打印报告
"""

import argparse
import glob
import html as html_mod
import io
import json
import os
import re
import sys

GITEE_BLOB = "https://gitee.com/xywhsoft/xrt/blob/master/"
EXT_PREFIXES = ("xhttp", "xws", "xruntime", "xmail", "xssh")

# 视为“无主题分组”的标题：其下的符号不显示组标题
GENERIC_HEADINGS = {
    "函数", "类型", "类型与常量", "常量", "常量与类型",
    "公开类型与常量", "公共类型与常量", "公共类型索引", "公共符号",
    "示例", "完整示例", "范例", "使用示例", "快速开始",
    "测试与示例", "示例与测试", "示例与回归", "范例与回归", "范例与测试",
    "API", "API 索引", "API 速览", "API 参考", "公共契约",
    "辅助函数索引", "错误总表", "常量总表", "版本与公共宏", "裁剪与依赖",
    # en / ru 同义标题（翻译文档同样不显示组标题）
    "Functions", "Types", "Types and constants", "Constants", "Constants and types",
    "Examples", "Complete example", "Usage examples", "Quick start",
    "API", "API index", "API overview", "Error summary", "Constant summary",
    "Trimming and dependencies",
    "Функции", "Типы", "Типы и константы", "Константы", "Константы и типы",
    "Примеры", "Полный пример", "Примеры использования", "Быстрый старт",
    "Сводка ошибок", "Сводка констант", "Обрезка и зависимости",
}

KIND_LABEL = {"fn": "函数", "type": "类型", "const": "常量"}

# h4 子段键的多语言规范化: 解析时统一收成 canonical 键，渲染时按语言出标签
SUBKEY_CANON = {
    "参数": "param", "Parameters": "param", "Параметры": "param",
    "返回值": "ret", "Return value": "ret", "Возвращаемое значение": "ret",
    "错误": "err", "Errors": "err", "Ошибки": "err",
    "范例": "example", "Example": "example", "Пример": "example",
}

TRIM_H2_PREFIXES = ("裁剪", "Trimming", "Обрезка")


def h4_key(text):
    """h4 子段键: 冒号前缀（全角/半角）+ 多语言规范化。"""
    key = text
    for sep in ("：", ":"):
        if sep in key:
            key = key.split(sep, 1)[0]
            break
    return key.strip()


def subkey_canon(text):
    return SUBKEY_CANON.get(h4_key(text), h4_key(text))


def search_text(sec):
    """搜索索引用一句话描述: 取第一段散文，去掉行内标记。"""
    for k, t in sec.desc:
        if k == "text" and t.strip() and not t.strip().startswith(("|", "- ", "#")):
            s = t.strip()
            s = CODE_SPAN_RE.sub(r"\1", s)
            s = BOLD_RE.sub(r"\1", s)
            return s[:120]
    return ""


def contract_h2_key(text):
    """识别契约类 h2: 严格形式 模块契约：X，或短标题含 所有权/线程/错误 的变体（含 en/ru 等价词）。"""
    if text.startswith("模块契约"):
        return text.split("：", 1)[-1].strip() if "：" in text else "总则"
    if text.startswith("Module contract"):
        return text.split(":", 1)[-1].strip() if ":" in text else "总则"
    if text.startswith("Контракт модуля"):
        return text.split(":", 1)[-1].strip() if ":" in text else "总则"
    variants = (
        (r"(所有权|线程|错误)", r"(函数|类型|常量|示例|范例)", 10),
        (r"(ownership|thread|error)", r"(function|type|constant|example)", 34),
        (r"(владени|поток|ошибк)", r"(функци|тип|констант|пример)", 30),
    )
    for kw, ex, maxlen in variants:
        if len(text) <= maxlen and re.search(kw, text, re.I) and not re.search(ex, text, re.I):
            return text
    return None


def e(s):
    return html_mod.escape(s, quote=True)


def norm_ws(s):
    return " ".join(s.split())


# ---------------------------------------------------------------------------
# 公共头扫描: 符号 -> 头文件 / 种类 / 声明文本
# ---------------------------------------------------------------------------

def strip_c_comments(src):
    """移除 /* */ 与 // 注释，保持行号（换行数不变）。"""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in src[i:j]))
            i = j
        elif c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


TYPE_KEYWORDS = {
    "typedef", "struct", "enum", "union", "const", "unsigned", "signed",
    "void", "char", "int", "long", "short", "float", "double", "volatile",
    "static", "inline", "extern", "register", "bool", "size_t",
}

IDENT_RE = re.compile(r"[A-Za-z_]\w*")
FN_RE = re.compile(r"\b(xrt[A-Z][A-Za-z0-9_]*)\s*\(")
ENUM_RE = re.compile(r"enum\s+(?:[A-Za-z_]\w*\s*)?\{([^{}]*)\}")
FNPTR_RE = re.compile(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)")
DEFINE_RE = re.compile(r"#\s*define\s+([A-Za-z_]\w*)")


def split_statements(src):
    """把去注释后的源码切为 (is_pre, text)：预处理指令整体一段，普通语句以 depth==0 的 ';' 结束。"""
    parts = []
    cur, cur_pre, depth = [], None, 0
    for line in src.split("\n"):
        stripped = line.strip()
        if cur_pre is None and depth == 0 and stripped.startswith("#"):
            cur_pre = True
        if cur_pre is not None:
            cur.append(line)
            if stripped.endswith("\\"):
                continue
            parts.append((True, "\n".join(cur)))
            cur, cur_pre = [], None
            continue
        cur.append(line)
        depth += line.count("{") - line.count("}")
        if depth <= 0 and (";" in line or "}" in line):
            # ';' 结束普通声明；'}' 结束函数体/结构体（无分号）
            parts.append((False, "\n".join(cur)))
            cur, depth = [], 0
        elif depth < 0:
            depth = 0
    if cur:
        parts.append((cur_pre is not None, "\n".join(cur)))
    return parts


def scan_header(path):
    """返回 {name: (kind, decl_text)}，kind in fn/type/const。"""
    src = strip_c_comments(io.open(path, encoding="utf-8", errors="replace").read())
    syms = {}
    for is_pre, text in split_statements(src):
        flat = norm_ws(text)
        if is_pre:
            m = DEFINE_RE.match(flat)
            if m:
                name = m.group(1)
                # 排除 include guard 与裁剪宏（裁剪信息在契约卡里单独呈现）
                if not name.endswith("_H") and not name.startswith("XRT_FEATURE_"):
                    syms.setdefault(name, ("const", flat))
            continue
        if "typedef" in flat:
            m = FNPTR_RE.search(flat)
            if m:
                syms.setdefault(m.group(1), ("type", flat))
                continue
            name = None
            for tok in reversed(IDENT_RE.findall(flat)):
                if tok not in TYPE_KEYWORDS:
                    name = tok
                    break
            if name:
                syms.setdefault(name, ("type", flat))
            continue
        m = FN_RE.search(flat)
        if m and (flat.endswith(";") or flat.rstrip().endswith("}")) and "typedef" not in flat:
            syms.setdefault(m.group(1), ("fn", flat))
    # 枚举常量: 在完整源码上扫（枚举体可能横跨多条“语句”）
    for m in ENUM_RE.finditer(src):
        for item in m.group(1).split(","):
            item = item.strip()
            if not item:
                continue
            im = IDENT_RE.match(item)
            if im and im.group(0) not in TYPE_KEYWORDS:
                syms.setdefault(im.group(0), ("const", im.group(0)))
    return syms


def scan_all_headers(repo):
    """返回 (sym_map, hdr_syms, hdr_srcs):
    sym_map={name: [hdr,...]}, hdr_syms={hdr: {name:(kind,decl)}}, hdr_srcs={hdr: 去注释源码}"""
    hdr_syms = {}
    hdr_srcs = {}
    paths = sorted(glob.glob(os.path.join(repo, "include", "xrt", "*.h")))
    paths += sorted(glob.glob(os.path.join(repo, "extlibs", "*", "include",
                                           "xrt", "*.h")))
    for path in paths:
        base = os.path.basename(path)[:-2]
        hdr_srcs[base] = strip_c_comments(io.open(path, encoding="utf-8", errors="replace").read())
        hdr_syms[base] = scan_header(path)
    sym_map = {}
    for hdr, syms in hdr_syms.items():
        for name in syms:
            sym_map.setdefault(name, []).append(hdr)
    return sym_map, hdr_syms, hdr_srcs


# ---------------------------------------------------------------------------
# docs/api/*.md 解析
# ---------------------------------------------------------------------------

H_RE = re.compile(r"^(#{1,4})\s+(.*?)\s*$")
SUBJECTS = ("参数", "返回值", "错误", "范例")


class Section(object):
    __slots__ = ("name", "group", "desc", "sig", "subs", "order")

    def __init__(self, name, group):
        self.name = name
        self.group = group
        self.desc = []          # 顶层散文行（含 ```c 签名块占位）
        self.sig = None         # 第一个顶层代码块 = 签名
        self.subs = {}          # 参数/返回值/错误/范例/其他 -> 行列表
        self.order = []         # 子标题出现顺序（渲染保序）

    def sub(self, key):
        if key not in self.subs:
            self.subs[key] = []
            self.order.append(key)
        return self.subs[key]


class Doc(object):
    __slots__ = ("name", "title", "intro", "sections", "contracts", "trim", "prose")

    def __init__(self, name):
        self.name = name                # 文件基名（不含 .md）
        self.title = name
        self.intro = []
        self.sections = []
        self.contracts = {}             # 错误/线程/所有权 -> 行
        self.trim = []                  # 裁剪与依赖 原始行
        self.prose = []                 # 非符号 h2 散文档 [(kind, title, lines)]，kind: prose|trim


def parse_doc(path):
    doc = Doc(os.path.basename(path)[:-3])
    cur_h2 = None          # 最近的主题标题（h2 或非符号 h3）
    cur_h2_generic = False
    cur_sec = None
    cur_sub = None
    prose_open = None      # doc.prose 中当前打开的散文档下标
    in_code = False
    code_buf = []
    code_target = None     # ('intro'|'sec-top'|sub-key) 当前代码块归属

    def flush_code():
        text = "\n".join(code_buf)
        if code_target == "intro":
            doc.intro.append(("code", text))
        elif code_target == "sec-top":
            if cur_sec is not None and cur_sec.sig is None:
                cur_sec.sig = text
        elif isinstance(code_target, tuple) and code_target[0] == "__prose__":
            if prose_open is None:
                doc.prose.append(("prose", code_target[1], []))
            doc.prose[-1][2].append(("code", text))
        elif code_target is not None and cur_sec is not None:
            cur_sec.sub(code_target).append(("code", text))

    for raw in io.open(path, encoding="utf-8"):
        line = raw.rstrip("\n")
        if line.startswith("```"):
            if in_code:
                flush_code()
                in_code, code_buf, code_target = False, [], None
            else:
                in_code, code_buf = True, []
                if cur_sec is None:
                    code_target = "intro"
                elif cur_sub is None:
                    code_target = "sec-top"
                else:
                    code_target = cur_sub
            continue
        if in_code:
            code_buf.append(line)
            continue
        m = H_RE.match(line)
        if m:
            lvl, text = len(m.group(1)), m.group(2).strip()
            if lvl == 1:
                doc.title = text
                cur_sec, cur_sub = None, None
                prose_open = None
            elif lvl == 2:
                cur_sec, cur_sub = None, None
                ckey = contract_h2_key(text)
                if ckey is not None:
                    cur_h2, cur_h2_generic = ckey, True
                    doc.contracts.setdefault(ckey, [])
                    cur_sub = ("__contract__", ckey)
                elif text.startswith(TRIM_H2_PREFIXES):
                    cur_h2, cur_h2_generic = text, True
                    cur_sub = ("__trim__",)
                    doc.prose.append(("trim", text, []))
                    prose_open = None
                else:
                    cur_h2, cur_h2_generic = text, text in GENERIC_HEADINGS
                    # h2 下、首个符号 h3 前的散文/表格收为全宽散文档（doc.prose），
                    # 修复“稳定契约/设计契约/常量表”等内容被静默丢弃的问题；
                    # 后续符号 h3 会把 cur_sub 复位，分组成员逻辑不变。
                    cur_sub = ("__prose__", text)
                    prose_open = None
            elif lvl == 3:
                name = text.strip("`").strip()
                if text.startswith("`") and name and IDENT_RE.fullmatch(name):
                    cur_sec = Section(name, None if cur_h2_generic else cur_h2)
                    cur_sub = None
                    doc.sections.append(cur_sec)
                else:
                    # 非符号小节标题: 作为更细的分组
                    cur_sec = None
                    cur_sub = None
                    prose_open = None
                    cur_h2, cur_h2_generic = text, text in GENERIC_HEADINGS
            else:  # h4
                prose_open = None
                if cur_sec is None:
                    continue
                key = subkey_canon(text)
                cur_sub = key
                cur_sec.sub(key)
            continue
        # 普通行
        if isinstance(cur_sub, tuple):
            if cur_sub[0] == "__contract__":
                doc.contracts.setdefault(cur_sub[1], []).append(("text", line))
            elif cur_sub[0] == "__prose__":
                if prose_open is None and line.strip():
                    doc.prose.append(("prose", cur_sub[1], []))
                    prose_open = len(doc.prose) - 1
                if prose_open is not None:
                    doc.prose[prose_open][2].append(("text", line))
            else:
                doc.trim.append(("text", line))
        elif cur_sec is None:
            if line.strip() or doc.intro:
                doc.intro.append(("text", line))
        elif cur_sub is None:
            cur_sec.desc.append(("text", line))
        else:
            cur_sec.sub(cur_sub).append(("text", line))
    if in_code and code_target:
        flush_code()
    return doc


# ---------------------------------------------------------------------------
# markdown 片段 -> HTML
# ---------------------------------------------------------------------------

LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)\s]+)\)")
CODE_SPAN_RE = re.compile(r"`([^`]+)`")
BOLD_RE = re.compile(r"\*\*([^*]+)\*\*")


def map_url(url, pages):
    if url.startswith(("http://", "https://", "#", "mailto:")):
        return url
    if url.endswith(".md"):
        name = os.path.basename(url)[:-3]
        if name in pages:
            return "ref-%s.html" % name
        return GITEE_BLOB + "docs/api/" + os.path.basename(url)
    rel = url
    for _ in range(3):
        if rel.startswith("../"):
            rel = rel[3:]
        else:
            break
    return GITEE_BLOB + rel


def inline_md(s, pages):
    s = e(s)
    def link_repl(m):
        text, url = m.group(1), m.group(2)
        return '<a href="%s">%s</a>' % (e(map_url(url, pages)), CODE_SPAN_RE.sub(r"<code>\1</code>", text))
    s = LINK_RE.sub(link_repl, s)
    s = CODE_SPAN_RE.sub(r"<code>\1</code>", s)
    s = BOLD_RE.sub(r"<b>\1</b>", s)
    return s


def render_md_lines(lines, pages):
    """行列表 [(kind,text)] -> html; 代码块 -> pre, 表格 -> table, 其余 -> p/ul。"""
    out = []
    i = 0
    n = len(lines)
    while i < n:
        kind, text = lines[i]
        if kind == "code":
            out.append('<pre class="code">%s</pre>' % e(text))
            i += 1
            continue
        if text.strip().startswith("|"):
            tbl = []
            while i < n and lines[i][1].strip().startswith("|"):
                tbl.append(lines[i][1].strip())
                i += 1
            out.append(render_table(tbl, pages))
            continue
        if text.strip().startswith("- "):
            items = []
            while i < n and lines[i][1].strip().startswith("- "):
                items.append(lines[i][1].strip()[2:])
                i += 1
            out.append("<ul>%s</ul>" % "".join(
                "<li>%s</li>" % inline_md(x, pages) for x in items if x))
            continue
        if not text.strip():
            i += 1
            continue
        para = [text.strip()]
        i += 1
        while (i < n and lines[i][0] == "text" and lines[i][1].strip()
               and not lines[i][1].strip().startswith(("|", "- "))
               and not lines[i][1].startswith("#")):
            para.append(lines[i][1].strip())
            i += 1
        out.append("<p>%s</p>" % inline_md(" ".join(para), pages))
    return "".join(out)


def render_table(rows, pages, cls="api-ref-params"):
    cells = []
    for r in rows:
        r = r.strip()
        if r.startswith("|"):
            r = r[1:]
        if r.endswith("|"):
            r = r[:-1]
        cells.append([c.strip() for c in r.split("|")])
    if len(cells) >= 2 and all(re.fullmatch(r":?-{2,}:?", c) for c in cells[1] if c):
        header, body = cells[0], cells[2:]
    else:
        header, body = None, cells
    out = ['<table class="%s">' % cls]
    if header:
        out.append("<tr>%s</tr>" % "".join("<th>%s</th>" % inline_md(c, pages) for c in header))
    for row in body:
        out.append("<tr>%s</tr>" % "".join("<td>%s</td>" % inline_md(c, pages) for c in row))
    out.append("</table>")
    return "".join(out)


def highlight_sig(sig):
    sig = norm_ws(sig)
    m = re.match(r"^(.*?)\s*\b([A-Za-z_]\w*)\s*\(", sig)
    if not m or not m.group(2):
        return e(sig)
    ret, fn = m.group(1).strip(), m.group(2)
    close = sig.rfind(")")
    inner = sig[m.end():close] if close > m.end() else ""
    parts, depth, buf = [], 0, []
    for ch in inner:
        if ch in "(<":
            depth += 1
        elif ch in ")>":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append("".join(buf))
            buf = []
        else:
            buf.append(ch)
    if buf and "".join(buf).strip():
        parts.append("".join(buf))
    rendered = []
    for p in parts:
        p = p.strip()
        if p == "void" or not p:
            continue
        pm = re.match(r"^(.*?)\s*\b([A-Za-z_]\w*)$", p)
        if pm:
            rendered.append('<span class="sig-type">%s</span> <span class="sig-param">%s</span>'
                            % (e(pm.group(1).strip()), e(pm.group(2))))
        else:
            rendered.append('<span class="sig-type">%s</span>' % e(p))
    ret_html = ('<span class="sig-type">%s</span> ' % e(ret)) if ret else ""
    return '%s<span class="sig-fn">%s</span>(%s)' % (ret_html, e(fn), ", ".join(rendered))


# ---------------------------------------------------------------------------
# 页面组装与渲染
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 多语言界面（zh/en/ru）：标签、导航、页脚与复数
# ---------------------------------------------------------------------------

L10N = {
    "zh": {
        "html_lang": "zh-CN", "title_suffix": "API 参考 - XRT 程序设计",
        "skip": "跳到主要内容", "nav_aria": "主导航", "nav_home": "XRT 首页", "nav_menu": "打开菜单",
        "nav_arch": "产品架构", "nav_rel": "工程品质", "nav_bench": "性能设计", "nav_start": "快速开始",
        "nav_docs": "开发文档", "nav_book": "程序设计教程", "nav_api": "API 参考与搜索",
        "nav_repo": "源码仓库", "nav_lang": "语言",
        "kinds": {"fn": "函数", "type": "类型", "const": "常量"},
        "sub": {"param": "参数", "ret": "返回值", "err": "错误", "example": "范例"},
        "example_tag": "已编译验证",
        "badge_empty": "0 个 API",
        "meta_desc": "%s 模块 API 参考：%s——签名、参数约束、返回值、错误与已编译范例。",
        "contract_title": "模块契约", "trim_title": "裁剪与依赖",
        "fallback_desc_link": "公共头文件",
        "fallback_desc_fmt": '契约注释见<a href="%s" target="_blank" rel="noopener">%s</a>。',
        "fallback_group": "补充参考 · 契约见公共头注释",
        "fallback_search_text": "契约注释见公共头文件。",
        "constindex_title": "常量索引",
        "constindex_desc": '以下常量在本页所属头文件中定义，语义见<a href="%s">公共头注释</a>。',
        "constindex_th": ("常量", "说明"), "constindex_td": "见头文件注释",
        "aside_list": "函数列表", "aside_ph": "搜索函数…", "aside_aria": "搜索 API",
        "aside_back": "← 返回目录",
        "footer_desc": "力求卓越的互联网 + AI 时代跨平台 C 基础设施库——一整套成体系的基础设施库。",
        "footer_site": "站点", "footer_home": "首页", "footer_book": "书籍", "footer_apiover": "API 概览",
        "footer_res": "资源", "footer_repo": "Gitee 仓库", "footer_issues": "问题反馈",
        "footer_entry": "API 参考入口",
    },
    "en": {
        "html_lang": "en", "title_suffix": "API Reference - XRT",
        "skip": "Skip to main content", "nav_aria": "Main navigation", "nav_home": "XRT home", "nav_menu": "Open menu",
        "nav_arch": "Architecture", "nav_rel": "Engineering", "nav_bench": "Performance", "nav_start": "Quick Start",
        "nav_docs": "Documentation", "nav_book": "Programming tutorial", "nav_api": "API reference & search",
        "nav_repo": "Source repositories", "nav_lang": "Language",
        "kinds": {"fn": "function", "type": "type", "const": "constant"},
        "sub": {"param": "Parameters", "ret": "Return value", "err": "Errors", "example": "Example"},
        "example_tag": "Compile-verified",
        "badge_empty": "0 APIs",
        "meta_desc": "%s module API reference: %s — signatures, parameter constraints, return values, errors, and compile-verified examples.",
        "contract_title": "Module contract", "trim_title": "Trimming and dependencies",
        "fallback_desc_link": "public header",
        "fallback_desc_fmt": 'Contract comments are in the <a href="%s" target="_blank" rel="noopener">%s</a>.',
        "fallback_group": "Supplementary reference · contracts in public headers",
        "fallback_search_text": "Contract comments are in the public header.",
        "constindex_title": "Constant index",
        "constindex_desc": 'The following constants are defined in this page\'s header; semantics are documented in the <a href="%s">public header comments</a>.',
        "constindex_th": ("Constant", "Description"), "constindex_td": "See header comments",
        "aside_list": "Function list", "aside_ph": "Search functions…", "aside_aria": "Search API",
        "aside_back": "← Back to index",
        "footer_desc": "A cross-platform C infrastructure library striving for excellence in the Internet + AI era — a complete, systematic foundation.",
        "footer_site": "Site", "footer_home": "Home", "footer_book": "Book", "footer_apiover": "API overview",
        "footer_res": "Resources", "footer_repo": "Gitee repository", "footer_issues": "Issues",
        "footer_entry": "API reference entry",
    },
    "ru": {
        "html_lang": "ru", "title_suffix": "Справочник API - XRT",
        "skip": "Перейти к основному содержимому", "nav_aria": "Главная навигация", "nav_home": "Главная XRT", "nav_menu": "Открыть меню",
        "nav_arch": "Архитектура", "nav_rel": "Инженерия", "nav_bench": "Производительность", "nav_start": "Быстрый старт",
        "nav_docs": "Документация", "nav_book": "Учебник по программированию", "nav_api": "Справочник API и поиск",
        "nav_repo": "Репозитории", "nav_lang": "Язык",
        "kinds": {"fn": "функция", "type": "тип", "const": "константа"},
        "sub": {"param": "Параметры", "ret": "Возвращаемое значение", "err": "Ошибки", "example": "Пример"},
        "example_tag": "Проверено компиляцией",
        "badge_empty": "0 API",
        "meta_desc": "Справочник API модуля %s: %s — сигнатуры, ограничения параметров, возвращаемые значения, ошибки и проверенные компиляцией примеры.",
        "contract_title": "Контракт модуля", "trim_title": "Обрезка и зависимости",
        "fallback_desc_link": "публичном заголовочном файле",
        "fallback_desc_fmt": 'Комментарии контракта — в <a href="%s" target="_blank" rel="noopener">%s</a>.',
        "fallback_group": "Дополнительная справка · контракты в публичных заголовках",
        "fallback_search_text": "Комментарии контракта — в публичном заголовочном файле.",
        "constindex_title": "Индекс констант",
        "constindex_desc": 'Следующие константы определены в заголовочном файле этой страницы; их смысл — в <a href="%s">комментариях публичного заголовка</a>.',
        "constindex_th": ("Константа", "Описание"), "constindex_td": "См. комментарии в заголовке",
        "aside_list": "Список функций", "aside_ph": "Поиск функций…", "aside_aria": "Поиск API",
        "aside_back": "← К оглавлению",
        "footer_desc": "Кроссплатформенная библиотека инфраструктуры C, стремящаяся к совершенству в эпоху интернета и ИИ — целостный системный фундамент.",
        "footer_site": "Сайт", "footer_home": "Главная", "footer_book": "Книга", "footer_apiover": "Обзор API",
        "footer_res": "Ресурсы", "footer_repo": "Репозиторий Gitee", "footer_issues": "Задачи",
        "footer_entry": "Вход в справочник API",
    },
}

LANG_NAMES = {"zh": "简体中文", "en": "English", "ru": "Русский"}
LANG_URL_PREFIX = {"zh": "", "en": "en/", "ru": "ru/"}


def _en_plural(n, one, many):
    return "%d %s" % (n, one if n == 1 else many)


def _ru_plural(n, one, few, many):
    if n % 10 == 1 and n % 100 != 11:
        form = one
    elif 2 <= n % 10 <= 4 and not 12 <= n % 100 <= 14:
        form = few
    else:
        form = many
    return "%d %s" % (n, form)


def badge_bits(lang, counts):
    t = L10N[lang]
    bits = []
    if lang == "zh":
        for k, lbl in (("fn", "函数"), ("type", "类型"), ("const", "常量")):
            if counts.get(k):
                bits.append("%d 个%s" % (counts[k], lbl))
    elif lang == "en":
        for k, one, many in (("fn", "function", "functions"), ("type", "type", "types"), ("const", "constant", "constants")):
            if counts.get(k):
                bits.append(_en_plural(counts[k], one, many))
    else:
        triples = (("fn", "функция", "функции", "функций"), ("type", "тип", "типа", "типов"),
                   ("const", "константа", "константы", "констант"))
        for k, one, few, many in triples:
            if counts.get(k):
                bits.append(_ru_plural(counts[k], one, few, many))
    return bits or [t["badge_empty"]]


def nav_html_for(lang, page=None, translated=None):
    """en/ru ref 页导航：语言菜单优先深链同语言 ref 页，
    未翻译语言回退 zh 页并带 ?lang= 触发提示条。zh 页沿用 NAV_HTML 常量（字节幂等）。"""
    if lang == "zh":
        return NAV_HTML
    t = L10N[lang]
    lang_items = []
    for lg in ("zh", "en", "ru"):
        name = LANG_NAMES[lg]
        if lg == lang:
            lang_items.append('<span class="lang-cur">%s</span>' % name)
            continue
        have = lg != "zh" and translated and page in translated.get(lg, set())
        if lg == "zh":
            href = "../../book/ref-%s.html" % page
        elif have:
            href = "../../%s/book/ref-%s.html" % (lg, page)
        else:
            href = "../../book/ref-%s.html?lang=%s" % (page, lg)
        lang_items.append('<a href="%s">%s</a>' % (href, name))
    return ('<nav class="nav" aria-label="%s"><div class="nav-inner">'
            '<a href="../index.html" class="nav-logo" aria-label="%s"><img src="../res/logo.png" alt="XRT" width="96" height="32"><em>v2.0</em></a>'
            '<button type="button" class="nav-toggle" aria-controls="site-nav" aria-expanded="false" aria-label="%s"><span></span></button>'
            '<ul class="nav-links" id="site-nav"><li><a href="../index.html#arch">%s</a></li>'
            '<li><a href="../reliability.html">%s</a></li><li><a href="../benchmarks.html">%s</a></li>'
            '<li><a href="../start.html">%s</a></li>'
            '<li><details class="nav-dropdown"><summary>%s</summary><div><a href="../book/index.html">%s</a><a href="../api.html">%s</a></div></details></li>'
            '<li><details class="nav-dropdown"><summary>%s</summary><div><a href="https://github.com/xywhsoft/xrt" target="_blank" rel="noopener">GitHub ↗</a><a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee ↗</a></div></details></li>'
            '<li><details class="nav-dropdown nav-lang"><summary>%s</summary><div class="nav-lang-menu">%s</div></details></li>'
            '</ul></div></nav>'
            % (t["nav_aria"], t["nav_home"], t["nav_menu"], t["nav_arch"], t["nav_rel"],
               t["nav_bench"], t["nav_start"], t["nav_docs"], t["nav_book"], t["nav_api"],
               t["nav_repo"], t["nav_lang"], "".join(lang_items)))


def footer_html_for(lang):
    """en/ru 页脚。zh 页沿用 FOOTER_HTML 常量（字节幂等）。"""
    if lang == "zh":
        return FOOTER_HTML
    t = L10N[lang]
    return ('<footer class="footer">\n  <div class="footer-inner">\n    <div>\n'
            '      <div class="f-brand"><img src="../res/logo.png" alt="XRT" width="72" height="24"></div>\n'
            '      <p class="f-desc">%s</p>\n    </div>\n'
            '    <div>\n      <h3>%s</h3>\n      <ul>\n        <li><a href="../index.html">%s</a></li>\n'
            '        <li><a href="index.html">%s</a></li>\n        <li><a href="../api.html">%s</a></li>\n      </ul>\n    </div>\n'
            '    <div>\n      <h3>%s</h3>\n      <ul>\n        <li><a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">%s</a></li>\n'
            '        <li><a href="https://gitee.com/xywhsoft/xrt/issues" target="_blank" rel="noopener">%s</a></li>\n'
            '        <li><a href="../api.html#reference">%s</a></li>\n      </ul>\n    </div>\n  </div>\n'
            '  <div class="footer-bottom">\n    XRT &copy; <span data-year>2026</span> xLeaves (xywhsoft) &middot; MIT License &middot; <a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee</a>\n  </div>\n</footer>'
            % (t["footer_desc"], t["footer_site"], t["footer_home"], t["footer_book"],
               t["footer_apiover"], t["footer_res"], t["footer_repo"], t["footer_issues"], t["footer_entry"]))


NAV_HTML = ('<nav class="nav" aria-label="主导航"><div class="nav-inner">'
            '<a href="../index.html" class="nav-logo" aria-label="XRT 首页"><img src="../res/logo.png" alt="XRT" width="96" height="32"><em>v2.0</em></a>'
            '<button type="button" class="nav-toggle" aria-controls="site-nav" aria-expanded="false" aria-label="打开菜单"><span></span></button>'
            '<ul class="nav-links" id="site-nav"><li><a href="../index.html#arch">产品架构</a></li>'
            '<li><a href="../reliability.html">工程品质</a></li><li><a href="../benchmarks.html">性能设计</a></li>'
            '<li><a href="../start.html">快速开始</a></li>'
            '<li><details class="nav-dropdown"><summary>开发文档</summary><div><a href="../book/index.html">程序设计教程</a><a href="../api.html">API 参考与搜索</a></div></details></li>'
            '<li><details class="nav-dropdown"><summary>源码仓库</summary><div><a href="https://github.com/xywhsoft/xrt" target="_blank" rel="noopener">GitHub ↗</a><a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee ↗</a></div></details></li>'
            '<li><details class="nav-dropdown nav-lang"><summary>语言</summary><div class="nav-lang-menu"><span class="lang-cur">简体中文</span><a href="../en/index.html">English</a><a href="../ru/index.html">Русский</a></div></details></li>'
            '</ul></div></nav>')

FOOTER_HTML = ('<footer class="footer">\n  <div class="footer-inner">\n    <div>\n'
               '      <div class="f-brand"><img src="../res/logo.png" alt="XRT" width="72" height="24"></div>\n'
               '      <p class="f-desc">力求卓越的互联网 + AI 时代跨平台 C 基础设施库——一整套成体系的基础设施库。</p>\n    </div>\n'
               '    <div>\n      <h3>站点</h3>\n      <ul>\n        <li><a href="../index.html">首页</a></li>\n        <li><a href="index.html">书籍</a></li>\n        <li><a href="../api.html">API 概览</a></li>\n      </ul>\n    </div>\n'
               '    <div>\n      <h3>资源</h3>\n      <ul>\n        <li><a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee 仓库</a></li>\n'
               '        <li><a href="https://gitee.com/xywhsoft/xrt/issues" target="_blank" rel="noopener">问题反馈</a></li>\n        <li><a href="../api.html#reference">API 参考入口</a></li>\n      </ul>\n    </div>\n  </div>\n'
               '  <div class="footer-bottom">\n    XRT &copy; <span data-year>2026</span> xLeaves (xywhsoft) &middot; MIT License &middot; <a href="https://gitee.com/xywhsoft/xrt" target="_blank" rel="noopener">Gitee</a>\n  </div>\n</footer>')


def doc_of_page_order(page, docs, modules):
    """页面归属文档的顺序: 与页面同名的文档优先，其余按 modules.json 出现顺序。"""
    seen, order = set(), []
    exact = page
    for m in modules:
        for d in m.get("docs", []):
            if not d.startswith("docs/api/") or not d.endswith(".md"):
                continue
            base = os.path.basename(d)
            if base.endswith("-reference.md"):
                continue
            for h in m.get("public_headers", []):
                if os.path.basename(h)[:-2] != page:
                    continue
                if base not in seen:
                    seen.add(base)
                    order.append(base[:-3])
    order = [(b, i) for i, b in enumerate(order)]
    order.sort(key=lambda t: (t[0] != exact, t[1]))
    return [b for b, _ in order]


def build_trim_info(trim_lines, pages):
    """从 裁剪与依赖 表中取 裁剪宏/直接依赖/头文件。"""
    rows = [l[1] for l in trim_lines if isinstance(l, tuple) and l[0] == "text" and l[1].strip().startswith("|")]
    info = {}
    for r in rows:
        cells = [c.strip() for c in r.strip().strip("|").split("|")]
        if len(cells) >= 2:
            info[cells[0].strip("`")] = cells[1]
    return info


def render_contract_card(doc, pages, idx=0, lang="zh"):
    t = L10N[lang]
    # 一页可含多张契约卡（多文档归一页）：id 必须唯一，否则 DOM 重复 id
    cid = "module-contract" if idx == 0 else "module-contract-%d" % (idx + 1)
    parts = ['<section class="api-ref-contract" id="%s">' % cid]
    parts.append("<h3>%s<span class=\"contract-doc\">%s</span></h3>" % (t["contract_title"], e(doc.title)))
    has_content = False
    grid = []
    for key, lines in doc.contracts.items():
        has_content = True
        grid.append('<div class="contract-cell"><h4>%s</h4>%s</div>'
                    % (e(key), render_md_lines(lines, pages)))
    trim = build_trim_info(doc.trim, pages)
    if trim:
        bits = []
        for label in ("裁剪宏", "直接依赖", "头文件"):
            v = trim.get(label)
            if v:
                bits.append("<span>%s：%s</span>" % (e(label), inline_md(v, pages)))
        if bits:
            has_content = True
            parts.append('<div class="contract-trim">%s</div>' % "".join(bits))
    if grid:
        parts.append('<div class="contract-grid">%s</div>' % "".join(grid))
    if not has_content:
        return ""
    parts.append("</section>")
    return "".join(parts)


def render_item(sec, kind, hdr_sig, pages, fallback=None, lang="zh"):
    """fallback=(decl_text, header_path) 用于未文档化符号的补充条目。"""
    t = L10N[lang]
    h = ['<section class="api-ref-item api-kind-%s" id="%s">' % (kind, e(sec.name))]
    h.append("<h3>%s<span class=\"api-kind\">%s</span></h3>" % (e(sec.name), t["kinds"][kind]))
    desc_lines = [l for l in sec.desc if l[0] == "text" and l[1].strip()]
    if fallback:
        decl_text, hdr_path = fallback
        use_sig = decl_text
        desc_html = render_md_lines(desc_lines, pages) if desc_lines else \
            '<p class="api-desc">%s</p>' % (t["fallback_desc_fmt"] % (GITEE_BLOB + hdr_path, t["fallback_desc_link"]))
    else:
        use_sig = sec.sig or hdr_sig
        desc_html = render_md_lines(desc_lines, pages) if desc_lines else ""
    if desc_html:
        h.append(desc_html)
    if use_sig:
        h.append('<div class="api-ref-sig">%s</div>' % highlight_sig(use_sig))
    for key in sec.order:
        lines = sec.subs[key]
        if key == "param":
            h.append('<div class="api-sub"><h4>%s</h4>%s</div>' % (t["sub"]["param"], render_md_lines(lines, pages)))
        elif key == "ret":
            h.append('<div class="api-ref-returns"><span class="ret-label">%s</span>%s</div>'
                     % (t["sub"]["ret"], render_md_lines(lines, pages)))
        elif key == "err":
            h.append('<div class="api-ref-errors"><span class="err-label">%s</span>%s</div>'
                     % (t["sub"]["err"], render_md_lines(lines, pages)))
        elif key == "example":
            prose = [l for l in lines if l[0] == "text" and l[1].strip()]
            code = [l for l in lines if l[0] == "code"]
            if code or prose:
                h.append('<div class="api-ref-example"><h4>%s<span class="ok-tag">%s</span></h4>'
                         % (t["sub"]["example"], t["example_tag"]))
                if prose:
                    h.append('<p class="example-src">%s</p>' % inline_md(" ".join(x[1].strip() for x in prose), pages))
                for _, c in code:
                    h.append('<pre class="code">%s</pre>' % e(c))
                h.append("</div>")
        else:
            h.append('<div class="api-sub"><h4>%s</h4>%s</div>' % (e(key), render_md_lines(lines, pages)))
    h.append("</section>")
    return "\n      ".join(h)


def render_page(page, title, desc, groups, counts, pages, undoc_const, contract_cards,
                lang="zh", translated=None, prose_blocks=None):
    """groups: [(group_label_or_None, items:[(sec, kind, sig, fallback)])] 保序。"""
    t = L10N[lang]
    fns = []
    for label, items in groups:
        for sec, kind, sig, fb in items:
            if kind == "fn":
                fns.append(sec.name)

    side = ['<ul class="api-fn-list">']
    last_group = object()
    for label, items in groups:
        for sec, kind, sig, fb in items:
            if kind != "fn":
                continue
            if label != last_group and label:
                side.append('<li class="api-side-group">%s</li>' % e(label))
                last_group = label
            side.append('<li><a href="#%s">%s</a></li>' % (e(sec.name), e(sec.name)))
    side.append("</ul>")

    bits = badge_bits(lang, counts)
    badge = " · ".join(bits)
    url_prefix = LANG_URL_PREFIX[lang]
    meta_desc = t["meta_desc"] % (page, "、".join(bits) if lang == "zh" else ", ".join(bits))

    body = []
    for card in contract_cards:
        body.append(card)
    for ptitle, phtml in (prose_blocks or []):
        body.append('<section class="api-ref-doc"><h3>%s</h3>%s</section>' % (e(ptitle), phtml))
    for label, items in groups:
        if label:
            body.append('<div class="api-ref-group">%s</div>' % e(label))
        for sec, kind, sig, fb in items:
            body.append(render_item(sec, kind, sig, pages, fb, lang))
    if undoc_const:
        body.append('<section class="api-ref-item api-ref-constindex" id="const-index">')
        body.append("<h3>%s<span class=\"api-kind\">%s</span></h3>"
                    % (t["constindex_title"], t["kinds"]["const"]))
        body.append('<p class="api-desc">%s</p>'
                    % (t["constindex_desc"] % (GITEE_BLOB + "include/xrt/%s.h" % page)))
        body.append('<table class="api-ref-params"><tr><th>%s</th><th>%s</th></tr>' % t["constindex_th"])
        for name in undoc_const:
            body.append("<tr><td>%s</td><td>%s</td></tr>" % (e(name), t["constindex_td"]))
        body.append("</table></section>")

    page_url = "https://xrt.xywhsoft.com/%sbook/ref-%s.html" % (url_prefix, page)
    doc_html = """<!DOCTYPE html>
<html lang="%s">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title} %s</title>
  <meta name="description" content="{meta_desc}">
  <link rel="canonical" href="%s">
  <meta property="og:type" content="website">
  <meta property="og:site_name" content="XRT">
  <meta property="og:title" content="{title} %s">
  <meta property="og:description" content="{meta_desc}">
  <meta property="og:url" content="%s">
  <meta name="twitter:card" content="summary">
  <meta name="twitter:title" content="{title} %s">
  <meta name="twitter:description" content="{meta_desc}">

  <link rel="icon" href="data:image/svg+xml,%%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%%3E%%3Crect width='32' height='32' rx='8' fill='%%235b9dff'%%3E%%3C/text%%3E%%3C/svg%%3E">
  <link rel="stylesheet" href="../style.css?v=i18n001">
  <link rel="stylesheet" href="book.css?v=730594b9cc">
  <link rel="stylesheet" href="../refinement.css?v=ad12773fdb">
</head>
<body>

<a class="skip-link" href="#main">%s</a>
<div class="bg-scene" aria-hidden="true"></div>

{nav}

<header class="api-ref-hero">
  <div style="max-width: 980px; margin: 0 auto; padding: 0 24px;">
    <div class="api-source">xrt/{page}.h</div>
    <h1>{title}</h1>
    <div class="api-count-badge">{badge}</div>
    <p class="api-desc">{desc}</p>
  </div>
</header>

<div class="book-layout" id="main" role="main" style="max-width:1180px; grid-template-columns: 250px minmax(0,1fr); gap:44px; align-items:start; padding:24px 24px 72px;">

  <aside class="book-side api-side">
    <h4>%s</h4>
    <input class="api-side-search" type="text" placeholder="%s" aria-label="%s">
    {sidebar}
    <h4 style="margin-top:16px;"><a href="index.html" style="font-size:13px;">%s</a></h4>
  </aside>

  <div class="book-main">
{body}
  </div>
</div>

{footer}

<script src="../script.js?v=i18n005" defer></script>
</body>
</html>
"""
    doc_html = (doc_html
                % (t["html_lang"], t["title_suffix"], page_url, t["title_suffix"], page_url,
                   t["title_suffix"], t["skip"], t["aside_list"], t["aside_ph"],
                   t["aside_aria"], t["aside_back"]))
    return (doc_html
            .replace("{title}", e(title))
            .replace("{meta_desc}", e(meta_desc))
            .replace("{page}", e(page))
            .replace("{badge}", e(badge))
            .replace("{desc}", desc)
            .replace("{nav}", nav_html_for(lang, page, translated))
            .replace("{sidebar}", "\n      ".join(side))
            .replace("{body}", "\n      ".join(body))
            .replace("{footer}", footer_html_for(lang)))


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------

HREFLANG_RE = re.compile(r'[ \t]*<link rel="alternate" hreflang="[^"]*" href="[^"]*">\n?')


def upsert_hreflang(wwwroot, page):
    """页面存在翻译变体时，为 zh/en/ru 三个变体页注入/刷新 4 链 hreflang；
    无翻译变体时确保 zh 页不残留 alternate。"""
    base = "https://xrt.xywhsoft.com"
    paths = {"zh": os.path.join(wwwroot, "book", "ref-%s.html" % page)}
    for lg in ("en", "ru"):
        paths[lg] = os.path.join(wwwroot, lg, "book", "ref-%s.html" % page)
    present = [lg for lg in ("zh", "en", "ru") if os.path.exists(paths[lg])]
    links = []
    if len(present) > 1:
        attr = {"zh": "zh-CN", "en": "en", "ru": "ru"}
        for lg in ("zh", "en", "ru"):
            if lg in present:
                links.append('  <link rel="alternate" hreflang="%s" href="%s/%sbook/ref-%s.html">'
                             % (attr[lg], base, LANG_URL_PREFIX[lg], page))
        links.append('  <link rel="alternate" hreflang="x-default" href="%s/book/ref-%s.html">'
                     % (base, page))
    block = "\n".join(links) + "\n" if links else ""
    for lg in present:
        path = paths[lg]
        txt = io.open(path, encoding="utf-8").read()
        txt = HREFLANG_RE.sub("", txt)
        if block:
            txt = txt.replace('<link rel="canonical" href="%s/%sbook/ref-%s.html">\n'
                              % (base, LANG_URL_PREFIX[lg], page),
                              '<link rel="canonical" href="%s/%sbook/ref-%s.html">\n%s'
                              % (base, LANG_URL_PREFIX[lg], page, block), 1)
        io.open(path, "w", encoding="utf-8", newline="\n").write(txt)


def load_card_meta(wwwroot):
    """从 api.html 提取 (page -> 展示名, 描述)。"""
    path = os.path.join(wwwroot, "api.html")
    txt = io.open(path, encoding="utf-8").read()
    meta = {}
    for m in re.finditer(
            r'<a class="api-index-card" href="book/ref-([a-z0-9_]+)\.html">'
            r'<div class="api-index-header"><h4>(.*?)</h4>'
            r'.*?<div class="api-index-file">.*?</div>'
            r'<p class="api-index-desc">(.*?)</p>', txt, re.S):
        meta[m.group(1)] = (m.group(2), m.group(3).strip())
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default="D:/GIT/xrt")
    ap.add_argument("--wwwroot", default="D:/GIT/home/host/xrt/wwwroot")
    ap.add_argument("--only", help="逗号分隔的页面名（如 tls_client,json）")
    ap.add_argument("--lang", choices=("zh", "en", "ru"), default="zh",
                    help="zh: 官网根 book/；en/ru: 从 docs/api/<lang>/*.md 生成 <lang>/book/ 翻译页")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    repo = args.repo.replace("\\", "/")
    wwwroot = args.wwwroot.replace("\\", "/")
    lang = args.lang

    modules = json.load(io.open(os.path.join(repo, "config", "modules.json"), encoding="utf-8"))["modules"]
    sym_map, hdr_syms, hdr_srcs = scan_all_headers(repo)
    # sym_map: name -> [hdrs]; 归属种类取第一个头文件的扫描结果
    sym_kind = {}
    for name, hdrs in sym_map.items():
        kinds = set(hdr_syms[h][name][0] for h in hdrs)
        sym_kind[name] = kinds.pop() if len(kinds) == 1 else "ambig"

    def occ_headers(name):
        """声明扫描找不到时（宏生成的类型等），按标识符出现位置归属。"""
        pats = {h for h, src in hdr_srcs.items() if re.search(r"\b%s\b" % re.escape(name), src)}
        return sorted(pats)

    pages = set(h for h in hdr_syms)  # 所有内核页面名

    # 解析全部主题文档（跳过 -reference 附录）；en/ru 再叠加翻译文档
    docs = {}
    for path in sorted(glob.glob(os.path.join(repo, "docs", "api", "*.md"))):
        name = os.path.basename(path)[:-3]
        if name.endswith("-reference"):
            continue
        docs[name] = parse_doc(path)
    lang_docs = {}
    if lang != "zh":
        for path in sorted(glob.glob(os.path.join(repo, "docs", "api", lang, "*.md"))):
            lang_docs[os.path.basename(path)[:-3]] = parse_doc(path)

    # 翻译结构门禁：符号序列与全部代码块必须与 zh 文档一致（生成即校验）
    if lang != "zh":
        def doc_code_blocks(d):
            blocks = [l for l in d.intro if l[0] == "code"]
            for s in d.sections:
                blocks += [l for l in s.desc if l[0] == "code"]
                for k in s.order:
                    blocks += [l for l in s.subs[k] if l[0] == "code"]
            return blocks
        drift = []
        for name, ld in sorted(lang_docs.items()):
            zd = docs.get(name)
            if zd is None:
                drift.append("%s: zh 源文档不存在" % name)
                continue
            zs = [s.name for s in zd.sections]
            ls = [s.name for s in ld.sections]
            if zs != ls:
                miss = [x for x in zs if x not in ls]
                extra = [x for x in ls if x not in zs]
                drift.append("%s: 符号不一致 缺%s 多%s" % (name, miss, extra))
            if doc_code_blocks(zd) != doc_code_blocks(ld):
                drift.append("%s: 代码块与 zh 不一致" % name)
        if drift:
            print("翻译结构校验失败:")
            for d in drift:
                print("  " + d)
            return 1

    # 页面 -> 文档顺序
    page_docs = {}
    for m in modules:
        for h in m.get("public_headers", []):
            base = os.path.basename(h)[:-2]
            for d in m.get("docs", []):
                if d.startswith("docs/api/") and d.endswith(".md"):
                    b = os.path.basename(d)
                    if not b.endswith("-reference.md") and b[:-3] in docs:
                        lst = page_docs.setdefault(base, [])
                        if b not in lst:
                            lst.append(b)

    target_pages = sorted(p for p in pages if page_docs.get(p))
    # features.h 是 tools/generate_features.py 生成的裁剪展开注册表，无公开 API，不出参考页
    target_pages = [p for p in target_pages if p != "features"]

    # 各语言“页面已翻译”判定：页面归属的全部主题文档都有该语言译本
    def page_lang_docs():
        out = {}
        for lg in ("en", "ru"):
            have = set()
            for path in glob.glob(os.path.join(repo, "docs", "api", lg, "*.md")):
                have.add(os.path.basename(path)[:-3])
            out[lg] = set(p for p, dn in page_docs.items()
                          if p != "features" and dn and all(n[:-3] in have for n in dn))
        return out

    translated_map = page_lang_docs()
    if lang != "zh":
        target_pages = [p for p in target_pages if p in translated_map[lang]]
    if args.only:
        want = set(x.strip() for x in args.only.split(","))
        target_pages = [p for p in target_pages if p in want]

    render_docs = docs if lang == "zh" else lang_docs
    card_meta = load_card_meta(wwwroot) if lang == "zh" else {}
    l10n = L10N[lang]

    warnings = []
    search_entries = []
    stats = {}

    documented = {}   # page -> set(names) 已渲染符号
    page_groups = {}  # page -> [(label, items)]
    page_prose = {}   # page -> [(title, html)] 全宽散文档（稳定契约/裁剪表/常量表等）
    page_counts = {}
    page_contracts = {}
    page_undoc_const = {}

    for page in target_pages:
        groups = []       # [(label, [(sec, kind, hdr_sig, fallback)])]
        counts = {"fn": 0, "type": 0, "const": 0}
        contracts = []
        seen_names = set()
        for doc_name in doc_of_page_order(page, docs, modules):
            if doc_name not in render_docs:
                continue
            doc = render_docs[doc_name]
            trim_labels = build_trim_info(doc.trim, pages)
            # 标签式裁剪表（| 项目 | 值 | 形态）由契约卡 contract-trim 呈现；
            # 宏列式裁剪表（| 公开选择宏 | … 形态）落全宽散文档
            trim_in_card = any(k in trim_labels for k in ("裁剪宏", "直接依赖", "头文件"))
            for kind, title, lines in doc.prose:
                if kind == "trim":
                    if trim_in_card:
                        continue
                    page_prose.setdefault(page, []).append(
                        (l10n["trim_title"], render_md_lines(doc.trim, pages)))
                elif lines:
                    page_prose.setdefault(page, []).append(
                        (title, render_md_lines(lines, pages)))
            contributed = False
            for sec in doc.sections:
                hdrs = sym_map.get(sec.name)
                if hdrs is None:
                    occ = occ_headers(sec.name)
                    if occ:
                        sym_map[sec.name] = occ
                        sym_kind[sec.name] = "const" if sec.name[:1].isupper() else "type"
                        hdrs = occ
                if "param" in sec.subs or "ret" in sec.subs:
                    kind = "fn"
                elif hdrs:
                    kind = sym_kind.get(sec.name)
                else:
                    kind = "fn" if re.match(r"^xrt[A-Z]", sec.name) else \
                           ("const" if sec.name[:1].isupper() else "type")
                if kind not in KIND_LABEL:
                    kind = "const" if sec.name[:1].isupper() else "type"
                if hdrs is None:
                    warnings.append("[%s] %s 文档节 `%s` 未在公共头中找到" % (page, doc_name, sec.name))
                    continue
                if page not in hdrs:
                    continue
                if sec.name in seen_names:
                    continue
                seen_names.add(sec.name)
                decl = hdr_syms[page].get(sec.name)
                hdr_sig = decl[1] if decl and decl[0] == "fn" else None
                if not groups or groups[-1][0] != sec.group:
                    groups.append((sec.group, []))
                groups[-1][1].append((sec, kind, hdr_sig, None))
                counts[kind] += 1
                contributed = True
                if kind in ("fn", "type"):
                    if lang == "zh":
                        search_entries.append({
                            "name": sec.name,
                            "module": page,
                            "url": "book/ref-%s.html#%s" % (page, sec.name),
                            "text": search_text(sec),
                        })
            if contributed:
                contracts.append(doc)
        # 未文档化符号（按本页头文件扫描）: 函数补签名条目，常量进索引
        fallback_items = []
        undoc_const = []
        for name, (kind, decl) in hdr_syms[page].items():
            if name in seen_names:
                continue
            if kind == "fn":
                placeholder = Section(name, None)
                fallback_items.append((placeholder, "fn", None, (decl, "include/xrt/%s.h" % page)))
            elif kind == "const":
                undoc_const.append(name)
        if fallback_items:
            groups.append((l10n["fallback_group"], fallback_items))
            counts["fn"] += len(fallback_items)
            if lang == "zh":
                for sec, kind, _sig, _fb in fallback_items:
                    search_entries.append({
                        "name": sec.name,
                        "module": page,
                        "url": "book/ref-%s.html#%s" % (page, sec.name),
                        "text": l10n["fallback_search_text"],
                    })
        page_groups[page] = groups
        page_counts[page] = counts
        page_contracts[page] = contracts
        page_undoc_const[page] = sorted(undoc_const)
        documented[page] = seen_names
        stats[page] = {
            "fn": counts["fn"], "type": counts["type"], "const": counts["const"],
            "undoc_fn": len(fallback_items), "undoc_const": len(undoc_const),
        }

    # ---- 输出 ----
    # 空页（无条目也无常量索引，如 features.h 只有裁剪宏）不生成
    skipped = [p for p in target_pages
               if not any(items for _label, items in page_groups[p])
               and not page_undoc_const.get(p)]
    target_pages = [p for p in target_pages if p not in set(skipped)]
    if not args.dry_run:
        book = os.path.join(wwwroot, *(LANG_URL_PREFIX[lang].rstrip("/").split("/") + ["book"]))

        def intro_desc(page):
            """翻译页 hero 描述：取首个 intro 散文段首行，去行内标记。"""
            for doc in page_contracts.get(page, []):
                for k, txt in doc.intro:
                    if k == "text" and txt.strip() and not txt.strip().startswith(("|", "- ")):
                        s = CODE_SPAN_RE.sub(r"\1", txt.strip())
                        s = BOLD_RE.sub(r"\1", s)
                        return e(s)
            return e("%s module API reference." % page)

        for page in target_pages:
            if lang == "zh":
                display, card_desc = card_meta.get(page, (page, "%s 模块 API 参考。" % page))
                desc = card_desc if card_desc else "%s 模块 API 参考。" % page
            else:
                display = page
                desc = intro_desc(page)
            cards = [render_contract_card(d, pages, i, lang) for i, d in enumerate(page_contracts[page])]
            cards = [c for c in cards if c]
            html_txt = render_page(page, display, desc, page_groups[page],
                                   page_counts[page], pages, page_undoc_const[page], cards,
                                   lang=lang, translated=translated_map,
                                   prose_blocks=page_prose.get(page))
            out = os.path.join(book, "ref-%s.html" % page)
            io.open(out, "w", encoding="utf-8", newline="\n").write(html_txt)

        if lang == "zh":
            # 搜索索引: 扩展库与未重生成的内核页条目原样保留
            idx_path = os.path.join(wwwroot, "search-index.json")
            old = json.load(io.open(idx_path, encoding="utf-8"))
            regen = set(target_pages)
            keep = [x for x in old
                    if x["module"].startswith(EXT_PREFIXES) or x["module"] not in regen]
            merged = search_entries + keep
            merged.sort(key=lambda x: x["module"])
            json.dump(merged, io.open(idx_path, "w", encoding="utf-8", newline="\n"),
                      ensure_ascii=False, indent=1)

            # api.html 计数
            api_path = os.path.join(wwwroot, "api.html")
            txt = io.open(api_path, encoding="utf-8").read()
            for page in target_pages:
                cnt = stats[page]["fn"]
                txt = re.sub(
                    r'(href="book/ref-%s\.html"><div class="api-index-header"><h4>.*?</h4><span class="api-index-count">)\d+(</span>)' % re.escape(page),
                    r"\g<1>%d\g<2>" % cnt, txt, count=1, flags=re.S)
            total = len(merged)
            kernel_mods = len(set(x["module"] for x in merged if not x["module"].startswith(EXT_PREFIXES)))
            txt = re.sub(r"[\d,]+ 个内核参考模块", "%d 个内核参考模块" % kernel_mods, txt)
            txt = re.sub(r"[\d,]+ 条目录参考条目", "%s 条目录参考条目" % format(total, ","), txt)
            txt = re.sub(r"共 [\d,]+ API", "共 %s API" % format(total, ","), txt)
            io.open(api_path, "w", encoding="utf-8", newline="\n").write(txt)

        # book.css 追加样式（幂等；zh 与语言目录各自的 book.css；散文档块独立标记）
        css_path = os.path.join(book, "book.css")
        css = io.open(css_path, encoding="utf-8").read()
        changed = False
        if "/* ---- gen_web_api 追加 ---- */" not in css:
            css += CSS_ADDON
            changed = True
        if "/* ---- gen_web_api 散文档 ---- */" not in css:
            css += CSS_ADDON_DOC
            changed = True
        if changed:
            io.open(css_path, "w", encoding="utf-8", newline="\n").write(css)

        # hreflang 互指：仅当页面存在翻译变体时注入/刷新四链
        for page in target_pages:
            upsert_hreflang(wwwroot, page)

    # ---- 报告 ----
    print("页面: %d 个重生成" % len(target_pages))
    if skipped:
        print("跳过空页: %s" % ", ".join(skipped))
    tf = sum(stats[p]["fn"] for p in target_pages)
    tt = sum(stats[p]["type"] for p in target_pages)
    tc = sum(stats[p]["const"] for p in target_pages)
    uf = sum(stats[p]["undoc_fn"] for p in target_pages)
    uc = sum(stats[p]["undoc_const"] for p in target_pages)
    print("条目: 函数 %d（含头文件回补 %d）· 类型 %d · 常量 %d · 常量索引 %d" % (tf, uf, tt, tc, uc))
    print("搜索索引: 内核新条目 %d" % len(search_entries))
    if warnings:
        print("警告 %d 条:" % len(warnings))
        for w in warnings[:40]:
            print("  " + w)
    return 0


CSS_ADDON_DOC = """
/* ---- gen_web_api 散文档 ---- */

.api-ref-doc { margin: 0 0 30px; }
.api-ref-doc > h3 {
  font-size: 17px;
  color: var(--text);
  margin: 26px 0 4px;
}
.api-ref-doc > h3::after {
  content: "";
  display: block;
  width: 34px;
  height: 2px;
  background: var(--green);
  margin-top: 8px;
}
.api-ref-doc p { color: var(--soft); font-size: 14px; line-height: 1.75; margin: 10px 0; }
.api-ref-doc ul { color: var(--soft); font-size: 14px; line-height: 1.75; margin: 8px 0; padding-left: 18px; }
.api-ref-doc li { margin: 4px 0; }
.api-ref-doc table { margin: 12px 0; }
"""

CSS_ADDON = """
/* ---- gen_web_api 追加 ---- */

.api-ref-hero .api-count-badge { color: var(--green); }

.api-ref-item > h3 .api-kind { color: var(--muted); }
.api-ref-item.api-kind-type > h3 .api-kind { color: var(--violet, #a78bfa); }
.api-ref-item.api-kind-const > h3 .api-kind { color: var(--yellow, #ffd479); }

.api-ref-item > p {
  color: var(--soft);
  font-size: 14px;
  margin: 0 0 4px;
}
.api-ref-item > p a { color: var(--blue); }
.api-ref-item > p + .api-ref-sig { margin-top: 10px; }

.api-ref-group {
  font-size: 13px;
  font-weight: 700;
  letter-spacing: 2px;
  text-transform: uppercase;
  color: var(--cyan);
  margin: 36px 0 -12px;
  padding-bottom: 8px;
  border-bottom: 1px dashed var(--line-soft);
}

.api-ref-item .api-sub h4,
.api-ref-contract .contract-cell h4 {
  font-size: 12px;
  color: var(--muted);
  text-transform: uppercase;
  letter-spacing: 1px;
  margin: 14px 0 4px;
}

.api-ref-returns table,
.api-ref-errors table { margin: 8px 0 0; }

.api-ref-errors ul { margin: 6px 0 0; padding-left: 18px; }
.api-ref-errors li { margin: 3px 0; }
.api-ref-errors li code,
.api-ref-returns td code { color: var(--red); font-size: 12px; }
.api-ref-returns td:first-child code { color: var(--green); font-size: 12px; }

.api-ref-example .example-src { color: var(--muted); font-size: 12.5px; margin: 2px 0 8px; }
.api-ref-example .example-src a { color: var(--blue); }
.api-ref-example pre.code {
  background: #060a14;
  border: 1px solid var(--line-soft);
  border-radius: 10px;
  padding: 12px 16px;
  overflow-x: auto;
  font-family: var(--font-mono);
  font-size: 12.5px;
  line-height: 1.7;
  color: #c9d6ee;
}

.api-ref-contract {
  background: rgba(9, 14, 26, 0.72);
  border: 1px solid var(--line);
  border-radius: var(--radius);
  padding: 16px 20px;
  margin: 0 0 36px;
}
.api-ref-contract > h3 {
  font-size: 15px;
  margin-bottom: 8px;
}
.api-ref-contract .contract-doc {
  font-size: 12px;
  font-weight: 400;
  color: var(--muted);
  margin-left: 10px;
}
.api-ref-contract .contract-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  gap: 4px 24px;
}
.api-ref-contract .contract-cell p { color: var(--soft); font-size: 13px; margin: 4px 0; }
.api-ref-contract .contract-cell ul { margin: 4px 0; padding-left: 16px; color: var(--soft); font-size: 13px; }
.api-ref-contract .contract-trim {
  margin-top: 10px;
  padding-top: 10px;
  border-top: 1px dashed var(--line-soft);
  display: flex;
  flex-wrap: wrap;
  gap: 6px 18px;
  font-size: 12.5px;
  color: var(--muted);
}
.api-ref-contract .contract-trim code { color: var(--cyan); font-size: 12px; }

.api-side .api-fn-list .api-side-group {
  font-size: 10.5px;
  font-weight: 700;
  letter-spacing: 1.5px;
  text-transform: uppercase;
  color: var(--muted);
  margin: 12px 0 2px;
  padding: 0;
  list-style: none;
}

.api-ref-constindex td:first-child { white-space: nowrap; }
"""


if __name__ == "__main__":
    sys.exit(main())
