#!/usr/bin/env python3
"""Exercise broker authentication with an independent MQTT peer and HMAC implementation."""
import argparse
from contextlib import ExitStack, contextmanager
import hashlib
import hmac
import json
import os
from pathlib import Path
import queue
import secrets
import socket
import ssl
import subprocess
import tempfile
from threading import Thread
import time

from run_server_checkpoint import (barrier, expect_silence, persistent_connect, received_publish,
                                   session_records, subscribe)
from run_server_handshake import topic
from run_server_tls import frame, receive
from run_server_ws import WsContext, WsPeer


METHOD = b"example-hmac-sha256"
PING = b"\xc0\0"
PONG = (0xd0, b"")
FILTER = topic(b"auth/retained-session")


def properties(method=METHOD, data=None):
    fields = b"" if method is None else b"\x15" + topic(method)
    return fields if data is None else fields + b"\x16" + topic(data)


def connect_wire(name, method=METHOD, data=None, *, clean=True, limit=None, receive_maximum=None):
    fields = b"\x11\0\0\0\x3c" + properties(method, data)
    if receive_maximum is not None:
        fields += b"\x21" + receive_maximum.to_bytes(2, "big")
    if limit is not None:
        fields += b"\x27" + limit.to_bytes(4, "big")
    return frame(0x10, b"\0\4MQTT\5" + bytes((2 if clean else 0, 0, 1, len(fields))) +
                 fields + topic(name))


def auth(reason, data=None, method=METHOD):
    fields = properties(method, data)
    return frame(0xf0, bytes((reason, len(fields))) + fields)


def expect_challenge(sock):
    kind, body = receive(sock)
    assert kind == 0xf0 and body[0] == 0x18, (kind, body.hex())
    assert body[1] == len(body) - 2
    nonce = body[-32:]
    assert body[2:] == properties(data=nonce)
    return nonce


def mac(key, role, nonce, name):
    return hmac.digest(key, role + nonce + name, hashlib.sha256)


def prove(sock, key, name, nonce):
    wire = auth(0x18, mac(key, b"C", nonce, name))
    sock.sendall(wire[:7])
    sock.sendall(wire[7:])
    next_nonce = expect_challenge(sock)
    assert next_nonce != nonce
    sock.sendall(auth(0x18, mac(key, b"C", next_nonce, name)))
    return properties(data=mac(key, b"S", next_nonce, name))


def accept(sock, key, name, nonce, *, resumed=False):
    final = prove(sock, key, name, nonce)
    kind, body = receive(sock)
    assert kind == 0x20 and body[:2] == bytes((resumed, 0)), (kind, body.hex())
    assert body[2] == len(body) - 3 and body.endswith(final)


def refused(sock, reason, kind=0x20):
    received_kind, body = receive(sock)
    expected = bytes((0, reason, 0)) if kind == 0x20 else bytes((reason,))
    assert (received_kind, body) == (kind, expected), (received_kind, body.hex(), expected.hex())
    graceful_close(sock)


def connect(address, context):
    sock = socket.create_connection(address, timeout=8)
    if context is None:
        return sock
    try:
        return context.wrap_socket(sock, server_hostname="localhost")
    except Exception:
        sock.close()
        raise


def graceful_close(sock):
    if isinstance(sock, (ssl.SSLSocket, WsPeer)):
        sock.unwrap().close()
        if isinstance(sock, WsPeer):
            assert sock.close_reply == b"\x03\xe8"
    else:
        assert not sock.recv(1)


def closed(sock):
    try:
        assert not sock.recv(1)
    except (ConnectionResetError, ssl.SSLEOFError):
        pass


def publish_to_self(sock, identifier, payload):
    sock.sendall(frame(0x32, FILTER + identifier + b"\0" + payload))
    frames = [receive(sock), receive(sock)]
    assert (0x40, identifier) in frames
    forwards = [body for kind, body in frames if kind == 0x32]
    assert len(forwards) == 1
    body = forwards[0]
    assert body.startswith(FILTER) and body[len(FILTER) + 2:] == b"\0" + payload
    return body


