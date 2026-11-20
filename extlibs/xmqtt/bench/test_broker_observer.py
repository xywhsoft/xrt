"""Verify the remote benchmark rejects invalid reliable acknowledgement and delivery traces."""
import argparse
from contextlib import ExitStack
from pathlib import Path
import select
import socket
import subprocess
import time

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
        p = subprocess.Popen([binary, "--remote", str(port), "1", "3", "100", "0", "exact", str(qos), "1"],
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
                    elif name == "negative-pubrec":
                        assert tag == bytes([0x34])
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
                    if name == "negative-pubrec":
                        assert "publisher acknowledgement type=5 reason=0x97" in err, err
                    print(name, "rejected", err.strip())
        finally:
            if p.poll() is None:
                p.kill()
                p.wait(timeout=5)


def shared_case(binary, duplicate):
    with ExitStack() as stack:
        listener = stack.enter_context(socket.socket())
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(8)
        port = listener.getsockname()[1]
        rate = "100" if duplicate else "1"
        warmup = 2 if duplicate else 4
        process = subprocess.Popen([binary, "--remote", str(port), "4", "4", rate, str(warmup), "shared", "2", "1"],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            peers = []
            for member in range(5):
                peer, _ = listener.accept()
                peers.append(stack.enter_context(peer))
                peer.settimeout(6)
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
            print("arbitrary shared member selection and idle reader verified")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("observer", type=Path)
    args = parser.parse_args()
    binary = str(args.observer.resolve(strict=True))
    for name, qos in (("early-pubcomp", 2), ("negative-pubrec", 2), ("wrong-pubcomp", 2),
                      ("duplicate-delivery", 1)):
        case(binary, name, qos)
    shared_case(binary, False)
    shared_case(binary, True)


if __name__ == "__main__":
    main()
