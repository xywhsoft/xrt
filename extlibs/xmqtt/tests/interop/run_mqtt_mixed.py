"""Supervise native xmqtt client and broker processes over private loopback."""
from __future__ import annotations

import argparse
from contextlib import ExitStack, suppress
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import queue
import subprocess
import sys
import threading

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from service_mixed_clock import CLOCK_BASIS, FIRST_WINDOW_SECONDS, MAX_GAP_SECONDS, WorkloadClock
from test_xrudp_native_soak import resources

RELEASE_SECONDS = 72 * 3600
PRIVATE_GROWTH = 64 * 1024 * 1024
HANDLE_GROWTH = 32


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def retire(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=6)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=6)
    for pipe in (process.stdin, process.stdout):
        if pipe is not None:
            pipe.close()


def read_output(process):
    inbox = queue.Queue()

    def read():
        try:
            for line in process.stdout:
                inbox.put(line)
        finally:
            inbox.put(None)

    threading.Thread(target=read, daemon=True).start()
    return inbox


def wait_output(inbox, seconds):
    try:
        line = inbox.get(timeout=seconds)
    except queue.Empty as error:
        raise RuntimeError("workload progress deadline") from error
    return json.loads(line) if line is not None else None


def process_snapshot(process):
    if process.poll() is not None:
        return None
    try:
        return resources(process.pid)
    except (OSError, AssertionError, KeyError):
        # A final client window can be read during OS process cleanup.
        with suppress(subprocess.TimeoutExpired):
            process.wait(timeout=0.2)
            return None
        raise


