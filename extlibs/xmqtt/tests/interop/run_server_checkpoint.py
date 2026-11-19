#!/usr/bin/env python3
"""Verify restored retained messages against a new broker process."""
import argparse
from contextlib import ExitStack, contextmanager
import json
import os
from pathlib import Path
import queue
from select import select
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
def broker(program, mode, image, *options, crash=False, exit_code=0):
    with subprocess.Popen([str(program.resolve()), mode, str(image), *options], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, text=True) as process:
        lines = queue.Queue()
        Thread(target=lambda: lines.put(process.stdout.readline()), daemon=True).start()
        try:
            host, port = lines.get(timeout=10).strip().rsplit(":", 1)
            assert host == "127.0.0.1"
            yield host, int(port)
        finally:
            if crash:
                assert process.poll() is None, "broker stopped before the restart probe"
                process.kill()
                process.wait(timeout=10)
            else:
                if process.poll() is None:
                    process.stdin.write("\n")
                    process.stdin.flush()
                try:
                    assert process.wait(timeout=10) == exit_code, "unexpected checkpoint broker exit"
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
    assert stored[:8] == b"XMQRET05" and int.from_bytes(stored[16:20], "big") == 4
    time.sleep(3.1)
    with broker(program, "restore", image) as address:
        first_expiry = verify_restored(address)
    with broker(program, "restore", image) as address:
        assert verify_restored(address) <= first_expiry
    offset = 20
    prior_retained = bytearray(stored[8:20])
    for _ in range(4):
        publisher_bytes = int.from_bytes(stored[offset + 1:offset + 3], "big")
        wire_bytes = int.from_bytes(stored[offset + 3:offset + 7], "big")
        end = offset + 23 + publisher_bytes + wire_bytes
        prior_retained += stored[offset:offset + 15] + stored[offset + 23:end]
        offset = end
    previous = b"XMQRET01" + prior_retained
    previous_path = directory / "retained-v1.bin"
    previous_path.write_bytes(previous + zlib.crc32(previous).to_bytes(4, "big"))
    with broker(program, "restore", previous_path) as address:
        assert verify_restored(address) <= first_expiry
    for magic in (b"XMQRET02", b"XMQRET03"):
        version = magic + prior_retained + b"\0\0\0\0"
        previous_path.write_bytes(version + zlib.crc32(version).to_bytes(4, "big"))
        with broker(program, "restore", previous_path) as address:
            assert verify_restored(address) <= first_expiry
    bad = directory / "invalid.bin"
    rejected_image(program, bad, stored[:-1])
    rejected_image(program, bad, stored[:-1] + bytes((stored[-1] ^ 1,)))
    future = stored[:8] + ((1 << 63) - 1).to_bytes(8, "big") + stored[16:-4]
    rejected_image(program, bad, future + zlib.crc32(future).to_bytes(4, "big"))
    return {"retained_restart": "pass", "source_versions": ["5.0", "3.1.1"],
            "finite_expiry_and_no_local": True, "corruption_and_clock_rejection": True}


def persistent_connect(client_id, clean, *, version=5, will=b"", max_packet=None, expiry=60, will_qos=0):
    fields = b"\x11" + expiry.to_bytes(4, "big")
    if max_packet is not None:
        fields += b"\x27" + max_packet.to_bytes(4, "big")
    fields = bytes((len(fields),)) + fields if version == 5 else b""
    flags = (2 if clean else 0) | ((0x24 | will_qos << 3) if will else 0)
    return frame(0x10, b"\0\4MQTT" + bytes((version, flags, 0, 0)) + fields + topic(client_id) + will)


def expect_connack(peer, present=False, reason=0):
    kind, body = receive(peer)
    assert kind == 0x20 and body[:2] == bytes((present, reason)), (kind, body)


def persistent(address, client_id, clean, present=False, *, version=5, will=b"", max_packet=None, expiry=60, will_qos=0):
    peer = socket.create_connection(address, timeout=8)
    peer.sendall(persistent_connect(client_id, clean, version=version, will=will, max_packet=max_packet,
                                    expiry=expiry, will_qos=will_qos))
    expect_connack(peer, present)
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
    saved = image.read_bytes()
    corrupted = bytearray(saved[:-4])
    sent = next(at for at, _, _ in session_records(saved) if saved[at] == 5)
    corrupted[sent + 8:sent + 16] = (1).to_bytes(8, "big")
    rejected_image(program, directory / "sent-with-lifetime.bin",
                   corrupted + zlib.crc32(corrupted).to_bytes(4, "big"))

    historical = bytearray(b"XMQRET02" + saved[8:24])
    for at, body, end in session_records(saved):
        historical += saved[at:at + 22] + saved[body:end]
    historical_path = directory / "historical-sessions.bin"
    historical_path.write_bytes(historical + zlib.crc32(historical).to_bytes(4, "big"))
    with broker(program, "restore", historical_path, crash=True) as address:
        with persistent(address, b"producer", False, True) as peer:
            barrier(peer)

    v4 = bytearray(b"XMQRET04" + saved[8:24])
    for at, body, end in session_records(saved):
        key_bytes = 16 if saved[at] in (4, 5) else 0
        v4 += saved[at:body - key_bytes] + saved[body:end]
    v4_path = directory / "v4-unkeyed-deliveries.bin"
    v4_path.write_bytes(v4 + zlib.crc32(v4).to_bytes(4, "big"))
    with broker(program, "restore-online", v4_path, crash=True) as address:
        with persistent(address, b"v4-upgrade", True) as peer:
            barrier(peer)
        assert v4_path.read_bytes()[:8] == b"XMQRET05"
        with persistent(address, b"worker", False, True) as worker:
            assert received_publish(worker, 0x3a, target, b"first") == first
            assert receive(worker) == (0x62, second)
            received_publish(worker, 0x32, target, b"queued")

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


def expect_silence(peer):
    peer.settimeout(0.15)
    try:
        raise AssertionError(f"peer returned {peer.recv(1)!r} before the store commit completed")
    except socket.timeout:
        pass
    finally:
        peer.settimeout(8)


@contextmanager
def prevent_replacement(image):
    if os.name == "nt":
        # A Python CRT file handle permits reads and denies delete sharing.
        with image.open("rb"):
            yield
        return
    # The sample can read through a symlink, but xrtFileWriteAtomic refuses
    # to replace it. Keep the original data available through that pathname.
    original = image.with_suffix(".target")
    image.rename(original)
    image.symlink_to(original)
    try:
        yield
    finally:
        image.unlink()
        original.rename(image)


