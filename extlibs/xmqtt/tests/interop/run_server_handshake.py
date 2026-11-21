"""Exercise the standalone MQTT TCP server and subscription routes with a separate peer."""
import argparse
from contextlib import ExitStack, contextmanager
import hashlib
import json
import math
import platform
from pathlib import Path
import queue
import socket
import struct
import subprocess
from threading import Thread
import time


def read(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise AssertionError("truncated MQTT reply")
        data.extend(chunk)
    return bytes(data)


def connect_packet(level, client_id, receive_maximum=None):
    body = b"\0\4MQTT" + bytes((level, 2, 0, 0))
    if level == 5:
        body += b"\0" if receive_maximum is None else b"\3\x21" + struct.pack("!H", receive_maximum)
    return small_frame(0x10, body + topic(client_id))


def connect(endpoint, level, client_id, receive_maximum=None, *, allow_refusal=False, timeout=5):
    wire = connect_packet(level, client_id, receive_maximum)
    sock = socket.socket()
    sock.settimeout(timeout)
    try:
        try:
            sock.connect(endpoint)
            # Split the CONNECT and pipeline the first PING.
            sock.sendall(wire[:8])
            sock.sendall(wire[8:] + b"\xc0\0")
            header = sock.recv(2)
        except (ConnectionResetError, BrokenPipeError):
            if not allow_refusal:
                raise
            header = b""
        if allow_refusal and not header:
            sock.close()
            return None, ""
        assert header, "truncated MQTT reply"
        header += read(sock, 2 - len(header))
        assert header[0] == 0x20 and header[1] < 64, header.hex()
        ack = read(sock, header[1])
        if allow_refusal and level == 5 and len(ack) >= 3 and ack[0] == 0 and ack[1] >= 0x80:
            sock.close()
            return None, ack.hex()
        assert ack[:2] == b"\0\0", ack.hex()
        if level == 4:
            assert ack == b"\0\0", ack.hex()
        assert read(sock, 2) == b"\xd0\0"
        return sock, ack.hex()
    except BaseException:
        sock.close()
        raise


def small_frame(kind, data):
    assert len(data) < 128
    return bytes((kind, len(data))) + data


def topic(name):
    return struct.pack("!H", len(name)) + name


def check_routes(legacy, modern):
    name = b"Case/test"
    payload = b"independent\0\xff"
    properties = b"\x04\x03\0\x01x"

    legacy.sendall(small_frame(0x82, b"\0\1" + topic(name) + b"\x02"))
    assert read(legacy, 5) == b"\x90\x03\0\1\2"
    modern.sendall(small_frame(0x82, b"\0\2\0" + topic(name) + b"\x04"))
    assert read(modern, 6) == b"\x90\x04\0\2\0\0"

    modern.sendall(small_frame(0x30, topic(name) + properties + payload) + b"\xc0\0")
    forwarded = small_frame(0x30, topic(name) + payload)
    assert read(legacy, len(forwarded)) == forwarded
    assert read(modern, 2) == b"\xd0\0", "No Local must exclude the publishing client"

    legacy.sendall(small_frame(0x30, topic(name) + payload))
    forwarded = small_frame(0x30, topic(name) + b"\0" + payload)
    assert read(modern, len(forwarded)) == forwarded
    forwarded = small_frame(0x30, topic(name) + payload)
    assert read(legacy, len(forwarded)) == forwarded
    modern.sendall(small_frame(0xa2, b"\0\3\0" + topic(name)))
    assert read(modern, 6) == b"\xb0\x04\0\3\0\0"

    wrong_case = small_frame(0x30, topic(name.lower()) + payload)
    legacy.sendall(wrong_case + b"\xc0\0")
    assert read(legacy, 2) == b"\xd0\0", "exact filters preserve case"
    legacy.sendall(small_frame(0x30, topic(name) + payload))
    assert read(legacy, len(forwarded)) == forwarded
    modern.sendall(b"\xc0\0")
    assert read(modern, 2) == b"\xd0\0", "unsubscribed connection must stop receiving"


def check_wildcards(legacy, modern):
    legacy.sendall(small_frame(0x82, b"\0\4" + topic(b"Case/#") + b"\0" + topic(b"#") + b"\0"))
    assert read(legacy, 6) == b"\x90\x04\0\4\0\0"
    for label in (b"Case", b"Case/test", b"Case//suffix"):
        modern.sendall(small_frame(0x30, topic(label) + b"\0payload"))
        forwarded = small_frame(0x30, topic(label) + b"payload")
        assert read(legacy, len(forwarded)) == forwarded
    modern.sendall(small_frame(0x30, topic(b"$SYS/ignored") + b"\0payload") + b"\xc0\0")
    assert read(modern, 2) == b"\xd0\0"
    legacy.sendall(b"\xc0\0")
    assert read(legacy, 2) == b"\xd0\0", "root wildcards exclude $ and overlapping filters forward once"

    legacy.sendall(small_frame(0xa2, b"\0\5" + topic(b"#") + topic(b"Case/#") + topic(b"Case/test")))
    assert read(legacy, 4) == b"\xb0\x02\0\5"
    modern.sendall(small_frame(0x30, topic(b"Case/test") + b"\0payload") + b"\xc0\0")
    assert read(modern, 2) == b"\xd0\0"
    legacy.sendall(b"\xc0\0")
    assert read(legacy, 2) == b"\xd0\0", "unsubscribe removes the matching wildcard paths"


def check_shared(endpoint, legacy, modern):
    peer, _ = connect(endpoint, 5, b"group")
    group = b"$share/g/Case/+"
    with peer:
        for consumer in (modern, peer):
            consumer.sendall(small_frame(0x82, b"\0\x06\0" + topic(group) + b"\0"))
            assert read(consumer, 6) == b"\x90\x04\0\x06\0\0"
        for winner, other in ((modern, peer), (peer, modern)):
            data = topic(b"Case/item")
            legacy.sendall(small_frame(0x30, data + b"payload") + b"\xc0\0")
            assert read(legacy, 2) == b"\xd0\0"
            expected = small_frame(0x30, data + b"\0payload")
            assert read(winner, len(expected)) == expected
            other.sendall(b"\xc0\0")
            assert read(other, 2) == b"\xd0\0", "one peer per shared subscription"

        legacy.sendall(small_frame(0x82, b"\0\x07" + topic(group) + b"\0"))
        assert read(legacy, 5) == b"\x90\x03\0\x07\0"
        data = topic(b"$share/g/Case/item")
        modern.sendall(small_frame(0x30, data + b"\0literal"))
        expected = small_frame(0x30, data + b"literal")
        assert read(legacy, len(expected)) == expected
        for consumer in (modern, peer):
            consumer.sendall(b"\xc0\0")
            assert read(consumer, 2) == b"\xd0\0"


def check_retained(legacy, modern):
    key = topic(b"snapshot")
    legacy.sendall(small_frame(0x31, key + b"old") + b"\xc0\0")
    assert read(legacy, 2) == b"\xd0\0"
    subscribe = small_frame(0x82, b"\0\x08\0" + key + b"\x10")
    ack = b"\x90\x04\0\x08\0\0"
    modern.sendall(subscribe)
    assert read(modern, len(ack)) == ack
    expected = small_frame(0x31, key + b"\0old")
    assert read(modern, len(expected)) == expected
    modern.sendall(subscribe + b"\xc0\0")
    assert read(modern, len(ack) + 2) == ack + b"\xd0\0", "only replay for a new subscription"
    modern.sendall(small_frame(0x82, b"\0\x09\0" + key + b"\x28") + b"\xc0\0")
    assert read(modern, 8) == b"\x90\x04\0\x09\0\0\xd0\0"

    legacy.sendall(small_frame(0x31, key + b"live"))
    expected = small_frame(0x31, key + b"\0live")
    assert read(modern, len(expected)) == expected, "preserve RETAIN on live deliveries when requested"
    legacy.sendall(small_frame(0x31, key))
    expected = small_frame(0x31, key + b"\0")
    assert read(modern, len(expected)) == expected, "empty retained payload clears and forwards"
    modern.sendall(small_frame(0x82, b"\0\x0a\0" + key + b"\0") + b"\xc0\0")
    assert read(modern, 8) == b"\x90\x04\0\x0a\0\0\xd0\0"


def check_retained_parallel(endpoint, publisher):
    key = topic(b"parallel")
    publisher.sendall(small_frame(0x31, key + b"stable") + b"\xc0\0")
    assert read(publisher, 2) == b"\xd0\0"
    subscribe = small_frame(0x82, b"\0\x01\0" + key + b"\0")
    expected = b"\x90\x04\0\x01\0\0" + small_frame(0x31, key + b"\0stable")
    with ExitStack() as connections:
        peers = []
        for index in range(64):
            peer, _ = connect(endpoint, 5, f"replay-{index}".encode("ascii"))
            peers.append(connections.enter_context(peer))
        for iteration in range(32):
            rotated = peers[iteration:] + peers[:iteration]
            for peer in rotated:
                peer.sendall(subscribe)
            for peer in rotated:
                assert read(peer, len(expected)) == expected, "concurrent exact retained lookup lost a replay"


def check_qos1(legacy, modern):
    key = topic(b"qos/in")
    legacy.sendall(small_frame(0x82, b"\0\x0b" + key + b"\x01"))
    assert read(legacy, 5) == b"\x90\x03\0\x0b\1"
    data = key + b"\0\x47\0qos-one"
    for identifier, kind in enumerate((0x32, 0x3a), start=1):
        modern.sendall(small_frame(kind, data))
        assert read(modern, 4) == b"\x40\x02\0\x47"
        packet_id = struct.pack("!H", identifier)
        expected = small_frame(0x32, key + packet_id + b"qos-one")
        assert read(legacy, len(expected)) == expected
        legacy.sendall(small_frame(0x40, packet_id))
    modern.sendall(small_frame(0x32, topic(b"qos/unmatched") + b"\0\x48\0payload"))
    assert read(modern, 5) == b"\x40\x03\0\x48\x10"
    key = topic(b"qos/legacy")
    modern.sendall(small_frame(0x82, b"\0\x0c\0" + key + b"\x01"))
    assert read(modern, 6) == b"\x90\x04\0\x0c\0\1"
    legacy.sendall(small_frame(0x32, key + b"\0\x49payload"))
    assert read(legacy, 4) == b"\x40\x02\0\x49"
    expected = small_frame(0x32, key + b"\0\1\0payload")
    assert read(modern, len(expected)) == expected
    modern.sendall(b"\x40\x03\0\1\x80\xc0\0")
    assert read(modern, 2) == b"\xd0\0"

def check_qos2(legacy, modern):
    key = topic(b"flow/qos2")
    payload = b"binary\0\xff"
    legacy.sendall(small_frame(0x82, b"\0\x0d" + key + b"\2"))
    assert read(legacy, 5) == b"\x90\x03\0\x0d\2"
    modern.sendall(small_frame(0x82, b"\0\x0e\0" + key + b"\1"))
    assert read(modern, 6) == b"\x90\x04\0\x0e\0\1"
    modern.sendall(small_frame(0x34, key + b"\0\x51\0" + payload))
    assert read(modern, 4) == b"\x50\x02\0\x51"
    downgraded = small_frame(0x32, key + b"\0\2\0" + payload)
    assert read(modern, len(downgraded)) == downgraded
    modern.sendall(b"\x40\x02\0\2")
    expected = small_frame(0x34, key + b"\0\3" + payload)
    assert read(legacy, len(expected)) == expected
    modern.sendall(small_frame(0x3c, key + b"\0\x51\0" + payload))
    assert read(modern, 4) == b"\x50\x02\0\x51"
    modern.sendall(b"\x62\x02\0\x51")
    assert read(modern, 4) == b"\x70\x02\0\x51"
    legacy.sendall(b"\x50\x02\0\3")
    assert read(legacy, 4) == b"\x62\x02\0\3"
    legacy.sendall(b"\x70\x02\0\3\xc0\0")
    assert read(legacy, 2) == b"\xd0\0", "no duplicate forwarding after PUBREC"

    key = topic(b"flow/reverse")
    modern.sendall(small_frame(0x82, b"\0\x0d\0" + key + b"\2"))
    assert read(modern, 6) == b"\x90\x04\0\x0d\0\2"
    legacy.sendall(small_frame(0x34, key + b"\0\x52" + payload))
    assert read(legacy, 4) == b"\x50\x02\0\x52"
    legacy.sendall(b"\x62\x02\0\x52")
    assert read(legacy, 4) == b"\x70\x02\0\x52"
    expected = small_frame(0x34, key + b"\0\3\0" + payload)
    assert read(modern, len(expected)) == expected
    modern.sendall(b"\x50\x02\0\3")
    assert read(modern, 4) == b"\x62\x02\0\3"
    modern.sendall(b"\x70\x02\0\3\xc0\0")
    assert read(modern, 2) == b"\xd0\0"


def eof(sock):
    try:
        assert sock.recv(1) == b"", "extra MQTT data after closure"
    except ConnectionResetError:
        pass


def open_session(endpoint, level, clean, present):
    client_id = b"saved" + bytes((ord("0") + level,))
    body = b"\0\4MQTT" + bytes((level, 2 if clean else 0, 0, 0))
    if level == 5:
        body += b"\5\x11\0\0\0\x1e"
    body += topic(client_id)
    deadline = time.monotonic() + 3
    while True:
        sock = socket.create_connection(endpoint, timeout=5)
        try:
            sock.sendall(small_frame(0x10, body))
            header = read(sock, 2)
            assert header[0] == 0x20
            ack = read(sock, header[1])
            if ack[:2] == bytes((int(present), 0)):
                return sock
            assert present and ack[0] == 0 and ack[1] == (0x89 if level == 5 else 3), ack.hex()
            eof(sock)
        except BaseException:
            sock.close()
            raise
        sock.close()
        assert time.monotonic() < deadline, "old TCP owner did not retire"
        time.sleep(0.01)


def check_session_resume(endpoint, publisher, level):
    key = topic(b"resume/" + bytes((ord("0") + level,)))
    properties = b"\0" if level == 5 else b""
    payload = b"resend\0with-binary"
    with open_session(endpoint, level, clean=(level == 5), present=False) as peer:
        peer.sendall(small_frame(0x82, b"\0\1" + properties + key + b"\1"))
        ack = small_frame(0x90, b"\0\1" + properties + b"\1")
        assert read(peer, len(ack)) == ack
        publisher.sendall(small_frame(0x32, key + b"\0\x59\0" + payload))
        assert read(publisher, 4) == b"\x40\x02\0\x59"
        body = key + b"\0\1" + properties + payload
        frame = small_frame(0x32, body)
        assert read(peer, len(frame)) == frame
        with open_session(endpoint, level, clean=False, present=True) as successor:
            if level == 5:
                assert read(peer, 3) == b"\xe0\x01\x8e"
            eof(peer)
            retry = small_frame(0x3a, body)
            assert read(successor, len(retry)) == retry
            successor.sendall(b"\x40\x02\0\1\xc0\0")
            assert read(successor, 2) == b"\xd0\0"



def check_will(endpoint, subscriber, level):
    name, payload = b"will/peer", b"offline\0"
    subscriber.sendall(small_frame(0x82, b"\0\x61\0" + topic(name) + b"\0"))
    assert read(subscriber, 6) == b"\x90\x04\0\x61\0\0"
    for graceful in (True, False):
        delay = 1 if level == 5 else 0
        connect_properties = b"\x05\x11\0\0\0\x03" if level == 5 else b""
        will_properties = b"\x05\x18\0\0\0" + bytes((delay,)) if level == 5 else b""
        body = b"\0\4MQTT" + bytes((level, 6, 0, 0)) + connect_properties
        body += topic(b"will-peer") + will_properties + topic(name) + topic(payload)
        with socket.create_connection(endpoint, timeout=5) as peer:
            peer.sendall(small_frame(0x10, body))
            header = read(peer, 2)
            assert header[0] == 0x20 and read(peer, header[1])[:2] == b"\0\0"
            start = time.monotonic()
            if graceful:
                peer.sendall(b"\xe0\0")
            else:
                peer.shutdown(socket.SHUT_WR)
            eof(peer)
        if graceful:
            subscriber.sendall(b"\xc0\0")
            assert read(subscriber, 2) == b"\xd0\0", "graceful DISCONNECT suppresses the Will"
        else:
            expected = small_frame(0x30, topic(name) + b"\0" + payload)
            assert read(subscriber, len(expected)) == expected, "independent Will publication"
            assert time.monotonic() - start >= delay - 0.1, "Will Delay must elapse"


@contextmanager
def running_server(program, *arguments):
    with subprocess.Popen(
        [str(program.resolve()), *arguments],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
    ) as server:
        start = queue.Queue(maxsize=1)
        Thread(target=lambda: start.put(server.stdout.readline()), daemon=True).start()
        try:
            host, sep, port = start.get(timeout=8).strip().partition(":")
            assert host == "127.0.0.1" and sep and int(port) > 0, (host, port)
            yield host, int(port)
            server.stdin.write("\n")
            server.stdin.flush()
            assert server.wait(timeout=8) == 0, "server teardown failed"
        finally:
            if server.poll() is None:
                server.kill()


def probe(program):
    with running_server(program, "wait") as endpoint:
        preserved, connack5 = connect(endpoint, 5, b"literal")
        with preserved:
            legacy, connack311 = connect(endpoint, 4, b"legacy")
            with legacy:
                newer, duplicate = connect(endpoint, 5, b"literal")
                with newer:
                    assert read(preserved, 3) == b"\xe0\x01\x8e"
                    eof(preserved)
                    legacy.sendall(b"\xc0\0")
                    assert read(legacy, 2) == b"\xd0\0", "other session must remain connected"
                    check_routes(legacy, newer)
                    check_wildcards(legacy, newer)
                    check_shared(endpoint, legacy, newer)
                    check_retained(legacy, newer)
                    check_retained_parallel(endpoint, legacy)
                    check_qos1(legacy, newer)
                    check_qos2(legacy, newer)
                    check_session_resume(endpoint, newer, 5)
                    check_session_resume(endpoint, newer, 4)
                    check_will(endpoint, newer, 5)
                    check_will(endpoint, newer, 4)
                    newer.sendall(b"\xe0\0")
                    eof(newer)
    return {"mqtt5_connack": connack5, "mqtt311_connack": connack311,
            "takeover_connack": duplicate, "remaining_peer_ping": True,
            "dual_version_routes": True, "wildcard_overlap": True, "shared_routing": True, "retained_routing": True, "parallel_replays": 2048, "bidirectional_qos1": True, "bidirectional_qos2": True, "persistent_resume": True, "live_takeover": True, "wills": True, "exact_case": True, "no_local": True, "unsubscribe": True}



def publish_qos2(peer, name, number, payload, quota=False):
    identifier = struct.pack("!H", number)
    peer.sendall(small_frame(0x34, topic(name) + identifier + b"\0" + payload))
    expected = identifier + b"\x97" if quota else identifier
    header = read(peer, 2)
    assert header == bytes((0x50, len(expected))), f"unexpected PUBREC header {header.hex()}"
    reply = read(peer, header[1])
    assert reply == expected, f"unexpected PUBREC {reply.hex()}, wanted {expected.hex()}"
    if quota:
        return
    peer.sendall(small_frame(0x62, identifier))
    assert read(peer, 4) == small_frame(0x70, identifier)


def receive_qos2(peer, name, payload):
    key = topic(name)
    header = read(peer, 2)
    assert header == bytes((0x34, len(key) + 3 + len(payload))), header.hex()
    body = read(peer, header[1])
    identifier = body[len(key):len(key) + 2]
    assert identifier != b"\0\0" and body == key + identifier + b"\0" + payload, body.hex()
    return identifier


def finish_qos2(peer, identifier):
    peer.sendall(small_frame(0x50, identifier))
    assert read(peer, 4) == small_frame(0x62, identifier)
    peer.sendall(small_frame(0x70, identifier))


def ping(peers):
    for start in range(0, len(peers), 128):
        wave = peers[start:start + 128]
        for peer in wave:
            peer.sendall(b"\xc0\0")
        for peer in wave:
            assert read(peer, 2) == b"\xd0\0", "peer must answer PING"


def healthy_flow(publisher, subscriber, name):
    times = []
    for number in range(1, 257):
        payload = struct.pack("!H", number)
        began = time.perf_counter_ns()
        publish_qos2(publisher, name, number, payload)
        finish_qos2(subscriber, receive_qos2(subscriber, name, payload))
        times.append((time.perf_counter_ns() - began) / 1000)
    # Confirm receipt of the last PUBCOMP and absence of an extra delivery.
    ping((subscriber,))
    times.sort()
    return {"rounds": len(times), **{f"p{label}_us": times[math.ceil(q * len(times)) - 1]
                                  for label, q in ((50, .50), (95, .95), (99, .99))}}


def drive_pressure(program, clients, result):
    # This is the standalone APP's default per-connection reliable quota.
    capacity = 256
    blocked, healthy = b"pressure/blocked", b"healthy/live"
    with running_server(program, "--compare", str(clients)) as endpoint, ExitStack() as cleanup:
        result["phase"] = "connect"

        def open_peer(index, receive_maximum=None):
            sock, _ = connect(endpoint, 5, f"pressure-{index}".encode("ascii"), receive_maximum)
            result["connected"] += 1
            return cleanup.enter_context(sock)

        slow = open_peer(0, receive_maximum=1)
        producer, fast, traffic = (open_peer(index) for index in range(1, 4))
        idle = [open_peer(index) for index in range(4, clients)]
        result["phase"] = "admission"
        for peer, name in ((fast, healthy), (slow, blocked)):
            peer.sendall(small_frame(0x82, b"\0\1\0" + topic(name) + b"\2"))
            assert read(peer, 6) == b"\x90\4\0\1\0\2"

        excess, _ = connect(endpoint, 5, b"over-capacity", allow_refusal=True)
        if excess is not None:
            with excess:
                raise AssertionError("broker accepted beyond its configured connection limit")
        result["over_capacity_refusals"] = 1
        ping(idle)
        result["baseline"] = healthy_flow(traffic, fast, healthy)

        result["phase"] = "fill"
        for number in range(1, capacity + 1):
            publish_qos2(producer, blocked, number, struct.pack("!H", number))
        first = receive_qos2(slow, blocked, b"\0\1")
        publish_qos2(producer, blocked, capacity + 1, b"rejected", quota=True)
        result["quota_exceeded"] = 1
        ping(idle + [slow])

        # Reuse slots with the slow recipient's queue still full.
        result["phase"] = "reconnect"
        replaced = max(1, clients // 100)
        for index in range(replaced):
            peer = idle[index]
            peer.sendall(b"\xe0\0")
            eof(peer)
            peer.close()
            identity = f"pressure-{clients + index}".encode("ascii")
            began = time.perf_counter()
            deadline = began + 5
            while True:
                remaining = deadline - time.perf_counter()
                if remaining <= 0:
                    raise TimeoutError("replacement slot did not become available")
                replacement, _ = connect(endpoint, 5, identity, allow_refusal=True, timeout=remaining)
                if replacement is not None:
                    idle[index] = cleanup.enter_context(replacement)
                    result["reconnected"] += 1
                    result["replacement_max_us"] = max(result["replacement_max_us"],
                                                        (time.perf_counter() - began) * 1_000_000)
                    break
                result["replacement_refusals"] += 1
                time.sleep(min(0.001, remaining))
        ping(idle[:replaced])
        result["admission_after_retirement"] = True

        result["phase"] = "saturated"
        result["saturated"] = healthy_flow(traffic, fast, healthy)
        # Match both the full subscriber and an otherwise healthy wildcard subscriber.
        fast.sendall(small_frame(0x82, b"\0\2\0" + topic(b"pressure/+") + b"\2"))
        assert read(fast, 6) == b"\x90\4\0\2\0\2"
        slow.sendall(small_frame(0x50, first))
        assert read(slow, 4) == small_frame(0x62, first)
        publish_qos2(producer, blocked, capacity + 1, b"still rejected", quota=True)
        result["quota_exceeded"] += 1
        ping([slow, fast])
        result["fanout_quota_atomic"] = True

        result["phase"] = "drain"
        slow.sendall(small_frame(0x70, first))
        second = receive_qos2(slow, blocked, b"\0\2")
        publish_qos2(producer, blocked, capacity + 1, b"reused")
        finish_qos2(fast, receive_qos2(fast, blocked, b"reused"))
        result["quota_reclaimed_on_pubcomp"] = True
        finish_qos2(slow, second)
        for number in range(3, capacity + 1):
            finish_qos2(slow, receive_qos2(slow, blocked, struct.pack("!H", number)))
        finish_qos2(slow, receive_qos2(slow, blocked, b"reused"))
        ping([slow, fast])
        result["reliable_deliveries_drained"] = capacity + 1
        result["recovered"] = healthy_flow(traffic, fast, healthy)

        result["phase"] = "retire"
        ping(idle)
        for peer in [slow, producer, fast, traffic, *idle]:
            peer.sendall(b"\xe0\0")
            eof(peer)


def probe_pressure(program, clients):
    result = {"platform": platform.platform(), "broker_sha256": hashlib.sha256(program.read_bytes()).hexdigest(),
              "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), "clients": clients,
              "phase": "startup", "connected": 0, "reconnected": 0, "replacement_refusals": 0,
              "replacement_max_us": 0.0, "complete": False}
    started = time.perf_counter()
    try:
        drive_pressure(program, clients, result)
    except (AssertionError, OSError, queue.Empty, subprocess.SubprocessError) as error:
        result["failure"] = f"{type(error).__name__}: {error}".rstrip()
    else:
        result.update(phase="complete", complete=True)
    result["seconds"] = time.perf_counter() - started
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--pressure-clients", type=int, metavar="5..10000",
                        help="run the separate MQTT 5 live-quota and healthy-traffic probe")
    args = parser.parse_args()
    if args.pressure_clients is None:
        result = probe(args.program)
    else:
        if not 5 <= args.pressure_clients <= 10000:
            parser.error("--pressure-clients must be 5..10000")
        result = probe_pressure(args.program, args.pressure_clients)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if args.pressure_clients is None:
        print("[pass] separate Python MQTT 5/3.1.1 peer, PING, takeover, exact, wildcard, shared and retained routing bidirectional QoS 1/2, persistent resume and Wills")
    elif result["complete"]:
        print(f"[pass] {result['clients']} MQTT 5 connections, {result['reconnected']} replacements and live quota pressure")
    else:
        print(f"[fail] {result['phase']} with {result['connected']}/{result['clients']} MQTT 5 connections"
              f" ({result['failure']})")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
