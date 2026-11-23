"""Verify the remote benchmark rejects invalid reliable acknowledgement and delivery traces."""
import argparse
import math
from contextlib import ExitStack
from pathlib import Path
import select
import socket
import ssl
import subprocess
import time

from compare_broker import OBSERVER_CPU_METRICS, ObserverFailure

def read(sock, size):
    result = b""
    while len(result) < size:
        chunk = sock.recv(size - len(result))
        if not chunk:
            raise EOFError("truncated wire")
        result += chunk
    return result

def frame(sock):
    tag = read(sock, 1)
    length, shift = 0, 0
    while True:
        b = read(sock, 1)[0]
        length += (b & 127) << shift
        if b < 128:
            break
        shift += 7
        assert shift <= 21
    return tag, read(sock, length)

def check_no_more_publications(publisher):
    try:
        assert not publisher.recv(1), "observer offered another publication after an invalid ACK"
    except ConnectionResetError:
        pass


def drain(process, sockets):
    # Answer the final PINGs if an observer incorrectly accepts the duplicate.
    deadline = time.monotonic() + 8
    while sockets and process.poll() is None:
        assert time.monotonic() < deadline, "observer did not retire"
        ready, _, _ = select.select(sockets, [], [], 0.1)
        for sock in ready:
            try:
                tag, body = frame(sock)
                if tag == b"\xc0" and not body:
                    sock.sendall(b"\xd0\x00")
                else:
                    assert tag == b"\x40" and len(body) == 2
            except (EOFError, ConnectionError):
                sockets.remove(sock)


