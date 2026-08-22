#!/usr/bin/env python3

"""清理 XRT 工作树中可以重新生成的构建和测试产物。"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys


ROOT = Path(__file__).resolve().parents[1]

GENERATED_ROOT_DIRS = (
	"build",
	"out",
	"release",
)

GENERATED_ROOT_FILES = (
	"example_logger_file.log",
	"example_logger_json.log",
	"example_logger_text.log",
	"xrt-async-example.txt",
	"xrt-async-whole-example.txt",
	"xrt-process-output.txt",
)

CACHE_DIR_NAMES = (
	"__pycache__",
	".mypy_cache",
	".pytest_cache",
)

HISTORICAL_DEV_DIRS = (
	"dev/linux-debug-merge",
	"dev/log",
	"dev/net",
	"dev/net-v1",
	"dev/regex",
	"dev/template",
	"dev/test",
)

HISTORICAL_FILES = (
	"HANDOFF_XRT_XLANG_REFACTOR.md",
	"dev/MODULE_ASSESSMENT.md",
	"dev/XRT_API_FREEZE_PLAN.md",
	"dev/XRT_ROADMAP_NEXT.md",
	"dev/XSON_DESIGN_DRAFT.md",
	"dev/run_coroutine_test_linux_tcc.sh",
	"dev/rwlock_api.h",
	"dev/rwlock_impl.c",
	"dev/safestruct_examples.c",
	"dev/test_coroutine_core.c",
	"dev/test_rwlock.h",
	"dev/xnet2_tls_test_cert.pem",
	"dev/xnet2_tls_test_key.pem",
	"dev/代码风格整理任务.md",
	"dev/恢复指令.txt",
	"dev/bench/xnet2/bench_echo_tcp.exe",
	"dev/bench/xnet2/bench_echo_tls.exe",
	"dev/bench/xnet2/bench_send_queue_pressure.exe",
)

LEGACY_ARTIFACT_SUFFIXES = frozenset((
	".a",
	".dll",
	".err",
	".exe",
	".exp",
	".i",
	".ilk",
	".lib",
	".log",
	".o",
	".obj",
	".pdb",
	".pyc",
	".so",
	".tmp",
))



def _inside(root: Path, path: Path) -> bool:
	"""判断解析后的路径是否严格位于工作树内部。"""

	try:
		path.resolve().relative_to(root.resolve())
	except ValueError:
		return False
	return path.resolve() != root.resolve()



def _path_size(path: Path) -> int:
	"""计算普通文件或目录中的可见文件总字节数。"""

	if path.is_symlink():
		return 0
	if path.is_file():
		return path.stat().st_size
	return sum(
		item.stat().st_size
		for item in path.rglob("*")
		if item.is_file() and not item.is_symlink()
	)



def collect(root: Path, history: bool = False) -> list[Path]:
	"""收集清单允许删除的现有生成物。"""

	root = root.resolve()
	items: set[Path] = set()

	for name in GENERATED_ROOT_DIRS:
		path = root / name
		if path.exists() or path.is_symlink():
			items.add(path)

	for name in GENERATED_ROOT_FILES:
		path = root / name
		if path.exists() or path.is_symlink():
			items.add(path)

	for pattern in (".xrt-*",):
		items.update(root.glob(pattern))

	dev = root / "dev"
	if dev.is_dir():
		items.update(dev.glob("_tmp_*"))

	for name in CACHE_DIR_NAMES:
		items.update(
			path for path in root.rglob(name)
			if ".git" not in path.parts
		)

	if history:
		for name in HISTORICAL_DEV_DIRS + HISTORICAL_FILES:
			path = root / name
			if path.exists() or path.is_symlink():
				items.add(path)

		legacy = root / "dev" / "ver1"
		if legacy.is_dir():
			items.update(
				path for path in legacy.rglob("*")
				if path.is_file() and path.suffix.lower() in LEGACY_ARTIFACT_SUFFIXES
			)

	return sorted(
		(path for path in items if _inside(root, path)),
		key=lambda path: path.as_posix(),
	)



def clean(root: Path, apply: bool, history: bool = False) -> tuple[int, int]:
	"""预览或删除生成物，返回项目数和总字节数。"""

	root = root.resolve()
	items = collect(root, history)
	total = sum(_path_size(path) for path in items)

	for path in items:
		print(f"[{'remove' if apply else 'clean'}] {path.relative_to(root)}")
		if not apply:
			continue
		if path.is_symlink() or path.is_file():
			path.unlink()
		else:
			shutil.rmtree(path)

	return len(items), total



def main(argv: list[str] | None = None) -> int:
	"""解析命令行并执行默认只读的工作树清理。"""

	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument(
		"--apply",
		action="store_true",
		help="实际删除；省略时只列出候选项",
	)
	parser.add_argument(
		"--history",
		action="store_true",
		help="同时清理已迁移的开发快照和旧基线编译产物",
	)
	parser.add_argument(
		"--root",
		type=Path,
		default=ROOT,
		help=argparse.SUPPRESS,
	)
	args = parser.parse_args(argv)

	count, total = clean(args.root, args.apply, args.history)
	mode = "removed" if args.apply else "dry-run"
	print(f"[{mode}] items={count} bytes={total}")
	return 0



if __name__ == "__main__":
	sys.exit(main())
