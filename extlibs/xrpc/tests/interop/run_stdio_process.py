"""Run a native C xrpc parent against a standard-stdio JSON-RPC peer."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys


def exact(stream, size):
    data = stream.read(size)
    if len(data) != size:
        raise EOFError(f"incomplete frame, wanted {size}, read {len(data)}")
    return data


def peer(binary):
    source, dest = sys.stdin.buffer, sys.stdout.buffer
    requests = 0
    while True:
        lead = source.read(1)
        if not lead:
            return
        if binary:
            size = struct.unpack(">I", lead + exact(source, 3))[0]
        else:
            header = lead + source.readline(63)
            assert header.startswith(b"Content-Length: ") and header.endswith(b"\r\n"), header
            size = int(header[16:-2])
            assert exact(source, 2) == b"\r\n"
        assert 0 < size <= 1048576
        item = json.loads(exact(source, size))
        assert (
            item["jsonrpc"] == "2.0" and item["method"] == "echo"
            and item["params"] == [42] and item["id"] == str(requests + 1)
        ), item
        body = json.dumps(
            {"jsonrpc": "2.0", "id": item["id"], "result": item["params"]},
            separators=(",", ":"),
        ).encode()
        prefix = struct.pack(">I", len(body)) if binary else f"Content-Length: {len(body)}\r\n\r\n".encode()
        if not requests:
            for fragment in (prefix[:1], prefix[1:] + body[:1], body[1:]):
                dest.write(fragment)
                dest.flush()
        else:
            dest.write(prefix + body)
            dest.flush()
        requests += 1


def measure(executable, mode, count, window):
    result = subprocess.run(
        [str(executable), mode, str(count), str(window), sys.executable, str(Path(__file__).resolve()),
         "--peer", mode],
        capture_output=True, text=True, timeout=60, check=True,
    )
    stats = dict(token.split("=") for token in result.stdout.strip().split())
    assert int(stats["calls"]) == count and int(stats["window"]) == window, result
    return {key: float(value) for key, value in stats.items()}


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--peer":
        peer(sys.argv[2] == "be")
        return
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--perf-count", type=int, default=0)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    report = {
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "peer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "python": sys.version.split()[0],
        "profiles": {},
    }
    for mode in ("text", "be"):
        smoke = measure(executable, mode, 17, 8)
        report["profiles"][mode] = {"smoke": smoke}
    if args.perf_count:
        for repeat in range(3):
            for mode in (("text", "be") if repeat % 2 == 0 else ("be", "text")):
                data = measure(executable, mode, args.perf_count, 32)
                report["profiles"][mode].setdefault("runs", []).append(data)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("[PASS] native xrpc parent and independent subprocess in both framing profiles")


if __name__ == "__main__":
    main()
