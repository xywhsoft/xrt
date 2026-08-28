#!/usr/bin/env python3

"""验证 XRT 发布库构建器的工具链边界。"""

from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import package as xrt_package



class CompilerFamilyTest(unittest.TestCase):
	"""验证编译器驱动分类不会混用参数口径。"""

	def test_known_families(self) -> None:
		"""cl、clang-cl、TCC 和 GNU 风格驱动必须明确区分。"""

		self.assertEqual(xrt_package._compiler_family("cl.exe"), "msvc")
		self.assertEqual(xrt_package._compiler_family("clang-cl.exe"), "msvc")
		self.assertEqual(xrt_package._compiler_family("tcc.exe"), "tcc")
		self.assertEqual(xrt_package._compiler_family("gcc.exe"), "gnu")

	def test_clang_cl_architecture_is_explicit(self) -> None:
		"""clang-cl 不能把宿主默认架构误当成发布目标。"""

		self.assertEqual(
			xrt_package._clang_cl_target("clang-cl.exe", "x86"),
			["--target=i686-pc-windows-msvc"],
		)
		self.assertEqual(
			xrt_package._clang_cl_target("clang-cl.exe", "x64"),
			["--target=x86_64-pc-windows-msvc"],
		)
		self.assertEqual(xrt_package._clang_cl_target("cl.exe", "x86"), [])



class ArtifactNameTest(unittest.TestCase):
	"""验证平台产物和导入库命名。"""

	def test_windows_shared_names(self) -> None:
		"""Windows 动态库按工具链生成对应导入库。"""

		self.assertEqual(
			xrt_package._artifact_names("shared", "windows", "gnu"),
			("xrt.dll", "libxrt.dll.a"),
		)
		self.assertEqual(
			xrt_package._artifact_names("shared", "windows", "msvc"),
			("xrt.dll", "xrt.lib"),
		)

	def test_posix_names(self) -> None:
		"""POSIX 静态库和动态库使用平台惯例名称。"""

		self.assertEqual(
			xrt_package._artifact_names("static", "linux", "gnu"),
			("libxrt.a", None),
		)
		self.assertEqual(
			xrt_package._artifact_names("shared", "linux", "gnu"),
			("libxrt.so", None),
		)

	def test_extension_product_names(self) -> None:
		"""扩展包必须使用自己的库名，不能覆盖核心产物。"""

		self.assertEqual(
			xrt_package._artifact_names(
				"static", "windows", "gnu", "xruntime"
			),
			("libxruntime.a", None),
		)
		self.assertEqual(
			xrt_package._artifact_names(
				"shared", "windows", "gnu", "xruntime"
			),
			("xruntime.dll", "libxruntime.dll.a"),
		)



class GnuArchiveTest(unittest.TestCase):
	"""验证旧版 GNU ar 不需要响应文件也能生成大归档。"""

	def test_large_archive_is_split_below_response_threshold(self) -> None:
		"""长对象列表必须分批追加，并在最后显式重建符号索引。"""

		with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
			xrt_package.xrt_build,
			"_run_compiler",
		) as run:
			root = Path(temporary)
			output = root / "libxrt.a"
			objects = [
				root / (f"object_{index:04d}_" + ("x" * 96) + ".o")
				for index in range(400)
			]
			xrt_package._archive_gnu("ar", objects, output)

		commands = [call.args[0] for call in run.call_args_list]
		self.assertGreater(len(commands), 2)
		self.assertEqual(commands[0][1], "rcs")
		self.assertTrue(all(command[1] == "r" for command in commands[1:-1]))
		self.assertEqual(commands[-1], ["ar", "s", str(output)])
		for command in commands[:-1]:
			self.assertLess(
				len(xrt_package.subprocess.list2cmdline(command)),
				xrt_package.ARCHIVE_COMMAND_LIMIT,
			)
		archived = [Path(item) for command in commands[:-1] for item in command[3:]]
		self.assertEqual(archived, objects)



