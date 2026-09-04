#!/usr/bin/env python3

"""验证 XRT 构建脚本的模块选择边界。"""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import amalgamate as xrt_amalgamate
import build as xrt_build
import generate_extension_features as xrt_extension_features
import generate_features as xrt_features
import xrt_manifest


CORE_TEST = "tests/single/test_single.c"
SELECTION_TEST = "tests/single/test_single_module_selection.c"



class ManifestDependencyTest(unittest.TestCase):
	"""验证扩展产品可以声明清晰、可递归的跨扩展依赖。"""

	def _write_manifest(
		self,
		root: Path,
		name: str,
		dependencies: list[str],
	) -> Path:
		"""写入一份只用于依赖图测试的最小扩展清单。"""

		path = root / "extlibs" / name / "config" / "modules.json"
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(json.dumps({
			"dependency_manifests": dependencies,
			"modules": [{
				"name": name,
				"feature": None,
				"depends": [],
				"sources": [],
				"tests": [],
			}],
		}),
			encoding="utf-8",
		)
		return path

	def test_dependencies_expand_before_product_once(self) -> None:
		"""递归依赖必须稳定去重，并把最终产品保持在列表末尾。"""

		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "config").mkdir()
			(root / "config" / "modules.json").write_text(
				'{"modules": [{"name": "core", "feature": null}]}',
				encoding="utf-8",
			)
			xhttp = self._write_manifest(root, "xhttp", [])
			xws = self._write_manifest(
				root,
				"xws",
				["extlibs/xhttp/config/modules.json"],
			)
			paths = xrt_manifest.expand_manifest_paths([xws, xhttp], root)

		self.assertEqual([xhttp.resolve(), xws.resolve()], paths)

	def test_dependency_cycle_is_rejected(self) -> None:
		"""清单循环必须在进入模块闭包前给出确定错误。"""

		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "config").mkdir()
			(root / "config" / "modules.json").write_text(
				'{"modules": [{"name": "core", "feature": null}]}',
				encoding="utf-8",
			)
			left = self._write_manifest(
				root,
				"left",
				["extlibs/right/config/modules.json"],
			)
			self._write_manifest(
				root,
				"right",
				["extlibs/left/config/modules.json"],
			)
			with self.assertRaisesRegex(ValueError, "dependency cycle"):
				xrt_manifest.expand_manifest_paths([left], root)

	def test_dependency_asset_collection_is_validated(self) -> None:
		"""聚合节点只能收集明确支持的测试资产类型。"""

		with tempfile.TemporaryDirectory() as temporary:
			path = Path(temporary) / "modules.json"
			path.write_text(json.dumps({
				"modules": [{
					"name": "invalid",
					"feature": None,
					"collect_dependency_assets": ["sources"],
				}],
			}), encoding="utf-8")

			with self.assertRaisesRegex(ValueError, "only accepts"):
				xrt_manifest.load_manifest(path)

	def test_all_exclude_macro_is_validated(self) -> None:
		"""MODULE_ALL 排除宏必须是可用于预处理器的公开宏名。"""

		with tempfile.TemporaryDirectory() as temporary:
			path = Path(temporary) / "modules.json"
			path.write_text(json.dumps({
				"modules": [{
					"name": "invalid",
					"feature": "XRT_FEATURE_INVALID",
					"all_exclude_macro": "invalid macro",
				}],
			}), encoding="utf-8")

			with self.assertRaisesRegex(ValueError, "uppercase C macro"):
				xrt_manifest.load_manifest(path)



class FeaturePlatformDependencyTest(unittest.TestCase):
	"""验证特性头与构建器使用相同的平台依赖回退规则。"""

	def test_posix_fallback_covers_unspecified_platforms(self) -> None:
		"""POSIX 回退应覆盖未显式声明的 Linux、BSD 和未知平台。"""

		condition = xrt_features._platform_test("posix", {
			"windows": [],
			"macos": [],
			"posix": ["file"],
		})

		self.assertIn("defined(__linux__)", condition)
		self.assertIn("defined(__FreeBSD__)", condition)
		self.assertIn("!defined(_WIN32)", condition)
		self.assertNotIn("\t(defined(_WIN32))", condition)
		self.assertNotIn(
			"\t(defined(__APPLE__) && defined(__MACH__))",
			condition,
		)

	def test_explicit_platform_keeps_direct_condition(self) -> None:
		"""显式平台依赖不得被 POSIX 回退条件扩大。"""

		self.assertEqual(
			xrt_features._platform_test("linux", {"linux": ["epoll"]}),
			"defined(__linux__) && !defined(__ANDROID__)",
		)

	def test_android_is_distinct_from_linux(self) -> None:
		"""Android 不得意外启用只在普通 Linux 验证过的依赖。"""

		self.assertEqual(
			xrt_features._platform_test("android", {"android": ["epoll"]}),
			"defined(__ANDROID__)",
		)
		condition = xrt_features._platform_test("posix", {
			"linux": ["epoll"],
			"posix": ["select"],
		})
		self.assertIn("defined(__ANDROID__)", condition)