def verify_reset(program, directory):
    image = directory / "session-reset.bin"
    replaced, kept = b"replace-on-clean-start", b"keep-other-session"
    identifier = b"\0\x19"
    pubrel = frame(0x62, identifier)
    with broker(program, "capture", image) as address:
        for client_id in (replaced, kept):
            with persistent(address, client_id, True) as peer:
                subscribe(peer, b"reset/input", 1, 1)
                peer.sendall(frame(0x34, topic(b"reset/no-matches") + identifier + b"\0old"))
                assert receive(peer) == (0x50, identifier + b"\x10")
                peer.sendall(b"\xe0\0")
                assert not peer.recv(1)
    original = image.read_bytes()

    with broker(program, "restore-fenced", image, "1500", crash=True) as address:
        with socket.create_connection(address, timeout=8) as replacement:
            replacement.sendall(persistent_connect(replaced, True) + PING)
            expect_silence(replacement)
            with socket.create_connection(address, timeout=8) as competing:
                competing.sendall(persistent_connect(replaced, False))
                expect_connack(competing, reason=0x89)
    assert image.read_bytes() == original, "crash before commit changed the checkpoint"

    with broker(program, "restore-fenced", image, "750", crash=True) as address:
        with socket.create_connection(address, timeout=8) as replacement:
            replacement.sendall(persistent_connect(replaced, True) + PING)
            expect_silence(replacement)
            with persistent(address, kept, False, True) as neighbor:
                barrier(neighbor)
            expect_connack(replacement)
            assert receive(replacement) == PONG
            assert image.read_bytes() != original, "CONNACK preceded checkpoint replacement"

    with broker(program, "restore-fenced", image, crash=True) as address:
        with persistent(address, replaced, False) as replacement:
            replacement.sendall(pubrel)
            assert receive(replacement) == (0x70, identifier + b"\x92")
        with persistent(address, kept, False, True) as neighbor:
            neighbor.sendall(pubrel)
            assert receive(neighbor) == (0x70, identifier)

    # A storage read rejected before atomic replacement preserves the backend
    # image and refuses CONNECT. The broker may still serve other clients.
    image.write_bytes(original)
    moved = image.with_suffix(".saved")
    with broker(program, "restore-fenced", image, crash=True) as address:
        image.rename(moved)
        with socket.create_connection(address, timeout=8) as replacement:
            replacement.sendall(persistent_connect(replaced, True))
            expect_connack(replacement, reason=0x88)
            assert not replacement.recv(1)
        moved.rename(image)
        with persistent(address, kept, False, True) as neighbor:
            barrier(neighbor)
    assert image.read_bytes() == original

    with prevent_replacement(image):
        with broker(program, "restore-fenced", image, exit_code=1) as address:
            for client_id, clean in ((replaced, True), (kept, False)):
                with socket.create_connection(address, timeout=8) as rejected:
                    try:
                        rejected.sendall(persistent_connect(client_id, clean))
                        assert not rejected.recv(1), "broker sent a reply after an unknown outcome"
                    except ConnectionResetError:
                        pass
    assert image.read_bytes() == original, "unknown outcome allowed checkpoint export"

    # Retire the server while the replacement is still queued in the APP.
    with broker(program, "restore-fenced", image, "750") as address:
        with socket.create_connection(address, timeout=8) as replacement:
            replacement.sendall(persistent_connect(replaced, True))
            expect_silence(replacement)
    with broker(program, "restore", image, crash=True) as address:
        with persistent(address, replaced, False) as replacement:
            replacement.sendall(pubrel)
            assert receive(replacement) == (0x70, identifier + b"\x92")
        with persistent(address, kept, False, True) as neighbor:
            barrier(neighbor)
    return {"session_reset_before_connack": True, "reset_crash_before_and_after_commit": True,
            "reset_same_identity_exclusion": True, "reset_checkpoint_rejection": True,
            "reset_unknown_seals_broker": True, "reset_retires_during_shutdown": True}


def qos2_exchange(peer, identifier, payload):
    peer.sendall(frame(0x34, topic(b"pubrel/input") + identifier + b"\0" + payload))
    kind, body = receive(peer)
    assert kind == 0x50 and body in (identifier, identifier + b"\x10"), (kind, body)


def expect_disconnect(peer):
    try:
        assert not peer.recv(1), "the broker sent a reply before closing"
    except ConnectionResetError:
        pass


def verify_pubrel_commit(program, directory):
    image = directory / "pubrel-commit.bin"
    sender, legacy = b"durable-publisher", b"durable-legacy-publisher"
    identifier, other = b"\0\x19", b"\0\x1a"
    pubrel = frame(0x62, identifier)
    with broker(program, "capture", image) as address:
        with persistent(address, sender, True) as peer:
            qos2_exchange(peer, identifier, b"old")
            qos2_exchange(peer, other, b"other")
            peer.sendall(b"\xe0\0")
            assert not peer.recv(1)
        with persistent(address, legacy, False, version=4) as peer:
            peer.sendall(frame(0x34, topic(b"pubrel/input") + identifier + b"legacy-old"))
            assert receive(peer) == (0x50, identifier)
            peer.sendall(b"\xe0\0")
            assert not peer.recv(1)
    original = image.read_bytes()

    with broker(program, "restore-fenced", image, "1500", crash=True) as address:
        with persistent(address, sender, False, True) as peer:
            peer.sendall(pubrel + PING)
            expect_silence(peer)
    assert image.read_bytes() == original, "interrupted PUBREL removed a durable identifier"

    missing = image.with_suffix(".saved")
    with broker(program, "restore-fenced", image, crash=True) as address:
        with persistent(address, sender, False, True) as peer:
            image.rename(missing)
            peer.sendall(pubrel)
            expect_disconnect(peer)
            missing.rename(image)
        assert image.read_bytes() == original
        with persistent(address, sender, False, True) as peer:
            peer.sendall(pubrel + PING)
            assert receive(peer) == (0x70, identifier)
            assert receive(peer) == PONG
            assert image.read_bytes() != original, "PUBCOMP preceded durable identifier removal"
        with persistent(address, legacy, False, True, version=4) as peer:
            before = image.read_bytes()
            peer.sendall(pubrel + PING)
            assert receive(peer) == (0x70, identifier)
            assert receive(peer) == PONG
            assert image.read_bytes() != before

    with broker(program, "restore-fenced", image, crash=True) as address:
        with persistent(address, sender, False, True) as peer:
            peer.sendall(pubrel)
            assert receive(peer) == (0x70, identifier + b"\x92")
            peer.sendall(frame(0x62, other))
            assert receive(peer) == (0x70, other), "unrelated identifier was removed"
            observer, _ = connect(address, 5, b"pubrel-observer")
            with observer:
                subscribe(observer, b"pubrel/input", 1, 0)
                qos2_exchange(peer, identifier, b"new")
                assert receive(observer) == (0x30, topic(b"pubrel/input") + b"\0new")
                # This identifier was accepted only in memory. PUBREL does
                # not need an application lookup for an unpersisted record.
                image.rename(missing)
                peer.sendall(pubrel)
                assert receive(peer) == (0x70, identifier)
                missing.rename(image)
                with persistent(address, legacy, False, True, version=4) as old_client:
                    old_client.sendall(frame(0x34, topic(b"pubrel/input") + identifier + b"legacy-new"))
                    assert receive(old_client) == (0x50, identifier)
                    assert receive(observer) == (0x30, topic(b"pubrel/input") + b"\0legacy-new")
                    old_client.sendall(pubrel)
                    assert receive(old_client) == (0x70, identifier)
                barrier(observer)

    # The old exchange finishes while its transport is being replaced. No
    # PUBCOMP reaches that transport, but the new binding sees the removal.
    image.write_bytes(original)
    with broker(program, "restore-fenced", image, "750", crash=True) as address:
        with persistent(address, sender, False, True) as previous:
            previous.sendall(pubrel)
            expect_silence(previous)
            with persistent(address, sender, False, True) as successor:
                assert receive(previous) == (0xe0, b"\x8e")
                successor.sendall(pubrel)
                assert receive(successor) == (0x70, identifier + b"\x92")
    with broker(program, "restore-fenced", image, crash=True) as address:
        with persistent(address, sender, False, True) as peer:
            peer.sendall(pubrel)
            assert receive(peer) == (0x70, identifier + b"\x92")

    image.write_bytes(original)
    with prevent_replacement(image):
        with broker(program, "restore-fenced", image, exit_code=1) as address:
            with persistent(address, sender, False, True) as peer:
                peer.sendall(pubrel)
                expect_disconnect(peer)
            with socket.create_connection(address, timeout=8) as rejected:
                try:
                    rejected.sendall(persistent_connect(sender, False))
                    expect_disconnect(rejected)
                except ConnectionResetError:
                    pass
    assert image.read_bytes() == original
    return {"pubcomp_after_durable_remove": ["5.0", "3.1.1"], "pubrel_recovery_and_identifier_reuse": True,
            "pubrel_takeover_and_lost_pubcomp": True, "pubrel_unknown_seals_broker": True}


