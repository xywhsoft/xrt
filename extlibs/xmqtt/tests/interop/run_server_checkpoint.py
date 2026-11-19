#!/usr/bin/env python3
"""Verify restored retained messages against a new broker process."""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import queue
import socket
import subprocess
import tempfile
from threading import Thread
import time
import zlib

from run_server_handshake import connect, topic
from run_server_tls import frame, receive


PUBLISHER = b"original-publisher"
STABLE = b"restart/stable"
FINITE = b"restart/finite"
SHORT = b"restart/short"
LEGACY = b"restart/legacy"
CLEARED = b"restart/cleared"
PING = b"\xc0\0"
PONG = (0xd0, b"")


@contextmanager
def broker(program, mode, image):
    with subprocess.Popen([str(program.resolve()), mode, str(image)], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, text=True) as process:
        lines = queue.Queue()
        Thread(target=lambda: lines.put(process.stdout.readline()), daemon=True).start()
        try:
            host, port = lines.get(timeout=10).strip().rsplit(":", 1)
            assert host == "127.0.0.1"
            yield host, int(port)
        finally:
            if process.poll() is None:
                process.stdin.write("\n")
                process.stdin.flush()
            try:
                assert process.wait(timeout=10) == 0, "broker did not publish its retained checkpoint"
            except subprocess.TimeoutExpired:
                process.kill()
                raise


def publish(sock, name, payload, expiry=None):
    fields = b"" if expiry is None else b"\x02" + expiry.to_bytes(4, "big")
    sock.sendall(frame(0x33, topic(name) + b"\0\x05" + bytes((len(fields),)) + fields + payload))
    ack = receive(sock)
    assert ack == (0x40, b"\0\x05\x10"), ack  # Stored, with no matching live subscribers.


def subscribe(sock, name, identifier, options):
    packet_id = identifier.to_bytes(2, "big")
    sock.sendall(frame(0x82, packet_id + b"\0" + topic(name) + bytes((options,))))
    assert receive(sock) == (0x90, packet_id + b"\0" + bytes((options & 3,)))


def barrier(sock):
    sock.sendall(PING)
    assert receive(sock) == PONG, "unexpected retained delivery before PINGRESP"


def verify_restored(address):
    same, _ = connect(address, 5, PUBLISHER)
    with same:
        subscribe(same, STABLE, 1, 5)
        barrier(same)

    observer, _ = connect(address, 5, b"observer")
    with observer:
        subscribe(observer, STABLE, 1, 1)
        kind, body = receive(observer)
        assert kind == 0x33 and body.startswith(topic(STABLE)) and body.endswith(b"\0renewed\x00\xff")
        at = len(topic(STABLE))
        observer.sendall(frame(0x40, body[at:at + 2]))
        barrier(observer)

        subscribe(observer, FINITE, 2, 0)
        kind, body = receive(observer)
        prefix = topic(FINITE) + b"\x05\x02"
        assert kind == 0x31 and body.startswith(prefix) and body[len(prefix) + 4:] == b"expiring"
        expiry = int.from_bytes(body[len(prefix):len(prefix) + 4], "big")
        assert 0 < expiry < 120, expiry
        barrier(observer)

        subscribe(observer, LEGACY, 3, 0)
        assert receive(observer) == (0x31, topic(LEGACY) + b"\0mqtt-311")
        barrier(observer)
        for index, name in enumerate((CLEARED, SHORT), 4):
            subscribe(observer, name, index, 0)
            barrier(observer)
        return expiry


def rejected_image(program, path, data):
    path.write_bytes(data)
    failed = subprocess.run([str(program.resolve()), "restore", str(path)], input="\n",
                            capture_output=True, text=True, timeout=10)
    assert failed.returncode == 1 and not failed.stdout, (failed.returncode, failed.stdout, failed.stderr)
    assert path.read_bytes() == data, "failed restore must preserve the original checkpoint"


def verify(program, directory):
    image = directory / "retained.bin"
    with broker(program, "capture", image) as address:
        legacy, _ = connect(address, 4, b"legacy-publisher")
        with legacy:
            legacy.sendall(frame(0x31, topic(LEGACY) + b"mqtt-311") + PING)
            assert receive(legacy) == PONG
        source, _ = connect(address, 5, PUBLISHER)
        with source:
            publish(source, STABLE, b"old")
            publish(source, STABLE, b"renewed\x00\xff")
            publish(source, FINITE, b"expiring", 120)
            publish(source, CLEARED, b"gone")
            publish(source, CLEARED, b"")
            publish(source, SHORT, b"expires while down", 3)
    stored = image.read_bytes()
    assert stored[:8] == b"XMQRET02" and int.from_bytes(stored[16:20], "big") == 4
    time.sleep(3.1)
    with broker(program, "restore", image) as address:
        first_expiry = verify_restored(address)
    with broker(program, "restore", image) as address:
        assert verify_restored(address) <= first_expiry
    offset = 20
    for _ in range(4):
        publisher_bytes = int.from_bytes(stored[offset + 1:offset + 3], "big")
        wire_bytes = int.from_bytes(stored[offset + 3:offset + 7], "big")
        offset += 15 + publisher_bytes + wire_bytes
    previous = b"XMQRET01" + stored[8:offset]
    previous_path = directory / "retained-v1.bin"
    previous_path.write_bytes(previous + zlib.crc32(previous).to_bytes(4, "big"))
    with broker(program, "restore", previous_path) as address:
        assert verify_restored(address) <= first_expiry
    bad = directory / "invalid.bin"
    rejected_image(program, bad, stored[:-1])
    rejected_image(program, bad, stored[:-1] + bytes((stored[-1] ^ 1,)))
    future = stored[:8] + ((1 << 63) - 1).to_bytes(8, "big") + stored[16:-4]
    rejected_image(program, bad, future + zlib.crc32(future).to_bytes(4, "big"))
    return {"retained_restart": "pass", "source_versions": ["5.0", "3.1.1"],
            "finite_expiry_and_no_local": True, "corruption_and_clock_rejection": True}


