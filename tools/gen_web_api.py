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
}

KIND_LABEL = {"fn": "函数", "type": "类型", "const": "常量"}


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
    """识别契约类 h2: 严格形式 模块契约：X，或短标题含 所有权/线程/错误 的变体。"""
    if text.startswith("模块契约"):
        return text.split("：", 1)[-1].strip() if "：" in text else "总则"
    if len(text) <= 10 and re.search(r"(所有权|线程|错误)", text) \
            and not re.search(r"(函数|类型|常量|示例|范例)", text):
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
    for path in sorted(glob.glob(os.path.join(repo, "include", "xrt", "*.h"))):
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
    __slots__ = ("name", "title", "intro", "sections", "contracts", "trim")

    def __init__(self, name):
        self.name = name                # 文件基名（不含 .md）
        self.title = name
        self.intro = []
        self.sections = []
        self.contracts = {}             # 错误/线程/所有权 -> 行
        self.trim = []                  # 裁剪与依赖 原始行


def parse_doc(path):
    doc = Doc(os.path.basename(path)[:-3])
    cur_h2 = None          # 最近的主题标题（h2 或非符号 h3）
    cur_h2_generic = False
    cur_sec = None
    cur_sub = None
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
            elif lvl == 2:
                cur_sec, cur_sub = None, None
                ckey = contract_h2_key(text)
                if ckey is not None:
                    cur_h2, cur_h2_generic = ckey, True
                    doc.contracts.setdefault(ckey, [])
                    cur_sub = ("__contract__", ckey)
                elif text.startswith("裁剪"):
                    cur_h2, cur_h2_generic = text, True
                    cur_sub = ("__trim__",)
                else:
                    cur_h2, cur_h2_generic = text, text in GENERIC_HEADINGS
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
                    cur_h2, cur_h2_generic = text, text in GENERIC_HEADINGS
            else:  # h4
                if cur_sec is None:
                    continue
                key = text.split("：", 1)[0].strip()
                if key in SUBJECTS or True:
                    cur_sub = key
                    cur_sec.sub(key)
            continue
        # 普通行
        if isinstance(cur_sub, tuple):
            if cur_sub[0] == "__contract__":
                doc.contracts[cur_sub[1]].append(("text", line))
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


def render_contract_card(doc, pages):
    parts = ['<section class="api-ref-contract" id="module-contract">']
    parts.append("<h3>模块契约<span class=\"contract-doc\">%s</span></h3>" % e(doc.title))
    has_content = False
    grid = []
    for key, lines in doc.contracts.items():
        has_content = True
        grid.append('<div class="contract-cell"><h4>%s</h4>%s</div>'
                    % (e(key), render_md_lines(lines, pages)))
    if grid:
        parts.append('<div class="contract-grid">%s</div>' % "".join(grid))
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
    if not has_content:
        return ""
    parts.append("</section>")
    return "".join(parts)