class FeatureAllExclusionTest(unittest.TestCase):
	"""验证 MODULE_ALL 排除只影响隐式选择。"""

	def test_explicit_module_wins_over_all_exclusion(self) -> None:
		"""显式根模块即使存在排除宏也必须继续启用。"""

		manifest = {"modules": [{
			"name": "debug",
			"feature": "XRT_FEATURE_DEBUG",
			"all_exclude_macro": "XRT_EXCLUDE_DEBUG",
			"depends": [],
		}]}
		with mock.patch.object(
			xrt_features,
			"load_manifest",
			return_value=manifest,
		):
			content = xrt_features._content()

		self.assertIn(
			"#if (defined(XRT_MODULE_ALL) && "
			"!defined(XRT_EXCLUDE_DEBUG)) || \\\n"
			"\tdefined(XRT_MODULE_DEBUG)",
			content,
		)

	def test_extension_all_uses_the_same_exclusion_contract(self) -> None:
		"""扩展模块清单不能丢失共享的 ALL 排除语义。"""

		condition = xrt_extension_features._selection_test(
			"XDEMO_MODULE_ALL",
			"XDEMO_MODULE_DEBUG",
			{"all_exclude_macro": "XDEMO_EXCLUDE_DEBUG"},
		)

		self.assertEqual(
			condition,
			"(defined(XDEMO_MODULE_ALL) && "
			"!defined(XDEMO_EXCLUDE_DEBUG)) || \\\n"
			"\tdefined(XDEMO_MODULE_DEBUG)",
		)



class AggregateAssetTest(unittest.TestCase):
	"""验证产品测试聚合不会把核心或其他扩展的资产误纳入。"""

	def test_collects_only_same_product_dependency_assets(self) -> None:
		"""聚合节点应收集闭包内同前缀模块，并保留显式根资产。"""

		core = {
			"name": "core",
			"_module_prefix": "XRT_MODULE_",
		}
		feature = {
			"name": "feature",
			"_module_prefix": "XDEMO_MODULE_",
		}
		aggregate = {
			"name": "demo_tests",
			"_module_prefix": "XDEMO_MODULE_",
			"collect_dependency_assets": ["tests", "single_tests"],
		}
		assets = xrt_build._asset_modules(
			[core, feature, aggregate],
			[aggregate],
		)

		self.assertEqual(assets["tests"], [aggregate, feature])
		self.assertEqual(assets["single_tests"], [aggregate, feature])
		self.assertEqual(assets["examples"], [aggregate])

	def test_runtime_capability_exclusions_match_each_selector_form(self) -> None:
		"""能力受限平台应能透明排除路径、文件名或名称 glob。"""

		tests = [
			"tests/network/test_net_port_uring.c",
			"tests/network/test_net_file.c",
			"tests/network/test_net_tcp.c",
		]
		self.assertEqual(
			xrt_build._exclude_tests(
				tests,
				["*uring*", "test_net_file.c"],
			),
			["tests/network/test_net_tcp.c"],
		)
		self.assertEqual(
			xrt_build._exclude_tests(tests, ["test_net_tcp"]),
			tests[:2],
		)

	def test_platform_assets_are_filtered_after_collection(self) -> None:
		"""平台专用后端资产不能泄漏到其他平台的 all 门禁。"""

		portable = {
			"name": "portable",
			"_module_prefix": "XRT_MODULE_",
		}
		windows = {
			"name": "windows_tests",
			"_module_prefix": "XRT_MODULE_",
			"asset_platforms": ["windows"],
		}
		posix = {
			"name": "posix_tests",
			"_module_prefix": "XRT_MODULE_",
			"asset_platforms": ["posix"],
		}
		assets = xrt_build._asset_modules(
			[portable, windows, posix],
			[portable, windows, posix],
			platform="linux",
		)

		self.assertEqual(assets["tests"], [portable, posix])

	def test_platform_owner_blocks_aggregate_duplicate(self) -> None:
		"""聚合节点不得让平台专属测试从通用列表重新漏入。"""

		aggregate = {
			"tests": ["test_portable.c", "test_iocp.c"],
		}
		windows = {
			"tests": ["test_iocp.c"],
			"asset_platforms": ["windows"],
		}

		self.assertEqual(
			xrt_build._asset_paths(
				[aggregate, windows],
				[aggregate],
				"tests",
				platform="linux",
			),
			["test_portable.c"],
		)

	def test_android_uses_epoll_without_uring(self) -> None:
		"""Android 网络引擎应保留 epoll/select，但不能继承 io_uring。"""

		(
			sources, _tests, _single_tests, _examples, _defines, _links,
			_test_modules, _by_name, _include_dirs, _header_roots,
		) = xrt_build._load_modules("net_engine", platform="android")

		self.assertIn("src/network/port_epoll.c", sources)
		self.assertIn("src/network/port_select.c", sources)
		self.assertNotIn("src/network/port_uring.c", sources)



