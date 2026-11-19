#!/usr/bin/env python3
"""Verify durable retained acknowledgments against an independent MQTT peer."""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import queue
import socket
import ssl
import subprocess
import tempfile
from threading import Thread

from run_server_checkpoint import barrier, subscribe
from run_server_handshake import connect, topic
from run_server_tls import frame, receive
from run_server_ws import WsContext, WsPeer


NAME = b"durable/retained"
PING = b"\xc0\0"
PONG = (0xd0, b"")


@contextmanager
def broker(program, mode, path, delay=0, transport="tcp", credentials=None):
    command = [str(program.resolve()), mode, str(path), str(delay), transport]
    if transport in ("tls", "wss"):
        command += [str(credentials / "certificate.der"), str(credentials / "key.der")]
    with subprocess.Popen(command,
                          stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as process:
        lines = queue.Queue()
        Thread(target=lambda: lines.put(process.stdout.readline()), daemon=True).start()
        try:
            host, port = lines.get(timeout=10).strip().rsplit(":", 1)
            assert host == "127.0.0.1"
            yield (host, int(port)), process
        finally:
            if process.poll() is None:
                process.stdin.write("\n")
                process.stdin.flush()
                try:
                    assert process.wait(timeout=10) == 0
                except subprocess.TimeoutExpired:
                    process.kill()
                    raise


def peer(address, name):
    sock, _ = connect(address, 5, name)
    return sock


def publish(sock, payload, identifier, qos=1, name=NAME, suffix=b""):
    kind = 0x31 | qos << 1
    packet_id = identifier.to_bytes(2, "big") if qos else b""
    sock.sendall(frame(kind, topic(name) + packet_id + b"\0" + payload) + suffix)


def pending(sock):
    transport = sock.sock if isinstance(sock, WsPeer) else sock
    previous = transport.gettimeout()
    transport.settimeout(0.15)
    try:
        raise AssertionError(("received data or close before retained commit", sock.recv(1)))
    except socket.timeout:
        pass
    finally:
        transport.settimeout(previous)


def crash(process):
    process.kill()
    assert process.wait(timeout=10) != 0


def restored(address, payload):
    with peer(address, b"snapshot") as observer:
        subscribe(observer, NAME, 1, 0)
        if payload is not None:
            assert receive(observer) == (0x31, topic(NAME) + b"\0" + payload)
        barrier(observer)


def verify(program, path):
    with broker(program, "capture", path, 1500) as (address, process):
        with peer(address, b"publisher") as publisher, peer(address, b"contender") as contender:
            publish(publisher, b"first", 1, suffix=PING)
            pending(publisher)
            assert not path.exists()
            publish(contender, b"overlap", 2)
            assert receive(contender) == (0x40, b"\0\x02\x97")
            with peer(address, b"observer") as observer:
                subscribe(observer, NAME, 1, 0)
                barrier(observer)
                assert receive(publisher) == (0x40, b"\0\1")
                assert receive(publisher) == PONG
                assert receive(observer) == (0x30, topic(NAME) + b"\0first")
            assert path.exists()
            crash(process)

    with broker(program, "restore", path, 1500) as (address, process):
        restored(address, b"first")
        with peer(address, b"interrupted") as publisher:
            publish(publisher, b"interrupted", 3)
            pending(publisher)
            crash(process)

    with broker(program, "restore", path, 1500) as (address, process):
        restored(address, b"first")
        with peer(address, b"qos2") as publisher:
            publish(publisher, b"qos2", 4, qos=2)
            pending(publisher)
            assert receive(publisher) == (0x50, b"\0\4\x10")
            publisher.sendall(frame(0x62, b"\0\4"))
            assert receive(publisher) == (0x70, b"\0\4")
            crash(process)

    with broker(program, "restore", path) as (address, process):
        restored(address, b"qos2")
        with peer(address, b"policy") as publisher:
            publish(publisher, b"unsupported slot", 5, name=b"other/topic")
            assert receive(publisher) == (0x40, b"\0\5\x97")
            publish(publisher, b"", 6)
            assert receive(publisher) == (0x40, b"\0\6\x10")
            crash(process)
    with broker(program, "restore", path) as (address, _):
        restored(address, None)

    with broker(program, "restore", path) as (address, process):
        legacy, _ = connect(address, 4, b"legacy")
        with legacy:
            legacy.sendall(frame(0x33, topic(NAME) + b"\0\1legacy"))
            assert receive(legacy) == (0x40, b"\0\1")
            crash(process)
    with broker(program, "restore", path) as (address, _):
        restored(address, b"legacy")

    closing = path.parent / "closing.bin"
    publisher = None
    try:
        with broker(program, "capture", closing, 500) as (address, _):
            publisher = peer(address, b"closing")
            publish(publisher, b"retired after close", 7)
            pending(publisher)
        assert closing.exists(), "shutdown returned before the pending writer retired"
    finally:
        if publisher is not None:
            publisher.close()
    with broker(program, "restore", closing) as (address, _):
        restored(address, b"retired after close")

    with broker(program, "capture", path.parent / "missing" / "slot.bin") as (address, _):
        with peer(address, b"failed") as publisher:
            publish(publisher, b"failed", 8)
            try:
                assert not publisher.recv(1), "unknown storage outcome emitted an acknowledgment"
            except ConnectionResetError:
                pass
        with socket.create_connection(address, timeout=5) as newcomer:
            try:
                newcomer.sendall(frame(0x10, b"\0\4MQTT\5\2\0\0\0" + topic(b"sealed")))
                assert not newcomer.recv(1), "storage uncertainty must seal admission"
            except (BrokenPipeError, ConnectionResetError):
                pass
    return {"retained_commit": "pass", "ack_after_commit": True, "legacy": True, "rollback": True, "shutdown_drains_writes": True,
            "process_loss": True, "durable_delete": True, "unknown_seals_admission": True}


def verify_transport(program, directory, credentials, mode):
    tls = None
    if mode in ("tls", "wss"):
        tls = ssl.create_default_context(cafile=str(credentials / "certificate.pem"))
        tls.minimum_version = ssl.TLSVersion.TLSv1_3
    context = WsContext(tls) if mode in ("ws", "wss") else tls
    path = directory / f"{mode}.bin"
    with broker(program, "capture", path, 500, mode, credentials) as (address, _):
        with socket.create_connection(address, timeout=8) as transport:
            with context.wrap_socket(transport, server_hostname="localhost") as publisher:
                publisher.sendall(frame(0x10, b"\0\4MQTT\5\2\0\0\0" + topic(b"transport")))
                kind, body = receive(publisher)
                assert kind == 0x20 and body[:2] == b"\0\0"
                publish(publisher, b"ordered suffix", 1, suffix=PING + PING)
                pending(publisher)
                assert receive(publisher) == (0x40, b"\0\1\x10")
                assert receive(publisher) == PONG and receive(publisher) == PONG
                assert path.exists()
                publish(publisher, b"qos0", 0, qos=0, suffix=PING)
                pending(publisher)
                assert receive(publisher) == PONG
    return "pass"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("credentials", type=Path, nargs="?")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="xmqtt-live-retained-") as temporary:
        directory = Path(temporary)
        result = verify(args.program, directory / "slot.bin")
        if args.credentials:
            result["transports"] = {mode: verify_transport(args.program, directory, args.credentials, mode)
                                    for mode in ("tls", "ws", "wss")}
        print(json.dumps(result))


if __name__ == "__main__":
    main()