def session_records(data):
    assert data[:8] in (b"XMQRET03", b"XMQRET04", b"XMQRET05", b"XMQRET06")
    at = 20
    retained_header = 15 if data[7:8] == b"3" else 23
    for _ in range(int.from_bytes(data[16:20], "big")):
        publisher = int.from_bytes(data[at + 1:at + 3], "big")
        publish = int.from_bytes(data[at + 3:at + 7], "big")
        at += retained_header + publisher + publish
    count = int.from_bytes(data[at:at + 4], "big")
    at += 4
    for _ in range(count):
        kind = data[at]
        body = at + {1: 46, 4: 30}.get(kind, 22)
        if data[7:8] >= b"5" and kind in (4, 5):
            body += 16
        size = int.from_bytes(data[at + 16:at + 20], "big")
        group = int.from_bytes(data[at + 20:at + 22], "big")
        yield at, body, body + size + group
        at = body + size + group
    assert at == len(data) - 4


def session_keys(image):
    data = image.read_bytes()
    keys = {}
    for at, body, end in session_records(data):
        if data[at] == 1:
            key = data[at + 22:at + 38]
            assert key != b"\0" * 16
            keys[data[body:end]] = key
    return keys


def verify_online_transactions(program, directory):
    image = directory / "online-qos2.bin"
    alice, bob, carol = b"online-source", b"offline-target", b"legacy-target"
    name, identifier = b"atomic/queue", b"\0\x31"
    publish = lambda header: frame(header, topic(name) + identifier + b"\5\x02\0\0\0\x1ecommit")
    pubrec = (0x50, identifier)

    with broker(program, "capture", image) as address:
        with persistent(address, alice, True) as publisher:
            barrier(publisher)
        with persistent(address, bob, True) as target:
            subscribe(target, name, 1, 1)
        with persistent(address, carol, False, version=4) as legacy:
            legacy.sendall(frame(0x82, b"\0\1" + topic(name) + b"\1"))
            assert receive(legacy) == (0x90, b"\0\1\1")
    original = image.read_bytes()
    keys = session_keys(image)
    assert len(set(keys.values())) == 3

    with broker(program, "restore-online", image, "1200", crash=True) as address:
        with persistent(address, alice, False, True) as publisher:
            observer, _ = connect(address, 5, b"online-observer")
            with observer:
                subscribe(observer, name, 1, 0)
                publisher.sendall(publish(0x34))
                expect_silence(publisher)
                expect_silence(observer)
    assert image.read_bytes() == original, "the uncommitted publication changed the checkpoint"

    with broker(program, "restore-online", image, "600", crash=True) as address:
        with persistent(address, alice, False, True) as publisher:
            observer, _ = connect(address, 5, b"online-observer")
            with observer:
                subscribe(observer, name, 1, 0)
                publisher.sendall(publish(0x34) + PING)
                expect_silence(observer)
                assert receive(publisher) == pubrec
                assert receive(publisher) == PONG
                kind, body = receive(observer)
                assert kind == 0x30 and body.startswith(topic(name)) and body.endswith(b"commit")
                assert image.read_bytes() != original, "PUBREC preceded the store transaction"

    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, alice, False, True) as publisher:
            publisher.sendall(publish(0x3c))
            assert receive(publisher) == pubrec
            with persistent(address, bob, False, True) as target:
                kind, body = receive(target)
                at = len(topic(name))
                assert kind == 0x32 and body[:at] == topic(name)
                assert body[at + 2:at + 4] == b"\5\x02" and body[at + 8:] == b"commit"
                assert 1 <= int.from_bytes(body[at + 4:at + 8], "big") <= 30
                target.sendall(frame(0x40, body[at:at + 2]))
                barrier(target)
            with persistent(address, carol, False, True, version=4) as target:
                kind, body = receive(target)
                at = len(topic(name))
                assert kind == 0x32 and body[:at] == topic(name) and body[at + 2:] == b"commit"
                target.sendall(frame(0x40, body[at:at + 2]))
                barrier(target)
            publisher.sendall(frame(0x62, identifier))
            assert receive(publisher) == (0x70, identifier)

    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, bob, False, True) as target:
            barrier(target)
        with persistent(address, carol, False, True, version=4) as target:
            barrier(target)

    image.write_bytes(original)
    saved = image.with_suffix(".unavailable")
    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, alice, False, True) as publisher:
            image.rename(saved)
            publisher.sendall(publish(0x34))
            assert receive(publisher) == (0x50, identifier + b"\x83")
            saved.rename(image)
            assert image.read_bytes() == original
            with persistent(address, bob, False, True) as target:
                barrier(target)

    with prevent_replacement(image):
        with broker(program, "restore-online", image, exit_code=1) as address:
            with persistent(address, alice, False, True) as publisher:
                publisher.sendall(publish(0x34))
                expect_disconnect(publisher)
    assert image.read_bytes() == original

    with broker(program, "restore-online", image, "500", crash=True) as address:
        publisher, _ = connect(address, 5, b"volatile-source")
        with publisher:
            publisher.sendall(publish(0x34))
            expect_silence(publisher)
            with persistent(address, bob, True) as replacement:
                barrier(replacement)
            assert receive(publisher) == pubrec
    replaced = session_keys(image)
    assert set(replaced) == set(keys)
    assert replaced[bob] != keys[bob] and replaced[alice] == keys[alice] and replaced[carol] == keys[carol]
    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, bob, False, True) as replacement:
            barrier(replacement)
        with persistent(address, carol, False, True, version=4) as remaining:
            kind, body = receive(remaining)
            assert kind == 0x32 and body[:len(topic(name))] == topic(name) and body.endswith(b"commit")
    return {"atomic_pubrec_and_recipient_recovery": ["5.0", "3.1.1"], "durable_fanout_deferred": True,
            "outgoing_puback_retired": ["5.0", "3.1.1"],
            "transaction_rejection_is_invisible": True, "transaction_unknown_seals": True,
            "fresh_session_fences_prior_recipient": True, "volatile_source_durable_fanout": True}



