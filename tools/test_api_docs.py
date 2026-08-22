#!/usr/bin/env python3

"""验证公共 API 文档覆盖检查器。"""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_api_docs



class HeaderSymbolsTest(unittest.TestCase):
	"""验证跨行函数、常量和类型提取。"""

	def test_extracts_family_symbols(self) -> None:
		"""只收集指定家族的导出函数，常量和类型保持完整标识符。"""

		text = """
			#define XWS_LIMIT 1
			typedef struct xwsitem xwsitem;
			XRT_API bool xrtWsOpen(
				xwsitem* pItem
			);
			XRT_API bool xrtHttpOpen(void);
		"""
		functions, constants, types = check_api_docs._header_symbols(
			text,
			"xrtWs",
			"XWS_",
			"xws",
		)

		self.assertEqual(functions, {"xrtWsOpen"})
		self.assertEqual(constants, {"XWS_LIMIT"})
		self.assertEqual(types, {"xwsitem"})

	def test_missing_uses_identifier_boundaries(self) -> None:
		"""较长标识符不能误报为较短 API 已有文档。"""

		self.assertEqual(
			check_api_docs._missing(
				{"xrtWsOpen", "xrtWsOpenResult"},
				"`xrtWsOpenResult` is documented.",
			),
			["xrtWsOpen"],
		)



	def test_excludes_include_guards(self) -> None:
		"""头文件防重包含标识不是公共常量。"""

		text = """
			#ifndef XRT_CO_H
			#define XRT_CO_H
			#define XRT_CO_LIMIT 16
			#endif
		"""
		_, constants, _ = check_api_docs._header_symbols(
			text,
			"xrtCo",
			"XRT_CO_",
			"xco",
		)

		self.assertEqual(constants, {"XRT_CO_LIMIT"})



class FamilyFilesTest(unittest.TestCase):
	"""验证模块清单范围和文件存在性。"""

	def test_collects_and_deduplicates_family_files(self) -> None:
		"""同一家族模块共享的头文件与文档只检查一次。"""

		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "include").mkdir()
			(root / "docs").mkdir()
			(root / "include/ws.h").write_text("", encoding="utf-8")
			(root / "docs/ws.md").write_text("", encoding="utf-8")
			manifest = root / "modules.json"
			manifest.write_text(json.dumps({
				"modules": [
					{
						"name": "websocket",
						"public_headers": ["include/ws.h"],
						"docs": ["docs/ws.md"],
					},
					{
						"name": "websocket_future",
						"public_headers": ["include/ws.h"],
						"docs": ["docs/ws.md"],
					},
				],
			}), encoding="utf-8")

			headers, docs = check_api_docs._family_files(
				root,
				manifest,
				"websocket",
			)

			self.assertEqual(headers, [root / "include/ws.h"])
			self.assertEqual(docs, [root / "docs/ws.md"])



if __name__ == "__main__":
	unittest.main()
