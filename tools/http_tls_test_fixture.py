"""Independent Python HTTP/TLS fixtures for extension transport tests."""

from __future__ import annotations

import socket
import ssl
import subprocess
import threading
import time
from pathlib import Path


RESPONSE = b'HTTP/1.1 200 OK\r\nContent-Length: 11\r\nConnection: close\r\n\r\n{"ok":true}'


def run_command(*args: str) -> None:
    result = subprocess.run(args, capture_output=True, text=True, timeout=240)
    if result.returncode != 0:
        raise RuntimeError(f"{args[0]} exited {result.returncode}:\n{result.stderr}")


def certificates(root: Path) -> tuple[Path, Path, Path, Path, Path]:
    ca = root / "ca.pem"
    ca_key = root / "ca.key"
    other_ca = root / "other-ca.pem"
    leaf = root / "leaf.pem"
    wrong_leaf = root / "wrong-leaf.pem"
    leaf_key = root / "leaf.key"
    request = root / "leaf.csr"
    extensions = root / "leaf.ext"
    # A locally built oracle may have no system openssl.cnf at its prefix.
    # Keep certificate generation independent of the runner's installation.
    config = root / "openssl.cnf"
    config.write_text("[req]\ndistinguished_name=dn\n[dn]\n", encoding="ascii")
    for name, cert, key in (
        ("xoauth2 loopback CA", ca, ca_key),
        ("untrusted loopback CA", other_ca, root / "other-ca.key"),
    ):
        run_command(
            "openssl", "req", "-config", str(config), "-x509", "-newkey", "rsa:2048", "-nodes",
            "-sha256", "-days", "2", "-subj", f"/CN={name}",
            "-addext", "basicConstraints=critical,CA:TRUE",
            "-addext", "keyUsage=critical,keyCertSign,cRLSign",
            "-keyout", str(key), "-out", str(cert),
        )
    run_command(
        "openssl", "req", "-config", str(config), "-newkey", "rsa:2048", "-nodes", "-sha256",
        "-subj", "/CN=localhost", "-keyout", str(leaf_key),
        "-out", str(request),
    )
    extensions.write_text(
        "basicConstraints=critical,CA:FALSE\n"
        "keyUsage=critical,digitalSignature,keyEncipherment\n"
        "extendedKeyUsage=serverAuth\n"
        "subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1\n",
        encoding="ascii",
    )
    run_command(
        "openssl", "x509", "-req", "-in", str(request),
        "-CA", str(ca), "-CAkey", str(ca_key), "-CAcreateserial",
        "-days", "2", "-sha256", "-extfile", str(extensions),
        "-out", str(leaf),
    )
    extensions.write_text(
        "basicConstraints=critical,CA:FALSE\n"
        "keyUsage=critical,digitalSignature,keyEncipherment\n"
        "extendedKeyUsage=serverAuth\n"
        "subjectAltName=DNS:example.invalid\n",
        encoding="ascii",
    )
    run_command(
        "openssl", "x509", "-req", "-in", str(request),
        "-CA", str(ca), "-CAkey", str(ca_key), "-CAcreateserial",
        "-days", "2", "-sha256", "-extfile", str(extensions),
        "-out", str(wrong_leaf),
    )
    return ca, other_ca, leaf, wrong_leaf, leaf_key


