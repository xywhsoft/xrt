"""Compare MQTT 5 receivers against the same paced, independent wire peer."""
import argparse
import json
import math
import os
from pathlib import Path
import platform
import select
import socket
import subprocess
import sys
import time

from compare_broker import affinity, fingerprint, noise, pin, start, stop

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
WIRE = HERE.parent / "tests" / "interop"
sys.path.insert(0, str(WIRE))
from run_client_wire import packet, read_packet, string, take_string

TOPIC = b"bench/client/receive-reference"
IDENTITY = b"bench-client-receive-reference"
WARMUP = 128
SECOND = 1_000_000_000
DEADLINE = 5 * SECOND
METRICS = ("application_p99_us", "terminal_p99_us", "from_send_p99_us",
           "offer_lag_p99_us", "verified_per_sec")


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def handshake(sock, window, qos):
    kind, body = read_packet(sock)
    protocol, offset = take_string(body, 0)
    maximum = b"\x21" + window.to_bytes(2, "big")
    fixed = b"\x05\x02\x00\x3c" + bytes([len(maximum)]) + maximum
    require(kind == 0x10 and protocol == b"MQTT" and body[offset:] == fixed + string(IDENTITY),
            f"MQTT 5 CONNECT or Receive Maximum mismatch ({body.hex()})")
    sock.sendall(packet(0x20, b"\x00\x00\x00"))
    kind, body = read_packet(sock)
    require(kind == 0x82 and len(body) >= 3 and int.from_bytes(body[:2], "big") and
            body[2:] == b"\x00" + string(TOPIC) + bytes([qos]), "subscription mismatch")
    sock.sendall(packet(0x90, body[:2] + b"\x00" + bytes([qos])))


def publication(sequence, due, qos):
    payload = (sequence.to_bytes(8, "big") + due.to_bytes(8, "big") +
               bytes((sequence * 17 + index * 131) & 255 for index in range(16, 32)))
    ident = (sequence + 1).to_bytes(2, "big") if qos else b""
    return packet(0x30 | (qos << 1), string(TOPIC) + ident + b"\x00" + payload)


