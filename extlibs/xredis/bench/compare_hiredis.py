#!/usr/bin/env python3
"""Alternate xredis and Hiredis fixed-arrival samples on a dedicated Redis port."""
from __future__ import annotations

import argparse
from contextlib import ExitStack
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import socket
import subprocess
import sys
import tempfile

BENCH = Path(__file__).resolve().parent
ROOT = BENCH.parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from measure_performance import _metric_summary, _parse_facts, _parse_metrics

REJECTIONS = {"harness_capacity", "client_overloaded", "client_closed",
              "admission_error", "expired_before_admission"}
COUNTS = REJECTIONS | {"offered", "accepted", "successful", "unsuccessful", "corrupt",
                       "deadline_missed", "accepted_get", "accepted_set"}
CPU_SCOPES = ("process", "generator")
CPU_MODES = ("user", "kernel")
CPU_SPLIT = {f"{scope}_window_{mode}_cpu_seconds" for scope in CPU_SCOPES for mode in CPU_MODES}
QUALITY = {"success_requests_per_sec", "success_p50_us", "success_p95_us",
           "success_p99_us", "success_p999_us"}
METRICS = COUNTS | QUALITY | {"generator_p99_us", "process_window_cpu_seconds",
                             "generator_window_cpu_seconds", "response_p99_us", "process_peak_rss_bytes",
                             "requests_peak_sampled", "window_peak"}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sample(args, client: Path, name: str, command: list[str], raw_path: Path | None = None) -> dict:
    env = os.environ.copy()
    env.pop("XREDIS_OPENLOOP_TRACE", None)
    if raw_path is not None:
        env["XREDIS_OPENLOOP_TRACE"] = str(raw_path)
    try:
        result = subprocess.run([str(client), *command], capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=45, cwd=ROOT, env=env)
    except subprocess.TimeoutExpired as error:
        return {"verified": False, "error": "benchmark exceeded 45 seconds",
                "stdout": (error.stdout or b"").decode("utf-8", "replace"),
                "stderr": (error.stderr or b"").decode("utf-8", "replace")}
    sample = {"returncode": result.returncode, "stdout": result.stdout, "stderr": result.stderr}
    if result.returncode != 0:
        return sample | {"verified": False}
    if raw_path is not None:
        if not raw_path.is_file():
            return sample | {"verified": False, "error": "missing requested open-loop trace"}
        sample["raw_samples"] = {"name": raw_path.name, "sha256": digest(raw_path)}
    transport = "tls13_aes128gcm_x25519" if getattr(args, "tls_ca", None) else "tcp"
    expected = {
        "client": name, "target": "external_loopback", "arrival_model": "fixed_absolute",
        "workers": "1" if name == "xredis" else "0", "transport": transport,
        "memory_stats": "not_compiled", "generator_mode": "0",
        "slot_capacity": str(args.window), "max_inflight": str(args.window),
        "payload_bytes": str(args.payload), "target_rate_per_sec": str(args.rate),
        "intended_timeout_ms": "1000", "connections": str(args.connections),
        "workload": "get_set" if args.mixed else "echo",
    }
    try:
        facts = _parse_facts(result.stdout, set(expected) | {"network_backend"})
        metric_names = METRICS
        if any(line.partition(":")[0] in CPU_SPLIT for line in result.stdout.splitlines()):
            metric_names |= CPU_SPLIT
        metrics = _parse_metrics(result.stdout, metric_names)
    except SystemExit as error:
        return sample | {"verified": False, "error": str(error)}
    if CPU_SPLIT <= metrics.keys():
        for scope in CPU_SCOPES:
            parts = [metrics[f"{scope}_window_{mode}_cpu_seconds"] for mode in CPU_MODES]
            if min(parts) < 0 or not math.isclose(sum(parts), metrics[f"{scope}_window_cpu_seconds"],
                                                 rel_tol=0, abs_tol=0.000002):
                return sample | {"verified": False, "error": f"inconsistent {scope} CPU counters"}
    groups, tail = divmod(args.count, 4 * args.connections)
    sets = groups * args.connections + min(tail, args.connections) if args.mixed else 0
    expected_get = args.count - sets if args.mixed else 0
    accounted = (all(facts[key] == value for key, value in expected.items()) and
                 metrics["offered"] == args.count and
                 metrics["accepted"] + sum(metrics[key] for key in REJECTIONS) == args.count and
                 metrics["successful"] + metrics["unsuccessful"] == args.count and
                 metrics["accepted_get"] <= expected_get and metrics["accepted_set"] <= sets and
                 (not args.mixed or metrics["accepted_get"] + metrics["accepted_set"] == metrics["accepted"]) and
                 0 <= metrics["window_peak"] <= args.window and
                 0 <= metrics["response_p99_us"] <= metrics["success_p99_us"] and
                 0 <= metrics["generator_window_cpu_seconds"] <= metrics["process_window_cpu_seconds"] + 0.000001)
    verified = (accounted and metrics["accepted"] == metrics["successful"] == args.count and
                all(metrics[key] == 0 for key in ("unsuccessful", "corrupt", "deadline_missed")) and
                metrics["window_peak"] > 0)
    return sample | {"verified": verified, "accounted": accounted, "facts": facts, "metrics": metrics}


