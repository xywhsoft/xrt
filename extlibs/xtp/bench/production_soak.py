"""Persistent, crash-evidenced acceptance run; a running job is never a pass."""
import argparse
from collections import deque
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from soak import resident

ROOT = Path(__file__).resolve().parents[3]


def file_hash(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576),b''):
            digest.update(block)
    return digest.hexdigest()


def atomic_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    os.replace(temporary, path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=259200)
    parser.add_argument('--connections', type=int, default=8)
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--count', type=int, default=2000)
    parser.add_argument('--mode', type=int, choices=range(4), default=1, help='0 TCP, 1 TLS1.3, 2 WS, 3 WSS')
    parser.add_argument('--max-rss-mib', type=int, default=3072)
    parser.add_argument('--warmup-seconds', type=int, default=300)
    parser.add_argument('--live-growth-kib-per-peer', type=int, default=128)
    parser.add_argument('--stall-seconds', type=int, default=150)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 604800 or not 1 <= args.connections <= 64 or not 1 <= args.workers <= 16 or not 1 <= args.count <= 100000:
        parser.error('bounded duration/connections/workers/count required')
    if min(args.max_rss_mib, args.live_growth_kib_per_peer) <= 0 or args.warmup_seconds < 0 or args.stall_seconds < 120:
        parser.error('finite positive memory budgets and >=120s stall timeout required')
    exe = args.exe.resolve();report = args.report.resolve()
    report.parent.mkdir(parents=True, exist_ok=True)
    if report.exists():
        parser.error('report already exists; choose a new run path to preserve evidence')
    raw = report.with_suffix('.child.log');events = report.with_suffix('.events.jsonl')
    if raw.exists() or events.exists():
        parser.error('run sidecar already exists')
    def git(*a):
        return subprocess.check_output(['git', *a], cwd=ROOT, text=True, encoding='utf-8').strip()
    command = [str(exe), *map(str, (args.connections, args.workers, args.count, args.mode, 64, args.seconds))]
    result = dict(schema=1, status='running', utc=datetime.now(timezone.utc).isoformat(),
                  supervisor_pid=os.getpid(), head=git('rev-parse', 'HEAD'), working_status=git('status', '--short'),
                  command=command, exe_sha256=file_hash(exe),
                  required_seconds=args.seconds, max_rss_bytes=args.max_rss_mib*1048576,
                  warmup_seconds=args.warmup_seconds, live_growth_allowance_bytes=args.live_growth_kib_per_peer*1024*args.connections*2,
                  child_log=str(raw), event_log=str(events), checkpoints=0, memory_samples=0,
                  scope='same connections/sessions retained, every round owner-thread quiescence and allocator-vs-session ledger checked; allocator tracking overhead included; OS RSS includes transport/runtime; completion requires real elapsed duration, child success, monotonic progress, memory budgets, final allocator zero assertions; no physical mobile/carrier qualification')
    result['source_sha256']={p.relative_to(ROOT).as_posix():file_hash(p) for folder in ('include','src','bench','examples/support')
                             for p in (ROOT/'extlibs/xtp'/folder).rglob('*') if p.suffix in ('.c','.h','.py')}
    start=time.monotonic();last_progress=start;last_completed=0;peak_rss=0;warm_peak=0;failure=None
    memory_tail=deque(maxlen=30);last_row=None;summary=None
    with raw.open('x', encoding='utf-8') as output, events.open('x', encoding='utf-8', buffering=1) as journal:
        process=subprocess.Popen(command,cwd=ROOT,stdout=output,stderr=output)
        result['child_pid']=process.pid;atomic_json(report,result)
        with raw.open(encoding='utf-8',errors='replace') as reader:
            pending=''
            try:
                while True:
                    now=time.monotonic();elapsed=now-start
                    pending+=reader.read()
                    parts=pending.split('\n');pending=parts.pop()
                    for line in parts:
                        if any(marker in line for marker in ('ERROR: AddressSanitizer','LeakSanitizer','WARNING: ThreadSanitizer','runtime error:','native fault ')):
                            raise RuntimeError('child diagnostic: '+line[:240])
                        if not line.startswith('{'):
                            continue
                        row=json.loads(line)
                        if row.get('summary'):
                            summary=row
                        if row.get('checkpoint'):
                            if row['completed']<=last_completed or not row.get('quiescent') or not row.get('ledger_match'):
                                raise RuntimeError('invalid/non-monotonic checkpoint')
                            last_completed=row['completed'];last_progress=now;last_row=row;result['checkpoints']+=1
                            if row['seconds']<=args.warmup_seconds or not warm_peak:
                                warm_peak=max(warm_peak,row['live_xtp_bytes'])
                            elif row['live_xtp_bytes']>warm_peak+result['live_growth_allowance_bytes']:
                                raise RuntimeError('quiescent allocator growth exceeded declared budget')
                            journal.write(json.dumps(dict(event='checkpoint',host_seconds=elapsed,**row))+'\n')
                    sample=resident(process.pid)
                    if sample:
                        peak_rss=max(peak_rss,sample['resident_bytes']);memory_tail.append(dict(seconds=elapsed,**sample))
                        result['memory_samples']+=1;journal.write(json.dumps(dict(event='memory',seconds=elapsed,**sample))+'\n')
                        if sample['resident_bytes']>result['max_rss_bytes']:
                            raise RuntimeError('RSS exceeded declared hard budget')
                    result.update(elapsed_seconds=elapsed,last_checkpoint=last_row,peak_rss_bytes=peak_rss,
                                  warm_peak_live_xtp_bytes=warm_peak,memory_tail=list(memory_tail))
                    atomic_json(report,result)
                    if process.poll() is not None:
                        break
                    if now-last_progress>args.stall_seconds:
                        raise RuntimeError('no completed round within stall budget')
                    if elapsed>args.seconds+180:
                        raise RuntimeError('duration plus shutdown grace exceeded')
                    time.sleep(2)
                # Popen.poll may race the last buffered line; collect it once more.
                for line in (pending+reader.read()).splitlines():
                    if any(marker in line for marker in ('ERROR: AddressSanitizer','LeakSanitizer','WARNING: ThreadSanitizer','runtime error:','native fault ')):
                        raise RuntimeError('child diagnostic: '+line[:240])
                    if line.startswith('{'):
                        row=json.loads(line)
                        if row.get('summary'):summary=row
                        if row.get('checkpoint'):
                            if row['completed']<=last_completed or not row.get('quiescent') or not row.get('ledger_match'):
                                raise RuntimeError('invalid/non-monotonic checkpoint')
                            last_completed=row['completed'];last_row=row;result['checkpoints']+=1
                            journal.write(json.dumps(dict(event='checkpoint',host_seconds=time.monotonic()-start,**row))+'\n')
                if process.returncode or not summary or not last_row:
                    raise RuntimeError(f'child incomplete/failed: {process.returncode}')
                if summary['seconds']<args.seconds or time.monotonic()-start<args.seconds:
                    raise RuntimeError('child did not run the required real duration')
                expected=summary['rounds']*args.connections*2*args.count
                if summary['completed']!=expected or summary['completed']<last_completed:
                    raise RuntimeError('summary completion accounting mismatch')
                if not result['memory_samples']:
                    raise RuntimeError('no process memory evidence collected')
            except BaseException as error:
                failure=str(error)
                if process.poll() is None:
                    process.kill();process.wait()
            finally:
                result.update(status='failed' if failure else 'passed',failure=failure,exit_code=process.returncode,
                              elapsed_seconds=time.monotonic()-start,summary=summary,last_checkpoint=last_row)
    result['child_log_sha256']=file_hash(raw)
    result['event_log_sha256']=file_hash(events)
    atomic_json(report,result)
    print(json.dumps(dict(status=result['status'],report=str(report),summary=summary,failure=failure)))
    return 1 if failure else 0


if __name__ == '__main__':
    sys.exit(main())
