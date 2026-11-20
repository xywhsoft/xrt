"""Compare MQTT 5 idle connection cost, admission limits and reconnect waves."""
import argparse
import asyncio
from contextlib import contextmanager
import ctypes
import json
import math
import os
from pathlib import Path
import platform
import re
import socket
import subprocess
import tempfile
import time

from compare_broker import MOSQUITTO, affinity, fingerprint, noise, pin, start, stop, usage


if os.name == "nt":
    from ctypes import wintypes
    from compare_broker import handle, kernel

    kernel.GetProcessHandleCount.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel.GetProcessHandleCount.restype = wintypes.BOOL


def resources(pid):
    cpu, rss, peak = usage(pid)
    if os.name == "nt":
        handles = wintypes.DWORD()
        with handle(pid) as process:
            if not kernel.GetProcessHandleCount(process, ctypes.byref(handles)):
                raise ctypes.WinError(ctypes.get_last_error())
        count = handles.value
    else:
        count = len(os.listdir(f"/proc/{pid}/fd"))
    return {"cpu_seconds": cpu, "rss_bytes": rss, "peak_rss_bytes": peak, "os_handles_or_fds": count}


def descriptor_budget(clients, concurrency):
    if os.name != "nt":
        import resource
        soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
        needed = 2 * clients + 2 * concurrency + 256
        if hard != resource.RLIM_INFINITY and needed > hard:
            raise RuntimeError(f"need a descriptor limit of at least {needed}, hard limit is {hard}")
        if soft < needed:
            resource.setrlimit(resource.RLIMIT_NOFILE, (needed, hard))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


async def exact(loop, sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = await loop.sock_recv(sock, size - len(data))
        if not chunk:
            raise EOFError("broker closed during MQTT response")
        data.extend(chunk)
    return bytes(data)


async def handshake(loop, sock, index):
    identity = f"cohort-{index:05}".encode("ascii")
    body = b"\x00\x04MQTT\x05\x02\x00\x00\x00" + len(identity).to_bytes(2, "big") + identity
    await loop.sock_sendall(sock, bytes((0x10, len(body))) + body)
    tag, size = await exact(loop, sock, 2)
    require(tag == 0x20 and 3 <= size < 128, "invalid CONNACK frame")
    ack = await exact(loop, sock, size)
    require(ack[0] == 0 and ack[2] == size - 3, "unexpected CONNACK session or properties length")
    return ack[1]


async def connect_mqtt(loop, sock, address, index, timings=None):
    began = time.perf_counter_ns() if timings is not None else 0
    await loop.sock_connect(sock, address)
    connected = time.perf_counter_ns() if timings is not None else 0
    reason = await handshake(loop, sock, index)
    if timings is not None and reason == 0:
        completed = time.perf_counter_ns()
        timings.append((index, (connected - began) / 1000, (completed - connected) / 1000))
    return reason


def connection_socket():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.setblocking(False)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except OSError:
        sock.close()
        raise
    return sock


async def bounded(indices, concurrency, operation):
    todo = iter(indices)

    async def worker():
        for index in todo:
            await operation(index)

    tasks = [asyncio.create_task(worker()) for _ in range(min(concurrency, len(indices)))]
    try:
        await asyncio.gather(*tasks)
    finally:
        for task in tasks:
            if not task.done():
                task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)


async def reject_excess(loop, address, index):
    with connection_socket() as sock:
        try:
            reason = await asyncio.wait_for(connect_mqtt(loop, sock, address, index), timeout=5)
        except (EOFError, ConnectionError) as error:
            return type(error).__name__
        require(reason >= 0x80, "broker admitted a connection beyond its configured limit")
        return f"CONNACK 0x{reason:02x}"


def latency_summary(values):
    ordered = sorted(values)
    return {f"p{label}_us": ordered[math.ceil(quantile * len(ordered)) - 1]
            for label, quantile in ((50, .50), (95, .95), (99, .99), (999, .999))}