def persistent(address, client_id, clean, present=False, *, version=5, will=b""):
    peer = socket.create_connection(address, timeout=8)
    fields = b"\5\x11\0\0\0\x3c" if version == 5 else b""
    flags = (2 if clean else 0) | (0x24 if will else 0)
    peer.sendall(frame(0x10, b"\0\4MQTT" + bytes((version, flags, 0, 0)) +
                       fields + topic(client_id) + will))
    kind, body = receive(peer)
    assert kind == 0x20 and body[:2] == bytes((present, 0)), (kind, body)
    return peer


def received_publish(peer, expected_header, name, payload):
    kind, body = receive(peer)
    prefix = topic(name)
    assert kind == expected_header and body.startswith(prefix) and body[len(prefix) + 2:] == b"\0" + payload
    return body[len(prefix):len(prefix) + 2]


def verify_sessions(program, directory):
    image = directory / "sessions.bin"
    target = b"restart/flow/q"
    due = b"restart/will"
    legacy_topic = b"restart/legacy-inbox"
    producer = None
    try:
        with broker(program, "capture", image) as address:
            with persistent(address, b"pending-will", True, will=b"\5\x18\0\0\0\4" +
                            topic(due) + topic(b"after-restart")) as will:
                will.sendall(b"\xe0\1\x04")
                assert not will.recv(1)
            producer = persistent(address, b"producer", True)
            with persistent(address, b"legacy-worker", False, version=4) as legacy:
                legacy.sendall(frame(0x82, b"\0\1" + topic(legacy_topic) + b"\1"))
                assert receive(legacy) == (0x90, b"\0\1\1")
                legacy.sendall(b"\xe0\0")
                assert not legacy.recv(1)
            producer.sendall(frame(0x32, topic(legacy_topic) + b"\0\x10\0legacy"))
            assert receive(producer) == (0x40, b"\0\x10")
            with persistent(address, b"worker", True) as worker:
                subscribe(worker, b"restart/flow/+", 1, 2)
                producer.sendall(frame(0x32, topic(target) + b"\0\x11\0first"))
                assert receive(producer) == (0x40, b"\0\x11")
                first = received_publish(worker, 0x32, target, b"first")
                producer.sendall(frame(0x34, topic(target) + b"\0\x12\0second"))
                assert receive(producer) == (0x50, b"\0\x12")
                second = received_publish(worker, 0x34, target, b"second")
                worker.sendall(frame(0x50, second))
                assert receive(worker) == (0x62, second)
                worker.sendall(b"\xe0\0")
                assert not worker.recv(1)
            producer.sendall(frame(0x32, topic(target) + b"\0\x13\0queued"))
            assert receive(producer) == (0x40, b"\0\x13")
    finally:
        if producer is not None:
            producer.close()
    time.sleep(4.1)
    with broker(program, "restore", image) as address:
        with persistent(address, b"legacy-worker", False, True, version=4) as legacy:
            kind, body = receive(legacy)
            prefix = topic(legacy_topic)
            assert kind == 0x32 and body.startswith(prefix) and body[len(prefix) + 2:] == b"legacy"
            legacy.sendall(frame(0x40, body[len(prefix):len(prefix) + 2]))
        with persistent(address, b"worker", False, True) as worker:
            assert received_publish(worker, 0x3a, target, b"first") == first
            assert receive(worker) == (0x62, second)
            queued = received_publish(worker, 0x32, target, b"queued")
            assert queued not in (first, second, b"\0\0")
            worker.sendall(frame(0x40, first) + frame(0x70, second) + frame(0x40, queued))
            with persistent(address, b"producer", False, True) as producer:
                producer.sendall(frame(0x3c, topic(target) + b"\0\x12\0second"))
                assert receive(producer) == (0x50, b"\0\x12")
                producer.sendall(b"\x62\x02\0\x12")
                assert receive(producer) == (0x70, b"\0\x12")
                barrier(worker)
        observer, _ = connect(address, 5, b"will-observer")
        with observer:
            subscribe(observer, due, 1, 8)
            assert receive(observer) == (0x31, topic(due) + b"\0after-restart")
    return {"sessions": "pass", "legacy_session": True, "outbound_qos1_and_qos2": True, "incoming_qos2_dedupe": True,
            "offline_fifo_and_subscriptions": True, "delayed_will": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="xmqtt-retained-") as directory:
        folder = Path(directory)
        print(json.dumps({**verify(args.program, folder), **verify_sessions(args.program, folder)}))


if __name__ == "__main__":
    main()
