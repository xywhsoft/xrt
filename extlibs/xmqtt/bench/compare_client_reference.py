"""Compare MQTT 5 publishers using an independent binary delivery subscriber."""
from __future__ import annotations

import argparse
from contextlib import suppress
import json
import math
import os
from pathlib import Path
import platform
import socket
import subprocess
import sys
import tempfile
import threading
import time

from compare_broker import MOSQUITTO, affinity, fingerprint, noise, start, stop

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
WIRE = HERE.parent / "tests" / "interop"
sys.path.insert(0, str(WIRE))
from run_client_wire import packet, read_packet, string, take_string

TOPIC = b"bench/client/reference"
WARMUP = 128
QUALITY = ("publishes_per_sec", "p50_us", "p95_us", "p99_us", "p999_us")


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def subscribe(connection):
    body = b"\x00\x04MQTT\x05\x02\x00\x3c\x00" + string(b"bench-client-reference-observer")
    connection.sendall(packet(0x10, body))
    kind, body = read_packet(connection)
    require(kind == 0x20 and len(body) >= 3 and body[:2] == b"\x00\x00", "observer CONNACK")
    connection.sendall(packet(0x82, b"\x00\x01\x00" + string(TOPIC) + b"\x00"))
    kind, body = read_packet(connection)
    require(kind == 0x90 and body == b"\x00\x01\x00\x00", "observer QoS 0 SUBACK")


def deliveries(connection, count):
    seen = bytearray(count)
    for _ in range(count):
        kind, body = read_packet(connection)
        topic, offset = take_string(body, 0)
        # The dedicated Mosquitto listener sends no properties or topic alias.
        require(kind == 0x30 and topic == TOPIC and len(body) == offset + 33 and body[offset] == 0,
                "observer PUBLISH envelope")
        payload = body[offset + 1:]
        sequence = int.from_bytes(payload[:8], "big")
        require(sequence < count and not seen[sequence], "unexpected or duplicate publication sequence")
        require(payload[8:] == bytes((sequence * 17 + index * 131) & 255 for index in range(8, 32)),
                "binary publication mismatch")
        seen[sequence] = 1
    return sum(seen)


