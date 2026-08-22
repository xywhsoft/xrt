#!/usr/bin/env python3

"""验证旧版资产审计账本的增量刷新规则。"""

from __future__ import annotations

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import refactor_audit



def _entry(path: str, digest: str) -> dict:
	"""建立一个足以验证增量合并行为的审计条目。"""

	return {
		"path": path,
		"kind": "source",
		"size": 1,
		"lines": 1,
		"sha256": digest,
		"audit": "pending",
		"decision": None,
		"module": [],
		"target": [],
		"evidence": [],
		"note": None,
	}



class MergeSnapshotTest(unittest.TestCase):
	"""验证刷新不会丢失有效证据，也不会沿用失效结论。"""

	def test_preserves_unchanged_audit_record(self) -> None:
		"""内容哈希未变化时必须原样保留逐行审计记录。"""

		stored = _entry("lib/a.h", "same")
		stored["audit"] = "verified"
		stored["decision"] = "refine"
		stored["module"] = ["core"]
		stored["segments"] = [{"start": 1, "end": 1, "audit": "verified"}]
		current = _entry("lib/a.h", "same")

		merged, counts = refactor_audit._merge_snapshot([stored], [current])

		self.assertIs(merged[0], stored)
		self.assertEqual(
			counts,
			{"preserved": 1, "changed": 0, "added": 0, "removed": 0},
		)

	def test_resets_changed_and_added_files(self) -> None:
		"""内容变化或新加入的文件必须从 pending 状态重新审计。"""

		stored = _entry("lib/a.h", "old")
		stored["audit"] = "verified"
		stored["segments"] = [{"start": 1, "end": 1, "audit": "verified"}]
		changed = _entry("lib/a.h", "new")
		added = _entry("lib/b.h", "added")

		merged, counts = refactor_audit._merge_snapshot(
			[stored],
			[changed, added],
		)

		self.assertEqual([item["audit"] for item in merged], ["pending", "pending"])
		self.assertEqual([item["segments"] for item in merged], [[], []])
		self.assertEqual(
			counts,
			{"preserved": 0, "changed": 1, "added": 1, "removed": 0},
		)

	def test_reports_removed_files(self) -> None:
		"""刷新摘要必须显示已经离开当前旧版参考树的文件。"""

		merged, counts = refactor_audit._merge_snapshot(
			[_entry("lib/removed.h", "old")],
			[],
		)

		self.assertEqual(merged, [])
		self.assertEqual(counts["removed"], 1)



class RelocationTest(unittest.TestCase):
	"""验证迁出扩展库后的资产只会按唯一、显式边界解析。"""

	def setUp(self) -> None:
		"""保存工具根目录，避免临时测试状态泄漏。"""

		self.root = refactor_audit.ROOT

	def tearDown(self) -> None:
		"""恢复工具根目录。"""

		refactor_audit.ROOT = self.root

	def test_resolves_unique_product_relative_path(self) -> None:
		"""原相对路径在一个登记产品中存在时应解析到该扩展。"""

		from tempfile import TemporaryDirectory

		with TemporaryDirectory() as directory:
			root = Path(directory)
			target = root / "extlibs" / "demo" / "include" / "demo.h"
			target.parent.mkdir(parents=True)
			target.write_text("demo", encoding="utf-8")
			refactor_audit.ROOT = root
			resolved, error = refactor_audit._resolve_current_path(
				"include/demo.h",
				{
					"roots": ["extlibs/demo"],
					"path_overrides": {},
				},
			)

		self.assertEqual(resolved, "extlibs/demo/include/demo.h")
		self.assertIsNone(error)

	def test_rejects_ambiguous_product_relative_path(self) -> None:
		"""同一路径在多个产品中存在时不能静默选择任意一个。"""

		from tempfile import TemporaryDirectory

		with TemporaryDirectory() as directory:
			root = Path(directory)
			for product in ("left", "right"):
				target = root / "extlibs" / product / "docs" / "api.md"
				target.parent.mkdir(parents=True)
				target.write_text(product, encoding="utf-8")
			refactor_audit.ROOT = root
			resolved, error = refactor_audit._resolve_current_path(
				"docs/api.md",
				{
					"roots": ["extlibs/left", "extlibs/right"],
					"path_overrides": {},
				},
			)

		self.assertIsNone(resolved)
		self.assertIn("ambiguous relocated path", error)



if __name__ == "__main__":
	unittest.main()
