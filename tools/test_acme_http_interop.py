"""Exercise ACME HTTP/TLS, UDP queries and resource lifecycle boundaries."""

from __future__ import annotations

import argparse
import os
import socket
import struct
import subprocess
import tempfile
import threading
from pathlib import Path

from http_tls_test_fixture import (
    RESPONSE, run_command, certificates, case, upload_fault_case,
    handshake_fault_case, tcp_eof_case, invalid_url_case,
)
from test_acme_provider_wire import build_client as build_wire_client, run_cases as run_wire_cases


ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / "extlibs/xacme/tests/acme/test_http_interop_client.c"
OWNERSHIP_CLIENT = ROOT / "extlibs/xacme/tests/acme/test_lifecycle_interop_client.c"
SINGLE_LIFECYCLE_CLIENT = ROOT / "extlibs/xacme/tests/acme/test_provider_lifecycle_single_client.c"
PROVIDER_BODY_CLIENT = ROOT / "extlibs/xacme/tests/acme/test_provider_body_client.c"
COPIED_HEADERS = (b'Location: /issued\r\nReplay-Nonce: nonce\r\nRetry-After: 7\r\n'
                  b'Link: </issuer>; rel="up"\r\nContent-Type: application/json\r\n')


def build_client(root: Path, compiler: str, sanitize: bool, *, ownership: bool = False,
                 coverage: bool = False, provider_bodies: bool = False,
                 lifecycle_single: bool = False) -> Path:
    if sanitize and os.name == "nt":
        raise ValueError("--sanitize is supported only on POSIX hosts")
    if sanitize and coverage:
        raise ValueError("coverage and sanitizer builds must run separately")
    if sum((ownership, provider_bodies, lifecycle_single)) > 1:
        raise ValueError("select one standalone probe per binary")
    binary = root / (("acme_provider_lifecycle_single_client" if lifecycle_single else
                     "acme_provider_body_client" if provider_bodies else
                     "acme_ownership_client" if ownership else "acme_http_client") +
                     (".exe" if os.name == "nt" else ""))
    flags = ["-fno-omit-frame-pointer", "-fsanitize=address,undefined"] if sanitize else []
    profile_flags = ["--coverage", "-fprofile-update=atomic"] if coverage else []
    libs = ["-lws2_32", "-lbcrypt", "-ladvapi32", "-liphlpapi"] if os.name == "nt" else ["-pthread", "-lm"]
    run_command(compiler, "-std=c11", "-D_GNU_SOURCE", "-O0" if coverage else "-O2", "-Wall", "-Wextra",
                "-Werror", *profile_flags, *flags, "-I", str(ROOT / "single"), "-I", str(ROOT / "include"),
                "-I", str(ROOT / "extlibs/xacme/include"),
                str(SINGLE_LIFECYCLE_CLIENT if lifecycle_single else
                    PROVIDER_BODY_CLIENT if provider_bodies else OWNERSHIP_CLIENT if ownership else CLIENT),
                "-o", str(binary), *libs, *flags)
    return binary


def request_capacity_case(client: Path, ca: Path) -> None:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(10)
    port = listener.getsockname()[1]
    errors = []
    requests = []

    def serve():
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(10)
                data = bytearray()
                while b"\r\n\r\n" not in data and len(data) < 16384:
                    piece = peer.recv(4096)
                    if not piece:
                        raise AssertionError("request closed before its headers")
                    data.extend(piece)
                head, body = bytes(data).split(b"\r\n\r\n", 1)
                while len(body) < 7:
                    piece = peer.recv(7 - len(body))
                    if not piece:
                        raise AssertionError("request closed before its body")
                    body += piece
                requests.append((head.split(b"\r\n"), body))
                peer.sendall(RESPONSE)
        except Exception as exc:
            errors.append(exc)

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    try:
        completed = subprocess.run([str(client), f"http://127.0.0.1:{port}/token", str(ca), "request-max"],
                                   capture_output=True, text=True, timeout=25)
    finally:
        worker.join(timeout=12)
        listener.close()
    if completed.returncode != 0:
        raise AssertionError(f"request limit client failed: {completed.stderr}")
    if worker.is_alive() or errors or len(requests) != 1:
        raise AssertionError(f"request limit server failed: {errors}")
    lines, body = requests[0]
    if lines[0] != b"POST /token HTTP/1.1" or len(lines) != 101 or body != b"request":
        raise AssertionError("request field count or body changed")
    for i in range(94):
        if f"X-Extra-{i}: v".encode("ascii") not in lines:
            raise AssertionError(f"request silently dropped extra field {i}")
    if lines.count(b"Content-Length: 7") != 1 or lines.count(b"Content-Type: text/plain") != 1:
        raise AssertionError("request framing fields changed")
    print("  100 request fields including 94 extras: passed")


