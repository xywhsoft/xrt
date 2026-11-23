"""Measure healthy MQTT 5 QoS 2 traffic with a full recipient."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, contextmanager
import json
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import sys
from threading import Event
import time

from compare_broker import affinity, fingerprint, parse_cpus, pin, run_observer, start, stop, usage

INTEROP = Path(__file__).resolve().parents[1] / "tests/interop"
sys.path.insert(0, str(INTEROP))
from run_server_handshake import (connect, eof, finish_qos2, ping, publish_qos2,
                                  read, receive_qos2, small_frame, topic)

BLOCKED = b"pressure/held"
SHARED = (b"$share/a/+", b"$share/a/pressure/resumed")
RESUMED = b"pressure/resumed"
CAPACITY = 256
SUBSCRIBERS = 8
REJECTIONS_PER_SECOND = 250
PHASES = ("baseline", "pressured", "recovered")


@contextmanager
def running_broker(args, logfile):
    with logfile.open("xb") as log:
        command = [str(args.broker), "--compare", str(args.idle + 32), "--workers", "2"]
        process = start(command, args.broker_cpus, stdin=subprocess.PIPE, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 8
            while process.poll() is None and time.monotonic() < deadline:
                ready = re.search(r"^127\.0\.0\.1:(\d+)$", logfile.read_text(encoding="utf-8"), re.M)
                if ready:
                    yield process, ("127.0.0.1", int(ready[1]))
                    return
                time.sleep(0.01)
            raise RuntimeError("broker readiness failed")
        finally:
            if stop(process, graceful=True) != 0:
                raise RuntimeError("broker retirement failed")


def measure(args, process, port):
    cpu, _, _ = usage(process.pid)
    began = time.perf_counter()
    result = run_observer(args, port, (args.workload, SUBSCRIBERS, args.rate))
    elapsed = time.perf_counter() - began
    after, rss, peak = usage(process.pid)
    return result | {"broker_cpu_seconds": after - cpu, "observer_process_seconds": elapsed,
                     "broker_rss_bytes": rss, "broker_peak_rss_bytes": peak}


def measure_pressured(args, process, port, producer, blocked_topic):
    finished, ready = Event(), Event()
    timestamps = []

    def generate():
        start_time = time.perf_counter()
        while not finished.is_set():
            sequence = len(timestamps)
            delay = start_time + sequence / REJECTIONS_PER_SECOND - time.perf_counter()
            if delay > 0:
                time.sleep(delay)
            if finished.is_set():
                return
            publish_qos2(producer, blocked_topic, CAPACITY + 1, struct.pack("!I", sequence), quota=True)
            timestamps.append(time.perf_counter())
            ready.set()

    with ThreadPoolExecutor(max_workers=1) as executor:
        job = executor.submit(generate)
        try:
            if not ready.wait(6):
                job.result(timeout=1)
                raise RuntimeError("pressure publisher did not receive its first rejection")
            began = time.perf_counter()
            result = measure(args, process, port)
            ended = time.perf_counter()
        finally:
            finished.set()
            job.result(timeout=6)
    overlap = [stamp for stamp in timestamps if began <= stamp <= ended]
    if len(overlap) < 2:
        raise RuntimeError("missing quota responses during the healthy observer")
    edges = [began, *overlap, ended]
    return result | {"rejected_during_observer": len(overlap), "producer_rejections": len(timestamps),
                     "pressure_observer_seconds": ended - began,
                     "rejection_max_gap_ms": 1000 * max(b - a for a, b in zip(edges, edges[1:]))}


def run_sample(args, logfile, row):
    with running_broker(args, logfile) as (process, endpoint), ExitStack() as peers:
        def open_peer(identity, maximum=None):
            sock, _ = connect(endpoint, 5, identity, maximum)
            return peers.enter_context(sock)

        row["phase"] = "connect"
        slow = open_peer(b"pressure-slow", 1)
        producer = open_peer(b"pressure-source")
        idle = [open_peer(f"pressure-idle-{index}".encode("ascii")) for index in range(args.idle)]
        ping(idle)
        slow.sendall(small_frame(0x82, b"\0\1\0" + topic(BLOCKED) + b"\2"))
        if read(slow, 6) != b"\x90\4\0\1\0\2":
            raise RuntimeError("slow subscriber did not receive a QoS 2 grant")

        row["phase"] = "baseline"
        row["baseline"] = measure(args, process, endpoint[1])
        row["phase"] = "fill"
        for number in range(1, CAPACITY + 1):
            publish_qos2(producer, BLOCKED, number, struct.pack("!H", number))
        first = receive_qos2(slow, BLOCKED, b"\0\1")
        shared = args.workload == "shared"
        blocked_topic = RESUMED if shared else BLOCKED
        if shared:
            # The first filter joins a measured group. The second is a
            # shared route with only the slow member, used to probe its quota.
            filters = b"".join(topic(name) + b"\x02" for name in SHARED)
            slow.sendall(small_frame(0x82, b"\0\2\0" + filters))
            if read(slow, 7) != b"\x90\x05\0\2\0\2\2":
                raise RuntimeError("slow subscriber did not receive both shared grants")
        publish_qos2(producer, blocked_topic, CAPACITY + 1, b"full", quota=True)
        row["quota_checks"] = 1
        ping((slow,))

        row["phase"] = "pressured"
        row["pressured"] = measure_pressured(args, process, endpoint[1], producer, blocked_topic)
        publish_qos2(producer, blocked_topic, CAPACITY + 1, b"still full", quota=True)
        row["quota_checks"] += 1
        ping((slow,))
        row["phase"] = "drain"
        finish_qos2(slow, first)
        for number in range(2, CAPACITY + 1):
            finish_qos2(slow, receive_qos2(slow, BLOCKED, struct.pack("!H", number)))
        ping((slow,))
        publish_qos2(producer, blocked_topic, CAPACITY + 1, b"credit reclaimed")
        finish_qos2(slow, receive_qos2(slow, blocked_topic, b"credit reclaimed"))
        ping((slow,))
        if shared:
            filters = b"".join(topic(name) for name in SHARED)
            slow.sendall(small_frame(0xa2, b"\0\3\0" + filters))
            if read(slow, 7) != b"\xb0\x05\0\3\0\0\0":
                raise RuntimeError("slow subscriber did not leave both shared routes")
            row["shared_recovered"] = True
        row["drained"] = CAPACITY + 1

        row["phase"] = "recovered"
        row["recovered"] = measure(args, process, endpoint[1])
        row["phase"] = "retire"
        ping(idle)
        for peer in (slow, producer, *idle):
            peer.sendall(b"\xe0\0")
            eof(peer)
    row["phase"] = "complete"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("broker", type=Path)
    parser.add_argument("observer", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--idle", type=int, default=1000)
    parser.add_argument("--workload", choices=("wildcard", "shared"), default="wildcard")
    parser.add_argument("--rate", type=int, default=4000)
    parser.add_argument("--seconds", type=int, default=3)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--broker-cpus", type=parse_cpus, required=True)
    parser.add_argument("--load-cpus", type=parse_cpus, required=True)
    parser.add_argument("--control-cpu", type=int, required=True)
    args = parser.parse_args()
    args.rounds = args.rate * args.seconds
    args.qos, args.window, args.subscriber_window, args.warmup = 2, 16, 16, 512
    args.tls_credentials = None
    args.broker, args.observer = args.broker.resolve(strict=True), args.observer.resolve(strict=True)
    if not (0 <= args.idle <= 9968 and 1 <= args.repeats <= 30 and
            1 <= args.seconds <= 60 and 1 <= args.rate and args.rounds <= 100000):
        parser.error("choose at most 9968 idle peers, 30 repeats, 60 seconds and 100000 offers per phase")
    control = {args.control_cpu}
    masks = (args.broker_cpus, args.load_cpus, control)
    available = affinity(os.getpid())
    if not all(mask and mask <= available for mask in masks) or sum(map(len, masks)) != len(set.union(*masks)):
        parser.error("broker, observer and controller require disjoint available CPUs")
    if args.report.exists():
        parser.error("choose a new report path")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    inputs = [args.broker, args.observer, Path(__file__).resolve(),
              Path(__file__).resolve().with_name("compare_broker.py"), INTEROP / "run_server_handshake.py"]
    report = {"platform": platform.platform(), "idle_peers": args.idle, "rate": args.rate,
              "rounds": args.rounds, "warmup": args.warmup, "subscribers": SUBSCRIBERS, "workers": 2,
              "workload": args.workload,
              "qos": args.qos, "publisher_window": args.window, "subscriber_window": args.subscriber_window,
              "reject_rate": REJECTIONS_PER_SECOND, "broker_cpus": sorted(args.broker_cpus),
              "load_cpus": sorted(args.load_cpus), "control_cpu": args.control_cpu,
              "inputs": [fingerprint(path) for path in inputs], "samples": [], "complete": False}
    pin(os.getpid(), control)
    with args.report.open("x", encoding="utf-8") as output:
        try:
            for repeat in range(1, args.repeats + 1):
                row = {"repeat": repeat, "phase": "startup"}
                report["samples"].append(row)
                logfile = args.report.with_suffix(f".{repeat}.broker.log")
                try:
                    run_sample(args, logfile, row)
                except (OSError, AssertionError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
                    row["error"] = f"{type(error).__name__}: {error}"
                    print(f"[failed] run {repeat} {row['phase']}: {row['error']}", flush=True)
                else:
                    p99 = ", ".join(f"{phase}={row[phase]['delivery_p99_us']:.1f}" for phase in PHASES)
                    print(f"[pass] run {repeat} delivery P99 in us, {p99}", flush=True)
            report["complete"] = all(row["phase"] == "complete" for row in report["samples"])
        finally:
            json.dump(report, output, indent=2, allow_nan=False)
            output.write("\n")
    return 0 if report["complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
