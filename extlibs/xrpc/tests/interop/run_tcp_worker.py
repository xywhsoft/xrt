"""Drive or measure the TCP example with an independent socket peer."""
import argparse
from bisect import bisect_right
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, suppress
import gzip
import hashlib
from heapq import nlargest
from itertools import accumulate
import json
import multiprocessing
from operator import itemgetter
import os
import platform
from pathlib import Path
import select
import socket
import statistics
import struct
import subprocess
import threading
import time
from types import SimpleNamespace


def record(value):
    return json.dumps(value).encode("utf-8") + b"\n"


def exchange(executable, mode, drive, *settings):
    with subprocess.Popen([executable, mode, *settings], stdout=subprocess.PIPE) as process:
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


def staggered_start(first):
    address = first.getpeername()
    pipelined(first)
    with socket.create_connection(address, timeout=5) as second:
        pipelined(second)


def dispatch_cases(executable):
    for mode in MODES.values():
        assert exchange(executable, mode, staggered_start, "2", "1") == 0
        assert exchange(executable, mode, normal) == 0
        assert exchange(executable, mode, pipelined) == 0
        assert exchange(executable, mode, lambda peer: peer.sendall(b" " * 4097)) == 1
        assert exchange(executable, mode, lambda peer: peer.sendall(b"{")) == 1
        healthy, report = multi_exchange(executable, mode, 4, 2, concurrent_peers)
        assert healthy == 0 and report == {"connections": 4, "workers": 2, "requests": 512}
        failed, report = multi_exchange(executable, mode, 2, 2, malformed_peer)
        assert failed == 1 and report is None
        reset, report = multi_exchange(executable, mode, 2, 1, backpressured_peer)
        assert reset == 1 and report is None
    print("[pass] TCP worker dispatch, staggered admission, framing, peer fault and backpressure isolation")
    return {"cases_per_mode": 8, "modes": list(MODES), "replies_per_mode": 131,
            "staggered_replies_per_mode": 256, "parallel_replies_per_mode": 512, "peer_fault_isolation": True,
            "backpressure_isolation_replies_per_mode": 256, "ok": True}


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


def backpressured_peer(peers):
    stalled, healthy = peers
    stalled.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    stalled.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4096)
    payload = "x" * 3000
    offer = record({"jsonrpc": "2.0", "method": "echo", "params": [payload], "id": 2})
    stalled.sendall(offer)
    with stalled.makefile("rb") as stream:
        assert json.loads(stream.readline(4097)) == {"jsonrpc": "2.0", "result": [payload], "id": 2}

    # Leave replies unread. Bound each write so Winsock cannot queue one giant send.
    pending = memoryview(offer * 4096)
    stalled.setblocking(False)
    sent = 0

    def fill():
        nonlocal sent
        deadline = time.monotonic() + 5
        while sent < len(pending) and time.monotonic() < deadline:
            if not select.select([], [stalled], [], 0.2)[1]:
                assert sent >= len(offer), "peer stalled before accepting a record"
                return
            try:
                count = stalled.send(pending[sent:sent + 4096])
            except BlockingIOError:
                continue
            assert count, "peer closed before reaching backpressure"
            sent += count
        raise AssertionError("failed to fill the slow peer's TCP windows")

    fill()
    assert select.select([stalled], [], [], 5)[0], "slow peer has no queued replies"
    assert stalled.recv(1, socket.MSG_PEEK) == b"{", "slow peer closed before isolation check"
    with healthy.makefile("rb") as stream:
        wire, replies = next(requests(128, 128))
        list(checked_batch(healthy, stream, wire, replies))
        fill()
        # SO_LINGER uses two shorts on Winsock and two ints on POSIX.
        linger = struct.pack("@HH" if os.name == "nt" else "@ii", 1, 0)
        stalled.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, linger)
        stalled.close()
        wire, replies = next(requests(128, 128, first=129))
        list(checked_batch(healthy, stream, wire, replies))
        healthy.shutdown(socket.SHUT_WR)
        assert stream.read(1) == b"", "healthy peer did not retire after reset"


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


SATURATION_DEPTH = 8
SATURATION_WARMUP = 64
# Match the native observer's wait guard for coarse Winsock timeouts.
SATURATION_SELECT_GUARD_NS = 16_000_000 if os.name == "nt" else 2_000_000