def lifecycle_case(client: Path, ca: Path) -> None:
    """A real listener detects any request mistakenly allowed after retirement."""
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(8)
    listener.settimeout(0.1)
    stop = threading.Event()
    connections = []
    errors = []

    def serve():
        while not stop.is_set():
            try:
                peer, _ = listener.accept()
            except socket.timeout:
                continue
            except OSError as exc:
                if not stop.is_set():
                    errors.append(exc)
                return
            connections.append(True)
            with peer:
                peer.settimeout(2)
                try:
                    peer.recv(16384)
                    peer.sendall(RESPONSE)
                except (ConnectionError, TimeoutError):
                    pass

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    failures = []
    try:
        for mode in ("after-cleanup", "lifecycle"):
            completed = subprocess.run([str(client),
                                       f"http://127.0.0.1:{listener.getsockname()[1]}/token",
                                       str(ca), mode], capture_output=True, text=True, timeout=40)
            print(completed.stdout, end="", flush=True)
            if completed.returncode != 0:
                failures.append(f"{mode}: {completed.stderr}")
    finally:
        stop.set()
        worker.join(timeout=3)
        listener.close()
    if worker.is_alive() or errors or connections or failures:
        raise AssertionError(f"requests after cleanup connected {len(connections)} times; "
                             f"server={errors}; " + "; ".join(failures))
    print("  all requests after cleanup rejected before connecting: passed")


