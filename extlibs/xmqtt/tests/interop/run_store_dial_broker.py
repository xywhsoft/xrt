"""Verify the durable TCP example with an independent MQTT subscriber and broker."""
import argparse
import json
from pathlib import Path
import socket
import statistics
import subprocess
import time

from run_reconnect_store_broker import LOOPBACK, connect_subscriber, field, readiness, send
from run_session_broker import digest, packet


TOPIC = b"xrt/example/durable"
PAYLOAD = b"committed QoS 1 exchange"
ENDPOINTS = (("numeric", LOOPBACK), ("hostname", "localhost"))


def receive(sock, header, body):
    assert packet(sock) == (bytes((header, len(body))) + body, body)


def subscribe(sock):
    sock.settimeout(10)
    send(sock, 0x10, b"\0\x04MQTT\x04\x02\0\0" + field(b"durable-tcp-dial-verifier"))
    receive(sock, 0x20, b"\0\0")
    send(sock, 0x82, b"\0\1" + field(TOPIC) + b"\0")
    receive(sock, 0x90, b"\0\1\0")


def exchange(exe, endpoint, port, version, subscriber, transcript):
    begin = time.perf_counter_ns()
    completed = subprocess.run([str(exe), endpoint, str(port), version],
                               capture_output=True, text=True, timeout=30)
    elapsed = (time.perf_counter_ns() - begin) / 1e6
    transcript.write_text(completed.stdout + completed.stderr, encoding="utf-8")
    assert completed.returncode == 0, (transcript, completed.returncode)
    assert f"MQTT {version} committed PUBLISH at 2 and PUBACK at 3." in completed.stdout
    assert "Committed graceful END at 4." in completed.stdout
    expected = field(TOPIC) + PAYLOAD
    receive(subscriber, 0x30, expected)
    return {"elapsed_ms": elapsed, "transcript": transcript.name,
            "transcript_sha256": digest(transcript)}


def summarize(samples):
    ordered = sorted(row["elapsed_ms"] for row in samples)
    return {"median_ms": statistics.median(ordered),
            "p95_ms": ordered[(95 * len(ordered) + 99) // 100 - 1],
            "max_ms": ordered[-1], "samples": samples}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--broker", required=True, type=Path)
    parser.add_argument("--client", nargs=2, action="append", required=True, metavar=("NAME", "EXE"))
    parser.add_argument("--windows-python", type=Path)
    parser.add_argument("--runs", type=int, default=8)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.runs <= 100:
        parser.error("--runs must be within 1..100")
    out = args.report.parent / (args.report.stem + "-artifacts")
    out.mkdir(parents=True, exist_ok=True)
    with socket.socket() as reservation:
        reservation.bind((LOOPBACK, 0))
        port = reservation.getsockname()[1]
    config = (out / "mosquitto.conf").resolve()
    config.write_text(f"listener {port} {LOOPBACK}\nallow_anonymous true\npersistence false\nlog_type all\n",
                      encoding="ascii")
    broker_log = out / "broker.log"
    records = {}
    with broker_log.open("wb") as transcript:
        broker = subprocess.Popen([str(args.broker.resolve()), "-c", str(config)],
                                  stdout=transcript, stderr=subprocess.STDOUT)
        try:
            with connect_subscriber(broker, port) as subscriber:
                readiness(args.windows_python, port)
                subscribe(subscriber)
                for name, path in args.client:
                    assert name.isascii() and name.replace("-", "").isalnum() and name not in records
                    exe = Path(path).resolve()
                    entry = {"binary": str(exe), "sha256": digest(exe), "protocols": {}}
                    records[name] = entry
                    for version in ("5", "311"):
                        samples = {"numeric": [], "hostname": []}
                        for index in range(args.runs):
                            order = ENDPOINTS if index % 2 == 0 else ENDPOINTS[::-1]
                            for mode, endpoint in order:
                                trace = out / f"{name}-{version}-{mode}-{index + 1}.log"
                                samples[mode].append(exchange(exe, endpoint, port, version, subscriber, trace))
                        entry["protocols"][version] = {
                            mode: summarize(rows) for mode, rows in samples.items()
                        }
                        print(f"[PASS] {name} MQTT {version} {args.runs} matched pairs", flush=True)
        finally:
            if broker.poll() is None:
                broker.terminate()
                try:
                    broker.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    broker.kill()
                    broker.wait(timeout=5)
    total = len(records) * args.runs * 4
    broker_text = broker_log.read_text(encoding="utf-8")
    assert broker_text.count("Received PUBLISH from xrt-durable-demo") == total
    assert broker_text.count("Received DISCONNECT from xrt-durable-demo") == total
    report = {"schema": 1, "broker_sha256": digest(args.broker), "broker_log_sha256": digest(broker_log),
              "subscriber_deliveries": total, "runs_per_endpoint": args.runs,
              "timing": "Full example process wall time including journal creation, protocol and cleanup",
              "clients": records}
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
