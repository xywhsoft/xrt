"""Exercise xoauth2 HTTP/TLS against independent Python loopback servers."""

from __future__ import annotations

import argparse
import os
import socket
import subprocess
import tempfile
from pathlib import Path

from http_tls_test_fixture import (
    RESPONSE, run_command, certificates, case, upload_fault_case,
    handshake_fault_case, tcp_eof_case, invalid_url_case,
)


ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / "extlibs/xoauth2/tests/test_tls_interop_client.c"
OWNERSHIP_CLIENT = ROOT / "extlibs/xoauth2/tests/test_lifecycle_interop_client.c"


def build_client(root: Path, compiler: str, sanitize: bool, *, ownership: bool = False,
                 coverage: bool = False) -> Path:
    binary = root / (("xoauth2_ownership_client" if ownership else "xoauth2_tls_client") +
                     (".exe" if os.name == "nt" else ""))
    if sanitize and os.name == "nt":
        raise ValueError("--sanitize is supported only on POSIX hosts")
    if sanitize and coverage:
        raise ValueError("coverage and sanitizer builds must run separately")
    sanitizer_flags = (
        ["-fno-omit-frame-pointer", "-fsanitize=address,undefined"]
        if sanitize else []
    )
    platform_libs = (["-lws2_32", "-lbcrypt", "-ladvapi32", "-liphlpapi"]
                     if os.name == "nt" else ["-pthread", "-lm"])
    profile_flags = ["--coverage", "-fprofile-update=atomic"] if coverage else []
    run_command(
        compiler, "-std=c11", "-D_GNU_SOURCE", "-O0" if coverage else "-O2", "-Wall", "-Wextra",
        "-Werror", *profile_flags, *sanitizer_flags, "-I", str(ROOT / "single"),
        "-I", str(ROOT / "include"),
        "-I", str(ROOT / "extlibs/xjwt/include"), "-I", str(ROOT / "extlibs/xoauth2/include"), str(OWNERSHIP_CLIENT if ownership else CLIENT),
        "-o", str(binary), *platform_libs, *sanitizer_flags,
    )
    return binary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--coverage-dir", type=Path,
                        help="retain GCC -O0 coverage binaries and counters in this directory")
    parser.add_argument("--output-dir", type=Path,
                        help="retain normal or sanitizer client binaries in this directory")
    choices = ("identity", "handshake", "many-headers", "trailers",
               "chunked-memory", "too-many-headers", "too-many-trailers",
               "upload-timeout", "upload-disconnect", "max-body",
               "too-large-body", "too-large-chunked", "nul-body",
               "close-body", "truncated-close-body", "tcp-eof",
               "truncated-fixed-body", "truncated-chunked-body", "lifecycle",
               "url-invalid", "url-vectors", "url-wire", "ownership")
    parser.add_argument("--case", action="append", choices=choices,
                        help="run only selected groups; default runs all")
    args = parser.parse_args()
    if args.sanitize and args.coverage_dir is not None:
        parser.error("--coverage-dir and --sanitize must run separately")
    if args.output_dir is not None and args.coverage_dir is not None:
        parser.error("--output-dir and --coverage-dir must run separately")
    selected = set(args.case or choices)
    with tempfile.TemporaryDirectory(prefix="xoauth2-tls-interop-") as temp:
        root = Path(temp)
        ca, other_ca, leaf, wrong_leaf, key = certificates(root)
        build_root = (args.coverage_dir or args.output_dir or root).resolve()
        build_root.mkdir(parents=True, exist_ok=True)
        coverage = args.coverage_dir is not None
        client = None if selected == {"ownership"} else build_client(
            build_root, args.compiler, args.sanitize, coverage=coverage)
        if "ownership" in selected:
            ownership_client = build_client(build_root, args.compiler, args.sanitize,
                                            ownership=True, coverage=coverage)
            completed = subprocess.run([str(ownership_client), str(ca)],
                                       capture_output=True, text=True, timeout=40)
            print(completed.stdout, end="", flush=True)
            if completed.returncode != 0:
                raise AssertionError(f"HTTP factory ownership: {completed.stderr}")
        if "url-invalid" in selected:
            invalid_url_case(client, ca, extra_numeric_aliases=(
                "2130706433", "017700000001", "127.1", "127.0.1", "0177.0.0.1", "127.0.0.01"))
        if "url-vectors" in selected:
            completed = subprocess.run([str(client), "unused", str(ca), "url-vectors"],
                                       capture_output=True, text=True, timeout=10)
            if completed.returncode != 0:
                raise AssertionError(completed.stderr)
            print(completed.stdout, end="")
        if "url-wire" in selected:
            for label, host, scheme, tail, target in (
                ("query without path", "localhost", "https", "?probe=query", "/?probe=query"),
                ("fragment excluded", "localhost", "https", "/token?q=%23#client-only", "/token?q=%23"),
                ("root fragment excluded", "localhost", "https", "#client-only", "/"),
                ("empty fragment excluded", "localhost", "https", "/token#", "/token"),
                ("fragment grammar", "localhost", "https", "/token?q=%23#AZaz09-._~!$&'()*+,;=:@/?%00%23%FF", "/token?q=%23"),
                ("DNS root dot", "localhost.", "https", "/token", "/token"),
                ("mixed-case HTTPS scheme", "localhost", "hTtPs", "/token", "/token"),
            ):
                case(client, ca, leaf, key, label=label, hostname=host, scheme=scheme,
                     bind="127.0.0.1", target=target, url_tail=tail,
                     expected_sni="localhost", success=True)
        if "lifecycle" in selected:
            completed = subprocess.run(
                [str(client), "unused", str(ca), "lifecycle"],
                capture_output=True, text=True, timeout=20,
            )
            if completed.returncode != 0:
                raise AssertionError(f"HTTP lifecycle: {completed.stderr}")
            print(completed.stdout, end="")
        if "identity" in selected:
            case(client, ca, leaf, key, label="DNS TLS and SNI",
                 hostname="localhost", bind="127.0.0.1", target="/?probe=dns",
                 expected_sni="localhost", success=True)
            case(client, ca, leaf, key, label="IPv4 TLS without SNI",
                 hostname="127.0.0.1", bind="127.0.0.1", target="/token?probe=ip4",
                 expected_sni=None, success=True)
            if socket.has_ipv6:
                try:
                    case(client, ca, leaf, key, label="IPv6 TLS without SNI",
                         hostname="::1", bind="::1", target="/token?probe=ip6",
                         expected_sni=None, success=True)
                except OSError as exc:
                    print(f"  IPv6 loopback unavailable: {exc}")
            case(client, other_ca, leaf, key, label="untrusted CA rejected",
                 hostname="localhost", bind="127.0.0.1", target="/token",
                 expected_sni="localhost", success=False)
            case(client, ca, wrong_leaf, key, label="wrong DNS identity rejected",
                 hostname="localhost", bind="127.0.0.1", target="/token",
                 expected_sni="localhost", success=False)
            case(client, ca, wrong_leaf, key, label="missing IP identity rejected",
                 hostname="127.0.0.1", bind="127.0.0.1", target="/token",
                 expected_sni=None, success=False)
        if "handshake" in selected:
            handshake_fault_case(client, ca, label="TLS handshake interrupted",
                                 stall=False)
            handshake_fault_case(client, ca, label="TLS handshake timeout",
                                 stall=True)
        payload = b'{"ok":true}'
        head = b"HTTP/1.1 200 OK\r\nContent-Length: 11\r\nConnection: close\r\n"
        chunk_head = b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        for total, name in ((100, "many-headers"), (101, "too-many-headers")):
            if name in selected:
                fields = b"".join(f"X-Probe-{i}: x\r\n".encode("ascii")
                                  for i in range(total - 2))
                case(client, ca, leaf, key, label=f"{total} response fields",
                     hostname="127.0.0.1", bind="127.0.0.1", target="/token",
                     expected_sni=None, success=total == 100,
                     mode="success" if total == 100 else "head-failure",
                     response=head + fields + b"\r\n" + payload, fragment_size=113)
        for total, name in ((100, "trailers"), (101, "too-many-trailers")):
            if name in selected:
                trailers = b"".join(f"X-Probe-{i}: x\r\n".encode("ascii")
                                    for i in range(total))
                case(client, ca, leaf, key, label=f"{total} chunked trailers",
                     hostname="127.0.0.1", bind="127.0.0.1", target="/token",
                     expected_sni=None, success=total == 100,
                     mode="success" if total == 100 else "body-failure",
                     response=chunk_head + b"b\r\n" + payload + b"\r\n0\r\n" +
                     trailers + b"\r\n", fragment_size=113)
        if "chunked-memory" in selected:
            chunk = b"1;p=" + b"v" * 4090 + b"\r\nx\r\n"
            case(client, ca, leaf, key, label="chunk extensions use bounded receive memory",
                 hostname="127.0.0.1", bind="127.0.0.1", target="/token",
                 expected_sni=None, success=True, mode="chunked",
                 response=chunk_head + chunk * 2048 + b"0\r\n\r\n", fragment_size=16381)
        for name, response in (
            ("max-body", b"HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n" + b"x" * 1048576),
            ("too-large-body", b"HTTP/1.1 200 OK\r\nContent-Length: 1048577\r\nConnection: close\r\n\r\nx"),
            ("too-large-chunked", chunk_head + b"100000\r\n" + b"x" * 1048576 + b"\r\n1\r\nx\r\n0\r\n\r\n"),
            ("nul-body", b"HTTP/1.1 200 OK\r\nContent-Length: 17\r\nConnection: close\r\n\r\n" + payload + b"\x00extra"),
        ):
            if name in selected:
                case(client, ca, leaf, key, label=name,
                     hostname="127.0.0.1", bind="127.0.0.1", target="/token",
                     expected_sni=None, success=name == "max-body",
                     mode="large-body" if name == "max-body" else "body-failure",
                     response=response, fragment_size=16381)
        if "upload-timeout" in selected:
            upload_fault_case(client, ca, leaf, key, disconnect=False)
        if "upload-disconnect" in selected:
            upload_fault_case(client, ca, leaf, key, disconnect=True)
        for name in ("close-body", "truncated-close-body"):
            if name in selected:
                graceful = name == "close-body"
                case(client, ca, leaf, key, label=name,
                     hostname="127.0.0.1", bind="127.0.0.1", target="/token",
                     expected_sni=None, success=graceful,
                     mode="success" if graceful else "failure",
                     response=b"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n" + payload,
                     fragment_size=13, graceful_close=graceful)
        if "tcp-eof" in selected:
            tcp_eof_case(client, ca, truncated=False)
            tcp_eof_case(client, ca, truncated=True)
        for name, response in (
            ("truncated-fixed-body", b"HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\n" + payload),
            ("truncated-chunked-body", chunk_head + b"b\r\n" + payload + b"\r\n"),
        ):
            if name in selected:
                case(client, ca, leaf, key, label=name,
                     hostname="127.0.0.1", bind="127.0.0.1", target="/token",
                     expected_sni=None, success=False, mode="body-failure",
                     response=response, fragment_size=13, graceful_close=True)
    print("xoauth2 independent HTTP/TLS interop passed")


if __name__ == "__main__":
    main()