def sample(executable, name, port, cpus, request):
    qos, window, rate, count = request
    args = [str(executable), *map(str, (port, count, rate, window, qos))]
    observed = {}

    def accept(connection):
        try:
            observed["count"] = deliveries(connection, count + WARMUP)
        except (OSError, EOFError, AssertionError, RuntimeError, ValueError) as error:
            observed["error"] = str(error)

    with socket.create_connection(("127.0.0.1", port), timeout=6) as connection:
        subscribe(connection)
        peer = threading.Thread(target=accept, args=(connection,))
        record = {"client": name, "observer": observed}
        peer.start()
        client = None
        try:
            client = start(args, cpus, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8")
            output, errors = client.communicate(timeout=26)
            peer.join(timeout=7)
            record.update(returncode=client.returncode, stdout=output, stderr=errors)
            require(not peer.is_alive(), "observer retirement deadline")
            require(client.returncode == 0 and observed.get("count") == count + WARMUP, "publisher or observer failed")
            connection.settimeout(0.1)
            try:
                extra = connection.recv(1)
            except socket.timeout:
                pass
            else:
                raise RuntimeError(f"unexpected observer data or disconnect after deliveries ({len(extra)} bytes)")
            connection.sendall(packet(0xe0, b""))
            result = json.loads(output)
            expected = {"client": name, "qos": qos, "window": window, "rate": rate, "count": count, "warmup": WARMUP,
                        "version": 5, "bytes": 32, "accepted": count, "completed": count, "teardown": True}
            require(all(result[key] == value for key, value in expected.items()), "report ledger")
            require(all(math.isfinite(result[key]) and result[key] >= 0 for key in
                        QUALITY + ("cpu_seconds", "seconds", "offer_lag_p99_us")), "report measurement")
            return record | {"verified": True, "metrics": result}
        except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
            return record | {"verified": False, "error": str(error)}
        finally:
            if client is not None:
                stop(client)
            if peer.is_alive():
                with suppress(OSError):
                    connection.shutdown(socket.SHUT_RDWR)
                peer.join(timeout=7)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--broker", type=Path, required=True)
    parser.add_argument("--xmqtt", type=Path, required=True)
    parser.add_argument("--paho", type=Path, required=True)
    parser.add_argument("--broker-cpu", type=int, required=True)
    parser.add_argument("--client-cpus", type=lambda text: set(map(int, text.split(","))), required=True)
    parser.add_argument("--qos", type=int, choices=(1, 2), required=True)
    parser.add_argument("--window", type=int, choices=range(1, 65), required=True)
    parser.add_argument("--rate", type=int, required=True)
    parser.add_argument("--count", type=int, required=True)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not (1 <= args.rate <= 100000 and 1 <= args.count <= min(15 * args.rate, 100000) and 1 <= args.repeats <= 20):
        parser.error("invalid offer count, rate or repeats")
    if len(args.client_cpus) != 2 or args.broker_cpu in args.client_cpus or not (args.client_cpus | {args.broker_cpu}) <= affinity(os.getpid()):
        parser.error("select one broker CPU and two other available client CPUs")
    paths = {key: getattr(args, key).resolve(strict=True) for key in ("xmqtt", "paho", "broker")}
    require(len(set(paths.values())) == 3, "distinct broker and client binaries required")
    version = subprocess.run([str(paths["broker"]), "-h"], capture_output=True, text=True, timeout=10).stdout.splitlines()
    require(MOSQUITTO in version, "pinned broker version")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "platform": platform.platform(), "broker_version": MOSQUITTO, "broker_cpu": args.broker_cpu,
        "client_cpus": sorted(args.client_cpus), "binaries": {name: fingerprint(path) for name, path in paths.items()},
        "inputs": [fingerprint(path) for path in (Path(__file__), HERE / "bench_client_reference.c",
                                                 HERE / "compare_broker.py", WIRE / "run_client_wire.py",
                                                 ROOT / "dev/bench/bench_common.h", ROOT / "single/xrt.h",
                                                 ROOT / "single/extlibs/xmqtt.h")],
        "runs": [],
    }
    with tempfile.TemporaryDirectory(prefix="xmqtt-client-reference-", dir=args.report.parent) as directory:
        with socket.socket() as address:
            address.bind(("127.0.0.1", 0))
            port = address.getsockname()[1]
        config = Path(directory) / "broker.conf"
        text = (f"listener {port} 127.0.0.1\nallow_anonymous true\npersistence false\n"
                "max_inflight_messages 64\nmax_queued_messages 256\nmax_packet_size 16384\n"
                "set_tcp_nodelay true\nsys_interval 0\nlog_type error\nlog_type notice\n")
        config.write_text(text, encoding="ascii")
        report["broker_config"] = text
        with (Path(directory) / "broker.log").open("wb+") as log:
            broker = start([str(paths["broker"]), "-c", str(config)], {args.broker_cpu}, stdout=log, stderr=log)
            try:
                limit = time.monotonic() + 8
                while True:
                    require(broker.poll() is None and time.monotonic() < limit, "broker readiness deadline")
                    try:
                        with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                            break
                    except OSError:
                        time.sleep(0.025)
                for repeat in range(args.repeats + 1):
                    order = ("xmqtt", "paho") if repeat % 2 == 0 else ("paho", "xmqtt")
                    for name in order:
                        run = sample(paths[name], name, port, args.client_cpus, (args.qos, args.window, args.rate, args.count))
                        report["runs"].append(run | {"repeat": repeat, "warmup": repeat == 0})
            finally:
                stop(broker)
                log.seek(0)
                report["broker_log"] = log.read().decode("utf-8", "replace")
    report["verified"] = all(run["verified"] for run in report["runs"])
    if report["verified"]:
        report["variation"] = {
            name: {metric: noise([run["metrics"][metric] for run in report["runs"]
                                  if run["client"] == name and not run["warmup"]])
                   for metric in QUALITY}
            for name in ("xmqtt", "paho")
        } if args.repeats >= 5 else None
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"MQTT client QoS {args.qos} window {args.window} verified={report['verified']}", flush=True)
    return 0 if report["verified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