def verify(open_peer, key):
    name = b"auth-session"
    with open_peer() as owner:
        wire = connect_wire(name)
        owner.sendall(wire[:9])
        owner.sendall(wire[9:])
        accept(owner, key, name, expect_challenge(owner))
        owner.sendall(frame(0x82, b"\0\1\0" + FILTER + b"\1"))
        assert receive(owner) == (0x90, b"\0\1\0\1")

        with open_peer() as intruder:
            intruder.sendall(connect_wire(name))
            expect_challenge(intruder)
            owner.sendall(PING)
            assert receive(owner) == PONG
            intruder.sendall(auth(0x18, bytes(32)))
            refused(intruder, 0x87)
            owner.sendall(PING)
            assert receive(owner) == PONG

        owner.sendall(auth(0x19) + PING)
        nonce = expect_challenge(owner)
        assert receive(owner) == PONG
        start = len(FILTER)
        delivered = publish_to_self(owner, b"\0\x07", b"prior authority")
        owner.sendall(frame(0x40, delivered[start:start + 2]))
        assert receive_after_proof(owner, key, name, nonce) == PONG
        pending = publish_to_self(owner, b"\0\x08", b"replay after proof")

        with open_peer() as successor:
            successor.sendall(connect_wire(name, clean=False))
            nonce = expect_challenge(successor)
            owner.sendall(PING)
            assert receive(owner) == PONG
            final = prove(successor, key, name, nonce)
            refused(owner, 0x8e, 0xe0)
            kind, body = receive(successor)
            assert kind == 0x20 and body[:2] == b"\1\0" and body.endswith(final)
            assert receive(successor) == (0x3a, pending)
            successor.sendall(frame(0x40, pending[start:start + 2]))
            successor.sendall(auth(0x19))
            expect_challenge(successor)
            successor.sendall(auth(0x18, bytes(32)))
            refused(successor, 0x87, 0xe0)

    for label, continuation in (
        (b"wrong-method", auth(0x18, method=b"other")),
        (b"unexpected-restart", auth(0x19)),
        (b"second-connect", connect_wire(b"duplicate")),
        (b"early-ping", PING),
    ):
        with open_peer() as peer:
            peer.sendall(connect_wire(label))
            expect_challenge(peer)
            peer.sendall(continuation)
            refused(peer, 0x82)
    with open_peer() as peer:
        peer.sendall(connect_wire(b"missing-method"))
        expect_challenge(peer)
        peer.sendall(auth(0x18, method=None))
        closed(peer)
    with open_peer() as peer:
        peer.sendall(connect_wire(b"unsupported", b"no-such-method"))
        refused(peer, 0x8c)
    with open_peer() as peer:
        peer.sendall(connect_wire(b"small-packet", limit=40))
        refused(peer, 0x95)
    with open_peer() as peer:
        peer.sendall(connect_wire(b"empty-data", data=b""))
        refused(peer, 0x87)
    with open_peer() as peer:
        peer.sendall(connect_wire(b"no-method", method=None))
        assert receive(peer)[1][:2] == b"\0\0"
        peer.sendall(auth(0x19))
        refused(peer, 0x82, 0xe0)
    with open_peer() as peer:
        peer.sendall(connect_wire(b"disconnect-during-auth"))
        expect_challenge(peer)
        peer.sendall(b"\xe0\0")
        graceful_close(peer)
    with open_peer() as peer:
        peer.sendall(connect_wire(b"initial-timeout"))
        expect_challenge(peer)
        closed(peer)
    with open_peer() as peer:
        peer.sendall(connect_wire(b"reauth-timeout"))
        accept(peer, key, b"reauth-timeout", expect_challenge(peer))
        peer.sendall(auth(0x19))
        expect_challenge(peer)
        start = time.monotonic()
        # PING refreshes the MQTT keepalive, but cannot refresh the auth deadline.
        while True:
            time.sleep(0.15)
            try:
                peer.sendall(PING)
                first = peer.recv(1)
                if not first:
                    break
                assert first == b"\xd0" and peer.recv(1) == b"\0"
            except (BrokenPipeError, ConnectionResetError, ssl.SSLEOFError):
                break
            assert time.monotonic() - start < 3
        assert 0.5 < time.monotonic() - start < 3


