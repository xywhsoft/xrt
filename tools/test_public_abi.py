#!/usr/bin/env python3

"""验证不同裁剪配置之间的 XRT 公开结构 ABI。"""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]



def _compiler() -> str:
	"""选择可用的 C11 编译器，允许 CI 和本地环境显式覆盖。"""

	candidates = [
		os.environ.get("XRT_TEST_CC"),
		os.environ.get("CC"),
		"cc",
		"gcc",
		"clang",
	]
	for candidate in candidates:
		if candidate and shutil.which(candidate):
			return candidate
	raise unittest.SkipTest("no C compiler is available for ABI test")



class PublicAbiTest(unittest.TestCase):
	"""公开数据布局不能随实现功能宏变化。"""

	def test_public_struct_fields_are_not_feature_gated(self) -> None:
		"""公开结构体内部不得再引入任何预处理条件。"""

		failures: list[str] = []
		start = re.compile(
			r"^\s*(?:typedef\s+)?(?:struct|union)"
			r"(?:\s+[A-Za-z_][A-Za-z0-9_]*)?\s*\{"
		)
		end = re.compile(
			r"^\s*}\s*(?:[A-Za-z_][A-Za-z0-9_]*)?\s*;"
		)
		headers = list((ROOT / "include" / "xrt").glob("*.h"))
		headers += list(
			(ROOT / "extlibs" / "xhttp" / "include" / "xrt").glob("*.h")
		)
		for header in sorted(headers):
			inside = False
			for number, line in enumerate(
				header.read_text(encoding="utf-8").splitlines(),
				1,
			):
				if not inside and start.match(line):
					inside = True
					continue
				if inside and re.match(r"^\s*#\s*(?:if|ifdef|ifndef)\b", line):
					failures.append(f"{header.name}:{number}: {line.strip()}")
				if inside and end.match(line):
					inside = False

		self.assertEqual(failures, [])

	def test_full_runtime_and_trimmed_consumer_share_layout(self) -> None:
		"""分别编译完整侧和裁剪侧，再链接验证尺寸、偏移与尾部边界。"""

		compiler = _compiler()
		source = ROOT / "tests" / "abi"
		with tempfile.TemporaryDirectory() as temporary:
			output = Path(temporary)
			full = output / "public_struct_full.o"
			executable = output / (
				"public_struct_abi.exe" if os.name == "nt"
				else "public_struct_abi"
			)
			common = [
				compiler,
				"-std=c11",
				"-Wall",
				"-Wextra",
				"-Werror",
				"-I",
				str(ROOT / "include"),
			]
			subprocess.run(
				common + [
					"-c",
					str(source / "public_struct_full.c"),
					"-o",
					str(full),
				],
				check=True,
				cwd=ROOT,
			)
			subprocess.run(
				common + [
					str(source / "test_public_struct_abi.c"),
					str(full),
					"-o",
					str(executable),
				],
				check=True,
				cwd=ROOT,
			)
			result = subprocess.run(
				[executable],
				check=False,
				cwd=ROOT,
				capture_output=True,
				text=True,
			)

		self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
		self.assertIn("[PASS] public struct ABI", result.stdout)

	def test_xhttp_full_and_trimmed_consumer_share_layout(self) -> None:
		"""分别编译完整 xhttp 和最小客户端，验证尺寸、偏移与尾部边界。"""

		compiler = _compiler()
		source = ROOT / "extlibs" / "xhttp" / "tests" / "abi"
		with tempfile.TemporaryDirectory() as temporary:
			output = Path(temporary)
			full = output / "public_struct_full.o"
			executable = output / (
				"xhttp_public_struct_abi.exe" if os.name == "nt"
				else "xhttp_public_struct_abi"
			)
			common = [
				compiler,
				"-std=c11",
				"-Wall",
				"-Wextra",
				"-Werror",
				"-I",
				str(ROOT / "include"),
				"-I",
				str(ROOT / "extlibs" / "xhttp" / "include"),
			]
			subprocess.run(
				common + [
					"-c",
					str(source / "public_struct_full.c"),
					"-o",
					str(full),
				],
				check=True,
				cwd=ROOT,
			)
			subprocess.run(
				common + [
					str(source / "test_public_struct_abi.c"),
					str(full),
					"-o",
					str(executable),
				],
				check=True,
				cwd=ROOT,
			)
			result = subprocess.run(
				[executable],
				check=False,
				cwd=ROOT,
				capture_output=True,
				text=True,
			)

		self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
		self.assertIn("[PASS] xhttp public struct ABI", result.stdout)



if __name__ == "__main__":
	unittest.main()