def verify_outbound_transactions(program, directory):
    image = directory / "outbound-transactions.bin"
    name, subscriber = b"atomic/outbound", b"durable-outbound"

    with broker(program, "capture", image) as address:
        with persistent(address, subscriber, True) as peer:
            subscribe(peer, name, 1, 2)

    with broker(program, "restore-online", image, crash=True) as address:
        publisher, _ = connect(address, 5, b"outbound-source")
        with publisher:
            for identifier in (b"\0\x01", b"\0\x02"):
                publisher.sendall(frame(0x34, topic(name) + identifier + b"\0same"))
                assert receive(publisher) == (0x50, identifier)
                publisher.sendall(frame(0x62, identifier))
                assert receive(publisher) == (0x70, identifier)

    def delivery_rows():
        data = image.read_bytes()
        return {data[body - 16:body]: (data[at], data[at + 1], data[at + 2:at + 4])
                for at, body, _ in session_records(data) if data[at] in (4, 5)}

    queued = image.read_bytes()
    rows = delivery_rows()
    assert len(rows) == 2 and b"\0" * 16 not in rows
    assert all(row == (4, 0, b"\0\0") for row in rows.values())

    with broker(program, "restore-online", image, "1200", crash=True) as address:
        with persistent(address, subscriber, False, True) as peer:
            expect_silence(peer)
    assert image.read_bytes() == queued

    with broker(program, "restore-online", image, "600", crash=True) as address:
        with persistent(address, subscriber, False, True) as peer:
            expect_silence(peer)
            first = received_publish(peer, 0x34, name, b"same")
            assert first != b"\0\0"
            assert (5, 5, first) in delivery_rows().values()
            expect_silence(peer)

    with broker(program, "restore-online", image, "600", crash=True) as address:
        with persistent(address, subscriber, False, True) as peer:
            assert received_publish(peer, 0x3c, name, b"same") == first
            second = received_publish(peer, 0x34, name, b"same")
            assert second not in (first, b"\0\0")
            peer.sendall(frame(0x50, first) + PING)
            expect_silence(peer)
            assert receive(peer) == (0x62, first)
            assert receive(peer) == PONG
            assert (5, 7, first) in delivery_rows().values()

    before_ack = image.read_bytes()
    saved = image.with_suffix(".unavailable")
    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, subscriber, False, True) as peer:
            assert receive(peer) == (0x62, first)
            assert received_publish(peer, 0x3c, name, b"same") == second
            image.rename(saved)
            peer.sendall(frame(0x70, first) + PING)
            expect_disconnect(peer)
            saved.rename(image)
    assert image.read_bytes() == before_ack

    with prevent_replacement(image):
        with broker(program, "restore-online", image, exit_code=1) as address:
            with persistent(address, subscriber, False, True) as peer:
                assert receive(peer) == (0x62, first)
                assert received_publish(peer, 0x3c, name, b"same") == second
                peer.sendall(frame(0x70, first))
                expect_disconnect(peer)
    assert image.read_bytes() == before_ack

    with broker(program, "restore-online", image, "500", crash=True) as address:
        with persistent(address, subscriber, False, True) as peer:
            assert receive(peer) == (0x62, first)
            assert received_publish(peer, 0x3c, name, b"same") == second
            peer.sendall(frame(0x70, first) + frame(0x50, second + b"\x83") + PING)
            expect_silence(peer)
            assert receive(peer) == PONG
            assert not delivery_rows()

    with broker(program, "restore-online", image, "500", crash=True) as address:
        with persistent(address, subscriber, False, True) as peer:
            barrier(peer)
            expect_silence(peer)
            publisher, _ = connect(address, 5, b"outbound-next")
            with publisher:
                publisher.sendall(frame(0x34, topic(name) + b"\0\x01\0next"))
                assert receive(publisher) == (0x50, b"\0\x01")
                next_id = received_publish(peer, 0x34, name, b"next")
                assert int.from_bytes(next_id, "big") == int.from_bytes(second, "big") + 1
                peer.sendall(frame(0x50, next_id))
                expect_silence(peer)
                with persistent(address, subscriber, False, True) as successor:
                    assert receive(peer) == (0xe0, b"\x8e")
                    assert receive(successor) == (0x62, next_id)
                    successor.sendall(frame(0x70, next_id))
                    barrier(successor)
                    successor.sendall(b"\xe0\0")
                    assert not successor.recv(1)
                publisher.sendall(frame(0x34, topic(name) + b"\0\x02\0" + b"!" * 256))
                assert receive(publisher) == (0x50, b"\0\x02")

    assert list(delivery_rows().values()) == [(4, 0, b"\0\0")]
    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, subscriber, False, True, max_packet=64) as peer:
            barrier(peer)
            assert not delivery_rows()
    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, subscriber, False, True) as peer:
            barrier(peer)
            expect_silence(peer)
    return {"outgoing_publish_and_pubrel_after_durable_phase": True, "outgoing_restart_replay": True,
            "terminal_pubcomp_and_negative_pubrec_retired": True, "outgoing_rejection_and_unknown": True,
            "outgoing_pubrel_commit_survives_takeover": True, "oversized_queued_publish_retired": True}

