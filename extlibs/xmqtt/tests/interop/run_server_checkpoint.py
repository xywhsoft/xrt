#!/usr/bin/env python3
"""Verify restored retained messages against a new broker process."""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import queue
import subprocess
import tempfile
from threading import Thread
import time
import zlib

from run_server_handshake import connect, topic
from run_server_tls import frame, receive


PUBLISHER = b"original-publisher"
STABLE = b"restart/stable"
FINITE = b"restart/finite"
SHORT = b"restart/short"
LEGACY = b"restart/legacy"
CLEARED = b"restart/cleared"
PING = b"\xc0\0"
PONG = (0xd0, b"")


@contextmanager
def broker(program, mode, image):
    with subprocess.Popen([str(program.resolve()), mode, str(image)], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, text=True) as process:
        lines = queue.Queue()
        Thread(target=lambda: lines.put(process.stdout.readline()), daemon=True).start()
        try:
            host, port = lines.get(timeout=10).strip().rsplit(":", 1)
            assert host == "127.0.0.1"
            yield host, int(port)
        finally:
            if process.poll() is None:
                process.stdin.write("\n")
                process.stdin.flush()
            try:
                assert process.wait(timeout=10) == 0, "broker did not publish its retained checkpoint"
            except subprocess.TimeoutExpired:
                process.kill()
                raise


def publish(sock, name, payload, expiry=None):
    fields = b"" if expiry is None else b"\x02" + expiry.to_bytes(4, "big")
    sock.sendall(frame(0x33, topic(name) + b"\0\x05" + bytes((len(fields),)) + fields + payload))
    ack = receive(sock)
    assert ack == (0x40, b"\0\x05\x10"), ack  # Stored, with no matching live subscribers.


def subscribe(sock, name, identifier, options):
    packet_id = identifier.to_bytes(2, "big")
    sock.sendall(frame(0x82, packet_id + b"\0" + topic(name) + bytes((options,))))
    assert receive(sock) == (0x90, packet_id + b"\0" + bytes((options & 3,)))


def barrier(sock):
    sock.sendall(PING)
    assert receive(sock) == PONG, "unexpected retained delivery before PINGRESP"


def verify_restored(address):
    same, _ = connect(address, 5, PUBLISHER)
    with same:
        subscribe(same, STABLE, 1, 5)
        barrier(same)

    observer, _ = connect(address, 5, b"observer")
    with observer:
        subscribe(observer, STABLE, 1, 1)
        kind, body = receive(observer)
        assert kind == 0x33 and body.startswith(topic(STABLE)) and body.endswith(b"\0renewed\x00\xff")
        at = len(topic(STABLE))
        observer.sendall(frame(0x40, body[at:at + 2]))
        barrier(observer)

        subscribe(observer, FINITE, 2, 0)
        kind, body = receive(observer)
        prefix = topic(FINITE) + b"\x05\x02"
        assert kind == 0x31 and body.startswith(prefix) and body[len(prefix) + 4:] == b"expiring"
        expiry = int.from_bytes(body[len(prefix):len(prefix) + 4], "big")
        assert 0 < expiry < 120, expiry
        barrier(observer)

        subscribe(observer, LEGACY, 3, 0)
        assert receive(observer) == (0x31, topic(LEGACY) + b"\0mqtt-311")
        barrier(observer)
        for index, name in enumerate((CLEARED, SHORT), 4):
            subscribe(observer, name, index, 0)
            barrier(observer)
        return expiry


def rejected_image(program, path, data):
    path.write_bytes(data)
    failed = subprocess.run([str(program.resolve()), "restore", str(path)], input="\n",
                            capture_output=True, text=True, timeout=10)
    assert failed.returncode == 1 and not failed.stdout, (failed.returncode, failed.stdout, failed.stderr)
    assert path.read_bytes() == data, "failed restore must preserve the original checkpoint"


def verify(program, directory):
    image = directory / "retained.bin"
    with broker(program, "capture", image) as address:
        legacy, _ = connect(address, 4, b"legacy-publisher")
        with legacy:
            legacy.sendall(frame(0x31, topic(LEGACY) + b"mqtt-311") + PING)
            assert receive(legacy) == PONG
        source, _ = connect(address, 5, PUBLISHER)
        with source:
            publish(source, STABLE, b"old")
            publish(source, STABLE, b"renewed\x00\xff")
            publish(source, FINITE, b"expiring", 120)
            publish(source, CLEARED, b"gone")
            publish(source, CLEARED, b"")
            publish(source, SHORT, b"expires while down", 3)
    stored = image.read_bytes()
    assert stored[:8] == b"XMQRET01" and int.from_bytes(stored[16:20], "big") == 4
    time.sleep(3.1)
    with broker(program, "restore", image) as address:
        first_expiry = verify_restored(address)
    with broker(program, "restore", image) as address:
        assert verify_restored(address) <= first_expiry
    bad = directory / "invalid.bin"
    rejected_image(program, bad, stored[:-1])
    rejected_image(program, bad, stored[:-1] + bytes((stored[-1] ^ 1,)))
    future = stored[:8] + ((1 << 63) - 1).to_bytes(8, "big") + stored[16:-4]
    rejected_image(program, bad, future + zlib.crc32(future).to_bytes(4, "big"))
    return {"retained_restart": "pass", "source_versions": ["5.0", "3.1.1"],
            "finite_expiry_and_no_local": True, "corruption_and_clock_rejection": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="xmqtt-retained-") as directory:
        print(json.dumps(verify(args.program, Path(directory))))


if __name__ == "__main__":
    main()