def case(binary, name, qos):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(8)
        port = listener.getsockname()[1]
        warmup = int(name == "negative-after-progress")
        p = subprocess.Popen([binary, "--remote", str(port), "1", "3", "100", str(warmup), "exact", str(qos), "1"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            subscriber, _ = listener.accept()
            with subscriber:
                subscriber.settimeout(5)
                assert frame(subscriber)[0] == b"\x10"
                subscriber.sendall(b"\x20\x03\x00\x00\x00")
                assert frame(subscriber)[0] == b"\x82"
                subscriber.sendall(b"\x90\x04\x00\x01\x00" + bytes([qos]))
                publisher, _ = listener.accept()
                with publisher:
                    publisher.settimeout(8)
                    assert frame(publisher)[0] == b"\x10"
                    publisher.sendall(b"\x20\x03\x00\x00\x00")
                    tag, body = frame(publisher)
                    assert body[6:8] == b"\x00\x01"
                    if name == "early-pubcomp":
                        assert tag == b"\x34"
                        publisher.sendall(b"\x70\x02\x00\x01")
                        check_no_more_publications(publisher)
                    elif name in ("negative-pubrec", "negative-after-progress"):
                        for sequence in range(2 if warmup else 0):
                            assert tag == b"\x34" and body[-4:] == sequence.to_bytes(4, "big")
                            subscriber.sendall(b"\x34\x89\x01" + body)
                            assert frame(subscriber) == (b"\x50", b"\x00\x01")
                            subscriber.sendall(b"\x62\x02\x00\x01")
                            assert frame(subscriber) == (b"\x70", b"\x00\x01")
                            publisher.sendall(b"\x50\x02\x00\x01")
                            assert frame(publisher) == (b"\x62", b"\x00\x01")
                            publisher.sendall(b"\x70\x02\x00\x01")
                            tag, body = frame(publisher)
                        assert tag == b"\x34"
                        publisher.sendall(bytes.fromhex("50 03 00 01 97"))
                        check_no_more_publications(publisher)
                    elif name == "wrong-pubcomp":
                        assert tag == b"\x34"
                        publisher.sendall(b"\x50\x02\x00\x01")
                        assert frame(publisher) == (b"\x62", b"\x00\x01")
                        publisher.sendall(b"\x70\x02\x00\x02")
                        check_no_more_publications(publisher)
                    else:
                        assert tag == b"\x32" and len(body) == 137
                        for sequence in range(3):
                            if sequence:
                                assert frame(publisher) == (tag, body[:-1] + bytes([sequence]))
                            delivery = b"\x32\x89\x01" + body[:6] + bytes((0, 50 + sequence)) + body[8:-1] + bytes([sequence])
                            try:
                                subscriber.sendall(delivery if sequence else delivery + delivery)
                            except ConnectionError:
                                if not sequence:
                                    raise
                            publisher.sendall(b"\x40\x02\x00\x01")
                        drain(p, [subscriber, publisher])
                    out, err = p.communicate(timeout=8)
                    assert p.returncode == 1 and not out and "Remote benchmark failed" in err, (p.returncode, out, err)
                    if name in ("negative-pubrec", "negative-after-progress"):
                        assert "publisher acknowledgement type=5 reason=0x97" in err, err
                        progress = ObserverFailure(p.returncode, err).progress
                        assert progress == {"sent": 1 + warmup, "confirmed": warmup, "deliveries": warmup}, err
                    print(name, "rejected", err.strip())
        finally:
            if p.poll() is None:
                p.kill()
                p.wait(timeout=5)


def multi_publisher_case(binary, reorder, tls=None, ca=None):
    with ExitStack() as stack:
        listener = stack.enter_context(socket.socket())
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(8)
        port = listener.getsockname()[1]
        command = [binary, "--remote", str(port), "1", "5", "100", "1", "exact", "2", "1", "2",
                   "--publishers", "2"]
        if tls:
            command.extend(("--tls", str(ca), "localhost"))
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            def accept(index):
                peer = stack.enter_context(listener.accept()[0])
                peer.settimeout(6)
                if tls:
                    peer = stack.enter_context(tls.wrap_socket(peer, server_side=True))
                tag, body = frame(peer)
                assert tag == b"\x10" and body[-8:] == f"{index:08d}".encode(), body
                peer.sendall(b"\x20\x03\x00\x00\x00")
                return peer

            subscriber = accept(0)
            assert frame(subscriber) == (b"\x82", b"\x00\x01\x00\x00\x04load\x02")
            subscriber.sendall(b"\x90\x04\x00\x01\x00\x02")
            first, second = accept(1), accept(2)

            def receive_publication(publisher, sequence):
                tag, body = frame(publisher)
                assert tag == b"\x34" and body[:9] == b"\x00\x04load\x00\x01\x00"
                assert body[9:] == b"A" * 124 + sequence.to_bytes(4, "big")
                return body

            def confirm(publisher, body, sequence):
                identifier = body[6:8]
                publisher.sendall(b"\x50\x02" + identifier)
                assert frame(publisher) == (b"\x62", identifier)
                publisher.sendall(b"\x70\x02" + identifier)
                if not reorder:
                    delivery_id = (20 + sequence).to_bytes(2, "big")
                    subscriber.sendall(b"\x34\x89\x01" + body[:6] + delivery_id + body[8:])
                    assert frame(subscriber) == (b"\x50", delivery_id)
                    subscriber.sendall(b"\x62\x02" + delivery_id)
                    assert frame(subscriber) == (b"\x70", delivery_id)

            held = receive_publication(first, 0)
            # Complete the other publisher before returning any credit to the first.
            for sequence in (1, 3, 5):
                confirm(second, receive_publication(second, sequence), sequence)
            confirm(first, held, 0)
            confirm(first, receive_publication(first, 2), 2)
            last = receive_publication(first, 4)
            confirm(first, last, 4)
            if reorder:
                # Inject a later same-publisher frame after all offers have reached the peer.
                subscriber.sendall(b"\x34\x89\x01" + last[:6] + b"\x00\x18" + last[8:])
                try:
                    assert not subscriber.recv(1), "observer acknowledged out-of-order delivery"
                except ConnectionResetError:
                    pass
                out, err = process.communicate(timeout=8)
                assert process.returncode == 1 and not out and "check subscriber delivery counts" in err, err
                print("same-publisher reorder rejected")
            else:
                for peer in (first, second, subscriber):
                    assert frame(peer) == (b"\xc0", b"")
                    peer.sendall(b"\xd0\x00")
                out, err = process.communicate(timeout=8)
                assert process.returncode == 0 and not err, (out, err)
                metrics = dict(line.split(": ") for line in out.splitlines())
                assert metrics["offered"] == metrics["deliveries"] == metrics["publish_confirmed"] == "5"
                assert int(metrics["publisher_credit_stalls"]) > 0
                for name in OBSERVER_CPU_METRICS:
                    cpu = float(metrics[name])
                    assert math.isfinite(cpu) and cpu >= 0
                if tls:
                    assert metrics["tls_connections"] == "3"
                print("unblocked publisher advanced and cross-publisher order was accepted")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


def shared_case(binary, duplicate, tls=None, ca=None):
    with ExitStack() as stack:
        listener = stack.enter_context(socket.socket())
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(8)
        port = listener.getsockname()[1]
        rate = "100" if duplicate else "1"
        warmup = 2 if duplicate else 4
        command = [binary, "--remote", str(port), "4", "4", rate, str(warmup), "shared", "2", "1"]
        if tls:
            command.extend(("--tls", str(ca), "localhost"))
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            peers = []
            for member in range(5):
                peer, _ = listener.accept()
                stack.enter_context(peer)
                peer.settimeout(6)
                if tls:
                    peer = stack.enter_context(tls.wrap_socket(peer, server_side=True))
                peers.append(peer)
                assert frame(peer)[0] == b"\x10"
                peer.sendall(b"\x20\x03\x00\x00\x00")
                if member < 4:
                    group = b"a" if member % 2 == 0 else b"b"
                    assert frame(peer) == (b"\x82", b"\x00\x01\x00\x00\x0a$share/" + group + b"/+\x02")
                    peer.sendall(b"\x90\x04\x00\x01\x00\x02")
            publisher = peers.pop()
            total = warmup + 4
            for sequence in range(total):
                tag, body = frame(publisher)
                assert tag == b"\x34" and len(body) == 137 and body[8:9] == b"\x00"
                assert body[:6] == b"\x00\x04load" and body[9:] == b"A" * 124 + sequence.to_bytes(4, "big")
                identifier = body[6:8]
                publisher.sendall(b"\x50\x02" + identifier)
                assert frame(publisher) == (b"\x62", identifier)
                publisher.sendall(b"\x70\x02" + identifier)

                first = 2 if sequence in (1, 4) else 0
                message_id = (20 + sequence).to_bytes(2, "big")
                wire = b"\x34\x89\x01" + body[:6] + message_id + body[8:]
                for member in (first, 3):
                    peer = peers[member]
                    peer.sendall(wire)
                    assert frame(peer) == (b"\x50", message_id)
                    if duplicate and sequence == total - 1:
                        # The first member has not received PUBREL. A second
                        # member in the same group must still reject this copy.
                        other = peers[2]
                        other.sendall(wire)
                        try:
                            assert not other.recv(1), "duplicate received an acknowledgement"
                        except ConnectionResetError:
                            pass
                        out, err = process.communicate(timeout=8)
                        assert process.returncode == 1 and not out and "check subscriber delivery counts" in err
                        print("cross-member pending QoS 2 duplicate rejected")
                        return
                    peer.sendall(b"\x62\x02" + message_id)
                    assert frame(peer) == (b"\x70", message_id)

            for peer in [*peers, publisher]:
                assert frame(peer) == (b"\xc0", b"")
                peer.sendall(b"\xd0\x00")
            out, err = process.communicate(timeout=8)
            assert process.returncode == 0, err
            result = dict(line.split(": ", 1) for line in out.splitlines())
            assert result["offered"] == "4" and result["deliveries"] == "8" and result["publish_confirmed"] == "4"
            assert result["share_min_deliveries"] == "0" and result["share_max_deliveries"] == "4"
            if tls:
                assert result["tls_connections"] == "5"
            print("arbitrary shared member selection and idle reader verified")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


def reject_tls_identity(binary, tls, ca, name):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(8)
        port = listener.getsockname()[1]
        command = [binary, "--remote", str(port), "1", "1", "100", "0", "exact", "--tls", str(ca), name]
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            peer, _ = listener.accept()
            with peer:
                peer.settimeout(6)
                try:
                    with tls.wrap_socket(peer, server_side=True) as secure:
                        assert not secure.recv(1), "observer sent MQTT data before verifying TLS identity"
                except (ssl.SSLError, ConnectionError):
                    pass
            out, err = process.communicate(timeout=8)
            assert process.returncode == 1 and not out and "TLS connection 0:" in err, (process.returncode, out, err)
            print(f"TLS identity rejected ({ca.name}, {name}) before MQTT")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("observer", type=Path)
    parser.add_argument("--tls-credentials", type=Path, help="localhost TLS fixture with an additional unrelated wrong.pem CA")
    args = parser.parse_args()
    binary = str(args.observer.resolve(strict=True))
    for name, qos in (("early-pubcomp", 2), ("negative-pubrec", 2), ("negative-after-progress", 2),
                      ("wrong-pubcomp", 2), ("duplicate-delivery", 1)):
        case(binary, name, qos)
    shared_case(binary, False)
    shared_case(binary, True)
    multi_publisher_case(binary, False)
    multi_publisher_case(binary, True)
    if args.tls_credentials:
        root = args.tls_credentials.resolve(strict=True)
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.minimum_version = ssl.TLSVersion.TLSv1_3
        tls.load_cert_chain(root / "certificate.pem", root / "key.pem")
        shared_case(binary, False, tls, root / "ca.pem")
        shared_case(binary, True, tls, root / "ca.pem")
        multi_publisher_case(binary, False, tls, root / "ca.pem")
        reject_tls_identity(binary, tls, root / "wrong.pem", "localhost")
        reject_tls_identity(binary, tls, root / "ca.pem", "wrong.invalid")


if __name__ == "__main__":
    main()
