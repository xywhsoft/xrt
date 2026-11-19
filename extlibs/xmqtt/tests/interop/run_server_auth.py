#!/usr/bin/env python3
"""Exercise broker authentication with an independent MQTT peer and HMAC implementation."""
import argparse
from contextlib import contextmanager
import hashlib
import hmac
import json
import os
from pathlib import Path
import queue
import secrets
import socket
import subprocess
from threading import Thread
import time

from run_server_handshake import topic
from run_server_tls import frame, receive


METHOD = b"example-hmac-sha256"
PING = b"\xc0\0"
PONG = (0xd0, b"")
FILTER = topic(b"auth/retained-session")


def properties(method=METHOD, data=None):
    fields = b"" if method is None else b"\x15" + topic(method)
    return fields if data is None else fields + b"\x16" + topic(data)


def connect_wire(name, method=METHOD, data=None, *, clean=True, limit=None):
    fields = b"\x11\0\0\0\x3c" + properties(method, data)
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
    sock.sendall(auth(0x18, mac(key, b"C", nonce, name)))
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
    assert not sock.recv(1)


def connect(address):
    return socket.create_connection(address, timeout=6)


def closed(sock):
    try:
        assert not sock.recv(1)
    except ConnectionResetError:
        pass


def verify(address, key):
    name = b"auth-session"
    with connect(address) as owner:
        wire = connect_wire(name)
        owner.sendall(wire[:9])
        owner.sendall(wire[9:])
        accept(owner, key, name, expect_challenge(owner))
        owner.sendall(frame(0x82, b"\0\1\0" + FILTER + b"\1"))
        assert receive(owner) == (0x90, b"\0\1\0\1")

        with connect(address) as intruder:
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
        owner.sendall(frame(0x32, FILTER + b"\0\x07\0prior authority"))
        frames = [receive(owner), receive(owner)]
        assert (0x40, b"\0\x07") in frames
        forwards = [body for kind, body in frames if kind == 0x32]
        assert len(forwards) == 1 and forwards[0].endswith(b"\0prior authority")
        start = len(FILTER)
        owner.sendall(frame(0x40, forwards[0][start:start + 2]))
        assert receive_after_proof(owner, key, name, nonce) == PONG

        with connect(address) as successor:
            successor.sendall(connect_wire(name, clean=False))
            nonce = expect_challenge(successor)
            owner.sendall(PING)
            assert receive(owner) == PONG
            final = prove(successor, key, name, nonce)
            refused(owner, 0x8e, 0xe0)
            kind, body = receive(successor)
            assert kind == 0x20 and body[:2] == b"\1\0" and body.endswith(final)
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
        with connect(address) as peer:
            peer.sendall(connect_wire(label))
            expect_challenge(peer)
            peer.sendall(continuation)
            refused(peer, 0x82)
    with connect(address) as peer:
        peer.sendall(connect_wire(b"missing-method"))
        expect_challenge(peer)
        peer.sendall(auth(0x18, method=None))
        closed(peer)
    with connect(address) as peer:
        peer.sendall(connect_wire(b"unsupported", b"no-such-method"))
        refused(peer, 0x8c)
    with connect(address) as peer:
        peer.sendall(connect_wire(b"small-packet", limit=40))
        refused(peer, 0x95)
    with connect(address) as peer:
        peer.sendall(connect_wire(b"empty-data", data=b""))
        refused(peer, 0x87)
    with connect(address) as peer:
        peer.sendall(connect_wire(b"no-method", method=None))
        assert receive(peer)[1][:2] == b"\0\0"
        peer.sendall(auth(0x19))
        refused(peer, 0x82, 0xe0)
    with connect(address) as peer:
        peer.sendall(connect_wire(b"disconnect-during-auth"))
        expect_challenge(peer)
        peer.sendall(b"\xe0\0")
        assert not peer.recv(1)
    with connect(address) as peer:
        peer.sendall(connect_wire(b"initial-timeout"))
        expect_challenge(peer)
        closed(peer)
    with connect(address) as peer:
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
                if not peer.recv(1, socket.MSG_PEEK):
                    break
                assert receive(peer) == PONG
            except (BrokenPipeError, ConnectionResetError):
                break
            assert time.monotonic() - start < 3
        assert 0.5 < time.monotonic() - start < 3


def receive_after_proof(sock, key, name, nonce):
    final = prove(sock, key, name, nonce)
    assert receive(sock) == (0xf0, b"\0" + bytes((len(final),)) + final)
    sock.sendall(PING)
    return receive(sock)


@contextmanager
def broker(program, key, *limits):
    env = {**os.environ, "XMQTT_EXAMPLE_AUTH_KEY": key.decode("ascii")}
    with subprocess.Popen([str(program.resolve()), *limits], env=env, stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, text=True) as process:
        lines = queue.Queue()
        Thread(target=lambda: lines.put(process.stdout.readline()), daemon=True).start()
        try:
            host, port = lines.get(timeout=10).strip().rsplit(":", 1)
            yield host, int(port)
        finally:
            if process.poll() is None:
                process.stdin.write("\n")
                process.stdin.flush()
            try:
                assert process.wait(timeout=10) == 0, "broker did not retire all exchanges"
            except subprocess.TimeoutExpired:
                process.kill()
                raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    args = parser.parse_args()
    key = secrets.token_hex(32).encode("ascii")
    unfinished = None
    try:
        with broker(args.program, key) as address:
            verify(address, key)
            unfinished = connect(address)
            unfinished.sendall(connect_wire(b"shutdown"))
            expect_challenge(unfinished)
        closed(unfinished)
    finally:
        if unfinished is not None:
            unfinished.close()
    with broker(args.program, key, "1", "4096") as address, connect(address) as peer:
        peer.sendall(connect_wire(b"rounds"))
        nonce = expect_challenge(peer)
        peer.sendall(auth(0x18, mac(key, b"C", nonce, b"rounds")))
        refused(peer, 0x97)
    with broker(args.program, key, "32", "64") as address, connect(address) as peer:
        peer.sendall(connect_wire(b"aggregate"))
        refused(peer, 0x97)
    print(json.dumps({"broker_auth": "pass", "rounds": 2, "method": METHOD.decode("ascii")}))


if __name__ == "__main__":
    main()
