#!/usr/bin/env python3

"""使用真实 C 预处理器验证 SDK 声明头与实现头的模块选择合同。"""

from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import amalgamate


class DeclarationFeatureTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls) -> None:
		cls.compiler = shutil.which("gcc")
		if cls.compiler is None:
			raise RuntimeError("gcc is required for declaration feature tests")
		cls.directory = tempfile.TemporaryDirectory(prefix="xrt-declarations-")
		cls.root = Path(cls.directory.name)
		for overlays in (None, [Path("extlibs/xruntime/config/modules.json")]):
			for render in (amalgamate._content, amalgamate._declaration_content):
				path, content = render(overlays)
				(cls.root / path.name).write_text(content, encoding="utf-8")
		for product, path in (
			("xrt", "include/xrt/features.h"),
			("xruntime", "extlibs/xruntime/include/xruntime/features.h"),
		):
			target = cls.root / product
			target.mkdir()
			shutil.copyfile(amalgamate.ROOT / path, target / "features.h")



	@classmethod
	def tearDownClass(cls) -> None:
		cls.directory.cleanup()



	def preprocess(self, source: str) -> dict[str, str]:
		result = subprocess.run(
			[self.compiler, "-E", "-dM", "-x", "c", "-I", str(self.root), "-"],
			input=source, text=True, encoding="utf-8", capture_output=True, timeout=30,
		)
		self.assertEqual(result.returncode, 0, result.stderr)
		self.assertNotIn("XRT_DECLARATIONS_RESTORE_", result.stdout)
		return {
			match[1]: match[2].strip()
			for match in re.finditer(
				r"(?m)^#define ((?:XRT|XRUNTIME)_(?:(?:MODULE|FEATURE)_\w+|EXCLUDE_\w+))([^\n]*)$",
				result.stdout,
			)
		}



	def test_selected_closures_match_implementation_headers(self) -> None:
		"""无选择、单模块、跨产品依赖、ALL 与排除均不得放大功能面。"""
		cases = (
			("xrt", ()),
			("xrt", ("XRT_MODULE_CODEC_PERCENT",)),
			("xrt", ("XRT_MODULE_DEFLATE", "XRT_MODULE_INFLATE")),
			("xrt", ("XRT_MODULE_TLS_STREAM_DIAL_SYNC",)),
			("xrt", ("XRT_MODULE_ALL", "XRT_EXCLUDE_MEMORY_DEBUG")),
			("xruntime", ()),
			("xruntime", ("XRUNTIME_MODULE_TYPED_DICT",)),
			("xruntime", ("XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD",)),
			("xruntime", ("XRT_MODULE_ALL", "XRUNTIME_MODULE_ALL", "XRT_EXCLUDE_MEMORY_DEBUG")),
		)
		for product, modules in cases:
			with self.subTest(product=product, modules=modules):
				prefix = "".join(f"#define {name} 1\n" for name in modules)
				expected = self.preprocess(prefix + f'#include "{product}.h"\n')
				actual = self.preprocess(prefix + f'#include "{product}_decl.h"\n')
				self.assertEqual(actual, expected)



	def test_late_selection_and_repeated_inclusion(self) -> None:
		"""自动注入空声明后，原生包再次包含同一头仍能选择真实依赖。"""
		for product in ("xrt", "xruntime"):
			with self.subTest(product=product):
				prefix = '#define XRT_MODULE_CODEC_PERCENT 7\n'
				expected = self.preprocess(prefix + f'#include "{product}.h"\n')
				actual = self.preprocess(
					f'#include "{product}_decl.h"\n' + prefix +
					f'#include "{product}_decl.h"\n#include "{product}_decl.h"\n'
				)
				self.assertEqual(actual, expected)
				self.assertEqual(actual["XRT_MODULE_CODEC_PERCENT"], "7")
				self.assertNotIn("XRT_FEATURE_DEFLATE", actual)



	def test_core_and_runtime_include_order(self) -> None:
		"""核心和扩展声明的包含顺序不能改变跨产品依赖。"""
		prefix = '#define XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD 1\n'
		expected = self.preprocess(prefix + '#include "xruntime.h"\n')
		for headers in (("xrt_decl.h", "xruntime_decl.h"),
						("xruntime_decl.h", "xrt_decl.h")):
			with self.subTest(headers=headers):
				actual = self.preprocess(prefix + "".join(f'#include "{h}"\n' for h in headers))
				self.assertEqual(actual, expected)



	def test_existing_feature_values_are_preserved(self) -> None:
		prefix = '#define XRT_FEATURE_CODEC_PERCENT 23\n#define XRT_MODULE_DEFLATE 9\n'
		expected = self.preprocess(prefix + '#include "xrt.h"\n')
		actual = self.preprocess(prefix + '#include "xrt_decl.h"\n')
		self.assertEqual(actual, expected)
		self.assertEqual(actual["XRT_FEATURE_CODEC_PERCENT"], "23")



	def test_real_implementation_after_declarations(self) -> None:
		"""先包含 SDK 后编译已选模块，实现仍必须通过完整 C 类型检查。"""
		for product, selection, implementation in (
			("xrt", "XRT_MODULE_CODEC_PERCENT", "XRT_IMPLEMENTATION"),
			("xruntime", "XRUNTIME_MODULE_TYPED_DICT", "XRUNTIME_IMPLEMENTATION"),
		):
			with self.subTest(product=product):
				source = (f'#include "{product}_decl.h"\n#define {selection}\n'
					f'#include "{product}_decl.h"\n#define {implementation}\n'
					f'#include "{product}.h"\n')
				result = subprocess.run(
					[self.compiler, "-std=c11", "-fsyntax-only", "-x", "c", "-I", str(self.root), "-"],
					input=source, text=True, encoding="utf-8", capture_output=True, timeout=60,
				)
				self.assertEqual(result.returncode, 0, result.stderr)




	def test_selection_header_before_declarations(self) -> None:
		"""先前生成的模块闭包不能阻止 SDK 输出其他公开声明。"""
		for product in ("xrt", "xruntime"):
			with self.subTest(product=product):
				source = (f'#define XRT_MODULE_CODEC_PERCENT 1\n#include <{product}/features.h>\n'
					f'#include "{product}_decl.h"\n'
					'void* sdk_complete_declaration_probe = (void*)&xrtDeflateAll;\n')
				result = subprocess.run(
					[self.compiler, "-std=c11", "-Werror=implicit-function-declaration",
					 "-fsyntax-only", "-x", "c", "-I", str(self.root), "-"],
					input=source, text=True, encoding="utf-8", capture_output=True, timeout=30,
				)
				self.assertEqual(result.returncode, 0, result.stderr)
				macros = self.preprocess(source)
				self.assertIn("XRT_FEATURE_CODEC_PERCENT", macros)
				self.assertNotIn("XRT_FEATURE_DEFLATE", macros)



if __name__ == "__main__":
	unittest.main()
