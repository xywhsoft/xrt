"""Run the existing ACME client against independently built Pebble/challtestsrv.

No validation bypass, system CA modification or cloud account is used. Services
listen only on loopback and terminate in finally. See xacme README for usage.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import time
import urllib.error
import urllib.request

from cryptography import x509
from test_acme_mock import read_stored_grant, verify_grant_pairing


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def port(kind: int = socket.SOCK_STREAM) -> int:
    with socket.socket(socket.AF_INET, kind) as handle:
        handle.bind(("127.0.0.1", 0))
        return handle.getsockname()[1]


def fetch(url: str, context: ssl.SSLContext, data: dict | None = None) -> bytes:
    opener = urllib.request.build_opener(
        urllib.request.ProxyHandler({}), urllib.request.HTTPSHandler(context=context))
    request = urllib.request.Request(url, data=None if data is None else json.dumps(data).encode(),
                                     headers={"Content-Type": "application/json"})
    with opener.open(request, timeout=3) as response:
        return response.read()


def ready(url: str, context: ssl.SSLContext, processes: list[subprocess.Popen]) -> None:
    deadline = time.monotonic() + 25
    last = None
    while time.monotonic() < deadline:
        if any(process.poll() is not None for process in processes):
            raise RuntimeError("independent service exited before readiness")
        try:
            fetch(url, context)
            return
        except (OSError, urllib.error.URLError) as error:
            last = error
            time.sleep(0.1)
    raise RuntimeError(f"independent service did not become ready: {last}")


def stop(processes: list[subprocess.Popen]) -> list[int]:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
    result = []
    for process in reversed(processes):
        try:
            result.append(process.wait(timeout=10))
        except subprocess.TimeoutExpired:
            process.kill()
            result.append(process.wait(timeout=10))
    return result


def run_case(args: argparse.Namespace, name: str, report: dict) -> None:
    output = args.output / name
    output.mkdir(mode=0o700)
    service = args.runtime / "source"
    ca = service / "test/certs/pebble.minica.pem"
    cert = service / "test/certs/localhost/cert.pem"
    key = service / "test/certs/localhost/key.pem"
    context = ssl.create_default_context(cafile=str(ca))
    api, management, challenge, dns = port(), port(), port(), port(socket.SOCK_DGRAM)
    # DNS binds both TCP and UDP; ensure the selected UDP port is also free for TCP.
    with socket.socket() as handle:
        handle.bind(("127.0.0.1", dns))
    config = json.loads((service / "test/config/pebble-config.json").read_text())
    config["pebble"].update(listenAddress=f"127.0.0.1:{api}",
                           managementListenAddress=f"127.0.0.1:{management}",
                           certificate=str(cert), privateKey=str(key))
    # Without a requested profile, Pebble chooses a configured profile. The
    # stock configuration also has a six-day profile, which correctly renews
    # at our C fixture's 30-day threshold. This case checks retention instead.
    config["pebble"]["profiles"] = {
        "default": {"description": "90-day stored-certificate retention case",
                    "validityPeriod": 90 * 86400}}
    config_path = output / "pebble-config.json"
    config_path.write_text(json.dumps(config, indent=2) + "\n")
    platform = "windows" if os.name == "nt" else "linux"
    suffix = ".exe" if os.name == "nt" else ""
    pebble = args.runtime / "bin" / platform / ("pebble" + suffix)
    chall = args.runtime / "bin" / platform / ("pebble-challtestsrv" + suffix)
    env = {key: value for key, value in os.environ.items() if not key.startswith(("PEBBLE_", "XACME_"))}
    env.update(PEBBLE_VA_NOSLEEP="1", PEBBLE_WFE_NONCEREJECT="0",
               PEBBLE_AUTHZREUSE="100" if name == "authorization-reuse" else "0")
    if name == "alternate-chain":
        env["PEBBLE_ALTERNATE_ROOTS"] = "1"
    processes = []
    streams = []
    case = {"name": name, "all_passed": False, "actual_dns_validation": True,
            "strict": True, "validation_bypass": False,
            "service_config_sha256": digest(config_path), "server_environment": {
                key: value for key, value in env.items() if key.startswith("PEBBLE_")}}
    report["cases"].append(case)
    try:
        flags = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
        commands = [[str(chall), "-http01", "", "-https01", "", "-doh", "",
                     "-tlsalpn01", "", "-dnsserver", f"127.0.0.1:{dns}",
                     "-management", f"127.0.0.1:{challenge}"],
                    [str(pebble), "-config", str(config_path), "-strict=true",
                     "-dnsserver", f"127.0.0.1:{dns}"]]
        for label, command in zip(("challenge", "pebble"), commands):
            stream = (output / (label + ".log")).open("xb")
            streams.append(stream)
            processes.append(subprocess.Popen(command, cwd=service, env=env,
                                               stdout=stream, stderr=subprocess.STDOUT, **flags))
        directory = f"https://localhost:{api}/dir"
        root_url = f"https://localhost:{management}/roots/0"
        ready(directory, context, processes)
        ready(root_url, context, processes)
        roots = [fetch(root_url, context).decode()]
        if name == "alternate-chain":
            roots.append(fetch(f"https://localhost:{management}/roots/1", context).decode())
        for index, pem in enumerate(roots):
            (output / f"issued-ca-{index}.pem").write_text(pem)
        flow_env = {**env, "XACME_PEBBLE_URL": directory,
                    "XACME_PEBBLE_CA": str(ca), "XACME_CHALL_URL": f"http://127.0.0.1:{challenge}",
                    "XACME_PROPAGATE_RESOLVER": f"127.0.0.1:{dns}", "XACME_TEST_ROOT": str(output)}
        if name == "alternate-chain":
            flow_env["XACME_PREFER_ALT"] = "1"
        log = output / "client.log"
        with log.open("xb") as stream:
            execution = subprocess.run([str(args.binary)], cwd=output, env=flow_env,
                                       stdout=stream, stderr=subprocess.STDOUT, timeout=180, **flags)
        case.update(client_exit_code=execution.returncode, client_log_sha256=digest(log))
        content = log.read_text(errors="replace")
        print(content[-1500:], flush=True)
        if execution.returncode != 0 or "[SKIP]" in content or "[PASS] acme flow pebble issuance" not in content:
            raise RuntimeError(f"ACME public flow failed against {name}: exit {execution.returncode}")
        chain = (output / "pebble_issued.pem").read_text()
        private = (output / "pebble_issued.key.pem").read_text()
        # Preference must actually switch the issuing chain. Trusting both
        # roots would let a silently ignored alternate preference pass.
        selected_root = 1 if name == "alternate-chain" else 0
        verify_grant_pairing(chain, private, "test.xxrpa.com", roots[selected_root])
        leaf = x509.load_pem_x509_certificates(chain.encode())[0]
        status = json.loads(fetch(f"https://localhost:{management}/cert-status-by-serial/{leaf.serial_number:x}", context))
        if status["Status"] != "Revoked" or status["Reason"] != 1:
            raise RuntimeError("independent CA did not record the requested revocation")
        for store in ("store_pebble", "store_obtain"):
            chain, private = read_stored_grant(output / store, "test.xxrpa.com")
            verify_grant_pairing(chain, private, "test.xxrpa.com", roots[0])
        history = json.loads(fetch(f"http://127.0.0.1:{challenge}/dns-request-history", context,
                                   {"host": "_acme-challenge.test.xxrpa.com"}))
        if not any(event["Question"]["Qtype"] == 16 for event in history):
            raise RuntimeError("independent challenge service recorded no TXT validation")
        case.update(all_passed=True, independent_chain_key_san_verified=True,
                    manual_chain_root_index=selected_root, stored_chain_root_index=0,
                    independent_revocation_verified=True, independent_stored_grants_verified=2,
                    dns_txt_requests=sum(event["Question"]["Qtype"] == 16 for event in history))
    finally:
        case["service_exit_codes"] = stop(processes)
        case["all_services_terminal"] = all(process.poll() is not None for process in processes)
        for stream in streams:
            stream.close()
        case["file_sha256"] = {p.relative_to(output).as_posix(): digest(p)
                                for p in output.rglob("*") if p.is_file()}
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True,
                        help="prepared upstream source/, bin/<platform>/ and runtime.json")
    parser.add_argument("--binary", type=Path, required=True, help="noncoverage test_flow binary")
    parser.add_argument("--output", type=Path, required=True, help="new isolated evidence directory")
    parser.add_argument("--case", choices=("baseline", "authorization-reuse", "alternate-chain"), action="append")
    args = parser.parse_args()
    args.runtime = args.runtime.resolve(); args.binary = args.binary.resolve(); args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    runtime = json.loads((args.runtime / "runtime.json").read_text())
    if not runtime.get("all_passed") or not runtime.get("source_sha256") or not runtime.get("binary_sha256"):
        raise RuntimeError("runtime has no successful source and binary build evidence")
    def verify_runtime() -> None:
        for name, value in runtime["source_sha256"].items():
            if digest(args.runtime / "source" / name) != value:
                raise RuntimeError(f"upstream source changed: {name}")
        for name, value in runtime["binary_sha256"].items():
            if digest(args.runtime / name) != value:
                raise RuntimeError(f"upstream binary changed: {name}")
    verify_runtime()
    report = {"platform": "windows" if os.name == "nt" else "linux", "upstream_commit": runtime["source_commit"],
              "runtime_metadata_sha256": digest(args.runtime / "runtime.json"), "client_binary_sha256": digest(args.binary),
              "runner_sha256": digest(Path(__file__)), "cases": [], "all_passed": False,
              "cloud_provider_live_test": False, "global_system_ca_store_modified": False}
    for name in args.case or ["baseline", "authorization-reuse", "alternate-chain"]:
        print(f"[independent Pebble] {name}", flush=True)
        run_case(args, name, report)
    verify_runtime()
    if digest(args.binary) != report["client_binary_sha256"]:
        raise RuntimeError("client binary changed during verification")
    report.update(all_passed=True, runtime_before_after_verified=True)
    (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print("[verified] independent Pebble DNS validation, issuance, storage, rollover, deactivation and revocation", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