def percentiles(values, prefix):
    ordered = sorted(values)
    ranks = ((50, 500), (95, 950), (99, 990), (999, 999))
    return {f"{prefix}_p{label}_us": ordered[(len(ordered) * rank + 999) // 1000 - 1] / 1000
            for label, rank in ranks}


def transfer(sock, qos, window, rate, count):
    total = count + WARMUP
    cpu_started = time.process_time_ns()
    start_ns = time.perf_counter_ns() + 20_000_000
    begin = start_ns + WARMUP * SECOND // rate
    end = start_ns + total * SECOND // rate + DEADLINE
    pending = {}
    terminal, offers, from_send = [], [], []
    sent = completed = peak = 0
    while completed < total:
        now = time.perf_counter_ns()
        require(now < end, "peer completion deadline")
        due = start_ns + sent * SECOND // rate
        can_send = sent < total and len(pending) < window
        # Keep sub-millisecond offers independent of socket wait rounding.
        # The fixture has its own logical CPU for this short spin window.
        timeout_ns = max(0, due - now - 2_000_000) if can_send else end - now
        readable, _, _ = select.select([sock], [], [], min(timeout_ns / SECOND, 0.05))
        if readable:
            kind, ack = read_packet(sock)
            observed = time.perf_counter_ns()
            ident = int.from_bytes(ack[:2], "big")
            require(len(ack) >= 2 and ack[2:] in (b"", b"\x00", b"\x00\x00") and
                    ident in pending and pending[ident][2] == kind, "unexpected acknowledgement or phase")
            scheduled, actual, _ = pending[ident]
            require(0 <= observed - scheduled < DEADLINE, "per-publication terminal deadline")
            if kind == 0x50:
                pending[ident] = scheduled, actual, 0x70
                sock.sendall(packet(0x62, ack[:2]))
            else:
                del pending[ident]
                completed += 1
                if ident > WARMUP:
                    terminal.append(observed - scheduled)
                    from_send.append(observed - actual)
        elif can_send and time.perf_counter_ns() >= due:
            wire = publication(sent, due, qos)
            actual = time.perf_counter_ns()
            require(0 <= actual - due < DEADLINE, "per-publication send deadline")
            sock.sendall(wire)
            pending[sent + 1] = due, actual, 0x40 if qos == 1 else 0x50
            if sent >= WARMUP:
                offers.append(actual - due)
            sent += 1
            peak = max(peak, len(pending))
    require(sent == total and not pending and len(terminal) == len(offers) == count, "wire completion ledger")
    elapsed = time.perf_counter_ns() - begin
    peer_cpu = (time.process_time_ns() - cpu_started) / SECOND
    require(elapsed > 0, "measurement interval")
    # Release the application only after every reliable exchange completed.
    sock.sendall(publication(total, time.perf_counter_ns(), 0))
    kind, body = read_packet(sock)
    require(kind == 0xe0 and body in (b"", b"\x00", b"\x00\x00"), "DISCONNECT after final barrier")
    require(sock.recv(1) == b"", "extra bytes after DISCONNECT")
    return {"wire_completed": count, "peak_inflight": peak, "verified_per_sec": count * SECOND / elapsed,
            "peer_cpu_seconds": peer_cpu,
            **percentiles(terminal, "terminal"), **percentiles(offers, "offer_lag"),
            **percentiles(from_send, "from_send")}


def sample(executable, name, args, *, xmqtt_default_tcp=False):
    row = {"client": name, "verified": False}
    process = None
    try:
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            listener.settimeout(5)
            command = [str(executable), *map(str, (listener.getsockname()[1], args.count, args.window, args.qos))]
            if name == "xmqtt" and xmqtt_default_tcp:
                command.append("--default-tcp")
            process = start(command, args.client_cpus, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            connection, _ = listener.accept()
            with connection:
                connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                connection.settimeout(5)
                handshake(connection, args.window, args.qos)
                wire = transfer(connection, args.qos, args.window, args.rate, args.count)
        output, errors = process.communicate(timeout=7)
        row.update(returncode=process.returncode, stdout=output, stderr=errors, wire=wire)
        require(process.returncode == 0, "receiver exit")
        metrics = json.loads(output)
        expected = {"client": name, "qos": args.qos, "window": args.window, "measured": args.count,
                    "warmup": WARMUP, "retired": True}
        require(all(metrics[key] == value for key, value in expected.items()), "application ledger")
        tails = [metrics[f"application_p{rank}_us"] for rank in (50, 95, 99, 999)]
        require(tails == sorted(tails) and all(math.isfinite(value) and value >= 0
                for value in [*tails, metrics["cpu_seconds"]]), "application measurement")
        require(0 < wire["peak_inflight"] <= args.window, "wire window")
        row.update(verified=True, metrics=metrics | wire)
    except (OSError, EOFError, ValueError, AssertionError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        row["error"] = str(error)
    finally:
        if process is not None:
            stop(process)
            if "stdout" not in row:
                output, errors = process.communicate(timeout=3)
                row.update(returncode=process.returncode, stdout=output, stderr=errors)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("xmqtt", "paho", "report"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--peer-cpu", type=int, required=True)
    parser.add_argument("--client-cpus", type=lambda text: set(map(int, text.split(","))), required=True)
    parser.add_argument("--qos", type=int, choices=(1, 2), required=True)
    parser.add_argument("--window", type=int, choices=range(1, 65), required=True)
    parser.add_argument("--rate", type=int, required=True)
    parser.add_argument("--count", type=int, required=True)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--xmqtt-default-tcp", action="store_true",
                        help="preserve xmqtt's default TCP settings for a deployment comparison")
    args = parser.parse_args()
    if not (1 <= args.rate <= 100000 and 1 <= args.count <= 60000 and
            args.count + WARMUP <= 15 * args.rate and 1 <= args.repeats <= 20):
        parser.error("invalid count, schedule or repeat limit")
    cpus = args.client_cpus | {args.peer_cpu}
    if len(args.client_cpus) != 2 or len(cpus) != 3 or not cpus <= affinity(os.getpid()):
        parser.error("select one peer CPU and two disjoint available client CPUs")
    clients = {name: getattr(args, name).resolve(strict=True) for name in ("xmqtt", "paho")}
    require(len(set(clients.values())) == 2, "distinct client binaries required")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    pin(os.getpid(), {args.peer_cpu})
    require(affinity(os.getpid()) == {args.peer_cpu}, "peer affinity")
    report = {"platform": platform.platform(), "mqtt": 5, "peer_cpu": args.peer_cpu,
              "client_cpus": sorted(args.client_cpus), "reference": "Paho C 1.3.15",
              "xmqtt_default_tcp": args.xmqtt_default_tcp,
              "peer_clock": time.get_clock_info("perf_counter").implementation,
              "qos": args.qos, "window": args.window, "rate": args.rate, "count": args.count,
              "warmup": WARMUP, "payload_bytes": 32, "runs": [], "verified": False,
              "binaries": {name: fingerprint(path) for name, path in clients.items()},
              "inputs": [fingerprint(path) for path in (Path(__file__), HERE / "bench_client_receive_reference.c",
                         HERE / "compare_broker.py", WIRE / "run_client_wire.py",
                         ROOT / "dev/bench/bench_common.h", ROOT / "single/xrt.h",
                         ROOT / "single/extlibs/xmqtt.h")]}
    for repeat in range(args.repeats + 1):
        for name in clients if repeat % 2 == 0 else reversed(clients):
            run = sample(clients[name], name, args, xmqtt_default_tcp=args.xmqtt_default_tcp)
            report["runs"].append(run | {"repeat": repeat, "warmup": repeat == 0})
            print(f"{name} repeat={repeat} verified={run['verified']} {run.get('error', '')}", flush=True)
        args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    report["verified"] = all(run["verified"] for run in report["runs"])
    if report["verified"] and args.repeats >= 5:
        report["variation"] = {name: {metric: noise([run["metrics"][metric] for run in report["runs"]
                                  if run["client"] == name and not run["warmup"]])
                                      for metric in METRICS} for name in clients}
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return 0 if report["verified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
