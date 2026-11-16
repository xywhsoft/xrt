"""Run a bounded persistent-connection soak and retain memory/time evidence."""
import argparse
import ctypes
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]


def resident(pid):
    if sys.platform == 'win32':
        class Counters(ctypes.Structure):
            _fields_ = [('cb', ctypes.c_uint32), ('faults', ctypes.c_uint32)] + [(s, ctypes.c_size_t) for s in
                ('peak_working', 'working', 'peak_paged', 'paged', 'peak_nonpaged', 'nonpaged', 'pagefile', 'peak_pagefile', 'private')]
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        psapi = ctypes.WinDLL('psapi', use_last_error=True)
        psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]
        handle = kernel.OpenProcess(0x0410, False, pid)
        if not handle:
            return None
        try:
            counters = Counters();counters.cb = ctypes.sizeof(counters)
            if not psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
                return None
            return dict(resident_bytes=counters.working, private_bytes=counters.private)
        finally:
            kernel.CloseHandle(handle)
    try:
        fields = dict(line.split(':', 1) for line in Path(f'/proc/{pid}/status').read_text().splitlines() if ':' in line)
        return dict(resident_bytes=int(fields['VmRSS'].split()[0]) * 1024)
    except (FileNotFoundError, KeyError):
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True, help='bench_multi executable; sanitizer builds supported')
    parser.add_argument('--seconds', type=int, default=300)
    parser.add_argument('--connections', type=int, default=8)
    parser.add_argument('--workers', type=int, default=4)
    parser.add_argument('--mode', type=int, choices=(0, 1, 2), default=1)
    parser.add_argument('--count', type=int, default=2000, help='RPC count per direction per round; connections stay open')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 86400 or not 1 <= args.connections <= 64 or not 1 <= args.workers <= 16 or not 1 <= args.count <= 100000:
        parser.error('finite duration/connections/workers/count required')
    exe = args.exe.resolve()
    command = [str(exe), *map(str, (args.connections, args.workers, args.count, args.mode, 64, args.seconds))]
    def git(*a):
        return subprocess.check_output(['git', *a], cwd=ROOT, text=True, encoding='utf-8').strip()
    result = dict(schema=1, utc=datetime.now(timezone.utc).isoformat(), head=git('rev-parse', 'HEAD'),
                  working_status=git('status', '--short'), command=command, exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                  scope='same connections and XTP sessions retained between rounds; latencies in rows describe final round; cumulative completed/bytes/writes and XTP peak memory in summary; OS process memory includes transport, runtime, measurement arrays and sanitizer if enabled; finite soak is not a multi-day/mobile/NAT qualification',
                  memory_samples=[])
    start = time.monotonic()
    with tempfile.TemporaryFile(mode='w+t', encoding='utf-8') as output:
        process = subprocess.Popen(command, cwd=ROOT, stdout=output, stderr=output)
        try:
            while process.poll() is None:
                elapsed = time.monotonic() - start
                if elapsed > args.seconds + 180:
                    process.kill();process.wait();raise RuntimeError('soak exceeded bounded shutdown grace')
                sample = resident(process.pid)
                if sample:
                    result['memory_samples'].append(dict(seconds=elapsed, **sample))
                time.sleep(2)
            output.seek(0);text = output.read()
        finally:
            if process.poll() is None:
                process.kill();process.wait()
    result['elapsed_seconds'] = time.monotonic() - start
    result['exit_code'] = process.returncode
    result['output'] = text
    rows = [json.loads(line) for line in text.splitlines() if line.startswith('{')]
    result['summary'] = next((row for row in rows if row.get('summary')), None)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    if process.returncode or not result['summary']:
        raise RuntimeError(f'soak failed; evidence retained in {args.report}')
    print(json.dumps(result['summary']))


if __name__ == '__main__':
    main()