class SaturationPeer:
    def __init__(self, sock, batches, window, phase_ns=0):
        self.sock = sock
        self.batches = [(wire, b"".join(replies),
                         tuple(accumulate(map(len, replies))) if len(replies) > 1 else (len(replies[0]),) * count)
                        for wire, replies, count in batches]
        self.window = window
        self.phase_ns = phase_ns
        self.sent_at, self.credit_wait, self.times = [], [], []
        self.peak = self.stalls = self.completed = self.reply_index = 0
        self.blocked_since = None
        self.outgoing = None
        self.reply_offset = 0

    def flush(self):
        try:
            sent = self.sock.send(self.outgoing)
        except BlockingIOError:
            return
        if not sent:
            raise RuntimeError("saturation socket send stalled")
        if sent < len(self.outgoing):
            self.outgoing = self.outgoing[sent:]
            return
        self.outgoing = None
        if len(self.sent_at) == len(self.batches):
            self.sock.shutdown(socket.SHUT_WR)

    def offer(self, start, period, rate, now, deadline):
        index = len(self.sent_at)
        if self.outgoing is not None or index == len(self.batches):
            return None
        due = start + self.phase_ns + index * period // rate
        if due > now:
            return due
        used = index - self.completed
        if used == self.window:
            if self.blocked_since is None:
                self.stalls += 1
                self.blocked_since = now
            return None
        sent = time.perf_counter_ns()
        if sent - due >= deadline:
            raise RuntimeError("publisher window did not reopen before the deadline")
        self.sent_at.append(sent)
        self.credit_wait.append(sent - self.blocked_since if self.blocked_since is not None else 0)
        self.blocked_since = None
        self.peak = max(self.peak, used + 1)
        self.outgoing = memoryview(self.batches[index][0])
        self.flush()
        return start + self.phase_ns + (index + 1) * period // rate

    def read(self):
        try:
            data = self.sock.recv(65536)
        except BlockingIOError:
            return True
        observed = time.perf_counter_ns()
        if not data:
            if self.completed != len(self.batches) or self.outgoing is not None:
                raise RuntimeError("saturation reply identity or order mismatch")
            return False
        offset = 0
        while offset < len(data):
            if self.completed == len(self.batches):
                raise RuntimeError("unexpected reply after the final offer")
            if self.completed == len(self.sent_at):
                raise RuntimeError("saturation reply identity or order mismatch")
            _, expected, ends = self.batches[self.completed]
            start = self.reply_offset
            size = min(len(data) - offset, len(expected) - start)
            end = start + size
            if start == 0 and end == len(expected):
                matched = data.startswith(expected, offset)
            else:
                matched = data[offset:offset + size] == expected[start:end]
            if not matched:
                raise RuntimeError("saturation reply identity or order mismatch")
            offset += size
            # Credit remains tied to the whole batch. An individual call is
            # timestamped as soon as the read reaches its final reply byte.
            if end == len(expected):
                self.times.extend([observed] * (len(ends) - self.reply_index))
                self.completed += 1
                self.reply_index = self.reply_offset = 0
            else:
                replies = bisect_right(ends, end, self.reply_index)
                self.times.extend([observed] * (replies - self.reply_index))
                self.reply_index = replies
                self.reply_offset = end
        return True


def saturation_streams(indexed_peers, calls, connections, rate, window, batch_documents, stagger_arrivals):
    streams = {}
    for index, peer in indexed_peers:
        first = index * (calls + SATURATION_WARMUP) + SATURATION_WARMUP + 1
        groups = list(saturation_batches(calls, SATURATION_DEPTH, first, batch_documents))
        phase = index * SATURATION_DEPTH * 1_000_000_000 // (rate * connections) if stagger_arrivals else 0
        streams[peer] = SaturationPeer(peer, groups, window, phase)
    return streams


def saturation_drive(streams, start, rate, end, deadline_ns):
    period = SATURATION_DEPTH * 1_000_000_000
    while streams:
        now = time.perf_counter_ns()
        if now >= end:
            raise RuntimeError("reply missed its fixed-arrival deadline")
        next_due = now + 50_000_000
        writable = []
        for peer, stream in streams.items():
            if (due := stream.offer(start, period, rate, now, deadline_ns)) is not None:
                next_due = min(next_due, due)
            if stream.outgoing is not None:
                writable.append(peer)
        # Keep sub-millisecond pacing independent of platform timeout rounding.
        wait = max(0, next_due - time.perf_counter_ns() - SATURATION_SELECT_GUARD_NS) / 1e9
        readers, writers, _ = select.select(streams, writable, [], wait)
        for peer in writers:
            streams[peer].flush()
        for peer in readers:
            if not streams[peer].read():
                del streams[peer]