def receive_after_proof(sock, key, name, nonce):
    final = prove(sock, key, name, nonce)
    assert receive(sock) == (0xf0, b"\0" + bytes((len(final),)) + final)
    sock.sendall(PING)
    return receive(sock)


@contextmanager
def broker(program, key, mode, credentials, *arguments, require_method=False, crash=False):
    command = [str(program.resolve())]
    if require_method:
        command.append("--require-method")
    command.append(mode)
    tls = None
    if mode in ("tls", "wss"):
        tls = ssl.create_default_context(cafile=str(credentials / "certificate.pem"))
        tls.minimum_version = ssl.TLSVersion.TLSv1_3
        command += [str(credentials / "certificate.der"), str(credentials / "key.der")]
    context = WsContext(tls) if mode in ("ws", "wss") else tls
    env = {**os.environ, "XMQTT_EXAMPLE_AUTH_KEY": key.decode("ascii")}
    with subprocess.Popen(command + list(arguments), env=env, stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, text=True) as process:
        lines = queue.Queue()
        Thread(target=lambda: lines.put(process.stdout.readline()), daemon=True).start()
        try:
            host, port = lines.get(timeout=10).strip().rsplit(":", 1)
            assert host == "127.0.0.1"
            address = host, int(port)
            yield lambda: connect(address, context)
        finally:
            if crash:
                assert process.poll() is None, "broker exited before the planned process stop"
                process.kill()
                process.wait(timeout=10)
            else:
                if process.poll() is None:
                    process.stdin.write("\n")
                    process.stdin.flush()
                try:
                    assert process.wait(timeout=10) == 0, "broker did not retire all exchanges"
                except subprocess.TimeoutExpired:
                    process.kill()
                    raise


def probe(program, credentials, mode):
    key = secrets.token_hex(32).encode("ascii")
    command = program, key, mode, credentials
    unfinished = None
    try:
        with broker(*command) as open_peer:
            verify(open_peer, key)
            unfinished = open_peer()
            unfinished.sendall(connect_wire(b"shutdown"))
            expect_challenge(unfinished)
        closed(unfinished)
    finally:
        if unfinished is not None:
            unfinished.close()
    with broker(*command, "1", "4096") as open_peer, open_peer() as peer:
        peer.sendall(connect_wire(b"rounds"))
        nonce = expect_challenge(peer)
        peer.sendall(auth(0x18, mac(key, b"C", nonce, b"rounds")))
        refused(peer, 0x97)
    with broker(*command, "32", "64") as open_peer, open_peer() as peer:
        peer.sendall(connect_wire(b"aggregate"))
        refused(peer, 0x97)
    with broker(*command, require_method=True) as open_peer:
        name = b"required"
        with open_peer() as peer:
            peer.sendall(connect_wire(name, method=None))
            refused(peer, 0x87)
        with open_peer() as peer:
            peer.sendall(connect_wire(name))
            accept(peer, key, name, expect_challenge(peer))
            peer.sendall(PING)
            assert receive(peer) == PONG
    return {"broker_auth": "pass", "rounds": 2, "method": METHOD.decode("ascii"), "required_method": True}


