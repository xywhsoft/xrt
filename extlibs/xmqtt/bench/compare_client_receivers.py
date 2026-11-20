"""Compare MQTT 5 receivers against a paced wire peer or pinned standalone broker."""
import argparse
from contextlib import ExitStack, contextmanager, nullcontext
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import select
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import time

from compare_broker import MOSQUITTO, TLS_CIPHER, affinity, fingerprint, noise, pin, start, stop, usage

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
BROKER_MAX_PACKET = 16384
TLS_MAX_PENDING = 128
METRICS = ("application_p99_us", "terminal_p99_us", "from_send_p99_us",
           "offer_lag_p99_us", "verified_per_sec")
TRACE_PUBLISH = re.compile(rb"Sending PUBLISH to " + IDENTITY + rb" \(d0, q([12]), r0, m(\d+), '" +
                           re.escape(TOPIC) + rb"', \.\.\. \(32 bytes\)\)")
TRACE_ACK = re.compile(rb"(Received PUBACK|Received PUBREC|Sending PUBREL|Received PUBCOMP) (?:from|to) " +
                       IDENTITY + rb" \((?:Mid: |m)(\d+)(?:, RC:0)?\)")


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def handshake(sock, window, qos, broker=None):
    kind, body = read_packet(sock)
    protocol, offset = take_string(body, 0)
    maximum = b"\x21" + window.to_bytes(2, "big")
    fixed = b"\x05\x02\x00\x3c" + bytes([len(maximum)]) + maximum
    require(kind == 0x10 and protocol == b"MQTT" and body[offset:] == fixed + string(IDENTITY),
            f"MQTT 5 CONNECT or Receive Maximum mismatch ({body.hex()})")
    if broker is None:
        sock.sendall(packet(0x20, b"\x00\x00\x00"))
    else:
        broker.sendall(packet(kind, body))
        kind, body = read_packet(broker)
        require(kind == 0x20 and len(body) >= 3 and body[:2] == b"\x00\x00", "broker subscriber CONNACK")
        sock.sendall(packet(kind, body))
    kind, body = read_packet(sock)
    require(kind == 0x82 and len(body) >= 3 and int.from_bytes(body[:2], "big") and
            body[2:] == b"\x00" + string(TOPIC) + bytes([qos]), "subscription mismatch")
    granted = body[:2] + b"\x00" + bytes([qos])
    if broker is not None:
        broker.sendall(packet(kind, body))
        require(read_packet(broker) == (0x90, granted), "broker subscriber SUBACK")
    sock.sendall(packet(0x90, granted))


def payload(sequence, due):
    return (sequence.to_bytes(8, "big") + due.to_bytes(8, "big") +
            bytes((sequence * 17 + index * 131) & 255 for index in range(16, 32)))


def publication(sequence, due, qos):
    ident = (sequence + 1).to_bytes(2, "big") if qos else b""
    return packet(0x30 | (qos << 1), string(TOPIC) + ident + b"\x00" + payload(sequence, due))


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


def broker_socket(port):
    sock = socket.create_connection(("127.0.0.1", port), timeout=5)
    try:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except OSError:
        sock.close()
        raise
    return sock


def ack_id(body):
    require(len(body) >= 2 and body[2:] in (b"", b"\x00", b"\x00\x00"), "broker comparison ACK envelope")
    return int.from_bytes(body[:2], "big")


def start_publisher(sock):
    sock.sendall(packet(0x10, b"\x00\x04MQTT\x05\x02\x00\x3c\x00" + string(IDENTITY + b"-pub")))
    kind, body = read_packet(sock)
    require(kind == 0x20 and len(body) >= 3 and body[:2] == b"\x00\x00", "broker publisher CONNACK")


def publisher_ack(sock, pending, kind, body):
    ident = ack_id(body)
    require(pending.get(ident) == kind, "broker publisher ACK phase or ID")
    if kind == 0x50:
        pending[ident] = 0x70
        sock.sendall(packet(0x62, body[:2]))
        return None
    del pending[ident]
    return ident


