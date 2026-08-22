#!/usr/bin/env python3

"""从模块清单生成 XRT 示例索引。"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

from xrt_text import write_utf8


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "config" / "modules.json"
OUTPUT = ROOT / "docs" / "EXAMPLES.md"



def _load_entries(root: Path, manifest_path: Path) -> dict[str, list[str]]:
	"""读取示例归属，并拒绝清单与源码树不一致。"""

	manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
	owners: dict[str, list[str]] = defaultdict(list)
	for module in manifest["modules"]:
		for example in module.get("examples", []):
			normalized = Path(example).as_posix()
			owners[normalized].append(module["name"])

	actual = {
		path.relative_to(root).as_posix()
		for path in (root / "examples").rglob("main.c")
	}
	missing = sorted(actual - owners.keys())
	stale = sorted(owners.keys() - actual)
	if missing or stale:
		lines = []
		lines.extend(f"unregistered example: {path}" for path in missing)
		lines.extend(f"missing example source: {path}" for path in stale)
		raise ValueError("\n".join(lines))
	return {
		path: sorted(set(module_names))
		for path, module_names in sorted(owners.items())
	}



def _render(entries: dict[str, list[str]]) -> str:
	"""按示例体系生成稳定 Markdown。"""

	groups: dict[str, list[tuple[str, list[str]]]] = defaultdict(list)
	for path, owners in entries.items():
		parts = Path(path).parts
		group = parts[1] if len(parts) > 1 else "other"
		groups[group].append((path, owners))

	lines = [
		"# XRT 示例索引",
		"",
		"此文件由 `tools/generate_example_index.py` 从 `config/modules.json` 生成，",
		"不要手工维护第二份示例清单。构建器会按所属模块的真实依赖闭包编译并运行示例。",
		"",
		f"当前共登记 `{len(entries)}` 个可运行示例。",
		"",
	]
	for group in sorted(groups):
		items = groups[group]
		lines.extend([f"## {group} ({len(items)})", ""])
		for path, owners in items:
			label = "/".join(Path(path).parts[1:-1])
			owner_text = ", ".join(f"`{name}`" for name in owners)
			lines.append(f"- [{label}](../{path}) - {owner_text}")
		lines.append("")
	return "\n".join(lines)



def main() -> int:
	"""生成索引，或检查现有文件没有漂移。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	try:
		content = _render(_load_entries(ROOT, MANIFEST))
	except ValueError as error:
		raise SystemExit(str(error)) from error
	if args.check:
		if not OUTPUT.is_file() or OUTPUT.read_text(encoding="utf-8") != content:
			raise SystemExit("docs/EXAMPLES.md is out of date")
		print("[pass] example index is current")
		return 0
	write_utf8(OUTPUT, content)
	print(f"[generate] {OUTPUT.relative_to(ROOT)}")
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