class ExternalRunnerTest(unittest.TestCase):
	"""验证交叉编译测试通过显式 runner 执行。"""

	def test_runner_receives_executable_as_last_argument(self) -> None:
		"""构建器不得让 runner 猜测产物目录或测试文件名。"""

		path = xrt_build.ROOT / "out" / "target" / "test_case"
		with mock.patch.object(xrt_build.subprocess, "run") as run:
			xrt_build._run(path, ["python3", "tools/run_adb.py"])

		run.assert_called_once_with(
			["python3", "tools/run_adb.py", str(path)],
			cwd=xrt_build.ROOT,
			check=True,
		)



class AmalgamateLicenseTest(unittest.TestCase):
	"""验证单头文件始终携带仓库许可声明。"""

	def test_license_banner_uses_authoritative_license(self) -> None:
		"""许可注释必须逐行来自根目录 LICENSE，而不是维护副本。"""

		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "LICENSE").write_text(
				"MIT License\n\nCopyright test\n",
				encoding="utf-8",
			)
			with mock.patch.object(xrt_amalgamate, "ROOT", root):
				banner = xrt_amalgamate._license_banner()

		self.assertEqual(
			banner,
			"/*\n * MIT License\n *\n * Copyright test\n */\n",
		)



class AmalgamateContractTest(unittest.TestCase):
	"""验证单头生成不会吞掉依赖或修改只读检查目标。"""

	def test_implementation_guard_has_clean_continuations(self) -> None:
		"""多所有者实现的续行只能包含条件，不得注入额外运算符。"""

		self.assertEqual(
			xrt_amalgamate._guard_begin(("XRT_FEATURE_A",)),
			"#if defined(XRT_FEATURE_A)\n",
		)
		self.assertEqual(
			xrt_amalgamate._guard_begin((
				"XRT_FEATURE_A",
				"XRT_FEATURE_B",
			)),
			"#if defined(XRT_FEATURE_A) || \\\n"
			"\tdefined(XRT_FEATURE_B)\n",
		)

	def test_extension_declaration_header_is_self_contained(self) -> None:
		"""扩展声明单头不得残留对核心模块化聚合头的 include。"""

		output, content = xrt_amalgamate._declaration_content([
			Path("extlibs/xruntime/config/modules.json"),
		])

		self.assertEqual(
			output,
			xrt_amalgamate.ROOT / "extlibs/xruntime/single/xruntime_decl.h",
		)
		self.assertNotIn("#include <xrt.h>", content)
		self.assertIn("#define XRT_DECLARATIONS 1", content)
		self.assertIn("#define XRUNTIME_DECLARATIONS 1", content)
		self.assertLess(
			content.index("#define XRT_DECLARATIONS 1"),
			content.index("public: include/xrt/memory.h"),
		)
		self.assertLess(
			content.index("feature selection: extlibs/xruntime/include/xruntime/features.h"),
			content.index("public: extlibs/xruntime/include/xrt/runtime_call.h"),
		)

	def test_core_implementation_is_unconditional(self) -> None:
		"""Core 实现必须始终存在，不能要求调用方选择模块。"""

		core = {
			"name": "core",
			"feature": None,
			"depends": [],
			"platform_depends": {},
			"internal_headers": ["src/internal/core.h"],
			"sources": ["src/core.c"],
		}
		guards = xrt_amalgamate._implementation_guards([core])

		self.assertIsNone(guards["src/internal/core.h"])
		self.assertIsNone(guards["src/core.c"])

	def test_shared_implementation_uses_owner_union(self) -> None:
		"""共享实现只在任一真实功能启用时进入翻译单元。"""

		modules = [
			{
				"name": "core",
				"feature": None,
				"depends": [],
				"platform_depends": {},
				"internal_headers": [],
				"sources": [],
			},
			{
				"name": "feature_a",
				"feature": "XRT_FEATURE_A",
				"depends": ["core"],
				"platform_depends": {},
				"internal_headers": ["src/internal/shared.h"],
				"sources": ["src/shared.c"],
			},
			{
				"name": "feature_b",
				"feature": "XRT_FEATURE_B",
				"depends": ["core"],
				"platform_depends": {},
				"internal_headers": ["src/internal/shared.h"],
				"sources": ["src/shared.c"],
			},
		]
		guards = xrt_amalgamate._implementation_guards(modules)

		self.assertEqual(
			guards["src/shared.c"],
			("XRT_FEATURE_A", "XRT_FEATURE_B"),
		)

	def test_support_implementation_uses_nearest_features(self) -> None:
		"""无公开宏支撑模块必须由最近的可选择使用者激活。"""

		modules = [
			{
				"name": "core",
				"feature": None,
				"depends": [],
				"platform_depends": {},
				"internal_headers": [],
				"sources": [],
			},
			{
				"name": "support",
				"feature": None,
				"depends": ["core"],
				"platform_depends": {},
				"internal_headers": ["src/internal/support.h"],
				"sources": ["src/support.c"],
			},
			{
				"name": "feature_a",
				"feature": "XRT_FEATURE_A",
				"depends": ["support"],
				"platform_depends": {},
				"internal_headers": [],
				"sources": [],
			},
			{
				"name": "aggregate",
				"feature": None,
				"depends": ["support"],
				"platform_depends": {},
				"internal_headers": [],
				"sources": [],
			},
			{
				"name": "feature_b",
				"feature": "XRT_FEATURE_B",
				"depends": ["aggregate"],
				"platform_depends": {},
				"internal_headers": [],
				"sources": [],
			},
		]
		guards = xrt_amalgamate._implementation_guards(modules)

		self.assertEqual(
			guards["src/support.c"],
			("XRT_FEATURE_A", "XRT_FEATURE_B"),
		)

	def test_missing_local_include_is_rejected(self) -> None:
		"""被展开规则识别的本地头必须已经登记到清单。"""

		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "src").mkdir()
			(root / "src/main.c").write_text(
				"#include <xrt/missing.h>\n",
				encoding="utf-8",
			)
			with mock.patch.object(xrt_amalgamate, "ROOT", root), \
				self.assertRaisesRegex(ValueError, "missing.h"):
				xrt_amalgamate._validate_local_includes(
					["src/main.c"],
					set(),
				)

	def test_check_rejects_stale_feature_header_first(self) -> None:
		"""特性头过期时不能用同样过期的内容误判单头为最新。"""

		with mock.patch.object(
			xrt_amalgamate, "check_features", return_value=False
		), mock.patch.object(
			xrt_amalgamate, "_content"
		) as content, mock.patch.object(
			xrt_amalgamate, "_declaration_content"
		) as declarations:
			self.assertFalse(xrt_amalgamate.check())
			content.assert_not_called()
			declarations.assert_not_called()

	def test_check_is_read_only(self) -> None:
		"""检查过期生成物只返回失败，不覆盖调用方文件。"""

		with tempfile.TemporaryDirectory() as temporary:
			output = Path(temporary) / "xrt.h"
			declarations = Path(temporary) / "xrt_decl.h"
			output.write_text("old\n", encoding="utf-8")
			declarations.write_text("old declarations\n", encoding="utf-8")
			with mock.patch.object(
				xrt_amalgamate, "check_features", return_value=True
			), mock.patch.object(
					xrt_amalgamate,
					"_content",
					return_value=(output, "new\n"),
				), mock.patch.object(
					xrt_amalgamate,
					"_declaration_content",
					return_value=(declarations, "new declarations\n"),
				):
				self.assertFalse(xrt_amalgamate.check())

			self.assertEqual(output.read_text(encoding="utf-8"), "old\n")
			self.assertEqual(
				declarations.read_text(encoding="utf-8"),
				"old declarations\n",
			)

	def test_declaration_header_has_no_implementation(self) -> None:
		"""声明单头必须复用公共头且不携带内部实现。"""

		output, content = xrt_amalgamate._declaration_content()

		self.assertEqual(output.name, "xrt_decl.h")
		self.assertIn("#define XRT_DECLARATIONS 1", content)
		self.assertIn("public: include/xrt/core.h", content)
		self.assertNotIn("XRT_IMPLEMENTATION_ONCE", content)
		self.assertNotIn("source: src/", content)
		self.assertIn("#define XRT_MODULE_ALL 1", content)
		self.assertIn("#undef XRT_MODULE_ALL", content)
		self.assertIn(
			"#if defined(XRT_FEATURE_MEMORY_DEBUG) && !defined(XRT_DECLARATIONS)",
			content,
		)

	def test_xruntime_overlay_has_independent_single_contract(self) -> None:
		"""扩展单头必须先解析扩展宏，并使用独立实现边界。"""

		manifest = (
			xrt_amalgamate.ROOT / "extlibs" / "xruntime" /
			"config" / "modules.json"
		)
		output, content = xrt_amalgamate._content([manifest])

		self.assertEqual(output.name, "xruntime.h")
		self.assertIn("#define XRUNTIME_SINGLE_HEADER 1", content)
		self.assertIn("#if defined(XRUNTIME_IMPLEMENTATION)", content)
		self.assertLess(
			content.index("xruntime/include/xruntime/features.h"),
			content.index("include/xrt/features.h"),
		)
		self.assertNotIn("../../../../src/internal", content)

	def test_xruntime_single_owner_uses_extension_namespace(self) -> None:
		"""产品聚合测试必须按显式 owner 选择最小扩展闭包。"""

		manifest = (
			xrt_build.ROOT / "extlibs" / "xruntime" /
			"config" / "modules.json"
		)
		(
			_, _, single_tests, _, _, _, test_modules, by_name, _, _,
		) = xrt_build._load_modules("xruntime", [manifest])
		defines = xrt_build._single_test_defines(
			single_tests,
			test_modules,
			by_name,
		)
		test = (
			"extlibs/xruntime/tests/single/"
			"test_single_runtime_type.c"
		)
		self.assertEqual(
			defines[test],
			["XRUNTIME_MODULE_RUNTIME_TYPE"],
		)

	def test_declaration_selection_restores_existing_state(self) -> None:
		"""完整声明的临时模块选择必须保留调用方已有宏。"""

		macros = ["XRT_FEATURE_ALPHA", "XRT_MODULE_ALL"]
		excludes = ["XRT_EXCLUDE_MEMORY_DEBUG"]
		begin = xrt_amalgamate._declaration_selection_begin(
			macros,
			exclude_macros=excludes,
		)
		end = xrt_amalgamate._declaration_selection_end(macros, excludes)

		self.assertIn(
			"#if !defined(XRT_FEATURE_ALPHA)",
			begin,
		)
		self.assertIn(
			"#if defined(XRT_DECLARATIONS_RESTORE_XRT_MODULE_ALL)",
			begin,
		)
		self.assertIn(
			"#if defined(XRT_DECLARATIONS_RESTORE_XRT_FEATURE_ALPHA)",
			end,
		)
		self.assertIn("#undef XRT_FEATURE_ALPHA", end)
		self.assertIn("#undef XRT_EXCLUDE_MEMORY_DEBUG", begin)
		self.assertIn("#define XRT_EXCLUDE_MEMORY_DEBUG 1", end)