def render_item(sec, kind, hdr_sig, pages, fallback=None):
    """fallback=(decl_text, header_path) 用于未文档化符号的补充条目。"""
    h = ['<section class="api-ref-item api-kind-%s" id="%s">' % (kind, e(sec.name))]
    h.append("<h3>%s<span class=\"api-kind\">%s</span></h3>" % (e(sec.name), KIND_LABEL[kind]))
    desc_lines = [l for l in sec.desc if l[0] == "text" and l[1].strip()]
    if fallback:
        decl_text, hdr_path = fallback
        use_sig = decl_text
        desc_html = render_md_lines(desc_lines, pages) if desc_lines else \
            '<p class="api-desc">契约注释见<a href="%s" target="_blank" rel="noopener">公共头文件</a>。</p>' % (GITEE_BLOB + hdr_path)
    else:
        use_sig = sec.sig or hdr_sig
        desc_html = render_md_lines(desc_lines, pages) if desc_lines else ""
    if desc_html:
        h.append(desc_html)
    if use_sig:
        h.append('<div class="api-ref-sig">%s</div>' % highlight_sig(use_sig))
    for key in sec.order:
        lines = sec.subs[key]
        if key == "参数":
            h.append('<div class="api-sub"><h4>参数</h4>%s</div>' % render_md_lines(lines, pages))
        elif key == "返回值":
            h.append('<div class="api-ref-returns"><span class="ret-label">返回值</span>%s</div>'
                     % render_md_lines(lines, pages))
        elif key == "错误":
            h.append('<div class="api-ref-errors"><span class="err-label">错误</span>%s</div>'
                     % render_md_lines(lines, pages))
        elif key == "范例":
            prose = [l for l in lines if l[0] == "text" and l[1].strip()]
            code = [l for l in lines if l[0] == "code"]
            if code or prose:
                h.append('<div class="api-ref-example"><h4>范例<span class="ok-tag">已编译验证</span></h4>')
                if prose:
                    h.append('<p class="example-src">%s</p>' % inline_md(" ".join(x[1].strip() for x in prose), pages))
                for _, c in code:
                    h.append('<pre class="code">%s</pre>' % e(c))
                h.append("</div>")
        else:
            h.append('<div class="api-sub"><h4>%s</h4>%s</div>' % (e(key), render_md_lines(lines, pages)))
    h.append("</section>")
    return "\n      ".join(h)


def render_page(page, title, desc, groups, counts, pages, undoc_const, contract_cards):
    """groups: [(group_label_or_None, items:[(sec, kind, sig, fallback)])] 保序。"""
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

    badge_bits = []
    for label, cnt in (("函数", counts.get("fn", 0)), ("类型", counts.get("type", 0)), ("常量", counts.get("const", 0))):
        if cnt:
            badge_bits.append("%d 个%s" % (cnt, label))
    badge = " · ".join(badge_bits) or "0 个 API"
    meta_desc = "%s 模块 API 参考：%s——签名、参数约束、返回值、错误与已编译范例。" % (page, "、".join(badge_bits))

    body = []
    for card in contract_cards:
        body.append(card)
    for label, items in groups:
        if label:
            body.append('<div class="api-ref-group">%s</div>' % e(label))
        for sec, kind, sig, fb in items:
            body.append(render_item(sec, kind, sig, pages, fb))
    if undoc_const:
        body.append('<section class="api-ref-item api-ref-constindex" id="const-index">')
        body.append("<h3>常量索引<span class=\"api-kind\">常量</span></h3>")
        body.append('<p class="api-desc">以下常量在本页所属头文件中定义，语义见<a href="%s">公共头注释</a>。</p>'
                    % (GITEE_BLOB + "include/xrt/%s.h" % page))
        body.append('<table class="api-ref-params"><tr><th>常量</th><th>说明</th></tr>')
        for name in undoc_const:
            body.append("<tr><td>%s</td><td>见头文件注释</td></tr>" % e(name))
        body.append("</table></section>")

    doc_html = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title} API 参考 - XRT 程序设计</title>
  <meta name="description" content="{meta_desc}">
  <link rel="canonical" href="https://xrt.xywhsoft.com/book/ref-{page}.html">
  <meta property="og:type" content="website">
  <meta property="og:site_name" content="XRT">
  <meta property="og:title" content="{title} API 参考 - XRT 程序设计">
  <meta property="og:description" content="{meta_desc}">
  <meta property="og:url" content="https://xrt.xywhsoft.com/book/ref-{page}.html">
  <meta name="twitter:card" content="summary">
  <meta name="twitter:title" content="{title} API 参考 - XRT 程序设计">
  <meta name="twitter:description" content="{meta_desc}">

  <link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='8' fill='%235b9dff'/%3E%3C/text%3E%3C/svg%3E">
  <link rel="stylesheet" href="../style.css?v=6bdafbdced">
  <link rel="stylesheet" href="book.css?v=730594b9cc">
  <link rel="stylesheet" href="../refinement.css?v=ad12773fdb">