class CommandTest(unittest.TestCase):
	"""验证归档和动态链接命令不混入另一类工具链语法。"""

	def test_gnu_shared_import_library(self) -> None:
		"""MinGW 动态库必须显式产出导入库。"""

		with tempfile.TemporaryDirectory() as temporary, \
			mock.patch.object(xrt_package.sys, "platform", "win32"), \
			mock.patch.object(
				xrt_package.xrt_build,
				"_run_compiler",
			) as run:
			root = Path(temporary)
			xrt_package._link_gnu_shared(
				"gcc",
				"x64",
				"gnu",
				[root / "a.o"],
				["ws2_32"],
				root / "xrt.dll",
				root / "libxrt.dll.a",
				[],
			)

		command = run.call_args.args[0]
		self.assertIn("-shared", command)
		self.assertIn("-m64", command)
		self.assertIn("-lws2_32", command)
		self.assertIn(
			[
				"-Xlinker",
				"--out-implib",
				"-Xlinker",
				str(root / "libxrt.dll.a"),
			],
			[
				command[index:index + 4]
				for index in range(len(command) - 3)
			],
		)
		self.assertFalse(any(
			item.startswith("-Wl,--out-implib,")
			for item in command
		))

	def test_shared_link_uses_platform_undefined_policy(self) -> None:
		"""ELF 与 Mach-O 动态库必须使用各自链接器的严格未定义符号参数。"""

		for platform, expected, rejected in [
			("linux", "-Wl,--no-undefined", "-Wl,-undefined,error"),
			("darwin", "-Wl,-undefined,error", "-Wl,--no-undefined"),
		]:
			with self.subTest(platform=platform), \
				tempfile.TemporaryDirectory() as temporary, \
				mock.patch.object(xrt_package.sys, "platform", platform), \
				mock.patch.object(
					xrt_package.xrt_build,
					"_run_compiler",
				) as run:
				root = Path(temporary)
				xrt_package._link_gnu_shared(
					"clang",
					"native",
					"gnu",
					[root / "a.o"],
					[],
					root / (
						"libxrt.dylib" if platform == "darwin" else "libxrt.so"
					),
					None,
					[],
				)

			command = run.call_args.args[0]
			self.assertIn(expected, command)
			self.assertNotIn(rejected, command)
			self.assertIn(
				"-dynamiclib" if platform == "darwin" else "-shared",
				command,
			)

	def test_msvc_options_export_shared_symbols(self) -> None:
		"""共享库对象必须通过公共导出宏编译。"""

		options = xrt_package._msvc_options(
			["XRT_FEATURE_CORE", "XRT_BUILD_SHARED"],
			[],
		)

		self.assertIn("/DXRT_BUILD_SHARED", options)
		self.assertIn("/WX", options)
		self.assertFalse(any(item.startswith("-Wl,") for item in options))

	def test_gnu_static_consumer_links_whole_archive(self) -> None:
		"""GNU 静态包消费者必须强制解析全部归档成员。"""

		with tempfile.TemporaryDirectory() as temporary, \
			mock.patch.object(xrt_package.sys, "platform", "linux"), \
			mock.patch.object(xrt_package, "ROOT", Path(temporary)), \
			mock.patch.object(
				xrt_package.xrt_build,
				"_run_compiler",
			) as run, mock.patch.object(xrt_package.subprocess, "run"):
			root = Path(temporary)
			xrt_package._verify_gnu(
				"gcc",
				"x64",
				"gnu",
				"static",
				root / "libxrt.a",
				None,
				["pthread"],
				[root / "a.o"],
			)

		command = run.call_args.args[0]
		self.assertIn("-Wl,--whole-archive", command)
		self.assertIn("-Wl,--no-whole-archive", command)
		self.assertIn(str(root / "libxrt.a"), command)

	def test_gnu_consumer_failure_persists_link_diagnostics(self) -> None:
		"""GNU 消费者链接失败时必须保留可供 CI 下载的诊断。"""

		with tempfile.TemporaryDirectory() as temporary, \
			mock.patch.object(xrt_package.sys, "platform", "linux"), \
			mock.patch.object(xrt_package, "ROOT", Path(temporary)), \
			mock.patch.object(
				xrt_package.xrt_build,
				"_run_compiler",
				side_effect=[
				xrt_package.subprocess.CalledProcessError(1, ["gcc"]),
				xrt_package.subprocess.CompletedProcess(
					["gcc"], 1, "undefined reference to strcmp",
				),
			],
			) as run:
			root = Path(temporary)
			with self.assertRaises(xrt_package.subprocess.CalledProcessError):
				xrt_package._verify_gnu(
					"gcc",
					"x86",
					"gnu",
					"static",
					root / "libxrt.a",
					None,
					[],
					[root / "a.o"],
				)

			log = root / "test_package_xrt.link.log"
			self.assertEqual(run.call_count, 2)
			self.assertIn("-v", run.call_args_list[1].args[0])
			self.assertIn(
				"undefined reference to strcmp",
				log.read_text("utf-8"),
			)

	def test_msvc_static_consumer_links_whole_archive(self) -> None:
		"""MSVC 静态包消费者必须强制解析全部归档成员。"""

		with tempfile.TemporaryDirectory() as temporary, \
			mock.patch.object(xrt_package, "ROOT", Path(temporary)), \
			mock.patch.object(
				xrt_package.xrt_build,
				"_run_compiler",
			) as run, mock.patch.object(xrt_package.subprocess, "run"):
			root = Path(temporary)
			xrt_package._verify_msvc(
				"cl",
				"x64",
				"static",
				root / "xrt.lib",
				None,
				["ws2_32"],
			)

		command = run.call_args.args[0]
		self.assertIn("/link", command)
		self.assertIn(f"/WHOLEARCHIVE:{root / 'xrt.lib'}", command)

	def test_tcc_windows_shared_verify_requires_import_library(self) -> None:
		"""不能把无法链接消费者的裸 Windows DLL 报告为已验证。"""

		with tempfile.TemporaryDirectory() as temporary, \
			mock.patch.object(xrt_package.sys, "platform", "win32"), \
			self.assertRaisesRegex(SystemExit, "requires an import library"):
			xrt_package._verify_gnu(
				"tcc",
				"x64",
				"tcc",
				"shared",
				Path(temporary) / "xrt.dll",
				None,
				[],
				[],
			)



if __name__ == "__main__":
	unittest.main()