def verify_qos1_transactions(program, directory):
    image = directory / "online-qos1.bin"
    source, subscriber = b"qos1-source", b"qos1-offline"
    name, identifier = b"durable/qos1", b"\0\x25"
    original = frame(0x33, topic(name) + identifier + b"\0new")
    plain = frame(0x32, topic(name) + identifier + b"\0plain")
    legacy = frame(0x32, topic(name) + identifier + b"legacy")
    success = (0x40, identifier)

    with broker(program, "capture", image) as address:
        owner, _ = connect(address, 5, b"qos1-old-owner")
        with owner:
            publish(owner, name, b"old")
        with persistent(address, source, True) as publisher:
            barrier(publisher)
        with persistent(address, subscriber, True) as recipient:
            subscribe(recipient, name, 1, 0x22)
    before = image.read_bytes()

    with broker(program, "restore-online", image, "1200", crash=True) as address:
        with persistent(address, source, False, True) as publisher:
            observer, _ = connect(address, 5, b"qos1-observer")
            with observer:
                subscribe(observer, name, 1, 0x20)
                publisher.sendall(original + PING)
                expect_silence(publisher)
                expect_silence(observer)
    assert image.read_bytes() == before

    with broker(program, "restore-online", image, crash=True) as address:
        peer311, _ = connect(address, 4, b"qos1-legacy")
        with persistent(address, source, False, True) as publisher, peer311:
            saved = image.with_suffix(".unavailable")
            image.rename(saved)
            try:
                publisher.sendall(original)
                assert receive(publisher) == (0x40, identifier + b"\x83")
                publisher.sendall(frame(0x32, topic(b"durable/no-match") + identifier + b"\0skip"))
                assert receive(publisher) == (0x40, identifier + b"\x10")
                peer311.sendall(legacy)
                expect_disconnect(peer311)
            finally:
                saved.rename(image)
            observer, _ = connect(address, 5, b"qos1-observer")
            with observer:
                check_retained_value(observer, name, 1, b"old")
    assert image.read_bytes() == before

    with prevent_replacement(image):
        with broker(program, "restore-online", image, exit_code=1) as address:
            with persistent(address, source, False, True) as publisher:
                publisher.sendall(plain)
                expect_disconnect(publisher)
    assert image.read_bytes() == before

    with broker(program, "restore-online", image, "600", crash=True) as address:
        with persistent(address, source, False, True) as publisher:
            observer, _ = connect(address, 5, b"qos1-observer")
            with observer:
                subscribe(observer, name, 1, 0x20)
                subscribe(observer, b"$share/qos1-group/" + name, 2, 0x22)
                publisher.sendall(original + PING)
                expect_silence(publisher)
                expect_silence(observer)
                assert receive(publisher) == success and receive(publisher) == PONG
                received_id = received_publish(observer, 0x32, name, b"new")
                observer.sendall(frame(0x40, received_id))
                assert receive(observer) == (0x30, topic(name) + b"\0new")
    committed = image.read_bytes()
    assert [committed[at] for at, _, _ in session_records(committed)].count(4) == 1
    assert not any(committed[at] == 3 for at, _, _ in session_records(committed))

    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, source, False, True) as publisher:
            # QoS 1 retransmission may produce a duplicate even after a durable PUBACK.
            publisher.sendall(bytes((0x3b,)) + original[1:])
            assert receive(publisher) == success
            publisher.sendall(plain)
            assert receive(publisher) == success
        peer311, _ = connect(address, 4, b"qos1-legacy")
        with peer311:
            peer311.sendall(legacy)
            assert receive(peer311) == success
        observer, _ = connect(address, 5, b"qos1-observer")
        with observer:
            check_retained_value(observer, name, 1, b"new")
        with persistent(address, subscriber, False, True) as recipient:
            for message in (b"new", b"new", b"plain", b"legacy"):
                received_id = received_publish(recipient, 0x32, name, message)
                recipient.sendall(frame(0x40, received_id))
            barrier(recipient)

    saved = image.read_bytes()
    assert not any(saved[at] in (3, 4, 5) for at, _, _ in session_records(saved))
    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, subscriber, False, True) as recipient:
            barrier(recipient)
    with broker(program, "restore-online", image, "2000") as address:
        with persistent(address, source, False, True) as publisher:
            publisher.sendall(plain)
            expect_silence(publisher)
    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, subscriber, False, True) as recipient:
            received_id = received_publish(recipient, 0x32, name, b"plain")
            recipient.sendall(frame(0x40, received_id))
            barrier(recipient)
    return {"online_qos1_commit_before_puback": True, "online_qos1_restart_and_retransmission": True,
            "online_qos1_refusal_and_unknown": True, "online_qos1_mqtt311": True,
            "online_qos1_pending_shutdown": True}


