#!/usr/bin/env python3
"""Bounded, loopback-only MCP Streamable HTTP protocol probe."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class ProbeState:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.failures: list[str] = []
        self.modern_methods: list[str] = []
        self.legacy_methods: list[str] = []
        self.legacy_request_id: int | None = None
        self.modern_header_mismatch_sent = False
        self.legacy_initialized = False
        self.legacy_resumed = False
        self.legacy_deleted = False

    def fail(self, message: str) -> None:
        with self.lock:
            self.failures.append(message)


STATE = ProbeState()
LEGACY_SESSION = "legacy-session-1"


def complete_response(request_id: int, result: dict[str, object]) -> bytes:
    return json.dumps(
        {"jsonrpc": "2.0", "id": request_id, "result": result},
        separators=(",", ":"),
    ).encode("utf-8")


def tools_result(modern: bool) -> dict[str, object]:
    valid_schema: dict[str, object] = {
        "type": "object",
        "properties": {
            "text": {"type": "string", "x-mcp-header": "text"},
            "count": {"type": "integer", "x-mcp-header": "count"},
            "nested": {
                "type": "object",
                "properties": {
                    "flag": {"type": "boolean", "x-mcp-header": "flag"}
                },
            },
        },
        "required": ["text"],
        "additionalProperties": False,
    }
    result: dict[str, object] = {
        "tools": [
            {
                "name": "echo",
                "description": "Loopback Streamable HTTP echo fixture.",
                "inputSchema": valid_schema,
            }
        ]
    }
    if modern:
        result["tools"].append(  # type: ignore[union-attr]
            {
                "name": "malformed-header-schema",
                "description": "Must be excluded without poisoning discovery.",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "first": {"type": "string", "x-mcp-header": "dup"},
                        "second": {"type": "string", "x-mcp-header": "DUP"},
                    },
                },
            }
        )
        result.update(
            {"resultType": "complete", "ttlMs": 60000, "cacheScope": "private"}
        )
    return result


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, _format: str, *_args: object) -> None:
        return

    def reply(
        self,
        status: int,
        body: bytes = b"",
        content_type: str | None = None,
        extra_headers: dict[str, str] | None = None,
    ) -> None:
        self.send_response(status)
        if content_type:
            self.send_header("Content-Type", content_type)
        if extra_headers:
            for name, value in extra_headers.items():
                self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        if body:
            self.wfile.write(body)

    def reject(self, message: str) -> None:
        STATE.fail(f"{self.command} {self.path}: {message}")
        body = json.dumps(
            {
                "jsonrpc": "2.0",
                "id": None,
                "error": {"code": -32600, "message": message},
            },
            separators=(",", ":"),
        ).encode("utf-8")
        self.reply(400, body, "application/json")

    def common_headers_valid(self, modern: bool) -> bool:
        expected_version = "2026-07-28" if modern else "2025-11-25"
        expected_auth = (
            "Bearer modern-secret" if modern else "Bearer legacy-secret"
        )
        accept = self.headers.get("Accept", "")
        if self.headers.get("MCP-Protocol-Version") != expected_version:
            self.reject("wrong MCP protocol version header")
            return False
        if self.headers.get("Authorization") != expected_auth:
            self.reject("authorization header was not preserved")
            return False
        if "application/json" not in accept or "text/event-stream" not in accept:
            self.reject("Accept does not advertise JSON and SSE")
            return False
        return True

    def read_json(self) -> dict[str, object] | None:
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = self.rfile.read(length)
            value = json.loads(payload.decode("utf-8"))
            if not isinstance(value, dict):
                raise ValueError("request root is not an object")
            return value
        except (ValueError, UnicodeError, json.JSONDecodeError) as error:
            self.reject(f"invalid JSON request: {error}")
            return None

    def do_POST(self) -> None:  # noqa: N802
        if self.path == "/modern":
            self.handle_modern_post()
        elif self.path == "/legacy":
            self.handle_legacy_post()
        else:
            self.reject("unknown endpoint")

    def handle_modern_post(self) -> None:
        if not self.common_headers_valid(True):
            return
        if self.headers.get("Mcp-Session-Id"):
            self.reject("modern transport unexpectedly sent a session id")
            return
        request = self.read_json()
        if request is None:
            return
        method = request.get("method")
        request_id = request.get("id")
        if not isinstance(method, str) or not isinstance(request_id, int):
            self.reject("modern request lacks method or integer id")
            return
        if self.headers.get("Mcp-Method") != method:
            self.reject("modern Mcp-Method does not match JSON-RPC method")
            return
        with STATE.lock:
            STATE.modern_methods.append(method)
        if method == "server/discover":
            body = complete_response(
                request_id,
                {
                    "resultType": "complete",
                    "supportedVersions": ["2026-07-28"],
                    "capabilities": {"tools": {}},
                    "ttlMs": 60000,
                    "cacheScope": "private",
                },
            )
            self.reply(200, body, "application/json")
        elif method == "tools/list":
            notification = json.dumps(
                {
                    "jsonrpc": "2.0",
                    "method": "notifications/progress",
                    "params": {"progress": 1},
                },
                separators=(",", ":"),
            )
            response = complete_response(request_id, tools_result(True)).decode("utf-8")
            body = (
                f"event: message\ndata: {notification}\n\n"
                f"event: message\ndata: {response}\n\n"
            ).encode("utf-8")
            self.reply(200, body, "text/event-stream")
        elif method == "tools/call":
            expected = {
                "Mcp-Name": "echo",
                "Mcp-Param-text": "=?base64?aGVsbG8gd29ybGQ=?=",
                "Mcp-Param-count": "7",
                "Mcp-Param-flag": "true",
            }
            mismatches = [
                f"{name}={self.headers.get(name)!r}"
                for name, value in expected.items()
                if self.headers.get(name) != value
            ]
            if mismatches:
                self.reject("wrong modern routing headers: " + ", ".join(mismatches))
                return
            with STATE.lock:
                send_mismatch = not STATE.modern_header_mismatch_sent
                STATE.modern_header_mismatch_sent = True
            if send_mismatch:
                body = json.dumps(
                    {
                        "jsonrpc": "2.0",
                        "id": request_id,
                        "error": {
                            "code": -32020,
                            "message": "HeaderMismatch",
                        },
                    },
                    separators=(",", ":"),
                ).encode("utf-8")
                self.reply(400, body, "application/json")
                return
            result = {
                "resultType": "complete",
                "content": [{"type": "text", "text": "modern http ok"}],
                "structuredContent": {"transport": "http"},
                "isError": False,
            }
            self.reply(200, complete_response(request_id, result), "application/json")
        else:
            self.reject(f"unexpected modern method {method}")

    def handle_legacy_post(self) -> None:
        if not self.common_headers_valid(False):
            return
        request = self.read_json()
        if request is None:
            return
        method = request.get("method")
        if not isinstance(method, str):
            self.reject("legacy request lacks method")
            return
        with STATE.lock:
            STATE.legacy_methods.append(method)
        request_id = request.get("id")
        if method == "initialize":
            if self.headers.get("Mcp-Session-Id"):
                self.reject("initialize sent a session id before negotiation")
                return
            if not isinstance(request_id, int):
                self.reject("initialize lacks integer id")
                return
            result = {
                "protocolVersion": "2025-11-25",
                "capabilities": {"tools": {"listChanged": False}},
                "serverInfo": {"name": "http-probe", "version": "1.0"},
            }
            self.reply(
                200,
                complete_response(request_id, result),
                "application/json",
                {"Mcp-Session-Id": LEGACY_SESSION},
            )
            return
        if self.headers.get("Mcp-Session-Id") != LEGACY_SESSION:
            self.reject("legacy request lacks the negotiated session id")
            return
        if method == "notifications/initialized":
            with STATE.lock:
                STATE.legacy_initialized = True
            self.reply(202)
        elif method == "tools/list":
            if not isinstance(request_id, int):
                self.reject("legacy tools/list lacks integer id")
                return
            with STATE.lock:
                initialized = STATE.legacy_initialized
                STATE.legacy_request_id = request_id
            if not initialized:
                self.reject("legacy tools/list arrived before initialized notification")
                return
            notification = json.dumps(
                {
                    "jsonrpc": "2.0",
                    "method": "notifications/progress",
                    "params": {"progress": 1},
                },
                separators=(",", ":"),
            )
            body = f"id: cursor-1\nevent: message\ndata: {notification}\n\n".encode(
                "utf-8"
            )
            self.reply(200, body, "text/event-stream")
        elif method == "tools/call":
            if not isinstance(request_id, int):
                self.reject("legacy tools/call lacks integer id")
                return
            result = {
                "content": [{"type": "text", "text": "legacy http ok"}],
                "isError": False,
            }
            self.reply(200, complete_response(request_id, result), "application/json")
        else:
            self.reject(f"unexpected legacy method {method}")

    def do_GET(self) -> None:  # noqa: N802
        if self.path != "/legacy" or not self.common_headers_valid(False):
            if self.path != "/legacy":
                self.reject("GET is only valid for the legacy endpoint")
            return
        if self.headers.get("Mcp-Session-Id") != LEGACY_SESSION:
            self.reject("legacy resume lacks session id")
            return
        if self.headers.get("Last-Event-ID") != "cursor-1":
            self.reject("legacy resume lacks the last SSE event id")
            return
        with STATE.lock:
            request_id = STATE.legacy_request_id
            STATE.legacy_resumed = True
        if request_id is None:
            self.reject("legacy resume has no pending request")
            return
        response = complete_response(request_id, tools_result(False)).decode("utf-8")
        body = f"id: cursor-2\nevent: message\ndata: {response}\n\n".encode("utf-8")
        self.reply(200, body, "text/event-stream")

    def do_DELETE(self) -> None:  # noqa: N802
        if self.path != "/legacy" or not self.common_headers_valid(False):
            if self.path != "/legacy":
                self.reject("DELETE is only valid for the legacy endpoint")
            return
        if self.headers.get("Mcp-Session-Id") != LEGACY_SESSION:
            self.reject("legacy DELETE lacks session id")
            return
        with STATE.lock:
            STATE.legacy_deleted = True
            STATE.legacy_methods.append("DELETE")
        self.reply(200, b"{}", "application/json")


def main() -> int:
    default_name = "test_xwork.exe" if os.name == "nt" else "test_xwork"
    executable = Path(sys.argv[1] if len(sys.argv) > 1 else f"build/{default_name}")
    executable = executable.resolve()
    if not executable.is_file():
        print(f"missing xwork test executable: {executable}", file=sys.stderr)
        return 2

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    port = server.server_address[1]
    env = os.environ.copy()
    env["NO_PROXY"] = "127.0.0.1,localhost"
    env["no_proxy"] = "127.0.0.1,localhost"
    try:
        completed = subprocess.run(
            [
                str(executable),
                "--mcp-http-probe",
                f"http://127.0.0.1:{port}/modern",
                f"http://127.0.0.1:{port}/legacy",
            ],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=20,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        print(error.stdout or "", end="")
        print("bounded MCP HTTP probe timed out", file=sys.stderr)
        return 1
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)

    print(completed.stdout, end="")
    expected_modern = [
        "server/discover",
        "tools/list",
        "tools/call",
        "tools/list",
        "tools/call",
    ]
    with STATE.lock:
        if STATE.modern_methods != expected_modern:
            STATE.failures.append(
                f"modern method sequence {STATE.modern_methods!r}, expected {expected_modern!r}"
            )
        if not STATE.legacy_initialized:
            STATE.failures.append("legacy initialized notification was not observed")
        if not STATE.legacy_resumed:
            STATE.failures.append("legacy SSE response was not resumed with GET")
        if not STATE.legacy_deleted:
            STATE.failures.append("legacy session was not terminated with DELETE")
        failures = list(STATE.failures)
    for failure in failures:
        print(f"HTTP fixture failure: {failure}", file=sys.stderr)
    if completed.returncode != 0:
        print(f"xwork HTTP probe exited with {completed.returncode}", file=sys.stderr)
    return 0 if completed.returncode == 0 and not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
