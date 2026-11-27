"""Compare loopback MQTT 5 fan-out with an identical fixed-arrival observer."""
import argparse
from contextlib import ExitStack, contextmanager
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import socket
import ssl
import statistics
import struct
import subprocess
import sys
import tempfile
import time

MOSQUITTO = "mosquitto version 2.1.2"
METRICS = ("offered", "deliveries", "measurement_seconds", "ingress_per_sec", "deliveries_per_sec",
           "delivery_p50_us", "delivery_p95_us", "delivery_p99_us", "delivery_p999_us", "offer_lag_p99_us",
           "pacer_lag_p99_us")
OBSERVER_CPU_METRICS = ("observer_cpu_seconds", "pacer_cpu_seconds")
OBSERVER_ACK_METRICS = ("subscriber_ack_packets", "subscriber_ack_writes")
PUBLISHER_METRICS = ("publisher_p99_us", "publisher_credit_wait_p99_us", "publisher_from_send_p99_us")
SHARED_METRICS = ("share_min_deliveries", "share_max_deliveries")
SHARED_GROUPS = 2
TLS_CIPHER = "TLS_AES_128_GCM_SHA256"
BROKER_LOG_TAIL_BYTES = 16 * 1024
PARTIAL_MEASUREMENT = re.compile(
    r"^Partial measurement: sent=(?P<sent>\d+) confirmed=(?P<confirmed>\d+) deliveries=(?P<deliveries>\d+)\n?$",
    re.MULTILINE,
)


class ObserverFailure(RuntimeError):
    def __init__(self, exit_code, errors):
        match = PARTIAL_MEASUREMENT.search(errors)
        self.progress = {key: int(value) for key, value in match.groupdict().items()} if match else None
        detail = PARTIAL_MEASUREMENT.sub("", errors).strip()
        super().__init__(f"observer exit {exit_code}: {detail}")


if os.name == "nt":
    from ctypes import wintypes as win

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    kernel.OpenProcess.argtypes = [win.DWORD, win.BOOL, win.DWORD]
    kernel.OpenProcess.restype = win.HANDLE
    kernel.CloseHandle.argtypes = [win.HANDLE]
    kernel.GetProcessAffinityMask.argtypes = [win.HANDLE, ctypes.POINTER(ctypes.c_size_t),
                                             ctypes.POINTER(ctypes.c_size_t)]
    kernel.SetProcessAffinityMask.argtypes = [win.HANDLE, ctypes.c_size_t]
    kernel.GetProcessTimes.argtypes = [win.HANDLE] + [ctypes.POINTER(win.FILETIME)] * 4
    psapi.GetProcessMemoryInfo.argtypes = [win.HANDLE, win.LPVOID, win.DWORD]

    @contextmanager
    def handle(pid):
        opened = kernel.OpenProcess(0x400 | 0x200 | 0x10, False, pid)
        if not opened:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            yield opened
        finally:
            kernel.CloseHandle(opened)

    def affinity(pid):
        mask, system = ctypes.c_size_t(), ctypes.c_size_t()
        with handle(pid) as process:
            if not kernel.GetProcessAffinityMask(process, ctypes.byref(mask), ctypes.byref(system)):
                raise ctypes.WinError(ctypes.get_last_error())
        return {cpu for cpu in range(ctypes.sizeof(mask) * 8) if mask.value & (1 << cpu)}

    def pin(pid, cpus):
        with handle(pid) as process:
            if not kernel.SetProcessAffinityMask(process, sum(1 << cpu for cpu in cpus)):
                raise ctypes.WinError(ctypes.get_last_error())

    class Memory(ctypes.Structure):
        _fields_ = [("size", win.DWORD), ("faults", win.DWORD), ("peak", ctypes.c_size_t),
                    ("rss", ctypes.c_size_t), ("unused", ctypes.c_size_t * 6)]

    def usage(pid):
        times = [win.FILETIME() for _ in range(4)]
        memory = Memory()
        memory.size = ctypes.sizeof(memory)
        with handle(pid) as process:
            if not kernel.GetProcessTimes(process, *(ctypes.byref(t) for t in times)):
                raise ctypes.WinError(ctypes.get_last_error())
            if not psapi.GetProcessMemoryInfo(process, ctypes.byref(memory), memory.size):
                raise ctypes.WinError(ctypes.get_last_error())
        cpu = sum((t.dwHighDateTime << 32) | t.dwLowDateTime for t in times[2:]) / 10_000_000
        return cpu, memory.rss, memory.peak

