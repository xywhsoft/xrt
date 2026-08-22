#!/usr/bin/env python3

"""测试 XRT 仓库级发布成熟度门禁。"""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from check_release_maturity import _check_product_root, audit_repository


class ReleaseMaturityTest(unittest.TestCase):
	"""保证所有实现都能沿正式构建路径获得验证。"""

	def test_repository_structure_is_complete(self) -> None:
		"""普通门禁必须覆盖生产代码、测试、单头和文档。"""

		errors, summary = audit_repository(ROOT)

		self.assertEqual([], errors)
		self.assertEqual(
			summary["implementation_modules"],
			summary["test_covered"],
		)
		self.assertEqual(
			summary["implementation_modules"],
			summary["single_covered"],
		)
		self.assertEqual(
			summary["implementation_modules"],
			summary["docs_covered"],
		)



	def test_strict_release_reports_every_open_state(self) -> None:
		"""严格门禁不能放过 developing 模块或 review 体系。"""

		errors, summary = audit_repository(ROOT, release=True)
		open_errors = [
			error for error in errors
			if ("未完成模块" in error) or ("待定体系" in error)
		]

		self.assertEqual(
			summary["developing"] + summary["review_systems"],
			len(open_errors),
		)



	def test_xruntime_extension_is_release_complete(self) -> None:
		"""扩展产品必须独立覆盖源码、测试、单头、文档和发布 profile。"""

		errors, summary = audit_repository(
			ROOT,
			release=True,
			manifest_path=Path("extlibs/xruntime/config/modules.json"),
		)

		self.assertEqual([], errors)
		self.assertEqual("xruntime", summary["product"])
		self.assertEqual(
			summary["implementation_modules"],
			summary["test_covered"],
		)
		self.assertEqual(
			summary["implementation_modules"],
			summary["single_covered"],
		)
		self.assertEqual(
			summary["implementation_modules"],
			summary["docs_covered"],
		)



	def test_extension_requires_named_product_root(self) -> None:
		"""扩展清单必须提供与产品同名的聚合根模块。"""

		errors: list[str] = []
		modules = [
			{
				"name": "sample_core",
				"feature": "XSAMPLE_FEATURE_CORE",
				"depends": [],
			}
		]

		_check_product_root({"product": "sample"}, modules, modules, errors)

		self.assertEqual(["扩展产品缺少同名根模块: sample"], errors)



	def test_extension_product_root_covers_every_public_feature(self) -> None:
		"""产品根依赖闭包不能漏掉同产品的公开功能。"""

		errors: list[str] = []
		modules = [
			{
				"name": "sample_core",
				"feature": "XSAMPLE_FEATURE_CORE",
				"depends": [],
			},
			{
				"name": "sample_extra",
				"feature": "XSAMPLE_FEATURE_EXTRA",
				"depends": [],
			},
			{
				"name": "sample",
				"feature": "XSAMPLE_FEATURE_SAMPLE",
				"depends": ["sample_core"],
			},
		]

		_check_product_root({"product": "sample"}, modules, modules, errors)

		self.assertEqual(
			[
				"扩展产品根模块未覆盖全部公开功能: "
				"sample -> ['sample_extra']"
			],
			errors,
		)



if __name__ == "__main__":
	unittest.main()
