"""Drive or measure the TCP example with an independent socket peer."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, suppress
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


def saturation_batches(count, depth, first, batch_documents):
    for wire, replies in requests(count, depth, first):
        calls = len(replies)
        if batch_documents:
            wire = b"[" + b",".join(wire.splitlines()) + b"]\n"
            replies = [b"[" + b",".join(reply[:-1] for reply in replies) + b"]\n"]
        yield wire, replies, calls


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


def multi_exchange(executable, mode, connections, workers, drive, app_cpus=None, observer_cpus=None):
    command = [executable, mode, str(connections), str(workers)]
    if app_cpus:
        process_affinity(app_cpus)
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE)
    except OSError:
        if observer_cpus:
            process_affinity(observer_cpus)
        raise
    with process:
        watchdog = threading.Timer(25, process.kill)
        watchdog.start()
        try:
            if observer_cpus:
                process_affinity(observer_cpus)
                process_affinity(app_cpus, process.pid)
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


def saturation_sample(executable, mode, calls, connections, workers, rate, window, app_cpus, observer_cpus,
                      batch_documents=False):
    depth, warmup, deadline_ns = 8, 64, 5_000_000_000
    start = None
    cpu = None
    results = []

    def synchronize():
        nonlocal start, cpu
        cpu = time.process_time_ns()
        start = time.perf_counter_ns() + 20_000_000

    barrier = threading.Barrier(connections, action=synchronize, timeout=10)

    def drive(peers):
        with ThreadPoolExecutor(max_workers=connections) as readers, ThreadPoolExecutor(max_workers=connections) as writers:
            def observe(index):
                peer = peers[index]
                first = index * (calls + warmup) + 1
                batches = list(saturation_batches(calls, depth, first + warmup, batch_documents))
                condition = threading.Condition()
                times, submitted_at, offer_lag, credit_wait = [], [], [], []
                received = sent = peak = stalls = 0
                failed = False

                def read_replies(stream):
                    nonlocal received, failed
                    try:
                        for _, replies, count in batches:
                            for reply in replies:
                                if stream.readline(len(reply) + 1) != reply:
                                    raise RuntimeError("saturation reply identity or order mismatch")
                                times.append(time.perf_counter_ns())
                            if batch_documents:
                                # The full array arrives as one framed response.
                                times.extend([times[-1]] * (count - 1))
                            with condition:
                                received += 1
                                condition.notify()
                        if stream.read(1) != b"":
                            raise RuntimeError("unexpected reply after the final offer")
                    except Exception:
                        with condition:
                            failed = True
                            condition.notify()
                        raise

                try:
                    with peer.makefile("rb") as stream:
                        for wire, replies, _ in saturation_batches(warmup, depth, first, batch_documents):
                            list(checked_batch(peer, stream, wire, replies))
                        reader = readers.submit(read_replies, stream)
                        try:
                            barrier.wait()
                            for index, (wire, _, _) in enumerate(batches):
                                due = start + index * depth * 1_000_000_000 // rate
                                while (remaining := due - time.perf_counter_ns()) > 0:
                                    time.sleep(remaining / 1e9)
                                ready = time.perf_counter_ns()
                                with condition:
                                    if sent - received >= window:
                                        stalls += 1
                                    timeout = max(0, (due + deadline_ns - ready) / 1e9)
                                    if not condition.wait_for(lambda: failed or sent - received < window, timeout):
                                        raise RuntimeError("publisher window did not reopen before the deadline")
                                    if not failed:
                                        sent += 1
                                        peak = max(peak, sent - received)
                                if failed:
                                    reader.result()
                                submitted = time.perf_counter_ns()
                                submitted_at.append(submitted)
                                offer_lag.append(submitted - due)
                                credit_wait.append(submitted - ready)
                                peer.sendall(wire)
                            peer.shutdown(socket.SHUT_WR)
                            reader.result(timeout=10)
                        except Exception:
                            with suppress(OSError):
                                peer.shutdown(socket.SHUT_RDWR)
                            raise
                except Exception:
                    barrier.abort()
                    raise

                latency = [when - (start + (index // depth) * depth * 1_000_000_000 // rate)
                           for index, when in enumerate(times)]
                if len(latency) != calls or any(value < 0 or value >= deadline_ns for value in latency):
                    raise RuntimeError("reply missed its fixed-arrival deadline")
                after_send = [when - submitted_at[index // depth] for index, when in enumerate(times)]
                return {"latencies": {"": latency, "offer_lag_": offer_lag, "credit_wait_": credit_wait,
                                      "from_send_": after_send},
                        "peak": peak, "stalls": stalls, "ended": times[-1]}

            results.extend(writers.map(observe, range(connections)))

    status, report = multi_exchange(executable, mode, connections, workers, drive, app_cpus, observer_cpus)
    documents_per_peer = ((calls + depth - 1) // depth + warmup // depth
                          if batch_documents else calls + warmup)
    documents = documents_per_peer * connections
    if status != 0 or report != {"connections": connections, "workers": workers, "requests": documents}:
        raise RuntimeError(f"APP completion ledger mismatch (status={status}, report={report})")
    elapsed = max(item["ended"] for item in results) - start
    total = calls * connections
    stats = {"calls": total, "app_documents": documents, "calls_per_second": total * 1e9 / elapsed,
             "observer_cpu_seconds": (time.process_time_ns() - cpu) / 1e9,
             "peak_batches_in_flight": max(item["peak"] for item in results),
             "credit_stall_batches": sum(item["stalls"] for item in results)}
    for prefix in results[0]["latencies"]:
        stats.update(percentiles([value for item in results for value in item["latencies"][prefix]], prefix))
    return stats


def variation(samples):
    mid = statistics.median(samples)
    mad = statistics.median(abs(value - mid) for value in samples)
    central = sorted(samples)[1:-1] if len(samples) >= 5 else samples
    span = max(central) - min(central)
    return {"median": mid, "relative_mad": mad / mid if mid else None,
            "relative_central_range": span / mid if mid else None,
            "stable": len(samples) >= 5 and mad <= mid * 0.03 and span <= mid * 0.06}


def process_affinity(cpus, pid=None):
    if hasattr(os, "sched_setaffinity"):
        if pid is None:
            os.sched_setaffinity(0, cpus)
        if os.sched_getaffinity(pid or 0) != cpus:
            raise RuntimeError("incorrect process affinity")
    elif os.name == "nt":
        import ctypes
        from ctypes import wintypes

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.GetCurrentProcess.restype = wintypes.HANDLE
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.SetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.c_size_t]
        kernel.GetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_size_t),
                                                 ctypes.POINTER(ctypes.c_size_t)]
        handle = kernel.GetCurrentProcess() if pid is None else kernel.OpenProcess(0x1000, False, pid)
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            mask, system = ctypes.c_size_t(), ctypes.c_size_t()
            requested = sum(1 << cpu for cpu in cpus)
            if pid is None and not kernel.SetProcessAffinityMask(handle, requested):
                raise ctypes.WinError(ctypes.get_last_error())
            if not kernel.GetProcessAffinityMask(handle, ctypes.byref(mask), ctypes.byref(system)) or mask.value != requested:
                raise RuntimeError("incorrect process affinity")
        finally:
            if pid is not None:
                kernel.CloseHandle(handle)
    else:
        raise RuntimeError("process affinity is unavailable on this platform")


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


def measure_saturation(executable, report, args):
    data = {"platform": platform.platform(), "machine": platform.machine(), "python": platform.python_version(),
            "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "calls_per_connection": args.calls, "warmup_per_connection": 64, "batch_depth": 8,
            "connections": args.connections, "workers": args.workers, "window_batches": args.window_batches,
            "request_framing": "json-rpc-batch" if args.batch_documents else "single-records",
            "repeats": args.repeats, "broker_cpus": sorted(args.app_cpus), "observer_cpus": sorted(args.cpus),
            "profiles": [], "complete": False}
    metrics = ("calls_per_second", "p99_us", "offer_lag_p99_us", "credit_wait_p99_us", "from_send_p99_us")
    try:
        for rate in args.rates:
            runs = []
            entry = {"rate_per_connection": rate, "samples": runs}
            data["profiles"].append(entry)
            for repeat in range(args.repeats):
                order = list(MODES) if repeat % 2 == 0 else list(reversed(MODES))
                run = {}
                runs.append(run)
                for name in order:
                    try:
                        run[name] = saturation_sample(executable, MODES[name], args.calls, args.connections,
                                                      args.workers, rate, args.window_batches, args.app_cpus,
                                                      args.cpus, args.batch_documents)
                    except (OSError, AssertionError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
                        run[name] = {"failure": str(error)}
            entry["summary"] = {}
            for name in MODES:
                samples = [run[name] for run in runs if "failure" not in run[name]]
                entry["summary"][name] = {"completed": len(samples), "metrics": {
                    metric: variation([sample[metric] for sample in samples]) for metric in metrics
                } if samples else {}}
            counts = ", ".join(f"{name}={summary['completed']}/{args.repeats}" for name, summary in entry["summary"].items())
            print(f"[saturation] {rate} per connection, {counts}", flush=True)
            report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        data["complete"] = all(entry["summary"][name]["completed"] == args.repeats
                               for entry in data["profiles"] for name in MODES)
    finally:
        report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return data["complete"]



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("report", type=Path)
    workload = parser.add_mutually_exclusive_group()
    workload.add_argument("--measure", action="store_true")
    workload.add_argument("--concurrent", action="store_true")
    workload.add_argument("--saturation", action="store_true")
    parser.add_argument("--calls", type=int, default=12000)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--rate", type=int, default=1600, help="offers per connection per second for --concurrent")
    parser.add_argument("--cpus", type=lambda value: set(map(int, value.split(","))), help="observer CPU mask, inherited by the APP outside saturation")
    parser.add_argument("--app-cpus", type=lambda value: set(map(int, value.split(","))), help="separate APP CPU mask for saturation")
    parser.add_argument("--rates", type=lambda value: tuple(map(int, value.split(","))), default=(1600, 4000, 8000, 16000))
    parser.add_argument("--connections", type=int, default=8)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--window-batches", type=int, default=16)
    parser.add_argument("--batch-documents", action="store_true",
                        help="send each scheduled group as one JSON-RPC batch in saturation mode")
    args = parser.parse_args()
    if not 1000 <= args.calls <= 100000 or not 1 <= args.repeats <= 30:
        parser.error("choose 1000..100000 calls and 1..30 repeats")
    if args.concurrent and not (1 <= args.rate <= 100000 and args.calls <= args.rate * 15):
        parser.error("concurrent offers must fit a 15 second horizon")
    if args.batch_documents and not args.saturation:
        parser.error("--batch-documents requires --saturation")
    if args.saturation:
        if not args.cpus or not args.app_cpus or args.cpus & args.app_cpus:
            parser.error("saturation requires disjoint --cpus and --app-cpus")
        if not all(1 <= rate <= 100000 and args.calls <= rate * 15 for rate in args.rates):
            parser.error("saturation offers must fit a 15 second horizon at each rate")
        if not (1 <= args.connections <= 32 and 1 <= args.workers <= 8 and 1 <= args.window_batches <= 64):
            parser.error("saturation allows 1..32 connections, 1..8 workers and 1..64 batches in flight")
    for cpus in (args.cpus, args.app_cpus):
        if cpus and not all(0 <= index < 64 for index in cpus):
            parser.error("CPU indices must be in 0..63")
    if args.cpus:
        process_affinity(args.cpus)
    executable = args.executable.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    if args.saturation:
        return 0 if measure_saturation(executable, args.report, args) else 1
    if args.concurrent:
        measure_concurrent(executable, args.report, args.calls, args.repeats, args.rate, args.cpus)
        return
    data = measure(executable, args.calls, args.repeats) if args.measure else dispatch_cases(executable)
    args.report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
