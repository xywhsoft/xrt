#!/usr/bin/env python3
"""使用无第三方依赖的原始 RFC 6455 peer 验证 XRT 双向互操作。"""

from __future__ import annotations

import argparse
import base64
import hashlib
import os
import socket
import struct
import subprocess
import sys
import time
import zlib


GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
PROTOCOL = "xrt.interop"
ORIGIN = "https://interop.test"
PMD = "permessage-deflate; server_no_context_takeover; client_no_context_takeover"
MAX_HTTP = 65536
MAX_PAYLOAD = 1024 * 1024


class InteropError(RuntimeError):
    """表示外部 peer 违反互操作契约。"""


def require(condition: bool, message: str) -> None:
    """在首个协议断言失败处终止当前方向。"""

    if not condition:
        raise InteropError(message)


class Wire:
    """保留一次 TCP 读取中跨越 HTTP 和 WebSocket 边界的余量。"""

    def __init__(self, sock: socket.socket) -> None:
        self.sock = sock
        self.buffer = bytearray()

    def exact(self, size: int) -> bytes:
        """精确读取指定字节数，并优先消费已经缓冲的余量。"""

        while len(self.buffer) < size:
            chunk = self.sock.recv(max(4096, size - len(self.buffer)))
            if not chunk:
                raise InteropError("unexpected EOF")
            self.buffer.extend(chunk)
        result = bytes(self.buffer[:size])
        del self.buffer[:size]
        return result

    def through(self, delimiter: bytes, limit: int) -> bytes:
        """读取到分隔符并保留同一 TCP 分块中的后续字节。"""

        while True:
            index = self.buffer.find(delimiter)
            if index >= 0:
                require(index <= limit, "HTTP handshake exceeds limit")
                result = bytes(self.buffer[:index])
                del self.buffer[:index + len(delimiter)]
                return result
            require(len(self.buffer) <= limit, "HTTP handshake exceeds limit")
            chunk = self.sock.recv(4096)
            if not chunk:
                raise InteropError("EOF during HTTP handshake")
            self.buffer.extend(chunk)


def free_port() -> int:
    """向系统申请一个临时 IPv4 回环端口。"""

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def recv_http(wire: Wire) -> tuple[str, dict[str, str]]:
    """读取一份 HTTP/1 头，并按字段名合并重复字段。"""

    head = wire.through(b"\r\n\r\n", MAX_HTTP)
    lines = head.decode("iso-8859-1").split("\r\n")
    require(bool(lines) and bool(lines[0]), "empty HTTP handshake")
    headers: dict[str, str] = {}
    for line in lines[1:]:
        require(":" in line, "malformed handshake header")
        name, value = line.split(":", 1)
        key = name.strip().lower()
        value = value.strip()
        headers[key] = f"{headers[key]}, {value}" if key in headers else value
    return lines[0], headers


def token_contains(value: str, token: str) -> bool:
    """按 HTTP token 列表规则查找大小写不敏感成员。"""

    return any(
        part.strip().lower() == token.lower()
        for part in value.split(",")
    )


def websocket_accept(key: str) -> str:
    """计算 RFC 6455 Sec-WebSocket-Accept。"""

    digest = hashlib.sha1(key.encode("ascii") + GUID).digest()
    return base64.b64encode(digest).decode("ascii")


def deflate_message(payload: bytes) -> bytes:
    """按 permessage-deflate 无上下文模式编码一条消息。"""

    codec = zlib.compressobj(level=6, wbits=-15)
    compressed = codec.compress(payload) + codec.flush(zlib.Z_SYNC_FLUSH)
    require(compressed.endswith(b"\x00\x00\xff\xff"), "invalid local PMD suffix")
    return compressed[:-4]


def inflate_message(payload: bytes) -> bytes:
    """在硬输出上限内解码一条 permessage-deflate 消息。"""

    codec = zlib.decompressobj(wbits=-15)
    output = codec.decompress(payload + b"\x00\x00\xff\xff", MAX_PAYLOAD + 1)
    require(len(output) <= MAX_PAYLOAD, "inflated message exceeds limit")
    require(not codec.unconsumed_tail, "compressed message exceeds output limit")
    return output


