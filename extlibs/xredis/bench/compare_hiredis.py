#!/usr/bin/env python3
"""Alternate xredis and Hiredis fixed-arrival samples on a dedicated Redis port."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import subprocess
import sys

BENCH = Path(__file__).resolve().parent
ROOT = BENCH.parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from measure_performance import _metric_summary, _parse_facts, _parse_metrics

COUNTS = {"offered", "accepted", "successful", "unsuccessful", "corrupt", "deadline_missed"}
QUALITY = {"success_requests_per_sec", "success_p50_us", "success_p95_us",
           "success_p99_us", "success_p999_us"}
METRICS = COUNTS | QUALITY | {"generator_p99_us", "process_window_cpu_seconds",
                             "process_peak_rss_bytes", "requests_peak_sampled"}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sample(args, client: Path, name: str, command: list[str]) -> dict:
    try:
        result = subprocess.run([str(client), *command], capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=45, cwd=ROOT)
    except subprocess.TimeoutExpired as error:
        return {"verified": False, "error": "benchmark exceeded 45 seconds",
                "stdout": (error.stdout or b"").decode("utf-8", "replace"),
                "stderr": (error.stderr or b"").decode("utf-8", "replace")}
    sample = {"returncode": result.returncode, "stdout": result.stdout, "stderr": result.stderr}
    if result.returncode != 0:
        return sample | {"verified": False}
    expected = {
        "client": name, "target": "external_loopback", "arrival_model": "fixed_absolute",
        "workers": "1" if name == "xredis" else "0", "transport": "tcp",
        "memory_stats": "not_compiled", "generator_mode": "0",
        "slot_capacity": str(args.window), "max_inflight": str(args.window),
        "payload_bytes": str(args.payload), "target_rate_per_sec": str(args.rate),
        "intended_timeout_ms": "1000",
    }
    try:
        facts = _parse_facts(result.stdout, set(expected) | {"network_backend"})
        metrics = _parse_metrics(result.stdout, METRICS)
    except SystemExit as error:
        return sample | {"verified": False, "error": str(error)}
    verified = (all(facts[key] == value for key, value in expected.items()) and
                all(metrics[key] == args.count for key in ("offered", "accepted", "successful")) and
                all(metrics[key] == 0 for key in ("unsuccessful", "corrupt", "deadline_missed")) and
                0 < metrics["requests_peak_sampled"] <= args.window)
    return sample | {"verified": verified, "facts": facts, "metrics": metrics}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xredis", type=Path, required=True)
    parser.add_argument("--hiredis", type=Path, required=True)
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--rate", type=int, default=5000)
    parser.add_argument("--count", type=int, default=20000)
    parser.add_argument("--window", type=int, default=16)
    parser.add_argument("--payload", type=int, default=32)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not (1 <= args.port <= 65535 and 1 <= args.window <= 128 and
            1 <= args.repeats <= 100 and args.rate > 0 and args.count > 1):
        parser.error("invalid workload or repeat count")
    clients = {"xredis": args.xredis.resolve(strict=True), "hiredis-1.3.0": args.hiredis.resolve(strict=True)}
    if clients["xredis"] == clients["hiredis-1.3.0"]:
        parser.error("provide two distinct executables")
    command = list(map(str, (args.count, args.rate, args.window, args.payload, 1000, 0, 0, args.port, 0)))
    report = {
        "platform": platform.platform(), "arguments": command, "runs": [], "summary": {},
        "executables": {name: {"path": str(path), "sha256": digest(path)} for name, path in clients.items()},
        "inputs": {str(path.relative_to(ROOT)): digest(path) for path in (
            Path(__file__).resolve(), BENCH / "bench_client_openloop.c", BENCH / "openloop_hiredis.h",
            BENCH / "openloop_run.h", BENCH / "openloop_schedule.h",
            BENCH.parent / "tests/support/client_peer.h", ROOT / "tools/measure_performance.py",
        )},
        "variation_limits": {"minimum_repeats": 5, "relative_mad": 0.03, "relative_central_range": 0.06},
    }
    for repeat in range(args.repeats + 1):
        order = list(clients)
        if repeat % 2:
            order.reverse()
        for name in order:
            report["runs"].append({"client": name, "repeat": repeat, "warmup": repeat == 0,
                                   **sample(args, clients[name], name, command)})
    verified = all(run["verified"] for run in report["runs"])
    if verified:
        for name in clients:
            results = [run["metrics"] for run in report["runs"] if run["client"] == name and not run["warmup"]]
            report["summary"][name] = group = {}
            for key in sorted(METRICS - COUNTS):
                group[key] = stats = _metric_summary([run[key] for run in results])
                if key in QUALITY and args.repeats >= 5:
                    stats["variation_pass"] = (stats["relative_mad"] <= 0.03 and
                                               stats["relative_central_range"] <= 0.06)
                for field in ("relative_mad", "relative_range", "relative_central_range"):
                    if not math.isfinite(stats[field]):
                        stats[field] = None
    report["verified"] = verified
    report["variation_pass"] = (all(report["summary"][name][key]["variation_pass"]
                                   for name in clients for key in QUALITY)
                                if verified and args.repeats >= 5 else None)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Redis paired comparison verified={verified} variation_pass={report['variation_pass']}", flush=True)
    return 0 if verified else 1


if __name__ == "__main__":
    raise SystemExit(main())