def saturation_shard(channel, indexed_peers, settings, cpu):
    with channel, ExitStack() as sockets:
        for _, peer in indexed_peers:
            sockets.enter_context(peer)
        try:
            process_affinity({cpu})
            streams = saturation_streams(indexed_peers, **settings)
            peers = list(streams.values())
            channel.send(("ready", None))
            start, end, deadline_ns = channel.recv()
            used = time.process_time_ns()
            saturation_drive(streams, start, settings['rate'], end, deadline_ns)
            channel.send(("done", (time.process_time_ns() - used) / 1e9))
            # Wait for all other shards to finish their measured interval
            # before spending CPU to serialize the per-reply observations.
            channel.recv()
            channel.send(("results", [SimpleNamespace(
                times=peer.times, sent_at=peer.sent_at, credit_wait=peer.credit_wait,
                peak=peer.peak, stalls=peer.stalls, phase_ns=peer.phase_ns,
            ) for peer in peers]))
        except (AssertionError, EOFError, OSError, RuntimeError, ValueError) as error:
            with suppress(OSError):
                channel.send(("failure", repr(error)))


def receive_shard(channel, kind):
    try:
        if not channel.poll(25):
            raise RuntimeError(f"observer shard did not report {kind}")
        received, value = channel.recv()
    except (EOFError, BrokenPipeError) as error:
        raise RuntimeError(f"observer shard exited before {kind}") from error
    if received != kind:
        raise RuntimeError(f"observer shard expected {kind}, received {received}: {value}")
    return value


def retire_shard(process):
    if process.is_alive():
        process.terminate()
    process.join(5)
    if process.is_alive():
        process.kill()
        process.join(5)
    process.close()


def saturation_sharded(peers, settings, cpus, bounds):
    indexed = list(enumerate(peers))
    context = multiprocessing.get_context("spawn")
    with ExitStack() as stack:
        children = []
        for shard, cpu in enumerate(cpus):
            coordinator, worker = context.Pipe()
            stack.enter_context(coordinator)
            with worker:
                process = context.Process(target=saturation_shard,
                                          args=(worker, indexed[shard::len(cpus)], settings, cpu))
                process.start()
            stack.callback(retire_shard, process)
            children.append((process, coordinator))
        for _, channel in children:
            receive_shard(channel, "ready")
        start = time.perf_counter_ns() + 20_000_000
        schedule = (start, *bounds(start))
        for _, channel in children:
            channel.send(schedule)
        cpu_seconds = [receive_shard(channel, "done") for _, channel in children]
        for _, channel in children:
            channel.send(None)
        results = [None] * len(peers)
        for shard, (process, channel) in enumerate(children):
            observations = receive_shard(channel, "results")
            if len(observations) != len(results[shard::len(cpus)]):
                raise RuntimeError("observer shard returned the wrong number of peers")
            results[shard::len(cpus)] = observations
            process.join(5)
            if process.exitcode != 0:
                raise RuntimeError("observer shard did not exit cleanly")
        return results, start, cpu_seconds


def write_saturation_samples(path, start, rate, streams):
    with gzip.open(path, "xt", compresslevel=3, encoding="utf-8", newline="\n") as output:
        header = {"version": 1, "start_ns": start, "rate_per_connection": rate,
                  "batch_depth": SATURATION_DEPTH}
        output.write(json.dumps(header) + "\n")
        for stream in streams:
            data = {"phase_ns": stream.phase_ns, "sent_ns": stream.sent_at,
                    "credit_wait_ns": stream.credit_wait, "reply_ns": stream.times}
            output.write(json.dumps(data, separators=(",", ":")) + "\n")