def verify_subscription_transactions(program, directory):
    image = directory / "online-subscriptions.bin"
    client, legacy = b"subscription-owner", b"subscription-legacy"
    old, new = b"edit/old", b"edit/new"
    wild, shared, legacy_topic = b"edit/wild/#", b"edit/shared", b"edit/legacy"
    group1, group2 = b"$share/first/" + shared, b"$share/second/" + shared
    identifier = b"\x00\x41"

    def sub(*items, version=5):
        props = b"\0" if version == 5 else b""
        return frame(0x82, identifier + props + b"".join(topic(name) + bytes((option,)) for name, option in items))

    def unsub(*items, version=5):
        props = b"\0" if version == 5 else b""
        return frame(0xa2, identifier + props + b"".join(map(topic, items)))

    def publish0(peer, name, payload):
        peer.sendall(frame(0x30, topic(name) + b"\0" + payload) + PING)
        assert receive(peer) == PONG

    with broker(program, "capture", image) as address:
        with persistent(address, client, True) as peer:
            subscribe(peer, old, 1, 0x21)
        with persistent(address, legacy, False, version=4) as peer:
            peer.sendall(sub((legacy_topic, 1), version=4))
            assert receive(peer) == (0x90, identifier + b"\x01")

    before = image.read_bytes()
    request = sub((old, 0x22), (new, 0x22), (new, 0x25), (wild, 0x21), (group1, 0x22), (group2, 0x21))
    with broker(program, "restore-online", image, "1400", crash=True) as address:
        with persistent(address, client, False, True) as peer:
            other, _ = connect(address, 5, b"subscription-publisher")
            with other:
                peer.sendall(request + PING)
                expect_silence(peer)
                publish0(other, new, b"staged")
                publish0(other, shared, b"staged")
                expect_silence(peer)
                publish0(other, old, b"existing")
                assert receive(peer) == (0x30, topic(old) + b"\0existing")
    assert image.read_bytes() == before

    with broker(program, "restore-online", image, "450", crash=True) as address:
        with persistent(address, client, False, True) as peer:
            peer.sendall(request + PING)
            expect_silence(peer)
            assert receive(peer) == (0x90, identifier + b"\0\x02\x02\x01\x01\x02\x01")
            assert receive(peer) == PONG

        with persistent(address, legacy, False, True, version=4) as peer:
            peer.sendall(sub((legacy_topic, 2), version=4))
            assert receive(peer) == (0x90, identifier + b"\x02")

    saved = image.read_bytes()
    normal = [saved[at + 1] for at, body, end in session_records(saved)
              if saved[at] == 2 and saved[body:end] == new]
    assert normal == [0x25], "one final subscription per filter"
    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, client, False, True) as peer:
            other, _ = connect(address, 5, b"subscription-publisher")
            with other:
                peer.sendall(frame(0x30, topic(new) + b"\0local") + PING)
                assert receive(peer) == PONG
                publish0(other, b"edit/wild/path", b"wildcard")
                assert receive(peer) == (0x30, topic(b"edit/wild/path") + b"\0wildcard")
                moved = image.with_suffix(".unavailable")
                image.rename(moved)
                try:
                    peer.sendall(sub((b"edit/rejected", 0x21)))
                    assert receive(peer) == (0x90, identifier + b"\0\x83")
                    peer.sendall(unsub(new, b"edit/missing") + PING)
                    assert receive(peer) == (0xb0, identifier + b"\0\x83\x11")
                    assert receive(peer) == PONG
                    publish0(other, b"edit/rejected", b"rolled-back")
                    expect_silence(peer)
                    publish0(other, new, b"removed-only-in-request")
                    assert receive(peer) == (0x30, topic(new) + b"\0removed-only-in-request")
                finally:
                    moved.rename(image)
    assert image.read_bytes() == saved

    with prevent_replacement(image):
        with broker(program, "restore-online", image, exit_code=1) as address:
            with persistent(address, client, False, True) as peer:
                peer.sendall(unsub(new))
                expect_disconnect(peer)
    assert image.read_bytes() == saved

    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, legacy, False, True, version=4) as peer:
            moved = image.with_suffix(".unavailable")
            image.rename(moved)
            try:
                peer.sendall(sub((b"edit/legacy-rejected", 1), version=4))
                assert receive(peer) == (0x90, identifier + b"\x80")
                peer.sendall(unsub(legacy_topic, version=4))
                expect_disconnect(peer)
            finally:
                moved.rename(image)
        with persistent(address, client, False, True) as peer:
            peer.sendall(unsub(new, new, group1) + PING)
            assert receive(peer) == (0xb0, identifier + b"\0\x00\x11\x00")
            assert receive(peer) == PONG

    with broker(program, "restore-online", image, crash=True) as address:
        other, _ = connect(address, 5, b"subscription-publisher")
        with other, persistent(address, client, False, True) as peer, \
                persistent(address, legacy, False, True, version=4) as peer311:
            publish0(other, new, b"removed")
            expect_silence(peer)
            publish0(other, shared, b"one-group-left")
            assert receive(peer) == (0x30, topic(shared) + b"\0one-group-left")
            expect_silence(peer)
            publish0(other, legacy_topic, b"legacy-preserved")
            assert receive(peer311) == (0x30, topic(legacy_topic) + b"legacy-preserved")
            peer311.sendall(unsub(legacy_topic, version=4))
            assert receive(peer311) == (0xb0, identifier)
    with broker(program, "restore-online", image, crash=True) as address:
        other, _ = connect(address, 5, b"subscription-publisher")
        with other, persistent(address, legacy, False, True, version=4) as peer:
            publish0(other, legacy_topic, b"legacy-removed")
            expect_silence(peer)
    takeover = b"edit/takeover"
    with broker(program, "restore-online", image, "600", crash=True) as address:
        with persistent(address, client, False, True) as replaced:
            other, _ = connect(address, 5, b"subscription-publisher")
            with other:
                replaced.sendall(sub((takeover, 0x21)) + PING)
                expect_silence(replaced)
                with persistent(address, client, False, True) as successor:
                    assert receive(replaced) == (0xe0, b"\x8e")
                    publish0(other, takeover, b"successor")
                    assert receive(successor) == (0x30, topic(takeover) + b"\0successor")
    return {"subscription_commit_before_ack": True, "subscription_matching_after_restart": True,
            "subscription_rollback_and_unknown": True, "subscription_mqtt311": True,
            "subscription_pending_takeover": True}


def verify_session_expiry_transactions(program, directory):
    image = directory / "online-expiry.bin"
    mode = "restore-online-expiry"
    first, second, legacy = b"expiry-first", b"expiry-second", b"expiry-legacy"

    def state(client):
        data = image.read_bytes()
        assert data[:8] == b"XMQRET06"
        for at, body, end in session_records(data):
            if data[at] == 1 and data[body:end] == client:
                return (int.from_bytes(data[at + 4:at + 8], "big"),
                        int.from_bytes(data[at + 8:at + 16], "big"),
                        int.from_bytes(data[at + 38:body], "big"))
        return None

    def close_with_interval(peer, expiry, reason=0):
        peer.sendall(frame(0xe0, bytes((reason, 5, 0x11)) + expiry.to_bytes(4, "big")))
        expect_disconnect(peer)

    def wait_file_change(previous):
        deadline = time.monotonic() + 5
        while image.stat().st_mtime_ns == previous:
            assert time.monotonic() < deadline, "session close reached no checkpoint"
            time.sleep(0.02)

    with broker(program, "capture", image):
        pass

    # Keep the socket alive until the process is killed, so no CLOSE can run.
    live = None
    try:
        with broker(program, mode, image, crash=True) as address:
            live = persistent(address, first, True, expiry=2)
            subscribe(live, b"expiry/recovered", 1, 1)
            assert state(first)[:2] == (1, 2000)
            time.sleep(2.3)
            barrier(live)
            assert state(first)[:2] == (1, 2000)
    finally:
        if live is not None:
            live.close()

    connected = state(first)
    with broker(program, mode, image, crash=True):
        offline = state(first)
        assert offline[:2] == (0, 2000) and offline[2] > connected[2]
    time.sleep(2.3)
    with broker(program, mode, image, crash=True) as address:
        with persistent(address, first, False, False, expiry=3) as peer:
            barrier(peer)
            assert state(first)[:2] == (1, 3000)

    # A new live session gets its expiry from the close event, including an
    # MQTT 5 DISCONNECT override. Deliberately delay the backend write.
    with broker(program, mode, image, "600", crash=True) as address:
        with persistent(address, second, True) as peer:
            assert state(second)[:2] == (1, 60000)
            before = image.stat().st_mtime_ns
            close_with_interval(peer, 8)
            wait_file_change(before)
            closed = state(second)
            assert closed[0] == 0 and 6000 < closed[1] < 7700, closed
    with broker(program, mode, image, crash=True) as address:
        with persistent(address, second, False, True, expiry=30) as peer:
            assert state(second)[:2] == (1, 30000)
            before = image.stat().st_mtime_ns
            close_with_interval(peer, 0)
            wait_file_change(before)
            assert state(second) is None
    with broker(program, mode, image, crash=True) as address:
        with persistent(address, second, False, False) as peer:
            barrier(peer)

    with broker(program, mode, image, crash=True) as address:
        with persistent(address, legacy, False, version=4) as peer:
            assert state(legacy)[:2] == (1, (1 << 64) - 1)
            before = image.stat().st_mtime_ns
            peer.sendall(b"\xe0\0")
            expect_disconnect(peer)
            wait_file_change(before)
            assert state(legacy)[:2] == (0, (1 << 64) - 1)
    with broker(program, mode, image, crash=True) as address:
        with persistent(address, legacy, False, True, version=4) as peer:
            barrier(peer)

    with broker(program, mode, image, "275", crash=True) as address:
        with persistent(address, b"expiry-takeover", True, expiry=7) as original:
            with persistent(address, b"expiry-takeover", False, True, expiry=27) as successor:
                assert receive(original) == (0xe0, b"\x8e")
                assert state(b"expiry-takeover")[:2] == (1, 27000)
                barrier(successor)

    # The writer crosses the old session deadline while persisting CLOSE.
    # The arrival time of CONNECT decides whether its successor may resume.
    for early in (True, False):
        client_id = b"expiry-early" if early else b"expiry-late"
        with broker(program, mode, image, "1450", crash=True) as address:
            with persistent(address, client_id, True) as peer:
                close_with_interval(peer, 1)
                if not early:
                    time.sleep(1.15)
                with persistent(address, client_id, False, early, expiry=40) as replacement:
                    assert state(client_id)[:2] == (1, 40000)
                    barrier(replacement)

    # A shorter Session Expiry still drives the in-process delayed Will.
    name = b"expiry/will"
    fields = b"\x05\x18\0\0\0\x0a" + topic(name) + topic(b"at-expiry")
    with broker(program, mode, image, crash=True) as address:
        observer, _ = connect(address, 5, b"expiry-observer")
        with observer, persistent(address, b"expiry-will", True, will=fields) as peer:
            subscribe(observer, name, 1, 0)
            close_with_interval(peer, 1, 4)
            assert receive(observer) == (0x30, topic(name) + b"\0at-expiry")

    # Seal admission if the observed CLOSE cannot be stored.
    with broker(program, mode, image, exit_code=1) as address:
        observer, _ = connect(address, 5, b"expiry-observer")
        with observer, persistent(address, b"expiry-refusal", True) as peer:
            saved = image.with_suffix(".unavailable")
            image.rename(saved)
            try:
                close_with_interval(peer, 0)
                expect_disconnect(observer)
            finally:
                saved.rename(image)
    return {"online_expiry_does_not_age_connected_session": True,
            "online_expiry_recovery_clock_written_once": True,
            "online_expiry_close_override_and_resume": True,
            "online_expiry_mqtt311_and_will_timing": True, "online_expiry_close_failure": True,
            "online_expiry_takeover_order": True, "online_expiry_reconnect_at_deadline": True}


