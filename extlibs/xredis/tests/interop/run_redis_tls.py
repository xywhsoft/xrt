#!/usr/bin/env python3
"""Opt-in TLS 1.2/1.3 real Redis matrix; all keys/services are ephemeral fixtures."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
from run_redis_init import digest, run


def certificates(openssl: Path, root: Path) -> dict[str, str]:
    def command(*arguments: str) -> None:
        subprocess.run([str(openssl), *arguments], cwd=root, check=True, capture_output=True, timeout=30)
    for name in ("ca", "wrong"):
        command("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-sha256", "-days", "2",
                "-subj", f"/CN=xredis-fixture-{name}", "-keyout", f"{name}.key", "-out", f"{name}.pem",
                "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign")
    command("req", "-newkey", "rsa:2048", "-nodes", "-sha256", "-subj", "/CN=localhost",
            "-keyout", "server.key", "-out", "server.csr")
    (root / "server.ext").write_text("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\n"
                                     "extendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost,IP:127.0.0.1\n", encoding="utf-8")
    command("x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key", "-CAcreateserial",
            "-days", "2", "-sha256", "-extfile", "server.ext", "-out", "server.pem")
    return {name: digest(root / name) for name in ("ca.pem", "wrong.pem", "server.pem")}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, default=shutil.which("redis-server"))
    parser.add_argument("--openssl", type=Path, default=shutil.which("openssl"))
    parser.add_argument("--example", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.server is None or args.openssl is None:
        parser.error("Redis TLS server and openssl are required")
    server, openssl = args.server.absolute(), args.openssl.absolute()  # Preserve argv[0] multiplexer names.
    client = args.client.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="xredis-tls-identity-") as temporary:
        root = Path(temporary)
        public_certificates = certificates(openssl, root)
        cases = []
        example_cases = []
        for version in (12, 13):
            for scenario in range(3):
                cases.append(run(client, server, scenario,
                                 ["{port}", str(scenario), str(root / "ca.pem"), str(root / "wrong.pem"), str(version)],
                                 {"certificate": str(root / "server.pem"), "key": str(root / "server.key"),
                                  "ca": str(root / "ca.pem"), "protocol": "TLSv1." + str(version - 10)}))
            if args.example is not None:
                example_cases.append(run(args.example.resolve(strict=True), server, 0,
                                         ["localhost", "{port}", str(root / "ca.pem")],
                                         {"certificate": str(root / "server.pem"), "key": str(root / "server.key"),
                                          "ca": str(root / "ca.pem"), "protocol": "TLSv1." + str(version - 10)}))
    report = {"schema": 1, "purpose": "local verified TLS interoperability; not production/performance or mTLS acceptance",
              "platform": platform.platform(), "server_path": str(server), "server_resolved_path": str(server.resolve(strict=True)),
              "server_sha256": digest(server), "server_version": subprocess.check_output([str(server), "--version"], text=True, encoding="utf-8").strip(),
              "openssl_sha256": digest(openssl), "openssl_version": subprocess.check_output([str(openssl), "version"], text=True, encoding="utf-8").strip(),
              "client_sha256": digest(client), "client_source_sha256": digest(Path(__file__).with_name("redis_init.c")),
              "runner_sha256": digest(Path(__file__)), "service_helper_sha256": digest(Path(__file__).with_name("run_redis_init.py")),
              "public_certificate_hashes": public_certificates, "cases": cases, "example_cases": example_cases,
              "example_sha256": None if args.example is None else digest(args.example.resolve(strict=True))}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("[PASS] real Redis verified TLS 1.2/1.3: no-auth/password/ACL plus wrong name and wrong trust")


if __name__ == "__main__":
    main()