class SingleTestDefinesTest(unittest.TestCase):
	"""验证单头测试归属与需要注入的模块宏彼此独立。"""

	def test_unconditional_core_needs_no_macro(self) -> None:
		"""无条件编入单头文件的基础层允许拥有空宏集合。"""

		core = {
			"name": "core",
			"feature": None,
			"depends": [],
			"single_tests": [CORE_TEST],
		}
		result = xrt_build._single_test_defines(
			[CORE_TEST], [core], {"core": core}
		)

		self.assertEqual(result[CORE_TEST], [])

	def test_feature_module_injects_owner_macro(self) -> None:
		"""功能模块测试必须得到对应的公开模块宏。"""

		feature = {
			"name": "string_split",
			"feature": "XRT_FEATURE_STRING_SPLIT",
			"depends": [],
			"single_tests": [CORE_TEST],
		}
		result = xrt_build._single_test_defines(
			[CORE_TEST], [feature], {"string_split": feature}
		)

		self.assertEqual(result[CORE_TEST], ["XRT_MODULE_STRING_SPLIT"])

	def test_source_selected_macro_is_not_injected_twice(self) -> None:
		"""测试源码已经声明模块宏时不再通过命令行重复注入。"""

		feature = {
			"name": "string_split",
			"feature": "XRT_FEATURE_STRING_SPLIT",
			"depends": [],
			"single_tests": [SELECTION_TEST],
		}
		result = xrt_build._single_test_defines(
			[SELECTION_TEST], [feature], {"string_split": feature}
		)

		self.assertEqual(result[SELECTION_TEST], [])

	def test_aggregate_uses_feature_dependencies(self) -> None:
		"""无功能宏的聚合节点必须穿透到真实功能根模块。"""

		feature = {
			"name": "string_split",
			"feature": "XRT_FEATURE_STRING_SPLIT",
			"depends": [],
			"single_tests": [],
		}
		aggregate = {
			"name": "string_tests",
			"feature": None,
			"depends": ["string_split"],
			"single_tests": [CORE_TEST],
		}
		result = xrt_build._single_test_defines(
			[CORE_TEST],
			[aggregate],
			{"string_split": feature, "string_tests": aggregate},
		)

		self.assertEqual(result[CORE_TEST], ["XRT_MODULE_STRING_SPLIT"])

	def test_unclaimed_test_is_rejected(self) -> None:
		"""不属于所选模块清单的单头测试必须明确失败。"""

		with self.assertRaisesRegex(ValueError, "no selected owner"):
			xrt_build._single_test_defines([CORE_TEST], [], {})



