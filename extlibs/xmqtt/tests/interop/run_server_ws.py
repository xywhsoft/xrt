#!/usr/bin/env python3
"""Independent WebSocket and WSS peer for the MQTT broker sample."""
import argparse
import base64
import hashlib
import json
import os
import queue
import socket
import ssl
import struct
import subprocess
from pathlib import Path
from threading import Thread

from run_server_handshake import read, small_frame, topic
from run_server_tls import connack, finish, ping_measurement, receive, verify


def websocket_frame(opcode, payload, final=True):
    size = len(payload)
    mask = os.urandom(4)
    head = bytes(((0x80 if final else 0) | opcode,))
    if size < 126:
        head += bytes((128 + size,))
    elif size < 65536:
        head += b"\xfe" + struct.pack("!H", size)
    else:
        head += b"\xff" + struct.pack("!Q", size)
    return head + mask + bytes(value ^ mask[index % 4] for index, value in enumerate(payload))


def read_http_head(sock):
    head = bytearray()
    while not head.endswith(b"\r\n\r\n"):
        chunk = sock.recv(1)
        if not chunk:
            break
        head += chunk
        assert len(head) <= 1024
    return bytes(head)


class WsPeer:
    def __init__(self, sock, early=b"", *, protocols=b"other, mqtt"):
        self.sock = sock
        self.input = b""
        self.close_reply = None
        key = base64.b64encode(os.urandom(16))
        query = (b"GET /mqtt HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                 b"Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                 b"Sec-WebSocket-Key: " + key + b"\r\nSec-WebSocket-Protocol: " + protocols + b"\r\n\r\n")
        sock.sendall(query + early)
        head = read_http_head(sock)
        assert head.startswith(b"HTTP/1.1 101 "), head
        fields = (field.split(b": ", 1) for field in head.split(b"\r\n")[1:-2])
        headers = {name.lower(): value for name, value in fields}
        assert headers[b"sec-websocket-protocol"] == b"mqtt"
        accept = base64.b64encode(hashlib.sha1(key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest())
        assert headers[b"sec-websocket-accept"] == accept

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        if self.sock:
            self.sock.close()

    def settimeout(self, seconds):
        self.sock.settimeout(seconds)

    def setsockopt(self, *args):
        self.sock.setsockopt(*args)

    def sendall(self, payload):
        self.sock.sendall(websocket_frame(2, payload))

    def recv(self, size):
        while not self.input and self.close_reply is None:
            prefix = self.sock.recv(1)
            if not prefix:
                return b""
            first, second = prefix[0], read(self.sock, 1)[0]
            assert not second & 128 and not first & 0x70
            length = second & 127
            if length == 126:
                length = struct.unpack("!H", read(self.sock, 2))[0]
            elif length == 127:
                length = struct.unpack("!Q", read(self.sock, 8))[0]
            assert length <= 1048576
            data = read(self.sock, length)
            opcode = first & 15
            if opcode == 8:
                self.sock.sendall(websocket_frame(8, data))
                self.close_reply = data
            elif opcode == 9:
                self.sock.sendall(websocket_frame(10, data))
            else:
                assert opcode == 2 and first & 128
                self.input = data
        data, self.input = self.input[:size], self.input[size:]
        return data

    def unwrap(self):
        assert self.recv(1) == b"" and self.close_reply is not None, "expected WebSocket close"
        sock, self.sock = self.sock, None
        if isinstance(sock, ssl.SSLSocket):
            return sock.unwrap()
        return sock


class WsContext:
    def __init__(self, tls=None):
        self.tls = tls

    def transport(self, sock):
        return self.tls.wrap_socket(sock, server_hostname="localhost") if self.tls else sock

    def wrap_socket(self, sock, server_hostname):
        return WsPeer(self.transport(sock))


def check_boundaries(endpoint, context):
    first = small_frame(0x10, b"\0\4MQTT\5\2\0\0\0" + topic(b"early"))
    with WsPeer(context.transport(socket.create_connection(endpoint, timeout=10)),
                websocket_frame(2, first[:3], False)) as peer:
        peer.sock.sendall(websocket_frame(0, first[3:11]) +
                          websocket_frame(2, first[11:] + b"\xc0\0\xc0\0"))
        connack(peer)
        assert receive(peer) == (0xd0, b"")
        assert receive(peer) == (0xd0, b"")
        finish(peer)
    with context.wrap_socket(socket.create_connection(endpoint, timeout=10), "localhost") as peer:
        peer.sock.sendall(websocket_frame(1, b"text"))
        peer.unwrap().close()
        assert peer.close_reply[:2] == struct.pack("!H", 1003)

    with context.transport(socket.create_connection(endpoint, timeout=10)) as sock:
        sock.sendall(b"GET /mqtt HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                     b"Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                     b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                     b"Sec-WebSocket-Protocol: different\r\n\r\n")
        try:
            reply = read_http_head(sock)
        except ConnectionResetError:
            reply = b""
        assert not reply.startswith(b"HTTP/1.1 101 "), reply


def probe(program, credentials, secure, count):
    command = [str(program.resolve()), "wait"]
    tls = None
    if secure:
        tls = ssl.create_default_context(cafile=str(credentials / "certificate.pem"))
        tls.minimum_version = ssl.TLSVersion.TLSv1_3
        command += [str(credentials / "certificate.der"), str(credentials / "key.der")]
    context = WsContext(tls)
    with subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as server:
        endpoint = queue.Queue(maxsize=1)
        Thread(target=lambda: endpoint.put(server.stdout.readline()), daemon=True).start()
        try:
            host, sep, port = endpoint.get(timeout=10).strip().partition(":")
            assert host == "127.0.0.1" and sep, (host, port)
            address = (host, int(port))
            check_boundaries(address, context)
            verify(address, context)
            ping = ping_measurement(address, context, count) if count else None
            with context.transport(socket.create_connection(address, timeout=10)) as pending:
                pending.sendall(b"GET /mqtt HTTP/1.1\r\n")
                server.stdin.write("\n")
                server.stdin.flush()
                try:
                    assert pending.recv(1) == b"", "unfinished HTTP Upgrade emitted data during shutdown"
                except ConnectionResetError:
                    pass
                assert server.wait(timeout=10) == 0, "broker did not drain unfinished upgrades"
        finally:
            if server.poll() is None:
                server.kill()
    result = {"tls": secure, "upgrade_suffix": True, "fragmented_mqtt": True,
              "binary_only": True, "subprotocol": "mqtt", "qos_1_and_2": True,
              "live_takeover": True, "queued_qos1_deliveries": 48,
              "unfinished_upgrade_drained": True}
    if ping is not None:
        result["loopback_ping"] = ping
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("credentials", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--ping-samples", type=int, default=0)
    args = parser.parse_args()
    if args.ping_samples < 0:
        parser.error("--ping-samples must be nonnegative")
    results = {mode: probe(args.program, args.credentials, secure, args.ping_samples)
               for mode, secure in (("ws", False), ("wss", True))}
    args.report.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print("[pass] independent WS/WSS peer, Upgrade suffix, split MQTT, QoS, takeover and shutdown")


if __name__ == "__main__":
    main()