def saturation_sample(executable, mode, calls, connections, workers, rate, window, app_cpus, observer_cpus,
                      batch_documents=False, stagger_arrivals=False, observer_shards=1, native_observer=None,
                      *, raw_samples=None):
    depth, warmup, deadline_ns = SATURATION_DEPTH, SATURATION_WARMUP, 5_000_000_000
    period = depth * 1_000_000_000
    batch_count = (calls + depth - 1) // depth
    if observer_shards < 1 or (observer_shards > 1 and observer_shards > min(connections, len(observer_cpus or ()))):
        raise ValueError("observer shards need distinct CPUs and at least one connection each")
    if native_observer is not None and observer_shards != 1:
        raise ValueError("native observer requires one shard")
    shard_cpus = sorted(observer_cpus)[:observer_shards] if observer_shards > 1 else []
    start = elapsed = None
    cpu_seconds = []
    results = []
    settings = dict(calls=calls, connections=connections, rate=rate, window=window,
                    batch_documents=batch_documents, stagger_arrivals=stagger_arrivals)

    def bounds(start):
        last_phase = (connections - 1) * period // (rate * connections) if stagger_arrivals else 0
        last_due = start + (batch_count - 1) * period // rate + last_phase
        return max(last_due, start + calls * 1_000_000_000 // rate) + deadline_ns, deadline_ns

    def drive(peers):
        nonlocal start, elapsed, cpu_seconds, results
        for index, peer in enumerate(peers):
            first = index * (calls + warmup) + 1
            # Preserve bytes beyond the warmup replies for the measured reader.
            with peer.makefile("rb", buffering=0) as stream:
                for wire, replies, _ in saturation_batches(warmup, depth, first, batch_documents):
                    list(checked_batch(peer, stream, wire, replies))
            peer.setblocking(False)
        if shard_cpus:
            results, start, cpu_seconds = saturation_sharded(peers, settings, shard_cpus, bounds)
        else:
            streams = saturation_streams(enumerate(peers), **settings)
            results = list(streams.values())
            native_peers = native_observer.prepare(streams, calls) if native_observer is not None else None
            used = time.process_time_ns()
            start = time.perf_counter_ns() + 20_000_000
            if native_peers is None:
                saturation_drive(streams, start, rate, *bounds(start))
            else:
                native_observer.drive(native_peers, window, rate, start, *bounds(start))
            cpu_seconds = [(time.process_time_ns() - used) / 1e9]
            if native_peers is not None:
                native_observer.collect(native_peers, streams)
        elapsed = max(stream.times[-1] for stream in results) - start

    status, report = multi_exchange(executable, mode, connections, workers, drive, app_cpus, observer_cpus)
    documents_per_peer = batch_count + warmup // depth if batch_documents else calls + warmup
    documents = documents_per_peer * connections
    if status != 0 or report != {"connections": connections, "workers": workers, "requests": documents}:
        raise RuntimeError(f"APP completion ledger mismatch (status={status}, report={report})")
    if elapsed <= 0:
        raise RuntimeError("saturation measurement interval")
    total = calls * connections
    stats = {"calls": total, "app_documents": documents, "calls_per_second": total * 1e9 / elapsed,
             "observer_cpu_seconds": sum(cpu_seconds), "observer_shards": observer_shards,
             "observer_native": native_observer is not None,
             "observer_shard_cpus": shard_cpus, "observer_shard_cpu_seconds": cpu_seconds,
             "peak_batches_in_flight": max(peer.peak for peer in results),
             "credit_stall_batches": sum(peer.stalls for peer in results),
             "phase_offsets_ns": [peer.phase_ns for peer in results]}
    dues = [start + index * period // rate for index in range(batch_count)]
    latency, after_send, offer_lag, credit_wait = [], [], [], []
    for peer in results:
        if len(peer.times) != calls or len(peer.sent_at) != batch_count or len(peer.credit_wait) != batch_count:
            raise RuntimeError("saturation reply count mismatch")
        latency.extend(when - dues[index // depth] - peer.phase_ns for index, when in enumerate(peer.times))
        after_send.extend(when - peer.sent_at[index // depth] for index, when in enumerate(peer.times))
        offer_lag.extend(sent - due - peer.phase_ns for sent, due in zip(peer.sent_at, dues))
        credit_wait.extend(peer.credit_wait)
    if any(value < 0 or value >= deadline_ns for value in latency):
        raise RuntimeError("reply missed its fixed-arrival deadline")
    # Attribute the same slow replies in every phase. Quantiles of separate
    # populations cannot be subtracted to recover a stage's tail.
    credit_by_call = (peer.credit_wait[index // depth] for peer in results for index in range(calls))
    tail = nlargest((total + 99) // 100, zip(latency, after_send, credit_by_call), key=itemgetter(0))
    elapsed_tail = sum(elapsed for elapsed, _, _ in tail)
    after_send_tail = sum(received for _, received, _ in tail)
    credit_tail = sum(blocked for _, _, blocked in tail)
    stats["slowest_one_percent"] = {
        "replies": len(tail), "elapsed_ns": elapsed_tail,
        "offer_delay_excluding_credit_wait_ns": elapsed_tail - after_send_tail - credit_tail,
        "observed_credit_wait_ns": credit_tail, "from_send_to_reply_ns": after_send_tail,
    }
    for prefix, samples in (("", latency), ("from_send_", after_send), ("offer_lag_", offer_lag),
                            ("credit_wait_", credit_wait)):
        stats.update(percentiles(samples, prefix))
    if raw_samples is not None:
        write_saturation_samples(raw_samples, start, rate, results)
        stats["raw_samples"] = raw_samples.name
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
                                      for metric in ("calls_per_second", "p99_us", "p999_us")}
                              for name in MODES}
        print(f"[measure] {connections} connections, {workers} workers, {repeats} verified pairs", flush=True)
        report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return data


def measure_saturation(executable, report, args, raw_dir=None):
    native_observer = None
    if args.observer_native is not None:
        from tcp_native_observer import NativeObserver
        native_observer = NativeObserver(args.observer_native)
    data = {"platform": platform.platform(), "machine": platform.machine(), "python": platform.python_version(),
            "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "calls_per_connection": args.calls, "warmup_per_connection": 64, "batch_depth": 8,
            "connections": args.connections, "workers": args.workers, "window_batches": args.window_batches,
            "request_framing": "json-rpc-batch" if args.batch_documents else "single-records",
            "observer_pacing": "native-select" if native_observer else "multiplexed-nonblocking",
            "observer_library_sha256": hashlib.sha256(args.observer_native.read_bytes()).hexdigest()
            if native_observer else None,
            "observer_shards": args.observer_shards,
            "arrival_schedule": "staggered" if args.stagger_arrivals else "aligned",
            "repeats": args.repeats, "broker_cpus": sorted(args.app_cpus), "observer_cpus": sorted(args.cpus),
            "profiles": [], "complete": False}
    if raw_dir is not None:
        raw_dir.mkdir(parents=True)
        data["raw_samples_directory"] = str(raw_dir.resolve())
    metrics = ("calls_per_second", "p99_us", "p999_us", "offer_lag_p99_us", "credit_wait_p99_us", "from_send_p99_us")
    try:
        for profile, rate in enumerate(args.rates, 1):
            runs = []
            entry = {"rate_per_connection": rate, "samples": runs}
            data["profiles"].append(entry)
            for repeat in range(args.repeats):
                order = list(MODES) if repeat % 2 == 0 else list(reversed(MODES))
                run = {}
                runs.append(run)
                for name in order:
                    try:
                        raw_samples = None if raw_dir is None else raw_dir / f"{profile}-{repeat + 1}-{name}.jsonl.gz"
                        run[name] = saturation_sample(executable, MODES[name], args.calls, args.connections,
                                                      args.workers, rate, args.window_batches, args.app_cpus,
                                                      args.cpus, args.batch_documents, args.stagger_arrivals,
                                                      args.observer_shards, native_observer, raw_samples=raw_samples)
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
    parser.add_argument("--observer-shards", type=int, default=1,
                        help="split the saturation observer across this many processes and CPUs")
    parser.add_argument("--observer-native", type=Path,
                        help="load the native verifier shared library for a single saturation observer")
    parser.add_argument("--batch-documents", action="store_true",
                        help="send each scheduled group as one JSON-RPC batch in saturation mode")
    parser.add_argument("--stagger-arrivals", action="store_true",
                        help="evenly offset each connection within the same saturation offer period")
    parser.add_argument("--raw-dir", type=Path, help="new directory for verified per-call saturation timestamps")
    args = parser.parse_args()
    if not 1000 <= args.calls <= 100000 or not 1 <= args.repeats <= 30:
        parser.error("choose 1000..100000 calls and 1..30 repeats")
    if args.concurrent and not (1 <= args.rate <= 100000 and args.calls <= args.rate * 15):
        parser.error("concurrent offers must fit a 15 second horizon")
    if (args.batch_documents or args.stagger_arrivals or args.observer_shards != 1 or
            args.observer_native is not None) and not args.saturation:
        parser.error("batch documents, staggered arrivals and observer options require --saturation")
    if args.observer_native is not None and args.observer_shards != 1:
        parser.error("--observer-native requires a single observer shard")
    if args.raw_dir is not None and not args.saturation:
        parser.error("--raw-dir requires --saturation")
    if args.saturation:
        if args.raw_dir is not None and args.raw_dir.exists():
            parser.error("--raw-dir must name a new directory")
        if not args.cpus or not args.app_cpus or args.cpus & args.app_cpus:
            parser.error("saturation requires disjoint --cpus and --app-cpus")
        if not 1 <= args.observer_shards <= min(args.connections, len(args.cpus)):
            parser.error("observer shards need distinct CPUs and at least one connection each")
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
        return 0 if measure_saturation(executable, args.report, args, args.raw_dir) else 1
    if args.concurrent:
        measure_concurrent(executable, args.report, args.calls, args.repeats, args.rate, args.cpus)
        return
    data = measure(executable, args.calls, args.repeats) if args.measure else dispatch_cases(executable)
    args.report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
