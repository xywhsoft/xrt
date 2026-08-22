#!/usr/bin/env python3

"""验证工作树清理器只处理明确的可再生文件。"""

from __future__ import annotations

from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import clean as xrt_clean



class CleanTest(unittest.TestCase):
	"""验证预览、删除和源码保留边界。"""

	def test_dry_run_and_apply(self) -> None:
		"""默认预览不得改文件，显式应用只能删除清单内项目。"""

		with tempfile.TemporaryDirectory() as temp:
			root = Path(temp)
			(root / "src").mkdir()
			(root / "src" / "keep.c").write_text("int keep;\n", encoding="utf-8")
			(root / "build").mkdir()
			(root / "build" / "test.o").write_bytes(b"object")
			(root / "tools" / "__pycache__").mkdir(parents=True)
			(root / "tools" / "__pycache__" / "tool.pyc").write_bytes(b"cache")
			(root / ".xrt-session").mkdir()
			(root / "xrt-process-output.txt").write_text("output\n", encoding="utf-8")

			with redirect_stdout(StringIO()):
				count, total = xrt_clean.clean(root, False)
			self.assertEqual(count, 4)
			self.assertGreater(total, 0)
			self.assertTrue((root / "build" / "test.o").exists())

			with redirect_stdout(StringIO()):
				count, total = xrt_clean.clean(root, True)
			self.assertEqual(count, 4)
			self.assertGreater(total, 0)
			self.assertFalse((root / "build").exists())
			self.assertFalse((root / "tools" / "__pycache__").exists())
			self.assertFalse((root / ".xrt-session").exists())
			self.assertTrue((root / "src" / "keep.c").exists())

	def test_history_keeps_legacy_source(self) -> None:
		"""历史清理可删旧产物和固定实验目录，但必须保留基线源码。"""

		with tempfile.TemporaryDirectory() as temp:
			root = Path(temp)
			(root / "dev" / "net").mkdir(parents=True)
			(root / "dev" / "net" / "old.c").write_text("old\n", encoding="utf-8")
			(root / "dev" / "ver1").mkdir(parents=True)
			(root / "dev" / "ver1" / "legacy.c").write_text("legacy\n", encoding="utf-8")
			(root / "dev" / "ver1" / "legacy.exe").write_bytes(b"binary")

			with redirect_stdout(StringIO()):
				count, total = xrt_clean.clean(root, True, True)
			self.assertEqual(count, 2)
			self.assertGreater(total, 0)
			self.assertFalse((root / "dev" / "net").exists())
			self.assertFalse((root / "dev" / "ver1" / "legacy.exe").exists())
			self.assertTrue((root / "dev" / "ver1" / "legacy.c").exists())



if __name__ == "__main__":
	unittest.main()
