"""Compare MQTT 5 offline replay after clean broker checkpoints and restarts."""
import argparse
from contextlib import ExitStack, contextmanager
import ctypes
import json
import os
from pathlib import Path
import platform
import re
import socket
import subprocess
import tempfile
import time

from compare_broker import MOSQUITTO, affinity, fingerprint, noise, pin, start


TOPIC = b"restart/load"
PING, PONG = b"\xc0\x00", (0xD0, b"")
SESSION_EXPIRY = 600


def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


def number(value):
    return value.to_bytes(2, "big")


def field(value):
    return number(len(value)) + value


def packet(tag, body):
    size = len(body)
    return bytes((tag, size)) + body if size < 128 else bytes((tag, (size & 127) | 128, size >> 7)) + body


def read(sock, length):
    data = bytearray()
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        require(chunk, "truncated MQTT response")
        data.extend(chunk)
    return bytes(data)


def receive(sock):
    tag = read(sock, 1)[0]
    length = 0
    for shift in range(0, 28, 7):
        digit = read(sock, 1)[0]
        length |= (digit & 127) << shift
        if digit < 128:
            require(length <= 16384, "unexpected MQTT packet size")
            return tag, read(sock, length)
    raise RuntimeError("invalid MQTT Remaining Length")


def connect(sock, identity, persistent=False, resume=False):
    properties = b"\x11" + SESSION_EXPIRY.to_bytes(4, "big") + b"\x21\x00\x01" if persistent else b""
    header = b"\x00\x04MQTT" + bytes((5, 0 if persistent else 2, 0, 0))
    sock.sendall(packet(0x10, header + bytes((len(properties),)) + properties + field(identity)))
    tag, ack = receive(sock)
    require(tag == 0x20 and len(ack) >= 3 and ack[:2] == bytes((resume, 0)), f"unexpected CONNACK {ack.hex()}")


@contextmanager
def peer(address):
    with socket.create_connection(address, timeout=5) as sock:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        yield sock


def ping(sock):
    sock.sendall(PING)
    require(receive(sock) == PONG, "unexpected MQTT packet before PINGRESP")


def park(sock):
    sock.sendall(b"\xe0\x00")
    require(sock.recv(1) == b"", "broker did not close the parked session")


def ack(sock, expected, identifier, reasons=(0,)):
    tag, fields = receive(sock)
    require(tag == expected and len(fields) in (2, 3, 4) and fields[:2] == identifier
            and (len(fields) == 2 or fields[2] in reasons)
            and (len(fields) != 4 or fields[3] == 0),
            f"unexpected MQTT acknowledgement {tag:02x} {fields.hex()}")


def payload(sequence):
    return bytes(range(124)) + sequence.to_bytes(4, "big")


def enqueue(address, count, qos, first=0):
    latency = []
    with peer(address) as sock:
        connect(sock, b"publisher")
        for index in range(count):
            identifier = number(index + 1)
            wire = packet(0x30 | (qos << 1), field(TOPIC) + identifier + b"\0" + payload(first + index))
            began = time.perf_counter_ns()
            sock.sendall(wire)
            ack(sock, 0x40 if qos == 1 else 0x50, identifier, (0, 0x10))
            if qos == 2:
                sock.sendall(packet(0x62, identifier))
                ack(sock, 0x70, identifier)
            latency.append((time.perf_counter_ns() - began) / 1000)
        ping(sock)
        park(sock)
    return latency


def delivery(sock, identity, sequence, qos):
    tag, body = receive(sock)
    received = time.perf_counter_ns()
    topic = field(TOPIC)
    identifier = body[len(topic):len(topic) + 2]
    require(tag == (0x30 | (qos << 1)) and body.startswith(topic) and len(identifier) == 2
            and identifier != b"\0\0" and body[len(topic) + 2:] == b"\0" + payload(sequence),
            f"invalid restored delivery for {identity!r}, sequence {sequence}")
    sock.sendall(packet(0x40 if qos == 1 else 0x50, identifier))
    if qos == 2:
        ack(sock, 0x62, identifier)
        sock.sendall(packet(0x70, identifier))
    return received


