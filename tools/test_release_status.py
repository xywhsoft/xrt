#!/usr/bin/env python3

"""验证发布状态文档与模块清单保持一致。"""

from __future__ import annotations

import json
import re
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
CORE_MANIFEST = "config/modules.json"
XHTTP_MANIFEST = "extlibs/xhttp/config/modules.json"



def _manifest(path: str = CORE_MANIFEST) -> dict:
	"""读取发布状态测试共用的模块清单。"""

	return json.loads(
		(ROOT / path).read_text(encoding="utf-8")
	)



def _developing_modules(manifest: dict) -> set[str]:
	"""返回尚缺真实运行证据的模块。"""

	return {
		module["name"] for module in manifest["modules"]
		if module["state"] == "developing"
	}



class ReleaseStatusTest(unittest.TestCase):
	"""验证 developing 模块不会从公开发布边界中消失。"""

	def test_developing_modules_are_exact(self) -> None:
		"""状态文档列出的 io_uring 节点必须与清单精确相等。"""

		expected = set().union(*(
			_developing_modules(_manifest(path))
			for path in (CORE_MANIFEST, XHTTP_MANIFEST)
		))
		text = (ROOT / "docs" / "RELEASE_STATUS.md").read_text(
			encoding="utf-8"
		)
		section = text.split("## io_uring 门禁", 1)[1].split("\n## ", 1)[0]
		actual = set(re.findall(r"^- `([^`]+)`$", section, re.MULTILINE))

		self.assertEqual(actual, expected)



	def test_developing_modules_have_linux_runtime_gate(self) -> None:
		"""待验证节点与 xhttp io_uring 契约必须进入 Linux 门禁。"""

		core = _manifest(CORE_MANIFEST)
		xhttp = _manifest(XHTTP_MANIFEST)
		modules = {
			module["name"]: module
			for manifest in (core, xhttp)
			for module in manifest["modules"]
		}
		expected = _developing_modules(core)
		expected.update(_developing_modules(xhttp))
		expected.update(
			module["name"] for module in xhttp["modules"]
			if module["name"].endswith("_uring_tests")
		)
		workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text(
			encoding="utf-8"
		)
		marker = "- name: Compile Linux io_uring network and TLS gates"
		self.assertIn(marker, workflow)
		section = workflow.split(marker, 1)[1].split("\n      - name:", 1)[0]
		# GitHub 托管 runner 通过 seccomp 禁用 io_uring（EPERM），
		# 此门禁在托管环境只保留编译与链接证据；运行证据由支持
		# io_uring 的环境（自托管 runner 或本机）承担并记录于
		# RELEASE_STATUS。环境限制说明必须随门禁保留。
		self.assertIn("io_uring", section)
		self.assertIn("--no-run", section)
		match = re.search(r"--suite\s+([a-z0-9_,]+)", section)
		self.assertIsNotNone(match)
		gated = set(match.group(1).split(","))

		self.assertFalse(gated - modules.keys())
		self.assertLessEqual(expected, gated)
		for name in gated:
			dependencies = set(modules[name]["depends"])
			dependencies.update(
				modules[name].get("platform_depends", {}).get("linux", [])
			)
			self.assertTrue(
				(name == "net_port_uring") or
				("net_port_uring" in dependencies)
			)

	def test_windows_msvc_package_matrix_is_complete(self) -> None:
		"""MSVC 风格工具链必须覆盖双架构和两种发布库。"""

		workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text(
			encoding="utf-8"
		)
		section = workflow.split("  windows-msvc:\n", 1)[1].split(
			"\n  android:\n",
			1,
		)[0]

		for compiler in ("cl", "clang-cl"):
			self.assertRegex(section, rf"(?m)^          - {re.escape(compiler)}$")
		for arch in ("x64", "x86"):
			self.assertRegex(section, rf"(?m)^          - {arch}$")
		self.assertIn("--suite all", section)
		self.assertIn("--kind static", section)
		self.assertIn("--kind shared", section)
		self.assertEqual(section.count("--verify"), 2)



	def test_compiler_priority_has_explicit_ci_evidence(self) -> None:
		"""GCC、TCC、XLang 和 VC 必须按分层策略保留明确门禁。"""

		workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text(
			encoding="utf-8"
		)
		status = (ROOT / "docs" / "RELEASE_STATUS.md").read_text(
			encoding="utf-8"
		)
		gcc = workflow.split("  gcc-baseline:\n", 1)[1].split(
			"\n  tcc:\n", 1
		)[0]
		tcc = workflow.split("  tcc:\n", 1)[1].split(
			"\n  xlang:\n", 1
		)[0]
		xlang = workflow.split("  xlang:\n", 1)[1].split(
			"\n  native:\n", 1
		)[0]

		self.assertIn("container: gcc:12", gcc)
		self.assertIn("--compiler gcc", gcc)
		self.assertIn("--kind static --verify", gcc)
		self.assertIn("runs-on: ubuntu-24.04", tcc)
		self.assertIn("tcc version 0.9.27", tcc)
		self.assertIn("--compiler tcc", tcc)
		self.assertIn("--kind static --verify", tcc)
		self.assertIn("continue-on-error: true", xlang)
		self.assertRegex(xlang, r"XLANG_REF: [0-9a-f]{40}")
		self.assertIn("demo5/lib/tcc/tcc.c", xlang)
		self.assertIn("tcc version 0.9.28rc", xlang)
		self.assertIn("-I single", xlang)
		self.assertIn("--compiler gcc --suite core --kind static", xlang)
		self.assertIn("out/xlang/consumer", xlang)
		self.assertIn("`GCC > TCC > XLang > VC`", status)



	def test_android_matrix_uses_explicit_target(self) -> None:
		"""Android 交叉编译不能按 Linux 宿主选择依赖和输出目录。"""

		workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text(
			encoding="utf-8"
		)
		section = workflow.split("  android:\n", 1)[1].split(
			"\n  ios:\n",
			1,
		)[0]

		self.assertIn("--target-platform android", section)
		self.assertIn("out/android-cc/native-android/core,net", section)
		self.assertNotIn("out/android-cc/native/core,net", section)



if __name__ == "__main__":
	unittest.main()