else:
    affinity = os.sched_getaffinity
    pin = os.sched_setaffinity

    def usage(pid):
        process = Path("/proc") / str(pid)
        stat = (process / "stat").read_text().rsplit(") ", 1)[1].split()
        status = (process / "status").read_text()
        rss, peak = (int(re.search(rf"^{field}:\s+(\d+) kB", status, re.M)[1]) * 1024
                     for field in ("VmRSS", "VmHWM"))
        return (int(stat[11]) + int(stat[12])) / os.sysconf("SC_CLK_TCK"), rss, peak


def start(command, cpus, **kwargs):
    pid = os.getpid()
    original = affinity(pid)
    pin(pid, cpus)
    try:
        process = subprocess.Popen(command, **kwargs)
    finally:
        pin(pid, original)
    try:
        if affinity(process.pid) != cpus:
            raise RuntimeError(f"unexpected affinity for process {process.pid}")
    except BaseException:
        stop(process)
        raise
    return process


def stop(process, graceful=False):
    if process.poll() is None:
        if graceful:
            process.stdin.write(b"\n")
            process.stdin.close()
        else:
            process.terminate()
    try:
        return process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        return process.wait(timeout=5)


def run_observer(args, port, workload):
    kind, peers, rate = workload
    command = [str(args.observer), "--remote", str(port), str(peers), str(args.rounds),
               str(rate), str(args.warmup), kind]
    if args.qos:
        command.extend((str(args.qos), str(args.window), str(args.subscriber_window)))
    publishers = getattr(args, "publishers", 1)
    if publishers > 1:
        command.extend(("--publishers", str(publishers)))
    if args.tls_credentials:
        command.extend(("--tls", str(args.tls_credentials / "ca.pem"), "localhost"))
    process = start(command, args.load_cpus, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        output, errors = process.communicate(timeout=20 + (args.rounds + args.warmup) / rate)
    except BaseException:
        stop(process)
        raise
    if process.returncode:
        raise ObserverFailure(process.returncode, errors)
    result = {key: float(value) for key, value in (line.split(": ", 1) for line in output.splitlines())}
    metrics = METRICS + (("publish_confirmed", "publisher_credit_stalls") + PUBLISHER_METRICS if args.qos else ())
    if kind == "shared":
        metrics += SHARED_METRICS
    if args.tls_credentials:
        metrics += ("tls_connections",)
    if result.keys() & set(OBSERVER_CPU_METRICS):
        metrics += OBSERVER_CPU_METRICS
    has_ack_metrics = (args.tls_credentials is not None and args.qos == 2
                       and any(key in result for key in OBSERVER_ACK_METRICS))
    if has_ack_metrics:
        metrics += OBSERVER_ACK_METRICS
    if set(result) != set(metrics) or not all(math.isfinite(v) and v >= 0 for v in result.values()):
        raise ValueError(f"invalid observer output {output!r}")
    destinations = SHARED_GROUPS if kind == "shared" else peers
    if result["offered"] != args.rounds or result["deliveries"] != destinations * args.rounds:
        raise ValueError("publication and delivery counts do not match the workload")
    if args.tls_credentials and result["tls_connections"] != peers + publishers:
        raise ValueError("missing authenticated observer connections")
    if has_ack_metrics:
        packets = result["subscriber_ack_packets"]
        writes = result["subscriber_ack_writes"]
        expected = (args.rounds + args.warmup) * destinations * 2
        if packets != expected or not writes.is_integer() or not 0 < writes <= packets:
            raise ValueError("invalid subscriber acknowledgement ledger")
    if kind == "shared":
        low, high = (result[key] for key in SHARED_METRICS)
        if not (low.is_integer() and high.is_integer() and low <= high <= args.rounds
                and low * peers <= result["deliveries"] <= high * peers):
            raise ValueError("invalid shared member delivery bounds")
    if args.qos:
        if result["publish_confirmed"] != args.rounds:
            raise ValueError("missing terminal publisher confirmations")
        stalls = result["publisher_credit_stalls"]
        if not stalls.is_integer() or stalls > args.rounds:
            raise ValueError("invalid publisher credit-stall count")
    return result


@contextmanager
def idle_connections(port, count):
    if not count:
        yield lambda: None
        return
    fixture = Path(__file__).resolve().parents[1] / "tests/interop"
    sys.path.insert(0, str(fixture))
    try:
        from run_server_handshake import connect_packet, ping, read
    finally:
        sys.path.pop(0)
    endpoint = ("127.0.0.1", port)
    linger = struct.pack("HH" if os.name == "nt" else "ii", 1, 0)
    with ExitStack() as stack:
        peers = []
        for start in range(0, count, 64):
            first = len(peers)
            for index in range(start, min(start + 64, count)):
                peer = stack.enter_context(socket.create_connection(endpoint, timeout=5))
                # Repeated 10,000-peer trials must release local ports on teardown.
                peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, linger)
                peer.sendall(connect_packet(5, f"compare-idle-{index}".encode("ascii")))
                peers.append(peer)
            for peer in peers[first:]:
                kind, remaining = read(peer, 2)
                if kind != 0x20 or not 3 <= remaining < 128 or read(peer, remaining)[:2] != b"\0\0":
                    raise RuntimeError("idle peer did not receive a successful MQTT 5 CONNACK")
        ping(peers)
        yield lambda: ping(peers)


