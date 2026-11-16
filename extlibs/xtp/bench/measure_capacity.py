"""Record real xrudp and shared-engine multi-connection capacity samples."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import measure_performance as performance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--probe', choices=('xrudp', 'multi', 'both'), default='both')
    parser.add_argument('--count', type=int, default=2000)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--quick', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.count <= 100000 or not 1 <= args.repeats <= 20:
        parser.error('finite count/repeats required')
    compiler = performance.xrt_build._compiler(args.compiler)
    manifests = tuple(performance.expand_manifest_paths([Path('extlibs/xtp/config/modules.json')]))
    def git(*a):
        return subprocess.check_output(['git', *a], cwd=ROOT, text=True, encoding='utf-8').strip()
    sources = [p for folder in ('include', 'src', 'bench', 'examples/support')
               for p in (ROOT / 'extlibs/xtp' / folder).rglob('*') if p.suffix in ('.c', '.h', '.py')]
    result = dict(schema=1, utc=datetime.now(timezone.utc).isoformat(), head=git('rev-parse', 'HEAD'),
                  working_status=git('status', '--short'), platform=platform.platform(), compiler=str(compiler),
                  compiler_version=subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
                  count_per_direction=args.count, repeats=args.repeats,
                  scope='closed-loop, real localhost sockets; xrudp Noise IK with test static-key pins/cookie and optional seeded loss/jitter/duplicates; one serialized xrudp executor, busy polling; NOTIFY completion is local admission and received counted separately; multi connections share one engine, one ephemeral listener per pair, both peers of each pair pinned to the same worker, independent pairs parallel; XTP memory only excludes transport/dependency state; no HTTP parity or Internet latency claim',
                  sources={p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}, samples=[])
    for name in ('xrudp', 'multi'):
        if args.probe not in (name, 'both'):
            continue
        exe = ROOT / 'out/xtp3-bench' / (name + ('.exe' if sys.platform == 'win32' else ''))
        suite = 'xtp_xrudp,net_socket,thread,time' if name == 'xrudp' else 'xtp_native,time'
        performance._compile_benchmark(compiler, 'native', {'source': f'extlibs/xtp/bench/bench_{name}.c', 'suite': suite},
                                       {'cflags': ['-O2'], 'ldflags': []}, exe, manifests)
        if name == 'xrudp':
            cases = [(body, window, kind, 0, 0) for kind in (1, 3, 5)
                     for body in ((16, 64, 256) if kind == 5 else (16, 64, 256, 1024, 16384, 65536)) for window in (1, 8, 32)]
            cases += [(64, 32, kind, loss, rtt) for kind in (1, 3, 5) for loss, rtt in ((1, 20), (10, 80))]
            if args.quick:
                cases = [(64, 32, kind, 0, 0) for kind in (1, 3, 5)] + [(64, 32, 1, 1, 20)]
        else:
            cases = [(connections, workers, mode, 64) for mode in range(3) for connections in (1, 8, 32) for workers in (1, 2, 4)]
            if args.quick:
                cases = [(8, workers, mode, 64) for mode in range(3) for workers in (1, 4)]
        for case in cases:
            def run(count):
                params = (count, *case) if name == 'xrudp' else (*case[:2], count, *case[2:])
                text = subprocess.check_output([str(exe), *map(str, params)], cwd=ROOT, text=True, timeout=135)
                rows = [json.loads(line) for line in text.splitlines() if line.startswith('{')]
                if name == 'xrudp':
                    if len(rows) != 2 or any(row['completed'] != count for row in rows):
                        raise RuntimeError('missing xrudp completions')
                elif not rows[-1].get('summary') or rows[-1]['completed'] != case[0] * 2 * count:
                    raise RuntimeError('missing parallel completions')
                return rows
            run(min(args.count, 128))
            for repeat in range(args.repeats):
                result['samples'].append(dict(probe=name, case=case, repeat=repeat, rows=run(args.count)))
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
            print(f'{name}: {case}', flush=True)
    print(f"recorded {len(result['samples'])} capacity samples: {args.report}")


if __name__ == '__main__':
    main()