class CompilerFlagsTest(unittest.TestCase):
	"""验证通用编译参数和链接专用参数保持清晰边界。"""

	def test_link_command_receives_both_flag_groups(self) -> None:
		"""编译参数参与最终链接，链接参数只追加到链接命令。"""

		with tempfile.TemporaryDirectory() as temporary, \
			mock.patch.object(xrt_build, "ROOT", Path(temporary)), \
			mock.patch.object(xrt_build, "_run_compiler") as run:
			root = Path(temporary)
			(root / "main.c").write_text(
				"int main(void) { return 0; }\n",
				encoding="utf-8",
			)
			xrt_build._compile_program(
				"fake-gcc",
				"native",
				"main.c",
				[],
				[],
				[],
				root / "main",
				["-fsanitize=address"],
				["-Wl,--gc-sections"],
			)

		command = run.call_args.args[0]
		self.assertIn("-fsanitize=address", command)
		self.assertIn("-Wl,--gc-sections", command)



class ResponseArgumentTest(unittest.TestCase):
	"""验证 GNU 与 MSVC 响应文件不会混淆绝对路径和选项。"""

	def test_posix_absolute_path_with_spaces_is_quoted(self) -> None:
		"""POSIX 的 `/` 是路径根，不能按 MSVC 选项原样写出。"""

		self.assertEqual(
			xrt_build._response_argument("/tmp/xrt objects/core.o"),
			'"/tmp/xrt objects/core.o"',
		)

	def test_msvc_path_option_quotes_only_the_value(self) -> None:
		"""MSVC 路径选项只引用值，不能把选项名伪装成文件名。"""

		self.assertEqual(
			xrt_build._response_argument(
				"/OUT:C:\\xrt release\\xrt.dll",
				True,
			),
			'/OUT:"C:/xrt release/xrt.dll"',
		)
		self.assertEqual(
			xrt_build._response_argument("/nologo", True),
			"/nologo",
		)

	def test_response_syntax_uses_driver_name(self) -> None:
		"""绝对 GNU 编译器路径仍使用 GNU 词法。"""

		self.assertFalse(
			xrt_build._response_uses_msvc_syntax("/usr/bin/clang")
		)
		self.assertTrue(
			xrt_build._response_uses_msvc_syntax("C:/LLVM/bin/clang-cl.exe")
		)



