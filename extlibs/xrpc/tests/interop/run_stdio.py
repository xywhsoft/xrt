"""Independent JSON/framing peer for the redirected stdio example."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import time


def frame(value, binary):
    payload = json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    prefix = struct.pack(">I", len(payload)) if binary else f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii")
    return prefix + payload


def exact(stream, count):
    data = stream.read(count)
    if len(data) != count:
        raise AssertionError(f"short output: expected {count}, got {len(data)}")
    return data


def response(stream, binary):
    if binary:
        length = struct.unpack(">I", exact(stream, 4))[0]
    else:
        header = stream.readline(64)
        assert header.startswith(b"Content-Length: ") and header.endswith(b"\r\n"), header
        length = int(header[16:-2])
        assert exact(stream, 2) == b"\r\n"
    assert 0 < length <= 1048576
    return json.loads(exact(stream, length).decode("utf-8"))


def write(stream, data):
    written = stream.write(data)
    assert written == len(data)
    stream.flush()


def command(executable, binary):
    return [str(executable), "serve"] + (["be"] if binary else [])


@contextmanager
def peer(executable, binary):
    process = subprocess.Popen(command(executable, binary), stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    pool = ThreadPoolExecutor(max_workers=2)
    try:
        yield process, pool
    finally:
        if process.poll() is None:
            process.kill()
        process.wait(timeout=10)
        pool.shutdown(wait=True)
        for stream in (process.stdin, process.stdout, process.stderr):
            stream.close()


def query(identity, params):
    return {"jsonrpc": "2.0", "method": "echo", "id": identity, "params": params}


def exercise(executable, binary):
    first = ["雪", "\u0000", {"escape": "\r\n"}]
    foreign_id = "id-\u0000-\u03bb"
    large = ["x" * 32768]
    batch = [
        query("large", large),
        {"jsonrpc": "2.0", "method": "notification", "params": []},
        query(194, [1]),
    ]
    with peer(executable, binary) as (p, pool):
        wire = frame(query(foreign_id, first), binary)
        incoming = pool.submit(response, p.stdout, binary)
        pool.submit(write, p.stdin, wire[:2]).result(timeout=10)
        pool.submit(write, p.stdin, wire[2:19]).result(timeout=10)
        pool.submit(write, p.stdin, wire[19:]).result(timeout=10)
        assert incoming.result(timeout=10) == {"jsonrpc": "2.0", "id": foreign_id, "result": first}

        wire = frame(batch, binary) + frame(query(195, [2]), binary)
        pool.submit(write, p.stdin, wire).result(timeout=10)
        p.stdin.close()
        # Supply enough response bytes to exhaust a typical anonymous pipe.
        # Keep the consumer paused briefly while the child observes input EOF.
        time.sleep(0.030)
        received = [pool.submit(response, p.stdout, binary).result(timeout=10) for _ in range(2)]
        outputs = next(v for v in received if isinstance(v, list))
        scalar = next(v for v in received if isinstance(v, dict))
        assert scalar == {"jsonrpc": "2.0", "id": 195, "result": [2]}
        assert {v["id"]: v["result"] for v in outputs} == {"large": large, 194: [1]}
        assert p.wait(timeout=10) == 0, p.stderr.read()
        assert p.stdout.read() == b""
    invalid = struct.pack(">I", 1048577) if binary else b"Content-Length: 1\r\nContent-Length: 1\r\n\r\n{}"
    truncated = frame(query(9, [9]), binary)[:-1]
    for wire in (invalid, truncated):
        p = subprocess.run(command(executable, binary), input=wire, capture_output=True, timeout=10)
        assert p.returncode == 7 and not p.stdout, (p.returncode, p.stdout, p.stderr)
    return {"checks": ["live_response_before_eof", "split_header_and_utf8_roundtrip",
                       "coalesced_batch_and_scalar", "32768_byte_echo_with_delayed_reader",
                       "clean_eof_drain", "invalid_frame", "truncated_eof"]}


def measure(executable, binary, count):
    width = 32
    with peer(executable, binary) as (p, pool):
        for offset in (0, width):
            payload = b"".join(frame(query(i, [i]), binary) for i in range(offset, offset + width))
            pool.submit(write, p.stdin, payload).result(timeout=10)
            for _ in range(width):
                pool.submit(response, p.stdout, binary).result(timeout=10)
        durations = []
        tick = time.perf_counter_ns()
        for start in range(0, count, width):
            identities = set(range(start, min(start + width, count)))
            payload = b"".join(frame(query(i, [i]), binary) for i in sorted(identities))
            offered = time.perf_counter_ns()
            pool.submit(write, p.stdin, payload).result(timeout=10)
            for _ in range(len(identities)):
                item = pool.submit(response, p.stdout, binary).result(timeout=10)
                identity = item["id"]
                assert identity in identities and item["result"] == [identity], item
                identities.remove(identity)
                durations.append(time.perf_counter_ns() - offered)
            assert not identities
        elapsed = time.perf_counter_ns() - tick
        p.stdin.close()
        assert p.wait(timeout=10) == 0 and not p.stdout.read()
    durations.sort()
    return {
        "calls": count, "window": width, "calls_per_second": count * 1e9 / elapsed,
        "p50_us": durations[math.ceil(count * .50) - 1] / 1000,
        "p99_us": durations[math.ceil(count * .99) - 1] / 1000,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--perf-count", type=int, default=0)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    report = {
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "scope": "Parent Python and example C over native diverted pipes. Performance uses closed-loop groups of 32 and includes the driver's idle poll and Python overhead.",
        "profiles": {},
    }
    for binary in (False, True):
        name = "uint32_be" if binary else "content_length"
        result = exercise(executable, binary)
        if args.perf_count > 0:
            result["performance"] = [measure(executable, binary, args.perf_count) for _ in range(3)]
        report["profiles"][name] = result
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("[PASS] stdio native subprocess profiles, fragmented/coalesced documents, clean/truncated EOF, output pressure")


if __name__ == "__main__":
    main()
