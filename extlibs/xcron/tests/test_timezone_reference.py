"""Differential footer-rule test; stdlib zoneinfo reads independently made TZif.

No installed database, network, TZ environment or upstream implementation source
is required. The synthetic TZif has no historical transitions and an explicit
footer, so the rule applies to the entire tested timeline (RFC 9636 section 3.3).
This verifies the rule engine, not XCRON's pending binary TZif reader.
"""
import argparse
import bisect
import datetime as dt
import hashlib
import io
import json
import os
import random
import struct
import subprocess
import sys
import time
from zoneinfo import ZoneInfo

UTC = dt.timezone.utc
EPOCH = dt.datetime(1970, 1, 1, tzinfo=UTC)
XRT_EPOCH = 62135596800000  # XRT_TIME_UNIX_EPOCH, milliseconds since 0001-01-01
RULES = [
    "UTC0", "<+03>-3", "EST5EDT,M3.2.0,M11.1.0",
    "AEST-10AEDT,M10.1.0,M4.1.0/3", "LHST-10:30LHDT-11,M10.1.0,M4.1.0",
    "IST-1GMT0,M10.5.0,M3.5.0/1", "AAA0BBB,M3.5.6/24,M10.5.6/24",
    "AAA-2BBB-4,J60/0,J300/0", "AAA2BBB1,59/0,300/0",
    "AAA0BBB,M3.2.0/-167:59:59,M11.1.0/167:59:59",
    "AAA0BBB,M3.2.0/+167:59:59,M11.1.0/-167:59:59",
    "AAA-23:59:59BBB-23,M10.1.0/-167,M4.1.0/167",
    "AAA23:59:59BBB23,M10.1.0/167,M4.1.0/-167",
    "XXX3EDT4,0/0,J365/23", "EST5EDT,0/0,J365/25",
]


def millis(value):
    delta = value - EPOCH
    return (delta.days * 86400 + delta.seconds) * 1000 + delta.microseconds // 1000


def fixture(rule):
    header = b"TZif3" + bytes(15) + struct.pack(">6I", 0, 0, 0, 0, 1, 4)
    body = struct.pack(">iBB", 0, 0, 0) + b"STD\0"
    return header + body + header + body + b"\n" + rule.encode("ascii") + b"\n"