class ObjectCacheTest(unittest.TestCase):
	"""验证对象缓存只重建受实现或共享头文件影响的编译单元。"""

	def setUp(self) -> None:
		"""建立不调用真实编译器的最小源码树。"""

		self.temporary = tempfile.TemporaryDirectory()
		self.root = Path(self.temporary.name)
		(self.root / "include").mkdir()
		(self.root / "src").mkdir()
		(self.root / "src/a.c").write_text("int a;\n", encoding="utf-8")
		(self.root / "src/b.c").write_text("int b;\n", encoding="utf-8")
		(self.root / "include/x.h").write_text("#define X 1\n", encoding="utf-8")
		self.compiler = self.root / "fake-gcc.exe"
		self.compiler.write_bytes(b"compiler")
		self.object_dir = self.root / "objects"
		self.commands: list[list[str]] = []

	def tearDown(self) -> None:
		"""释放临时源码树。"""

		self.temporary.cleanup()

	def compile(self, extra_cflags: list[str] | None = None) -> list[Path]:
		"""记录编译命令并生成足以复用的占位对象。"""

		def run(command: list[str], response: Path) -> None:
			self.assertIsInstance(response, Path)
			self.commands.append(command)
			Path(command[-1]).write_bytes(b"object")

		with mock.patch.object(xrt_build, "ROOT", self.root), \
			mock.patch.object(xrt_build, "_run_compiler", run):
			return xrt_build._compile_objects(
				str(self.compiler),
				"native",
				["src/a.c", "src/b.c"],
				[],
				self.object_dir,
				False,
				extra_cflags,
			)

	def test_source_change_rebuilds_one_object(self) -> None:
		"""单个实现变化不能使整个套件对象缓存失效。"""

		self.compile()
		self.commands.clear()
		(self.root / "src/a.c").write_text("int a = 1;\n", encoding="utf-8")
		self.compile()

		self.assertEqual(len(self.commands), 1)
		self.assertTrue(self.commands[0][-1].endswith("src__a.o"))

	def test_header_change_rebuilds_all_objects(self) -> None:
		"""共享头文件变化仍保守重建整个对象闭包。"""

		self.compile()
		self.commands.clear()
		(self.root / "include/x.h").write_text("#define X 2\n", encoding="utf-8")
		self.compile()

		self.assertEqual(len(self.commands), 2)

	def test_compiler_flag_change_rebuilds_all_objects(self) -> None:
		"""Sanitizer 等附加参数变化时不能复用普通构建对象。"""

		self.compile()
		self.commands.clear()
		self.compile(["-fsanitize=address"])

		self.assertEqual(len(self.commands), 2)



