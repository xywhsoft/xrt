#!/usr/bin/env python3
"""Real loopback HTTP errors and explicit recovery, without external services."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import threading
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

STATUS = dict(auth=401, payment=402, permission=403, limit=429, malformed=503,
              missing=404, method=405, **{"false-success": 401, "wrong-id": 429},
              notification=401, listing=503, expired=404)
STAGE = dict(notification="notifications/initialized", listing="tools/list", expired="tools/list")
MODERN = {"modern-auth": 401, "modern-wrong-id": 400, "modern-both": 400}
STATUS.update(MODERN)
STAGE.update({name: "tools/call" for name in MODERN})


class ErrorPeer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        super().__init__(("127.0.0.1", 0), Handler)
        self.lock = threading.Lock()
        self.failed: set[str] = set()
        self.calls: Counter = Counter()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def reply(self, status, body=b"", content_type="application/json", session=False):
        if isinstance(body, dict):
            body = json.dumps(body).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        if session:
            self.send_header("Mcp-Session-Id", "qa-session")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        name = self.path.rsplit("/", 1)[-1]
        request = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        method, request_id = request["method"], request.get("id")
        with self.server.lock:
            self.server.calls[(name, method)] += 1
            fail = name not in self.server.failed and method == STAGE.get(name, "initialize")
            if fail:
                self.server.failed.add(name)
        if fail:
            body = {"jsonrpc": "2.0", "id": request_id,
                    "error": {"code": -32020, "message": "qa-private-marker"}}
            content_type = "application/json"
            if name == "payment":
                body = {"error": "qa-private-marker"}
            elif name == "permission":
                body["id"] = None
            elif name == "malformed":
                body = b"{qa-private-marker"
            elif name in ("missing", "method"):
                body, content_type = b"qa-private-marker", "text/plain"
            elif name == "false-success":
                body = {"jsonrpc": "2.0", "id": request_id,
                        "result": {"protocolVersion": "2025-11-25", "capabilities": {"tools": {}}}}
            elif name in ("wrong-id", "modern-wrong-id"):
                body["id"] = request_id + 99
            elif name == "modern-both":
                body["result"] = {}
            self.reply(STATUS[name], body, content_type)
        elif method == "server/discover":
            self.reply(200, {"jsonrpc": "2.0", "id": request_id, "result": {
                "resultType": "complete", "supportedVersions": ["2026-07-28"],
                "capabilities": {"tools": {}}, "ttlMs": 60000, "cacheScope": "private"}})
        elif method == "initialize":
            self.reply(200, {"jsonrpc": "2.0", "id": request_id,
                            "result": {"protocolVersion": "2025-11-25", "capabilities": {"tools": {}}}},
                       session=name == "expired")
        elif method == "notifications/initialized":
            self.reply(202)
        elif method == "tools/list":
            result = {"tools": []}
            if name in MODERN:
                result.update(resultType="complete", ttlMs=60000, cacheScope="private")
                result["tools"] = [{"name": "echo", "description": "Loopback fixture.",
                                   "inputSchema": {"type": "object", "properties": {}}}]
            self.reply(200, {"jsonrpc": "2.0", "id": request_id, "result": result})
        elif method == "tools/call":
            self.reply(200, {"jsonrpc": "2.0", "id": request_id, "result": {
                "resultType": "complete", "content": [{"type": "text", "text": "ok"}]}})
        else:
            self.reply(400)

    def do_DELETE(self):
        self.reply(200, {})


def main():
    executable = Path(sys.argv[1]).resolve()
    server = ErrorPeer()
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    env = dict(os.environ, NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")
    try:
        result = subprocess.run([str(executable), "--mcp-http-errors",
                                 f"http://127.0.0.1:{server.server_port}/errors"],
                                env=env, capture_output=True, text=True, timeout=25)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        assert result.returncode == 0, f"probe exited with {result.returncode}"
        assert server.failed == set(STATUS), server.failed
        # A failure causes no automatic POST replay; only the explicit second
        # refresh is allowed to issue the next initialization/discovery request.
        for name in STATUS:
            if name in MODERN:
                actual = Counter({method: count for (case, method), count in server.calls.items() if case == name})
                assert actual == Counter({"server/discover": 1, "tools/list": 1, "tools/call": 1}), (name, actual)
                continue
            expected = Counter(initialize=2, **{"notifications/initialized": 1, "tools/list": 1})
            if name == "notification":
                expected["notifications/initialized"] = 2
            elif name == "listing":
                expected["notifications/initialized"] = 2
                expected["tools/list"] = 2
            elif name == "expired":
                expected["notifications/initialized"] = 2
                expected["tools/list"] = 2
            actual = Counter({method: count for (case, method), count in server.calls.items() if case == name})
            assert actual == expected, (name, actual, expected)
        print("PASS 12 HTTP failures, 12 explicit recoveries, 3 rejected replay signals")
        return 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == "__main__":
    raise SystemExit(main())