def invalid_url_case(client: Path, ca: Path, *,
                     extra_numeric_aliases: tuple[str, ...] = ()) -> None:
    """Malformed URI components must fail before any connection to a real listener."""
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(16)
    listener.settimeout(0.1)
    port = listener.getsockname()[1]
    stop = threading.Event()
    peers = []
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
            peers.append(True)
            with peer:
                peer.settimeout(1)
                try:
                    peer.recv(16384)
                    peer.sendall(RESPONSE)
                except (ConnectionError, TimeoutError):
                    pass

    host = f"127.0.0.1:{port}"
    urls = [
        f"http://{host}junk/token", f"http://{host}@ignored.invalid/token",
        f"http://{host} /token", f"http://{host}:80/token",
        f"http://user@{host}/token", f"http://{host}\\x/token",
        "http://127.0.0.1:/token", "http://127.0.0.1:0/token",
        "http://127.0.0.1:65536/token", "http://127.0.0.1:999999999999999999999/token",
        "http://127.0.0.1:+80/token", "http://127.0.0.1:-80/token",
        "http://127.0.0.1: 80/token", "http://[::1/token",
        "http://[]/token", "http://[::1]junk/token", "http://[::1]:/token",
        "http://[::1]:80junk/token", "http://::1/token", "http://[v1.fe]/token",
        "http://[fe80::1%25lo]/token", "http://[127.0.0.1]/token",
        "http://[1::2::3]/token", "http://127.0.0.999/token",
        "http://127.0.0.01/token", "http://127.1/token", "http://2130706433/token",
        "http://0x7f000001/token", "http://.localhost/token", "http://localhost../token",
        f"http://{host}/bad%", f"http://{host}/bad%0", f"http://{host}/bad%GG",
        f"http://{host}/bad path", f"http://{host}/bad\r\nInjected:x",
        f"http://{host}/bad\\path", f"http://{host}/" + "x" * 1023,
        "http://" + "a" * 256 + "/token", "http:///token", "ftp://127.0.0.1/token",
    ]
    # These legacy inet_aton forms must not be treated as DNS identities.
    for alias in ("0x7f.0.0.1", "127.0x0.0.1", "127.0.0x0.1", "127.0.0.0x1",
                  "0X7F.0.0.1", "0x7f.1", "127.0x1", "0177.0x0.0.1",
                  "0x7f.0x0.0X0.0x1", "0x7f000001", "0X7F000001",
                  "0x7f.0.0.1.", "0x100.0.0.1", "127.0x1000000") + extra_numeric_aliases:
        urls.append(f"http://{alias}:{port}/token")
    for fragment in ("bad%", "bad%0", "bad%GG", "bad space", "bad\tvalue",
                     "bad\r\nInjected:x", "bad\\value", "bad#second", "bad[", "bad]",
                     "bad<", "bad>", 'bad"', "bad^", "bad`", "bad{", "bad}", "bad|",
                     "bad\x7f", "badé", "bad中文"):
        urls.append(f"http://{host}/token#{fragment}")
    urls.extend((f"http://{host}#bad%", f"http://{host}?value=x#bad%GG",
                 "http://[::1]/token#bad%", "http://[::1]#bad space"))
    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    failures = []
    try:
        for index, url in enumerate(urls):
            # Preserve the actual URI bytes on Windows: narrow argv substitutes
            # non-ASCII characters with '?' on some runner code pages.
            completed = subprocess.run([str(client), "--url-stdin", str(ca), "url-failure", "50000"],
                                       input=url, encoding="utf-8",
                                       capture_output=True, timeout=10)
            if completed.returncode != 0:
                failures.append(f"URL {index}: {completed.stderr.strip()}")
    finally:
        stop.set()
        worker.join(timeout=3)
        listener.close()
    if worker.is_alive() or errors or peers or failures:
        raise AssertionError(f"invalid URLs connected {len(peers)} times; server={errors}; " +
                             "; ".join(failures))
    print(f"  {len(urls)} malformed URLs rejected before connecting: passed")


def case(
    client: Path, ca: Path, leaf: Path, key: Path, *,
    label: str, hostname: str, bind: str, target: str,
    expected_sni: str | None, success: bool,
    response: bytes = RESPONSE, mode: str | None = None,
    fragment_size: int = 16384, graceful_close: bool = False,
    url_tail: str | None = None, scheme: str = "https",
) -> None:
    family = socket.AF_INET6 if ":" in bind else socket.AF_INET
    listener = socket.socket(family, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((bind, 0))
    listener.listen(1)
    listener.settimeout(15)
    port = listener.getsockname()[1]
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(leaf), str(key))
    seen_sni: list[str | None] = []
    requests: list[bytes] = []
    errors: list[Exception] = []
    context.set_servername_callback(
        lambda _socket, name, _context: seen_sni.append(name)
    )

    def serve() -> None:
        try:
            with listener.accept()[0] as raw:
                raw.settimeout(10)
                with context.wrap_socket(raw, server_side=True) as tls:
                    request = bytearray()
                    while b"\r\n\r\n" not in request and len(request) < 16384:
                        piece = tls.recv(4096)
                        if not piece:
                            break
                        request.extend(piece)
                    requests.append(bytes(request))
                    for offset in range(0, len(response), fragment_size):
                        tls.sendall(response[offset:offset + fragment_size])
                    if graceful_close:
                        # Complete both close_notify directions; EOF is authenticated.
                        tls.unwrap().close()
        except Exception as exc:  # Expected for certificate rejection.
            errors.append(exc)

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    host = f"[{hostname}]" if ":" in hostname else hostname
    url = f"{scheme}://{host}:{port}{target if url_tail is None else url_tail}"
    try:
        completed = subprocess.run(
            [str(client), url, str(ca),
             mode or ("success" if success else "failure")],
            capture_output=True, text=True, timeout=25,
        )
    finally:
        worker.join(timeout=12)
        listener.close()
    if worker.is_alive():
        raise AssertionError(f"{label}: TLS server did not finish")
    if completed.returncode != 0:
        raise AssertionError(
            f"{label}: client exit {completed.returncode}: {completed.stderr}"
        )
    if success or mode is not None:
        if success and errors:
            raise AssertionError(f"{label}: server error: {errors[0]}")
        if len(requests) != 1 or not requests[0].startswith(
            f"GET {target} HTTP/1.1\r\n".encode("ascii")
        ):
            raise AssertionError(f"{label}: wrong HTTP request target")
        host_values = [line.split(b":", 1)[1].strip() for line in
                       requests[0].split(b"\r\n")[1:]
                       if line.lower().startswith(b"host:")]
        expected_host = host if port == 443 else f"{host}:{port}"
        if host_values != [expected_host.encode("ascii")]:
            raise AssertionError(f"{label}: wrong Host field: {host_values}")
        if not success and any(not isinstance(exc, (ssl.SSLError, ConnectionError))
                               for exc in errors):
            raise AssertionError(f"{label}: unexpected server error: {errors}")
    elif not errors or not isinstance(errors[0], ssl.SSLError):
        raise AssertionError(f"{label}: expected a rejected TLS handshake: {errors}")
    if expected_sni is None:
        if seen_sni != [None]:
            raise AssertionError(f"{label}: unexpected IP SNI: {seen_sni}")
    elif seen_sni != [expected_sni]:
        raise AssertionError(f"{label}: wrong DNS SNI: {seen_sni}")
    print(f"  {label}: passed")
    if mode == "chunked":
        print(f"    {completed.stderr.strip()}")