class TrimDependencyTest(unittest.TestCase):
	"""验证裁剪依赖探针能够一次报告全部缺失守卫。"""

	def test_collects_all_missing_guards(self) -> None:
		"""多个缺失直接依赖不能被首个失败提前遮蔽。"""

		dependency_a = {
			"name": "dependency_a",
			"feature": "XRT_FEATURE_DEPENDENCY_A",
			"depends": [],
		}
		dependency_b = {
			"name": "dependency_b",
			"feature": "XRT_FEATURE_DEPENDENCY_B",
			"depends": [],
		}
		target = {
			"name": "target",
			"feature": "XRT_FEATURE_TARGET",
			"depends": ["dependency_a", "dependency_b"],
		}
		by_name = {
			"dependency_a": dependency_a,
			"dependency_b": dependency_b,
			"target": target,
		}

		with tempfile.TemporaryDirectory() as temporary, \
			mock.patch.object(
				xrt_build,
				"_run_compiler",
				return_value=mock.Mock(returncode=0),
			) as run, self.assertRaises(SystemExit) as raised:
			xrt_build._check_trim_dependencies(
				"fake-gcc",
				"native",
				[target],
				by_name,
				Path(temporary),
			)

		message = str(raised.exception)
		self.assertIn("target -> dependency_a", message)
		self.assertIn("target -> dependency_b", message)
		self.assertEqual(run.call_count, 3)



if __name__ == "__main__":
	unittest.main()
