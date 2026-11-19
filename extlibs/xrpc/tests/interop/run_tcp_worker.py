"""Drive the TCP example with an independent socket peer."""
import json
from pathlib import Path
import socket
import subprocess
import sys
import threading


def record(value):
    return json.dumps(value).encode("utf-8") + b"\n"


def exchange(executable, drive):
    with subprocess.Popen([executable, "--serve"], stdout=subprocess.PIPE) as process:
        watchdog = threading.Timer(15, process.kill)
        watchdog.start()
        try:
            host, port = process.stdout.readline().decode("ascii").strip().split(":")
            assert host == "127.0.0.1"
            with socket.create_connection((host, int(port)), timeout=5) as peer:
                drive(peer)
            return process.wait(timeout=5)
        finally:
            watchdog.cancel()
            if process.poll() is None:
                process.kill()


def normal(peer):
    request = {"jsonrpc": "2.0", "id": "tcp-1", "method": "echo", "params": [42]}
    notification = {"jsonrpc": "2.0", "method": "echo"}
    batch = [notification, {"jsonrpc": "2.0", "id": 9, "method": "missing"}]
    wire = record(request)
    peer.sendall(wire[:-1])
    peer.sendall(wire[-1:] + record(notification) + record(batch) + b"[\n")
    peer.shutdown(socket.SHUT_WR)
    with peer.makefile("rb") as stream:
        messages = [json.loads(line) for line in stream]
    assert messages[0] == {"jsonrpc": "2.0", "id": "tcp-1", "result": [42]}
    assert len(messages) == 3 and len(messages[1]) == 1
    error = messages[1][0]
    assert error["id"] == 9 and error["error"]["code"] == -32601
    assert messages[2]["id"] is None and messages[2]["error"]["code"] == -32700


def main():
    executable, report = sys.argv[1:]
    executable = str(Path(executable).resolve())
    assert exchange(executable, normal) == 0
    assert exchange(executable, lambda peer: peer.sendall(b" " * 4097)) == 1
    assert exchange(executable, lambda peer: peer.sendall(b"{")) == 1
    out = Path(report)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"cases": 6, "replies": 3, "ok": True}) + "\n", encoding="utf-8")
    print("[pass] TCP worker dispatch, notification, batch, parse error and record bounds")


if __name__ == "__main__":
    main()
