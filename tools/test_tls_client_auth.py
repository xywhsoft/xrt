#!/usr/bin/env python3
"""Native TLS client certificate proof against an independent OpenSSL verifier."""
from __future__ import annotations
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import platform
import shutil
import ssl
import subprocess
import sys
import tempfile

from test_tls_interop import ROOT, MODULES, build, configure, exchange, fixtures
from test_protocol_fuzz import _closure


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--regression", action="store_true", help="also verify both legacy roles and empty client certificates using the same library")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if compiler is None:
        parser.error("C compiler not found")
    output = ROOT / "out/tls-client-auth" / sys.platform
    output.mkdir(parents=True, exist_ok=True)
    library = output / ("peer.dll" if sys.platform == "win32" else "peer.so")
    if not args.no_build:
        build(compiler, library)
    dll = c.CDLL(str(library))
    configure(dll)
    cases, regressions = [], []
    with tempfile.TemporaryDirectory(prefix="certs-", dir=output) as temporary:
        folder = Path(temporary)
        for name in ("server", "client", "large"):
            (folder / name).mkdir()
        root, root_pem, servers = fixtures(folder / "server")
        client_root, client_pem, clients = fixtures(folder / "client", client_auth=True)
        large_root, large_pem, large = fixtures(folder / "large", client_auth=True, padding=20000)
        public = {"server_ca": hashlib.sha256(root).hexdigest(),
                  "client_ca": hashlib.sha256(client_root).hexdigest(),
                  "large_ca": hashlib.sha256(large_root).hexdigest()}
        for version in (12, 13):
            for algorithm in (("rsa",) if args.quick else ("rsa", "p256", "p384", "ed25519")):
                ciphers = ((0x1301,) if args.quick else (0x1301, 0x1302, 0x1303)) if version == 13 else (
                    (0xC02F,) if args.quick else (0xC02F, 0xC030, 0xCCA8))
                for cipher in ciphers:
                    result = exchange(dll, root, root_pem, servers["rsa"], "rsa", version, cipher, False,
                                      client_auth="required", local_identity=(algorithm, clients[algorithm]),
                                      client_root_pem=client_pem)
                    cases.append({"version": version, "client_key": algorithm, "cipher": cipher, **result})
                    print(f"[PASS] client {algorithm} TLS 1.{version-10} cipher={cipher:04x} proof/ALPN/data/close", flush=True)
            if not args.quick:
                cipher = 0x1301 if version == 13 else 0xC02F
                for require in (False, True):
                    result = exchange(dll, root, root_pem, servers["rsa"], "rsa", version, cipher, False,
                                      local_identity=("rsa", clients["rsa"]), local_require=require,
                                      expect_local_refusal=require)
                    cases.append({"version": version, "no_request": True, "local_require": require, **result})
                result = exchange(dll, root, root_pem, servers["rsa"], "rsa", version, cipher, False,
                                  client_auth="required", local_identity=("rsa", large["rsa"]),
                                  client_root_pem=large_pem)
                cases.append({"version": version, "large_certificate": True, **result})
                result = exchange(dll, root, root_pem, servers["rsa"], "rsa", version, cipher, False,
                                  client_auth="required", local_identity=("rsa", clients["rsa"]),
                                  client_root_pem=client_pem, bad_name=True)
                cases.append({"version": version, "bad_server_name": True, **result})
        if not args.quick:
            for algorithm in ("rsa", "p256"):
                result = exchange(dll, root, root_pem, servers["rsa"], "rsa", 13, 0x1301, False, hrr=True,
                                  client_auth="required", local_identity=(algorithm, clients[algorithm]),
                                  client_root_pem=client_pem)
                cases.append({"version": 13, "client_key": algorithm, "hrr": True, **result})
        if args.regression:
            for algorithm, fixture in servers.items():
                for version in (12, 13):
                    if version == 12 and algorithm == "ed25519":
                        continue
                    ciphers = (0x1301, 0x1302, 0x1303) if version == 13 else (
                        (0xC02F, 0xC030, 0xCCA8) if algorithm == "rsa" else (0xC02B, 0xC02C, 0xCCA9))
                    for cipher in ciphers:
                        for server in (False, True):
                            result = exchange(dll, root, root_pem, fixture, algorithm, version, cipher, server)
                            regressions.append({"kind": "legacy", "version": version, "key": algorithm,
                                                "cipher": cipher, "xrt_server": server, **result})
            for server in (False, True):
                for bad_name in (False, True):
                    result = exchange(dll, root, root_pem, servers["rsa"], "rsa", 13, 0x1301,
                                      server, hrr=not bad_name, bad_name=bad_name)
                    regressions.append({"kind": "legacy_name_or_retry", "xrt_server": server, "bad_name": bad_name,
                                        **(result or {"result": "server_name_rejected"})})
            for version in (12, 13):
                for cipher in ((0x1301, 0x1302, 0x1303) if version == 13 else (0xC02F, 0xC030, 0xCCA8)):
                    for request in ("optional", "required"):
                        result = exchange(dll, root, root_pem, servers["rsa"], "rsa", version, cipher, False, client_auth=request)
                        regressions.append({"kind": "no_identity", "version": version, "cipher": cipher, "request": request, **result})
            for request in ("optional", "required"):
                result = exchange(dll, root, root_pem, servers["rsa"], "rsa", 13, 0x1301, False, hrr=True, client_auth=request)
                regressions.append({"kind": "no_identity_retry", "request": request, **result})
    paths = {ROOT / path for module in MODULES for path in _closure(module)[0]}
    paths.update((ROOT / "include").rglob("*.h"))
    paths.update((ROOT / "src/internal").glob("*.h"))
    paths.update(ROOT / path for path in ("tools/probes/tls_peer.c", "tools/test_tls_interop.py",
                                        "tools/test_protocol_fuzz.py", "tools/xrt_manifest.py", "config/modules.json"))
    paths.add(Path(__file__).resolve())
    hashes = {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(paths)}
    report = {"schema": 1, "scope": "local full certificate handshakes; resumed identity binding and production acceptance pending",
              "platform": platform.platform(), "python": sys.version, "openssl": ssl.OPENSSL_VERSION,
              "compiler": compiler, "compiler_version": subprocess.check_output([compiler, "--version"], text=True, encoding="utf-8").splitlines()[0],
              "compiler_sha256": digest(Path(compiler)), "library_sha256": digest(library), "built_this_run": not args.no_build,
              "source_fingerprint": hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest(),
              "inputs": hashes, "public_certificate_hashes": public, "cases": cases,
              "regressions": regressions, "quick": args.quick}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] native client certificate authentication: {len(cases)} cases; {len(regressions)} independent-peer regressions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
