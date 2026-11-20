"""Kill one owned loopback Redis after BLPOP is on the wire, then start a new one."""
import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import platform
import queue
import random
import shutil
import socket
import ssl
import subprocess
import tempfile
import threading
import time
import traceback

from run_redis_role import exchange, frame, send, sha
from run_redis_tls import certificates, issue_server_certificate


TLS_CIPHER = "TLS_AES_128_GCM_SHA256"


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def tls_listener(root, certificate, issuer):
    context = ssl.create_default_context(cafile=str(root / f"{issuer}.pem"))
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_3
    leaf = ssl.PEM_cert_to_DER_cert((root / f"{certificate}.pem").read_text(encoding="ascii"))
    options = ["--port", "0", "--tls-cert-file", str(root / f"{certificate}.pem"),
               "--tls-key-file", str(root / f"{certificate}.key"), "--tls-ca-cert-file", str(root / f"{issuer}.pem"),
               "--tls-protocols", "TLSv1.3", "--tls-ciphersuites", TLS_CIPHER, "--tls-auth-clients", "no"]
    return context, leaf, options


def info(port, section, tls=None):
    if tls is None:
        value, _ = exchange(port, ["INFO", section])
    else:
        context, leaf, _ = tls
        with socket.create_connection(("127.0.0.1", port), timeout=2) as raw:
            with context.wrap_socket(raw, server_hostname="localhost") as peer, peer.makefile("rb") as stream:
                require(peer.version() == "TLSv1.3" and peer.cipher()[0] == TLS_CIPHER and
                        peer.getpeercert(binary_form=True) == leaf, "unexpected Redis TLS identity or profile")
                send(peer, ["INFO", section])
                value, _ = frame(stream)
    return dict(row.split(":", 1) for row in value.decode().splitlines() if ":" in row)


def command_calls(fields):
    return {key.removeprefix("cmdstat_"): int(value.split(",", 1)[0].removeprefix("calls="))
            for key, value in fields.items() if key.startswith("cmdstat_")}


def check_commands(calls, expected, stage):
    # INFO traffic consists only of the independent observer's requests.
    require({key: value for key, value in calls.items() if key != "info"} == expected,
            f"unexpected {stage} command ledger {calls}")


def fixture_port(candidates):
    for port in candidates:
        require(0 < port < 65536, f"invalid port {port}")
        try:
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", port))
            return port
        except OSError:
            continue
    raise RuntimeError("no private fixture port available")


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def start(stack, server, runtime, port, label, log_path, tls=None):
    directory = runtime / label
    directory.mkdir()
    log = stack.enter_context(log_path.open("wb"))
    command = [str(server), "--bind", "127.0.0.1", "--save", "", "--appendonly", "no", "--dir", str(directory)]
    command.extend(["--port", str(port)] if tls is None else ["--tls-port", str(port), *tls[2]])
    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
    stack.callback(stop, process)
    due = time.monotonic() + 10
    while True:
        require(process.poll() is None, f"{label} Redis exited with {process.returncode}")
        try:
            fields = info(port, "server", tls)
        except OSError:
            if time.monotonic() >= due:
                raise
            time.sleep(.02)
            continue
        require(int(fields["process_id"]) == process.pid, f"{label} port served by another process")
        identity = dict(pid=process.pid, run_id=fields["run_id"])
        if tls is not None:
            identity["certificate_sha256"] = hashlib.sha256(tls[1]).hexdigest()
        return process, identity


def read_phases(stream, lines):
    try:
        for line in stream:
            lines.put(line)
    finally:
        lines.put(None)


def wait_phase(lines, phases, name):
    line = lines.get(timeout=20)
    require(line is not None, f"client exited before {name}, see client log")
    phase = json.loads(line)
    phases.append(phase)
    require(phase["phase"] == name, f"expected {name}, received {phase}")
    return phase


