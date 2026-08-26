#!/usr/bin/env python3

"""按模块清单生成 XRT 静态库或动态库。"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

import build as xrt_build
from xrt_manifest import expand_manifest_paths, load_manifest


ROOT = Path(__file__).resolve().parents[1]
ARCHIVE_COMMAND_LIMIT = 20000



def _compiler_family(compiler: str) -> str:
	"""区分 GNU 风格、TCC 和 MSVC 风格驱动。"""

	name = Path(compiler).stem.lower()
	if name in {"cl", "clang-cl"}:
		return "msvc"
	if name == "tcc":
		return "tcc"
	return "gnu"



def _platform() -> str:
	"""返回产物命名使用的宿主平台。"""

	if sys.platform == "win32":
		return "windows"
	if sys.platform == "darwin":
		return "macos"
	return "linux"



def _artifact_names(
	kind: str,
	platform: str,
	family: str,
	product: str = "xrt",
) -> tuple[str, str | None]:
	"""返回主产物和可选导入库名称。"""

	if kind == "static":
		return (
			(f"{product}.lib", None) if family == "msvc" else
			(f"lib{product}.a", None)
		)
	if platform == "windows":
		if family == "msvc":
			return f"{product}.dll", f"{product}.lib"
		if family == "gnu":
			return f"{product}.dll", f"lib{product}.dll.a"
		return f"{product}.dll", None
	if platform == "macos":
		return f"lib{product}.dylib", None
	return f"lib{product}.so", None



def _find_tool(explicit: str | None, candidates: list[Path | str]) -> str:
	"""按显式路径、编译器同目录和 PATH 查找配套工具。"""

	if explicit is not None:
		path = shutil.which(explicit)
		if path is None:
			raise SystemExit(f"tool not found: {explicit}")
		return path
	for candidate in candidates:
		text = str(candidate)
		if Path(text).is_file():
			return text
		path = shutil.which(text)
		if path is not None:
			return path
	name = ", ".join(Path(str(item)).name for item in candidates)
	raise SystemExit(f"archive tool not found: {name}")



def _gnu_archiver(compiler: str, explicit: str | None) -> str:
	"""查找 GNU 或 LLVM 静态库归档器。"""

	directory = Path(compiler).resolve().parent
	suffix = ".exe" if sys.platform == "win32" else ""
	return _find_tool(explicit, [
		directory / f"gcc-ar{suffix}",
		directory / f"llvm-ar{suffix}",
		directory / f"ar{suffix}",
		"gcc-ar",
		"llvm-ar",
		"ar",
	])



def _msvc_archiver(compiler: str, explicit: str | None) -> str:
	"""查找 MSVC 或 LLVM 静态库归档器。"""

	directory = Path(compiler).resolve().parent
	return _find_tool(explicit, [
		directory / "llvm-lib.exe",
		directory / "lib.exe",
		"llvm-lib",
		"lib",
	])



def _msvc_options(
	defines: list[str],
	extra_cflags: list[str],
	preprocessor_conformance: bool = True,
) -> list[str]:
	"""生成 cl 与 clang-cl 共用的严格 C 编译参数。"""

	options = [
		"/nologo",
		"/TC",
		"/std:c11",
		"/utf-8",
		"/W4",
		"/WX",
		"/O2",
	]
	# clang-cl 把 /Zc:preprocessor 视为未使用参数并按错误报告。
	if preprocessor_conformance:
		options.append("/Zc:preprocessor")
	options.extend(f"/D{item}" for item in defines)
	options.append(f"/I{ROOT / 'include'}")
	options.extend(extra_cflags)
	return options



def _msvc_environment(
	compiler: str,
	arch: str,
	sources: list[str],
	options: list[str],
	header_roots: list[str] | None = None,
) -> str:
	"""计算 MSVC 对象闭包的共享环境指纹。"""

	digest = hashlib.sha256()
	path = Path(compiler).resolve()
	stat = path.stat()
	for value in [
		sys.platform,
		str(path),
		str(stat.st_size),
		str(stat.st_mtime_ns),
		arch,
		*options,
		*sources,
	]:
		digest.update(value.encode("utf-8"))
		digest.update(b"\0")
	roots = [ROOT / "include", ROOT / "src"]
	roots.extend(ROOT / path for path in header_roots or [])
	for root in roots:
		for header in sorted(root.rglob("*.h")):
			digest.update(str(header.relative_to(ROOT)).encode("utf-8"))
			digest.update(b"\0")
			digest.update(header.read_bytes())
			digest.update(b"\0")
	return digest.hexdigest()



def _compile_msvc_objects(
	compiler: str,
	arch: str,
	sources: list[str],
	defines: list[str],
	object_dir: Path,
	rebuild: bool,
	extra_cflags: list[str],
	header_roots: list[str] | None = None,
) -> list[Path]:
	"""增量编译 MSVC 风格对象文件。"""

	options = _msvc_options(
		defines,
		extra_cflags,
		preprocessor_conformance="clang" not in compiler,
	)
	environment = _msvc_environment(
		compiler,
		arch,
		sources,
		options,
		header_roots,
	)
	objects = [
		object_dir / ("__".join(Path(source).with_suffix("").parts) + ".obj")
		for source in sources
	]
	object_dir.mkdir(parents=True, exist_ok=True)
	compiled = 0
	for source, output in zip(sources, objects):
		source_path = ROOT / source
		stamp = output.with_suffix(output.suffix + ".fingerprint")
		fingerprint = xrt_build._object_source_fingerprint(
			environment,
			source_path,
		)
		if (
			not rebuild and
			output.is_file() and
			stamp.is_file() and
			(stamp.read_text(encoding="ascii").strip() == fingerprint)
		):
			continue
		command = [
			compiler,
			*options,
			"/c",
			str(source_path),
			f"/Fo{output}",
		]
		xrt_build._run_compiler(
			command,
			output.with_suffix(output.suffix + ".rsp"),
		)
		stamp.write_text(fingerprint + "\n", encoding="ascii")
		compiled += 1
	if compiled == 0:
		print(f"[reuse] objects={len(objects)} dir={object_dir.relative_to(ROOT)}")
	elif compiled < len(objects):
		print(
			f"[reuse] objects={len(objects) - compiled} "
			f"rebuilt={compiled} dir={object_dir.relative_to(ROOT)}"
		)
	return objects



def _archive_gnu(
	archiver: str,
	objects: list[Path],
	output: Path,
) -> None:
	"""分批生成 GNU/LLVM 静态库，兼容不支持响应文件的旧版 ar。"""

	output.unlink(missing_ok=True)
	chunks: list[list[str]] = []
	chunk: list[str] = []
	for item in objects:
		candidate = [archiver, "rcs", str(output), *chunk, str(item)]
		if chunk and (
			len(subprocess.list2cmdline(candidate)) >= ARCHIVE_COMMAND_LIMIT
		):
			chunks.append(chunk)
			chunk = []
		chunk.append(str(item))
	if chunk:
		chunks.append(chunk)

	for index, items in enumerate(chunks):
		mode = "rcs" if index == 0 else "r"
		xrt_build._run_compiler(
			[archiver, mode, str(output), *items],
			output.with_suffix(output.suffix + f".{index}.rsp"),
		)
	if len(chunks) > 1:
		xrt_build._run_compiler(
			[archiver, "s", str(output)],
			output.with_suffix(output.suffix + ".index.rsp"),
		)



def _archive_msvc(
	archiver: str,
	objects: list[Path],
	output: Path,
) -> None:
	"""生成 MSVC/LLVM COFF 静态库。"""

	output.unlink(missing_ok=True)
	command = [
		archiver,
		"/nologo",
		f"/OUT:{output}",
		*(str(item) for item in objects),
	]
	xrt_build._run_compiler(command, output.with_suffix(output.suffix + ".rsp"))



def _link_gnu_shared(
	compiler: str,
	arch: str,
	family: str,
	objects: list[Path],
	links: list[str],
	output: Path,
	import_library: Path | None,
	extra_ldflags: list[str],
) -> None:
	"""链接 GNU/TCC 风格动态库。"""

	command = [compiler]
	if arch == "x86":
		command.append("-m32")
	elif arch == "x64":
		command.append("-m64")
	command.append("-dynamiclib" if sys.platform == "darwin" else "-shared")
	command.extend(str(item) for item in objects)
	if sys.platform != "win32":
		command.extend(["-pthread", "-lm"])
	command.extend(f"-l{item}" for item in links)
	if import_library is not None:
		command.extend([
			"-Xlinker",
			"--out-implib",
			"-Xlinker",
			str(import_library),
		])
	if (sys.platform != "win32") and (family != "tcc"):
		command.append("-Wl,--no-undefined")
	command.extend(extra_ldflags)
	command.extend(["-o", str(output)])
	xrt_build._run_compiler(command, output.with_suffix(output.suffix + ".rsp"))



def _link_msvc_shared(
	compiler: str,
	objects: list[Path],
	links: list[str],
	output: Path,
	import_library: Path,
	extra_ldflags: list[str],
) -> None:
	"""链接 cl/clang-cl 风格动态库。"""

	# 对象走响应文件，/LD 与 /link 选项保留在直接命令行：
	# 个别 cl 版本会忽略响应文件里的链接选项（/OUT、/IMPLIB 静默丢失）。
	response = output.with_suffix(output.suffix + ".objects.rsp")
	response.parent.mkdir(parents=True, exist_ok=True)
	response.write_text(
		"\n".join(xrt_build._response_argument(str(item)) for item in objects)
		+ "\n",
		encoding="utf-8",
	)
	command = [
		compiler,
		"/nologo",
		"/LD",
		"@" + str(response),
		"/link",
		f"/OUT:{output}",
		f"/IMPLIB:{import_library}",
		*(f"{item}.lib" for item in links),
		*extra_ldflags,
	]
	subprocess.run(command, cwd=ROOT, check=True)
	# 兜底：若个别环境仍按默认名落盘，显式收回到期望位置。
	if (not output.exists()) or (not import_library.exists()):
		fallback_base = ROOT / objects[0].stem
		if (not output.exists()) and fallback_base.with_suffix(".dll").is_file():
			shutil.move(str(fallback_base.with_suffix(".dll")), str(output))
		if (not import_library.exists()) and fallback_base.with_suffix(".lib").is_file():
			shutil.move(str(fallback_base.with_suffix(".lib")), str(import_library))
		fallback_base.with_suffix(".exp").unlink(missing_ok=True)
	if not import_library.exists():
		raise SystemExit(f"msvc import library was not produced: {import_library}")
	if not output.exists():
		raise SystemExit(f"msvc shared library was not produced: {output}")



def _validate_msvc_arch(arch: str) -> None:
	"""在可识别开发环境时拒绝架构标签与实际工具链不一致。"""

	if arch == "native":
		return
	actual = os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower()
	expected = "x86" if arch == "x86" else "x64"
	aliases = {"amd64": "x64", "x86_amd64": "x64"}
	actual = aliases.get(actual, actual)
	if actual and (actual != expected):
		raise SystemExit(
			f"MSVC environment targets {actual}, requested {expected}"
		)




def _gnu_static_archive_flags(artifact: Path) -> list[str]:
	"""返回强制链接静态库全部成员的平台参数。"""

	if sys.platform == "darwin":
		return [f"-Wl,-force_load,{artifact}"]
	return [
		"-Wl,--whole-archive",
		str(artifact),
		"-Wl,--no-whole-archive",
	]



def _verify_gnu(
	compiler: str,
	arch: str,
	family: str,
	kind: str,
	artifact: Path,
	import_library: Path | None,
	links: list[str],
	objects: list[Path],
	consumer: str = "tests/package/test_core_consumer.c",
	include_dirs: list[str] | None = None,
	product: str = "xrt",
) -> None:
	"""真实链接并运行 GNU/TCC 发布库消费者。"""

	if (
		(kind == "shared") and
		(sys.platform == "win32") and
		(import_library is None)
	):
		raise SystemExit(
			"shared-library verification requires an import library; "
			"TCC Windows DLL is package-only"
		)
	output = artifact.parent / (
		f"test_package_{product}.exe" if sys.platform == "win32" else
		f"test_package_{product}"
	)
	command = [compiler]
	if arch == "x86":
		command.append("-m32")
	elif arch == "x64":
		command.append("-m64")
	if family != "tcc":
		command.extend(["-std=c11", "-Wall", "-Wextra", "-Werror"])
	command.extend(["-I", str(ROOT / "include")])
	for directory in include_dirs or []:
		command.extend(["-I", str(ROOT / directory)])
	if kind == "shared":
		command.append("-DXRT_USE_SHARED")
	command.append(str(ROOT / consumer))
	if (kind == "static") and (family != "tcc"):
		command.extend(_gnu_static_archive_flags(artifact))
	else:
		command.append(str(import_library or artifact))
	if sys.platform != "win32":
		command.extend(["-pthread", "-lm"])
		if kind == "shared":
			rpath = "@loader_path" if sys.platform == "darwin" else "$ORIGIN"
			command.append(f"-Wl,-rpath,{rpath}")
	command.extend(f"-l{item}" for item in links)
	command.extend(["-o", str(output)])
	xrt_build._run_compiler(command, output.with_suffix(output.suffix + ".rsp"))
	print(f"[verify] {output.relative_to(ROOT)}")
	subprocess.run([str(output)], cwd=artifact.parent, check=True)

	# TCC 没有可移植的 whole-archive 参数，额外直链全部对象验证闭包。
	if (kind == "static") and (family == "tcc"):
		closure_output = artifact.parent / (
			f"test_package_{product}_closure.exe" if sys.platform == "win32" else
			f"test_package_{product}_closure"
		)
		closure_command = [*command]
		artifact_index = closure_command.index(str(artifact))
		closure_command[artifact_index:artifact_index + 1] = [
			str(item) for item in objects
		]
		closure_command[-2:] = ["-o", str(closure_output)]
		xrt_build._run_compiler(
			closure_command,
			closure_output.with_suffix(closure_output.suffix + ".rsp"),
		)
		print(f"[verify] {closure_output.relative_to(ROOT)}")
		subprocess.run([str(closure_output)], cwd=artifact.parent, check=True)



def _verify_msvc(
	compiler: str,
	kind: str,
	artifact: Path,
	import_library: Path | None,
	links: list[str],
	consumer: str = "tests/package/test_core_consumer.c",
	include_dirs: list[str] | None = None,
	product: str = "xrt",
) -> None:
	"""真实链接并运行 MSVC/clang-cl 发布库消费者。"""

	link_library = import_library or artifact
	output = artifact.parent / f"test_package_{product}.exe"
	command = [
		compiler,
		"/nologo",
		"/TC",
		"/std:c11",
		"/utf-8",
		"/W4",
		"/WX",
		f"/I{ROOT / 'include'}",
	]
	if "clang" not in compiler:
		command.append("/Zc:preprocessor")
	command.extend(f"/I{ROOT / directory}" for directory in include_dirs or [])
	if kind == "shared":
		command.append("/DXRT_USE_SHARED")
	command.append(str(ROOT / consumer))
	command.append(f"/Fe{output}")
	# 库引用统一进 /link 段；新版 cl 不再把 /Fe 之前的裸 .lib 当链接输入。
	command.append("/link")
	if kind == "shared":
		command.append(str(link_library))
	if kind == "static":
		command.append(f"/WHOLEARCHIVE:{artifact}")
	command.extend(f"{item}.lib" for item in links)
	xrt_build._run_compiler(command, output.with_suffix(".rsp"))
	print(f"[verify] {output.relative_to(ROOT)}")
	subprocess.run([str(output)], cwd=artifact.parent, check=True)



def main() -> int:
	"""解析模块闭包并生成一个发布库。"""

	parser = argparse.ArgumentParser()
	parser.add_argument("--compiler", default="gcc")
	parser.add_argument("--arch", choices=["native", "x86", "x64"], default="native")
	parser.add_argument(
		"--manifest",
		action="append",
		default=[],
		help="叠加一个仓库相对路径的扩展模块清单",
	)
	parser.add_argument("--suite", default="all", help="根模块、逗号组合或 all")
	parser.add_argument("--kind", choices=["static", "shared"], required=True)
	parser.add_argument("--archiver", help="静态库归档器路径或命令名")
	parser.add_argument("--rebuild", action="store_true")
	parser.add_argument(
		"--verify",
		action="store_true",
		help="真实链接并运行最小发布库消费者",
	)
	parser.add_argument("--cflag", action="append", default=[])
	parser.add_argument("--ldflag", action="append", default=[])
	args = parser.parse_args()
	try:
		overlays = expand_manifest_paths([Path(path) for path in args.manifest])
	except (OSError, ValueError) as error:
		parser.error(str(error))
	product_manifest = load_manifest(overlays[-1]) if overlays else {}
	product = product_manifest.get("product", "xrt")
	if not isinstance(product, str) or not product.isidentifier():
		parser.error("扩展 product 必须是 C 标识符")
	consumer = product_manifest.get(
		"package_consumer",
		"tests/package/test_core_consumer.c",
	)
	if not isinstance(consumer, str) or not (ROOT / consumer).is_file():
		parser.error(f"发布消费者不存在: {consumer}")

	compiler = xrt_build._compiler(args.compiler)
	family = _compiler_family(compiler)
	if family == "msvc":
		_validate_msvc_arch(args.arch)
	(
		sources, _, _, _, defines, links, _, _, include_dirs, header_roots,
	) = xrt_build._load_modules(args.suite, overlays)
	if args.kind == "shared":
		defines = [*defines, "XRT_BUILD_SHARED"]
	extra_cflags = list(args.cflag)
	for directory in include_dirs:
		if family == "msvc":
			extra_cflags.append(f"/I{ROOT / directory}")
		else:
			extra_cflags.extend(["-I", str(ROOT / directory)])

	platform = _platform()
	main_name, import_name = _artifact_names(
		args.kind,
		platform,
		family,
		product,
	)
	base = (
		ROOT / "release" / family / args.arch /
		xrt_build._suite_output_name(args.suite) / args.kind
	)
	base.mkdir(parents=True, exist_ok=True)
	output = base / main_name
	import_library = (base / import_name) if import_name is not None else None
	object_dir = base / "obj"

	if family == "msvc":
		objects = _compile_msvc_objects(
			compiler,
			args.arch,
			sources,
			defines,
			object_dir,
			args.rebuild,
			extra_cflags,
			header_roots,
		)
	else:
		compile_flags = ["-O2", *extra_cflags]
		if (args.kind == "shared") and (sys.platform != "win32"):
			compile_flags.append("-fPIC")
		if (args.kind == "shared") and (family == "gnu"):
			compile_flags.append("-fvisibility=hidden")
		objects = xrt_build._compile_objects(
			compiler,
			args.arch,
			sources,
			defines,
			object_dir,
			args.rebuild,
			compile_flags,
			header_roots,
		)

	if args.kind == "static":
		if family == "msvc":
			_archive_msvc(
				_msvc_archiver(compiler, args.archiver),
				objects,
				output,
			)
		else:
			_archive_gnu(
				_gnu_archiver(compiler, args.archiver),
				objects,
				output,
			)
	elif family == "msvc":
		assert import_library is not None
		_link_msvc_shared(
			compiler,
			objects,
			links,
			output,
			import_library,
			args.ldflag,
		)
	else:
		_link_gnu_shared(
			compiler,
			args.arch,
			family,
			objects,
			links,
			output,
			import_library,
			args.ldflag,
		)

	print(
		f"[pass] package={output.relative_to(ROOT)} "
		f"compiler={args.compiler} arch={args.arch} suite={args.suite}"
	)
	if import_library is not None:
		print(f"[pass] import={import_library.relative_to(ROOT)}")
	if args.verify:
		if family == "msvc":
			_verify_msvc(
				compiler,
				args.kind,
				output,
				import_library,
				links,
				consumer,
				include_dirs,
				product,
			)
		else:
			_verify_gnu(
				compiler,
				args.arch,
				family,
				args.kind,
				output,
				import_library,
				links,
				objects,
				consumer,
				include_dirs,
				product,
			)
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
