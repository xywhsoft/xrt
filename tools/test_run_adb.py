#!/usr/bin/env python3

"""验证 Android 外部测试运行器的无设备逻辑。"""

from __future__ import annotations

from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_adb



class AdbRunnerTest(unittest.TestCase):
	"""验证设备选择和远端命名保持确定。"""

	def test_serial_is_inserted_before_subcommand(self) -> None:
		"""显式设备必须约束 runner 发出的每条 adb 命令。"""

		self.assertEqual(
			run_adb._adb_prefix("adb", "device-1"),
			["adb", "-s", "device-1"],
		)
		self.assertEqual(run_adb._adb_prefix("adb", None), ["adb"])

	def test_remote_name_is_stable_and_keeps_test_name(self) -> None:
		"""同一测试应稳定命名，同时保留可读的本地文件名。"""

		path = Path("out/android/coroutine/test_coroutine")
		first = run_adb._remote_name(path)

		self.assertEqual(first, run_adb._remote_name(path))
		self.assertTrue(first.endswith("-test_coroutine"))
		self.assertEqual(len(first.split("-", 1)[0]), 12)



	def test_wsl_converts_local_path_for_windows_adb(self) -> None:
		"""WSL 调用 Windows adb 时必须传入 Windows 可识别的文件路径。"""

		completed = mock.Mock(stdout="D:\\GIT\\xrt\\out\\test_coroutine\n")
		with mock.patch.object(run_adb.os, "name", "posix"), \
			 mock.patch.object(run_adb.shutil, "which", return_value="/usr/bin/wslpath"), \
			 mock.patch.object(run_adb.subprocess, "run", return_value=completed) as run:
			path = run_adb._local_path(
				"/mnt/e/android/adb.exe",
				Path("/mnt/d/GIT/xrt/out/test_coroutine"),
			)

		self.assertEqual(path, "D:\\GIT\\xrt\\out\\test_coroutine")
		run.assert_called_once_with(
			[
				"/usr/bin/wslpath",
				"-w",
				"/mnt/d/GIT/xrt/out/test_coroutine",
			],
			check=True,
			capture_output=True,
			text=True,
		)



	def test_native_adb_keeps_local_path(self) -> None:
		"""原生 adb 必须继续接收宿主平台自己的路径。"""

		path = Path("/tmp/test_coroutine")
		with mock.patch.object(run_adb.os, "name", "posix"):
			self.assertEqual(run_adb._local_path("adb", path), str(path))



if __name__ == "__main__":
	unittest.main()
