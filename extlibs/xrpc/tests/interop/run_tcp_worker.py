"""Drive or measure the TCP example with an independent socket peer."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
import hashlib
import json
import os
import platform
from pathlib import Path
import socket
import statistics
import subprocess
import threading
import time


def record(value):
    return json.dumps(value).encode("utf-8") + b"\n"


def exchange(executable, mode, drive):
    with subprocess.Popen([executable, mode], stdout=subprocess.PIPE) as process:
        watchdog = threading.Timer(15, process.kill)
        watchdog.start()
        try:
            host, port = process.stdout.readline().decode("ascii").strip().split(":")
            assert host == "127.0.0.1"
            with socket.create_connection((host, int(port)), timeout=5) as peer:
                drive(peer)
            return process.wait(timeout=5)
        finally:
            watchdog.cancel()
            if process.poll() is None:
                process.kill()


def normal(peer):
    request = {"jsonrpc": "2.0", "id": "tcp-1", "method": "echo", "params": [42]}
    notification = {"jsonrpc": "2.0", "method": "echo"}
    batch = [notification, {"jsonrpc": "2.0", "id": 9, "method": "missing"}]
    wire = record(request)
    peer.sendall(wire[:-1])
    peer.sendall(wire[-1:] + record(notification) + record(batch) + b"[\n")
    peer.shutdown(socket.SHUT_WR)
    with peer.makefile("rb") as stream:
        messages = [json.loads(line) for line in stream]
    assert messages[0] == {"jsonrpc": "2.0", "id": "tcp-1", "result": [42]}
    assert len(messages) == 3 and len(messages[1]) == 1
    error = messages[1][0]
    assert error["id"] == 9 and error["error"]["code"] == -32601
    assert messages[2]["id"] is None and messages[2]["error"]["code"] == -32700


MODES = {"duplex": "--serve", "dispatch": "--dispatch-only"}


def requests(count, depth, first=1):
    for begin in range(0, count, depth):
        identifiers = range(begin + first, min(begin + depth, count) + first)
        wires = [b'{"jsonrpc":"2.0","method":"echo","params":[42],"id":%d}\n' % i for i in identifiers]
        replies = [b'{"jsonrpc":"2.0","result":[42],"id":%d}\n' % i for i in identifiers]
        yield b"".join(wires), replies


def pipelined(peer):
    wire, replies = next(requests(128, 128))
    peer.sendall(wire)
    peer.shutdown(socket.SHUT_WR)
    with peer.makefile("rb") as stream:
        assert list(stream) == replies, "coalesced request stream changed the reply order"


def dispatch_cases(executable):
    for mode in MODES.values():
        assert exchange(executable, mode, normal) == 0
        assert exchange(executable, mode, pipelined) == 0
        assert exchange(executable, mode, lambda peer: peer.sendall(b" " * 4097)) == 1
        assert exchange(executable, mode, lambda peer: peer.sendall(b"{")) == 1
        healthy, report = multi_exchange(executable, mode, 4, 2, concurrent_peers)
        assert healthy == 0 and report == {"connections": 4, "workers": 2, "requests": 512}
        failed, report = multi_exchange(executable, mode, 2, 2, malformed_peer)
        assert failed == 1 and report is None
    print("[pass] TCP worker dispatch, pipeline, notification, batch, parse error and record bounds in both modes")
    return {"cases_per_mode": 7, "modes": list(MODES), "replies_per_mode": 131,
            "parallel_replies_per_mode": 512, "peer_fault_isolation": True, "ok": True}


def sample(executable, mode, warmup, batches, calls):
    latencies = []
    elapsed = None

    def drive(peer):
        nonlocal elapsed
        peer.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        with peer.makefile("rb") as stream:
            for wire, replies in warmup:
                peer.sendall(wire)
                for reply in replies:
                    if stream.readline(len(reply) + 1) != reply:
                        raise RuntimeError("warm-up reply mismatch")

            started = time.perf_counter_ns()
            for wire, replies in batches:
                sent = time.perf_counter_ns()
                peer.sendall(wire)
                for reply in replies:
                    line = stream.readline(len(reply) + 1)
                    latencies.append(time.perf_counter_ns() - sent)
                    if line != reply:
                        raise RuntimeError("timed reply mismatch")
            elapsed = time.perf_counter_ns() - started

    if exchange(executable, mode, drive) != 0 or len(latencies) != calls or not elapsed:
        raise RuntimeError("incomplete TCP worker performance sample")
    latencies.sort()
    return {"calls_per_second": calls * 1e9 / elapsed,
            **{f"p{label}_us": latencies[(calls * quantile + 999) // 1000 - 1] / 1000
               for label, quantile in ((50, 500), (95, 950), (99, 990), (999, 999))}}


def measure(executable, calls, repeats):
    profiles = []
    for depth in (1, 8, 32):
        warmup = list(requests(256, depth))
        batches = list(requests(calls, depth))
        runs = []
        for repeat in range(repeats):
            order = list(MODES) if repeat % 2 == 0 else list(reversed(MODES))
            runs.append({name: sample(executable, MODES[name], warmup, batches, calls) for name in order})
        metrics = runs[0]["duplex"]
        medians = {name: {metric: statistics.median(run[name][metric] for run in runs)
                          for metric in metrics}
                   for name in MODES}
        profiles.append({"depth": depth, "samples": runs, "median": medians})
        print(f"[measure] depth={depth} duplex={medians['duplex']['calls_per_second']:.0f} calls/s "
              f"dispatch={medians['dispatch']['calls_per_second']:.0f} calls/s")
    return {"platform": platform.platform(), "machine": platform.machine(),
            "python": platform.python_version(),
            "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "calls_per_mode_per_repeat": calls, "warmup_calls": 256, "repeats": repeats,
            "profiles": profiles}


def multi_exchange(executable, mode, connections, workers, drive):
    with subprocess.Popen([executable, mode, str(connections), str(workers)], stdout=subprocess.PIPE) as process:
        watchdog = threading.Timer(25, process.kill)
        watchdog.start()
        try:
            host, port = process.stdout.readline().decode("ascii").strip().split(":")
            assert host == "127.0.0.1"
            with ExitStack() as peers:
                sockets = [peers.enter_context(socket.create_connection((host, int(port)), timeout=5))
                           for _ in range(connections)]
                for peer in sockets:
                    peer.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                drive(sockets)
            status = process.wait(timeout=5)
            line = process.stdout.readline()
            return status, json.loads(line) if line else None
        finally:
            watchdog.cancel()
            if process.poll() is None:
                process.kill()


def checked_batch(peer, stream, wire, replies):
    peer.sendall(wire)
    for reply in replies:
        if stream.readline(len(reply) + 1) != reply:
            raise RuntimeError("reply identity or order mismatch")
        yield time.perf_counter_ns()


def concurrent_peers(peers):
    def drive(index):
        peer = peers[index]
        with peer.makefile("rb") as stream:
            for wire, replies in requests(128, 32, index * 128 + 1):
                list(checked_batch(peer, stream, wire, replies))
            peer.shutdown(socket.SHUT_WR)
            assert stream.read(1) == b"", "owner failed to close its output"

    with ThreadPoolExecutor(max_workers=len(peers)) as pool:
        list(pool.map(drive, range(len(peers))))


def malformed_peer(peers):
    bad, healthy = peers
    bad.sendall(b" " * 4097)
    bad.shutdown(socket.SHUT_WR)
    assert bad.recv(1) == b"", "malformed connection did not close independently"
    pipelined(healthy)


def percentiles(samples, prefix):
    samples.sort()
    count = len(samples)
    return {f"{prefix}p{label}_us": samples[(count * q + 999) // 1000 - 1] / 1000
            for label, q in ((50, 500), (95, 950), (99, 990), (999, 999))}


def concurrent_sample(executable, mode, calls, connections, workers, rate):
    depth, warmup = 8, 64
    start = None
    cpu = None
    results = []

    def synchronize():
        nonlocal start, cpu
        cpu = time.process_time_ns()
        start = time.perf_counter_ns() + 20_000_000

    barrier = threading.Barrier(connections, action=synchronize, timeout=10)

    def drive(peers):
        def observe(index):
            peer = peers[index]
            limit = index * (calls + warmup) + 1
            batch = list(requests(calls, depth, limit + warmup))
            latency, lag = [], []
            try:
                with peer.makefile("rb") as stream:
                    for wire, replies in requests(warmup, depth, limit):
                        list(checked_batch(peer, stream, wire, replies))
                    barrier.wait()
                    for i, (wire, replies) in enumerate(batch):
                        due = start + i * depth * 1_000_000_000 // rate
                        while (remaining := due - time.perf_counter_ns()) > 0:
                            time.sleep(remaining / 1e9)
                        submitted = time.perf_counter_ns()
                        lag.extend([submitted - due] * len(replies))
                        for received in checked_batch(peer, stream, wire, replies):
                            if received - due >= 5_000_000_000:
                                raise RuntimeError("reply missed its fixed-arrival deadline")
                            latency.append(received - due)
                    ended = time.perf_counter_ns()
                    peer.shutdown(socket.SHUT_WR)
                    assert stream.read(1) == b"", "unexpected reply after the final offer"
            except Exception:
                barrier.abort()
                raise
            return latency, lag, ended

        with ThreadPoolExecutor(max_workers=connections) as pool:
            results.extend(pool.map(observe, range(connections)))

    status, report = multi_exchange(executable, mode, connections, workers, drive)
    assert status == 0 and report == {"connections": connections, "workers": workers,
                                    "requests": (calls + warmup) * connections}
    elapsed = max(ended for _, _, ended in results) - start
    latency = [value for samples, _, _ in results for value in samples]
    lag = [value for _, samples, _ in results for value in samples]
    assert elapsed > 0 and len(latency) == len(lag) == calls * connections
    return {"calls": len(latency), "calls_per_second": len(latency) * 1e9 / elapsed,
            "observer_cpu_seconds": (time.process_time_ns() - cpu) / 1e9,
            **percentiles(latency, ""), **percentiles(lag, "offer_lag_")}


def variation(samples):
    mid = statistics.median(samples)
    mad = statistics.median(abs(value - mid) for value in samples) / mid
    central = sorted(samples)[1:-1] if len(samples) >= 5 else samples
    span = (max(central) - min(central)) / mid
    return {"median": mid, "relative_mad": mad, "relative_central_range": span,
            "stable": len(samples) >= 5 and mad <= 0.03 and span <= 0.06}


def pin_observer(cpus):
    if hasattr(os, "sched_setaffinity"):
        os.sched_setaffinity(0, cpus)
        assert os.sched_getaffinity(0) == cpus
    elif os.name == "nt":
        import ctypes
        from ctypes import wintypes

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.GetCurrentProcess.restype = wintypes.HANDLE
        kernel.SetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.c_size_t]
        kernel.GetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_size_t),
                                                 ctypes.POINTER(ctypes.c_size_t)]
        handle = kernel.GetCurrentProcess()
        mask, system = ctypes.c_size_t(), ctypes.c_size_t()
        requested = sum(1 << cpu for cpu in cpus)
        if not kernel.SetProcessAffinityMask(handle, requested):
            raise ctypes.WinError(ctypes.get_last_error())
        if not kernel.GetProcessAffinityMask(handle, ctypes.byref(mask), ctypes.byref(system)) or mask.value != requested:
            raise RuntimeError("incorrect observer affinity")
    else:
        raise RuntimeError("observer affinity is unavailable on this platform")


def measure_concurrent(executable, report, calls, repeats, rate, cpus):
    data = {"platform": platform.platform(), "machine": platform.machine(),
            "python": platform.python_version(), "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "calls_per_connection": calls, "warmup_per_connection": 64, "rate_per_connection": rate,
            "batch_depth": 8, "repeats": repeats, "affinity": sorted(cpus) if cpus else "inherited", "profiles": []}
    for connections, workers in ((1, 1), (4, 2), (8, 1), (8, 2)):
        runs = []
        entry = {"connections": connections, "workers": workers, "samples": runs}
        data["profiles"].append(entry)
        try:
            for repeat in range(repeats):
                order = list(MODES) if repeat % 2 == 0 else list(reversed(MODES))
                run = {}
                runs.append(run)
                for name in order:
                    run[name] = concurrent_sample(executable, MODES[name], calls, connections, workers, rate)
        except (OSError, AssertionError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
            entry["error"] = str(error)
            report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
            raise
        entry["variation"] = {name: {metric: variation([run[name][metric] for run in runs])
                                      for metric in ("calls_per_second", "p99_us")}
                              for name in MODES}
        print(f"[measure] {connections} connections, {workers} workers, {repeats} verified pairs", flush=True)
        report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("report", type=Path)
    workload = parser.add_mutually_exclusive_group()
    workload.add_argument("--measure", action="store_true")
    workload.add_argument("--concurrent", action="store_true")
    parser.add_argument("--calls", type=int, default=12000)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--rate", type=int, default=1600, help="offers per connection per second for --concurrent")
    parser.add_argument("--cpus", type=lambda value: set(map(int, value.split(","))), help="inherited CPU mask for observer and server")
    args = parser.parse_args()
    if not 1000 <= args.calls <= 100000 or not 1 <= args.repeats <= 30:
        parser.error("choose 1000..100000 calls and 1..30 repeats")
    if args.concurrent and not (1 <= args.rate <= 100000 and args.calls <= args.rate * 15):
        parser.error("concurrent offers must fit a 15 second horizon")
    if args.cpus:
        if not all(0 <= index < 64 for index in args.cpus):
            parser.error("CPU indices must be in 0..63")
        pin_observer(args.cpus)
    executable = args.executable.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    if args.concurrent:
        measure_concurrent(executable, args.report, args.calls, args.repeats, args.rate, args.cpus)
        return
    data = measure(executable, args.calls, args.repeats) if args.measure else dispatch_cases(executable)
    args.report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