def send_frame(
    sock: socket.socket,
    opcode: int,
    payload: bytes = b"",
    *,
    fin: bool = True,
    mask: bool,
    rsv1: bool = False,
) -> None:
    """写出一帧严格使用最短长度编码的 WebSocket 数据。"""

    require(0 <= opcode <= 0xF, "invalid local opcode")
    require(
        not (opcode >= 8 and (not fin or len(payload) > 125)),
        "invalid local control frame",
    )
    first = (0x80 if fin else 0) | (0x40 if rsv1 else 0) | opcode
    length = len(payload)
    second = 0x80 if mask else 0
    if length < 126:
        header = bytes((first, second | length))
    elif length <= 0xFFFF:
        header = bytes((first, second | 126)) + struct.pack("!H", length)
    else:
        header = bytes((first, second | 127)) + struct.pack("!Q", length)
    if mask:
        key = os.urandom(4)
        payload = bytes(
            value ^ key[index & 3]
            for index, value in enumerate(payload)
        )
        header += key
    sock.sendall(header + payload)


def recv_frame(wire: Wire, *, expect_mask: bool) -> tuple[bool, bool, int, bytes]:
    """读取并严格验证一帧线路头。"""

    first, second = wire.exact(2)
    fin = bool(first & 0x80)
    rsv1 = bool(first & 0x40)
    require((first & 0x30) == 0, "RSV2 or RSV3 set")
    opcode = first & 0x0F
    masked = bool(second & 0x80)
    require(masked == expect_mask, "incorrect masking direction")
    length = second & 0x7F
    if length == 126:
        length = struct.unpack("!H", wire.exact(2))[0]
        require(length >= 126, "non-minimal 16-bit payload length")
    elif length == 127:
        length = struct.unpack("!Q", wire.exact(8))[0]
        require(
            length >= 65536 and not (length & (1 << 63)),
            "invalid 64-bit payload length",
        )
    require(length <= MAX_PAYLOAD, "frame payload exceeds limit")
    key = wire.exact(4) if masked else b""
    payload = wire.exact(length)
    if masked:
        payload = bytes(
            value ^ key[index & 3]
            for index, value in enumerate(payload)
        )
    if opcode >= 8:
        require(fin and length <= 125 and not rsv1, "invalid control frame")
    return fin, rsv1, opcode, payload


class MessageReader:
    """跨控制帧保存一条分片消息的读取状态。"""

    def __init__(self) -> None:
        self.opcode: int | None = None
        self.compressed = False
        self.parts: list[bytes] = []
        self.size = 0

    def receive(self, wire: Wire, *, expect_mask: bool) -> tuple[int, bytes]:
        """返回一个控制帧或一条完成解压的逻辑消息。"""

        while True:
            fin, rsv1, opcode, payload = recv_frame(
                wire,
                expect_mask=expect_mask,
            )
            if opcode >= 8:
                return opcode, payload
            if opcode in (1, 2):
                require(self.opcode is None, "new data frame during fragmented message")
                self.opcode = opcode
                self.compressed = rsv1
            elif opcode == 0:
                require(
                    self.opcode is not None and not rsv1,
                    "invalid continuation frame",
                )
            else:
                raise InteropError(f"reserved opcode {opcode}")
            self.parts.append(payload)
            self.size += len(payload)
            require(self.size <= MAX_PAYLOAD, "message exceeds limit")
            if not fin:
                continue
            require(self.opcode is not None, "final continuation without message")
            message_opcode = self.opcode
            message = b"".join(self.parts)
            if self.compressed:
                message = inflate_message(message)
            self.opcode = None
            self.compressed = False
            self.parts.clear()
            self.size = 0
            return message_opcode, message