def fixture_expected_commands(args, runs):
    counts = []
    for run in runs:
        if not run.get("accounted"):
            return None
        metrics = run["metrics"]
        if metrics["accepted"] != metrics["successful"] or metrics["corrupt"]:
            return None
        counts.append(metrics)

    connections = len(counts) * args.connections
    expected = dict(hello=connections, ping=connections)
    if args.mixed:
        gets = sum(metrics["accepted_get"] for metrics in counts)
        if gets:
            expected["get"] = gets
        expected["set"] = len(counts) + sum(metrics["accepted_set"] for metrics in counts)
    else:
        expected["echo"] = sum(metrics["accepted"] for metrics in counts)
    return expected


def start_fixture(args, stack: ExitStack):
    interop = BENCH.parent / "tests" / "interop"
    sys.path.insert(0, str(interop))
    from run_redis_restart import command_calls, info, start
    from run_redis_tls import certificates, tls_listener

    original = os.sched_getaffinity(0)
    server_cpu, *client_cpus = args.fixture_cpus
    server_mask = {server_cpu}
    client_mask = set(client_cpus)
    stack.callback(os.sched_setaffinity, 0, original)
    runtime = Path(stack.enter_context(tempfile.TemporaryDirectory(prefix="xredis-hiredis-")))
    tls = None
    certificates_sha256 = {}
    if args.fixture_tls:
        openssl = shutil.which("openssl")
        if openssl is None:
            raise RuntimeError("TLS comparison requires openssl")
        certificates_sha256 = certificates(Path(openssl), runtime)
        tls = tls_listener(runtime, "server", "ca")
        args.tls_ca = runtime / "ca.pem"

    # Verify the server's PID and advertised identity after releasing this reservation.
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        args.port = reservation.getsockname()[1]
    server = args.server.absolute()
    os.sched_setaffinity(0, server_mask)
    process, identity = start(stack, server, runtime, args.port, "redis", args.report.with_suffix(".redis.log"), tls)
    os.sched_setaffinity(0, client_mask)
    if os.sched_getaffinity(process.pid) != server_mask or os.sched_getaffinity(0) != client_mask:
        raise RuntimeError("Redis comparison CPU placement differs from the requested masks")

    report = {
        "path": str(server), "sha256": digest(server), "identity": identity,
        "version": info(args.port, "server", tls)["redis_version"],
        "port": args.port, "server_cpu": server_cpu, "client_cpus": client_cpus,
        "certificates_sha256": certificates_sha256, "ledger_verified": False,
        "cpu_before": info(args.port, "cpu", tls),
    }

    def audit(runs):
        final = info(args.port, "server", tls)
        if process.poll() is not None or final["run_id"] != identity["run_id"] or int(final["process_id"]) != process.pid:
            raise RuntimeError("Redis fixture changed during the comparison")
        report["cpu_after"] = info(args.port, "cpu", tls)
        report["commands"] = calls = command_calls(info(args.port, "commandstats", tls))
        expected = fixture_expected_commands(args, runs)
        if expected is not None:
            report["expected_commands"] = expected
            report["ledger_verified"] = {key: count for key, count in calls.items() if key != "info"} == expected
        return report

    return audit


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xredis", type=Path, required=True)
    parser.add_argument("--hiredis", type=Path, required=True)
    endpoint = parser.add_mutually_exclusive_group(required=True)
    endpoint.add_argument("--port", type=int)
    endpoint.add_argument("--server", type=Path, help="start a disposable local Redis fixture on Linux")
    parser.add_argument("--fixture-cpus", help="three distinct logical CPUs: Redis, then two for both clients")
    parser.add_argument("--fixture-tls", action="store_true", help="give the fixture freshly generated TLS credentials")
    parser.add_argument("--rate", type=int, default=5000)
    parser.add_argument("--count", type=int, default=20000)
    parser.add_argument("--window", type=int, default=16)
    parser.add_argument("--payload", type=int, default=32)
    parser.add_argument("--connections", type=int, default=1)
    parser.add_argument("--mixed", action="store_true", help="three binary GETs per SET on each connection")
    parser.add_argument("--tls-ca", type=Path, help="explicit PEM trust for TLS comparison binaries")
    parser.add_argument("--tls-server-name", help="expected peer name and SNI, defaults to localhost")
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--raw-dir", type=Path, help="new directory for per-offer CSV traces")
    args = parser.parse_args()
    if not ((args.port is None or 1 <= args.port <= 65535) and 1 <= args.window <= 128 and
            1 <= args.connections <= 8 and 1 <= args.repeats <= 100 and args.rate > 0 and args.count > 1):
        parser.error("invalid workload or repeat count")
    clients = {"xredis": args.xredis.resolve(strict=True), "hiredis-1.3.0": args.hiredis.resolve(strict=True)}
    if clients["xredis"] == clients["hiredis-1.3.0"]:
        parser.error("provide two distinct executables")
    if args.server is None:
        if args.fixture_cpus is not None or args.fixture_tls:
            parser.error("fixture options require --server")
    else:
        if args.tls_ca is not None or args.tls_server_name is not None:
            parser.error("use --fixture-tls to configure TLS with --server")
        if not hasattr(os, "sched_getaffinity") or args.fixture_cpus is None:
            parser.error("--server requires Linux and --fixture-cpus")
        try:
            args.fixture_cpus = tuple(int(cpu) for cpu in args.fixture_cpus.split(","))
        except ValueError:
            parser.error("expected --fixture-cpus SERVER,CLIENT,CLIENT")
        if (len(args.fixture_cpus) != 3 or len(set(args.fixture_cpus)) != 3 or
                not set(args.fixture_cpus) <= os.sched_getaffinity(0)):
            parser.error("--fixture-cpus needs three different available logical CPUs")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with ExitStack() as stack:
        fixture = start_fixture(args, stack) if args.server is not None else None
        return compare(args, clients, parser, fixture)


