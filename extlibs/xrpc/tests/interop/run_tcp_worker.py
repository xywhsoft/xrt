"""Drive or measure the TCP example with an independent socket peer."""
import argparse
import hashlib
import json
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


def requests(count, depth):
    for begin in range(0, count, depth):
        identifiers = range(begin + 1, min(begin + depth, count) + 1)
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
    print("[pass] TCP worker dispatch, pipeline, notification, batch, parse error and record bounds in both modes")
    return {"cases_per_mode": 7, "modes": list(MODES), "replies_per_mode": 131, "ok": True}


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--measure", action="store_true")
    parser.add_argument("--calls", type=int, default=12000)
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()
    if not 1000 <= args.calls <= 100000 or not 1 <= args.repeats <= 30:
        parser.error("choose 1000..100000 calls and 1..30 repeats")
    executable = args.executable.resolve()
    data = measure(executable, args.calls, args.repeats) if args.measure else dispatch_cases(executable)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
