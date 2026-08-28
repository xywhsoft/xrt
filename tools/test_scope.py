#!/usr/bin/env python3

"""验证 XRT 产品边界、体系归属和外部集成隔离。"""

from __future__ import annotations

import json
from pathlib import Path
import re
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from xrt_manifest import load_manifest  # noqa: E402



class ScopeTest(unittest.TestCase):
	"""保证源码目录和产品范围使用同一份事实来源。"""

	def setUp(self) -> None:
		"""读取当前模块清单和范围描述。"""

		self.manifest = load_manifest(ROOT / "config" / "modules.json")
		self.scope = self.manifest["scope"]



	def test_current_product_records_completed_xlang_integration(self) -> None:
		"""XRT 仍独立发布，但清单必须记录已经完成的 XLang 集成。"""

		self.assertEqual(self.scope["product"], "xrt")
		integrations = {
			item["name"]: item["state"]
			for item in self.scope["external_integrations"]
		}
		self.assertEqual(integrations, {"xlang": "integrated"})



	def test_every_source_root_has_one_system(self) -> None:
		"""每个实际源码根目录必须且只能属于一个体系。"""

		registered = [
			root
			for system in self.scope["systems"]
			for root in system["source_roots"]
		]
		actual = {
			path.name for path in (ROOT / "src").iterdir()
			if path.is_dir() and any(item.is_file() for item in path.rglob("*"))
		}
		self.assertEqual(len(registered), len(set(registered)))
		self.assertEqual(set(registered), actual)



	def test_scope_document_matches_manifest(self) -> None:
		"""公开范围表必须与清单中的体系和状态一致。"""

		text = (ROOT / "docs" / "SCOPE.md").read_text(encoding="utf-8")
		rows = re.findall(
			r"^\| `([a-z0-9_]+)` \| `([a-z]+)` \| `([^`]+)` \| ([^|]+) \|$",
			text,
			re.MULTILINE,
		)
		actual = {
			name: (state, roots, review.strip())
			for name, state, roots, review in rows
		}
		expected = {
			system["name"]: (
				system["state"],
				", ".join(system["source_roots"]),
				(", ".join(
					f"`{name}`" for name in system.get("review_modules", [])
				) or "-"),
			)
			for system in self.scope["systems"]
		}
		self.assertEqual(actual, expected)



	def test_xlang_repository_tools_are_not_xrt_assets(self) -> None:
		"""外部仓库同步器、专用迁移文档和 CI 门禁不得回流。"""

		for path in (
			"tools/check_xlang_api.py",
			"tools/test_xlang_api.py",
			"docs/XLANG_INTEGRATION.md",
		):
			self.assertFalse((ROOT / path).exists(), path)

		workflow = (ROOT / ".github/workflows/ci.yml").read_text(
			encoding="utf-8"
		)
		self.assertNotIn("test_xlang_api", workflow)
		for path in (ROOT / "docs").rglob("*.md"):
			text = path.read_text(encoding="utf-8")
			self.assertNotIn("D:\\GIT\\x-lang", text, path.as_posix())
			self.assertNotIn("demo6/", text, path.as_posix())



	def test_extension_collection_is_isolated_from_xrt_core(self) -> None:
		"""扩展库可以同仓保存，但不能进入 XRT 核心清单或生成物。"""

		extlibs = ROOT / "extlibs"
		self.assertTrue((extlibs / "xhttp" / "README.md").is_file())
		self.assertTrue((extlibs / "xws" / "README.md").is_file())
		for module in self.manifest["modules"]:
			for key in (
				"public_headers", "internal_headers", "sources", "tests",
				"single_tests", "examples", "docs",
			):
				for path in module.get(key, []):
					self.assertFalse(
						path.startswith("extlibs/"),
						f"{module['name']}: {path}",
					)



	def test_extension_integration_states_match_current_contract(self) -> None:
		"""扩展清单必须准确表达已经完成和仍然延期的 xlang 集成。"""

		expected = {
			"xruntime": "integrated",
			"xhttp": "deferred",
			"xws": "deferred",
			"xregex": "integrated",
			"xmail": "deferred",
			"xssh": "integrated",
		}
		for product, state in expected.items():
			manifest = load_manifest(
				ROOT / "extlibs" / product / "config" / "modules.json"
			)
			integrations = {
				item["name"]: item["state"]
				for item in manifest["scope"]["external_integrations"]
			}
			self.assertEqual(integrations, {"xlang": state}, product)



	def test_websocket_core_keeps_only_protocol_and_transport(self) -> None:
		"""WebSocket 核心只保留协议原语和 TCP/TLS 高性能通信链路。"""

		protocol = {
			"websocket_frame",
			"websocket_close",
			"websocket_message",
			"websocket_handshake",
			"websocket_keygen",
			"websocket_extension",
			"websocket_deflate",
			"websocket_inflater",
			"websocket_deflater",
			"websocket_deflate_runtime_tests",
			"websocket_protocol_fuzz_tests",
		}
		transport = {
			"websocket_stream",
			"websocket_stream_ref",
			"websocket_stream_iocp_tests",
			"websocket_stream_tls",
			"websocket_stream_tls_tests",
			"websocket_stream_tls_iocp_tests",
			"websocket_stream_deflate",
			"websocket_stream_tls_deflate_tests",
			"websocket_upgrade",
			"websocket_upgrade_deflate",
			"websocket_upgrade_stream",
			"websocket_upgrade_stream_deflate_tests",
			"websocket_upgrade_stream_tls_tests",
		}
		expected = protocol | transport
		modules = {
			item["name"]: item
			for item in self.manifest["modules"]
		}
		actual = {
			name for name in modules
			if name.startswith("websocket")
		}
		self.assertEqual(actual, expected)

		for root in protocol:
			pending = [root]
			closure = set()
			while pending:
				name = pending.pop()
				if name in closure:
					continue
				closure.add(name)
				pending.extend(modules[name].get("depends", []))

			for name in closure:
				self.assertFalse(
					name.startswith((
						"websocket_connection",
						"websocket_writer",
						"websocket_group",
						"net_",
						"tls_",
						"future",
						"coroutine",
						"task",
						"channel",
					)),
					f"{root}: {name}",
				)

		for root in transport:
			pending = [root]
			closure = set()
			while pending:
				name = pending.pop()
				if name in closure:
					continue
				closure.add(name)
				pending.extend(modules[name].get("depends", []))

			for name in closure:
				self.assertFalse(
					name.startswith((
						"websocket_connection",
						"websocket_writer",
						"websocket_group",
						"future",
						"coroutine",
						"task",
						"channel",
					)),
					f"{root}: {name}",
				)

		for path in (
			"include/xrt/websocket.h",
			"include/xrt/websocket_stream.h",
			"include/xrt/websocket_upgrade.h",
		):
			header = (ROOT / path).read_text(encoding="utf-8")
			for token in (
				"xwsconn",
				"xwswriter",
				"xwsgroup",
				"XRT_FEATURE_WEBSOCKET_CONNECTION",
				"XRT_FEATURE_WEBSOCKET_WRITER",
				"XRT_FEATURE_WEBSOCKET_GROUP",
			):
				self.assertNotIn(token, header, path)

		self.assertFalse((ROOT / "include" / "xrt" / "websocket_group.h").exists())

		archive_path = ROOT / "extlibs" / "xws" / "archive"
		archive = json.loads(
			(archive_path / "modules.json").read_text(encoding="utf-8")
		)
		archived = {item["name"] for item in archive["modules"]}
		self.assertTrue({
			"websocket_connection",
			"websocket_connection_future",
			"websocket_connection_tls",
			"websocket_writer",
			"websocket_group",
		}.issubset(archived))
		for path in (
			"include/xrt/websocket_group.h",
			"src/websocket/connection.c",
			"src/websocket/connection_future.c",
			"src/websocket/writer.c",
			"src/websocket/group.c",
		):
			self.assertTrue((archive_path / "xrt" / path).is_file(), path)



	def test_browser_cors_client_is_excluded(self) -> None:
		"""浏览器 CORS 客户端策略不能进入 XRT 发布面。"""

		public_assets = (
			"include/xrt/http_cors_safelist.h",
			"include/xrt/http_cors_client.h",
			"include/xrt/http_cors_cache.h",
			"src/internal/xrt_http_cors_client.h",
			"src/http/http_cors_safelist.c",
			"src/http/http_cors_client.c",
			"src/http/http_cors_client_write.c",
			"src/http/http_cors_cache.c",
		)
		for path in public_assets:
			self.assertFalse((ROOT / path).exists(), path)

		module_names = {item["name"] for item in self.manifest["modules"]}
		self.assertTrue({
			"http_cors",
			"http_cors_policy",
			"http_cors_write",
			"http_cors_safelist",
			"http_cors_client",
			"http_cors_cache",
		}.isdisjoint(module_names))




if __name__ == "__main__":
	unittest.main()
