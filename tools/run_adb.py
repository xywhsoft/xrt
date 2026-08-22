#!/usr/bin/env python3

"""把一个交叉编译测试部署到 Android 设备并返回真实退出状态。"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess



def _find_adb(value: str | None) -> str:
	"""解析显式路径、环境变量或 PATH 中的 adb。"""

	requested = value or os.environ.get("XRT_ADB") or "adb"
	path = shutil.which(requested)
	if path is None:
		raise SystemExit(
			"adb not found; pass --adb or set XRT_ADB"
		)
	return path



def _adb_prefix(adb: str, serial: str | None) -> list[str]:
	"""生成绑定可选设备序列号的 adb 命令前缀。"""

	command = [adb]
	if serial:
		command.extend(["-s", serial])
	return command



def _remote_name(executable: Path) -> str:
	"""为不同工具链和测试套件生成稳定且互不覆盖的远端文件名。"""

	identity = str(executable.resolve()).encode("utf-8")
	digest = hashlib.sha256(identity).hexdigest()[:12]
	return f"{digest}-{executable.name}"



def _local_path(adb: str, executable: Path) -> str:
	"""在 WSL 调用 Windows adb 时把本地文件转换为 Windows 路径。"""

	if (os.name == "posix") and adb.lower().endswith(".exe"):
		wslpath = shutil.which("wslpath")
		if wslpath is not None:
			result = subprocess.run(
				[wslpath, "-w", str(executable)],
				check=True,
				capture_output=True,
				text=True,
			)
			path = result.stdout.strip()
			if path:
				return path
	return str(executable)



def _run(command: list[str], *, check: bool = True) -> None:
	"""显示并执行一个 adb 命令。"""

	print("[adb]", subprocess.list2cmdline(command), flush=True)
	subprocess.run(command, check=check)



def main() -> int:
	"""部署一个测试，在设备临时目录运行，并传播测试退出码。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("--adb", help="adb 可执行文件路径")
	parser.add_argument(
		"--serial",
		default=os.environ.get("ANDROID_SERIAL"),
		help="目标设备序列号；默认读取 ANDROID_SERIAL",
	)
	parser.add_argument(
		"--remote-dir",
		default=os.environ.get("XRT_ADB_DIR", "/data/local/tmp/xrt-tests"),
		help="设备上的临时测试目录",
	)
	parser.add_argument(
		"--keep",
		action="store_true",
		help="测试结束后保留设备上的可执行文件",
	)
	parser.add_argument("executable", type=Path)
	args = parser.parse_args()

	executable = args.executable.resolve()
	if not executable.is_file():
		parser.error(f"test executable does not exist: {executable}")
	adb = _find_adb(args.adb)
	prefix = _adb_prefix(adb, args.serial)
	remote_name = _remote_name(executable)
	remote_path = args.remote_dir.rstrip("/") + "/" + remote_name
	local_path = _local_path(adb, executable)
	quoted_dir = shlex.quote(args.remote_dir)
	quoted_name = shlex.quote(remote_name)

	_run([*prefix, "get-state"])
	_run([*prefix, "shell", f"mkdir -p {quoted_dir}"])
	_run([*prefix, "push", local_path, remote_path])
	try:
		_run([
			*prefix,
			"shell",
			f"cd {quoted_dir} && chmod 700 {quoted_name} && ./{quoted_name}",
		])
	finally:
		if not args.keep:
			_run([
				*prefix, "shell", f"rm -f {shlex.quote(remote_path)}",
			], check=False)
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
