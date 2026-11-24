#!/usr/bin/env python3
"""Verify the XRT MQTT/TLS listener with Python's independent TLS stack."""
import argparse
from contextlib import ExitStack
import json
import math
import queue
import socket
import ssl
import statistics
import subprocess
from pathlib import Path
from threading import Thread
from time import perf_counter_ns

from run_server_handshake import finish_qos2, ping, publish_qos2, read, receive_qos2, small_frame, topic


def frame(kind, body):
    size = len(body)
    header = bytearray((kind,))
    while size >= 128:
        header.append((size & 127) | 128)
        size >>= 7
    header.append(size)
    return bytes(header) + body


def receive(sock):
    kind, size, shift = read(sock, 1)[0], 0, 0
    for _ in range(4):
        digit = read(sock, 1)[0]
        size |= (digit & 127) << shift
        if not digit & 128:
            return kind, read(sock, size)
        shift += 7
    raise AssertionError("invalid Remaining Length")


def mqtt_connect(sock, name, level=5, clean=True, persistent=False, receive_maximum=None):
    body = b"\0\4MQTT" + bytes((level, 2 if clean else 0, 0, 0))
    if level == 5:
        properties = b"\x11\0\0\0\x3c" if persistent else b""
        if receive_maximum is not None:
            properties += b"\x21" + receive_maximum.to_bytes(2, "big")
        body += bytes((len(properties),)) + properties
    wire = small_frame(0x10, body + topic(name))
    # Separate TLS writes fragment the MQTT fixed/variable header.
    sock.sendall(wire[:1])
    sock.sendall(wire[1:9])
    sock.sendall(wire[9:] + b"\xc0\0")


def connack(sock, present=False):
    kind, body = receive(sock)
    assert kind == 0x20 and body[:2] == bytes((present, 0)), (kind, body)


def finish(sock):
    sock.sendall(b"\xe0\0")
    sock.unwrap().close()


def verify(endpoint, context):
    def connect():
        sock = socket.create_connection(endpoint, timeout=10)
        return context.wrap_socket(sock, server_hostname="localhost")

    with connect() as legacy:
        mqtt_connect(legacy, b"legacy", level=4)
        connack(legacy)
        assert receive(legacy) == (0xd0, b"")
        finish(legacy)

    with connect() as observer, connect() as producer:
        mqtt_connect(observer, b"watch", persistent=True)
        connack(observer)
        assert receive(observer) == (0xd0, b"")
        mqtt_connect(producer, b"send")
        connack(producer)
        assert receive(producer) == (0xd0, b"")
        name = topic(b"tls/data")
        observer.sendall(small_frame(0x82, b"\0\1\0" + name + b"\1"))
        assert receive(observer) == (0x90, b"\0\1\0\1")
        payload = b"\0\xffxyz" * 3100
        publication = frame(0x32, name + b"\0\7\0" + payload)
        for at in range(0, len(publication), 719):
            producer.sendall(publication[at:at + 719])
        assert receive(producer) == (0x40, b"\0\7")
        expected = name + b"\0\1\0" + payload
        assert receive(observer) == (0x32, expected)
        with connect() as successor:
            mqtt_connect(successor, b"watch", clean=False, persistent=True)
            assert receive(observer) == (0xe0, b"\x8e")
            observer.unwrap().close()
            connack(successor, present=True)
            assert receive(successor) == (0x3a, expected)
            assert receive(successor) == (0xd0, b"")
            successor.sendall(b"\x40\x02\0\1")
            producer.sendall(frame(0x34, name + b"\0\x08\0second"))
            assert receive(producer) == (0x50, b"\0\x08")
            producer.sendall(b"\x62\x02\0\x08")
            assert receive(producer) == (0x70, b"\0\x08")
            assert receive(successor) == (0x32, name + b"\0\2\0second")
            successor.sendall(b"\x40\x02\0\2")
            successor.sendall(small_frame(0x82, b"\0\3\0" + name + b"\2"))
            assert receive(successor) == (0x90, b"\0\3\0\2")
            producer.sendall(frame(0x32, name + b"\0\x09\0third"))
            assert receive(producer) == (0x40, b"\0\x09")
            assert receive(successor) == (0x32, name + b"\0\3\0third")
            successor.sendall(b"\x40\x02\0\3")
            producer.sendall(frame(0x34, name + b"\0\x0a\0fourth"))
            assert receive(producer) == (0x50, b"\0\x0a")
            producer.sendall(b"\x62\x02\0\x0a")
            assert receive(producer) == (0x70, b"\0\x0a")
            assert receive(successor) == (0x34, name + b"\0\4\0fourth")
            successor.sendall(b"\x50\x02\0\4")
            assert receive(successor) == (0x62, b"\0\4")
            successor.sendall(b"\x70\x02\0\4")
            successor.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            # Queue several TLS records before the receiver reads, then verify
            # the full stream and identifier order under the smaller window.
            for index in range(48):
                message = payload + index.to_bytes(2, "big")
                sent_id = (100 + index).to_bytes(2, "big")
                producer.sendall(frame(0x32, name + sent_id + b"\0" + message))
                assert receive(producer) == (0x40, sent_id)
            for index in range(48):
                recv_id = (5 + index).to_bytes(2, "big")
                assert receive(successor) == (0x32, name + recv_id + b"\0" + payload +
                                              index.to_bytes(2, "big"))
                successor.sendall(b"\x40\x02" + recv_id)
            finish(successor)
        finish(producer)


