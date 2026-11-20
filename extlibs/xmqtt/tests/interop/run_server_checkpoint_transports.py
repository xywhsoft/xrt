#!/usr/bin/env python3
"""Verify MQTT checkpoint recovery across TCP, TLS, WS and WSS."""
import argparse
from contextlib import ExitStack, contextmanager
import json
from pathlib import Path
import socket
import ssl
from tempfile import TemporaryDirectory

from run_server_checkpoint import (barrier, broker, check_retained_value, expect_connack,
                                   persistent_connect, publish, received_publish, session_records, subscribe)
from run_server_handshake import topic
from run_server_tls import finish, frame, receive
from run_server_ws import WsPeer


WORKER = b"transport-worker"
SOURCE = b"transport-source"
NAME = b"transport/delivery"
RETAINED = b"transport/retained"
VALUE = b"saved\0\xff"


def arguments(mode, credentials):
    if mode in ("tls", "wss"):
        return "--" + mode, str(credentials / "certificate.der"), str(credentials / "key.der")
    return ("--ws",) if mode == "ws" else ()


@contextmanager
def connected(address, mode, context, client_id, clean=True, present=False):
    with ExitStack() as sockets:
        peer = sockets.enter_context(socket.create_connection(address, timeout=8))
        if mode in ("tls", "wss"):
            peer = sockets.enter_context(context.wrap_socket(peer, server_hostname="localhost"))
            assert peer.version() == "TLSv1.3" and peer.cipher()[0] == "TLS_AES_128_GCM_SHA256"
        if mode in ("ws", "wss"):
            peer = sockets.enter_context(WsPeer(peer))
        peer.sendall(persistent_connect(client_id, clean))
        expect_connack(peer, present)
        yield peer


def send_qos(peer, qos, identifier, value):
    peer.sendall(frame(0x30 | qos << 1, topic(NAME) + identifier + b"\0" + value))
    assert receive(peer) == (0x40 if qos == 1 else 0x50, identifier)
    if qos == 2:
        peer.sendall(frame(0x62, identifier))
        assert receive(peer) == (0x70, identifier)


def record_initial_session(program, image, mode, context, credentials):
    # Enable online transactions before admitting the first durable delivery.
    with broker(program, "capture", image):
        pass
    with broker(program, "restore-online", image, prefix=arguments(mode, credentials)) as address:
        with connected(address, mode, context, SOURCE) as source:
            publish(source, RETAINED, VALUE)
            with connected(address, mode, context, WORKER) as worker:
                subscribe(worker, NAME, 1, 2)
                send_qos(source, 1, b"\0\x11", b"first")
                first = received_publish(worker, 0x32, NAME, b"first")
                send_qos(source, 2, b"\0\x12", b"second")
                second = received_publish(worker, 0x34, NAME, b"second")
                worker.sendall(frame(0x50, second))
                assert receive(worker) == (0x62, second)
                finish(worker)
            send_qos(source, 1, b"\0\x13", b"offline")
            finish(source)
    return first, second


def advance_before_stop(program, image, context, credentials, first, second):
    with ExitStack() as peers:
        # Terminate the broker while both network connections are still open.
        with broker(program, "restore-online", image, crash=True, prefix=arguments("tls", credentials)) as address:
            worker = peers.enter_context(connected(address, "tls", context, WORKER, False, True))
            source = peers.enter_context(connected(address, "tls", context, SOURCE, False, True))
            assert received_publish(worker, 0x3a, NAME, b"first") == first
            assert receive(worker) == (0x62, second)
            queued = received_publish(worker, 0x32, NAME, b"offline")
            assert queued not in (b"\0\0", first, second)
            worker.sendall(frame(0x40, first) + frame(0x70, second) + frame(0x40, queued))
            barrier(worker)
            send_qos(source, 2, b"\0\x14", b"after-restore")
            inflight = received_publish(worker, 0x34, NAME, b"after-restore")
            worker.sendall(frame(0x50, inflight))
            assert receive(worker) == (0x62, inflight)
    data = image.read_bytes()
    outgoing = [(data[at:at + 4], data[body - 16:body])
                for at, body, _ in session_records(data) if data[at] in (4, 5)]
    assert len(outgoing) == 1, outgoing
    header, delivery_key = outgoing[0]
    assert header == b"\x05\x07" + inflight and delivery_key != bytes(16)
    return inflight


def probe(program, credentials, image, initial, recovery, context):
    first, second = record_initial_session(program, image, initial, context, credentials)
    inflight = advance_before_stop(program, image, context, credentials, first, second)
    with broker(program, "restore-online", image, prefix=arguments(recovery, credentials)) as address:
        with connected(address, recovery, context, WORKER, False, True) as worker:
            packet = receive(worker)
            assert packet == (0x62, inflight), (recovery, packet, inflight)
            worker.sendall(frame(0x70, inflight))
            barrier(worker)
            finish(worker)
        with connected(address, recovery, context, b"transport-observer") as observer:
            check_retained_value(observer, RETAINED, 1, VALUE)
            barrier(observer)
            finish(observer)

    with broker(program, "restore-online", image) as address:
        with connected(address, "tcp", context, WORKER, False, True) as worker:
            barrier(worker)
    return {"transports": [initial, "tls", recovery, "tcp"], "session_present": True, "qos1_dup": True,
            "qos2_pubrel": True, "offline_fifo": True, "retained": True, "completed_qos_removed": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("credentials", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    credentials = args.credentials.resolve(strict=True)
    context = ssl.create_default_context(cafile=str(credentials / "certificate.pem"))
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_3
    with TemporaryDirectory(prefix="xmqtt-checkpoint-transport-") as directory:
        results = {initial: probe(args.program, credentials, Path(directory) / (initial + ".bin"),
                                  initial, recovery, context)
                   for initial, recovery in (("ws", "wss"), ("wss", "ws"))}
    with args.report.open("x", encoding="utf-8") as report:
        json.dump(results, report, indent=2)
        report.write("\n")
    print("[pass] cross-transport checkpoint, live acknowledgements and restart")


if __name__ == "__main__":
    main()
