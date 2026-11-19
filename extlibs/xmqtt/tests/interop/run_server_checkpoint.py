#!/usr/bin/env python3
"""Verify restored retained messages against a new broker process."""
import argparse
from contextlib import contextmanager
import json
import os
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
    assert stored[:8] == b"XMQRET03" and int.from_bytes(stored[16:20], "big") == 4
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
    version_two = b"XMQRET02" + stored[8:offset] + b"\0\0\0\0"
    previous_path.write_bytes(version_two + zlib.crc32(version_two).to_bytes(4, "big"))
    with broker(program, "restore", previous_path) as address:
        assert verify_restored(address) <= first_expiry
    bad = directory / "invalid.bin"
    rejected_image(program, bad, stored[:-1])
    rejected_image(program, bad, stored[:-1] + bytes((stored[-1] ^ 1,)))
    future = stored[:8] + ((1 << 63) - 1).to_bytes(8, "big") + stored[16:-4]
    rejected_image(program, bad, future + zlib.crc32(future).to_bytes(4, "big"))
    return {"retained_restart": "pass", "source_versions": ["5.0", "3.1.1"],
            "finite_expiry_and_no_local": True, "corruption_and_clock_rejection": True}


def persistent_connect(client_id, clean, *, version=5, will=b""):
    fields = b"\5\x11\0\0\0\x3c" if version == 5 else b""
    flags = (2 if clean else 0) | (0x24 if will else 0)
    return frame(0x10, b"\0\4MQTT" + bytes((version, flags, 0, 0)) + fields + topic(client_id) + will)


def expect_connack(peer, present=False, reason=0):
    kind, body = receive(peer)
    assert kind == 0x20 and body[:2] == bytes((present, reason)), (kind, body)


def persistent(address, client_id, clean, present=False, *, version=5, will=b""):
    peer = socket.create_connection(address, timeout=8)
    peer.sendall(persistent_connect(client_id, clean, version=version, will=will))
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
    assert data[:8] == b"XMQRET03" and data[16:20] == b"\0" * 4
    at = 24
    for _ in range(int.from_bytes(data[20:24], "big")):
        body = at + {1: 46, 4: 30}.get(data[at], 22)
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
            "transaction_rejection_is_invisible": True, "transaction_unknown_seals": True,
            "fresh_session_fences_prior_recipient": True, "volatile_source_durable_fanout": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="xmqtt-retained-") as directory:
        folder = Path(directory)
        print(json.dumps({**verify(args.program, folder), **verify_sessions(args.program, folder),
                          **verify_reset(args.program, folder), **verify_pubrel_commit(args.program, folder),
                          **verify_online_transactions(args.program, folder)}))


if __name__ == "__main__":
    main()