def verify_quota_recovery(endpoint, context):
    capacity = 256  # Standalone APP's default outbound delivery credit.
    blocked, healthy = b"tls/blocked", b"tls/healthy"
    with ExitStack() as stack:
        peers = []
        for name, receive_maximum in ((b"quota-slow", 1), (b"quota-fast", None),
                                      (b"quota-publisher", None), (b"quota-optional", None)):
            raw = stack.enter_context(socket.create_connection(endpoint, timeout=10))
            peer = stack.enter_context(context.wrap_socket(raw, server_hostname="localhost"))
            mqtt_connect(peer, name, receive_maximum=receive_maximum)
            connack(peer)
            assert receive(peer) == (0xd0, b"")
            peers.append(peer)
        slow, fast, producer, optional = peers
        for peer, name in ((slow, blocked), (fast, healthy)):
            peer.sendall(small_frame(0x82, b"\0\1\0" + topic(name) + b"\2"))
            assert receive(peer) == (0x90, b"\0\1\0\2")

        for number in range(1, capacity + 1):
            publish_qos2(producer, blocked, number, number.to_bytes(2, "big"))
        first = receive_qos2(slow, blocked, b"\0\1")
        shared = b"$share/quota/" + blocked
        optional_only = b"tls/optional-only"
        optional.sendall(small_frame(0x82, b"\0\1\0" + topic(shared) + b"\0" +
                                     topic(optional_only) + b"\0"))
        assert receive(optional) == (0x90, b"\0\1\0\0\0")

        retry = capacity + 1
        publish_qos2(producer, blocked, retry, b"refused", quota=True)
        ping((optional,))
        publish_qos2(producer, healthy, retry + 1, b"healthy")
        finish_qos2(fast, receive_qos2(fast, healthy, b"healthy"))

        slow.sendall(small_frame(0x50, first))
        assert receive(slow) == (0x62, first)
        publish_qos2(producer, blocked, retry, b"still refused", quota=True)
        ping((optional,))
        publish_qos2(producer, optional_only, retry + 2, b"best effort only")
        assert receive(optional) == (0x30, topic(optional_only) + b"\0best effort only")
        slow.sendall(small_frame(0x70, first))
        second = receive_qos2(slow, blocked, b"\0\2")

        publish_qos2(producer, blocked, retry, b"retried")
        assert receive(optional) == (0x30, topic(blocked) + b"\0retried")
        finish_qos2(slow, second)
        for number in range(3, capacity + 1):
            finish_qos2(slow, receive_qos2(slow, blocked, number.to_bytes(2, "big")))
        finish_qos2(slow, receive_qos2(slow, blocked, b"retried"))
        ping(peers)
        for peer in peers:
            finish(peer)
    return {"quota_refusals": 2, "other_topic_deliveries": 1, "ordered_recovered_deliveries": capacity + 1,
            "shared_qos0_after_recovery": 1, "best_effort_only": 1}



