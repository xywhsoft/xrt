"""Independent IANA label oracle: zoneinfo states plus explicit RFC coverage.

Never uses XCRON Resolve to construct expected candidates or policy outcomes.
All frozen input hashes are checked. UTC brute stepping has separate C evidence.
"""
import argparse
import bisect
import datetime as dt
import hashlib
import io
import json
from pathlib import Path
import subprocess
from zoneinfo import ZoneInfo
from test_timezone_reference import EPOCH, UTC, XRT_EPOCH, millis
from test_tzif_reference import decode
from test_zoned_reference import footer_offsets


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((args.data / 'manifest.json').read_text())
    totals = dict(labels=0, calls=0, found=0, none=0, ambiguous=0, missing=0, unknown=0)
    files = []
    for name, digest in manifest['files'].items():
        path = args.data / name
        raw = path.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == digest, name
        zone = ZoneInfo.from_file(io.BytesIO(raw), key=name)
        times, indices, kinds, footer = decode(raw)
        offsets = {k[0] for k in kinds if k[2] != '-00'} | set(footer_offsets(footer))

        def state(second):
            at = bisect.bisect_right(times, second)
            if times and at == len(times):
                if not footer:
                    return None
            elif times or not footer:
                k = kinds[indices[at - 1] if at else 0]
                return None if k[2] == '-00' else k[0]
            local = (EPOCH + dt.timedelta(seconds=second)).astimezone(zone)
            return None if local.tzname() == '-00' else int(local.utcoffset().total_seconds())

        def unknown(center):
            lo, hi = center - (2**31 - 1), center + (2**31 - 1)
            if state(lo) is None:
                return True
            for second in times[bisect.bisect_left(times, lo):bisect.bisect_right(times, hi)]:
                if state(second) is None:
                    return True
            if '-00' in footer:
                assert footer == '<-00>0', (name, footer)
                return state(hi) is None
            return False

        centers = {0, -1}
        for year in [1600, 1900, 2024, 2100, 2400]:
            centers.add(millis(dt.datetime(year, 7, 1, 12, 30, tzinfo=UTC)) // 1000)
        for second in times:
            if not -5364662400 <= second <= 7289654400:  # 1800..2200
                continue
            before, after = state(second - 1), state(second)
            if before == after:
                continue
            for offset in (before, after):
                if offset is not None:
                    centers.update(second + offset + delta for delta in (-1, 0, 1))
            if before is not None and after is not None:
                centers.add(second + (before + after) // 2)
        centers = sorted(centers)
        labels = [EPOCH + dt.timedelta(seconds=c, milliseconds=137) for c in centers]
        text = ''.join(f'{d.year} {d.month} {d.day} {d.hour} {d.minute} {d.second} 137\n' for d in labels)
        process = subprocess.run([args.probe, str(path)], input=text, text=True, capture_output=True, timeout=120, check=True)
        lines = process.stdout.splitlines()
        assert len(lines) == len(centers), (name, process.stderr)
        for center, line in zip(centers, lines):
            uncertain = unknown(center)
            candidates = [] if uncertain else sorted((center - o) * 1000 + 137 + XRT_EPOCH for o in offsets if state(center - o) == o)
            segments = line.split('|')
            assert len(segments) == 8
            for index, segment in enumerate(segments):
                fold, missing = divmod(index, 2)
                values = list(map(int, segment.split()))
                n = len(candidates)
                if uncertain:
                    expected = [-1, 777, 8002, 2009, 0]
                    category = 'unknown'
                elif not n and missing:
                    expected = [-1, 777, 8005, 0, 0]
                    category = 'missing'
                elif n > 1 and fold == 3:
                    expected = [-1, 777, 8004, 0, n]
                    category = 'ambiguous'
                else:
                    selected = candidates if fold == 0 else candidates[:1] if fold != 2 else candidates[-1:]
                    expected = [int(bool(selected)), len(selected), 0, 0, n, *selected]
                    category = 'found' if selected else 'none'
                assert values == expected, (name, center, fold, missing, values, expected)
                totals[category] += 1
                totals['calls'] += 1
            totals['labels'] += 1
        files.append(dict(name=name, sha256=digest, labels=len(centers), stdout_sha256=hashlib.sha256(process.stdout.encode()).hexdigest()))
    report = dict(schema=1, version=manifest['version'], zones=len(files), totals=totals, files=files,
                  oracle='zoneinfo states, explicit RFC coverage, all distinct stored/footer offsets; all8 policies; input bytes retired before queries')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('[PASS] IANA delivery selection', manifest['version'], len(files), totals)


if __name__ == '__main__':
    main()
