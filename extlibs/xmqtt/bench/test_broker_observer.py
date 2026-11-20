"""Verify the remote benchmark rejects invalid reliable acknowledgement and delivery traces."""
import argparse
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
                    print(name, "rejected", err.strip())
        finally:
            if p.poll() is None:
                p.kill()
                p.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("observer", type=Path)
    args = parser.parse_args()
    binary = str(args.observer.resolve(strict=True))
    for name, qos in (("early-pubcomp", 2), ("wrong-pubcomp", 2), ("duplicate-delivery", 1)):
        case(binary, name, qos)


if __name__ == "__main__":
    main()