def receive_packets(sock, pending):
    """Read available bytes without waiting for the rest of a split packet."""
    data = sock.recv(BROKER_MAX_PACKET)
    require(bool(data), "broker comparison stream closed")
    pending.extend(data)
    while pending:
        length = 0
        for offset in range(1, min(5, len(pending))):
            digit = pending[offset]
            length += (digit & 127) << (7 * (offset - 1))
            if not digit & 128:
                break
        else:
            require(len(pending) < 5, "broker comparison packet length encoding")
            return
        end = offset + 1 + length
        require(end <= BROKER_MAX_PACKET, "broker comparison packet limit")
        if len(pending) < end:
            return
        kind, body = pending[0], bytes(pending[offset + 1:end])
        del pending[:end]
        yield kind, body


def broker_transfer(receiver, upstream, publisher, qos, window, rate, count):
    """Relay the subscriber stream, correlating the broker's IDs with fixed offers."""
    total = count + WARMUP
    start_publisher(publisher)
    cpu_started = time.process_time_ns()
    start_ns = time.perf_counter_ns() + 20_000_000
    begin = start_ns + WARMUP * SECOND // rate
    end = start_ns + total * SECOND // rate + DEADLINE
    pending_publish, pending_deliver, sends = {}, {}, {}
    terminal, offers, from_send, relay = [], [], [], []
    sent = delivered = completed = confirmed = peak = 0
    ack = 0x40 if qos == 1 else 0x50
    streams = [receiver, upstream, publisher]
    incoming = {stream: bytearray() for stream in streams}
    while completed < total or pending_publish:
        now = time.perf_counter_ns()
        require(now < end, "broker completion deadline")
        due = start_ns + sent * SECOND // rate
        available = sent < total and len(pending_publish) < window
        timeout = max(0, due - now - 2_000_000) if available else end - now
        readable, _, _ = select.select(streams, [], [], min(timeout / SECOND, 0.05))
        frames = ((source, kind, body) for source in readable for kind, body in receive_packets(source, incoming[source]))
        for source, kind, body in frames:
            observed = time.perf_counter_ns()
            if source is publisher:
                if publisher_ack(publisher, pending_publish, kind, body) is not None:
                    confirmed += 1
            elif source is upstream and kind == (0x30 | (qos << 1)):
                topic, offset = take_string(body, 0)
                ident = int.from_bytes(body[offset:offset + 2], "big")
                scheduled = start_ns + delivered * SECOND // rate
                require(topic == TOPIC and ident and ident not in pending_deliver and delivered in sends and
                        len(pending_deliver) < window and body[offset + 2:] == b"\x00" + payload(delivered, scheduled),
                        "broker forwarded publication or Receive Maximum")
                forwarded = time.perf_counter_ns()
                receiver.sendall(packet(kind, body))
                pending_deliver[ident] = delivered, ack, forwarded
                delivered += 1
                peak = max(peak, len(pending_deliver))
            else:
                ident = ack_id(body)
                require(ident in pending_deliver, "broker delivery ACK ID")
                sequence, phase, forwarded = pending_deliver[ident]
                require(kind == phase and (source is upstream) == (kind == 0x62), "broker delivery ACK phase")
                destination = receiver if source is upstream else upstream
                destination.sendall(packet(kind, body))
                if kind in (0x50, 0x62):
                    pending_deliver[ident] = sequence, 0x62 if kind == 0x50 else 0x70, forwarded
                else:
                    del pending_deliver[ident]
                    actual = sends.pop(sequence)
                    completed += 1
                    scheduled = start_ns + sequence * SECOND // rate
                    require(0 <= observed - scheduled < DEADLINE, "broker per-publication deadline")
                    if sequence >= WARMUP:
                        terminal.append(observed - scheduled)
                        from_send.append(observed - actual)
                        relay.append(observed - forwarded)
        if available and time.perf_counter_ns() >= due:
            wire = publication(sent, due, qos)
            actual = time.perf_counter_ns()
            require(0 <= actual - due < DEADLINE, "broker per-publication send deadline")
            publisher.sendall(wire)
            sends[sent] = actual
            pending_publish[sent + 1] = ack
            if sent >= WARMUP:
                offers.append(actual - due)
            sent += 1
    elapsed = time.perf_counter_ns() - begin
    peer_cpu = (time.process_time_ns() - cpu_started) / SECOND
    require(elapsed > 0 and sent == delivered == completed == confirmed == total and not sends and not pending_deliver
            and not any(incoming.values()) and len(terminal) == len(offers) == count, "broker completion ledgers")
    # A response on the same stream follows the relayed subscriber acknowledgements.
    upstream.sendall(packet(0xc0, b""))
    require(read_packet(upstream) == (0xd0, b""), "broker subscriber ACK drain barrier")
    barrier = publication(total, time.perf_counter_ns(), 0)
    publisher.sendall(barrier)
    kind, body = read_packet(upstream)
    require(packet(kind, body) == barrier, "broker final application barrier")
    receiver.sendall(barrier)
    kind, body = read_packet(receiver)
    require(kind == 0xe0 and body in (b"", b"\x00", b"\x00\x00"), "broker subscriber DISCONNECT")
    upstream.sendall(packet(kind, body))
    publisher.sendall(packet(0xe0, b""))
    require(receiver.recv(1) == b"", "extra subscriber bytes after DISCONNECT")
    return {"wire_completed": count, "publisher_confirmed": count, "broker_ack_drain": True,
            "peak_inflight": peak, "verified_per_sec": count * SECOND / elapsed, "peer_cpu_seconds": peer_cpu,
            **percentiles(terminal, "terminal"), **percentiles(offers, "offer_lag"),
            **percentiles(from_send, "from_send"), **percentiles(relay, "subscriber_leg")}