def verify_coalesced_credit(endpoint, context):
    topic_name = b"tls/coalesced-credit"
    with ExitStack() as stack:
        peers = []
        for name in (b"credit-consumer", b"credit-producer"):
            raw = stack.enter_context(socket.create_connection(endpoint, timeout=10))
            peer = stack.enter_context(context.wrap_socket(raw, server_hostname="localhost"))
            mqtt_connect(peer, name, receive_maximum=2)
            connack(peer)
            assert receive(peer) == (0xd0, b"")
            peers.append(peer)
        subscriber, producer = peers
        subscriber.sendall(small_frame(0x82, b"\0\1\0" + topic(topic_name) + b"\2"))
        assert receive(subscriber) == (0x90, b"\0\1\0\2")
        for number in range(1, 9):
            publish_qos2(producer, topic_name, number, bytes([number]))

        identifiers = [receive_qos2(subscriber, topic_name, bytes([n])) for n in (1, 2)]
        for group in range(1, 5):
            subscriber.sendall(b"".join(small_frame(0x50, ident) for ident in identifiers))
            assert [receive(subscriber), receive(subscriber)] == [(0x62, ident) for ident in identifiers]
            acknowledgements = b"".join(small_frame(0x70, ident) for ident in identifiers)
            if group == 4:
                subscriber.sendall(acknowledgements + b"\xc0\0")
                assert receive(subscriber) == (0xd0, b"")
                break

            next_number = 2 * group + 1
            if group == 2:
                # The first complete PUBCOMP frees credit before the second PUBCOMP is complete.
                subscriber.sendall(acknowledgements[:-2])
                first = receive_qos2(subscriber, topic_name, bytes([next_number]))
                subscriber.sendall(acknowledgements[-2:])
            else:
                subscriber.sendall(acknowledgements)
                first = receive_qos2(subscriber, topic_name, bytes([next_number]))
            identifiers = [first, receive_qos2(subscriber, topic_name, bytes([next_number + 1]))]
        for peer in peers:
            finish(peer)
    return {"ordered_deliveries": 8, "fragmented_terminal_ack": True, "ping_barrier": True}


def ping_measurement(endpoint, context, count):
    runs = []
    with context.wrap_socket(socket.create_connection(endpoint, timeout=10),
                             server_hostname="localhost") as peer:
        mqtt_connect(peer, b"measure")
        connack(peer)
        assert receive(peer) == (0xd0, b"")
        for repeat in range(4):
            samples = []
            begin = perf_counter_ns()
            for _ in range(count):
                start = perf_counter_ns()
                peer.sendall(b"\xc0\0")
                assert receive(peer) == (0xd0, b"")
                samples.append(perf_counter_ns() - start)
            elapsed = perf_counter_ns() - begin
            samples.sort()
            if repeat:
                runs.append({"pings_per_sec": count * 1e9 / elapsed,
                             "p99_us": samples[math.ceil(0.99 * count) - 1] / 1000})
        finish(peer)
    return {**{key: statistics.median(run[key] for run in runs) for key in runs[0]},
            "samples_per_run": count, "runs": runs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("credentials", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--ping-samples", type=int, default=0)
    args = parser.parse_args()
    if args.ping_samples < 0:
        parser.error("--ping-samples must be nonnegative")
    context = ssl.create_default_context(cafile=str(args.credentials / "certificate.pem"))
    context.minimum_version = ssl.TLSVersion.TLSv1_3
    with subprocess.Popen(
        [str(args.program.resolve()), str(args.credentials / "certificate.der"),
         str(args.credentials / "key.der")], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
    ) as server:
        endpoint = queue.Queue(maxsize=1)
        Thread(target=lambda: endpoint.put(server.stdout.readline()), daemon=True).start()
        try:
            host, sep, port = endpoint.get(timeout=10).strip().partition(":")
            assert host == "127.0.0.1" and sep
            address = (host, int(port))
            verify(address, context)
            quota = verify_quota_recovery(address, context)
            coalesced = verify_coalesced_credit(address, context)
            ping = ping_measurement(address, context, args.ping_samples) if args.ping_samples else None
            server.stdin.write("\n")
            server.stdin.flush()
            assert server.wait(timeout=10) == 0, "TLS broker did not drain"
        finally:
            if server.poll() is None:
                server.kill()
    result = {"tls_1_3": True, "verified_identity": True, "mqtt_5_and_311": True,
              "fragmented_connect_and_publish": True, "qos_1_and_2": True,
              "live_tls_takeover": True, "clean_tls_close": True, "queued_qos1_deliveries": 48,
              "qos2_quota_recovery": quota, "qos2_coalesced_credit": coalesced}
    if ping is not None:
        result["loopback_ping"] = ping
    args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("[pass] MQTT/TLS 1.3 independent peer, fragmentation, QoS 1/2, takeover and quota recovery")


if __name__ == "__main__":
    main()