def upload_fault_case(client: Path, ca: Path, leaf: Path, key: Path, *,
                      disconnect: bool) -> None:
    """Stop reading after a real POST prefix; inspect close before client cleanup."""
    label = "TLS upload disconnect" if disconnect else "TLS partial upload timeout"
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(6)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(leaf), str(key))
    release = threading.Event()
    returned = threading.Event()
    marker: list[str] = []
    received: list[int] = []
    closed: list[bool] = []
    errors: list[Exception] = []

    def serve() -> None:
        try:
            with listener.accept()[0] as raw:
                raw.settimeout(4)
                with context.wrap_socket(raw, server_side=True,
                                         suppress_ragged_eofs=False) as tls:
                    request = bytearray()
                    while b"\r\n\r\n" not in request:
                        piece = tls.recv(16384)
                        if not piece or len(request) + len(piece) > 32768:
                            raise AssertionError("POST header not received")
                        request.extend(piece)
                    head, prefix = bytes(request).split(b"\r\n\r\n", 1)
                    if not head.startswith(b"POST /token HTTP/1.1\r\n") or \
                            b"\r\nContent-Length: 8388608\r\n" not in head + b"\r\n":
                        raise AssertionError("unexpected POST header")
                    if not prefix:
                        prefix = tls.recv(16384)
                    if not prefix or prefix.strip(b"x"):
                        raise AssertionError("POST body prefix not received")
                    total = len(prefix)
                    if disconnect:
                        received.append(total)
                        return
                    if not release.wait(timeout=6):
                        raise AssertionError("client did not return while peer held read")
                    # The observer and HTTP engine still exist while this loop runs.
                    while True:
                        try:
                            piece = tls.recv(16384)
                        except (ssl.SSLEOFError, ConnectionResetError):
                            closed.append(True)
                            break
                        if not piece:
                            raise AssertionError("expected Abort, received normal TLS close")
                        if piece.strip(b"x"):
                            raise AssertionError("unexpected data after POST prefix")
                        total += len(piece)
                    received.append(total)
        except Exception as exc:
            errors.append(exc)

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    url = f"https://127.0.0.1:{listener.getsockname()[1]}/token"
    started = time.monotonic()
    process = subprocess.Popen(
        [str(client), url, str(ca), "send-failure", "1000"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True,
    )

    def read_returned() -> None:
        assert process.stdout is not None
        marker.append(process.stdout.readline())
        returned.set()

    watcher = threading.Thread(target=read_returned, daemon=True)
    watcher.start()
    try:
        if not returned.wait(timeout=6) or marker != ["RETURNED\n"]:
            raise AssertionError(f"{label}: client did not report return before destruction")
        elapsed = time.monotonic() - started
        release.set()
        worker.join(timeout=5)
        if worker.is_alive() or errors:
            raise AssertionError(f"{label}: peer did not observe failure: {errors}")
        if len(received) != 1 or not 0 < received[0] < 8 * 1024 * 1024:
            raise AssertionError(f"{label}: POST not partially sent: {received}")
        if not disconnect and closed != [True]:
            raise AssertionError(f"{label}: missing Abort before client destruction")
        _, stderr = process.communicate(input="\n", timeout=5)
        if process.returncode != 0:
            raise AssertionError(f"{label}: client exit {process.returncode}: {stderr}")
        if elapsed >= 4:
            raise AssertionError(f"{label}: client exceeded send deadline ({elapsed:.2f}s)")
        listener.settimeout(0.2)
        try:
            extra, _ = listener.accept()
        except TimeoutError:
            pass
        else:
            extra.close()
            raise AssertionError(f"{label}: failed POST was retried")
        print(f"  {label}: passed ({received[0]} body bytes, {elapsed:.2f}s)")
    finally:
        release.set()
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
        worker.join(timeout=5)
        watcher.join(timeout=1)
        listener.close()


def handshake_fault_case(client: Path, ca: Path, *, label: str,
                         stall: bool) -> None:
    """Keep a raw peer open until the client returns, or drop its ClientHello."""
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(5)
    port = listener.getsockname()[1]
    release = threading.Event()
    records: list[bytes] = []
    errors: list[Exception] = []

    def serve() -> None:
        try:
            with listener.accept()[0] as raw:
                raw.settimeout(4)
                header = bytearray()
                while len(header) < 6:
                    piece = raw.recv(6 - len(header))
                    if not piece:
                        break
                    header.extend(piece)
                records.append(bytes(header))
                if stall:
                    release.wait(timeout=8)
        except Exception as exc:
            errors.append(exc)

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    url = f"https://127.0.0.1:{port}/token"
    started = time.monotonic()
    try:
        completed = subprocess.run(
            [str(client), url, str(ca), "failure", "1000"],
            capture_output=True, text=True, timeout=5,
        )
        elapsed = time.monotonic() - started
    finally:
        release.set()
        worker.join(timeout=6)
        listener.close()
    if worker.is_alive():
        raise AssertionError(f"{label}: raw TCP peer did not finish")
    if errors:
        raise AssertionError(f"{label}: raw TCP peer error: {errors[0]}")
    if len(records) != 1:
        raise AssertionError(f"{label}: expected one TLS ClientHello")
    if len(records[0]) != 6 or records[0][:2] != b"\x16\x03" or records[0][5] != 1:
        raise AssertionError(f"{label}: expected a TLS ClientHello, got {records[0]!r}")
    if completed.returncode != 0:
        raise AssertionError(
            f"{label}: client exit {completed.returncode}: {completed.stderr}"
        )
    if elapsed >= 4:
        raise AssertionError(f"{label}: client exceeded handshake deadline ({elapsed:.2f}s)")
    print(f"  {label}: passed ({elapsed:.2f}s)")


def tcp_eof_case(client: Path, ca: Path, *, truncated: bool) -> None:
    """FIN completes a close-delimited body, but never a short fixed body."""
    label = "TCP truncated fixed body" if truncated else "TCP close-delimited body"
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(10)
    port = listener.getsockname()[1]
    errors: list[Exception] = []
    requests: list[bytes] = []

    def serve() -> None:
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(10)
                request = bytearray()
                while b"\r\n\r\n" not in request and len(request) < 16384:
                    piece = peer.recv(4096)
                    if not piece:
                        break
                    request.extend(piece)
                requests.append(bytes(request))
                framing = b"Content-Length: 12\r\n" if truncated else b""
                peer.sendall(b"HTTP/1.1 200 OK\r\n" + framing +
                             b"Connection: close\r\n\r\n{\"ok\":true}")
                peer.shutdown(socket.SHUT_WR)
                try:
                    if peer.recv(1) != b"":
                        raise AssertionError("unexpected request replay")
                except ConnectionResetError:
                    if not truncated:
                        raise
        except Exception as exc:
            errors.append(exc)

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    try:
        completed = subprocess.run(
            [str(client), f"http://127.0.0.1:{port}/token", str(ca),
             "failure" if truncated else "success"],
            capture_output=True, text=True, timeout=25,
        )
    finally:
        worker.join(timeout=12)
        listener.close()
    if completed.returncode != 0:
        raise AssertionError(f"{label}: client exit {completed.returncode}: {completed.stderr}")
    if worker.is_alive() or errors:
        raise AssertionError(f"{label}: server did not complete normally: {errors}")
    if len(requests) != 1 or not requests[0].startswith(b"GET /token HTTP/1.1\r\n"):
        raise AssertionError(f"{label}: wrong HTTP request")
    print(f"  {label}: passed")