def trace_lines(trace, process, phase, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        position = trace.tell()
        line = trace.readline()
        if line.endswith(b"\n"):
            yield line.rstrip(b"\r\n")
        else:
            require(process.poll() is None, f"TLS subscriber exit before {phase}")
            trace.seek(position)
            time.sleep(0.001)
    raise RuntimeError(f"TLS subscriber trace deadline at {phase}")


def trace_subscriber(trace, process, qos, window, total, timeout):
    next_ack = b"Received PUBACK" if qos == 1 else b"Received PUBREC"
    pending = {}
    delivered = completed = peak = 0
    for line in trace_lines(trace, process, "terminal acknowledgements", timeout):
        if IDENTITY not in line:
            continue
        require(line != b"Outgoing messages are being dropped for client " + IDENTITY,
                "TLS broker dropped subscriber messages")
        if line.startswith(b"Sending PUBLISH to " + IDENTITY + b" "):
            found = TRACE_PUBLISH.fullmatch(line)
            require(found is not None, "TLS broker PUBLISH trace envelope")
            kind, ident = map(int, found.groups())
            require(kind == qos and ident and ident not in pending and len(pending) < window,
                    "TLS broker Receive Maximum or publish ID")
            pending[ident] = next_ack
            delivered += 1
            peak = max(peak, len(pending))
        elif line.startswith((b"Received PUB", b"Sending PUBREL")) and IDENTITY + b" (" in line:
            found = TRACE_ACK.fullmatch(line)
            require(found is not None, "TLS broker ACK trace envelope")
            step, raw_id = found.groups()
            ident = int(raw_id)
            require(pending.get(ident) == step, "TLS broker subscriber ACK phase or ID")
            if step == b"Received PUBREC":
                pending[ident] = b"Sending PUBREL"
            elif step == b"Sending PUBREL":
                pending[ident] = b"Received PUBCOMP"
            else:
                del pending[ident]
                completed += 1
                if completed == total:
                    require(not pending and delivered == total, "TLS broker subscriber trace ledger")
                yield completed, peak
                if completed == total:
                    return


def tls_transfer(publisher, trace, process, qos, window, rate, count):
    negotiated = b"Client " + IDENTITY + b" negotiated TLSv1.3 cipher " + TLS_CIPHER.encode("ascii")
    secure = False
    for line in trace_lines(trace, process, "SUBACK"):
        if line == negotiated:
            secure = True
        if line == b"Sending SUBACK to " + IDENTITY:
            require(secure, "TLS subscriber negotiated profile")
            break
    start_publisher(publisher)
    total = count + WARMUP
    cpu_started = time.process_time_ns()
    start_ns = time.perf_counter_ns() + 20_000_000
    begin = start_ns + WARMUP * SECOND // rate
    end = start_ns + total * SECOND // rate + DEADLINE
    pending, buffer = {}, bytearray()
    acks, offers = [], []
    sent = confirmed = completed = peak = credit_checks = 0
    ack = 0x40 if qos == 1 else 0x50
    progress = trace_subscriber(trace, process, qos, window, total, total / rate + DEADLINE / SECOND)
    while confirmed < total:
        now = time.perf_counter_ns()
        require(now < end, "TLS comparison publisher deadline")
        due = start_ns + sent * SECOND // rate
        subscriber_credit = sent - completed < TLS_MAX_PENDING
        if not subscriber_credit and not pending:
            credit_checks += 1
            completed, peak = next(progress)
            continue
        available = sent < total and len(pending) < window and subscriber_credit
        timeout = max(0, due - now - 2_000_000) if available else end - now
        readable, _, _ = select.select([publisher], [], [], min(timeout / SECOND, 0.05))
        if readable:
            for kind, body in receive_packets(publisher, buffer):
                ident = publisher_ack(publisher, pending, kind, body)
                if ident is not None:
                    confirmed += 1
                    if ident > WARMUP:
                        acks.append(time.perf_counter_ns() - (start_ns + (ident - 1) * SECOND // rate))
        elif available and time.perf_counter_ns() >= due:
            wire = publication(sent, due, qos)
            actual = time.perf_counter_ns()
            require(0 <= actual - due < DEADLINE, "TLS comparison per-publication send deadline")
            publisher.sendall(wire)
            pending[sent + 1] = ack
            if sent >= WARMUP:
                offers.append(actual - due)
            sent += 1
    elapsed = time.perf_counter_ns() - begin
    peer_cpu = (time.process_time_ns() - cpu_started) / SECOND
    require(elapsed > 0 and sent == confirmed == total and not pending and not buffer and
            len(acks) == len(offers) == count, "TLS comparison publisher ledger")
    while completed < total:
        completed, peak = next(progress)
    publisher.sendall(publication(total, time.perf_counter_ns(), 0))
    for line in trace_lines(trace, process, "DISCONNECT"):
        if line == b"Received DISCONNECT from " + IDENTITY:
            break
    publisher.sendall(packet(0xe0, b""))
    return {"wire_completed": count, "publisher_confirmed": count, "broker_ack_drain": True,
            "peak_inflight": peak, "publisher_confirmed_per_sec": count * SECOND / elapsed,
            "subscriber_credit_checks": credit_checks,
            "measurement_begin_ns": begin, "peer_cpu_seconds": peer_cpu,
            **percentiles(acks, "publisher"), **percentiles(offers, "offer_lag")}


def sample(executable, name, args, *, xmqtt_default_tcp=False, broker=None, tls_ca=None):
    row = {"client": name, "verified": False}
    process = None
    try:
        with ExitStack() as stack:
            if tls_ca is None:
                listener = stack.enter_context(socket.socket())
                listener.bind(("127.0.0.1", 0))
                listener.listen(1)
                listener.settimeout(5)
                port = listener.getsockname()[1]
            else:
                publisher_port, broker_pid, port, trace_path = broker
                trace = stack.enter_context(trace_path.open("rb"))
                trace.seek(0, os.SEEK_END)
            command = [str(executable), *map(str, (port, args.count, args.window, args.qos))]
            if tls_ca is not None:
                command.extend(("--tls", str(tls_ca)))
            if name == "xmqtt" and xmqtt_default_tcp:
                command.append("--default-tcp")
            process = start(command, args.client_cpus, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if tls_ca is not None:
                cpu, _, _ = usage(broker_pid)
                with broker_socket(publisher_port) as publisher:
                    wire = tls_transfer(publisher, trace, process, args.qos, args.window, args.rate, args.count)
                after, rss, peak = usage(broker_pid)
                wire.update(broker_cpu_seconds=after - cpu, broker_rss_bytes=rss, broker_peak_rss_bytes=peak)
            else:
                connection = stack.enter_context(listener.accept()[0])
                connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                connection.settimeout(5)
                if broker is None:
                    handshake(connection, args.window, args.qos)
                    wire = transfer(connection, args.qos, args.window, args.rate, args.count)
                else:
                    port, broker_pid = broker
                    cpu, _, _ = usage(broker_pid)
                    with broker_socket(port) as upstream, broker_socket(port) as publisher:
                        handshake(connection, args.window, args.qos, upstream)
                        wire = broker_transfer(connection, upstream, publisher, args.qos, args.window, args.rate, args.count)
                    after, rss, peak = usage(broker_pid)
                    wire.update(broker_cpu_seconds=after - cpu, broker_rss_bytes=rss, broker_peak_rss_bytes=peak)
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
        if tls_ca is not None:
            elapsed = metrics["last_reliable_ns"] - wire["measurement_begin_ns"]
            require(metrics["tls"] is True and 0 < elapsed < args.count * SECOND // args.rate + DEADLINE,
                    "TLS application measurement interval")
            wire["application_verified_per_sec"] = args.count * SECOND / elapsed
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


@contextmanager
def standalone_broker(path, cpu, directory, report, *, tls=None, trace_path=None):
    with socket.socket() as reservation, socket.socket() as secure:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
        if tls is not None:
            secure.bind(("127.0.0.1", 0))
            tls_port = secure.getsockname()[1]
    report["broker_config"] = (
        f"listener {port} 127.0.0.1\nallow_anonymous true\npersistence false\n"
        f"max_inflight_messages 64\nmax_queued_messages 256\nmax_packet_size {BROKER_MAX_PACKET}\n"
        "set_tcp_nodelay true\nsys_interval 0\nlog_dest stderr\nlog_type error\nlog_type notice\n"
    )
    if tls is not None:
        report["broker_config"] += (
            f"log_type debug\nlog_timestamp false\nlistener {tls_port} 127.0.0.1\n"
            f"certfile certificate.pem\nkeyfile key.pem\ntls_version tlsv1.3\nciphers_tls1.3 {TLS_CIPHER}\n"
        )
    # Keep the live trace on the host's temporary filesystem. Archive it after the broker exits.
    with tempfile.TemporaryDirectory(prefix="xmqtt-receiver-", dir=directory if tls is None else None) as temp:
        config = Path(temp) / "broker.conf"
        config.write_text(report["broker_config"], encoding="ascii")
        output = Path(temp) / "broker.log"
        with output.open("wb+") as log:
            broker = start([str(path), "-c", str(config)], {cpu}, cwd=tls, stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 8
                while True:
                    require(broker.poll() is None and time.monotonic() < deadline, "broker readiness deadline")
                    try:
                        with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                            break
                    except OSError:
                        time.sleep(0.025)
                if tls is None:
                    yield port, broker.pid
                else:
                    context = ssl.create_default_context(cafile=str(tls / "ca.pem"))
                    context.minimum_version = ssl.TLSVersion.TLSv1_3
                    leaf = ssl.PEM_cert_to_DER_cert((tls / "certificate.pem").read_text(encoding="ascii"))
                    with broker_socket(tls_port) as peer, context.wrap_socket(peer, server_hostname="localhost") as checked:
                        require(checked.version() == "TLSv1.3" and checked.cipher()[0] == TLS_CIPHER and
                                checked.getpeercert(binary_form=True) == leaf, "TLS broker identity or negotiated profile")
                    yield port, broker.pid, tls_port, output
                require(broker.poll() is None, "unexpected broker exit")
            finally:
                report["broker_exit"] = stop(broker)
                log.seek(0)
                if tls is None:
                    report["broker_log"] = log.read().decode("utf-8", "replace")
                else:
                    shutil.copyfile(output, trace_path)
                    report["broker_trace"] = {"name": trace_path.name, "bytes": output.stat().st_size,
                                              "sha256": hashlib.file_digest(log, "sha256").hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("xmqtt", "paho", "report"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--broker", type=Path, help="relay the subscriber stream through pinned Mosquitto")
    parser.add_argument("--broker-cpu", type=int)
    parser.add_argument("--tls-credentials", type=Path, metavar="DIRECTORY",
                        help="connect TLS-enabled receivers directly to the pinned broker")
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
    if (args.broker is None) != (args.broker_cpu is None):
        parser.error("select --broker and --broker-cpu together")
    tls = args.tls_credentials.resolve(strict=True) if args.tls_credentials else None
    if tls is not None and args.broker is None:
        parser.error("--tls-credentials requires a standalone broker")
    if tls is not None and not all((tls / file).is_file() for file in ("ca.pem", "certificate.pem", "key.pem")):
        parser.error("TLS credentials require ca.pem, certificate.pem and key.pem")
    clients = {name: getattr(args, name).resolve(strict=True) for name in ("xmqtt", "paho")}
    require(len(set(clients.values())) == 2, "distinct client binaries required")
    broker = args.broker.resolve(strict=True) if args.broker else None
    if broker is not None:
        if args.broker_cpu in cpus or args.broker_cpu not in affinity(os.getpid()):
            parser.error("select a separate available broker CPU")
        require(broker not in clients.values(), "distinct broker and client binaries required")
        version = subprocess.run([str(broker), "-h"], capture_output=True, text=True, timeout=10)
        require(MOSQUITTO in version.stdout.splitlines(), "pinned broker version")
    args.report = args.report.resolve()
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
    report["fixture"] = "mosquitto-relay" if broker is not None else "wire-peer"
    fixture = nullcontext()
    if broker is not None:
        report.update(broker_version=MOSQUITTO, broker_cpu=args.broker_cpu)
        report["binaries"]["broker"] = fingerprint(broker)
        trace_path = None
        if tls is not None:
            trace_path = args.report.with_name(args.report.name + ".broker.log")
            report["fixture"] = "mosquitto-tls-direct"
            report["tls"] = {"version": "1.3", "cipher": TLS_CIPHER, "dns": "localhost",
                             "ca": fingerprint(tls / "ca.pem"), "leaf": fingerprint(tls / "certificate.pem"),
                             "outstanding_publications": TLS_MAX_PENDING}
            report["inputs"].append(fingerprint(HERE.parent / "examples/tls/material.h"))
        fixture = standalone_broker(broker, args.broker_cpu, args.report.parent, report, tls=tls, trace_path=trace_path)
    try:
        with fixture as running_broker:
            for repeat in range(args.repeats + 1):
                for name in clients if repeat % 2 == 0 else reversed(clients):
                    run = sample(clients[name], name, args, xmqtt_default_tcp=args.xmqtt_default_tcp,
                                 broker=running_broker, tls_ca=tls / "ca.pem" if tls is not None else None)
                    report["runs"].append(run | {"repeat": repeat, "warmup": repeat == 0})
                    print(f"{name} repeat={repeat} verified={run['verified']} {run.get('error', '')}", flush=True)
                args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        report["error"] = str(error)
        args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        print(str(error), file=sys.stderr)
        return 1
    report["verified"] = all(run["verified"] for run in report["runs"])
    if report["verified"] and args.repeats >= 5:
        if tls is not None:
            metrics = ("application_p99_us", "application_verified_per_sec", "publisher_p99_us",
                       "publisher_confirmed_per_sec", "offer_lag_p99_us")
        else:
            metrics = METRICS + (("subscriber_leg_p99_us",) if broker is not None else ())
        report["variation"] = {name: {metric: noise([run["metrics"][metric] for run in report["runs"]
                                  if run["client"] == name and not run["warmup"]])
                                      for metric in metrics} for name in clients}
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return 0 if report["verified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
