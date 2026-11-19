"""Recompute HTTP or framed TCP statistics from per-offer CSV.

Use --binary to select the built matched TCP/TLS benchmark. Diagnostic sleep and
yield samples are NOT stable performance baselines. CSV I/O happens after timing.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import subprocess

root = Path(__file__).resolve().parents[4]
name, arch, suffix = ('windows', 'native', '.exe') if os.name == 'nt' else ('wsl', 'x64', '')
out = root/'out/service-extensions/xrpc-http-timing'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path)
parser.add_argument('--report', type=Path)
parser.add_argument('--label', default=name)
parser.add_argument('--burst', action='store_true')
parser.add_argument('--tcp', action='store_true', help='use the framed TCP arrival benchmark')
args = parser.parse_args()
if args.tcp:
    out = root/'out/service-extensions/xrpc-tcp-arrival'
benchmark = 'xrpc_tcp_arrival_plain_4000' if args.tcp else 'xrpc_http_server_tls_tls_ordinary'
binary = (args.binary or root/'out/performance/gnu'/arch/'xrpc'/(benchmark+suffix)).resolve()
report = args.report or out/(args.label+'-trace-oracle.json')
assert re.fullmatch(r'[a-z0-9_-]+', args.label)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert binary.is_file()
out.mkdir(parents=True, exist_ok=True)
before = sha(binary)
env = os.environ.copy()
env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
metric = re.compile(r'^([a-z][a-z0-9_]*): ([-+0-9.eE]+)$')


def near(actual, expected):
    assert math.isfinite(actual) and abs(actual-expected) <= max(.002, abs(expected)*1e-7), (actual, expected)


def rank(values, percent):
    # Mathematical nearest-rank definition, independently sorting CSV values.
    return sorted(values)[math.ceil(len(values)*percent/100)-1]


def command_for(feature, mode, wait, count, rate, trace):
    if args.tcp:
        extension = 'deadline' if feature else 'plain'
        return [str(binary), str(count), str(rate), extension, '1' if args.burst else '32', wait, str(trace)]
    return [str(binary), str(count), str(rate), str(mode), str(feature), wait, str(trace)]


cases = []
workloads = [(s, m, w, 200, 1000) for s in [0, 1] for m in [0, 1, 16] for w in ['sleep', 'yield']]
if args.burst:
    workloads = [(0, 0, 'yield', 2000, 1000000), (1, 16, 'yield', 2000, 1000000)]
if args.tcp:
    workloads = [(d, 0, w, 512, 2000) for d in [0, 1] for w in ['sleep', 'yield']]
    if args.burst:
        workloads = [(d, 0, 'yield', 4096, 1000000) for d in [0, 1]]
for feature, mode, wait, count, rate in workloads:
    tag = f'{args.label}-trace-{feature}-{mode}-{wait}'
    trace = out/(tag+'.csv')
    command = command_for(feature, mode, wait, count, rate, trace)
    r = subprocess.run(command, env=env, check=True, capture_output=True, text=True, timeout=60)
    assert not r.stderr, r.stderr
    (out/(tag+'.log')).write_text(r.stdout, encoding='utf-8')
    values = {m[1]: float(m[2]) for line in r.stdout.splitlines() if (m := metric.fullmatch(line))}
    with trace.open(newline='', encoding='utf-8') as f:
        rows = list(csv.DictReader(f))
    assert len(rows) == count
    latency, service, lag, lateness = [], [], [], []
    accepted, ready, rejected = 0, 0, 0
    for index, row in enumerate(rows):
        assert int(row['index']) == index
        planned, submitted, terminal, observed = [float(row[k]) for k in ['scheduled_us', 'submitted_us', 'terminal_us', 'observed_us']]
        assert all(math.isfinite(x) for x in [planned, submitted, terminal, observed])
        near(planned, index/rate*1e6)
        assert planned <= submitted <= terminal <= observed
        admission = int(row['admission'])
        if admission == 0:
            accepted += 1
            provenance = int(row['watch_ready'])
            assert provenance in [0, 1]
            ready += provenance
            assert int(row['status']) == (9 if mode == 1 else 10 if mode else 0)
            assert int(row['delivery']) == (1 if mode == 1 else 2)
            lag.append(observed-terminal)
        else:
            rejected += 1
            assert admission == 1 and int(row['watch_ready']) == int(row['status']) == int(row['delivery']) == -1
            near(terminal, observed)
        latency.append(terminal-planned)
        service.append(terminal-submitted)
        lateness.append(submitted-planned)
    assert accepted > 0 and accepted+rejected == count
    if args.tcp and args.burst:
        assert rejected > 0, values
        first_refused = next(i for i, row in enumerate(rows) if int(row['admission']) != 0)
        assert any(int(row['admission']) == 0 for row in rows[first_refused+1:]), values
    for field, expected in {'offered': count, 'accepted': accepted, 'completed': accepted,
                            'rejected': rejected, 'failed': 0, 'latency_samples': count,
                            'observer_samples': accepted, 'watch_ready': ready,
                            'watch_pending': accepted-ready}.items():
        near(values[field], expected)
    for field, population, percentile in [
        ('all_p50_us', latency, 50), ('all_p95_us', latency, 95),
        ('all_p99_us', latency, 99), ('all_p999_us', latency, 99.9),
        ('submit_p50_us', service, 50), ('submit_p99_us', service, 99),
        ('submit_p999_us', service, 99.9), ('observer_lag_p50_us', lag, 50),
        ('observer_lag_p99_us', lag, 99), ('observer_lag_p999_us', lag, 99.9),
        ('offer_lateness_p99_us', lateness, 99),
    ]:
        near(values[field], rank(population, percentile))
    last = max(float(row['terminal_us']) for row in rows)
    near(values['terminal_elapsed_us'], last)
    near(values['completed_per_sec'], accepted*1e6/last)
    assert values['collector_elapsed_us'] >= max(float(row['observed_us']) for row in rows)
    near(values['observed_per_sec'], accepted*1e6/values['collector_elapsed_us'])
    # Short samples may be below the OS accounting quantum. The
    # dedicated busy-work test separately proves CPU-clock progress.
    assert values['process_cpu_us'] >= 0
    near(values['cpu_us_per_offer'], values['process_cpu_us']/count)
    near(values['harness_arrays_bytes'], count*(values['harness_slot_bytes']+32))
    cases.append({'command': command, 'trace': trace.relative_to(root).as_posix(),
                  'trace_sha256': sha(trace), 'metrics': values, 'exit_code': r.returncode})

# Trace output failure is observable after normal client/server cleanup.
failure = subprocess.run(command_for(1, 0, 'yield', 8, 1000, out),
                         env=env, capture_output=True, text=True, timeout=60)
assert failure.returncode == 5 and not failure.stderr, (failure.returncode, failure.stderr)
bad = subprocess.run(command_for(1, 0, 'invalid', 8, 1000, out), capture_output=True, timeout=10)
assert bad.returncode == 1
assert sha(binary) == before
report.write_text(json.dumps({'binary': str(binary), 'binary_sha256': before,
    'script_sha256': sha(Path(__file__)), 'platform': platform.platform(), 'label': args.label, 'burst': args.burst, 'tcp': args.tcp, 'cases': cases,
    'io_failure_exit': failure.returncode, 'invalid_mode_exit': bad.returncode,
    'limitation': 'Diagnostic per-offer oracle and complete count populations, not performance acceptance or proof of every overload/fault path'}, indent=2)+'\n', encoding='utf-8')
print('[PASS] independent CSV oracle:', len(cases), 'traces; counts/provenance/percentiles/elapsed/CPU/memory; trace I/O failure and invalid wait mode; refusals='+str(sum(c['metrics']['rejected'] for c in cases)))
