#!/usr/bin/env python3
"""Opt-in initial mutual TLS against a disposable real Redis, with ephemeral keys."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
from run_redis_init import digest, run
from run_redis_tls import certificates

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from test_protocol_fuzz import _closure


def input_hashes(compile_inputs=()) -> dict[str, str]:
    inputs = set(compile_inputs)
    if not inputs:
        for module in ("xredis_tls_auth", "tls_identity_builtin", "pem"):
            inputs.update(ROOT / p for p in _closure(module, "extlibs/xredis/config/modules.json")[0])
    inputs.update((ROOT / "include").rglob("*.h")); inputs.update((ROOT / "src").rglob("*.h"))
    inputs.update((ROOT / "extlibs/xredis/include").rglob("*.h"))
    inputs.update((ROOT / "extlibs/xredis/src").rglob("*.h"))
    inputs.update((ROOT / "extlibs/xredis/tests/interop").glob("*.py"))
    inputs.update((ROOT / "extlibs/xredis/tests/interop").glob("*.c"))
    inputs.update((ROOT / "extlibs/xredis/tests/interop").glob("*.h"))
    inputs.update(ROOT / path for path in ("config/modules.json", "extlibs/xredis/config/modules.json", "tools/test_protocol_fuzz.py", "tools/xrt_manifest.py",
        "extlibs/xredis/tests/interop/redis_init.c", "extlibs/xredis/tests/interop/redis_tls_auth.c", "extlibs/xredis/tests/interop/tls_auth_support.h",
        "extlibs/xredis/examples/tls_auth/material.h"))
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(inputs)}


def fingerprint(hashes):
    return hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()


def build(compiler: str, output: Path, transaction: bool = False, watch: bool = False, pool: bool = False, pool_sessions: bool = False, pool_connect: bool = False) -> tuple[list[Path], list[str]]:
    modules = ("xredis_tls_auth", "tls_identity_builtin", "pem") + (("xredis_watch",) if watch else ("xredis_transaction",) if transaction else ()) + (("xredis_pool",) if pool else ()) + (("xredis_pool_transaction", "xredis_pool_watch") if pool_sessions else ())
    modules += ("xredis_pool_connect",) if pool_connect else ()
    closures = [_closure(module, "extlibs/xredis/config/modules.json")
                for module in modules]
    sources, defines, links, includes = [list(dict.fromkeys(item for c in closures for item in c[i])) for i in range(4)]
    inputs = [ROOT / source for source in sources]
    command = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", *["-D" + d for d in defines],
               "-I", str(ROOT / "include")]
    for include in includes:
        command += ["-I", str(ROOT / include)]
    if sys.platform != "win32":
        command += ["-pthread"]
    command += [*map(str, inputs), str(Path(__file__).with_name("redis_pool_connect.c" if pool_connect else "redis_pool_sessions.c" if pool_sessions else "redis_pool.c" if pool else "redis_watch.c" if watch else "redis_transaction.c" if transaction else "redis_tls_auth.c")),
                *["-l" + link for link in links], "-o", str(output)]
    before = input_hashes(inputs)
    subprocess.run(command, cwd=ROOT, check=True, timeout=300)
    assert before == input_hashes(inputs), "inputs changed while compiling the interop driver"
    compiler_path = Path(shutil.which(compiler)).resolve(strict=True)
    receipt = {"schema": 1, "platform": sys.platform, "transaction": transaction, "watch": watch, "pool": pool, "pool_sessions": pool_sessions, "pool_connect": pool_connect, "compiler": str(compiler_path), "compiler_sha256": digest(compiler_path),
        "build_python": sys.executable, "build_python_sha256": digest(Path(sys.executable)),
        "compiler_version": subprocess.check_output([compiler, "--version"], text=True, encoding="utf-8").splitlines()[0],
        "command": command, "cwd": str(ROOT), "defines": defines, "client_sha256": digest(output),
        "inputs": before, "source_fingerprint": fingerprint(before)}
    output.with_name(output.name + ".build.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return inputs, defines


def client_certificates(openssl: Path, folder: Path) -> dict:
    def command(*args: str):
        subprocess.run([str(openssl), *args], cwd=folder, check=True, capture_output=True, timeout=30)
    command("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-sha256", "-days", "2", "-subj", "/CN=xredis-client-fixture-CA",
            "-keyout", "client-ca.key", "-out", "client-ca.pem", "-addext", "basicConstraints=critical,CA:TRUE",
            "-addext", "keyUsage=critical,keyCertSign,cRLSign")
    (folder / "client.ext").write_text("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=clientAuth\n", encoding="utf-8")
    for name in ("rsa", "p256", "p384", "ed25519", "bad-rsa"):
        key = ["-newkey", "rsa:2048"] if name in ("rsa", "bad-rsa") else ["-newkey", "ed25519"] if name == "ed25519" else (
            ["-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:" + ("prime256v1" if name == "p256" else "secp384r1")])
        command("req", *key, "-nodes", "-subj", "/CN=xredis-client-fixture-" + name, "-keyout", name + ".key", "-out", name + ".csr")
        ca = "wrong" if name == "bad-rsa" else "client-ca"
        command("x509", "-req", "-in", name + ".csr", "-CA", ca + ".pem", "-CAkey", ca + ".key", "-CAcreateserial",
                "-days", "2", "-sha256", "-extfile", "client.ext", "-out", name + ".pem")
        (folder / (name + "-chain.pem")).write_bytes((folder / (name + ".pem")).read_bytes() + (folder / (ca + ".pem")).read_bytes())
    return {p.name: digest(p) for p in folder.glob("*.pem")}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--transaction", action="store_true", help="also verify atomic transactions over the authenticated connection")
    parser.add_argument("--pool-connect", action="store_true", help="also verify automatic setup and repair (implies --pool-sessions)")
    parser.add_argument("--pool-sessions", action="store_true", help="also verify native leased transactions and nested WATCH handoff (implies --pool)")
    parser.add_argument("--pool", action="store_true", help="also verify exclusive pooled leases and real blocking commands (implies --watch)")
    parser.add_argument("--watch", action="store_true", help="also verify exclusive WATCH scopes (implies --transaction)")
    parser.add_argument("--key", choices=("rsa", "p256", "p384", "ed25519"), help="select one client key; default covers all four")
    parser.add_argument("--server", type=Path, default=shutil.which("redis-server"))
    parser.add_argument("--openssl", type=Path, default=shutil.which("openssl"))
    parser.add_argument("--example", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.pool_connect:
        args.pool_sessions = True
    if args.pool_sessions:
        args.pool = True
    if args.pool:
        args.watch = True
    if args.watch:
        args.transaction = True
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({"schema": 1, "status": "incomplete", "scope": "no successful result recorded yet"}) + "\n", encoding="utf-8")
    output = ROOT / ("out/redis-tls-pool-connect" if args.pool_connect else "out/redis-tls-pool-sessions" if args.pool_sessions else "out/redis-tls-pool" if args.pool else "out/redis-tls-watch" if args.watch else "out/redis-tls-transaction" if args.transaction else "out/redis-tls-auth") / sys.platform; output.mkdir(parents=True, exist_ok=True)
    client = args.client or output / ("client.exe" if sys.platform == "win32" else "client")
    compiler = shutil.which(args.compiler)
    if args.build and compiler is None:
        parser.error("C compiler not found")
    compile_inputs, defines = build(compiler, client, args.transaction, args.watch, args.pool, args.pool_sessions, args.pool_connect) if args.build else ([], [])
    if args.build_only:
        if not args.build:
            parser.error("--build-only requires --build")
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_bytes(client.with_name(client.name + ".build.json").read_bytes())
        print("[PASS] interop client and exact build receipt created")
        return
    if args.server is None or args.openssl is None:
        parser.error("local Redis TLS and OpenSSL are required; use --client for an already-built native Windows client from WSL")
    client = client.resolve(strict=True); server = args.server.absolute(); openssl = args.openssl.absolute()
    receipt_path = client.with_name(client.name + ".build.json")
    if not receipt_path.is_file():
        parser.error("client requires its exact build receipt; use --build or --build-only first")
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    assert receipt.get("pool_connect", False) == args.pool_connect, "client receipt does not match requested automatic pool mode"
    assert receipt.get("pool_sessions", False) == args.pool_sessions, "client receipt does not match requested pooled sessions mode"
    assert receipt.get("pool", False) == args.pool, "client receipt does not match requested pool mode"
    assert receipt.get("watch", False) == args.watch, "client receipt does not match requested WATCH mode"
    assert receipt.get("transaction", False) == args.transaction, "client receipt does not match requested interop mode"
    # Revalidate the actual compiled closure, including its platform backend.
    # A native Windows binary must not be compared to a WSL/epoll closure.
    receipt_inputs = [ROOT / path for path in receipt["inputs"]]
    hashes_before = input_hashes(receipt_inputs)
    assert receipt["client_sha256"] == digest(client), "prebuilt client does not match its receipt"
    assert receipt["inputs"] == hashes_before, "prebuilt client source receipt is stale"
    windows_client = client.suffix.lower() == ".exe" and sys.platform != "win32"
    def client_path(path: Path) -> str:
        return subprocess.check_output(["wslpath", "-w", str(path)], text=True, encoding="utf-8").strip() if windows_client else str(path)
    health_python = None
    if windows_client:
        health_python = Path(subprocess.check_output(["wslpath", "-u", receipt["build_python"]], text=True, encoding="utf-8").strip())
        assert digest(health_python) == receipt["build_python_sha256"], "native health interpreter no longer matches build receipt"
    cases, example_cases = [], []
    with tempfile.TemporaryDirectory(prefix="certs-", dir=output) as temporary:
        folder = Path(temporary); public = certificates(openssl, folder); public.update(client_certificates(openssl, folder))
        for version in (12, 13):
            for algorithm in (("rsa",) if args.quick else (args.key,) if args.key else ("rsa", "p256", "p384", "ed25519")):
                for scenario in ((0,) if args.quick else range(3)):
                    print(f"[CASE] TLS 1.{version - 10} key={algorithm} scenario={scenario}", flush=True)
                    arguments = ["{port}", str(scenario), client_path(folder / "ca.pem"), client_path(folder / "wrong.pem"), str(version),
                                 client_path(folder / (algorithm + "-chain.pem")), client_path(folder / (algorithm + ".key")),
                                 client_path(folder / "bad-rsa-chain.pem"), client_path(folder / "bad-rsa.key")]
                    tls = {"certificate": str(folder / "server.pem"), "key": str(folder / "server.key"), "ca": str(folder / "client-ca.pem"),
                           "health_ca": str(folder / "ca.pem"), "health_cert": str(folder / "rsa.pem"), "health_key": str(folder / "rsa.key"),
                           "client_auth": "yes", "protocol": "TLSv1." + str(version - 10)}
                    health_command = None if health_python is None else [str(health_python),
                        client_path(Path(__file__).with_name("health_redis_tls.py")), "{port}",
                        client_path(folder / "ca.pem"), client_path(folder / "rsa.pem"), client_path(folder / "rsa.key")]
                    case = run(client, server, scenario, arguments, tls, health_command); case["client_key"] = algorithm
                    cases.append(case)
            if args.example:
                example = args.example.resolve(strict=True)
                # Path conversion is selected by the actual example too.
                example_windows = example.suffix.lower() == ".exe" and sys.platform != "win32"
                def example_path(path):
                    return subprocess.check_output(["wslpath", "-w", str(path)], text=True, encoding="utf-8").strip() if example_windows else str(path)
                example_cases.append(run(example, server, 0, ["localhost", "{port}", example_path(folder / "ca.pem"),
                    example_path(folder / "rsa-chain.pem"), example_path(folder / "rsa.key")], tls, health_command))
    hashes = input_hashes(receipt_inputs)
    assert hashes == hashes_before, "source/header/helper inputs changed while exercising the client"
    assert digest(client) == receipt["client_sha256"], "client changed while exercising it"
    report = {"schema": 1, "status": "passed", "transaction": args.transaction, "watch": args.watch, "pool": args.pool, "pool_sessions": args.pool_sessions, "pool_connect": args.pool_connect,
        "scope": "local certificate handshakes and optional transactions, WATCH scopes, pooled leases, nested sessions and automatic repair; production/performance and identity-bound resumption pending",
        "platform": platform.platform(), "client_platform": "native Windows client via WSL peer" if windows_client else sys.platform,
        "server_path": str(server), "server_sha256": digest(server), "server_version": subprocess.check_output([str(server), "--version"], text=True, encoding="utf-8").strip(),
        "openssl_sha256": digest(openssl), "openssl_version": subprocess.check_output([str(openssl), "version"], text=True, encoding="utf-8").strip(),
        "client_sha256": digest(client), "built_this_run": args.build, "build_receipt_verified": receipt is not None,
        "build_receipt_sha256": digest(receipt_path), "compile_defines": receipt["defines"], "compiler": receipt["compiler"],
        "compiler_version": receipt["compiler_version"], "compiler_sha256": receipt["compiler_sha256"],
        "native_windows_health": health_python is not None, "public_certificates": public, "cases": cases, "example_cases": example_cases,
        "source_fingerprint": fingerprint(hashes), "inputs": hashes,
        "example_sha256": None if args.example is None else digest(args.example.resolve(strict=True))}
    args.report.parent.mkdir(parents=True, exist_ok=True); args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] real Redis client certificates: {len(cases)} key/version/credential cases; {len(example_cases)} examples")


if __name__ == "__main__":
    main()