def run(args, report, stack):
    candidates = args.port_candidate or random.sample(range(12000, 45000), 64)
    port = fixture_port(candidates)
    report["port"] = port
    runtime = Path(stack.enter_context(tempfile.TemporaryDirectory(prefix="xredis-restart-")))
    prefix = args.report.with_suffix("")
    def log_path(label):
        return prefix.with_name(f"{prefix.name}-{label}.log")

    before_tls = rejected_tls = after_tls = None
    client_args = [str(args.client), str(port)]
    if args.tls:
        material = certificates(args.openssl, runtime)
        material["untrusted.pem"] = issue_server_certificate(args.openssl, runtime, "untrusted", "wrong")
        material["renewed.pem"] = issue_server_certificate(args.openssl, runtime, "renewed", "ca")
        report["public_certificate_hashes"] = material
        before_tls = tls_listener(runtime, "server", "ca")
        rejected_tls = tls_listener(runtime, "untrusted", "wrong")
        after_tls = tls_listener(runtime, "renewed", "ca")
        require(len({before_tls[1], rejected_tls[1], after_tls[1]}) == 3, "fixture TLS identities must differ")
        ca = str(runtime / "ca.pem")
        if args.client.suffix.lower() == ".exe" and platform.system() == "Linux":
            ca = subprocess.check_output(["wslpath", "-w", ca], text=True, timeout=5).strip()
        client_args.append(ca)

    first, before = start(stack, args.server, runtime, port, "before", log_path("before"), before_tls)
    report["before"] = before
    # The Windows relay can appear slightly after Redis is already reachable in WSL.
    if args.client.suffix.lower() == ".exe" and platform.system() == "Linux":
        time.sleep(1)
    log = stack.enter_context(log_path("client").open("wb"))
    probe = subprocess.Popen(client_args, stdin=subprocess.PIPE if args.tls else None,
                             stdout=subprocess.PIPE, stderr=log, text=True)
    stack.enter_context(probe.stdout)
    if probe.stdin is not None:
        stack.enter_context(probe.stdin)
    stack.callback(stop, probe)
    report["launcher_pid"] = probe.pid
    lines = queue.Queue()
    reader = threading.Thread(target=read_phases, args=(probe.stdout, lines), daemon=True)
    reader.start()

    wait_phase(lines, report["phases"], "blocked_submitted")
    due = time.monotonic() + 5
    while True:
        fields = info(port, "all", before_tls)
        require(first.poll() is None and int(fields["process_id"]) == before["pid"] and
                fields["run_id"] == before["run_id"], "original Redis identity changed")
        blocked = int(fields["blocked_clients"])
        if blocked == 1:
            break
        require(blocked == 0 and time.monotonic() < due, f"BLPOP not observed, blocked_clients={blocked}")
        time.sleep(.005)
    before.update(blocked_clients=blocked, commands=command_calls(fields))
    warmup = {"hello": 4, "subscribe": 1, "client|tracking": 1, "get": 1, "set": 2, "publish": 1}
    check_commands(before["commands"], warmup, "original")
    before["killed_at_monotonic"] = time.monotonic()
    first.kill()
    before["exit_code"] = first.wait(timeout=5)
    require(before["exit_code"] != 0, "original process did not terminate forcibly")
    outage = wait_phase(lines, report["phases"], "outage_ready")
    require(outage["pool_failures"] > 2 and outage["subscription_failures"] > 1, "failure barriers not observed")

    if args.tls:
        middle, rejected = start(stack, args.server, runtime, port, "untrusted", log_path("untrusted"), rejected_tls)
        report["untrusted"] = rejected
        require(rejected["run_id"] != before["run_id"], "untrusted replacement reused run_id")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=2) as raw:
                with before_tls[0].wrap_socket(raw, server_hostname="localhost"):
                    raise RuntimeError("original trust accepted the untrusted replacement")
        except ssl.SSLCertVerificationError:
            pass
        probe.stdin.write("\n")
        probe.stdin.flush()
        refusal = wait_phase(lines, report["phases"], "untrusted_rejected")
        require(all(refusal[key] > outage[key] for key in ("pool_failures", "subscription_failures")),
                "untrusted replacement did not exercise both recovery paths")
        fields = info(port, "all", rejected_tls)
        require(middle.poll() is None and int(fields["process_id"]) == middle.pid and
                fields["run_id"] == rejected["run_id"], "untrusted Redis identity changed")
        rejected["commands"] = command_calls(fields)
        check_commands(rejected["commands"], {}, "untrusted replacement")
        stop(middle)
        rejected["exit_code"] = middle.returncode

    second, after = start(stack, args.server, runtime, port, "after", log_path("after"), after_tls)
    report["after"] = after
    require(after["run_id"] != before["run_id"] and
            (not args.tls or after["run_id"] != report["untrusted"]["run_id"]), "replacement Redis reused run_id")
    complete = wait_phase(lines, report["phases"], "complete")
    require(complete["pool_generations"] == 4 and complete["subscription_generation"] == 2 and
            complete["recovery_ms"] > 0, f"unexpected recovery result {complete}")
    report["launcher_exit_code"] = probe.wait(timeout=5)
    require(report["launcher_exit_code"] == 0, "client did not close successfully")
    reader.join(timeout=5)
    require(not reader.is_alive() and lines.get_nowait() is None and lines.empty(), "unexpected client output at exit")

    fields = info(port, "all", after_tls)
    require(second.poll() is None and int(fields["process_id"]) == after["pid"] and fields["run_id"] == after["run_id"],
            "replacement Redis identity changed")
    calls = command_calls(fields)
    after.update(commands=calls, blocked_clients=int(fields["blocked_clients"]), connected_clients=int(fields["connected_clients"]))
    check_commands(calls, {**warmup, "get": 2, "echo": 16}, "replacement")
    require(after["blocked_clients"] == 0 and after["connected_clients"] == 1,
            "only the observer connection may remain")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, default=Path(shutil.which("redis-server") or "redis-server"))
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--tls", action="store_true", help="test verified TLS 1.3 recovery and certificate rotation")
    parser.add_argument("--openssl", type=Path, default=shutil.which("openssl"))
    parser.add_argument("--port-candidate", type=int, action="append", default=[],
                        help="Caller-screened candidate for cross-host WSL and Windows loopback")
    args = parser.parse_args()
    if args.tls and args.openssl is None:
        parser.error("--tls requires openssl to generate temporary identities")
    if args.tls:
        args.openssl = args.openssl.absolute()
    args.client = args.client.resolve(strict=True)
    # Keep the invocation name for multi-call Redis packages.
    args.server = args.server.absolute()
    args.report = args.report.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = dict(schema=1, complete=False, fixture_host=platform.platform(), client=str(args.client),
                  client_sha256=sha(args.client), server_sha256=sha(args.server), runner_sha256=sha(__file__),
                  server_version=subprocess.check_output([str(args.server), "--version"], text=True).strip(),
                  transport="tls1.3" if args.tls else "tcp", phases=[])
    if args.tls:
        report["tls"] = dict(cipher=TLS_CIPHER, hostname="localhost", openssl_sha256=sha(args.openssl),
                             certificate_helper_sha256=sha(Path(__file__).with_name("run_redis_tls.py")))
    try:
        with ExitStack() as stack:
            run(args, report, stack)
        report["complete"] = True
        print(json.dumps(report["phases"][-1]))
    except Exception:
        report["failure"] = traceback.format_exc()
        raise
    finally:
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
