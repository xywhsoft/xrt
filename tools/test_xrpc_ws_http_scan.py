"""Verify bounded incremental WS HTTP probing against Core and real paced IO."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess

import build

ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--sanitize', choices=['address', 'thread'])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('compiler not found')
    out = args.output or ROOT / 'out/local-service-extensions/xrpc-ws-http-scan' / ('windows' if os.name == 'nt' else 'linux')
    if not out.is_absolute():
        out = ROOT / out
    out.mkdir(parents=True, exist_ok=True)
    sources, _, _, _, defines, links, _, _, includes, headers = build._load_modules(
        'xrpc_ws_http_tests', [ROOT / 'extlibs/xrpc/config/modules.json'])
    # The whitebox probe includes the actual production TU exactly once.
    sources.remove('extlibs/xrpc/src/ws/ws.c')
    test = 'extlibs/xrpc/tests/whitebox/test_ws_http_scan.c'
    inputs = {ROOT / p for p in sources}
    for folder in ('include', 'src/internal', 'extlibs/xrpc/include', 'extlibs/xrpc/src/internal'):
        inputs.update((ROOT / folder).rglob('*.h'))
    inputs.update(ROOT / p for p in (
        test, 'extlibs/xrpc/src/ws/ws.c', 'tools/test_xrpc_ws_http_scan.py',
        'tools/build.py', 'tools/amalgamate.py', 'tools/xrt_manifest.py',
        'config/modules.json', 'extlibs/xrpc/config/modules.json',
        'tests/test.h', 'extlibs/xrpc/tests/test.h'))
    hashes = {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(inputs)}
    flags = ['-O2']
    if args.sanitize == 'address':
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    elif args.sanitize == 'thread':
        # GCC documents that atomic_thread_fence is not instrumented by TSan.
        flags += ['-g', '-fsanitize=thread', '-fno-omit-frame-pointer', '-Wno-error=tsan']
    for folder in includes:
        flags += ['-I', str(ROOT / folder)]
    arch = 'native' if os.name == 'nt' else 'x64'
    objects = build._compile_objects(compiler, arch, sources, defines, out / 'obj', False, flags, headers)
    binary = out / ('ws_http_scan.exe' if os.name == 'nt' else 'ws_http_scan')
    build._compile_program(compiler, arch, test, objects, defines, links, binary, flags)
    env = dict(os.environ)
    if args.sanitize == 'address':
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    elif args.sanitize == 'thread':
        env.update(TSAN_OPTIONS='halt_on_error=1')
    results = []
    for backend in ('select', 'iocp' if os.name == 'nt' else 'epoll'):
        command = [str(binary), backend]
        result = subprocess.run(command, cwd=ROOT, env=env, text=True, capture_output=True, timeout=300)
        stdout = out / (backend + '.stdout.txt')
        stderr = out / (backend + '.stderr.txt')
        stdout.write_text(result.stdout, encoding='utf-8')
        stderr.write_text(result.stderr, encoding='utf-8')
        print(result.stdout, end='')
        print(result.stderr, end='')
        result.check_returncode()
        results.append(dict(backend=backend, command=command, exit_code=result.returncode,
                            stdout_sha256=digest(stdout), stderr_sha256=digest(stderr)))
    assert hashes == {p: digest(ROOT / p) for p in hashes}, 'verification inputs changed'
    receipt = dict(platform=platform.platform(), compiler=compiler,
                   compiler_sha256=digest(Path(compiler)),
                   compiler_version=subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
                   sanitize=args.sanitize, cflags=build._compile_options(compiler, arch, defines, flags),
                   inputs=hashes, binary_sha256=digest(binary), runs=results,
                   scope='authoritative parser input complexity; paced IO wall time is not throughput/tail acceptance')
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
