"""Verify aggregate QoS 2 delivery credit after losing a full TCP recipient."""
import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import socket
import struct
import subprocess
import time

from run_server_handshake import (connect, eof, finish_qos2, healthy_flow, offer_qos2, ping,
                                  publish_qos2, read, receive_qos2, running_server, small_frame, topic)


# Together these default quotas hold the broker's entire outgoing count budget.
SLOW_COUNT = 64
PER_RECIPIENT = 256
FULL = b"quota/held"
REFILL = b"quota/refill"
HEALTHY = b"healthy/live"


def subscribe(peer, name):
    peer.sendall(small_frame(0x82, b"\0\1\0" + topic(name) + b"\2"))
    assert read(peer, 6) == b"\x90\4\0\1\0\2", "expected a QoS 2 subscription"


def fill(source, name, peers):
    for number in range(1, PER_RECIPIENT + 1):
        publish_qos2(source, name, number, struct.pack("!H", number))
    held = []
    for peer in peers:
        identifier = receive_qos2(peer, name, b"\0\1")
        peer.sendall(small_frame(0x50, identifier))
        assert read(peer, 4) == small_frame(0x62, identifier)
        held.append(identifier)
    return held


def reset_and_recover(source, fast, slow):
    began = time.perf_counter()
    slow.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                    struct.pack("HH" if os.name == "nt" else "ii", 1, 0))
    slow.close()
    rejected = 0
    while time.perf_counter() - began < 5:
        payload = struct.pack("!I", rejected)
        if offer_qos2(source, HEALTHY, PER_RECIPIENT + 1, payload):
            finish_qos2(fast, receive_qos2(fast, HEALTHY, payload))
            ping((fast,))
            return {"quota_replies_after_reset": rejected,
                    "verified_recovery_us": (time.perf_counter() - began) * 1_000_000}
        rejected += 1
        time.sleep(0.001)
    raise TimeoutError("aggregate QoS 2 credit did not recover after TCP reset")


def reclaim_slot(endpoint, identity):
    deadline = time.monotonic() + 5
    while (remaining := deadline - time.monotonic()) > 0:
        peer, _ = connect(endpoint, 5, identity, 1, allow_refusal=True, timeout=remaining)
        if peer is not None:
            return peer
        time.sleep(min(0.001, remaining))
    raise TimeoutError("reset subscriber connection slot was not reclaimed")


def drive(program, clients, result):
    with running_server(program, "--compare", str(clients), "--workers", "2") as endpoint, ExitStack() as peers:
        def open_peer(identity, maximum=None):
            peer, _ = connect(endpoint, 5, identity, maximum)
            return peers.enter_context(peer)

        result["phase"] = "connect"
        source, fast = open_peer(b"quota-source"), open_peer(b"quota-fast")
        for peer in (source, fast):
            peer.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        identities = [f"quota-held-{index}".encode("ascii") for index in range(SLOW_COUNT)]
        slow = [open_peer(identity, 1) for identity in identities]
        idle = [open_peer(f"quota-idle-{index}".encode("ascii")) for index in range(clients - SLOW_COUNT - 2)]
        ping(idle)
        subscribe(fast, HEALTHY)
        for peer in slow:
            subscribe(peer, FULL)
        result["connected"] = clients
        result["baseline"] = healthy_flow(source, fast, HEALTHY)

        result["phase"] = "fill"
        held = fill(source, FULL, slow)
        result["initial_held_deliveries"] = len(held) * PER_RECIPIENT
        publish_qos2(source, HEALTHY, PER_RECIPIENT + 1, b"aggregate full", quota=True)
        ping((fast,))

        result["phase"] = "reset"
        result["initial_reset"] = reset_and_recover(source, fast, slow.pop())
        held.pop()
        replacement = peers.enter_context(reclaim_slot(endpoint, identities[-1]))
        subscribe(replacement, REFILL)
        result["phase"] = "refill"
        result["refilled_deliveries"] = PER_RECIPIENT * len(fill(source, REFILL, (replacement,)))
        publish_qos2(source, HEALTHY, PER_RECIPIENT + 1, b"aggregate full again", quota=True)
        ping((fast,))
        result["phase"] = "recover"
        result["replacement_reset"] = reset_and_recover(source, fast, replacement)
        result["recovered"] = healthy_flow(source, fast, HEALTHY)

        result["phase"] = "drain surviving sessions"
        for peer, identifier in zip(slow, held, strict=True):
            peer.sendall(small_frame(0x70, identifier))
            for number in range(2, PER_RECIPIENT + 1):
                finish_qos2(peer, receive_qos2(peer, FULL, struct.pack("!H", number)))
            ping((peer,))
        result["surviving_deliveries_drained"] = len(slow) * PER_RECIPIENT
        result["phase"] = "retire"
        ping(idle)
        for peer in (source, fast, *slow, *idle):
            peer.sendall(b"\xe0\0")
            eof(peer)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("broker", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--clients", type=int, default=SLOW_COUNT + 2)
    args = parser.parse_args()
    if not SLOW_COUNT + 2 <= args.clients <= 10000:
        parser.error("choose 66..10000 connections")
    broker = args.broker.resolve(strict=True)
    inputs = (broker, Path(__file__), Path(__file__).with_name("run_server_handshake.py"))
    result = {"platform": platform.platform(), "clients": args.clients, "workers": 2,
              "inputs": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs},
              "complete": False, "phase": "startup"}
    with args.report.open("x", encoding="utf-8") as output:
        try:
            drive(broker, args.clients, result)
        except (AssertionError, OSError, queue.Empty, subprocess.SubprocessError) as error:
            result["error"] = f"{type(error).__name__}: {error}"
            print(f"[fail] {result['phase']}: {result['error']}", flush=True)
        else:
            result.update(complete=True, phase="complete")
            print(f"[pass] aggregate QoS 2 credit recovered at {args.clients} connections", flush=True)
        finally:
            json.dump(result, output, indent=2, allow_nan=False)
            output.write("\n")
    return 0 if result["complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