def dns_lifecycle_case(client: Path, ca: Path) -> None:
    listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listener.bind(("127.0.0.1", 0))
    listener.settimeout(0.1)
    stop = threading.Event()
    errors = []
    seen = set()

    def serve():
        while not stop.is_set():
            try:
                packet, peer = listener.recvfrom(2048)
            except socket.timeout:
                continue
            try:
                offset, labels = 12, []
                while packet[offset]:
                    length = packet[offset]
                    labels.append(packet[offset + 1:offset + 1 + length].decode("ascii"))
                    offset += length + 1
                name = ".".join(labels)
                question = packet[12:offset + 5]
                if packet[2:12] != b"\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00" or question[-4:] != b"\x00\x10\x00\x01":
                    raise AssertionError("DNS query type or count changed")
                seen.add(name)
                if name == "timeout.test":
                    continue
                if name == "bad.test":
                    response = packet[:2] + b"\x01\x00" + packet[4:12] + question
                elif name == "nx.test":
                    response = packet[:2] + b"\x81\x83\x00\x01\x00\x00\x00\x00\x00\x00" + question
                else:
                    response = packet[:2] + b"\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00" + question + \
                        b"\xc0\x0c" + struct.pack("!HHIH", 16, 1, 5, 6) + b"\x05probe"
                if name == "wrong-id.test":
                    bad_id = (int.from_bytes(packet[:2], "big") ^ 1).to_bytes(2, "big")
                    listener.sendto(bad_id + response[2:], peer)
                listener.sendto(response, peer)
            except Exception as exc:
                errors.append(exc)
                break

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    try:
        completed = subprocess.run([str(client), str(ca), "dns", str(listener.getsockname()[1])],
                                   capture_output=True, text=True, timeout=45)
        print(completed.stdout, end="", flush=True)
    finally:
        stop.set()
        worker.join(timeout=3)
        listener.close()
    if completed.returncode != 0 or worker.is_alive() or errors:
        raise AssertionError(f"DNS UDP lifecycle failed: {completed.stderr}; server={errors}")
    if not {"success.test", "nx.test", "bad.test", "timeout.test", "wrong-id.test"} <= seen:
        raise AssertionError(f"DNS UDP cases did not all reach the server: {seen}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--coverage-dir", type=Path,
                        help="retain GCC -O0 coverage binaries and counters in this directory")
    choices = ("identity", "handshake", "headers", "link-fields", "trailers", "chunked-memory",
               "body-limits", "nul-body", "header-oom", "eof", "upload", "request-max", "url-invalid", "url-vectors", "lifecycle", "ownership", "dns-lifecycle", "provider-bodies", "provider-wire")
    parser.add_argument("--case", action="append", choices=choices)
    args = parser.parse_args()
    if args.sanitize and args.coverage_dir is not None:
        parser.error("--coverage-dir and --sanitize must run separately")
    selected = set(args.case or choices)
    failures = []
    with tempfile.TemporaryDirectory(prefix="acme-http-interop-") as temp:
        root = Path(temp)
        ca, other_ca, leaf, wrong_leaf, key = certificates(root)
        build_root = args.coverage_dir.resolve() if args.coverage_dir is not None else root
        build_root.mkdir(parents=True, exist_ok=True)
        coverage = args.coverage_dir is not None
        client = build_client(build_root, args.compiler, args.sanitize, coverage=coverage)
        if "link-fields" in selected:
            completed = subprocess.run([str(client), "unused", str(ca), "link-bounds"],
                                       capture_output=True, text=True, timeout=10)
            if completed.returncode != 0:
                raise AssertionError(completed.stderr)
            print(completed.stdout, end="")
        if "url-vectors" in selected:
            completed = subprocess.run([str(client), "unused", str(ca), "url-vectors"],
                                       capture_output=True, text=True, timeout=10)
            if completed.returncode != 0:
                raise AssertionError(completed.stderr)
            print(completed.stdout, end="")

        def check(label, operation):
            try:
                operation()
            except AssertionError as exc:
                failures.append(f"{label}: {exc}")
                print(f"  FAIL {label}: {exc}", flush=True)

        def response_case(label, response, mode="headers", graceful=False):
            check(label, lambda: case(
                client, ca, leaf, key, label=label, hostname="localhost",
                bind="127.0.0.1", target="/token", expected_sni="localhost",
                success=mode in ("success", "headers", "link-fields", "chunked", "large-body"),
                mode=mode, response=response,
                fragment_size=16381 if len(response) > 8192 else 113,
                graceful_close=graceful))

        payload = b'{"ok":true}'
        fixed = b"HTTP/1.1 200 OK\r\nContent-Length: 11\r\nConnection: close\r\n" + COPIED_HEADERS
        chunk_head = b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n" + COPIED_HEADERS + b"\r\n"
        if "identity" in selected:
            response_case("DNS TLS and copied response fields", fixed + b"\r\n" + payload)
            check("IPv4 identity", lambda: case(
                client, ca, leaf, key, label="IPv4 TLS without SNI", hostname="127.0.0.1",
                bind="127.0.0.1", target="/token?probe=ip4", expected_sni=None, success=True))
            if socket.has_ipv6:
                try:
                    case(client, ca, leaf, key, label="IPv6 TLS without SNI", hostname="::1",
                         bind="::1", target="/token?probe=ip6", expected_sni=None, success=True)
                except OSError as exc:
                    print(f"  IPv6 loopback unavailable: {exc}")
                except AssertionError as exc:
                    failures.append(f"IPv6 identity: {exc}")
                    print(f"  FAIL IPv6 identity: {exc}", flush=True)
            check("missing IP identity", lambda: case(
                client, ca, wrong_leaf, key, label="missing IP identity rejected",
                hostname="127.0.0.1", bind="127.0.0.1", target="/token",
                expected_sni=None, success=False))
            for label, host, scheme, tail, target in (
                ("query without path", "localhost", "https", "?probe=query", "/?probe=query"),
                ("fragment excluded", "localhost", "https", "/token?probe=%23#client-only", "/token?probe=%23"),
                ("root fragment excluded", "localhost", "https", "#client-only", "/"),
                ("empty fragment excluded", "localhost", "https", "/token#", "/token"),
                ("fragment grammar", "localhost", "https", "/token?q=%23#AZaz09-._~!$&'()*+,;=:@/?%00%23%FF", "/token?q=%23"),
                ("DNS root dot", "localhost.", "https", "/token", "/token"),
                ("mixed-case scheme", "localhost", "hTtPs", "/token", "/token"),
            ):
                check(label, lambda label=label, host=host, scheme=scheme, tail=tail, target=target: case(
                    client, ca, leaf, key, label=label, hostname=host, scheme=scheme,
                    bind="127.0.0.1", target=target, url_tail=tail,
                    expected_sni="localhost", success=True))
            for label, trust, cert in (("untrusted CA rejected", other_ca, leaf),
                                       ("wrong DNS identity rejected", ca, wrong_leaf)):
                check(label, lambda label=label, trust=trust, cert=cert: case(
                    client, trust, cert, key, label=label, hostname="localhost",
                    bind="127.0.0.1", target="/token", expected_sni="localhost",
                    success=False))
        if "handshake" in selected:
            for stall in (False, True):
                check(f"handshake stall={stall}", lambda stall=stall: handshake_fault_case(
                    client, ca, label=f"ACME handshake stall={stall}", stall=stall))
        if "headers" in selected:
            for total in (100, 101):
                extra = b"".join(f"X-Probe-{i}: x\r\n".encode("ascii") for i in range(total - 7))
                response_case(f"{total} response fields", fixed + extra + b"\r\n" + payload,
                              "headers" if total == 100 else "head-failure")
        if "link-fields" in selected:
            first = b'Link: </issuer>; rel="up"\r\n'
            second = b'liNK: </alternate>; rel="alternate"\r\n'
            response_case("repeated Link fields preserve order", fixed + second + b"\r\n" + payload, "link-fields")
            response_case("combined Link field", fixed.replace(first, first[:-2] + b', </alternate>; rel="alternate"\r\n') + b"\r\n" + payload, "link-fields")
            response_case("Link fields around unrelated headers", fixed.replace(first, b"") + first + second + b"\r\n" + payload, "link-fields")
            response_case("combined Link allocation failure", fixed + second + b"\r\n" + payload, "header-oom-link-fields")
        if "trailers" in selected:
            for total in (100, 101):
                trailers = b"".join(f"X-Trailer-{i}: y\r\n".encode("ascii") for i in range(total))
                response_case(f"{total} chunked trailers", chunk_head + b"b\r\n" + payload +
                              b"\r\n0\r\n" + trailers + b"\r\n",
                              "headers" if total == 100 else "body-failure")
        if "chunked-memory" in selected:
            response_case("bounded chunk extension memory", chunk_head +
                          (b"1;p=" + b"v" * 4090 + b"\r\nx\r\n") * 2048 + b"0\r\n\r\n", "chunked")
        if "body-limits" in selected:
            response_case("max 4 MiB fixed body", b"HTTP/1.1 200 OK\r\nContent-Length: 4194304\r\nConnection: close\r\n\r\n" + b"x" * 4194304, "large-body")
            response_case("oversized fixed declaration", b"HTTP/1.1 200 OK\r\nContent-Length: 4194305\r\nConnection: close\r\n\r\nx", "body-failure")
            response_case("oversized cumulative chunked body", chunk_head + b"400000\r\n" + b"x" * 4194304 + b"\r\n1\r\nx\r\n0\r\n\r\n", "body-failure")
        if "nul-body" in selected:
            response_case("raw NUL body rejected", b"HTTP/1.1 200 OK\r\nContent-Length: 17\r\nConnection: close\r\n\r\n" + payload + b"\x00extra", "body-failure")
        if "header-oom" in selected:
            for mode in ("header-oom", "header-oom-nonce", "header-oom-retry", "header-oom-link", "header-oom-type"):
                response_case(mode, fixed + b"\r\n" + payload, mode)
        if "eof" in selected:
            response_case("authenticated close-delimited body", b"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n" + payload, "success", True)
            response_case("ragged close-delimited body", b"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n" + payload, "failure")
            response_case("authenticated short fixed body", b"HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\n" + payload, "body-failure", True)
            response_case("authenticated unfinished chunked body", chunk_head + b"b\r\n" + payload + b"\r\n", "body-failure", True)
            for truncated in (False, True):
                check(f"TCP EOF truncated={truncated}", lambda truncated=truncated:
                      tcp_eof_case(client, ca, truncated=truncated))
        if "upload" in selected:
            for disconnect in (False, True):
                check(f"upload disconnect={disconnect}", lambda disconnect=disconnect:
                      upload_fault_case(client, ca, leaf, key, disconnect=disconnect))
        if "request-max" in selected:
            check("request capacity", lambda: request_capacity_case(client, ca))
        if "url-invalid" in selected:
            check("invalid URL authority/target", lambda: invalid_url_case(client, ca))
        if "lifecycle" in selected:
            check("HTTP lifecycle", lambda: lifecycle_case(client, ca))
        if "ownership" in selected or "dns-lifecycle" in selected:
            ownership_client = build_client(build_root, args.compiler, args.sanitize,
                                            ownership=True, coverage=coverage)
        if "ownership" in selected:
            completed = subprocess.run([str(ownership_client), str(ca)], capture_output=True,
                                       text=True, timeout=90)
            print(completed.stdout, end="", flush=True)
            if completed.returncode != 0:
                failures.append(f"client/provider ownership: {completed.stderr}")
            single_client = build_client(build_root, args.compiler, args.sanitize,
                                         lifecycle_single=True, coverage=coverage)
            completed = subprocess.run([str(single_client), str(ca)], capture_output=True,
                                       text=True, timeout=90)
            print(completed.stdout, end="", flush=True)
            if completed.returncode != 0:
                failures.append(f"public aggregate provider guards: {completed.stderr}")
        if "dns-lifecycle" in selected:
            check("DNS UDP lifecycle", lambda: dns_lifecycle_case(ownership_client, ca))
        if "provider-bodies" in selected:
            body_client = build_client(build_root, args.compiler, args.sanitize,
                                       provider_bodies=True, coverage=coverage)
            completed = subprocess.run([str(body_client)], capture_output=True, text=True, timeout=60)
            print(completed.stdout, end="", flush=True)
            if completed.returncode != 0:
                failures.append(f"provider request bodies: {completed.stderr}")
        if "provider-wire" in selected:
            wire_client = build_wire_client(build_root, args.compiler, args.sanitize, coverage)
            check("provider wire contracts", lambda: run_wire_cases(wire_client, ca, leaf, key))
    if failures:
        raise AssertionError("ACME HTTP interop failures:\n" + "\n".join(failures))
    print("ACME independent HTTP/TLS interop passed")


if __name__ == "__main__":
    main()
