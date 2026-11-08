"""Frozen IANA binary-data differential tests; never reads implementation source.

Prepare a directory with manifest.json containing version and files{name:sha256};
each named file is a compiled TZif snapshot. zoneinfo independently decodes the
same bytes. RFC validity (-00 and an absent footer) is checked explicitly because
zoneinfo otherwise extrapolates the final type beyond advertised coverage.
"""
import argparse
import bisect
import datetime as dt
import hashlib
import io
import json
from pathlib import Path
import random
import struct
import subprocess
from zoneinfo import ZoneInfo
from test_timezone_reference import EPOCH, UTC, XRT_EPOCH, millis


def decode(raw):
    version = raw[4]
    at, width = 0, 4
    if version:
        ut, std, leaps, count, types, chars = struct.unpack_from(">6I", raw, 20)
        at = 44 + count * 5 + types * 6 + chars + leaps * 8 + std + ut
        width = 8
    ut, std, leaps, count, types, chars = struct.unpack_from(">6I", raw, at + 20)
    assert not leaps
    start = at + 44
    times = list(struct.unpack_from(">" + ("q" if width == 8 else "i") * count, raw, start))
    start += width * count
    indices = raw[start:start + count]
    start += count
    records = [struct.unpack_from(">iBB", raw, start + i * 6) for i in range(types)]
    names = raw[start + types * 6:start + types * 6 + chars]
    kinds = [(offset, bool(dst), names[index:names.index(b"\0", index)].decode("ascii")) for offset, dst, index in records]
    end = start + types * 6 + chars + std + ut
    footer = raw[end + 1:-1].decode("ascii") if version else ""
    return times, indices, kinds, footer


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((args.data / "manifest.json").read_text())
    rng = random.Random(0x545A4946)
    total = unknowns = partials = 0
    reports = []
    for name, digest in manifest["files"].items():
        path = args.data / name
        raw = path.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == digest, name
        zone = ZoneInfo.from_file(io.BytesIO(raw), key=name)
        times, indices, kinds, footer = decode(raw)

        def state(second):
            index = bisect.bisect_right(times, second)
            if times and index == len(times):
                if not footer:
                    return None
            elif times or not footer:
                kind = kinds[indices[index - 1] if index else 0]
                return None if kind[2] == "-00" else kind
            local = (EPOCH + dt.timedelta(seconds=second)).astimezone(zone)
            return None if local.tzname() == "-00" else (int(local.utcoffset().total_seconds()), bool(local.dst()), local.tzname())

        effective = [second * 1000 for second in times if state(second - 1) != state(second)]
        future = {}

        def next_change(stamp):
            index = bisect.bisect_right(effective, stamp)
            if index < len(effective):
                return effective[index]
            if not footer:
                return None
            year = (EPOCH + dt.timedelta(milliseconds=stamp)).year
            if year not in future:
                begin = millis(dt.datetime(year, 1, 1, tzinfo=UTC)) // 1000
                end = millis(dt.datetime(year + 2, 1, 1, tzinfo=UTC)) // 1000
                changes = []
                previous = state(begin)
                for right in range(begin + 86400, end + 1, 86400):
                    current = state(right)
                    if previous != current:
                        lo, hi = right - 86400, right
                        while hi - lo > 1:
                            middle = (lo + hi) // 2
                            if state(middle) == previous:
                                lo = middle
                            else:
                                hi = middle
                        changes.append(hi * 1000)
                    previous = current
                future[year] = changes
            index = bisect.bisect_right(future[year], stamp)
            return future[year][index] if index < len(future[year]) else None

        def uncertain(label):
            # No offset can be assumed for an unspecified interval. RFC's
            # complete legal int32 range determines its possible local labels.
            center = millis(label.replace(tzinfo=UTC)) // 1000
            lo, hi = center - (2**31 - 1), center + (2**31 - 1)
            if state(lo) is None:
                return True
            for second in times[bisect.bisect_left(times, lo):bisect.bisect_right(times, hi)]:
                if state(second) is None:
                    return True
            if "-00" in footer:
                # The frozen corpus's only unspecified footer is constant;
                # fail loudly if a new dataset requires a periodic oracle.
                assert footer == "<-00>0", (name, footer)
                return state(hi) is None
            return False

        stamps = {millis(dt.datetime(1600, 1, 1, tzinfo=UTC)), 0, -1}
        lower = millis(dt.datetime(1800, 1, 1, tzinfo=UTC))
        upper = millis(dt.datetime(2500, 1, 1, tzinfo=UTC))
        for second in times:
            if lower <= second * 1000 < upper:
                stamps.update(second * 1000 + delta for delta in (-1, 0, 1, 999))
        for year in (1900, 1960, 2000, 2024, 2040, 2100, 2400):
            begin = millis(dt.datetime(year, 1, 1, tzinfo=UTC))
            end = millis(dt.datetime(year + 1, 1, 1, tzinfo=UTC))
            stamps.update(rng.randrange(begin, end) for _ in range(8))
            nxt = next_change(begin)
            if nxt is not None and begin <= nxt < end:
                stamps.update(nxt + delta for delta in (-1, 0, 1))
        stamps = sorted(stamps)
        result = subprocess.run([args.probe, "--tzif", str(path)], input="".join(f"{stamp + XRT_EPOCH}\n" for stamp in stamps),
                                text=True, capture_output=True, timeout=120, check=True)
        lines = result.stdout.splitlines()
        assert len(lines) == len(stamps), (name, result.stderr)
        for stamp, line in zip(stamps, lines):
            expected_state = state(stamp // 1000)
            if expected_state is None:
                assert line == "UNKNOWN", (name, stamp, line)
                unknowns += 1
                continue
            local = (EPOCH + dt.timedelta(milliseconds=stamp)).astimezone(zone)
            if uncertain(local.replace(tzinfo=None)):
                assert line == "PARTIAL", (name, stamp, line)
                partials += 1
                continue
            assert line not in ("UNKNOWN", "PARTIAL"), (name, stamp, line)
            fields = line.split()
            offset, dst, *civil = map(int, fields[:9])
            expected = [local.year, local.month, local.day, local.hour, local.minute, local.second, local.microsecond // 1000]
            assert (offset, bool(dst), fields[9], civil) == (*expected_state, expected), (name, stamp, line, expected_state, expected)
            candidates = set()
            label = local.replace(tzinfo=None)
            for fold in (0, 1):
                instant = label.replace(tzinfo=zone, fold=fold).astimezone(UTC)
                if instant.astimezone(zone).replace(tzinfo=None) == label and state(millis(instant) // 1000) is not None:
                    candidates.add(millis(instant))
            count = int(fields[10])
            assert [int(value) - XRT_EPOCH for value in fields[11:11 + count]] == sorted(candidates), (name, stamp, line, candidates)
            nxt = next_change(stamp)
            assert int(fields[11 + count]) == (0 if nxt is None else 1), (name, stamp, line, nxt)
            if nxt is not None:
                assert int(fields[12 + count]) - XRT_EPOCH == nxt, (name, stamp, line, nxt)
        total += len(stamps)
        reports.append({"zone": name, "sha256": digest, "queries": len(stamps), "transitions": len(times)})
    report = {"schema": 1, "version": manifest["version"], "zones": len(reports), "queries": total, "unknowns": unknowns, "partial_resolutions": partials, "files": reports}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"[PASS] IANA {manifest['version']}: {len(reports)} zones, {total} state/civil/resolve/next queries, {unknowns} unknowns")


if __name__ == "__main__":
    main()
