"""Run deterministic localhost response-failure tests for xacme HTTP.

Build xacme_tests/test_http first, then run this script on Windows or Linux.
The cases cover partial responses, a dropped read-only GET, a POST accepted
before connection close, and a slow reader holding a large POST in send.
Writes may not replay.
"""

from __future__ import annotations

import argparse
import http.server
import os
from pathlib import Path
import subprocess
import sys
import threading
import time


ROOT = Path(__file__).resolve().parents[1]


class FaultServer(http.server.ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        super().__init__(("127.0.0.1", 0), FaultHandler)
        self.hits = 0
        self.lock = threading.Lock()


class FaultHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self):
        with self.server.lock:
            self.server.hits += 1
            hits = self.server.hits
        if self.path == "/get-drop":
            if hits == 1:
                self.close_connection = True
                return
            self.send_response(200)
            self.send_header("Content-Length", "2")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"ok")
            self.close_connection = True
            return
        self.send_response(200)
        self.send_header("Location", "/partial")
        self.send_header("Replay-Nonce", "partial")
        self.send_header("Content-Length", "8")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(b"short")
        self.wfile.flush()
        if self.path == "/timeout":
            time.sleep(2.0)
        self.close_connection = True

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        with self.server.lock:
            self.server.hits += 1
        if self.path == "/send-slow":
            self.connection.settimeout(0.2)
            deadline = time.monotonic() + 5.0
            received = 0
            while received < length and time.monotonic() < deadline:
                try:
                    chunk = self.connection.recv(min(16384, length - received))
                except TimeoutError:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                received += len(chunk)
                time.sleep(0.1)
            self.close_connection = True
            return
        self.rfile.read(length)
        self.close_connection = True

    def log_message(self, _format, *_args):
        pass


def run_case(kind: str, test_http: Path) -> None:
    server = FaultServer()
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        env = dict(os.environ)
        url = f"http://127.0.0.1:{server.server_port}/{kind}"
        if kind == "get-drop":
            env["XACME_TEST_URL"] = url
            env["XACME_TEST_LOCAL_TIMEOUT"] = "1"
            expected_hits = 2
        elif kind in ("post-drop", "post-once-drop"):
            env["XACME_TEST_POST_DROP_URL"] = url
            if kind == "post-once-drop":
                env["XACME_TEST_POST_DROP_ONCE"] = "1"
            expected_hits = 1
        elif kind == "send-slow":
            env["XACME_TEST_SLOW_SEND_URL"] = url
            expected_hits = 1
        else:
            env["XACME_TEST_FAULT_URL"] = url
            env["XACME_TEST_FAULT_KIND"] = kind
            expected_hits = 1
        started = time.monotonic()
        result = subprocess.run(
            [str(test_http)], cwd=ROOT, env=env,
            capture_output=True, text=True, timeout=12,
            encoding="utf-8", errors="replace",
        )
        elapsed = time.monotonic() - started
        if result.returncode != 0:
            raise AssertionError(
                f"{kind} client failed:\n{result.stdout}{result.stderr}"
            )
        with server.lock:
            hits = server.hits
        if hits != expected_hits:
            raise AssertionError(
                f"{kind} expected {expected_hits} requests, got {hits}"
            )
        if kind == "send-slow" and elapsed >= 3.2:
            raise AssertionError(
                f"{kind} exceeded the total send deadline: {elapsed:.2f}s"
            )
        print(f"[PASS] acme http {kind}: {hits} request(s), {elapsed:.2f}s")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--binary", type=Path,
                        help="built test_http executable (for alternate output directories)")
    args = parser.parse_args()
    binary = "test_http.exe" if sys.platform.startswith(
        ("win", "msys", "cygwin")
    ) else "test_http"
    test_http = (ROOT / args.binary if args.binary is not None else
                 ROOT / "out" / args.compiler / "native/xacme_tests" / binary)
    if not test_http.is_file():
        raise SystemExit(f"build test_http first: {test_http}")
    run_case("truncated", test_http)
    run_case("timeout", test_http)
    run_case("get-drop", test_http)
    run_case("post-drop", test_http)
    run_case("post-once-drop", test_http)
    run_case("send-slow", test_http)


if __name__ == "__main__":
    main()
