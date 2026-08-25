#!/usr/bin/env python3

"""验证协议模糊测试能够解析核心与扩展模块闭包。"""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_protocol_fuzz as protocol_fuzz



class ProtocolFuzzToolTest(unittest.TestCase):
	"""覆盖模糊目标的清单归属和扩展头文件路径。"""

	def test_all_target_closures_resolve(self) -> None:
		"""每个公开目标都必须从所属产品清单得到完整闭包。"""

		for name, config in protocol_fuzz.TARGETS.items():
			with self.subTest(target=name):
				sources, defines, _, _ = protocol_fuzz._closure(
					config["module"],
					config.get("manifest", "config/modules.json"),
				)
				self.assertTrue(sources)
				self.assertTrue(defines)

	def test_extension_targets_include_xhttp_headers(self) -> None:
		"""xhttp 目标必须独立声明扩展头文件根目录。"""

		for name in ("auth", "route", "router", "sse"):
			config = protocol_fuzz.TARGETS[name]
			_, _, _, include_dirs = protocol_fuzz._closure(
				config["module"], config["manifest"],
			)
			self.assertIn("extlibs/xhttp/include", include_dirs)


	def test_sanitizer_keeps_alignment_checks(self) -> None:
		"""未对齐契约只关闭 memcpy 内建展开，不能关闭 alignment 检查。"""

		command = protocol_fuzz._build_command(
			"clang", ["fuzz/http1_protocol.c"], [], [], [],
			None, Path("out/http1_protocol_fuzz"),
		)
		self.assertIn("-fno-builtin-memcpy", command)
		self.assertNotIn("-fno-sanitize=alignment", command)


	def test_release_targets_have_persistent_corpus(self) -> None:
		"""核心协议 fuzz 必须携带可回流的仓库语料。"""

		minimum = {
			"tls": 6,
			"x509": 6,
			"net-address": 6,
			"http1": 10,
			"websocket": 6,
		}
		for name, count in minimum.items():
			with self.subTest(target=name):
				path = protocol_fuzz._persistent_corpus(
					protocol_fuzz.TARGETS[name]
				)
				self.assertIsNotNone(path)
				files = [item for item in path.iterdir() if item.is_file()]
				self.assertGreaterEqual(len(files), count)



if __name__ == "__main__":
	unittest.main()
