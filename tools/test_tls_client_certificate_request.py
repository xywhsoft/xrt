#!/usr/bin/env python3
"""Opt-in native-client/OpenSSL CertificateRequest matrix; ephemeral local keys only."""
from __future__ import annotations

import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import platform
import shutil
import ssl
import sys
import tempfile

from test_tls_interop import ROOT, MODULES, build, configure, exchange, fixtures
from test_protocol_fuzz import _closure


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    output = ROOT / "out/tls-client-certificate-request" / sys.platform
    output.mkdir(parents=True, exist_ok=True)
    library = output / ("peer.dll" if sys.platform == "win32" else "peer.so")
    compiler = shutil.which(args.compiler)
    if compiler is None:
        parser.error("C compiler not found: " + args.compiler)
    if not args.no_build:
        build(compiler, library)
    dll = c.CDLL(str(library))
    configure(dll)
    inputs = {ROOT / path for module in MODULES for path in _closure(module)[0]}
    inputs.update((ROOT / "include").rglob("*.h"))
    inputs.update((ROOT / "src/internal").glob("*.h"))
    inputs.update(ROOT / path for path in ("tools/test_tls_interop.py", "tools/probes/tls_peer.c",
                                         "tools/test_protocol_fuzz.py", "config/modules.json"))
    inputs.add(Path(__file__).resolve())
    fingerprints = {path.relative_to(ROOT).as_posix(): digest(path) for path in sorted(inputs)}
    cases = []
    with tempfile.TemporaryDirectory(prefix="certs-", dir=output) as temporary:
        root, root_pem, identities = fixtures(Path(temporary))
        public_hashes = {"root": hashlib.sha256(root).hexdigest(),
                         "server": hashlib.sha256(identities["rsa"][2]).hexdigest()}
        for version in (12, 13):
            for cipher in ((0x1301, 0x1302, 0x1303) if version == 13 else (0xC02F, 0xC030, 0xCCA8)):
                for auth in ("optional", "required"):
                    result = exchange(dll, root, root_pem, identities["rsa"], "rsa", version,
                                      cipher, False, client_auth=auth)
                    cases.append({"tls_version": version, "cipher": cipher, "client_auth": auth, **result})
                    print(f"[PASS] TLS 1.{version - 10} cipher={cipher:04x} client-certificate={auth}", flush=True)
        for auth in ("optional", "required"):
            result = exchange(dll, root, root_pem, identities["rsa"], "rsa", 13, 0x1301, False,
                              hrr=True, client_auth=auth)
            cases.append({"tls_version": 13, "cipher": 0x1301, "hrr": True, "client_auth": auth, **result})
            print(f"[PASS] TLS 1.3 HRR client-certificate={auth}", flush=True)
    report = {"schema": 1, "purpose": "local empty-certificate interoperability; client signing and production acceptance pending",
              "platform": platform.platform(), "python": sys.version, "openssl": ssl.OPENSSL_VERSION,
              "compiler": compiler, "library_sha256": digest(library), "built_this_run": not args.no_build,
              "source_fingerprint": hashlib.sha256(json.dumps(fingerprints, sort_keys=True).encode()).hexdigest(),
              "inputs": fingerprints, "public_certificate_hashes": public_hashes, "cases": cases}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] native-client/OpenSSL CertificateRequest: {len(cases)} cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
