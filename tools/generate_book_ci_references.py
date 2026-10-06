#!/usr/bin/env python3
"""Build the book's API reference targets in an isolated CI output directory."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

from gen_web_api import Section, render_page, scan_header

ROOT = Path(__file__).resolve().parents[1]


def generate(output: Path) -> None:
    book = output / "book"
    book.mkdir(parents=True, exist_ok=True)
    for name, content in (("search-index.json", "[]"), ("api.html", ""),
                          ("book/book.css", "")):
        (output / name).write_text(content, encoding="utf-8")
    subprocess.run([sys.executable, str(ROOT / "tools/gen_web_api.py"),
                    "--repo", str(ROOT), "--wwwroot", str(output)], check=True)

    # The website's core generator preserves separately published extensions.
    # A fresh CI checkout needs those reference targets built from public headers.
    headers = {}
    for manifest in sorted((ROOT / "extlibs").glob("*/config/modules.json")):
        product = manifest.parent.parent.name
        data = json.loads(manifest.read_text(encoding="utf-8"))
        for module in data["modules"]:
            for relative in module["public_headers"]:
                header = ROOT / relative
                if not header.is_file():
                    raise FileNotFoundError(relative)
                headers[product + "-" + header.stem] = (header, relative)
    pages = set(headers)
    for page, (header, relative) in sorted(headers.items()):
        symbols = scan_header(str(header))
        items = [(Section(name, None), kind, None, (decl, relative))
                 for name, (kind, decl) in sorted(symbols.items())]
        counts = {kind: sum(item[1] == kind for item in items)
                  for kind in ("fn", "type", "const")}
        rendered = render_page(page, page, "公开头文件的声明参考。",
                               [(None, items)], counts, pages, [], [])
        (book / ("ref-" + page + ".html")).write_text(rendered, encoding="utf-8")
    print("[generated] %d extension reference targets" % len(headers))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "out/doc-site")
    args = parser.parse_args()
    generate(args.output.resolve())


if __name__ == "__main__":
    main()