def verify_checkpoint(program):
    key = secrets.token_hex(32).encode("ascii")
    name = b"auth-checkpoint"
    with tempfile.TemporaryDirectory(prefix="xmqtt-auth-checkpoint-") as directory:
        image = str(Path(directory) / "broker.bin")
        with broker(program, key, "capture", None, image, require_method=True) as open_peer:
            with open_peer() as peer:
                peer.sendall(connect_wire(name))
                accept(peer, key, name, expect_challenge(peer))
                peer.sendall(frame(0x82, b"\0\1\0" + FILTER + b"\1"))
                assert receive(peer) == (0x90, b"\0\1\0\1")
                pending = publish_to_self(peer, b"\0\x09", b"checkpoint\0delivery")
                peer.sendall(b"\xe0\0")
                graceful_close(peer)
        assert Path(image).is_file()
        with broker(program, key, "restore", None, image, require_method=True) as open_peer:
            with open_peer() as peer:
                peer.sendall(connect_wire(name, method=None, clean=False))
                refused(peer, 0x87)
            with open_peer() as peer:
                peer.sendall(persistent_connect(name, False, version=4))
                assert receive(peer) == (0x20, b"\0\x05")
                graceful_close(peer)
            with open_peer() as peer:
                peer.sendall(connect_wire(name, clean=False))
                accept(peer, key, name, expect_challenge(peer), resumed=True)
                assert receive(peer) == (0x3a, pending)
                peer.sendall(frame(0x40, pending[len(FILTER):len(FILTER) + 2]) + PING)
                assert receive(peer) == PONG
                peer.sendall(b"\xe0\0")
                graceful_close(peer)
        with broker(program, key, "restore", None, image, require_method=True) as open_peer, open_peer() as peer:
            peer.sendall(connect_wire(name, clean=False))
            accept(peer, key, name, expect_challenge(peer), resumed=True)
            peer.sendall(PING)
            assert receive(peer) == PONG
    return {"proved_before_replay": True, "original_qos1_id": True, "acked_delivery_retired_after_restart": True}


