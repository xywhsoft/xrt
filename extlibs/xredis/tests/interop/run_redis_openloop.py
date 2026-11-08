#!/usr/bin/env python3
"""Single unpinned open-loop diagnostic against a disposable real Redis.

The supplied build report records compiler/options/source identity. This runner
also pins actual executable and harness/service fixture hashes. No Hiredis or
production-performance acceptance is implied by a successful diagnostic.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys

import run_redis_init

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
import measure_performance


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--build-report", type=Path, required=True)
    parser.add_argument("--server", type=Path, default=shutil.which("redis-server"))
    parser.add_argument("--count", type=int, default=10000)
    parser.add_argument("--rate", type=int, default=10000)
    parser.add_argument("--slots", type=int, default=256)
    parser.add_argument("--payload", type=int, default=32)
    parser.add_argument("--timeout", type=int, default=1000)
    parser.add_argument("--stats", action="store_true")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.server is None:
        parser.error("redis-server is unavailable; provide --server")
    client = args.client.resolve(strict=True)
    server = Path(args.server).absolute()  # Preserve Redis argv[0] multiplexer name.
    build_path = args.build_report.resolve(strict=True)
    build = json.loads(build_path.read_text(encoding="utf-8"))
    arguments = [str(args.count), str(args.rate), str(args.slots), str(args.payload), str(args.timeout), "0", "0", "{port}", "1" if args.stats else "0"]
    result = run_redis_init.run(client, server, 0, arguments)
    metrics = measure_performance._parse_metrics(result["client_stdout"], {
        "offered", "accepted", "successful", "unsuccessful", "harness_capacity", "client_overloaded", "expired_before_admission",
        "request_timeout", "deadline_missed", "corrupt", "success_requests_per_sec", "success_p50_us", "success_p95_us", "success_p99_us", "success_p999_us",
        "all_p99_us", "all_p999_us", "generator_p99_us", "process_window_cpu_seconds", "process_peak_rss_bytes",
        "window_malloc_calls", "window_calloc_calls", "window_realloc_calls", "window_memdup_calls", "window_block_alloc_calls", "window_backing_alloc_calls",
        "window_backing_alloc_bytes", "handles_before", "handles_after", "retire_seconds",
    })
    facts = measure_performance._parse_facts(result["client_stdout"], {
        "arrival_model", "target", "network_backend", "memory_stats", "generator_mode", "workers", "slot_capacity",
        "payload_bytes", "max_inflight", "target_rate_per_sec", "intended_timeout_ms",
    })
    if facts["target"] != "external_loopback" or facts["workers"] != "1":
        raise RuntimeError("unexpected external workload target/thread budget")
    if facts["memory_stats"] != "enabled":
        for key in metrics:
            if key.startswith("window_"):
                metrics[key] = None  # Unmeasured counters must not masquerade as zero allocations.
    inputs = [Path(__file__), Path(run_redis_init.__file__), ROOT / "tools/measure_performance.py",
              ROOT / "extlibs/xredis/bench/bench_client_openloop.c", ROOT / "extlibs/xredis/bench/openloop_run.h",
              ROOT / "extlibs/xredis/bench/openloop_schedule.h", ROOT / "extlibs/xredis/tests/support/client_peer.h"]
    report = {
        "schema": 1, "purpose": "single unpinned open-loop diagnostic; no comparator or production acceptance",
        "platform": platform.platform(), "arguments": result["client_arguments"], "facts": facts, "metrics": metrics, "stdout": result["client_stdout"],
        "server_version": subprocess.check_output([str(server), "--version"], text=True, encoding="utf-8").strip(),
        "server_path": str(server), "server_sha256": digest(server), "client_path": str(client), "client_sha256": digest(client),
        "supplied_build_report_sha256": digest(build_path),
        "supplied_build_identity": {key: build.get(key) for key in ("compiler_version", "arch", "cflags", "ldflags", "source_revision", "source_dirty", "source_fingerprint", "benchmark_fingerprint")},
        "inputs_sha256": {str(path.resolve().relative_to(ROOT)): digest(path) for path in inputs},
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("[PASS] real Redis fixed-arrival diagnostic; all planned offers accounted for")


if __name__ == "__main__":
    main()
