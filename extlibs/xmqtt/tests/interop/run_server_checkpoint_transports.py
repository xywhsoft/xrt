#!/usr/bin/env python3
"""Verify MQTT checkpoint recovery across TCP, TLS, WS and WSS."""
import argparse
from contextlib import ExitStack, contextmanager
import json
from pathlib import Path
import socket
import ssl
from tempfile import TemporaryDirectory
import time

from run_server_checkpoint import (barrier, broker, check_retained_value, expect_connack, expect_disconnect,
                                   persistent_connect, publish, received_publish, session_keys, session_records, subscribe)
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
def connected(address, mode, context, client_id, clean=True, present=False, *, expiry=60):
    with ExitStack() as sockets:
        peer = sockets.enter_context(socket.create_connection(address, timeout=8))
        if mode in ("tls", "wss"):
            peer = sockets.enter_context(context.wrap_socket(peer, server_hostname="localhost"))
            assert peer.version() == "TLSv1.3" and peer.cipher()[0] == "TLS_AES_128_GCM_SHA256"
        if mode in ("ws", "wss"):
            peer = sockets.enter_context(WsPeer(peer))
        peer.sendall(persistent_connect(client_id, clean, expiry=expiry))
        expect_connack(peer, present)
        yield peer


def send_qos(peer, qos, identifier, value, *, name=NAME):
    peer.sendall(frame(0x30 | qos << 1, topic(name) + identifier + b"\0" + value))
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


def probe_expiry(program, credentials, image, mode, context):
    """Race incoming CONNECT against the old deadline during a delayed durable CLOSE."""
    prefix = arguments(mode, credentials)
    with broker(program, "capture", image):
        pass
    for early in (True, False):
        client_id = b"expiry-early" if early else b"expiry-late"
        name = NAME + b"/" + client_id
        with broker(program, "restore-online-expiry", image, "1500", crash=True, prefix=prefix) as address:
            with connected(address, mode, context, SOURCE, expiry=0) as source:
                with connected(address, mode, context, client_id) as original:
                    subscribe(original, name, 1, 1)
                    send_qos(source, 1, b"\0\x11", VALUE, name=name)
                    packet_id = received_publish(original, 0x32, name, VALUE)
                    old_key = session_keys(image)[client_id]
                    # Change the live 60-second Session Expiry to one second.
                    original.sendall(frame(0xe0, b"\0\5\x11\0\0\0\1"))
                    if mode == "tcp":
                        expect_disconnect(original)
                    else:
                        original.unwrap().close()
                if not early:
                    time.sleep(1.12)
                with connected(address, mode, context, client_id, False, early, expiry=40) as replacement:
                    assert (session_keys(image)[client_id] == old_key) == early
                    if early:
                        assert received_publish(replacement, 0x3a, name, VALUE) == packet_id
                        before_ack = image.stat().st_mtime_ns
                        replacement.sendall(frame(0x40, packet_id))
                    barrier(replacement)
                    if early:
                        # PINGRESP can precede the independent journal writer.
                        deadline = time.monotonic() + 6
                        while image.stat().st_mtime_ns == before_ack:
                            assert time.monotonic() < deadline, "missing durable outgoing ACK"
                            time.sleep(0.02)
                        data = image.read_bytes()
                        assert not any(data[at] in (4, 5) for at, _, _ in session_records(data))

    saved = image.read_bytes()
    assert not any(saved[at] in (4, 5) for at, _, _ in session_records(saved))
    with broker(program, "restore-online-expiry", image) as address:
        for name in (b"expiry-early", b"expiry-late"):
            with connected(address, "tcp", context, name, False, True, expiry=40) as restored:
                barrier(restored)
    return {"close_delay_ms": 1500, "expiry_seconds": 1, "early_session_present": True,
            "early_qos1_duplicate_id": True, "late_session_present": False,
            "old_deliveries_absent_after_restart": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("credentials", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--expiry-only", action="store_true", help="test the durable CLOSE expiry boundary")
    args = parser.parse_args()
    credentials = args.credentials.resolve(strict=True)
    context = ssl.create_default_context(cafile=str(credentials / "certificate.pem"))
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_3
    with TemporaryDirectory(prefix="xmqtt-checkpoint-transport-") as directory:
        if args.expiry_only:
            results = {mode: probe_expiry(args.program, credentials, Path(directory) / (mode + ".bin"), mode, context)
                       for mode in ("tcp", "tls", "ws", "wss")}
        else:
            results = {initial: probe(args.program, credentials, Path(directory) / (initial + ".bin"),
                                      initial, recovery, context)
                       for initial, recovery in (("ws", "wss"), ("wss", "ws"))}
    with args.report.open("x", encoding="utf-8") as report:
        json.dump(results, report, indent=2)
        report.write("\n")
    message = "durable CLOSE expiry" if args.expiry_only else "checkpoint, live acknowledgements"
    print(f"[pass] cross-transport {message} and restart")


if __name__ == "__main__":
    main()