def verify_crash_checkpoint(program):
    key = secrets.token_hex(32).encode("ascii")
    publisher, subscriber, name = b"auth-crash-source", b"auth-crash-reader", b"auth/crash"
    messages = [(2 if index % 2 == 0 else 1, b"recover\0" + index.to_bytes(2, "big") + b"\xff")
                for index in range(17)]

    def authenticate(peer, identity, *, resumed=False, window=None, quiet=False):
        peer.sendall(connect_wire(identity, clean=not resumed, receive_maximum=window))
        nonce = expect_challenge(peer)
        if quiet:
            expect_silence(peer)
        accept(peer, key, identity, nonce, resumed=resumed)

    def finish_qos2(peer, identifier):
        peer.sendall(frame(0x50, identifier))
        assert receive(peer) == (0x62, identifier)
        peer.sendall(frame(0x70, identifier))

    def durable_records(image):
        data = image.read_bytes()
        return [(data[at], data[at + 1], data[at + 2:at + 4]) for at, _, _ in session_records(data)]

    with tempfile.TemporaryDirectory(prefix="xmqtt-auth-crash-") as directory:
        image = Path(directory) / "broker.bin"
        with broker(program, key, "capture", None, str(image), require_method=True) as open_peer:
            for identity in (publisher, subscriber):
                with open_peer() as peer:
                    authenticate(peer, identity)
                    if identity == subscriber:
                        subscribe(peer, name, 1, 2)
                    peer.sendall(b"\xe0\0")
                    graceful_close(peer)

        arguments = program, key, "restore-online", None, str(image)
        with ExitStack() as sockets, broker(*arguments, require_method=True, crash=True) as open_peer:
            source = sockets.enter_context(open_peer())
            authenticate(source, publisher, resumed=True)
            for index, (qos, payload) in enumerate(messages, 1):
                identifier = index.to_bytes(2, "big")
                wire = topic(name) + identifier + b"\0" + payload
                source.sendall(frame(0x30 | qos << 1, wire))
                assert receive(source) == (0x50 if qos == 2 else 0x40, identifier)
                if qos == 2 and index < len(messages):
                    source.sendall(frame(0x62, identifier))
                    assert receive(source) == (0x70, identifier)
            pending_id, pending_wire = identifier, wire

        records = durable_records(image)
        assert sum(kind == 3 for kind, _, _ in records) == 1
        assert sum(kind == 4 for kind, _, _ in records) == len(messages)
        with ExitStack() as sockets, broker(*arguments, require_method=True, crash=True) as open_peer:
            before_auth = image.read_bytes()
            with open_peer() as unauthenticated:
                unauthenticated.sendall(connect_wire(subscriber, method=None, clean=False))
                refused(unauthenticated, 0x87)
            with open_peer() as rejected:
                rejected.sendall(connect_wire(subscriber, clean=False))
                nonce = expect_challenge(rejected)
                proof = bytearray(mac(key, b"C", nonce, subscriber))
                proof[0] ^= 1
                rejected.sendall(auth(0x18, bytes(proof)))
                refused(rejected, 0x87)
            assert image.read_bytes() == before_auth

            with open_peer() as source:
                authenticate(source, publisher, resumed=True)
                source.sendall(frame(0x3c, pending_wire))
                assert receive(source) == (0x50, pending_id)
                source.sendall(frame(0x62, pending_id))
                assert receive(source) == (0x70, pending_id)

            recipient = sockets.enter_context(open_peer())
            authenticate(recipient, subscriber, resumed=True, window=1, quiet=True)
            qos2_id = received_publish(recipient, 0x34, name, messages[0][1])
            barrier(recipient)
            recipient.sendall(frame(0x50, qos2_id))
            assert receive(recipient) == (0x62, qos2_id)

        assert (5, 7, qos2_id) in durable_records(image)
        with ExitStack() as sockets, broker(*arguments, require_method=True, crash=True) as open_peer:
            recipient = sockets.enter_context(open_peer())
            authenticate(recipient, subscriber, resumed=True, window=1)
            assert receive(recipient) == (0x62, qos2_id)
            recipient.sendall(frame(0x70, qos2_id))
            qos1_id = received_publish(recipient, 0x32, name, messages[1][1])
            # The next PUBLISH may already be on the wire when PUBCOMP
            # arrives. Fence retirement before stopping the process again.
            barrier(recipient)

        assert (5, 7, qos2_id) not in durable_records(image)
        with ExitStack() as sockets, broker(*arguments, require_method=True, crash=True) as open_peer:
            recipient = sockets.enter_context(open_peer())
            authenticate(recipient, subscriber, resumed=True, window=1)
            assert received_publish(recipient, 0x3a, name, messages[1][1]) == qos1_id
            recipient.sendall(frame(0x40, qos1_id))
            for qos, payload in messages[2:]:
                identifier = received_publish(recipient, 0x30 | qos << 1, name, payload)
                assert identifier != b"\0\0"
                if qos == 2:
                    finish_qos2(recipient, identifier)
                else:
                    recipient.sendall(frame(0x40, identifier))
            barrier(recipient)

        assert not any(kind in (3, 4, 5) for kind, _, _ in durable_records(image))
        with broker(*arguments, require_method=True) as open_peer, open_peer() as recipient:
            authenticate(recipient, subscriber, resumed=True, window=1)
            barrier(recipient)

    return {"process_stops": 4, "unique_qos1_deliveries": 8, "unique_qos2_deliveries": 9,
            "authentication_gates_replay": True, "inbound_qos2_duplicate_suppressed": True,
            "original_ids_on_pubrel_and_dup": True, "acknowledged_deliveries_retired": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("credentials", type=Path, nargs="?")
    parser.add_argument("--checkpoint", type=Path, help="checkpoint example built with server auth")
    args = parser.parse_args()
    modes = ("tcp", "tls", "ws", "wss") if args.credentials else ("tcp",)
    result = {mode: probe(args.program, args.credentials, mode) for mode in modes}
    if args.checkpoint:
        result["checkpoint"] = verify_checkpoint(args.checkpoint)
        result["crash_checkpoint"] = verify_crash_checkpoint(args.checkpoint)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
