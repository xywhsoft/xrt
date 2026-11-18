"""Independent JSON/framing peer for the redirected stdio example."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager, ExitStack
import hashlib
import json
import math
import os
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


def command(executable, binary, driver):
    mode = "serve-event" if driver == "event" else "serve"
    return [str(executable), mode] + (["be"] if binary else [])


@contextmanager
def windows_pipe(reader_is_child):
    import _winapi
    from asyncio.windows_utils import pipe
    import msvcrt

    handles = list(pipe(overlapped=(reader_is_child, not reader_is_child), bufsize=4096))
    streams = []
    try:
        for index, mode in enumerate(("rb", "wb")):
            fd = msvcrt.open_osfhandle(handles[index], os.O_BINARY)
            handles[index] = None
            try:
                streams.append(os.fdopen(fd, mode))
            except BaseException:
                os.close(fd)
                raise
        yield streams
    finally:
        for stream in streams:
            stream.close()
        for handle in handles:
            if handle is not None:
                _winapi.CloseHandle(handle)


@contextmanager
def peer(executable, binary, driver):
    args = command(executable, binary, driver)
    with ExitStack() as pipes:
        if os.name == "nt" and driver == "event":
            child_in, parent_in = pipes.enter_context(windows_pipe(reader_is_child=True))
            parent_out, child_out = pipes.enter_context(windows_pipe(reader_is_child=False))
            process = subprocess.Popen(args, stdin=child_in, stdout=child_out, stderr=subprocess.PIPE)
            process.stdin, process.stdout = parent_in, parent_out
            # Release the local child ends so peer closure propagates immediately.
            child_in.close()
            child_out.close()
        else:
            process = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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


def exercise(executable, binary, driver):
    first = ["雪", "\u0000", {"escape": "\r\n"}]
    foreign_id = "id-\u0000-\u03bb"
    large = ["".join(f"{i:08x}" for i in range(4096))]
    batch = [
        query("large", large),
        {"jsonrpc": "2.0", "method": "notification", "params": []},
        query(194, [1]),
    ]
    with peer(executable, binary, driver) as (p, pool):
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
        with peer(executable, binary, driver) as (p, pool):
            pool.submit(write, p.stdin, wire).result(timeout=10)
            p.stdin.close()
            assert p.wait(timeout=10) == 7 and not p.stdout.read(), (p.returncode, p.stderr.read())
    return {"checks": ["live_response_before_eof", "split_header_and_utf8_roundtrip",
                       "coalesced_batch_and_scalar", "32768_byte_echo_with_delayed_reader",
                       "clean_eof_drain", "invalid_frame", "truncated_eof"]}


def exercise_retirement(executable, binary):
    params = ["".join(f"{i:08x}" for i in range(32768))]
    wire = frame(query("blocked", params), binary)
    expected = frame({"jsonrpc": "2.0", "id": "blocked", "result": params}, binary)
    header = expected[:4] if binary else expected.split(b"\r\n\r\n", 1)[0] + b"\r\n\r\n"
    with peer(executable, binary, "event") as (p, pool):
        pool.submit(write, p.stdin, wire).result(timeout=10)
        prefix = pool.submit(exact, p.stdout, 1).result(timeout=10)
        assert prefix == expected[:1]
        time.sleep(0.030)
        # Keep the input writer open while breaking an in-flight output.
        p.stdout.close()
        assert p.wait(timeout=10) == 7, (p.returncode, p.stderr.read())
    with peer(executable, binary, "event") as (p, pool):
        pool.submit(write, p.stdin, wire).result(timeout=10)
        prefix = pool.submit(exact, p.stdout, 1).result(timeout=10)
        assert prefix == expected[:1]
        start = time.monotonic()
        p.stdin.close()
        # Leave the output handle open without reading until the EOF budget expires.
        assert p.wait(timeout=10) == 7, (p.returncode, p.stderr.read())
        elapsed = time.monotonic() - start
        assert elapsed >= 4.5, elapsed
        received = prefix + p.stdout.read()
        assert len(header) < len(received) < len(expected) and received.startswith(header), len(received)
        _, marker, partial = received[len(header):].partition(b'"result":["')
        assert marker and partial and params[0].encode("ascii").startswith(partial), len(received)
    return {"checks": ["reader_disappears_with_input_open", "unread_output_hits_eof_deadline"],
            "drain_wait_seconds": elapsed, "partial_output_bytes": len(received)}


def measure(executable, binary, driver, count):
    width = 32
    with peer(executable, binary, driver) as (p, pool):
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
    parser.add_argument("--driver", choices=("both", "event", "poll"), default="both")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    drivers = ("poll", "event") if args.driver == "both" else (args.driver,)
    report = {
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "scope": "Independent Python peer and C example over native pipes. Performance uses closed-loop groups of 32 and includes Python processing.",
        "profiles": {},
    }
    for binary in (False, True):
        name = "uint32_be" if binary else "content_length"
        results = {driver: exercise(executable, binary, driver) for driver in drivers}
        if "event" in results:
            results["event"]["retirement"] = exercise_retirement(executable, binary)
        if args.perf_count > 0:
            for repeat in range(3):
                for driver in (drivers if repeat % 2 == 0 else reversed(drivers)):
                    results[driver].setdefault("performance", []).append(
                        measure(executable, binary, driver, args.perf_count)
                    )
        report["profiles"][name] = results
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] stdio subprocess drivers {', '.join(drivers)} in both framing profiles")


if __name__ == "__main__":
    main()
