#!/usr/bin/env python3
"""Supervise the continuous xredis workload on a dedicated loopback Redis."""
from __future__ import annotations

import argparse
from contextlib import ExitStack
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import queue
import socket
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from test_xrudp_native_soak import observe_clock, resources

LONG_SECONDS = 72 * 3600
COMMANDS = ("set", "get", "publish", "subscribe", "hello")
PERCENTILES = ("round_p50_us", "round_p95_us", "round_p99_us", "round_p999_us")
PRIVATE_GROWTH = 64 * 1024 * 1024
HANDLE_GROWTH = 32


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def redis_info(port):
    command = b"*2\r\n$4\r\nINFO\r\n$3\r\nall\r\n"
    with socket.create_connection(("127.0.0.1", port), timeout=2) as connection:
        connection.sendall(command)
        with connection.makefile("rb") as reader:
            size = reader.readline(32)
            require(size.startswith(b"$") and size.endswith(b"\r\n"), "Redis INFO bulk header")
            length = int(size[1:-2])
            require(0 < length <= 512 * 1024, "Redis INFO size")
            raw = reader.read(length)
            require(len(raw) == length and reader.read(2) == b"\r\n", "truncated Redis INFO")
    fields = dict(line.split(":", 1) for line in raw.decode("utf-8", "replace").splitlines() if ":" in line)
    counts = {}
    for command in COMMANDS:
        stats = fields.get(f"cmdstat_{command}", "calls=0").split(",")
        counts[command] = int(stats[0].removeprefix("calls="))
    return {"counts": counts, **{key: fields[key] for key in (
        "redis_version", "run_id", "process_id", "connected_clients", "used_memory", "used_cpu_sys", "used_cpu_user")}}


def ready(server, port):
    limit = time.monotonic() + 10
    while time.monotonic() < limit:
        require(server.poll() is None, "Redis exited during startup")
        try:
            initial = redis_info(port)
        except OSError:
            time.sleep(0.05)
        else:
            require(int(initial["process_id"]) == server.pid, "loopback Redis is not the spawned fixture")
            return initial
    raise RuntimeError("Redis readiness deadline")


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=6)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=6)