def verify_stored_will_transactions(program, directory):
    image = directory / "stored-will.bin"
    mode = "restore-online-will"
    source, target, name = b"will-source", b"will-recipient", b"will/stored"

    def capture(path, delay, provision=False):
        fields = b"\x05\x18" + delay.to_bytes(4, "big") + topic(name) + topic(b"from-will")
        with broker(program, "capture", path) as address:
            if provision:
                writer, _ = connect(address, 5, b"will-retained-seed")
                with writer:
                    publish(writer, name, b"previous")
                with persistent(address, target, True) as recipient:
                    subscribe(recipient, name, 1, 0x22)
            with persistent(address, source, True, will=fields, will_qos=1) as peer:
                barrier(peer)
            # Let the server park the disconnected source before checkpoint close.
            time.sleep(0.18)
        data = path.read_bytes()
        assert sum(data[at] == 6 for at, _, _ in session_records(data)) == 1
        return data

    def wait_file(path, previous):
        deadline = time.monotonic() + 6
        while path.stat().st_mtime_ns == previous:
            assert time.monotonic() < deadline, "Will did not reach the checkpoint"
            time.sleep(0.02)

    before = capture(image, 3, True)
    time.sleep(3.1)
    with broker(program, mode, image, "1500", crash=True):
        # The first expiry tick submits the due Will to a delayed writer.
        time.sleep(1.25)
    assert image.read_bytes() == before

    rejected = directory / "stored-will-rejected.bin"
    rejected.write_bytes(before)
    with broker(program, mode, rejected, "700", exit_code=1):
        moved = rejected.with_suffix(".unavailable")
        rejected.rename(moved)
        try:
            time.sleep(2.05)
        finally:
            moved.rename(rejected)
    assert rejected.read_bytes() == before

    previous = image.stat().st_mtime_ns
    with broker(program, mode, image, crash=True):
        wait_file(image, previous)
    committed = image.read_bytes()
    kinds = [committed[at] for at, _, _ in session_records(committed)]
    assert 6 not in kinds and kinds.count(4) == 1

    with broker(program, mode, image, crash=True) as address:
        with persistent(address, target, False, True) as recipient:
            identifier = received_publish(recipient, 0x32, name, b"from-will")
            recipient.sendall(frame(0x40, identifier))
            barrier(recipient)
        observer, _ = connect(address, 5, b"will-checker")
        with observer:
            check_retained_value(observer, name, 1, b"from-will")

    with broker(program, mode, image, crash=True) as address:
        with persistent(address, target, False, True) as recipient:
            barrier(recipient)
            expect_silence(recipient)

    cancelled = directory / "stored-will-cancelled.bin"
    capture(cancelled, 5)
    time.sleep(3.1)
    with broker(program, mode, cancelled, "2000", crash=True) as address:
        # Admit RESUME before the deadline and complete it after the deadline.
        with persistent(address, source, False, True) as peer:
            barrier(peer)
            data = cancelled.read_bytes()
            assert not any(data[at] == 6 for at, _, _ in session_records(data))
            assert int.from_bytes(data[16:20], "big") == 0

    replaced = directory / "stored-will-replaced.bin"
    capture(replaced, 10)
    with broker(program, mode, replaced, crash=True) as address:
        previous = replaced.stat().st_mtime_ns
        with socket.create_connection(address, timeout=8) as peer:
            peer.sendall(persistent_connect(source, True))
            expect_connack(peer, reason=0x89)
        wait_file(replaced, previous)
        data = replaced.read_bytes()
        assert not any(data[at] == 1 and data[body:end] == source for at, body, end in session_records(data))
        with persistent(address, source, True) as peer:
            barrier(peer)
    return {"saved_will_recovers_after_pending_write": True,
            "saved_will_commits_retained_and_queued_delivery": True,
            "saved_will_backend_refusal_preserves_record": True,
            "saved_will_fenced_resume_and_clean_start": True}


def check_retained_value(peer, name, identifier, value, expiry=False):
    subscribe(peer, name, identifier, 0)
    kind, body = receive(peer)
    assert kind == 0x31 and body.startswith(topic(name)) and body.endswith(value)
    header = body[len(topic(name)):-len(value)]
    if expiry:
        assert header[:2] == b"\5\x02" and len(header) == 6
        assert 1 <= int.from_bytes(header[2:], "big") <= 120
    else:
        assert header == b"\0"


