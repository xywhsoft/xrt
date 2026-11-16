"""Record real loopback RPC capacity, with separate TCP/TLS1.3/WS samples."""
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
    parser.add_argument('--count', type=int, default=2000)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--quick', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.count <= 1000000 or not 1 <= args.repeats <= 20:
        parser.error('finite count/repeats required')
    compiler = performance.xrt_build._compiler(args.compiler)
    output = ROOT / 'out/xtp3-bench' / ('native.exe' if sys.platform == 'win32' else 'native')
    performance._compile_benchmark(compiler, 'native', {'source': 'extlibs/xtp/bench/bench_native.c', 'suite': 'xtp_native,time'},
                                   {'cflags': ['-O2'], 'ldflags': []}, output,
                                   tuple(performance.expand_manifest_paths([Path('extlibs/xtp/config/modules.json')])))
    def git(*a):
        return subprocess.check_output(['git', *a], cwd=ROOT, text=True, encoding='utf-8').strip()
    sources = [ROOT / 'extlibs/xtp/bench/bench_native.c', ROOT / 'extlibs/xtp/bench/bench_clock.h', ROOT / 'extlibs/xtp/examples/support/native_loopback.h']
    result = {'schema': 1, 'utc': datetime.now(timezone.utc).isoformat(), 'head': git('rev-parse', 'HEAD'),
              'working_status': git('status', '--short'), 'platform': platform.platform(), 'processor': platform.processor(),
              'compiler': str(compiler), 'compiler_version': subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
              'cflags': ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O2'], 'count_per_direction': args.count, 'repeats': args.repeats,
              'scope': 'one real loopback connection, bidirectional RPC, one network worker; closed-loop or deterministic offered arrivals sampled by a 1ms worker timer; bounded client admission window, all rejected arrivals counted; latency from API submission and generator lag reported separately; handshake excluded from RPC elapsed but included in native TCP counters and CPU; WS already upgraded; TLS1.3 ChaCha/test pin; no HTTP parity claim',
              'fixture_sha256': {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}, 'samples': []}
    cases = [(mode, body, window, 0) for mode in range(3) for body in (16, 64, 256, 1024, 16384, 65536) for window in (1, 8, 32)]
    cases += [(mode, 64, 32, rate) for mode in range(3) for rate in (10000, 200000)]
    if args.quick:
        cases = [(mode, 64, 32, rate) for mode in range(3) for rate in (0, 10000, 200000)]
    for mode, body, window, rate in cases:
        def run(count):
            text = subprocess.check_output([str(output), *map(str, (mode, count, body, window, rate))], cwd=ROOT, text=True, timeout=75)
            rows = [json.loads(line) for line in text.splitlines() if line.startswith('{')]
            cpu = float(next(line.split(':', 1)[1] for line in text.splitlines() if line.startswith('cpu_seconds:')))
            if len(rows) != 2 or any(row['count'] + row['rejected'] != count for row in rows):
                raise RuntimeError('missing peer completions')
            events = int(next(line.split(':', 1)[1] for line in text.splitlines() if line.startswith('engine_events:')))
            timers = int(next(line.split(':', 1)[1] for line in text.splitlines() if line.startswith('engine_timers:')))
            return {'mode': mode, 'body': body, 'window': window, 'rate': rate, 'cpu_seconds_whole_process': cpu,
                    'engine_events': events, 'engine_timers': timers,
                    'rpc_per_sec_both_directions': sum(row['rpc_per_sec'] for row in rows), 'peers': rows}
        run(min(args.count, 256))
        for repeat in range(args.repeats):
            sample = run(args.count);sample['repeat'] = repeat;result['samples'].append(sample)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print(f'mode={mode} body={body} window={window} rate={rate}', flush=True)
    print(f"recorded {len(result['samples'])} native samples: {args.report}")


if __name__ == '__main__':
    main()
