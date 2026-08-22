#!/usr/bin/env python3

"""验证公共 API 符号参考生成器。"""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_api_reference as reference



class ApiReferenceTest(unittest.TestCase):
	"""验证家族收集、符号归属和稳定输出。"""

	def test_groups_symbols_by_first_header(self) -> None:
		"""重复符号只归属第一次出现的公共头。"""

		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			first = root / "include/xrt/tls.h"
			second = root / "include/xrt/tls_more.h"
			first.parent.mkdir(parents=True)
			first.write_text(
				"typedef enum xtlsstate { XTLS_READY = 1 } xtlsstate;\n"
				"XRT_API bool xrtTlsOpen(void);\n",
				encoding="utf-8",
			)
			second.write_text(
				"XRT_API bool xrtTlsOpen(void);\n"
				"XRT_API void xrtTlsClose(void);\n",
				encoding="utf-8",
			)
			groups = reference._symbol_groups(
				[first, second], "xrtTls", "XTLS_", "xtls"
			)
			output = root / "docs/api/tls-reference.md"
			text = reference._render(
				root, output, "TLS", "tls.md", groups
			)

		self.assertEqual(text.count("`xrtTlsOpen`"), 1)
		self.assertIn("`xrtTlsClose`", text)
		self.assertIn("`XTLS_READY`", text)
		self.assertIn("`xtlsstate`", text)
		self.assertIn("../../include/xrt/tls.h", text)

	def test_missing_manifest_header_is_rejected(self) -> None:
		"""清单登记的失效公共头不能生成空参考。"""

		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			manifest = root / "modules.json"
			manifest.write_text(json.dumps({"modules": [{
				"name": "tls",
				"public_headers": ["include/xrt/tls.h"],
			}]}), encoding="utf-8")
			with self.assertRaisesRegex(ValueError, "header is missing"):
				reference._family_headers(root, manifest, "tls")



if __name__ == "__main__":
	unittest.main()