def watch(client, broker, args, state, append, clock):
    inbox = read_output(client)
    previous = {"rounds": 0, "seconds": 0, "cpu_seconds": 0, "shared0": 0, "shared1": 0}
    baseline = {}
    windows = 0
    final = None
    deadline = FIRST_WINDOW_SECONDS
    while (event := wait_output(inbox, deadline)) is not None:
        deadline = MAX_GAP_SECONDS
        complete = event.get("complete") is True
        tick = clock.sample(args.seconds if complete else None)
        require(final is None, "output after terminal client report")
        rounds, replays = event["rounds"], event["replays"]
        require(isinstance(rounds, int) and rounds > 0 and replays == (rounds + 15) // 16,
                "client round or retained replay ledger")
        elapsed = event["seconds"]
        observed = tick["monotonic_elapsed_seconds"]
        require(math.isfinite(elapsed) and previous["seconds"] <= elapsed <= observed,
                f"client time ledger: previous={previous['seconds']:.6f} client={elapsed:.6f} observer={observed:.6f}")
        if complete:
            require(rounds == previous["rounds"] and event["deliveries"] == 6 * rounds + replays and
                    elapsed >= args.seconds and observed >= args.seconds,
                    "incomplete client delivery ledger or duration")
            final = event
        else:
            require(event["samples"] == rounds - previous["rounds"] > 0 and
                    elapsed > previous["seconds"] and math.isfinite(event["cpu_seconds"]) and
                    event["cpu_seconds"] >= previous["cpu_seconds"], "nonadvancing client window")
            shares = ("shared0", "shared1")
            require(all(isinstance(event[name], int) and event[name] >= previous[name] for name in shares) and
                    sum(event[name] for name in shares) == 3 * rounds, "shared recipient accounting")
            tails = [event[f"p{rank}_us"] for rank in (50, 95, 99, 999)]
            require(all(math.isfinite(value) and value >= 0 for value in tails) and tails == sorted(tails),
                    "invalid latency distribution")
            for name, process in (("client", client), ("broker", broker)):
                snapshot = process_snapshot(process)
                if snapshot is None:
                    continue
                event[name] = snapshot
                original = baseline.setdefault(name, snapshot)
                require(snapshot["private"] - original["private"] <= PRIVATE_GROWTH and
                        snapshot["handles"] - original["handles"] <= HANDLE_GROWTH, f"{name} resource growth limit")
            previous = event
            windows += 1
        append(event | {"observer_seconds": observed, "clock": tick})
        if args.kill_broker_after is not None and final is None and "fault" not in state and event["seconds"] >= args.kill_broker_after:
            require(broker.poll() is None, "broker exited before injected kill")
            broker.kill()
            state["fault"] = {"verified_rounds": rounds, "broker_exit": broker.wait(timeout=6), "event": "broker process killed"}
            append(state["fault"])
    state["client_exit"] = client.wait(timeout=6)
    require(state["client_exit"] == 0 and final is not None, "client exited without verified retirement")
    return final | {"windows": windows, "baseline": baseline, "shared0": previous["shared0"], "shared1": previous["shared1"],
                    "cpu_seconds": previous["cpu_seconds"], "largest_tick_gap_seconds": clock.largest_gap}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--pause-ms", type=int, default=3)
    parser.add_argument("--kill-broker-after", type=int, help="separate fault run, inject after a verified window")
    parser.add_argument("--tls-directory", type=Path, help="directory with ca.der, certificate.der and key.der")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not (1 <= args.seconds <= 73 * 3600 and 2 <= args.pause_ms <= 100):
        parser.error("invalid duration or pacing")
    if args.kill_broker_after is not None and not 1 <= args.kill_broker_after <= args.seconds - 6:
        parser.error("leave at least six seconds for fault observation")
    binary = args.binary.resolve(strict=True)
    base = args.report.with_suffix("")
    base.parent.mkdir(parents=True, exist_ok=True)
    wsl = sys.platform.startswith("linux") and ("microsoft" in platform.release().lower() or "WSL_DISTRO_NAME" in os.environ)
    state = {"status": "running", "native_72h": False, "wsl": wsl, "platform": platform.platform(),
             "clock_basis": CLOCK_BASIS, "seconds_requested": args.seconds, "pause_ms": args.pause_ms, "binary_sha256": digest(binary),
             "resource_growth_limits": {"private": PRIVATE_GROWTH, "handles": HANDLE_GROWTH},
             "inputs": {str(path.relative_to(ROOT)): digest(path) for path in (
                 HERE / "mqtt_mixed.c", Path(__file__).resolve(), ROOT / "dev/bench/bench_common.h",
                 ROOT / "extlibs/xmqtt/examples/server/app_listener.h", ROOT / "tools/test_xrudp_native_soak.py",
                 ROOT / "single/xrt.h", ROOT / "single/extlibs/xmqtt.h", ROOT / "tools/service_mixed_clock.py",
             )}}
    broker_arguments, client_arguments = ["--broker"], []
    state["transport"] = "tls13" if args.tls_directory else "tcp"
    clock = None
    try:
        if args.tls_directory:
            material = args.tls_directory.resolve(strict=True)
            ca, certificate, key = (material / name for name in ("ca.der", "certificate.der", "key.der"))
            require(all(path.is_file() for path in (ca, certificate, key)), "missing MQTT TLS material")
            broker_arguments.extend(map(str, (certificate, key)))
            client_arguments.append(str(ca))
            state["tls"] = dict(hostname="localhost", cipher="TLS_AES_128_GCM_SHA256",
                                leaf_sha256=digest(certificate), ca_sha256=digest(ca))
            helper = ROOT / "extlibs/xmqtt/examples/server/app_tls.h"
            state["inputs"][helper.relative_to(ROOT).as_posix()] = digest(helper)
        with ExitStack() as cleanup, Path(f"{base}.jsonl").open("x", encoding="utf-8") as journal:
            def append(event):
                journal.write(json.dumps(event, allow_nan=False) + "\n")
                journal.flush()

            def start(arguments, role, **options):
                errors = cleanup.enter_context(Path(f"{base}.{role}.stderr").open("x", encoding="utf-8"))
                process = subprocess.Popen([str(binary), *arguments], stdout=subprocess.PIPE, stderr=errors,
                                           text=True, encoding="utf-8", cwd=ROOT, **options)
                cleanup.callback(retire, process)
                state[f"{role}_pid"] = process.pid
                return process

            broker = start(broker_arguments, "broker", stdin=subprocess.PIPE)
            status = read_output(broker)
            ready = wait_output(status, 10)
            require(ready and isinstance(ready["port"], int) and 1 <= ready["port"] <= 65535, "broker readiness")
            require(ready.get("transport", "tcp") == state["transport"], "broker transport mismatch")
            state["port"] = ready["port"]
            clock = WorkloadClock()
            client = start(["--load", str(ready["port"]), str(args.seconds), str(args.pause_ms), *client_arguments], "client")
            append(state)
            state["client"] = watch(client, broker, args, state, append, clock)
            require(args.kill_broker_after is None, "fault run unexpectedly completed")
            broker.stdin.write("\n")
            broker.stdin.flush()
            final = wait_output(status, 12)
            require(final and final["complete"] is True and broker.wait(timeout=6) == 0, "broker retirement report")
            require(final["accepted"] == 4 + state["client"]["replays"] and
                    final["forwarded"] == state["client"]["deliveries"], "broker and client counters disagree")
            state["broker"] = final
            state["native_72h"] = (args.seconds >= RELEASE_SECONDS and not wsl and sys.platform in ("win32", "linux") and
                                   set(state["client"]["baseline"]) == {"client", "broker"})
            state["status"] = "passed"
    except (OSError, ValueError, KeyError, RuntimeError, AssertionError, subprocess.SubprocessError) as error:
        state.update(status="failed", error=str(error), native_72h=False)
        if clock is not None and clock.failure is not None:
            state["clock_failure"] = clock.failure
    except KeyboardInterrupt:
        state.update(status="cancelled", native_72h=False)
    with Path(f"{base}.json").open("x", encoding="utf-8") as output:
        json.dump(state, output, indent=2, allow_nan=False)
        output.write("\n")
    print(json.dumps({key: state[key] for key in ("status", "native_72h", "error", "broker", "fault") if key in state}), flush=True)
    return 0 if state["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
