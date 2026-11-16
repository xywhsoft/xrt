"""Compile once and record raw repeated XTP codec/session benchmark samples."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import measure_performance as performance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--count', type=int, default=5000)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--quick', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.count <= 1000000 or not 1 <= args.repeats <= 20:
        parser.error('finite count/repeats required')
    compiler = performance.xrt_build._compiler(args.compiler)
    output = ROOT / 'out/xtp3-bench' / ('bench.exe' if sys.platform == 'win32' else 'bench')
    benchmark = {'source': 'extlibs/xtp/bench/bench_xtp.c', 'suite': 'xtp,time'}
    performance._compile_benchmark(compiler, 'native', benchmark, {'cflags': ['-O2'], 'ldflags': []},
                                   output, tuple(performance.expand_manifest_paths([Path('extlibs/xtp/config/modules.json')])))
    cases = [(mode, shape, body, batch, kind, 0)
             for mode in (0, 1) for shape in range(4)
             for body in (16, 64, 256, 1024, 16384, 65536) for batch in (1, 8, 32)
             for kind in ((5,) if mode == 0 else (1, 3, 5))]
    # Optional RPC/PUSH recovery has explicit retention costs. Never hide
    # window refusals among successful ordinary-session throughput numbers.
    cases += [(1, shape, 64, 8, kind, 1) for shape in range(4) for kind in (1, 3)]
    if args.quick:
        cases = [(0, 0, 64, 8, 5, 0), (1, 0, 64, 8, 1, 0), (1, 0, 64, 8, 1, 1)]
    def git(*a):
        return subprocess.check_output(['git', *a], cwd=ROOT, text=True, encoding='utf-8').strip()
    owned = [p for directory in ('include', 'src', 'bench')
             for p in (ROOT / 'extlibs/xtp' / directory).rglob('*') if p.suffix in ('.c', '.h', '.py')]
    result = {'schema': 1, 'utc': datetime.now(timezone.utc).isoformat(),
              'head': git('rev-parse', 'HEAD'), 'working_status': git('status', '--short'),
              'compiler': str(compiler), 'compiler_version': subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
              'platform': platform.platform(), 'machine': platform.machine(), 'processor': platform.processor(),
              'cflags': ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O2'],
              'suite': 'xtp,time', 'count': args.count, 'repeats': args.repeats,
              'scope': 'local synchronous echo; one host staging copy per output, no sockets or encryption; no arrival-rate/HTTP parity claim',
              'sources': {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in owned}, 'samples': []}
    for case in cases:
        mode, shape, body, batch, kind, replay = case
        def run(count):
            command = [str(output), *map(str, (count, shape, body, batch, kind, mode, replay))]
            return json.loads(subprocess.check_output(command, cwd=ROOT, text=True, timeout=60))
        run(min(args.count, 256))  # Separate process warmup; allocation still measured cold each sample.
        for repeat in range(args.repeats):
            sample = run(args.count);sample['repeat'] = repeat;result['samples'].append(sample)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print('case=' + ','.join(map(str, case)), flush=True)
    print(f"recorded {len(result['samples'])} samples: {args.report}")


if __name__ == '__main__':
    main()