def run_sample(args, implementation, workload, directory):
    path = directory / "broker.log"
    tls = args.tls_credentials
    connection_limit = args.idle_peers + workload[1] + args.publishers if args.idle_peers else 1024
    cwd = None
    if implementation == "mosquitto":
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        config = directory / "mosquitto.conf"
        # The xmqtt APP admits 256 outstanding deliveries per connection.
        # Mosquitto's queue limit applies beyond the active inflight window.
        limits = f"max_inflight_messages 64\nmax_queued_messages {256 - args.subscriber_window}\n" if args.qos else ""
        security = ""
        if tls:
            cwd = tls
            security = ("certfile certificate.pem\nkeyfile key.pem\n"
                        f"tls_version tlsv1.3\nciphers_tls1.3 {TLS_CIPHER}\n")
        config.write_text(f"listener {port} 127.0.0.1\nallow_anonymous true\npersistence false\n"
                          f"max_connections {connection_limit}\nmax_packet_size 16384\nset_tcp_nodelay true\n"
                          "sys_interval 0\nlog_dest stderr\nlog_type error\nlog_type notice\n"
                          + limits + security, encoding="ascii")
        command = [str(args.mosquitto), "-c", str(config)]
        pattern = None
    else:
        port = None
        command = [str(args.xmqtt)]
        if tls:
            command.extend((str(tls / "certificate.der"), str(tls / "key.der")))
        command.append("--compare")
        if args.idle_peers:
            command.append(str(connection_limit))
        if args.xmqtt_workers != 1:
            command.extend(("--workers", str(args.xmqtt_workers)))
        pattern = re.compile(r"^127\.0\.0\.1:(\d+)$", re.M)
    with path.open("wb") as log:
        broker = start(command, args.broker_cpus, cwd=cwd, stdin=subprocess.PIPE, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 8
            while True:
                message = path.read_text(errors="replace")
                if broker.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError(f"broker did not start: {message.strip()}")
                if pattern and (match := pattern.search(message)):
                    port = int(match[1])
                if port is not None:
                    try:
                        with socket.create_connection(("127.0.0.1", port), timeout=0.1 if tls else 0.05) as peer:
                            if tls:
                                with args.tls_context.wrap_socket(peer, server_hostname="localhost") as secure:
                                    if (secure.version() != "TLSv1.3" or secure.cipher()[0] != TLS_CIPHER
                                            or secure.getpeercert(binary_form=True) != args.tls_leaf):
                                        raise ValueError("negotiated TLS profile or leaf certificate differs")
                            break
                    except (ConnectionError, TimeoutError):
                        pass
                time.sleep(0.01)
            time.sleep(0.05)
            with idle_connections(port, args.idle_peers) as verify_idle:
                cpu, idle_rss, _ = usage(broker.pid)
                start_time = time.perf_counter()
                result = run_observer(args, port, workload)
                seconds = time.perf_counter() - start_time
                after, rss, peak = usage(broker.pid)
                verify_idle()
            if args.idle_peers:
                result.update(idle_peers_verified=args.idle_peers, broker_idle_rss_bytes=idle_rss)
            result.update(broker_cpu_seconds=after - cpu, observer_process_seconds=seconds,
                          broker_rss_bytes=rss, broker_peak_rss_bytes=peak)
        finally:
            status = stop(broker, graceful=implementation == "xmqtt")
            if implementation == "xmqtt" and status and broker.returncode is not None:
                raise RuntimeError(f"xmqtt retirement exit {status}: {path.read_text(errors='replace')}")
    return result


def fingerprint(path):
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def noise(samples):
    middle = statistics.median(samples)
    mad = statistics.median(abs(value - middle) for value in samples)
    central = sorted(samples)[1:-1] if len(samples) >= 5 else samples
    span = max(central) - min(central)
    return {"median": middle, "relative_mad": mad / middle if middle else 0,
            "relative_central_span": span / middle if middle else 0,
            "stable": mad <= middle * 0.03 and span <= middle * 0.06}


def parse_workload(value):
    try:
        kind, peers, rate = value.split(":")
        peers, rate = int(peers), int(rate)
        if kind not in ("exact", "wildcard", "shared") or not 1 <= peers <= 64 or not 1 <= rate <= 100000:
            raise ValueError(value)
        if kind == "shared" and peers < SHARED_GROUPS:
            raise ValueError(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected exact|wildcard|shared:subscribers:rate, with at least two shared members") from error
    return kind, peers, rate


def parse_cpus(value):
    return {int(cpu) for cpu in value.split(",")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for label in ("xmqtt", "mosquitto", "observer", "report"):
        parser.add_argument(f"--{label}", required=True, type=Path)
    affinity_args = parser.add_mutually_exclusive_group(required=True)
    affinity_args.add_argument("--broker-cpu", type=int)
    affinity_args.add_argument("--broker-cpus", type=parse_cpus)
    parser.add_argument("--load-cpus", type=parse_cpus, required=True)
    parser.add_argument("--xmqtt-workers", type=int, choices=range(1, 65), metavar="1..64", default=1)
    parser.add_argument("--workload", action="append", type=parse_workload, required=True)
    parser.add_argument("--rounds", type=int, default=6000)
    parser.add_argument("--warmup", type=int, default=512)
    parser.add_argument("--qos", type=int, choices=(0, 1, 2), default=0)
    parser.add_argument("--window", type=int, choices=range(1, 65), metavar="1..64", default=16,
                        help="publisher outstanding window for QoS 1/2")
    parser.add_argument("--subscriber-window", type=int, choices=range(1, 65), metavar="1..64",
                        help="Receive Maximum advertised by each subscriber, defaults to --window")
    parser.add_argument("--publishers", type=int, choices=range(1, 9), metavar="1..8", default=1,
                        help="publishing connections sharing the reliable arrival schedule")
    parser.add_argument("--idle-peers", type=int, default=0,
                        help="hold and verify this many additional plaintext MQTT 5 connections")
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--tls-credentials", type=Path, metavar="DIRECTORY",
                        help="use the same CA and localhost test identity for both brokers")
    args = parser.parse_args()
    if args.idle_peers < 0 or any(args.idle_peers + peers + args.publishers > 10000
                                for _, peers, _ in args.workload):
        parser.error("idle peers and workload peers must fit 10,000 total connections")
    if args.idle_peers and args.tls_credentials:
        parser.error("--idle-peers requires a plaintext comparison")
    if args.subscriber_window is not None and not args.qos:
        parser.error("--subscriber-window requires QoS 1 or 2")
    if args.publishers > 1 and not args.qos:
        parser.error("multiple publishers require QoS 1 or 2")
    if args.qos and args.subscriber_window is None:
        args.subscriber_window = args.window
    if args.broker_cpu is not None:
        args.broker_cpus = {args.broker_cpu}
    if not (args.broker_cpus | args.load_cpus) <= affinity(os.getpid()) or args.broker_cpus & args.load_cpus:
        parser.error("broker and observer need disjoint available logical CPUs")
    if not 1 <= args.rounds <= 100000 or not 0 <= args.warmup <= 100000 or not 3 <= args.repeats <= 31:
        parser.error("invalid round, warmup, or repeat count")
    for label in ("xmqtt", "mosquitto", "observer"):
        setattr(args, label, getattr(args, label).resolve(strict=True))
    if args.tls_credentials:
        args.tls_credentials = args.tls_credentials.resolve(strict=True)
        tls = args.tls_credentials
        for filename in ("ca.pem", "certificate.pem", "certificate.der", "key.pem", "key.der"):
            if not (tls / filename).is_file():
                parser.error(f"missing TLS fixture file {filename}")
        args.tls_context = ssl.create_default_context(cafile=str(tls / "ca.pem"))
        args.tls_context.minimum_version = ssl.TLSVersion.TLSv1_3
        args.tls_leaf = (tls / "certificate.der").read_bytes()
        if ssl.PEM_cert_to_DER_cert((tls / "certificate.pem").read_text(encoding="ascii")) != args.tls_leaf:
            parser.error("both brokers require the same leaf certificate")
    version = subprocess.check_output([args.mosquitto, "-h"], stderr=subprocess.DEVNULL, text=True, timeout=8).splitlines()[0]
    if version != MOSQUITTO:
        parser.error(f"expected {MOSQUITTO!r}, obtained {version!r}")
    report = {"platform": platform.platform(), "broker_cpu": args.broker_cpu,
              "broker_cpus": sorted(args.broker_cpus), "load_cpus": sorted(args.load_cpus),
              "xmqtt_workers": args.xmqtt_workers, "publishers": args.publishers,
              "rounds": args.rounds, "warmup": args.warmup, "repeats": args.repeats,
              "mosquitto_version": version, "mqtt": 5, "qos": args.qos,
              "window": args.window if args.qos else None, "subscriber_window": args.subscriber_window,
              "payload_bytes": 128, "security": "tls1.3" if args.tls_credentials else "plaintext",
              "persistence": False, "complete": False,
              "programs": {label: fingerprint(getattr(args, label)) for label in ("xmqtt", "mosquitto", "observer")},
              "runner": fingerprint(Path(__file__)),
              "observer_source": fingerprint(Path(__file__).with_name("bench_server.c")),
              "workloads": []}
    if args.tls_credentials:
        report["tls"] = {"cipher": TLS_CIPHER, "observer_group": "X25519", "dns_name": "localhost",
                         "trust_anchor": fingerprint(args.tls_credentials / "ca.pem"),
                         "leaf": fingerprint(args.tls_credentials / "certificate.der")}
    if args.idle_peers:
        report["idle_peers"] = args.idle_peers
        report["idle_peer_fixture"] = fingerprint(Path(__file__).resolve().parents[1] /
                                                   "tests/interop/run_server_handshake.py")
    args.report = args.report.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    summaries = ("ingress_per_sec", "delivery_p99_us", "delivery_p999_us", "offer_lag_p99_us", "pacer_lag_p99_us")
    if args.qos:
        summaries += PUBLISHER_METRICS
    try:
        for workload in args.workload:
            group = {"filter": workload[0], "subscribers": workload[1], "scheduled_rate": workload[2], "samples": []}
            if workload[0] == "shared":
                group["shared_groups"] = SHARED_GROUPS
            report["workloads"].append(group)
            for repeat in range(args.repeats):
                order = ("xmqtt", "mosquitto") if repeat % 2 == 0 else ("mosquitto", "xmqtt")
                for label in order:
                    row = {"repeat": repeat, "broker": label}
                    group["samples"].append(row)
                    with tempfile.TemporaryDirectory(prefix="xmqtt-broker-compare-") as temp:
                        directory = Path(temp)
                        try:
                            row.update(run_sample(args, label, workload, directory))
                        except ObserverFailure as error:
                            row["failure"] = str(error)
                            if error.progress is not None:
                                row["failure_progress"] = error.progress
                        except Exception as error:
                            row["failure"] = str(error)
                        log = directory / "broker.log"
                        if "failure" in row and log.is_file():
                            with log.open("rb") as output:
                                size = output.seek(0, os.SEEK_END)
                                output.seek(max(0, size - BROKER_LOG_TAIL_BYTES))
                                row["broker_log_tail"] = output.read(BROKER_LOG_TAIL_BYTES).decode("utf-8", errors="replace")
                    print(workload, repeat, label, row.get("failure", row.get("delivery_p99_us")), flush=True)
            if not any("failure" in row for row in group["samples"]):
                group_metrics = summaries
                if all("observer_cpu_seconds" in row for row in group["samples"]):
                    group_metrics += OBSERVER_CPU_METRICS
                group["summary"] = {
                    label: {metric: noise([row[metric] for row in group["samples"] if row["broker"] == label])
                            for metric in group_metrics}
                    for label in ("xmqtt", "mosquitto")
                }
        report["complete"] = all("summary" in group for group in report["workloads"])
    finally:
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
