"""Exercise the standalone MQTT TCP server and subscription routes with a separate peer."""
import argparse
import json
from pathlib import Path
import queue
import socket
import struct
import subprocess
from threading import Thread


def read(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise AssertionError("truncated MQTT reply")
        data.extend(chunk)
    return bytes(data)


def connect(endpoint, level, client_id):
    sock = socket.create_connection(endpoint, timeout=5)
    body = b"\0\4MQTT" + bytes((level, 2, 0, 0))
    if level == 5:
        body += b"\0"
    body += struct.pack("!H", len(client_id)) + client_id
    # Send the protocol level in a later TCP write and pipeline the first PING.
    wire = bytes((0x10, len(body))) + body
    sock.sendall(wire[:8])
    sock.sendall(wire[8:] + b"\xc0\0")
    header = read(sock, 2)
    assert header[0] == 0x20 and header[1] < 64, header.hex()
    ack = read(sock, header[1])
    assert ack[:2] == b"\0\0", ack.hex()
    if level == 4:
        assert ack == b"\0\0", ack.hex()
    assert read(sock, 2) == b"\xd0\0"
    return sock, ack.hex()


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
    assert read(legacy, 5) == b"\x90\x03\0\1\0"
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


def eof(sock):
    try:
        assert sock.recv(1) == b"", "extra MQTT data after closure"
    except ConnectionResetError:
        pass


def probe(program):
    with subprocess.Popen(
        [str(program.resolve()), "wait"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
    ) as server:
        start = queue.Queue(maxsize=1)
        Thread(target=lambda: start.put(server.stdout.readline()), daemon=True).start()
        try:
            host, sep, port = start.get(timeout=8).strip().partition(":")
            assert host == "127.0.0.1" and sep and int(port) > 0, (host, port)
            endpoint = (host, int(port))
            preserved, connack5 = connect(endpoint, 5, b"literal")
            with preserved:
                legacy, connack311 = connect(endpoint, 4, b"legacy")
                with legacy:
                    newer, duplicate = connect(endpoint, 5, b"literal")
                    with newer:
                        eof(preserved)
                        legacy.sendall(b"\xc0\0")
                        assert read(legacy, 2) == b"\xd0\0", "other session must remain connected"
                        check_routes(legacy, newer)
                        check_wildcards(legacy, newer)
                        check_shared(endpoint, legacy, newer)
                        newer.sendall(b"\xe0\0")
                        eof(newer)
            server.stdin.write("\n")
            server.stdin.flush()
            assert server.wait(timeout=8) == 0, "server teardown failed"
        finally:
            if server.poll() is None:
                server.kill()
    return {"mqtt5_connack": connack5, "mqtt311_connack": connack311,
            "takeover_connack": duplicate, "remaining_peer_ping": True,
            "dual_version_routes": True, "wildcard_overlap": True, "shared_routing": True, "exact_case": True, "no_local": True, "unsubscribe": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    result = probe(args.program)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("[pass] separate Python MQTT 5/3.1.1 peer, PING, takeover, exact, wildcard and shared routing")


if __name__ == "__main__":
    main()