</head>
<body>

<a class="skip-link" href="#main">跳到主要内容</a>
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
    <h4>函数列表</h4>
    <input class="api-side-search" type="text" placeholder="搜索函数…" aria-label="搜索 API">
    {sidebar}
    <h4 style="margin-top:16px;"><a href="index.html" style="font-size:13px;">← 返回目录</a></h4>
  </aside>

  <div class="book-main">
{body}
  </div>
</div>

{footer}

<script src="../script.js?v=31cac74985" defer></script>
</body>
</html>
"""
    return (doc_html
            .replace("{title}", e(title))
            .replace("{meta_desc}", e(meta_desc))
            .replace("{page}", e(page))
            .replace("{badge}", e(badge))
            .replace("{desc}", e(desc))
            .replace("{nav}", NAV_HTML)
            .replace("{sidebar}", "\n      ".join(side))
            .replace("{body}", "\n      ".join(body))
            .replace("{footer}", FOOTER_HTML))


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------

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
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    repo = args.repo.replace("\\", "/")
    wwwroot = args.wwwroot.replace("\\", "/")

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

    # 解析全部主题文档（跳过 -reference 附录）
    docs = {}
    for path in sorted(glob.glob(os.path.join(repo, "docs", "api", "*.md"))):
        name = os.path.basename(path)[:-3]
        if name.endswith("-reference"):
            continue
        docs[name] = parse_doc(path)

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
    if args.only:
        want = set(x.strip() for x in args.only.split(","))
        target_pages = [p for p in target_pages if p in want]

    card_meta = load_card_meta(wwwroot)

    warnings = []
    search_entries = []
    stats = {}

    documented = {}   # page -> set(names) 已渲染符号
    page_groups = {}  # page -> [(label, items)]
    page_counts = {}
    page_contracts = {}
    page_undoc_const = {}

    for page in target_pages:
        groups = []       # [(label, [(sec, kind, hdr_sig, fallback)])]
        counts = {"fn": 0, "type": 0, "const": 0}
        contracts = []
        seen_names = set()
        for doc_name in doc_of_page_order(page, docs, modules):
            if doc_name not in docs:
                continue
            doc = docs[doc_name]
            contributed = False
            for sec in doc.sections:
                hdrs = sym_map.get(sec.name)
                if hdrs is None:
                    occ = occ_headers(sec.name)
                    if occ:
                        sym_map[sec.name] = occ
                        sym_kind[sec.name] = "const" if sec.name[:1].isupper() else "type"
                        hdrs = occ
                if "参数" in sec.subs or "返回值" in sec.subs:
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
            groups.append(("补充参考 · 契约见公共头注释", fallback_items))
            counts["fn"] += len(fallback_items)
            for sec, kind, _sig, _fb in fallback_items:
                search_entries.append({
                    "name": sec.name,
                    "module": page,
                    "url": "book/ref-%s.html#%s" % (page, sec.name),
                    "text": "契约注释见公共头文件。",
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
        book = os.path.join(wwwroot, "book")
        for page in target_pages:
            display, card_desc = card_meta.get(page, (page, "%s 模块 API 参考。" % page))
            desc = card_desc if card_desc else "%s 模块 API 参考。" % page
            cards = [render_contract_card(d, pages) for d in page_contracts[page]]
            cards = [c for c in cards if c]
            html_txt = render_page(page, display, desc, page_groups[page],
                                   page_counts[page], pages, page_undoc_const[page], cards)
            out = os.path.join(book, "ref-%s.html" % page)
            io.open(out, "w", encoding="utf-8", newline="\n").write(html_txt)

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

        # book.css 追加样式（幂等）
        css_path = os.path.join(wwwroot, "book", "book.css")
        css = io.open(css_path, encoding="utf-8").read()
        if "/* ---- gen_web_api 追加 ---- */" not in css:
            css += CSS_ADDON
            io.open(css_path, "w", encoding="utf-8", newline="\n").write(css)

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
