"""Actual whole-release import and named immutable lookup versus zoneinfo/RFC coverage.

The C probe loads every frozen file into ONE database revision, then closes the
root/manager and destroys its input names before answering. No XCRON code supplies
expected offsets/DST or source identities; no reference implementation is read.
"""
import argparse
import bisect
import datetime as dt
import hashlib
import io
import json
from pathlib import Path
import random
import subprocess
from zoneinfo import ZoneInfo
from test_tzif_reference import decode
from test_timezone_reference import EPOCH, UTC, XRT_EPOCH, millis


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--probe', required=True)
    p.add_argument('--data', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    manifest = json.loads((args.data / 'manifest.json').read_text())
    rng = random.Random(0x54444231)
    cases, expected, files = [], [], []
    low = millis(dt.datetime(1850, 1, 1, tzinfo=UTC))
    high = millis(dt.datetime(2401, 1, 1, tzinfo=UTC))
    unknown = 0
    for name, digest in manifest['files'].items():
        raw = (args.data / name).read_bytes()
        assert hashlib.sha256(raw).hexdigest() == digest, name
        zone = ZoneInfo.from_file(io.BytesIO(raw), key=name)
        times, indices, kinds, footer = decode(raw)
        stamps = {millis(dt.datetime(year, month, 15, 12, tzinfo=UTC)) for year in (1850, 1900, 1970, 2000, 2024, 2100, 2400) for month in (1, 4, 7, 10)}
        stamps.update(rng.randrange(low, high) for _ in range(48))
        for second in sorted(set(times[:3] + times[-12:])):
            stamps.update(second * 1000 + delta for delta in (-1000, -1, 0, 1, 1000) if low <= second * 1000 + delta < high)
        for stamp in sorted(stamps):
            index = bisect.bisect_right(times, stamp // 1000)
            if times and index == len(times) and not footer:
                value = (-1, 123, 0)
            elif (times and index < len(times)) or not footer:
                kind = kinds[indices[index-1] if index else 0]
                value = (-1, 123, 0) if kind[2] == '-00' else (1, kind[0], int(kind[1]))
                if value[0] == 1:
                    local = (EPOCH + dt.timedelta(milliseconds=stamp)).astimezone(zone)
                    assert int(local.utcoffset().total_seconds()) == value[1], (name, stamp)
            else:
                local = (EPOCH + dt.timedelta(milliseconds=stamp)).astimezone(zone)
                value = (-1, 123, 0) if local.tzname() == '-00' else (1, int(local.utcoffset().total_seconds()), int(bool(local.dst())))
            unknown += value[0] == -1
            cases.append(f'{name} {stamp+XRT_EPOCH}\n')
            expected.append(value)
        files.append(dict(name=name, sha256=digest, queries=len(stamps)))
    missing = 'Etc/Xcron_Missing_Manifest_Name'
    assert missing not in manifest['files']
    cases.append(f'{missing} {XRT_EPOCH}\n')
    expected.append((-2, 123, 0))
    names = args.output.with_suffix('.names.txt')
    names.write_text(''.join(name+'\n' for name in manifest['files']))
    version = manifest['version'].split()[-1]
    command = [args.probe, str(args.data), str(names), version]
    result = subprocess.run(command, input=''.join(cases), capture_output=True, text=True, timeout=180, check=True)
    lines = result.stdout.splitlines()
    ready = lines[0].split()
    assert ready[0] == 'READY' and int(ready[1]) == len(files) and int(ready[2]) <= 128*1024*1024 and int(ready[3]) == 1, ready
    assert len(lines)-1 == len(cases)
    for case, line, value in zip(cases, lines[1:], expected):
        assert tuple(map(int, line.split())) == value, (case, line, value)
    report = dict(schema=1, version=manifest['version'], zones=len(files), queries=len(cases), unknown=unknown, named_missing=1,
                  snapshot_bytes=int(ready[2]), files=files, command=command,
                  manifest_sha256=hashlib.sha256((args.data/'manifest.json').read_bytes()).hexdigest(), names_sha256=hashlib.sha256(names.read_bytes()).hexdigest(),
                  stdout_sha256=hashlib.sha256(result.stdout.encode()).hexdigest(), stderr=result.stderr, exit_code=result.returncode,
                  scope='One actual all-name LoadRoot revision; lookups after source-name/root/database destruction. Independent zoneinfo offsets plus raw RFC coverage/type flags.')
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(f"[PASS] whole named snapshot {manifest['version']}: {len(files)} zones, {len(cases)} states, {unknown} unknown, bytes={ready[2]}")


if __name__ == '__main__':
    main()