async def sample_cohort(args, address, ready, clients, broker, row):
    loop = asyncio.get_running_loop()
    sockets = {}
    dial_times = [] if args.trace_connect else None
    snapshots = row["broker_resources"] = {}
    outcomes = row["phases"] = {}

    async def open_peer(index):
        sock = connection_socket()
        try:
            reason = await connect_mqtt(loop, sock, address, index, dial_times)
            require(reason == 0, f"client {index} was refused with reason 0x{reason:02x}")
            sockets[index] = sock
        except BaseException:
            sock.close()
            raise

    async def ping_peer(index):
        sock = sockets[index]
        await loop.sock_sendall(sock, b"\xc0\x00")
        require(await exact(loop, sock, 2) == b"\xd0\x00", f"client {index} did not get PINGRESP")

    async def disconnect(sock):
        with sock:
            await loop.sock_sendall(sock, b"\xe0\x00")
            require(await loop.sock_recv(sock, 1) == b"", "broker did not close after DISCONNECT")

    async def close_peer(index):
        await disconnect(sockets.pop(index))

    async def measure(label, indices, operation):
        times = []
        result = outcomes[label] = {"requested": len(indices), "completed": 0}
        if dial_times is not None:
            dial_times.clear()

        async def timed(index):
            began = time.perf_counter_ns()
            try:
                async with asyncio.timeout(5):
                    await operation(index)
            except Exception as error:
                raise RuntimeError(f"{label} client {index}: {type(error).__name__} {error}".rstrip()) from error
            times.append((time.perf_counter_ns() - began) / 1000)

        began = time.perf_counter_ns()
        cpu = time.process_time()
        try:
            await bounded(indices, args.concurrency, timed)
        finally:
            seconds = (time.perf_counter_ns() - began) / 1e9
            result.update(completed=len(times), seconds=seconds, per_sec=len(times) / seconds,
                          observer_cpu_seconds=time.process_time() - cpu)
            if times:
                result.update(latency_summary(times))
            if dial_times:
                result["connect_parts"] = {
                    "tcp": latency_summary([item[1] for item in dial_times]),
                    "mqtt": latency_summary([item[2] for item in dial_times]),
                    "samples": dial_times.copy(),
                }

    all_clients = range(clients)
    churn = range(max(1, clients // 10))
    try:
        with ready:
            ready.setblocking(False)
            ready.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            require(await asyncio.wait_for(handshake(loop, ready, clients + 3), timeout=5) == 0,
                    "broker did not admit readiness client")
            await asyncio.wait_for(disconnect(ready), timeout=5)
        snapshots["empty"] = resources(broker.pid)
        await measure("connect", all_clients, open_peer)
        row["first_limit_refusal"] = await reject_excess(loop, address, clients)
        snapshots["idle_start"] = resources(broker.pid)
        idle_started = time.perf_counter()
        await asyncio.sleep(args.idle_seconds)
        idle = snapshots["idle_end"] = resources(broker.pid)
        row["idle_seconds"] = time.perf_counter() - idle_started
        row["idle_cpu_seconds"] = idle["cpu_seconds"] - snapshots["idle_start"]["cpu_seconds"]
        row["idle_rss_delta_bytes"] = idle["rss_bytes"] - snapshots["empty"]["rss_bytes"]
        row["idle_os_handles_or_fds_delta"] = idle["os_handles_or_fds"] - snapshots["empty"]["os_handles_or_fds"]
        await measure("ping_after_idle", all_clients, ping_peer)
        await measure("disconnect_wave", churn, close_peer)
        await measure("reconnect_wave", churn, open_peer)
        row["second_limit_refusal"] = await reject_excess(loop, address, clients + 1)
        await measure("ping_after_reconnect", all_clients, ping_peer)
        await measure("final_disconnect", all_clients, close_peer)
        snapshots["retired"] = resources(broker.pid)
        await asyncio.wait_for(open_peer(clients + 2), timeout=5)
        await asyncio.wait_for(ping_peer(clients + 2), timeout=5)
        await asyncio.wait_for(close_peer(clients + 2), timeout=5)
        row["admission_after_drain"] = True
    finally:
        for sock in sockets.values():
            sock.close()


@contextmanager
def running_broker(args, implementation, clients, directory, row):
    log = directory / "broker.log"
    port = None
    if implementation == "xmqtt":
        command = [args.xmqtt, "--compare", str(clients)]
        if args.prepare_xmqtt_fds:
            command.append("--prepare-fds")
    else:
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        config = directory / "mosquitto.conf"
        config.write_text(f"listener {port} 127.0.0.1\nmax_connections {clients}\n"
                          "allow_anonymous true\npersistence false\nset_tcp_nodelay true\n"
                          "connection_messages false\nsys_interval 0\nlog_dest stderr\nlog_type error\n",
                          encoding="ascii")
        command = [args.mosquitto, "-c", config]
    with log.open("wb") as output:
        broker = start(command, {args.broker_cpu}, stdin=subprocess.PIPE, stdout=output, stderr=output)
        try:
            deadline = time.monotonic() + 8
            while True:
                require(broker.poll() is None and time.monotonic() < deadline, "broker did not become ready")
                if implementation == "xmqtt":
                    match = re.search(r"^127\.0\.0\.1:(\d+)$", log.read_text(errors="replace"), re.M)
                    if match:
                        port = int(match[1])
                if port is not None:
                    address = ("127.0.0.1", port)
                    try:
                        ready = socket.create_connection(address, timeout=.05)
                        break
                    except (ConnectionError, TimeoutError):
                        pass
                time.sleep(.005)
            with ready:
                yield address, broker, ready
        finally:
            status = stop(broker, graceful=implementation == "xmqtt")
            broker.stdin.close()
            row["broker_log"] = log.read_text(errors="replace")
            if implementation == "xmqtt":
                require(status == 0, f"broker failed retirement with status {status}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("xmqtt", "mosquitto", "report"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--broker-cpu", required=True, type=int)
    parser.add_argument("--load-cpu", required=True, type=int)
    parser.add_argument("--clients", required=True, action="append", type=int)
    parser.add_argument("--concurrency", type=int, choices=range(1, 129), metavar="1..128", default=32)
    parser.add_argument("--idle-seconds", type=float, default=2)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--trace-connect", action="store_true",
                        help="record TCP dial and MQTT handshake time for each connection")
    parser.add_argument("--prepare-xmqtt-fds", action="store_true",
                        help="enable the Linux xmqtt example's startup descriptor preparation")
    args = parser.parse_args()
    original = affinity(os.getpid())
    if args.broker_cpu == args.load_cpu or not {args.broker_cpu, args.load_cpu} <= original:
        parser.error("broker and observer need disjoint available logical CPUs")
    if not all(1 <= count <= 10000 for count in args.clients):
        parser.error("each client count must be 1..10000")
    if not 3 <= args.repeats <= 31 or not 1 <= args.idle_seconds <= 60:
        parser.error("invalid repeat count or idle duration")
    if args.prepare_xmqtt_fds and platform.system() != "Linux":
        parser.error("--prepare-xmqtt-fds requires Linux")
    descriptor_budget(max(args.clients), args.concurrency)
    for name in ("xmqtt", "mosquitto"):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    version = subprocess.check_output([args.mosquitto, "-h"], stderr=subprocess.DEVNULL, text=True, timeout=8).splitlines()[0]
    if version != MOSQUITTO:
        parser.error(f"expected {MOSQUITTO!r}, obtained {version!r}")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = {"platform": platform.platform(), "broker_cpu": args.broker_cpu, "load_cpu": args.load_cpu,
              "mosquitto_version": version, "mqtt": 5, "security": "plaintext", "persistence": False,
              "concurrency": args.concurrency, "idle_seconds": args.idle_seconds, "repeats": args.repeats,
              "trace_connect": args.trace_connect,
              "xmqtt_prepared_descriptors": args.prepare_xmqtt_fds,
              "programs": {name: fingerprint(getattr(args, name)) for name in ("xmqtt", "mosquitto")},
              "runner": fingerprint(Path(__file__)), "workloads": [], "complete": False}
    pin(os.getpid(), {args.load_cpu})
    try:
        for clients in args.clients:
            group = {"clients": clients, "reconnections": max(1, clients // 10), "samples": []}
            report["workloads"].append(group)
            for repeat in range(args.repeats):
                order = ("xmqtt", "mosquitto") if repeat % 2 == 0 else ("mosquitto", "xmqtt")
                for implementation in order:
                    row = {"repeat": repeat, "broker": implementation}
                    group["samples"].append(row)
                    with tempfile.TemporaryDirectory(prefix="xmqtt-connections-") as folder:
                        try:
                            with running_broker(args, implementation, clients, Path(folder), row) as (address, broker, ready):
                                asyncio.run(sample_cohort(args, address, ready, clients, broker, row))
                        except Exception as error:
                            row["failure"] = f"{type(error).__name__}: {error}"
                    print(clients, repeat, implementation,
                          row.get("failure", row.get("idle_rss_delta_bytes")), flush=True)
            group["summary"] = {}
            for label in ("xmqtt", "mosquitto"):
                matching = [row for row in group["samples"] if row["broker"] == label]
                if not any("failure" in row for row in matching):
                    group["summary"][label] = {
                        phase: {metric: noise([row["phases"][phase][metric] for row in matching])
                                for metric in ("per_sec", "p99_us")}
                        for phase in ("connect", "ping_after_idle", "reconnect_wave", "ping_after_reconnect")
                    }
            group["complete"] = len(group["summary"]) == 2
        report["complete"] = all(group["complete"] for group in report["workloads"])
    finally:
        pin(os.getpid(), original)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
