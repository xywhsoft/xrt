#!/usr/bin/env python3

"""提供发布报告共用的工具版本、Git 状态和内容指纹。"""

from __future__ import annotations

import hashlib
from pathlib import Path
import shutil
import subprocess



def tool_version(command: str, root: Path) -> str:
	"""读取工具版本首行，失败时退回可识别的文件名。"""

	result = subprocess.run(
		[command, "--version"],
		cwd=root,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		text=True,
		encoding="utf-8",
		errors="replace",
		check=False,
	)
	for line in result.stdout.splitlines():
		if line.strip():
			return line.strip()
	return Path(command).name



def git_state(root: Path) -> tuple[str | None, bool | None]:
	"""读取 Git 修订和工作树状态；非 Git 环境返回未知。"""

	git = shutil.which("git")
	if git is None:
		return None, None
	revision = subprocess.run(
		[git, "rev-parse", "HEAD"],
		cwd=root,
		stdout=subprocess.PIPE,
		stderr=subprocess.DEVNULL,
		text=True,
		encoding="ascii",
		errors="replace",
		check=False,
	)
	if revision.returncode != 0:
		return None, None
	status = subprocess.run(
		[git, "status", "--porcelain", "--untracked-files=normal"],
		cwd=root,
		stdout=subprocess.PIPE,
		stderr=subprocess.DEVNULL,
		text=True,
		encoding="utf-8",
		errors="replace",
		check=False,
	)
	return revision.stdout.strip() or None, (
		None if status.returncode != 0 else bool(status.stdout)
	)



def fingerprint_paths(root: Path, paths: list[Path]) -> str:
	"""按仓库相对路径和文件内容生成稳定的 SHA-256 指纹。"""

	resolved_root = root.resolve()
	files: dict[str, Path] = {}
	for path in paths:
		resolved = path.resolve()
		try:
			relative = resolved.relative_to(resolved_root).as_posix()
		except ValueError as error:
			raise SystemExit(f"fingerprint path is outside repository: {path}") from error
		if not resolved.is_file():
			raise SystemExit(f"fingerprint input is missing: {relative}")
		files[relative] = resolved

	digest = hashlib.sha256()
	for relative in sorted(files):
		digest.update(relative.encode("utf-8"))
		digest.update(b"\0")
		digest.update(files[relative].read_bytes())
		digest.update(b"\0")
	return digest.hexdigest()
