"""Reconcile Redis or MQTT mixed-load journal evidence with its build."""
from __future__ import annotations

import argparse
from itertools import chain
import json
import math
from pathlib import Path
import re

from build_service_mixed import PROBES, digest
from service_mixed_clock import CLOCK_BASIS, FIRST_WINDOW_SECONDS, validate_clock

RELEASE_SECONDS = 72 * 3600


def require(condition, message):
    if not condition:
        raise ValueError(message)


def input_hashes(record):
    return {path.replace("\\", "/"): value for path, value in record["inputs"].items()}


def platform_family(name):
    family = name.split("-", 1)[0].lower()
    return "windows" if family == "win32" else family


def check_build(product, binary, receipt, report, binary_field):
    sha = digest(binary)
    require(sha == receipt["binary_sha256"] == report[binary_field], "binary SHA-256 mismatch")
    # Receipts for the initial Windows campaigns predate the product field.
    require(receipt.get("product", product) == product, "build product mismatch")
    require(platform_family(receipt["platform"]) == platform_family(report["platform"]),
            "build and run platform families differ")
    built, observed = input_hashes(receipt), input_hashes(report)
    source, _ = PROBES[product]
    required = (f"extlibs/{product}/tests/interop/{source}", "single/xrt.h", f"single/extlibs/{product}.h")
    require(all(path in built and path in observed for path in required), "missing build input fingerprint")
    require(all(built[path] == observed[path] for path in built.keys() & observed.keys()),
            "build and run input fingerprints differ")


