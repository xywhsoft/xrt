#!/usr/bin/env python3

"""验证示例索引生成器的闭包检查。"""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_example_index as example_index



class ExampleIndexTest(unittest.TestCase):
	"""验证示例源码和清单必须双向闭合。"""

	def _tree(self, examples: list[str]) -> tuple[tempfile.TemporaryDirectory, Path, Path]:
		"""创建最小示例树和模块清单。"""

		temporary = tempfile.TemporaryDirectory()
		root = Path(temporary.name)
		for example in examples:
			path = root / example
			path.parent.mkdir(parents=True, exist_ok=True)
			path.write_text("int main(void) { return 0; }\n", encoding="utf-8")
		manifest = root / "modules.json"
		return temporary, root, manifest

	def test_render_uses_manifest_owner(self) -> None:
		"""有效示例必须在索引中保留所属模块。"""

		temporary, root, manifest = self._tree(["examples/core/version/main.c"])
		with temporary:
			manifest.write_text(json.dumps({"modules": [{
				"name": "core",
				"examples": ["examples/core/version/main.c"],
			}]}), encoding="utf-8")
			text = example_index._render(
				example_index._load_entries(root, manifest)
			)

		self.assertIn("[core/version]", text)
		self.assertIn("`core`", text)

	def test_unregistered_source_is_rejected(self) -> None:
		"""目录扫描发现而清单遗漏的示例不能静默消失。"""

		temporary, root, manifest = self._tree(["examples/core/version/main.c"])
		with temporary:
			manifest.write_text(json.dumps({"modules": []}), encoding="utf-8")
			with self.assertRaisesRegex(ValueError, "unregistered example"):
				example_index._load_entries(root, manifest)



if __name__ == "__main__":
	unittest.main()
