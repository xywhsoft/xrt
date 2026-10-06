"""Run the real POP3, SMTP and IMAP examples against Python's TLS server."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = {
    "pop3": ("xpop3", "pop3_client_example", "examples_client_main"),
    "smtp": ("xsmtp", "smtp_submit_example", "examples_submit_main"),
    "imap": ("ximap", "imap_client_example", "examples_client_main"),
}
CREDS = b"\x00demo\x00secret"
COVERAGE_FLAGS = ['-std=c11', '-D_GNU_SOURCE=', '-O0', '--coverage', '-fprofile-update=atomic']


def coverage_protocols(product: str) -> list[str]:
    if product not in ('xmail', 'xpop3', 'xsmtp', 'ximap'):
        raise ValueError('unknown mail coverage product')
    return list(EXAMPLES) if product == 'xmail' else [product[1:]]


def example_suite(kind: str, coverage_product: str | None = None) -> str:
    suite = EXAMPLES[kind][1]
    # Each measured product owns its profiles. The full product root also keeps
    # the example and module builds on the same owned-feature configuration.
    return suite if coverage_product is None else suite + ',' + coverage_product


def example_directory(kind: str, compiler: str, coverage_product: str | None = None) -> Path:
    platform = 'windows' if os.name == 'nt' else 'linux'
    architecture = 'native' if coverage_product is None else 'native-' + platform
    suite = example_suite(kind, coverage_product)
    return ROOT / 'out' / Path(compiler).stem.lower() / architecture / suite


def execution_path(product: str) -> Path:
    return ROOT / 'out/mail/tls-coverage' / sys.platform / product / 'execution.json'


def command(*args: str, env: dict[str, str] | None = None,
            timeout: int = 240) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, cwd=ROOT, env=env, text=True,
                          capture_output=True, timeout=timeout)


def require_command(*args: str, timeout: int = 240) -> None:
    result = command(*args, timeout=timeout)
    if result.returncode:
        raise AssertionError(
            f"{args[0]} exited {result.returncode}:\n{result.stdout[-3000:]}"
            f"\n{result.stderr[:6000]}\n{result.stderr[-3000:]}"
        )


def certificates(root: Path) -> tuple[Path, Path, Path, Path]:
    ca, ca_key = root / "ca.pem", root / "ca.key"
    leaf, leaf_key = root / "leaf.pem", root / "leaf.key"
    ip_leaf = root / "ip-leaf.pem"
    csr, ext = root / "leaf.csr", root / "leaf.ext"
    require_command(
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
        "-sha256", "-days", "2", "-subj", "/CN=mail interop CA",
        "-addext", "basicConstraints=critical,CA:TRUE",
        "-addext", "keyUsage=critical,keyCertSign,cRLSign",
        "-keyout", str(ca_key), "-out", str(ca),
    )
    require_command(
        "openssl", "req", "-newkey", "rsa:2048", "-nodes", "-sha256",
        "-subj", "/CN=localhost", "-keyout", str(leaf_key),
        "-out", str(csr),
    )
    ext.write_text(
        "basicConstraints=critical,CA:FALSE\n"
        "keyUsage=critical,digitalSignature,keyEncipherment\n"
        "extendedKeyUsage=serverAuth\n"
        "subjectAltName=DNS:localhost\n", encoding="ascii",
    )
    require_command(
        "openssl", "x509", "-req", "-in", str(csr), "-CA", str(ca),
        "-CAkey", str(ca_key), "-CAcreateserial", "-days", "2",
        "-sha256", "-extfile", str(ext), "-out", str(leaf),
    )
    ext.write_text(
        "basicConstraints=critical,CA:FALSE\n"
        "keyUsage=critical,digitalSignature,keyEncipherment\n"
        "extendedKeyUsage=serverAuth\n"
        "subjectAltName=IP:127.0.0.1,IP:::1\n", encoding="ascii",
    )
    require_command(
        "openssl", "x509", "-req", "-in", str(csr), "-CA", str(ca),
        "-CAkey", str(ca_key), "-CAcreateserial", "-days", "2",
        "-sha256", "-extfile", str(ext), "-out", str(ip_leaf),
    )
    return ca, leaf, ip_leaf, leaf_key


def build_examples(compiler: str, coverage_product: str | None = None) -> None:
    protocols = list(EXAMPLES) if coverage_product is None else coverage_protocols(coverage_product)
    for kind in protocols:
        product, _, _ = EXAMPLES[kind]
        suite = example_suite(kind, coverage_product)
        print(f'[interop build] {kind}: {suite}', flush=True)
        flags = []
        if coverage_product is not None:
            directory = example_directory(kind, compiler, coverage_product)
            for parent in (directory, directory / 'obj'):
                if parent.is_dir():
                    for counter in parent.glob('*.gcda'):
                        counter.unlink()
            flags = ['--target-platform', 'windows' if os.name == 'nt' else 'linux',
                     '--rebuild', *['--cflag=' + flag for flag in COVERAGE_FLAGS],
                     '--ldflag=--coverage']
        require_command(
            sys.executable, "tools/build.py", "--compiler", compiler,
            "--manifest", f"extlibs/{product}/config/modules.json",
            "--suite", suite, "--no-single", "--exclude-test=*",
            "--no-run", "--jobs", "4", *flags, timeout=900,
        )


def line(reader: object) -> bytes:
    data = reader.readline(8193)
    if not data.endswith(b"\r\n"):
        raise AssertionError(f"missing CRLF or oversized protocol line: {data[:100]!r}")
    return data[:-2]


def expect(reader: object, wanted: bytes) -> None:
    got = line(reader)
    if got != wanted:
        raise AssertionError(f"expected {wanted!r}, got {got!r}")


def pop3(tls: ssl.SSLSocket, greeting: bool = True,
         drop_after: bool = False, close_notify: bool = True) -> None:
    reader = tls.makefile("rb")
    if greeting:
        tls.sendall(b"+OK localhost ready\r\n")
    expect(reader, b"CAPA")
    tls.sendall(b"+OK capabilities\r\nUSER\r\nXOAUTH2\r\nUTF8\r\nX_VENDOR!\r\n.\r\n")
    expect(reader, b"USER demo")
    tls.sendall(b"+OK user\r\n")
    expect(reader, b"PASS secret")
    tls.sendall(b"+OK mailbox\r\n")
    expect(reader, b"RETR 1")
    if drop_after:
        tls.sendall(b"+OK message\r\nSubject: truncated\r\n\r\npartial\r\n")
        reader.close()
        return
    tls.sendall(b"+OK message\r\nSubject: interop\r\n\r\n"
                b"Hello from independent TLS.\r\n.\r\n")
    expect(reader, b"QUIT")
    tls.sendall(b"+OK bye\r\n")
    reader.close()
    if close_notify:
        tls.unwrap()


def smtp(tls: ssl.SSLSocket, greeting: bool = True,
         drop_after: bool = False, close_notify: bool = True) -> None:
    reader = tls.makefile("rb")
    if greeting:
        tls.sendall(b"220 localhost ready\r\n")
    expect(reader, b"EHLO localhost")
    tls.sendall(b"250-localhost\r\n250 AUTH PLAIN\r\n")
    auth = line(reader)
    if not auth.startswith(b"AUTH PLAIN ") or \
            base64.b64decode(auth[11:], validate=True) != CREDS:
        raise AssertionError(f"bad SMTP AUTH PLAIN: {auth!r}")
    tls.sendall(b"235 authenticated\r\n")
    expect(reader, b"MAIL FROM:<sender@example.test>")
    tls.sendall(b"250 sender accepted\r\n")
    expect(reader, b"RCPT TO:<recipient@example.test>")
    tls.sendall(b"250 recipient accepted\r\n")
    expect(reader, b"DATA")
    tls.sendall(b"354 send data\r\n")
    data = bytearray()
    while True:
        part = line(reader)
        if part == b".":
            break
        data.extend(part + b"\r\n")
        if len(data) > 65536:
            raise AssertionError("SMTP DATA exceeded fixture bound")
    if b"Subject: TLS interop" not in data or \
            b"Hello from independent TLS." not in data:
        raise AssertionError("SMTP message content mismatch")
    if drop_after:
        reader.close()
        return
    tls.sendall(b"250 queued\r\n")
    expect(reader, b"QUIT")
    tls.sendall(b"221 bye\r\n")
    reader.close()
    if close_notify:
        tls.unwrap()


def imap(tls: ssl.SSLSocket, greeting: bool = True,
         drop_after: bool = False, close_notify: bool = True) -> None:
    reader = tls.makefile("rb")
    if greeting:
        tls.sendall(b"* OK localhost ready\r\n")
    first = line(reader)
    tag, separator, command_name = first.partition(b" ")
    if not separator or command_name != b"CAPABILITY":
        raise AssertionError(f"bad IMAP CAPABILITY: {first!r}")
    tls.sendall(b"* CAPABILITY IMAP4rev1 AUTH=PLAIN\r\n" +
                tag + b" OK capability complete\r\n")
    auth = line(reader)
    tag, separator, command_name = auth.partition(b" ")
    if not separator or command_name != b"AUTHENTICATE PLAIN":
        raise AssertionError(f"bad IMAP AUTHENTICATE: {auth!r}")
    tls.sendall(b"+ \r\n")
    response = line(reader)
    if base64.b64decode(response, validate=True) != CREDS:
        raise AssertionError("IMAP PLAIN credentials mismatch")
    tls.sendall(tag + b" OK authenticated\r\n")
    examine = line(reader)
    examine_tag, separator, command_name = examine.partition(b" ")
    if not separator or command_name != b'EXAMINE "INBOX"':
        raise AssertionError(f"bad IMAP EXAMINE: {examine!r}")
    if drop_after:
        tls.sendall(b"* 2 EXISTS\r\n")
        reader.close()
        return
    tls.sendall(b"* 2 EXISTS\r\n* 0 RECENT\r\n"
                b"* OK [UIDVALIDITY 42] valid\r\n" +
                examine_tag + b" OK [READ-ONLY] examined\r\n")
    logout = line(reader)
    logout_tag, separator, command_name = logout.partition(b" ")
    if not separator or command_name != b"LOGOUT":
        raise AssertionError(f"bad IMAP LOGOUT: {logout!r}")
    tls.sendall(b"* BYE signing off\r\n" +
                logout_tag + b" OK logout complete\r\n")
    reader.close()
    if close_notify:
        tls.unwrap()


def upgrade(raw: socket.socket, context: ssl.SSLContext,
            kind: str, stages: list[str] | None = None) -> ssl.SSLSocket:
    reader = raw.makefile("rb")
    if kind == "pop3":
        raw.sendall(b"+OK localhost ready\r\n")
        expect(reader, b"CAPA")
        raw.sendall(b"+OK capabilities\r\nSTLS\r\nUSER\r\nXOAUTH2\r\n.\r\n")
        expect(reader, b"STLS")
        raw.sendall(b"+OK begin TLS\r\n")
    elif kind == "smtp":
        raw.sendall(b"220 localhost ready\r\n")
        expect(reader, b"EHLO localhost")
        raw.sendall(b"250-localhost\r\n250 STARTTLS\r\n")
        expect(reader, b"STARTTLS")
        raw.sendall(b"220 begin TLS\r\n")
    else:
        raw.sendall(b"* OK localhost ready\r\n")
        first = line(reader)
        tag, separator, command_name = first.partition(b" ")
        if not separator or command_name != b"CAPABILITY":
            raise AssertionError(f"bad IMAP pre-TLS CAPABILITY: {first!r}")
        raw.sendall(b"* CAPABILITY IMAP4rev1 STARTTLS\r\n" +
                    tag + b" OK capability complete\r\n")
        request = line(reader)
        tag, separator, command_name = request.partition(b" ")
        if not separator or command_name != b"STARTTLS":
            raise AssertionError(f"bad IMAP STARTTLS: {request!r}")
        raw.sendall(tag + b" OK begin TLS\r\n")
    reader.close()
    if stages is not None:
        stages.append("handshake")
    return context.wrap_socket(raw, server_side=True)


def ipv6_loopback_available() -> bool:
    if not socket.has_ipv6:
        return False
    try:
        with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as probe:
            probe.bind(("::1", 0))
        return True
    except OSError:
        return False


def run_case(kind: str, mode: str, ca: Path, leaf: Path, key: Path,
             compiler: str, *, hostname: str = "localhost",
             reject_identity: bool = False,
             drop_after: bool = False,
             coverage_product: str | None = None,
             server_fault: str | None = None,
             close_notify: bool = True) -> None:
    if server_fault not in (None, "rejection-reset", "rejection-timeout",
                            "protocol-ssl", "protocol-reset"):
        raise ValueError("unknown server fault")
    product, suite, name = EXAMPLES[kind]
    client = example_directory(kind, compiler, coverage_product) / (
        name + (".exe" if os.name == "nt" else "")
    )
    if not client.is_file():
        raise AssertionError(f"missing built {kind} example: {client}")
    ipv6 = hostname == "::1"
    listener = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET,
                             socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("::1" if ipv6 else "127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(45)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(leaf), str(key))
    seen_sni: list[str | None] = []
    context.set_servername_callback(
        lambda _sock, host, _context: seen_sni.append(host)
    )
    errors: list[Exception] = []
    stages = ["accept"]
    error_stages: list[str] = []

    def serve() -> None:
        try:
            with listener.accept()[0] as raw:
                raw.settimeout(20)
                try:
                    if mode == "tls":
                        stages.append("handshake")
                        tls = context.wrap_socket(raw, server_side=True)
                    else:
                        stages.append("upgrade")
                        tls = upgrade(raw, context, kind, stages)
                except (ssl.SSLError, ConnectionResetError) as exc:
                    # Controlled error surfaces for an actual failed handshake;
                    # these do not claim to force a native TCP reset or timeout.
                    if server_fault == "rejection-reset":
                        raise ConnectionResetError(10054, "controlled handshake reset") from exc
                    if server_fault == "rejection-timeout":
                        raise TimeoutError("controlled handshake timeout") from exc
                    raise
                with tls:
                    stages.append("protocol")
                    if server_fault == "protocol-ssl":
                        raise ssl.SSLError("controlled error after successful handshake")
                    if server_fault == "protocol-reset":
                        raise ConnectionResetError(10054, "controlled reset after successful handshake")
                    {"pop3": pop3, "smtp": smtp, "imap": imap}[kind](
                        tls, greeting=mode == "tls", drop_after=drop_after,
                        close_notify=close_notify
                    )
                stages.append("complete")
        except Exception as exc:
            errors.append(exc)
            error_stages.append(stages[-1])

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    port = str(listener.getsockname()[1])
    host = hostname
    args = {
        "pop3": [host, port, str(ca), "1",
                 "tls" if mode == "tls" else "stls"],
        "smtp": [host, port, str(ca), "sender@example.test",
                 "recipient@example.test", "TLS interop",
                 "Hello from independent TLS.",
                 "tls" if mode == "tls" else "starttls"],
        "imap": [host, port, str(ca), "INBOX",
                 "tls" if mode == "tls" else "starttls"],
    }[kind]
    env = os.environ.copy()
    env.update({
        "XPOP3_USER": "demo", "XPOP3_PASSWORD": "secret",
        "XSMTP_USER": "demo", "XSMTP_PASSWORD": "secret",
        "XIMAP_USER": "demo", "XIMAP_PASSWORD": "secret",
    })
    try:
        # Preserve raw message bytes: universal newline conversion would hide
        # Windows text-mode CRCRLF corruption in the POP3 example.
        result = subprocess.run([str(client), *args], cwd=ROOT, env=env,
                                capture_output=True, timeout=45)
        raw_stdout = result.stdout
        result.stdout = result.stdout.decode('utf-8', errors='replace')
        result.stderr = result.stderr.decode('utf-8', errors='replace')
    finally:
        worker.join(timeout=25)
        listener.close()
    if reject_identity:
        expected_result = (result.returncode == 1 and len(errors) == 1 and
                           error_stages == ["handshake"] and
                           isinstance(errors[0], (ssl.SSLError, ConnectionResetError)))
    elif drop_after:
        expected_result = result.returncode == 1 and not errors
    else:
        expected_result = result.returncode == 0 and not errors
    valid = (not worker.is_alive() and expected_result and
             seen_sni == ([None] if hostname != "localhost" else
                          ["localhost"]))
    if not valid:
        raise AssertionError(
            f"{kind} {mode} identity={host} interop failed: "
            f"result={result.returncode}, "
            f"stdout={result.stdout!r}, stderr={result.stderr!r}, "
            f"SNI={seen_sni!r}, server errors={errors!r}, error stages={error_stages!r}"
        )
    if not close_notify and not reject_identity and not drop_after and \
            'completed; shutdown failed:' not in result.stderr:
        raise AssertionError('completed operation lost the TLS shutdown diagnostic')
    if not reject_identity and not drop_after and kind == "pop3" and \
            raw_stdout != b"Subject: interop\r\n\r\nHello from independent TLS.\r\n":
        raise AssertionError("POP3 retrieval output mismatch")
    if not reject_identity and not drop_after and kind == "imap" and \
            "INBOX: 2 messages" not in result.stdout:
        raise AssertionError("IMAP mailbox output mismatch")
    if drop_after and kind == "imap" and \
            "INBOX: 2 messages" in result.stdout:
        raise AssertionError("IMAP accepted an incomplete EXAMINE response")
    label = ("identity rejection" if reject_identity else
             "truncated TLS response" if drop_after else
             f"{hostname} example")
    print(f"  {kind.upper()} {mode} {label}: passed")


def rejection_policy_cases(kind: str, mode: str, ca: Path, leaf: Path,
                           ip_leaf: Path, key: Path, compiler: str,
                           coverage_product: str | None = None) -> None:
    """Require a real rejection, and exclude failures after identity acceptance."""
    run_case(kind, mode, ca, leaf, key, compiler, hostname="127.0.0.1",
             reject_identity=True, coverage_product=coverage_product,
             server_fault="rejection-reset")
    for fault in ("rejection-timeout", "protocol-ssl", "protocol-reset"):
        stage = "handshake" if fault == "rejection-timeout" else "protocol"
        try:
            run_case(kind, mode, ca, leaf if stage == "handshake" else ip_leaf,
                     key, compiler, hostname="127.0.0.1", reject_identity=True,
                     coverage_product=coverage_product, server_fault=fault)
        except AssertionError as exc:
            # A missing binary, bad greeting, or build failure is not this control.
            if f"error stages=['{stage}']" not in str(exc):
                raise
        else:
            raise AssertionError(f"{kind}/{mode} accepted {fault} as certificate rejection")
    print(f"  {kind.upper()} {mode} rejection classification controls: passed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--rejection-policy", action="store_true",
                        help="check handshake reset, timeout and post-handshake error classification")
    parser.add_argument('--coverage-product', choices=('xmail', 'xpop3', 'xsmtp', 'ximap'),
                        help='build and retain isolated GCC coverage profiles for this product')
    args = parser.parse_args()
    if args.rejection_policy and args.coverage_product is not None:
        parser.error("--rejection-policy controls run separately from primary coverage")
    if args.coverage_product is not None:
        if args.skip_build:
            parser.error('--coverage-product requires a fresh build; --skip-build is not permitted')
        if Path(args.compiler).stem.lower() != 'gcc' or sys.platform not in ('win32', 'linux'):
            parser.error('mail coverage requires GCC on Windows or Linux')
        execution_path(args.coverage_product).unlink(missing_ok=True)
    if not args.skip_build:
        build_examples(args.compiler, args.coverage_product)
    protocols = list(EXAMPLES) if args.coverage_product is None else coverage_protocols(args.coverage_product)
    cases = []
    with tempfile.TemporaryDirectory(prefix="mail-tls-interop-") as temp:
        ca, leaf, ip_leaf, key = certificates(Path(temp))
        ipv6 = ipv6_loopback_available()
        if not ipv6:
            print("  IPv6 loopback unavailable; IPv6 cases skipped")
        def exercise(kind, mode, certificate, **options):
            run_case(kind, mode, ca, certificate, key, args.compiler,
                     coverage_product=args.coverage_product, **options)
            cases.append({'protocol': kind, 'mode': mode, 'hostname': options.get('hostname', 'localhost'),
                          'reject_identity': options.get('reject_identity', False),
                          'truncated_response': options.get('drop_after', False),
                          'close_notify': options.get('close_notify', True)})
        for kind in protocols:
            for mode in ("tls", "starttls"):
                exercise(kind, mode, leaf)
                exercise(kind, mode, leaf, close_notify=False)
                exercise(kind, mode, leaf, drop_after=True)
                exercise(kind, mode, ip_leaf, hostname='127.0.0.1')
                if ipv6:
                    exercise(kind, mode, ip_leaf, hostname='::1')
                exercise(kind, mode, leaf, hostname='127.0.0.1', reject_identity=True)
                if args.rejection_policy:
                    rejection_policy_cases(kind, mode, ca, leaf, ip_leaf, key, args.compiler)
    if args.coverage_product is not None:
        binaries = [example_directory(kind, args.compiler, args.coverage_product) /
                    (EXAMPLES[kind][2] + ('.exe' if os.name == 'nt' else '')) for kind in protocols]
        report = {'schema': 2, 'product': args.coverage_product, 'platform': sys.platform,
                  'compile_flags': COVERAGE_FLAGS, 'ipv6_available': ipv6, 'cases': cases,
                  'binary_sha256': {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in binaries}}
        path = execution_path(args.coverage_product)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print("independent mail TLS interop passed")


if __name__ == "__main__":
    main()
