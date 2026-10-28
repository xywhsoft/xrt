#!/usr/bin/env python3
"""Run a prebuilt client against three disposable Redis server configurations.

The service never uses an existing Redis instance, persists data or listens on
non-loopback interfaces. Synthetic credentials below are public test fixtures.
No Redis implementation code is used by the extension.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import socket
import subprocess
import tempfile
import time


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(client: Path, server: Path, scenario: int) -> dict:
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="xredis-interop-") as directory:
        root = Path(directory)
        auth = ""
        if scenario == 1:
            auth = "requirepass xredis-fixture-password\n"
        elif scenario == 2:
            auth = "user default off\nuser xredis-fixture-user on >xredis-fixture-password ~* +@all\n"
        config = root / "redis.conf"
        config.write_text(
            f'bind 127.0.0.1\nport {port}\nprotected-mode yes\nsave ""\nappendonly no\n'
            f"daemonize no\nloglevel notice\n{auth}", encoding="utf-8"
        )
        with (root / "server.log").open("wb") as log:
            process = subprocess.Popen([str(server), str(config)], cwd=root, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 10
                while True:
                    if process.poll() is not None:
                        raise RuntimeError(f"fixture server exited: {(root / 'server.log').read_text(encoding='utf-8', errors='replace')[-2000:]}")
                    # The ephemeral probe port is released before Redis binds.
                    # Require THIS child to report a successful bind/start before
                    # contacting it; a port-reuse race must never test another service.
                    startup = (root / "server.log").read_text(encoding="utf-8", errors="replace")
                    if "Ready to accept connections" in startup:
                        try:
                            with socket.create_connection(("127.0.0.1", port), timeout=0.25) as connection:
                                connection.sendall(b"*1\r\n$4\r\nPING\r\n")
                                response = connection.recv(4096)
                                if response.startswith((b"+PONG\r\n", b"-NOAUTH")) and process.poll() is None:
                                    break
                        except OSError:
                            pass
                    if time.monotonic() >= deadline:
                        raise TimeoutError("fixture Redis did not become ready")
                    time.sleep(0.025)
                result = subprocess.run([str(client), str(port), str(scenario)], check=False, capture_output=True, text=True, encoding="utf-8", timeout=90)
                print(result.stdout, end="", flush=True)
                if result.returncode:
                    raise RuntimeError(f"client exited {result.returncode}: {result.stderr[-4000:]}")
                return {"scenario": scenario, "result": "passed", "client_stdout": result.stdout}
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, default=shutil.which("redis-server"))
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.server is None:
        parser.error("redis-server is unavailable; provide --server")
    client = args.client.resolve(strict=True)
    # Redis installations can use symlinks to one argv[0]-dispatched executable.
    # Preserve redis-server's invocation name, while hashing its actual bytes.
    server = Path(args.server).absolute()
    if not server.is_file():
        parser.error("Redis server executable does not exist")
    source = Path(__file__).with_name("redis_init.c")
    report = {
        "schema": 1, "purpose": "local initialization interoperability, not performance or production acceptance",
        "platform": platform.platform(), "server_path": str(server), "server_resolved_path": str(server.resolve(strict=True)), "server_sha256": digest(server),
        "server_version": subprocess.check_output([str(server), "--version"], text=True, encoding="utf-8").strip(),
        "client_sha256": digest(client), "client_source_sha256": digest(source), "runner_sha256": digest(Path(__file__)),
        "cases": [run(client, server, scenario) for scenario in range(3)],
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] real Redis initialization interoperability: {report['server_version']}")


if __name__ == "__main__":
    main()