def verify_retained_transactions(program, directory):
    image = directory / "online-retained-qos2.bin"
    source, target = b"retain-source", b"retain-target"
    name, identifier = b"atomic/retained", b"\0\x35"
    def wire(payload, expires, packet_id=identifier):
        properties = b"\5\x02" + expires.to_bytes(4, "big") if expires is not None else b"\0"
        return frame(0x35, topic(name) + packet_id + properties + payload)

    with broker(program, "capture", image) as address:
        owner, _ = connect(address, 5, b"old-retained-owner")
        with owner:
            publish(owner, name, b"old")
        with persistent(address, source, True) as publisher:
            barrier(publisher)
        with persistent(address, target, True) as recipient:
            subscribe(recipient, name, 1, 0x22)
    original = image.read_bytes()
    replacement = wire(b"new", 120)
    pubrec = (0x50, identifier)

    with broker(program, "restore-online", image, "1500", crash=True) as address:
        with persistent(address, source, False, True) as publisher:
            observer, _ = connect(address, 5, b"retained-observer")
            with observer:
                publisher.sendall(replacement)
                expect_silence(publisher)
                check_retained_value(observer, name, 1, b"old")
                observer.sendall(frame(0x33, topic(name) + b"\0\x44\0competing"))
                assert receive(observer) == (0x40, b"\0\x44\x97")
    assert image.read_bytes() == original

    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, source, False, True) as publisher:
            observer, _ = connect(address, 5, b"retained-observer")
            with observer:
                moved = image.with_suffix(".unavailable")
                image.rename(moved)
                publisher.sendall(replacement)
                assert receive(publisher) == (0x50, identifier + b"\x83")
                check_retained_value(observer, name, 1, b"old")
                moved.rename(image)
    assert image.read_bytes() == original

    with prevent_replacement(image):
        with broker(program, "restore-online", image, exit_code=1) as address:
            with persistent(address, source, False, True) as publisher:
                publisher.sendall(replacement)
                expect_disconnect(publisher)
    assert image.read_bytes() == original

    with broker(program, "restore-online", image, "600", crash=True) as address:
        with persistent(address, source, False, True) as publisher:
            observer, _ = connect(address, 5, b"retained-observer")
            with observer:
                subscribe(observer, name, 1, 0x20)
                publisher.sendall(replacement + PING)
                expect_silence(observer)
                assert receive(publisher) == pubrec and receive(publisher) == PONG
                assert image.read_bytes() != original
                kind, body = receive(observer)
                assert kind == 0x30 and body.startswith(topic(name)) and body.endswith(b"new")

    with broker(program, "restore-online", image, crash=True) as address:
        with persistent(address, source, False, True) as publisher:
            publisher.sendall(bytes((0x3d,)) + replacement[1:])
            assert receive(publisher) == pubrec
            with persistent(address, target, False, True) as recipient:
                kind, body = receive(recipient)
                assert kind == 0x34 and body.startswith(topic(name)) and body.endswith(b"new")
                at = len(topic(name))
                delivered = body[at:at + 2]
                assert body[at + 2:at + 4] == b"\5\x02"
                recipient.sendall(frame(0x50, delivered))
                assert receive(recipient) == (0x62, delivered)
                recipient.sendall(frame(0x70, delivered))
                barrier(recipient)
            observer, _ = connect(address, 5, b"retained-observer")
            with observer:
                check_retained_value(observer, name, 1, b"new", expiry=True)
                publisher.sendall(frame(0x62, identifier))
                assert receive(publisher) == (0x70, identifier)
                publisher.sendall(wire(b"", None, b"\0\x36"))
                assert receive(publisher) == (0x50, b"\0\x36")
                assert receive(observer) == (0x30, topic(name) + b"\0")

    legacy_name = b"atomic/retained-311"
    with broker(program, "restore-online", image, crash=True) as address:
        observer, _ = connect(address, 5, b"retained-observer")
        with observer:
            subscribe(observer, name, 1, 0)
            barrier(observer)
        old_client, _ = connect(address, 4, b"retained-legacy")
        with old_client:
            previous = image.read_bytes()
            old_client.sendall(frame(0x31, topic(legacy_name) + b"standalone") + PING)
            assert receive(old_client) == PONG and image.read_bytes() != previous
            old_client.sendall(frame(0x35, topic(legacy_name) + identifier + b"legacy"))
            assert receive(old_client) == pubrec
    with broker(program, "restore-online", image, crash=True) as address:
        observer, _ = connect(address, 5, b"retained-observer")
        with observer:
            check_retained_value(observer, legacy_name, 1, b"legacy")

    image.write_bytes(original)
    with broker(program, "restore-online", image, "1400", crash=True) as address:
        with persistent(address, source, False, True) as publisher:
            observer, _ = connect(address, 5, b"retained-observer")
            with observer:
                subscribe(observer, name, 1, 0x20)
                publisher.sendall(wire(b"expires-during-commit", 1))
                assert receive(publisher) == pubrec
                barrier(observer)
    with broker(program, "restore-online", image, crash=True) as address:
        observer, _ = connect(address, 5, b"retained-observer")
        with observer:
            subscribe(observer, name, 1, 0)
            barrier(observer)
    return {"retained_atomic_recovery": ["5.0", "3.1.1"], "retained_rejection_preserves_old_value": True,
            "retained_writes_share_shard_lease": True, "retained_combined_unknown_seals": True,
            "retained_combined_tombstone_and_expiry": True}


def verify_full_writer(program, directory):
    image = directory / "retained-full-writer.bin"
    peers = [f"transaction-writer-{i}".encode() for i in range(22)]
    with broker(program, "capture", image) as address:
        for client_id in peers:
            with persistent(address, client_id, True) as peer:
                barrier(peer)
    original = image.read_bytes()
    identifier = b"\0\x45"
    with broker(program, "restore-online", image, "2000", crash=True) as address:
        with ExitStack() as owners:
            connected = [owners.enter_context(persistent(address, client_id, False, True)) for client_id in peers]
            for peer in connected[:-1]:
                peer.sendall(frame(0x34, topic(b"writer/fill") + identifier + b"\0pending"))
            readable, _, _ = select(connected[:-1], [], [], 1.0)
            assert readable, "the writer queue did not reach its bound"
            assert receive(readable[0]) == (0x50, identifier + b"\x83")
            for publish_header, ack_header in ((0x35, 0x50), (0x33, 0x40), (0x33, 0x40)):
                candidate = frame(publish_header, topic(b"writer/rejected") + identifier + b"\0retained")
                connected[-1].sendall(candidate + PING)
                assert receive(connected[-1]) == (ack_header, identifier + b"\x83")
                assert receive(connected[-1]) == PONG
    assert image.read_bytes() == original
    return {"full_writer_preserves_input_order_and_releases_retained_lease": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="xmqtt-retained-") as directory:
        folder = Path(directory)
        print(json.dumps({**verify(args.program, folder), **verify_sessions(args.program, folder),
                          **verify_reset(args.program, folder), **verify_pubrel_commit(args.program, folder),
                          **verify_online_transactions(args.program, folder),
                          **verify_outbound_transactions(args.program, folder),
                          **verify_qos1_transactions(args.program, folder),
                          **verify_subscription_transactions(args.program, folder),
                          **verify_session_expiry_transactions(args.program, folder),
                          **verify_stored_will_transactions(args.program, folder),
                          **verify_retained_transactions(args.program, folder),
                          **verify_full_writer(args.program, folder)}))


if __name__ == "__main__":
    main()
