#!/usr/bin/env python3
"""按模块清单构建并运行协议层 Clang/libFuzzer 门禁。"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

from xrt_manifest import expand_manifest_paths, load_manifest


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "config" / "modules.json"
CORPUS_ROOT = ROOT / "fuzz" / "corpus"
TARGETS = {
	"tls": {
		"module": "tls_protocol_fuzz_tests",
		"label": "TLS record and handshake",
		"output": "tls_protocol_fuzz",
		"adapter": "XRT_TLS_FUZZ_LIBFUZZER",
		"corpus": "tls_protocol",
		"seeds": {
			"empty": b"",
			"empty-client-hello": (
				b"\x16\x03\x03\x00\x04\x01\x00\x00\x00"
			),
			"truncated-record": b"\x16\x03\x03\x00\x20\x01",
			"oversized-record": b"\x16\x03\x03\xff\xff",
			"hello-retry-prefix": (
				b"\x02\x00\x00\x26\x03\x03\xcf\x21\xad\x74"
			),
		},
	},
	"x509": {
		"module": "x509_asn1_protocol_fuzz_tests",
		"label": "X.509 and ASN.1 DER",
		"output": "x509_asn1_fuzz",
		"adapter": "XRT_X509_FUZZ_LIBFUZZER",
		"corpus": "x509_asn1",
		"seeds": {
			"empty": b"",
			"sequence": b"\x30\x06\x02\x01\x01\x01\x01\xff",
			"non-canonical-length": b"\x04\x81\x01\x00",
			"truncated-sequence": b"\x30\x82\x01\x00\x02\x01",
			"utc-time": b"\x17\x0d250102030405Z",
		},
	},
	"net-address": {
		"module": "net_address_protocol_fuzz_tests",
		"label": "Network address and numeric DNS input",
		"output": "net_address_fuzz",
		"adapter": "XRT_NET_ADDRESS_FUZZ_LIBFUZZER",
		"corpus": "net_address",
		"seeds": {
			"empty": b"",
			"ipv4": b"192.0.2.1:443",
			"ipv6": b"[2001:db8::1]:65535",
			"mapped": b"::ffff:192.0.2.1",
			"scope": b"[fe80::1%3]:80",
			"malformed": b"2001:db8:::1",
		},
	},
	"auth": {
		"module": "http_auth_protocol_fuzz_tests",
		"manifest": "extlibs/xhttp/config/modules.json",
		"label": "HTTP authentication",
		"output": "http_auth_protocol_fuzz",
		"adapter": "XRT_HTTP_AUTH_FUZZ_LIBFUZZER",
		"seeds": {
			"empty": b"",
			"basic": b"Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==",
			"bearer": b"Bearer mF_9.B5f-4.1JqM",
			"digest-challenge": (
				b'Digest realm="api", nonce="n,1", '
				b'algorithm=SHA-256, qop="auth,auth-int"'
			),
			"digest-extension-qop": (
				b'Digest realm="api", nonce="n,1", '
				b'al\'gorithm=SHA-256, qop="auuhttah,-int"'
			),
			"digest-credentials": (
				b'Digest username="Mufasa", realm="api", uri="/", '
				b'algorithm=SHA-256, nonce="n", nc=00000001, '
				b'cnonce="c", qop=auth, response="'
				b"0123456789abcdef0123456789abcdef"
				b'0123456789abcdef0123456789abcdef"'
			),
			"digest-info": (
				b'nextnonce="next", qop=auth, rspauth="'
				b"0123456789abcdef0123456789abcdef"
				b'0123456789abcdef0123456789abcdef", '
				b'cnonce="c", nc=00000001'
			),
			"malformed-param": b"Digest realm =",
			"unterminated": b'Digest realm="unterminated',
			"empty-param": b'Digest realm="api",, qop="auth,auth-int"',
			"credential-list": b"Basic abc, Bearer def",
		},
	},
	"http1": {
		"module": "http1_protocol_fuzz_tests",
		"label": "HTTP/1",
		"output": "http1_protocol_fuzz",
		"adapter": "XRT_HTTP1_FUZZ_LIBFUZZER",
		"corpus": "http1_protocol",
		"seeds": {
			"empty": b"",
		},
	},
	"route": {
		"module": "http_route_fuzz_tests",
		"manifest": "extlibs/xhttp/config/modules.json",
		"label": "HTTP route",
		"output": "http_route_fuzz",
		"adapter": "XRT_HTTP_ROUTE_FUZZ_LIBFUZZER",
		"seeds": {
			"empty": b"",
			"root": b"\x00\x01//",
			"parameter": (
				b"\x00\x0b/users/{id}/users/42"
			),
			"tail": (
				b"\x00\x10/files/{path...}/files/a/b%2Fc"
			),
			"invalid-duplicate": (
				b"\x00\x08/{a}/{a}/one/two"
			),
		},
	},
	"router": {
		"module": "http_router_fuzz_tests",
		"manifest": "extlibs/xhttp/config/modules.json",
		"label": "HTTP router",
		"output": "http_router_fuzz",
		"adapter": "XRT_HTTP_ROUTER_FUZZ_LIBFUZZER",
		"seeds": {
			"empty": b"",
			"root-get": b"\x00/",
			"static": b"\x03/users/me/detail",
			"parameter": b"\x01/users/alice/detail",
			"head": b"\x02/files/name",
			"tail": b"\x06/files/a/b%2Fc",
			"not-found": b"\x05/not-found",
			"invalid-method": b"\x09/users/42/detail",
		},
	},
	"sse": {
		"module": "http_sse_fuzz_tests",
		"manifest": "extlibs/xhttp/config/modules.json",
		"label": "HTTP SSE",
		"output": "http_sse_fuzz",
		"adapter": "XRT_HTTP_SSE_FUZZ_LIBFUZZER",
		"seeds": {
			"empty": b"",
			"message": b"data: hello\n\n",
			"metadata": (
				b"id: 42\rretry: 1000\rdata: x\r\r"
			),
			"comment": b": ping\r\ndata:\r\n\r\n",
			"invalid-utf8": b"data: \xff\n\n",
			"incomplete": b"data: incomplete",
		},
	},
	"websocket": {
		"module": "websocket_protocol_fuzz_tests",
		"label": "WebSocket",
		"output": "websocket_protocol_fuzz",
		"adapter": None,
		"corpus": "websocket_protocol",
		"seeds": {
			"empty": b"",
		},
	},
}



def _platform_name() -> str:
	"""返回模块清单使用的平台键。"""

	if sys.platform == "win32":
		return "windows"
	if sys.platform == "darwin":
		return "macos"
	if sys.platform.startswith("linux"):
		return "linux"
	return "posix"



def _dependencies(module: dict) -> list[str]:
	"""合并公共依赖和当前平台依赖。"""

	result = list(module.get("depends", []))
	platform = module.get("platform_depends", {})
	result.extend(platform.get(
		_platform_name(),
		platform.get("posix", []),
	))
	return result



def _closure(
	module_name: str,
	manifest_path: str = "config/modules.json",
) -> tuple[list[str], list[str], list[str], list[str]]:
	"""返回一个 fuzz 根的源码、裁剪宏和平台链接闭包。"""

	requested_manifest = (ROOT / manifest_path).resolve()
	manifest_paths = [MANIFEST.resolve()]
	if requested_manifest != MANIFEST.resolve():
		manifest_paths.extend(expand_manifest_paths(
			[requested_manifest], ROOT,
		))
	manifests = [load_manifest(path) for path in manifest_paths]
	all_modules = [
		module
		for manifest in manifests
		for module in manifest["modules"]
	]
	include_dirs = list(dict.fromkeys(
		path
		for manifest in manifests[1:]
		for path in manifest.get("include_dirs", [])
	))
	by_name = {module["name"]: module for module in all_modules}
	if len(by_name) != len(all_modules):
		raise SystemExit("core and extension manifests contain duplicate modules")
	if module_name not in by_name:
		raise SystemExit(f"module not found: {module_name}")
	needed: set[str] = set()

	def add(name: str) -> None:
		"""按依赖方向递归加入一个模块。"""

		if name in needed:
			return
		if name not in by_name:
			raise SystemExit(f"unknown dependency: {name}")
		for dependency in _dependencies(by_name[name]):
			add(dependency)
		needed.add(name)

	add(module_name)
	modules = [
		module for module in all_modules
		if module["name"] in needed
	]
	sources: list[str] = []
	defines: list[str] = []
	links: list[str] = []
	for module in modules:
		sources.extend(module.get("sources", []))
		if module["name"] == module_name:
			sources.extend(module.get("fuzz_sources", []))
		feature = module.get("feature")
		if feature is not None:
			defines.append(feature)
		platform_links = module.get("link_libraries", {})
		links.extend(platform_links.get(
			_platform_name(),
			platform_links.get("posix", []),
		))
	sources = list(dict.fromkeys(sources))
	defines = list(dict.fromkeys(defines))
	links = list(dict.fromkeys(links))
	missing = [source for source in sources if not (ROOT / source).is_file()]
	if missing:
		raise SystemExit("missing fuzz sources: " + ", ".join(missing))
	return sources, defines, links, include_dirs



def _compiler(value: str) -> str:
	"""解析 Clang 路径并拒绝误用 clang-cl 驱动。"""

	path = shutil.which(value)
	if path is None and Path(value).is_file():
		path = str(Path(value).resolve())
	if path is None:
		raise SystemExit(f"Clang compiler not found: {value}")
	if Path(path).stem.lower() == "clang-cl":
		raise SystemExit("use the clang C driver, not clang-cl")
	return path



def _persistent_corpus(config: dict) -> Path | None:
	"""解析并验证目标声明的仓库内持久语料目录。"""

	name = config.get("corpus")
	if name is None:
		return None
	path = CORPUS_ROOT / name
	if not path.is_dir():
		raise SystemExit(f"persistent fuzz corpus is missing: {path}")
	if not any(item.is_file() for item in path.iterdir()):
		raise SystemExit(f"persistent fuzz corpus is empty: {path}")
	return path



def _seed_corpus(
	path: Path,
	seeds: dict[str, bytes],
	persistent: Path | None,
) -> None:
	"""复制持久语料和固定种子，并保留覆盖引导产生的已有语料。"""

	path.mkdir(parents=True, exist_ok=True)
	if persistent is not None:
		for item in sorted(persistent.iterdir()):
			if item.is_file():
				shutil.copy2(item, path / item.name)
	for name, data in seeds.items():
		(path / name).write_bytes(data)



def _build_command(
	compiler: str,
	sources: list[str],
	defines: list[str],
	links: list[str],
	include_dirs: list[str],
	adapter: str | None,
	output: Path,
) -> list[str]:
	"""生成带 libFuzzer、ASan 和 UBSan 的单次构建命令。"""

	command = [
		compiler,
		"-std=c11",
		"-O1",
		"-g",
		"-fno-omit-frame-pointer",
		"-fno-builtin-memcpy",
		"-fno-sanitize-recover=all",
		"-fsanitize=fuzzer,address,undefined",
	]
	command.extend(f"-D{define}" for define in defines)
	if adapter is not None:
		command.append(f"-D{adapter}")
	command.extend(["-I", str(ROOT / "include")])
	for path in include_dirs:
		command.extend(["-I", str(ROOT / path)])
	if sys.platform != "win32":
		command.append("-pthread")
	command.extend(str(ROOT / source) for source in sources)
	if sys.platform != "win32":
		command.append("-lm")
	command.extend(f"-l{link}" for link in links)
	command.extend(["-o", str(output)])
	return command



def _run_target(
	name: str,
	config: dict,
	compiler: str | None,
	runs: int,
	max_len: int,
	build_only: bool,
	dry_run: bool,
) -> None:
	"""构建并执行一个协议目标。"""

	sources, defines, links, include_dirs = _closure(
		config["module"],
		config.get("manifest", "config/modules.json"),
	)
	label = config["label"]
	persistent = _persistent_corpus(config)
	if dry_run:
		persistent_count = 0 if persistent is None else sum(
			item.is_file() for item in persistent.iterdir()
		)
		print(
			f"{label} fuzz closure: sources={len(sources)} "
			f"defines={len(defines)} links={len(links)} "
			f"persistent_seeds={persistent_count}"
		)
		return
	if compiler is None:
		raise RuntimeError("compiler was not resolved")

	out_dir = ROOT / "out" / "fuzz" / _platform_name() / name
	output = out_dir / (
		config["output"] + (".exe" if sys.platform == "win32" else "")
	)
	corpus = out_dir / "corpus"
	artifacts = out_dir / "artifacts"
	out_dir.mkdir(parents=True, exist_ok=True)
	artifacts.mkdir(parents=True, exist_ok=True)
	_seed_corpus(corpus, config["seeds"], persistent)
	command = _build_command(
		compiler, sources, defines, links, include_dirs,
		config["adapter"], output,
	)
	print(
		f"[build] {label} libFuzzer sources={len(sources)} "
		f"defines={len(defines)}"
	)
	subprocess.run(command, cwd=ROOT, check=True)
	if build_only:
		print(f"[PASS] {label} libFuzzer build: {output.relative_to(ROOT)}")
		return

	run = [
		str(output),
		str(corpus),
		f"-runs={runs}",
		f"-max_len={max_len}",
		"-timeout=5",
		"-print_final_stats=1",
		"-artifact_prefix=" + str(artifacts) + os.sep,
	]
	subprocess.run(run, cwd=ROOT, check=True)
	print(f"[PASS] {label} libFuzzer ({runs} runs)")



def main() -> int:
	"""构建选定协议闭包并执行限定轮数的覆盖引导 fuzz。"""

	parser = argparse.ArgumentParser()
	parser.add_argument(
		"targets",
		nargs="*",
		metavar="TARGET",
		help="protocol target name; default: all declared targets",
	)
	parser.add_argument("--clang", default="clang", help="Clang C driver")
	parser.add_argument("--runs", type=int, default=20000)
	parser.add_argument("--max-len", type=int, default=4096)
	parser.add_argument("--build-only", action="store_true")
	parser.add_argument("--dry-run", action="store_true")
	arguments = parser.parse_args()
	if arguments.runs <= 0:
		raise SystemExit("runs must be positive")
	if arguments.max_len <= 0:
		raise SystemExit("max-len must be positive")

	targets = arguments.targets or list(TARGETS)
	unknown = [target for target in targets if target not in TARGETS]
	if unknown:
		raise SystemExit("unknown fuzz target: " + ", ".join(unknown))
	compiler = None if arguments.dry_run else _compiler(arguments.clang)
	for target in dict.fromkeys(targets):
		_run_target(
			target,
			TARGETS[target],
			compiler,
			arguments.runs,
			arguments.max_len,
			arguments.build_only,
			arguments.dry_run,
		)
	return 0



if __name__ == "__main__":
	raise SystemExit(main())