def wait_data(reader: MessageReader, wire: Wire, opcode: int) -> bytes:
    """等待指定数据消息并自动响应期间收到的 Ping。"""

    while True:
        event_opcode, payload = reader.receive(wire, expect_mask=False)
        if event_opcode == 9:
            send_frame(wire.sock, 10, payload, mask=True)
            continue
        require(event_opcode == opcode, f"expected opcode {opcode}, got {event_opcode}")
        return payload


def connect_retry(port: int, process: subprocess.Popen[str]) -> socket.socket:
    """等待 XRT 服务端完成异步启动。"""

    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        if process.poll() is not None:
            stdout, stderr = process.communicate()
            raise InteropError(
                f"XRT server exited early ({process.returncode}): {stdout}{stderr}"
            )
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        try:
            sock.connect(("127.0.0.1", port))
            return sock
        except OSError:
            sock.close()
            time.sleep(0.02)
    raise InteropError("timed out waiting for XRT server")


def finish_process(process: subprocess.Popen[str], label: str) -> None:
    """收割外部 peer 并把标准输出错误纳入失败信息。"""

    try:
        stdout, stderr = process.communicate(timeout=15.0)
    except subprocess.TimeoutExpired as error:
        process.kill()
        stdout, stderr = process.communicate()
        raise InteropError(f"{label} timed out: {stdout}{stderr}") from error
    require(
        process.returncode == 0,
        f"{label} failed ({process.returncode}): {stdout}{stderr}",
    )