def check_windows(journal, report, summary, claim, roles, mqtt):
    start = json.loads(next(journal))
    require(start["status"] == "running" and start[claim] is False, "invalid journal start")
    require(all(report[key] == value for key, value in start.items() if key not in ("status", claim)),
            "journal start and report disagree")

    basis = report.get("clock_basis")
    require(basis in (None, CLOCK_BASIS) and start.get("clock_basis") == basis, "invalid journal clock basis")
    limits = report["resource_growth_limits"]
    require(limits == dict(private=64 * 1024 * 1024, handles=32), "unexpected resource growth limits")
    require_resources = summary is None or report[claim]
    required_roles = set(roles) - {"server"}
    required_through_last = required_roles - {"client"}
    prefix = "" if mqtt else "round_"
    percentiles = tuple(f"{prefix}p{rank}_us" for rank in (50, 95, 99, 999))
    previous = dict(rounds=0, seconds=0, cpu_seconds=0, observer_seconds=0, shared0=0, shared1=0)
    windows, largest_gap, terminal = 0, 0, None
    baseline = {}
    previous_clock = None
    for line in journal:
        require(terminal is None, "data after journal terminal record")
        row = json.loads(line)
        run_time, elapsed = row["seconds"], row["observer_seconds"]
        clock = row["clock"]
        require(0 < run_time <= elapsed and math.isfinite(elapsed) and
                elapsed == clock["monotonic_elapsed_seconds"], "invalid journal clock")
        if basis == CLOCK_BASIS:
            duration = report["seconds_requested"] if row.get("complete") is True else None
            validate_clock(clock, previous_clock, duration)
        else:
            offset = clock["wall_elapsed_seconds"] - elapsed
            require(abs(offset) < 5 and math.isclose(offset, clock["offset_seconds"], rel_tol=0, abs_tol=1e-6),
                    "invalid journal clock")
            if windows:
                gap = clock["tick_gap_seconds"]
                require(0 <= gap < 15 and math.isclose(gap, elapsed - previous["observer_seconds"],
                                                       rel_tol=0, abs_tol=1e-6),
                        "journal observation gap")
            else:
                require(elapsed <= FIRST_WINDOW_SECONDS, "initial journal observation gap")
        if previous_clock is not None:
            largest_gap = max(largest_gap, clock["tick_gap_seconds"])
        previous_clock = clock
        rounds = row["rounds"]
        require(type(rounds) is int and rounds > 0, "invalid round count")
        if mqtt:
            require(row["replays"] == (rounds + 15) // 16, "invalid retained replay count")
        if row.get("complete") is True:
            require(windows > 0 and rounds == previous["rounds"] and
                    run_time >= max(report["seconds_requested"], previous["seconds"]),
                    "terminal duration or rounds mismatch")
            terminal = row
            continue

        if require_resources:
            # A broker serves until after the last workload window. The
            # client can retire while its last sample is being collected.
            require(required_through_last <= row.keys() and
                    (not windows or required_roles <= previous.keys()), "missing process resource sample")
        cpu = row["cpu_seconds"]
        require(row["samples"] == rounds - previous["rounds"] > 0 and
                run_time > previous["seconds"] and math.isfinite(cpu) and cpu >= previous["cpu_seconds"],
                "missing windows or nonmonotonic workload counters")
        tails = [row[name] for name in percentiles]
        require(all(math.isfinite(value) and value >= 0 for value in tails) and tails == sorted(tails),
                "invalid latency distribution")
        if mqtt:
            require(sum(row[key] for key in ("shared0", "shared1")) == 3 * rounds and
                    all(row[key] >= previous[key] for key in ("shared0", "shared1")), "invalid shared delivery count")
        for role in roles:
            if role not in row:
                continue  # Retirement can race a process resource sample.
            sample = row[role]
            first = baseline.setdefault(role, sample)
            require(all(type(sample[key]) is int and 0 <= sample[key] <= first[key] + limit
                        for key, limit in limits.items()),
                    f"{role} resource growth limit")
        previous = row
        windows += 1

    if summary is None:
        require(windows > 0, "no complete workload windows")
        return dict(windows=windows, rounds=previous["rounds"], terminal_seen=terminal is not None,
                    observer_seconds=previous_clock["monotonic_elapsed_seconds"],
                    awake_seconds=previous_clock.get("active_elapsed_seconds"),
                    latest_sampled_roles=sorted(role for role in roles if role in previous),
                    largest_tick_gap_seconds=largest_gap)

    require(terminal is not None, "missing journal terminal record")
    expected = {key: value for key, value in terminal.items() if key not in ("observer_seconds", "clock")}
    expected.update(windows=windows, baseline=baseline, cpu_seconds=previous["cpu_seconds"],
                    largest_tick_gap_seconds=largest_gap)
    if mqtt:
        expected.update(shared0=previous["shared0"], shared1=previous["shared1"])
    require(summary == expected, "journal and workload summary disagree")
    return terminal


def complete_lines(stream):
    for line in stream:
        if not line.endswith(b"\n"):
            return  # The writer can be appending this line.
        yield line


def check_running(product, binary, journal_path, receipt):
    mqtt = product == "xmqtt"
    claim, binary_field = ("native_72h", "binary_sha256") if mqtt else ("native_client_72h", "client_sha256")
    with journal_path.open("rb") as stream:
        lines = complete_lines(stream)
        first = next(lines)
        start = json.loads(first)
        check_build(product, binary, receipt, start, binary_field)
        roles = ("client", "broker") if mqtt else ("client",)
        if not mqtt and start["server_owned"]:
            roles += ("server",)
        progress = check_windows(chain((first,), lines), start, None, claim, roles, mqtt)
    return dict(product=product, platform=start["platform"], wsl=start["wsl"], build_commit=receipt["commit"],
                seconds_requested=start["seconds_requested"], **progress)


def check_report(product, binary, report_path, receipt):
    mqtt = product == "xmqtt"
    claim, binary_field, workload = (("native_72h", "binary_sha256", "client") if mqtt else
                                      ("native_client_72h", "client_sha256", "workload"))
    report = json.loads(report_path.read_text(encoding="utf-8"))
    require(report["status"] == "passed", "workload did not pass")
    check_build(product, binary, receipt, report, binary_field)
    roles = ("client", "broker") if mqtt else ("client",)
    sampled_roles = roles + ("server",) if not mqtt and report["server_owned"] else roles
    summary = report[workload]
    with report_path.with_suffix(".jsonl").open(encoding="utf-8") as journal:
        terminal = check_windows(journal, report, summary, claim, sampled_roles, mqtt)
    rounds = terminal["rounds"]
    if mqtt:
        broker = report["broker"]
        require(report["client_exit"] == 0 and broker["complete"] is True and
                broker["accepted"] == 4 + terminal["replays"] and
                broker["forwarded"] == terminal["deliveries"] == 6 * rounds + terminal["replays"],
                "broker and client counters disagree")
    else:
        before, after = report["server_before"], report["server_after"]
        require(all(after[key] == before[key] for key in ("run_id", "process_id", "connected_clients")),
                "Redis peer continuity mismatch")
        expected = dict(set=9 * rounds, get=9 * rounds, publish=rounds, subscribe=1, hello=4)
        require(report["command_delta"] == expected ==
                {key: after["counts"][key] - before["counts"][key] for key in expected},
                "Redis command ledger mismatch")

    platform_name = report["platform"]
    native_run = (report["wsl"] is False and platform_name.startswith(("Windows-", "Linux-")) and
                  not any(token in platform_name.lower() for token in ("microsoft", "wsl")) and
                  report["seconds_requested"] >= RELEASE_SECONDS and set(roles) <= summary["baseline"].keys())
    require(report[claim] is native_run, "native 72-hour claim disagrees with evidence")
    # Replay historical reports with their original claim, then apply the release clock requirement.
    qualified = native_run and report.get("clock_basis") == CLOCK_BASIS
    return dict(product=product, platform=platform_name, native_72h=qualified, build_commit=receipt["commit"],
                windows=summary["windows"], seconds=terminal["seconds"], rounds=rounds)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("product", choices=PROBES)
    parser.add_argument("--binary", type=Path, required=True)
    evidence = parser.add_mutually_exclusive_group(required=True)
    evidence.add_argument("--report", type=Path, help="completed .json report with sibling .jsonl")
    evidence.add_argument("--running-journal", type=Path, help="replay complete lines in an active .jsonl journal")
    parser.add_argument("--build-receipt", type=Path, help="defaults to the binary name followed by .build.json")
    parser.add_argument("--require-native-72h", action="store_true",
                        help="require native endurance with uptime and awake-time evidence")
    args = parser.parse_args()
    if args.running_journal and args.require_native_72h:
        parser.error("--require-native-72h requires a completed --report")
    receipt_path = args.build_receipt or args.binary.with_name(args.binary.name + ".build.json")
    try:
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        require(re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", receipt["commit"]), "invalid build commit")
        if args.running_journal:
            result = check_running(args.product, args.binary, args.running_journal, receipt)
        else:
            result = check_report(args.product, args.binary, args.report, receipt)
            require(not args.require_native_72h or result["native_72h"], "native 72-hour coverage is missing")
    except (OSError, ValueError, TypeError, AttributeError, KeyError, StopIteration) as error:
        parser.exit(1, f"mixed workload evidence rejected: {error or 'empty journal'}\n")
    print(json.dumps(result, allow_nan=False))


if __name__ == "__main__":
    main()