def replay(address, identities, count, qos):
    first_delivery = []
    began = time.perf_counter_ns()
    for identity in identities:
        with peer(address) as sock:
            resumed = time.perf_counter_ns()
            connect(sock, identity, persistent=True, resume=True)
            for sequence in range(count):
                received = delivery(sock, identity, sequence, qos)
                if sequence == 0:
                    first_delivery.append((received - resumed) / 1000)
            ping(sock)
            park(sock)
    return (time.perf_counter_ns() - began) / 1000, first_delivery


def request_checkpoint(process, implementation):
    if implementation == "xmqtt":
        process.stdin.write(b"\n")
        process.stdin.close()
    elif os.name != "nt":
        process.terminate()  # Mosquitto handles SIGTERM and saves before exit.
    else:
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenEventW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
        kernel.OpenEventW.restype = wintypes.HANDLE
        kernel.SetEvent.argtypes = kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        event = kernel.OpenEventW(0x0002, False, f"mosq{process.pid}_shutdown")
        if not event:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            if not kernel.SetEvent(event):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            kernel.CloseHandle(event)
    require(process.wait(timeout=10) == 0, f"broker checkpoint exit {process.returncode}")


@contextmanager
def broker(args, implementation, folder, image, action, run, facts):
    log = folder / f"phase-{run}.log"
    facts["mode"] = action
    port = None
    if implementation == "xmqtt":
        command = [args.xmqtt, "--compare", action, image]
    else:
        config = folder / "mosquitto.conf"
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        config.write_text(
            f"listener {port} 127.0.0.1\n"
            "allow_anonymous true\npersistence true\nautosave_interval 0\n"
            "persistence_file mosquitto.db\nset_tcp_nodelay true\n"
            "max_inflight_messages 64\nmax_queued_messages 255\nmax_packet_size 16384\n"
            "sys_interval 0\nlog_dest stderr\nlog_type error\nlog_type notice\n", encoding="ascii",
        )
        command = [args.mosquitto, "-c", config]
    with log.open("wb") as output:
        began = time.perf_counter_ns()
        process = start(command, {args.broker_cpu}, cwd=folder, stdin=subprocess.PIPE, stdout=output, stderr=output)
        try:
            deadline = time.monotonic() + 10
            while True:
                require(process.poll() is None and time.monotonic() < deadline, "broker did not become ready")
                if implementation == "xmqtt":
                    match = re.search(r"^127\.0\.0\.1:(\d+)$", log.read_text(errors="replace"), re.M)
                    if match:
                        port = int(match[1])
                if port is not None:
                    address = ("127.0.0.1", port)
                    try:
                        with socket.create_connection(address, timeout=0.05):
                            break
                    except (ConnectionError, TimeoutError):
                        pass
                time.sleep(0.001)
            facts["ready_us"] = (time.perf_counter_ns() - began) / 1000
            yield address
            began = time.perf_counter_ns()
            request_checkpoint(process, implementation)
            facts["checkpoint_exit_us"] = (time.perf_counter_ns() - began) / 1000
            facts["image_bytes"] = image.stat().st_size
            require(facts["image_bytes"] > 0, "empty checkpoint image")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
            process.stdin.close()
            facts["log"] = log.read_text(errors="replace")


