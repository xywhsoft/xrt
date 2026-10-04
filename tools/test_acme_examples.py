"""Execute the registered public ACME example against the local TLS CA and DNS.

Checks a real issuance, account persistence, matching grant, cached renewal and
TXT removal; also checks that unconfirmed manual changes are never replayed.
No cloud credentials or external services are used.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

from http_tls_test_fixture import run_command
from test_acme_mock import MockServer, fetch_stats, read_stored_grant, verify_grant_pairing

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "extlibs/xacme/examples/obtain/main.c"


def build_client(directory: Path, compiler: str, single: bool, sanitize: bool) -> Path:
    if sanitize and not single:
        raise ValueError("--sanitize requires --single so every implementation is instrumented")
    if sanitize and os.name == "nt":
        raise ValueError("the example sanitizer gate requires POSIX")
    if not single:
        subprocess.run([sys.executable, str(ROOT / "tools/build.py"), "--compiler", compiler,
                        "--manifest", "extlibs/xacme/config/modules.json", "--suite", "xacme",
                        "--exclude-test", "*", "--no-single", "--no-run"], cwd=ROOT, check=True)
        binary = ROOT / "out" / Path(compiler).stem.lower() / "native/xacme" / (
                 "examples_obtain_main.exe" if os.name == "nt" else "examples_obtain_main")
        if not binary.is_file(): raise RuntimeError("the registered example was not built: " + str(binary))
        return binary
    binary = directory / ("acme_obtain.exe" if os.name == "nt" else "acme_obtain")
    libs = ["-lws2_32", "-lbcrypt", "-ladvapi32", "-liphlpapi"] if os.name == "nt" else ["-pthread", "-lm"]
    flags = ["-fno-omit-frame-pointer", "-fsanitize=address,undefined"] if sanitize else []
    run_command(compiler, "-std=c11", "-D_GNU_SOURCE", "-DACME_EXAMPLE_SINGLE", "-O2",
                "-Wall", "-Wextra", "-Werror", *flags,
                "-I", str(ROOT / "extlibs/xacme/single"), "-I", str(ROOT / "single"),
                str(SOURCE), "-o", str(binary), *libs, *flags)
    return binary


def exercise(binary: Path, work: Path) -> None:
    server = MockServer(work, [])
    process = None
    reader = None
    try:
        store = work / "store"
        arguments = [str(binary), server.info["directory"], server.info["ca_pem"],
                     "127.0.0.1:" + str(server.info["dns"]), str(store), "test.xxrpa.com"]
        process = subprocess.Popen(arguments, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        lines = queue.Queue()
        def read_output():
            for line in process.stdout: lines.put(line.rstrip("\n"))
            lines.put(None)
        reader = threading.Thread(target=read_output, daemon=True); reader.start()
        deadline = time.monotonic() + 180
        output = []; counts = {"ADD": 0, "REMOVE": 0}
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0: raise TimeoutError("example execution timed out")
            line = lines.get(timeout=remaining)
            if line is None: break
            output.append(line)
            print(line, flush=True)
            if line.startswith(("DNS_ADD ", "DNS_REMOVE ")):
                action, owner, value = line.split(" ")
                action = action.removeprefix("DNS_"); counts[action] += 1
                assert owner == "_acme-challenge.test.xxrpa.com"
                request = urllib.request.Request("http://127.0.0.1:" + str(server.info["chall"]) +
                          ("/set-txt" if action == "ADD" else "/clear-txt"),
                          data=json.dumps({"host": owner, "value": value}).encode(),
                          headers={"Content-Type": "application/json"})
                with urllib.request.urlopen(request, timeout=5) as response: assert response.status == 200
                process.stdin.write("yes\n"); process.stdin.flush()

        status = process.wait(timeout=5)
        diagnostics = process.stderr.read()
        assert status == 0, diagnostics
        assert "AddressSanitizer" not in diagnostics and "runtime error:" not in diagnostics, diagnostics
        assert counts == {"ADD": 1, "REMOVE": 1}, counts
        assert "CERT_READY renewed=1" in output
        assert "CACHE_READY account=stored grant=matched pending_dns=0" in output
        verify_grant_pairing(*read_stored_grant(store, "test.xxrpa.com"), "test.xxrpa.com",
                            Path(server.info["ca_pem"]).read_text(encoding="utf-8"))
        accounts = list((store / "accounts").glob("*/account.pem")); assert len(accounts) == 1
        if os.name != "nt":
            assert accounts[0].stat().st_mode & 0o777 == 0o600
            for key in (store / "certs").rglob("key.pem"): assert key.stat().st_mode & 0o777 == 0o600
        stats = fetch_stats(server)
        assert stats["accounts"] == stats["orders"] == 1, stats
        print("issued grant, stored account, cache and one TXT cleanup verified", flush=True)
        unconfirmed = subprocess.run([str(binary), server.info["directory"], server.info["ca_pem"],
                       "127.0.0.1:" + str(server.info["dns"]), str(work / "unconfirmed-store"), "other.xxrpa.com"],
                       input="", capture_output=True, text=True, timeout=180)
        assert unconfirmed.returncode == 1 and unconfirmed.stdout.count("DNS_ADD ") == 1, unconfirmed.stderr
        assert "DNS_REMOVE " not in unconfirmed.stdout and "pending_dns=1" in unconfirmed.stderr
        assert "manual TXT change was not confirmed" in unconfirmed.stderr
        assert "AddressSanitizer" not in unconfirmed.stderr and "runtime error:" not in unconfirmed.stderr, unconfirmed.stderr
        print("unconfirmed TXT retained without Add replay", flush=True)
        print("ACME registered public example execution passed", flush=True)
    finally:
        if process is not None:
            if process.poll() is None: process.kill(); process.wait(timeout=5)
            if reader is not None: reader.join(timeout=5)
            for stream in (process.stdin, process.stdout, process.stderr): stream.close()
        server.stop()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--single", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--client", type=Path, help="execute an already built registered example")
    args = parser.parse_args()
    if args.client and (args.single or args.sanitize):
        parser.error("--client cannot confirm --single or --sanitize instrumentation")
    with tempfile.TemporaryDirectory(prefix="acme-public-example-") as temp:
        work = Path(temp)
        binary = args.client.resolve() if args.client else build_client(work, args.compiler, args.single, args.sanitize)
        exercise(binary, work)


if __name__ == "__main__":
    main()
