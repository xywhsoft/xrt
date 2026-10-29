"""Build private pending-table invariants with the real module dependency closure."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from test_protocol_fuzz import _closure

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('compiler not found')
    out = args.output or ROOT / 'out/service-extensions/xrpc-pending' / ('windows-structure' if os.name == 'nt' else 'linux-structure')
    out = out.resolve(); out.mkdir(parents=True, exist_ok=True)
    sources, defines, links, _ = _closure('xrpc_pending', 'extlibs/xrpc/config/modules.json')
    # The probe includes pending.c to inspect internals, avoiding a second copy.
    sources = [s for s in sources if s != 'extlibs/xrpc/src/pending/pending.c']
    sources += ['extlibs/xrpc/tests/whitebox/test_pending_structure.c']
    inputs = {ROOT / p for p in sources}
    inputs.update((ROOT / 'include').rglob('*.h'))
    inputs.update((ROOT / 'src').rglob('*.h'))
    inputs.update((ROOT / 'extlibs/xrpc').rglob('*.h'))
    inputs.update([Path(__file__).resolve(), ROOT / 'extlibs/xrpc/src/pending/pending.c', ROOT / 'extlibs/xrpc/src/internal/xrpc_string.h',
                   ROOT / 'extlibs/xrpc/config/modules.json', ROOT / 'tools/test_protocol_fuzz.py', ROOT / 'tests/test.h'])
    hashes = {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(inputs)}
    binary = out / ('pending_structure.exe' if os.name == 'nt' else 'pending_structure')
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else []
    command = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', *flags,
               *['-D' + d for d in defines], '-Iinclude', '-Iextlibs/xrpc/include',
               *sources, *['-l' + v for v in links], '-o', str(binary)]
    with (out / 'build.log').open('w', encoding='utf-8') as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
    env = os.environ.copy()
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    result = subprocess.run([str(binary)], cwd=ROOT, env=env, text=True, capture_output=True, check=True, timeout=120)
    (out / 'output.txt').write_text(result.stdout, encoding='utf-8')
    (out / 'stderr.txt').write_text(result.stderr, encoding='utf-8')
    assert hashes == {p: digest(ROOT / p) for p in hashes}, 'inputs changed during verification'
    receipt = dict(platform=platform.platform(), compiler=compiler, compiler_sha256=digest(Path(compiler)),
                   compiler_version=subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
                   command=command, sanitize=args.sanitize, inputs=hashes, binary_sha256=digest(binary),
                   output_sha256=digest(out / 'output.txt'), stderr_sha256=digest(out / 'stderr.txt'))
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(result.stdout, end='')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
