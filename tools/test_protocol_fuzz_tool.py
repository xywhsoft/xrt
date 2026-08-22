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



if __name__ == "__main__":
	unittest.main()
