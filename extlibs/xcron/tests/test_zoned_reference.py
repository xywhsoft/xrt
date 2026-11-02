"""Independent occurrence oracle: enumerate labels, map with zoneinfo, sort UTC.

No XCRON matcher/search or upstream scheduler implementation is used by this
oracle. Unknown coverage is derived from RFC TZif metadata over the query window.
Every frozen corpus file is verified before it is used.
"""
import argparse
import bisect
import datetime as dt
import hashlib
import io
import itertools
import json
from pathlib import Path
import re
import subprocess
from zoneinfo import ZoneInfo
from test_timezone_reference import EPOCH, UTC, XRT_EPOCH, millis
from test_tzif_reference import decode

# Explicit masks and predicates, independently authored from the expressions.
PROFILES = [
    ("30 1 * * *", 5, [1], [30], [0], lambda d: True),
    ("30 2 * * *", 5, [2], [30], [0], lambda d: True),
    ("45 1 * * *", 5, [1], [45], [0], lambda d: True),
    ("0 0 9-17 * * MON-FRI", 6, range(9, 18), [0], [0], lambda d: d.weekday() < 5),
    ("17,43 */15 * * * *", 6, range(24), range(0, 60, 15), [17, 43], lambda d: True),
    ("0 0 0 29 FEB *", 6, [0], [0], [0], lambda d: d.month == 2 and d.day == 29),
    ("0 15 2 8-14 MAR SUN", 6, [2], [15], [0], lambda d: d.month == 3 and (8 <= d.day <= 14 or d.weekday() == 6)),
    ("0 15 2 */2 MAR SUN", 6, [2], [15], [0], lambda d: d.month == 3 and d.day % 2 == 1 and d.weekday() == 6),
    ("0 0 0 30 FEB *", 6, [0], [0], [0], lambda d: False),
]