def python_client_to_xrt_server(peer: str) -> None:
    """用原始 Python 客户端验证 XRT 服务端。"""

    port = free_port()
    process = subprocess.Popen(
        [peer, "server", str(port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        with connect_retry(port, process) as sock:
            wire = Wire(sock)
            key = base64.b64encode(os.urandom(16)).decode("ascii")
            request = (
                "GET /interop?peer=python HTTP/1.1\r\n"
                f"Host: 127.0.0.1:{port}\r\n"
                "Upgrade: websocket\r\n"
                "Connection: keep-alive, Upgrade\r\n"
                f"Sec-WebSocket-Key: {key}\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                f"Sec-WebSocket-Protocol: other, {PROTOCOL}\r\n"
                f"Sec-WebSocket-Extensions: {PMD}\r\n"
                f"Origin: {ORIGIN}\r\n"
                "\r\n"
            )
            sock.sendall(request.encode("ascii"))
            status, headers = recv_http(wire)
            require(
                status == "HTTP/1.1 101 Switching Protocols",
                "XRT server rejected handshake",
            )
            require(
                headers.get("sec-websocket-accept") == websocket_accept(key),
                "bad accept hash",
            )
            require(
                headers.get("sec-websocket-protocol") == PROTOCOL,
                "bad selected protocol",
            )
            require(
                headers.get("sec-websocket-extensions", "").lower() == PMD,
                "bad PMD response",
            )

            reader = MessageReader()
            text_payload = b"python-client-" * 64
            compressed = deflate_message(text_payload)
            split = max(1, len(compressed) // 2)
            send_frame(
                sock,
                1,
                compressed[:split],
                fin=False,
                mask=True,
                rsv1=True,
            )
            send_frame(sock, 0, compressed[split:], mask=True)
            require(
                wait_data(reader, wire, 1) == text_payload,
                "XRT server text echo mismatch",
            )

            binary_payload = b"\x00\x01python\xff"
            send_frame(sock, 2, binary_payload, mask=True)
            require(
                wait_data(reader, wire, 2) == binary_payload,
                "XRT server binary echo mismatch",
            )

            send_frame(sock, 9, b"python-ping", mask=True)
            opcode, payload = reader.receive(wire, expect_mask=False)
            require(
                opcode == 10 and payload == b"python-ping",
                "XRT server Pong mismatch",
            )

            close_payload = struct.pack("!H", 1000) + b"python-client-done"
            send_frame(sock, 8, close_payload, mask=True)
            opcode, payload = reader.receive(wire, expect_mask=False)
            require(
                opcode == 8
                and len(payload) >= 2
                and struct.unpack("!H", payload[:2])[0] == 1000,
                "XRT server Close reply mismatch",
            )
        finish_process(process, "XRT server peer")
    except Exception:
        if process.poll() is None:
            process.kill()
            process.communicate()
        raise


def parse_client_handshake(wire: Wire) -> dict[str, str]:
    """严格检查 XRT 客户端写出的请求目标和管理字段。"""

    request, headers = recv_http(wire)
    require(
        request == "GET /interop?peer=xrt HTTP/1.1",
        "unexpected XRT request target",
    )
    require(headers.get("upgrade", "").lower() == "websocket", "missing Upgrade")
    require(
        token_contains(headers.get("connection", ""), "upgrade"),
        "missing Connection upgrade",
    )
    require(headers.get("sec-websocket-version") == "13", "bad WebSocket version")
    require(
        token_contains(headers.get("sec-websocket-protocol", ""), PROTOCOL),
        "protocol not offered",
    )
    require(headers.get("origin") == ORIGIN, "Origin mismatch")
    require(headers.get("x-interop") == "xrt-client", "custom header mismatch")
    require(
        headers.get("sec-websocket-extensions", "").lower() == PMD,
        "PMD offer mismatch",
    )
    return headers


def python_server_for_xrt_client(peer: str) -> None:
    """用原始 Python 服务端验证 XRT 客户端。"""

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(5.0)
        port = int(listener.getsockname()[1])
        process = subprocess.Popen(
            [peer, "client", str(port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            conn, _ = listener.accept()
            with conn:
                conn.settimeout(5.0)
                wire = Wire(conn)
                headers = parse_client_handshake(wire)
                key = headers.get("sec-websocket-key", "")
                require(bool(key), "missing WebSocket key")
                response = (
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    f"Sec-WebSocket-Accept: {websocket_accept(key)}\r\n"
                    f"Sec-WebSocket-Protocol: {PROTOCOL}\r\n"
                    f"Sec-WebSocket-Extensions: {PMD}\r\n"
                    "\r\n"
                )
                conn.sendall(response.encode("ascii"))

                reader = MessageReader()
                got_text = False
                got_binary = False
                got_ping = False
                while not (got_text and got_binary and got_ping):
                    opcode, payload = reader.receive(wire, expect_mask=True)
                    if opcode == 1:
                        expected = b"xrt-client-" * 64
                        require(payload == expected, "XRT client text mismatch")
                        send_frame(
                            conn,
                            1,
                            deflate_message(payload),
                            mask=False,
                            rsv1=True,
                        )
                        got_text = True
                    elif opcode == 2:
                        require(payload == b"\x10xrt\x00\xff", "XRT client binary mismatch")
                        send_frame(conn, 2, payload, mask=False)
                        got_binary = True
                    elif opcode == 9:
                        require(payload == b"xrt-ping", "XRT client Ping mismatch")
                        send_frame(conn, 10, payload, mask=False)
                        got_ping = True
                    else:
                        raise InteropError(f"unexpected XRT client opcode {opcode}")

                close_payload = struct.pack("!H", 1000) + b"python-server-done"
                send_frame(conn, 8, close_payload, mask=False)
                opcode, payload = reader.receive(wire, expect_mask=True)
                require(
                    opcode == 8
                    and len(payload) >= 2
                    and struct.unpack("!H", payload[:2])[0] == 1000,
                    "XRT client Close reply mismatch",
                )
            finish_process(process, "XRT client peer")
        except Exception:
            if process.poll() is None:
                process.kill()
                process.communicate()
            raise


def main() -> int:
    """依次执行两个方向并输出发布门禁结果。"""

    parser = argparse.ArgumentParser()
    parser.add_argument("--peer", required=True, help="path to WebSocket interop peer")
    arguments = parser.parse_args()
    peer = os.path.abspath(arguments.peer)
    require(os.path.isfile(peer), f"peer executable not found: {peer}")

    python_client_to_xrt_server(peer)
    print("python client -> XRT server: PASS")
    python_server_for_xrt_client(peer)
    print("XRT client -> python server: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except InteropError as error:
        print(f"interop failure: {error}", file=sys.stderr)
        raise SystemExit(1)