def monitor(client, server, duration, append, start, wall):
    inbox = queue.Queue()

    def copy_lines():
        for line in client.stdout:
            inbox.put(line)
        inbox.put(None)

    thread = threading.Thread(target=copy_lines, daemon=True)
    thread.start()
    rounds = 0
    samples = 0
    cpu = 0
    elapsed = 0
    final = None
    baseline = {}
    clock = {"largest_tick_gap": 0}
    last = None
    try:
        while True:
            try:
                line = inbox.get(timeout=25 if last is None else 15)
            except queue.Empty as error:
                raise RuntimeError("mixed workload heartbeat deadline") from error
            if line is None:
                break
            now = time.monotonic()
            observe_clock(clock, now, last, start, wall, time.time())
            last = now
            event = json.loads(line)
            require(isinstance(event["rounds"], int) and math.isfinite(event["seconds"]) and
                    0 < event["seconds"] <= now - start, "mixed workload clock or count")
            require(final is None, "output after terminal report")
            if event.get("complete") is True:
                require(rounds > 0 and event["rounds"] == rounds and event["seconds"] >= duration and
                        now - start >= duration, "incomplete measured interval")
                final = event
            else:
                require(event["rounds"] > rounds and event["samples"] == event["rounds"] - rounds,
                        "nonadvancing workload or missing samples")
                require(math.isfinite(event["cpu_seconds"]) and event["cpu_seconds"] >= cpu and event["seconds"] > elapsed,
                        "nonmonotonic workload counters")
                cpu, elapsed = event["cpu_seconds"], event["seconds"]
                tails = [event[name] for name in PERCENTILES]
                require(all(math.isfinite(value) and value >= 0 for value in tails) and tails == sorted(tails),
                        "invalid latency distribution")
                rounds = event["rounds"]
                samples += 1
                for name, process in (("client", client), ("server", server)):
                    if process is None or process.poll() is not None:
                        continue
                    try:
                        snapshot = resources(process.pid)
                    except (OSError, AssertionError, KeyError):
                        # A final tick can arrive concurrently with process exit.
                        if name == "client":
                            try:
                                if client.wait(timeout=0.2) == 0:
                                    continue
                            except subprocess.TimeoutExpired:
                                pass
                        raise
                    event[name] = snapshot
                    original = baseline.setdefault(name, snapshot)
                    require(snapshot["private"] - original["private"] <= PRIVATE_GROWTH and
                            snapshot["handles"] - original["handles"] <= HANDLE_GROWTH, f"{name} resource growth limit")
            append(event | {"observer_seconds": now - start, "clock": clock["clock"]})
        require(client.wait(timeout=6) == 0 and final is not None, "workload exited without verified retirement")
        return final | {"windows": samples, "cpu_seconds": cpu,
                        "largest_tick_gap_seconds": clock["largest_tick_gap"], "baseline": baseline}
    finally:
        stop(client)
        thread.join(timeout=3)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    endpoint = parser.add_mutually_exclusive_group(required=True)
    endpoint.add_argument("--server", type=Path, help="start this native Redis executable in a private directory")
    endpoint.add_argument("--port", type=int, help="use an already isolated loopback Redis fixture")
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--pause-ms", type=int, default=5)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not (1 <= args.seconds <= LONG_SECONDS + 3600 and 2 <= args.pause_ms <= 100 and
            (args.port is None or 1 <= args.port <= 65535)):
        parser.error("invalid duration, pacing or loopback port")

    client_path = args.client.resolve(strict=True)
    platform_name = platform.platform()
    wsl = sys.platform.startswith("linux") and ("microsoft" in platform.release().lower() or "WSL_DISTRO_NAME" in os.environ)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    state = {"status": "running", "native_client_72h": False, "platform": platform_name, "wsl": wsl,
             "seconds_requested": args.seconds, "pause_ms": args.pause_ms, "clients": 4, "workers": 2, "pipeline": 8,
             "client_sha256": digest(client_path), "resource_growth_limits": {"private": PRIVATE_GROWTH, "handles": HANDLE_GROWTH},
             "inputs": {str(path.relative_to(ROOT)): digest(path) for path in (
                 Path(__file__).resolve(), Path(__file__).with_name("redis_mixed.c").resolve(),
                 ROOT / "extlibs/xredis/bench/openloop_schedule.h", ROOT / "single/xrt.h",
                 ROOT / "single/extlibs/xredis.h", ROOT / "tools/test_xrudp_native_soak.py",
             )}}
    server = None
    try:
        with ExitStack() as cleanup, args.report.with_suffix(".jsonl").open("x", encoding="utf-8") as journal, \
                args.report.with_suffix(".stderr").open("x", encoding="utf-8") as errors:
            def append(event):
                journal.write(json.dumps(event, allow_nan=False) + "\n")
                journal.flush()

            if args.server:
                server_path = args.server.absolute()
                state["server_sha256"] = digest(server_path)
                folder = cleanup.enter_context(tempfile.TemporaryDirectory(prefix="xredis-mixed-"))
                with socket.socket() as listener:
                    listener.bind(("127.0.0.1", 0))
                    port = listener.getsockname()[1]
                log = cleanup.enter_context(args.report.with_suffix(".redis.log").open("x", encoding="utf-8"))
                command = [str(server_path), "--bind", "127.0.0.1", "--port", str(port),
                           "--save", "", "--appendonly", "no", "--dir", folder]
                server = subprocess.Popen(command, stdout=log, stderr=log, cwd=ROOT)
                cleanup.callback(stop, server)
                before = ready(server, port)
            else:
                port = args.port
                before = redis_info(port)
            state["server_before"] = before
            state["server_owned"] = server is not None
            append(state)
            wall, start = time.time(), time.monotonic()
            with subprocess.Popen([str(client_path), str(port), str(args.seconds), str(args.pause_ms)],
                                  stdout=subprocess.PIPE, stderr=errors, text=True, encoding="utf-8", cwd=ROOT) as client:
                state["workload"] = monitor(client, server, args.seconds, append, start, wall)
            after = redis_info(port)
            state["server_after"] = after
            rounds = state["workload"]["rounds"]
            state["command_delta"] = {cmd: after["counts"][cmd] - before["counts"][cmd] for cmd in COMMANDS}
            require(after["run_id"] == before["run_id"] and after["process_id"] == before["process_id"],
                    "server identity changed")
            require(state["command_delta"] == dict(set=9 * rounds, get=9 * rounds, publish=rounds, subscribe=1, hello=4),
                    "Redis command ledger mismatch")
            require(after["connected_clients"] == before["connected_clients"], "server connected-client baseline changed")
            state["native_client_72h"] = (args.seconds >= LONG_SECONDS and not wsl and
                                          sys.platform in ("win32", "linux") and
                                          "client" in state["workload"]["baseline"])
            state["status"] = "passed"
    except (OSError, ValueError, KeyError, RuntimeError, AssertionError, subprocess.SubprocessError) as error:
        state.update(status="failed", error=str(error), native_client_72h=False)
    except KeyboardInterrupt:
        state.update(status="cancelled", native_client_72h=False)
    output = args.report.with_suffix(".json")
    with output.open("x", encoding="utf-8") as target:
        json.dump(state, target, indent=2, allow_nan=False)
        target.write("\n")
    print(json.dumps({key: state[key] for key in ("status", "native_client_72h", "command_delta", "error") if key in state}))
    return 0 if state["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