def footer_offsets(text):
    if not text:
        return []
    name = r"(?:<[A-Za-z0-9+-]+>|[A-Za-z]+)"
    clock = r"([+-]?\d+(?::\d+(?::\d+)?)?)"
    match = re.match(name + clock + "(?:" + name + clock + "?)?", text)
    assert match, text

    def east(value):
        sign = -1 if value.startswith("-") else 1
        parts = list(map(int, value.lstrip("+-").split(":")))
        return -sign * sum(part * weight for part, weight in zip(parts, (3600, 60, 1)))

    std = east(match[1])
    # Find the optional DST designation separately; group2 is its offset.
    remainder = text[re.match(name + clock, text).end():]
    has_dst = bool(re.match(name, remainder))
    return [std, east(match[2]) if match[2] else std + 3600] if has_dst else [std]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--probe", required=True)
    p.add_argument("--data", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    manifest = json.loads((args.data / "manifest.json").read_text())
    totals = {"match": 0, "next": 0, "zone_errors": 0, "found": 0, "none": 0}
    reports = []
    for zone_number, (name, digest) in enumerate(manifest["files"].items()):
        path = args.data / name
        raw = path.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == digest, name
        zone = ZoneInfo.from_file(io.BytesIO(raw), key=name)
        times, indices, kinds, footer = decode(raw)
        offsets = [kind[0] for kind in kinds] + footer_offsets(footer)
        low_offset, high_offset = min(offsets), max(offsets)
        if "-00" in footer:
            assert footer == "<-00>0", (name, footer)

        def known(second):
            index = bisect.bisect_right(times, second)
            if times and index == len(times):
                return bool(footer) and "-00" not in footer
            if not times and footer:
                return "-00" not in footer
            return kinds[indices[index - 1] if index else 0][2] != "-00"

        def first_unknown(first, last):
            if not known(first // 1000):
                return first
            for second in times[bisect.bisect_left(times, first // 1000):bisect.bisect_right(times, last // 1000)]:
                if not known(second):
                    return max(first, second * 1000)
            return None

        def predicate(profile, local):
            _, _, hours, minutes, seconds, day = profile
            return not local.microsecond and local.hour in hours and local.minute in minutes and local.second in seconds and day(local)

        def expected_next(profile, begin, end, flags):
            first = ((begin + 999) // 1000) * 1000
            if flags and begin % 1000 == 0:
                first += 1000
            last = end // 1000 * 1000
            if first > last:
                return 0, 777
            unknown = first_unknown(first, last)
            start = (EPOCH + dt.timedelta(milliseconds=first, seconds=low_offset)).date()
            finish = (EPOCH + dt.timedelta(milliseconds=last, seconds=high_offset)).date()
            candidates = set()
            while start <= finish:
                for h, m, s in itertools.product(profile[2], profile[3], profile[4]):
                    label = dt.datetime.combine(start, dt.time(h, m, s))
                    if not profile[5](label):
                        continue
                    for fold in (0, 1):
                        instant = label.replace(tzinfo=zone, fold=fold).astimezone(UTC)
                        stamp = millis(instant)
                        if first <= stamp <= last and known(stamp // 1000) and instant.astimezone(zone).replace(tzinfo=None) == label:
                            candidates.add(stamp)
                start += dt.timedelta(days=1)
            candidate = min(candidates) if candidates else None
            if unknown is not None and (candidate is None or unknown <= candidate):
                return -1, 777
            return (1, candidate + XRT_EPOCH) if candidate is not None else (0, 777)

        # Every profile sees each selected transition. Dates are independent of
        # library search; labels on either side are all enumerated before sorting.
        usable = [second for second in times if -2208988800 <= second <= 4133980800]
        selected = sorted(set(usable[:3] + usable[-9:]))
        selected += [millis(dt.datetime(year, 3, 1, tzinfo=UTC)) // 1000 for year in (2024, 2100, 2400)]
        zone_cases = 0
        for number, profile in enumerate(PROFILES):
            cases = []
            for i, second in enumerate(selected):
                base = second * 1000
                cases.extend(("M", base + delta) for delta in (-1000, -1, 0, 1, 1000))
                cases.append(("N", base - 15 * 60000 + (i % 2), base + 6 * 3600000, i % 2))
                if number in (0, 1, 2, 5):
                    cases.append(("N", base - 12 * 3600000, base + 36 * 3600000, 1))
            lines = []
            for case in cases:
                lines.append(f"M {case[1] + XRT_EPOCH}\n" if case[0] == "M" else f"N {case[1] + XRT_EPOCH} {case[2] + XRT_EPOCH} {case[3]}\n")
            result = subprocess.run([args.probe, "--tzif", str(path), str(profile[1]), profile[0]], input="".join(lines), text=True, capture_output=True, timeout=120, check=True)
            output = result.stdout.splitlines()
            assert len(output) == len(cases), (name, result.stderr)
            for case, line in zip(cases, output):
                status, value, code, field, zone_code, segments, months = map(int, line.split())
                if case[0] == "M":
                    totals["match"] += 1
                    stamp = case[1]
                    expected_status = 1 if stamp % 1000 or known(stamp // 1000) else -1
                    local = (EPOCH + dt.timedelta(milliseconds=stamp)).astimezone(zone)
                    expected_value = int(predicate(profile, local)) if expected_status == 1 else 1
                else:
                    totals["next"] += 1
                    expected_status, expected_value = expected_next(profile, *case[1:])
                    if expected_status != -1:
                        totals["found" if expected_status else "none"] += 1
                assert (status, value) == (expected_status, expected_value), (name, profile[0], case, line, expected_status, expected_value)
                if expected_status == -1:
                    totals["zone_errors"] += 1
                    assert code != 0 and zone_code != 0, (name, case, line)
                else:
                    assert code == 0 and zone_code == 0, (name, case, line)
            zone_cases += len(cases)
        reports.append({"zone": name, "sha256": digest, "queries": zone_cases})
    report = {"schema": 1, "oracle": "zoneinfo label enumeration and independent UTC sorting/RFC coverage", "version": manifest["version"], "zones": len(reports), "totals": totals, "files": reports}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"[PASS] zoned IANA {manifest['version']}: {len(reports)} zones, {totals}")


if __name__ == "__main__":
    main()