def state(zone, stamp):
    if zone is None:
        value = time.localtime(stamp // 1000)
        return value.tm_gmtoff, bool(value.tm_isdst), value.tm_zone
    local = (EPOCH + dt.timedelta(milliseconds=stamp)).astimezone(zone)
    return int(local.utcoffset().total_seconds()), bool(local.dst()), local.tzname()


def transitions(zone, year):
    # These fixtures have two well-separated changes, or perpetual/fixed state.
    # Daily sampling brackets changes; integer bisection locates the exact second.
    start = millis(dt.datetime(year - 1, 1, 1, tzinfo=UTC)) // 1000
    end = millis(dt.datetime(year + 2, 1, 1, tzinfo=UTC)) // 1000
    found = []
    previous = state(zone, start * 1000)
    for right in range(start + 86400, end + 1, 86400):
        current = state(zone, right * 1000)
        if current != previous:
            lo, hi = right - 86400, right
            while hi - lo > 1:
                mid = (lo + hi) // 2
                if state(zone, mid * 1000) == previous:
                    lo = mid
                else:
                    hi = mid
            found.append(hi * 1000)
        previous = current
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--oracle", choices=("zoneinfo", "libc"), default="zoneinfo")
    args = parser.parse_args()
    rng = random.Random(0x5843524F4E)
    reports = []
    total = 0
    for rule in RULES:
        # CPython 3.12 zoneinfo treats unprefixed n as one-based. The libc
        # pass checks the exact same n rules against their specified zero base;
        # never alter the production implementation to match this oracle defect.
        if args.oracle == "zoneinfo" and rule == "AAA2BBB1,59/0,300/0":
            continue
        # Direct libc TZ parsing does not provide RFC 9636 3.3.1 footer
        # perpetual semantics for these strings. zoneinfo plus exact C fixtures
        # check them; the direct-libc oracle covers the other thirteen rules.
        if args.oracle == "libc" and rule in ("XXX3EDT4,0/0,J365/23", "EST5EDT,0/0,J365/25"):
            continue
        raw = fixture(rule)
        if args.oracle == "zoneinfo":
            zone = ZoneInfo.from_file(io.BytesIO(raw), key=rule)
        else:
            os.environ["TZ"] = rule
            time.tzset()
            zone = None
        cases = []
        # libc's direct TZ rules are not proleptic before their supported rule
        # era; zoneinfo supplies the ancient-year oracle. Check libc from 1971.
        years = (1971, 2000, 2024, 2100, 2400, 9000) if zone is None else (1000, 1900, 2000, 2024, 2100, 2400, 9000)
        for year in years:
            begin = millis(dt.datetime(year, 1, 1, tzinfo=UTC))
            end = millis(dt.datetime(year + 1, 1, 1, tzinfo=UTC))
            changes = transitions(zone, year)
            stamps = {begin, end - 1}
            stamps.update(rng.randrange(begin, end) for _ in range(180))
            for change in changes:
                if begin <= change < end:
                    stamps.update(change + delta for delta in (-1, 0, 1, 999))
            cases.extend((stamp, changes) for stamp in sorted(stamps))
        result = subprocess.run([args.probe, rule], input="".join(f"{stamp + XRT_EPOCH}\n" for stamp, _ in cases),
                                text=True, capture_output=True, timeout=120, check=True)
        lines = result.stdout.splitlines()
        assert len(lines) == len(cases), (rule, result.stderr, len(lines), len(cases))
        for (stamp, changes), line in zip(cases, lines):
            fields = line.split()
            offset, dst, *civil = map(int, fields[:9])
            if zone is None:
                local = dt.datetime(*time.localtime(stamp // 1000)[:6], microsecond=(stamp % 1000) * 1000)
            else:
                local = (EPOCH + dt.timedelta(milliseconds=stamp)).astimezone(zone)
            expected = [local.year, local.month, local.day, local.hour, local.minute,
                        local.second, local.microsecond // 1000]
            assert (offset, bool(dst), fields[9], civil) == (*state(zone, stamp), expected), (rule, stamp, line, expected)
            candidates = set()
            label = local.replace(tzinfo=None)
            for fold in (0, 1):
                if zone is None:
                    second = int(time.mktime((*label.timetuple()[:8], fold)))
                    if tuple(time.localtime(second)[:6]) == tuple(label.timetuple()[:6]):
                        candidates.add(second * 1000 + label.microsecond // 1000)
                else:
                    instant = label.replace(tzinfo=zone, fold=fold).astimezone(UTC)
                    if instant.astimezone(zone).replace(tzinfo=None) == label:
                        candidates.add(millis(instant))
            count = int(fields[10])
            assert [int(value) - XRT_EPOCH for value in fields[11:11 + count]] == sorted(candidates), (rule, stamp, line, candidates)
            index = bisect.bisect_right(changes, stamp)
            expected_next = changes[index] if index < len(changes) else None
            assert int(fields[13]) == (1 if expected_next is not None else 0), (rule, stamp, line, changes)
            if expected_next is not None:
                assert int(fields[14]) - XRT_EPOCH == expected_next, (rule, stamp, line, expected_next)
        total += len(cases)
        reports.append({"rule": rule, "fixture_sha256": hashlib.sha256(raw).hexdigest(), "cases": len(cases)})
    report = {"schema": 1, "oracle": args.oracle, "python": sys.version, "cases": total, "rules": reports}
    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(f"[PASS] independent {args.oracle}: {len(reports)} rules, {total} state/civil/resolve/next comparisons")


if __name__ == "__main__":
    main()
