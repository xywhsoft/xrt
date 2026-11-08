"""Independent OpenSSL certificate proof fixture for the C cold-cycle benchmark.

The documented public example CA signs a fresh temporary dual-purpose leaf.
The measured child uses the production C API; this process runs no client code.
"""
import argparse
import base64
from datetime import datetime, timedelta, timezone
from pathlib import Path
import re
import socket
import ssl
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[4]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--cycles", type=int, required=True)
    args = parser.parse_args()
    if not 1 <= args.cycles <= 10000:
        parser.error("cycle count must be bounded")
    material = (ROOT / "examples/tls/embedded_identity.h").read_text(encoding="utf-8")
    def array(name):
        found = re.search(name + r"\[\d+\]\s*=\s*\{(.*?)\};", material, re.S)
        if found is None:
            raise ValueError("fixed fixture array missing")
        return bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", found.group(1)))
    authority = array("g_ExampleRsaCertDer")
    def pem(label, der):
        encoded = base64.b64encode(der).decode()
        return ("-----BEGIN " + label + "-----\n" + "\n".join(encoded[i:i + 64] for i in range(0, len(encoded), 64)) +
                "\n-----END " + label + "-----\n").encode()
    with tempfile.TemporaryDirectory(prefix="xredis-auth-bench-") as temporary:
        import cryptography
        from cryptography import x509
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import rsa
        from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID
        ca = x509.load_der_x509_certificate(authority)
        ca_key = serialization.load_der_private_key(array("g_ExampleRsaKeyDer"), None)
        leaf_key = rsa.generate_private_key(65537, 2048)
        now = datetime.now(timezone.utc)
        leaf = (x509.CertificateBuilder().issuer_name(ca.subject)
            .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "xredis-auth-bench-leaf")]))
            .public_key(leaf_key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now - timedelta(days=1)).not_valid_after(now + timedelta(days=7))
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName("localhost")]), critical=False)
            .add_extension(x509.KeyUsage(True, False, True, False, False, False, False, False, False), critical=True)
            .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH, ExtendedKeyUsageOID.CLIENT_AUTH]), critical=False)
            .sign(ca_key, hashes.SHA256()))
        expected = leaf.public_bytes(serialization.Encoding.DER)
        folder = Path(temporary); certificate = folder / "cert.pem"; key = folder / "key.pem"; trust = folder / "ca.pem"
        trust.write_bytes(pem("CERTIFICATE", authority)); certificate.write_bytes(leaf.public_bytes(serialization.Encoding.PEM))
        key.write_bytes(leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_3
        context.load_cert_chain(certificate, key); context.load_verify_locations(cafile=trust); context.verify_mode = ssl.CERT_REQUIRED
        listener = socket.socket(); listener.settimeout(0.25); listener.bind(("127.0.0.1", 0)); listener.listen(8)
        stop = threading.Event(); verified = []; failures = []
        def peer():
            try:
                while len(verified) < args.cycles and not stop.is_set():
                    try:
                        raw, _ = listener.accept()
                    except socket.timeout:
                        continue
                    raw.settimeout(10)
                    raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    with context.wrap_socket(raw, server_side=True) as connection:
                        assert connection.getpeercert(binary_form=True) == expected
                        assert connection.version() == "TLSv1.3"
                        with connection.makefile("rb") as stream:
                            for wanted in ((b"HELLO", b"3"), (b"PING",)):
                                line = stream.readline(128)
                                assert line == b"*" + str(len(wanted)).encode() + b"\r\n"
                                command = []
                                for value in wanted:
                                    assert stream.readline(128) == b"$" + str(len(value)).encode() + b"\r\n"
                                    command.append(stream.read(len(value)))
                                    assert stream.read(2) == b"\r\n"
                                assert tuple(command) == wanted
                                response = b"%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n" if wanted[0] == b"HELLO" else b"+PONG\r\n"
                                connection.sendall(response)
                            try:
                                assert not stream.read(1), "unexpected command after measured PING"
                            except (ssl.SSLEOFError, ConnectionResetError):
                                pass  # The library permits an authenticated-stream abort on client Close.
                        verified.append(True)
            except BaseException as error:
                failures.append(repr(error))
        worker = threading.Thread(target=peer); worker.start()
        try:
            result = subprocess.run([str(args.client.resolve(strict=True)), "--client", str(args.cycles), str(listener.getsockname()[1]),
                                     str(trust), str(certificate), str(key)], capture_output=True, text=True, encoding="utf-8", timeout=90)
            if result.returncode:
                raise RuntimeError("C auth benchmark failed: " + result.stderr[-3000:] + str(failures))
            worker.join(timeout=10)
            assert not worker.is_alive() and not failures and len(verified) == args.cycles, (failures, len(verified))
            print(result.stdout, end="")
            print("peer_verified:", len(verified))
            print("peer:", ssl.OPENSSL_VERSION, "/ cryptography", cryptography.__version__)
            print("measurement:", "sequential cold cycles; no steady-request/tail-latency or production acceptance")
        finally:
            stop.set(); worker.join(timeout=11); listener.close()
            if worker.is_alive():
                raise RuntimeError("fixture did not retire")


if __name__ == "__main__":
    main()