def percentile99(samples):
    return sorted(samples)[len(samples) - len(samples) // 100 - 1]


def sample(args, implementation, work, folder, row):
    peers, count, qos = work
    identities = [f"offline-{i}".encode("ascii") for i in range(peers)]
    image = folder / ("checkpoint.bin" if implementation == "xmqtt" else "mosquitto.db")
    phases = [{}, {}, {}]
    row["phases"] = phases
    with broker(args, implementation, folder, image, "capture", 0, phases[0]) as address:
        for identity in identities:
            with peer(address) as sock:
                connect(sock, identity, persistent=True)
                sock.sendall(packet(0x82, b"\x00\x01\x00" + field(TOPIC) + bytes((qos,))))
                require(receive(sock) == (0x90, b"\x00\x01\x00" + bytes((qos,))), "subscription failed")
                park(sock)
        row["publisher_latency_us"] = enqueue(address, count, qos)
    row["publisher_p99_us"] = percentile99(row["publisher_latency_us"])
    with broker(args, implementation, folder, image, "restore", 1, phases[1]) as address:
        elapsed, first = replay(address, identities, count, qos)
        row.update(replayed=peers * count, replay_us=elapsed, first_delivery_us=first,
                   first_delivery_p99_us=percentile99(first), replay_per_sec=1_000_000 * peers * count / elapsed)
    with broker(args, implementation, folder, image, "restore", 2, phases[2]) as address:
        with ExitStack() as stack:
            sockets = []
            for identity in identities:
                sock = stack.enter_context(peer(address))
                connect(sock, identity, persistent=True, resume=True)
                ping(sock)
                sockets.append(sock)
            # Live traffic on the restored subscriptions must follow any old
            # queued frames from the same publisher on this ordered topic.
            enqueue(address, 1, qos, first=count)
            for identity, sock in zip(identities, sockets, strict=True):
                delivery(sock, identity, count, qos)
                ping(sock)
                park(sock)
    row["verified_empty_sessions"] = peers
    row["first_checkpoint_us"] = phases[0]["checkpoint_exit_us"]
    row["restore_ready_us"] = phases[1]["ready_us"]


def workload(value):
    try:
        peers, count, qos = map(int, value.split(":"))
        if 1 <= peers <= 32 and 1 <= count <= 200 and qos in (1, 2):
            return peers, count, qos
    except ValueError:
        pass
    raise argparse.ArgumentTypeError("expected subscribers:publications:qos (1..32:1..200:1|2)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("xmqtt", "mosquitto", "report"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--broker-cpu", required=True, type=int)
    parser.add_argument("--load-cpu", required=True, type=int)
    parser.add_argument("--workload", required=True, action="append", type=workload)
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()
    original = affinity(os.getpid())
    if args.broker_cpu == args.load_cpu or not {args.broker_cpu, args.load_cpu} <= original:
        parser.error("broker and observer need disjoint available logical CPUs")
    if not 3 <= args.repeats <= 31:
        parser.error("repeats must be 3..31")
    args.xmqtt = args.xmqtt.resolve(strict=True)
    args.mosquitto = args.mosquitto.resolve(strict=True)
    version = subprocess.check_output([args.mosquitto, "-h"], stderr=subprocess.DEVNULL, text=True, timeout=8).splitlines()[0]
    if version != MOSQUITTO:
        parser.error(f"expected {MOSQUITTO!r}, obtained {version!r}")
    pin(os.getpid(), {args.load_cpu})
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = {"platform": platform.platform(), "broker_cpu": args.broker_cpu, "load_cpu": args.load_cpu,
              "mqtt": 5, "security": "plaintext", "mosquitto_version": version, "payload_bytes": 128,
              "persistence": "checkpoint on graceful exit", "receive_maximum": 1,
              "subscriber_clean_start": False, "expiry_seconds": SESSION_EXPIRY,
              "programs": {name: fingerprint(getattr(args, name)) for name in ("xmqtt", "mosquitto")},
              "runner": fingerprint(Path(__file__)), "repeats": args.repeats, "workloads": [], "complete": False}
    metrics = ("publisher_p99_us", "first_checkpoint_us", "restore_ready_us", "first_delivery_p99_us", "replay_per_sec")
    try:
        for work in args.workload:
            group = dict(zip(("peers", "publications", "qos"), work), samples=[])
            report["workloads"].append(group)
            for repeat in range(args.repeats):
                order = ("xmqtt", "mosquitto") if repeat % 2 == 0 else ("mosquitto", "xmqtt")
                for label in order:
                    row = {"repeat": repeat, "broker": label}
                    group["samples"].append(row)
                    with tempfile.TemporaryDirectory(prefix="xmqtt-restart-") as path:
                        try:
                            sample(args, label, work, Path(path), row)
                        except Exception as error:
                            row["failure"] = str(error)
                    print(work, repeat, label, row.get("failure", row.get("replayed")), flush=True)
            if not any("failure" in row for row in group["samples"]):
                group["summary"] = {
                    label: {metric: noise([row[metric] for row in group["samples"] if row["broker"] == label])
                            for metric in metrics}
                    for label in ("xmqtt", "mosquitto")
                }
        report["complete"] = all("summary" in group for group in report["workloads"])
    finally:
        pin(os.getpid(), original)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