def compare(args, clients, parser, fixture) -> int:
    command = list(map(str, (args.count, args.rate, args.window, args.payload, 1000, 0, 0, args.port, 0, args.connections, int(args.mixed))))
    if args.tls_ca is not None:
        args.tls_ca = args.tls_ca.resolve(strict=True)
        if not args.tls_ca.is_file() or not 0 < args.tls_ca.stat().st_size < 65536 or args.tls_server_name == "":
            parser.error("TLS requires a nonempty peer name and a PEM CA file smaller than 64 KiB")
        command += [str(args.tls_ca), args.tls_server_name or "localhost"]
    elif args.tls_server_name is not None:
        parser.error("--tls-server-name requires --tls-ca")
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
    if args.tls_ca is not None:
        report["tls_ca_sha256"] = digest(args.tls_ca)
    if args.raw_dir is not None:
        args.raw_dir = args.raw_dir.resolve()
        try:
            args.raw_dir.mkdir(parents=True)
        except OSError as error:
            parser.error(f"cannot create a new trace directory: {error}")
        report["raw_samples_directory"] = str(args.raw_dir)
    for repeat in range(args.repeats + 1):
        order = list(clients)
        if repeat % 2:
            order.reverse()
        for name in order:
            raw = args.raw_dir / f"{repeat:03}-{name}.csv" if args.raw_dir is not None else None
            report["runs"].append({"client": name, "repeat": repeat, "warmup": repeat == 0,
                                   **sample(args, clients[name], name, command, raw)})
    verified = all(run["verified"] for run in report["runs"])
    if fixture is not None:
        try:
            report["fixture"] = fixture(report["runs"])
        except (OSError, RuntimeError, KeyError, ValueError) as error:
            report["fixture"] = {"ledger_verified": False, "error": str(error)}
        verified = verified and report["fixture"]["ledger_verified"]
    if verified:
        for name in clients:
            results = [run["metrics"] for run in report["runs"] if run["client"] == name and not run["warmup"]]
            report["summary"][name] = group = {}
            reported = METRICS - COUNTS
            if all(CPU_SPLIT <= run.keys() for run in results):
                reported |= CPU_SPLIT
            for key in sorted(reported):
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
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Redis paired comparison verified={verified} variation_pass={report['variation_pass']}", flush=True)
    return 0 if verified else 1


if __name__ == "__main__":
    raise SystemExit(main())
