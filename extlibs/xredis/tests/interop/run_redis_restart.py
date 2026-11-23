"""Verify xredis across Redis process replacement and deliberate loopback reply loss."""
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

from run_redis_role import frame, sha
from run_redis_tls import (TLS_CIPHER, certificates, issue_server_certificate,
                           listener_connection, listener_exchange, tls_listener)


EDIT_CHANNELS = (b"xrt:restart:\0channel", b"xrt:restart:\0intermediate", b"xrt:restart:\0replacement")
INCREMENT_REPLY = b":1\r\n"


def check_subscribers(port, counts, tls):
    expected = [item for pair in zip(EDIT_CHANNELS, counts) for item in pair]
    reply, _ = listener_exchange(port, ["PUBSUB", "NUMSUB", *EDIT_CHANNELS], tls=tls)
    require(reply == expected, f"unexpected independent subscription ledger {reply!r}")
    return counts


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def info(port, section, tls=None):
    value, _ = listener_exchange(port, ["INFO", section], tls=tls)
    return dict(row.split(":", 1) for row in value.decode().splitlines() if ":" in row)


def command_calls(fields):
    return {key.removeprefix("cmdstat_"): int(value.split(",", 1)[0].removeprefix("calls="))
            for key, value in fields.items() if key.startswith("cmdstat_")}


def check_commands(calls, expected, stage):
    # INFO traffic consists only of the independent observer's requests.
    require({key: value for key, value in calls.items() if key != "info"} == expected,
            f"unexpected {stage} command ledger {calls}")


def blocked_queues(port, tls):
    clients, _ = listener_exchange(port, ["CLIENT", "LIST", "TYPE", "normal"], tls=tls)
    queues = []
    for line in clients.decode("ascii").splitlines():
        fields = dict(entry.split("=", 1) for entry in line.split())
        if "b" in fields["flags"] and fields["cmd"] == "blpop":
            queues.append(int(fields["qbuf"]))
    return sorted(queues)


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


def launch_client(stack, command, log_path, report, *, with_input=False):
    log = stack.enter_context(log_path.open("wb"))
    probe = subprocess.Popen(command, stdin=subprocess.PIPE if with_input else None,
                             stdout=subprocess.PIPE, stderr=log, text=True)
    stack.enter_context(probe.stdout)
    if probe.stdin is not None:
        stack.enter_context(probe.stdin)
    stack.callback(stop, probe)
    report["launcher_pid"] = probe.pid
    lines = queue.Queue()
    reader = threading.Thread(target=read_phases, args=(probe.stdout, lines), daemon=True)
    reader.start()
    return probe, lines, reader


def wait_client(probe, lines, reader, report):
    report["launcher_exit_code"] = probe.wait(timeout=5)
    require(report["launcher_exit_code"] == 0, "client did not close successfully")
    reader.join(timeout=5)
    require(not reader.is_alive() and lines.get_nowait() is None and lines.empty(), "unexpected client output at exit")


def client_ca(runtime, client):
    ca = str(runtime / "ca.pem")
    if client.suffix.lower() == ".exe" and platform.system() == "Linux":
        ca = subprocess.check_output(["wslpath", "-w", ca], text=True, timeout=5).strip()
    return ca


def run_lost_reply(args, report, stack):
    candidates = args.port_candidate or random.sample(range(12000, 45000), 64)
    port = fixture_port(candidates)
    runtime = Path(stack.enter_context(tempfile.TemporaryDirectory(prefix="xredis-reply-loss-")))
    upstream_tls = relay_tls = None
    if args.tls:
        material = certificates(args.openssl, runtime)
        material["relay.pem"] = issue_server_certificate(args.openssl, runtime, "relay", "ca")
        report["public_certificate_hashes"] = material
        upstream_tls = tls_listener(runtime, "server", "ca")
        relay_tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        relay_tls.minimum_version = relay_tls.maximum_version = ssl.TLSVersion.TLSv1_3
        relay_tls.num_tickets = 0
        relay_tls.load_cert_chain(runtime / "relay.pem", runtime / "relay.key")
    server, identity = start(stack, args.server, runtime, port, "reply-loss",
                             args.report.with_suffix(".redis.log"), upstream_tls)
    report["server"] = identity
    relay = stack.enter_context(socket.socket())
    relay_port = fixture_port(candidates) if args.port_candidate else 0
    relay.bind(("127.0.0.1", relay_port))
    relay.listen(1)
    relay.settimeout(6)
    relay_port = relay.getsockname()[1]
    report.update(port=port, relay_port=relay_port)
    if args.client.suffix.lower() == ".exe" and platform.system() == "Linux":
        time.sleep(1)  # Allow WSL to publish both loopback listeners on Windows.
    command = [str(args.client), "--lost-reply", str(relay_port), str(port), str(args.reply_bytes)]
    if args.tls:
        command.append(client_ca(runtime, args.client))
    probe, lines, reader = launch_client(stack, command, args.report.with_suffix(".client.log"), report)

    key = b"xrt:restart:\0applied"
    prefix = b"pipeline\0prefix"
    tail = b"pipeline\0tail"
    client, _ = relay.accept()
    client.settimeout(5)
    relay.close()
    # Forward exactly the three requests and hold the actual Redis responses.
    with ExitStack() as peers:
        peers.enter_context(client)
        if relay_tls is not None:
            client = peers.enter_context(relay_tls.wrap_socket(client, server_side=True))
            require(client.version() == "TLSv1.3" and client.cipher()[0] == TLS_CIPHER,
                    "unexpected TLS profile at the reply relay")
            report["relay_tls"] = {"version": client.version(), "cipher": client.cipher()[0]}
        upstream = peers.enter_context(listener_connection(port, tls=upstream_tls, timeout=5))
        with client.makefile("rb") as client_wire, upstream.makefile("rb") as server_wire:
            if relay_tls is not None:
                request, wire = frame(client_wire)
                require(request == [b"HELLO", b"3"], "TLS client did not negotiate RESP3")
                upstream.sendall(wire)
                hello, wire = frame(server_wire)
                require(hello[b"proto"] == 3, "TLS Redis did not negotiate RESP3")
                client.sendall(wire)
            for expected in ([b"ECHO", prefix], [b"INCR", key], [b"ECHO", tail]):
                request, wire = frame(client_wire)
                require(request == expected, f"unexpected pipeline command {request!r}")
                upstream.sendall(wire)
            replies = [frame(server_wire) for _ in range(3)]
        require([value for value, _ in replies] == [prefix, 1, tail], "unexpected Redis pipeline replies")
        require(replies[1][1] == INCREMENT_REPLY, "unexpected integer reply framing")
        integer_prefix = INCREMENT_REPLY[:args.reply_bytes]
        client.sendall(replies[0][1] + integer_prefix)
        wait_phase(lines, report["phases"], "prefix_verified")
        stored, _ = listener_exchange(port, ["GET", key], tls=upstream_tls)
        require(stored == b"1", "independent observer did not see the applied INCR before cutoff")
        calls = command_calls(info(port, "commandstats", upstream_tls))
        expected_calls = {"echo": 2, "incr": 1, "get": 1}
        if args.tls:
            expected_calls["hello"] = 1
        check_commands(calls, expected_calls, "before reply cutoff")
        report["cutoff"] = {"stored_value": 1, "commands": calls, "forwarded_integer_bytes": len(integer_prefix),
                            "withheld_bytes": len(INCREMENT_REPLY) - len(integer_prefix) + len(replies[2][1])}
        if relay_tls is None:
            client.shutdown(socket.SHUT_WR)
        else:
            try:
                client.unwrap().close()
            except (ssl.SSLEOFError, ConnectionResetError):
                # The probe retires the transport once it classifies the RESP EOF.
                pass

    completed = wait_phase(lines, report["phases"], "complete")
    replied = args.reply_bytes == len(INCREMENT_REPLY)
    truncated = 0 < args.reply_bytes < len(INCREMENT_REPLY)
    require(completed == {"phase": "complete", "retained_replied": 1 + replied, "retained_uncertain": 2 - replied,
                          "status": "PROTOCOL" if truncated else "DISCONNECTED",
                          "delivery": "MAY_HAVE_EXECUTED", "read_after_reconnect": 1},
            "unexpected retained result ledger")
    wait_client(probe, lines, reader, report)
    stored, _ = listener_exchange(port, ["GET", key], tls=upstream_tls)
    fields = info(port, "all", upstream_tls)
    require(server.poll() is None and fields["run_id"] == identity["run_id"] and
            int(fields["process_id"]) == identity["pid"], "Redis identity changed during reply loss")
    require(stored == b"1" and int(fields["connected_clients"]) == 1, "duplicate mutation or surviving application connection")
    calls = command_calls(fields)
    expected_calls["get"] = 3
    if args.tls:
        expected_calls["hello"] = 2
    check_commands(calls, expected_calls, "after reply loss recovery")
    report["after"] = {"stored_value": 1, "commands": calls, "connected_clients": 1}


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
        client_args.append(client_ca(runtime, args.client))
    if args.edit_subscriptions:
        client_args.append("--edit-subscriptions")

    first, before = start(stack, args.server, runtime, port, "before", log_path("before"), before_tls)
    report["before"] = before
    # The Windows relay can appear slightly after Redis is already reachable in WSL.
    if args.client.suffix.lower() == ".exe" and platform.system() == "Linux":
        time.sleep(1)
    probe, lines, reader = launch_client(stack, client_args, log_path("client"), report,
                                         with_input=args.tls or args.edit_subscriptions)

    initial = wait_phase(lines, report["phases"], "blocked_submitted")
    key = b"xrt:restart:\0pipeline-write"
    queued_wire = b"*2\r\n$4\r\nINCR\r\n$" + str(len(key)).encode("ascii") + b"\r\n" + key + b"\r\n"
    inspections = 0
    queues = []
    due = time.monotonic() + 5
    while True:
        fields = info(port, "all", before_tls)
        require(first.poll() is None and int(fields["process_id"]) == before["pid"] and
                fields["run_id"] == before["run_id"], "original Redis identity changed")
        blocked = int(fields["blocked_clients"])
        if blocked == 2:
            queues = blocked_queues(port, before_tls)
            inspections += 1
            if queues == [0, len(queued_wire)]:
                break
        require(0 <= blocked <= 2 and time.monotonic() < due,
                f"blocked pipeline not observed, blocked_clients={blocked}, queues={queues}")
        time.sleep(.005)
    before.update(blocked_clients=blocked, blocked_query_bytes=queues, commands=command_calls(info(port, "all", before_tls)))
    warmup = {"hello": 4, "subscribe": 1, "client|tracking": 1, "get": 1, "set": 2, "publish": 1}
    check_commands(before["commands"], {**warmup, "echo": 1, "client|list": inspections}, "original")
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
    expected = {**warmup, "get": 2, "echo": 16}
    if args.edit_subscriptions:
        edited = wait_phase(lines, report["phases"], "edited_recovered")
        require(edited["subscription_revision"] == initial["subscription_revision"] + 4,
                "replacement did not publish the final desired revision")
        after["active_subscriber_counts"] = check_subscribers(port, [0, 0, 1], after_tls)
        probe.stdin.write("\n")
        probe.stdin.flush()
        expected.update(publish=3, **{"pubsub|numsub": 2})

    complete = wait_phase(lines, report["phases"], "complete")
    require(complete["pipeline_prefix_sequence"] == initial["pipeline_prefix_sequence"] > 0 and
            complete["unconfirmed_pipeline_commands"] == 2, "pipeline outcomes changed during recovery")
    if args.edit_subscriptions:
        require(complete["retained_observations"] and
                complete["subscription_revision"] == edited["subscription_revision"],
                "edited revision or retained observations were lost after teardown")
        after["retired_subscriber_counts"] = check_subscribers(port, [0, 0, 0], after_tls)
    require(complete["pool_generations"] == 4 and complete["subscription_generation"] == 2 and
            complete["recovery_ms"] > 0, f"unexpected recovery result {complete}")
    wait_client(probe, lines, reader, report)

    fields = info(port, "all", after_tls)
    require(second.poll() is None and int(fields["process_id"]) == after["pid"] and fields["run_id"] == after["run_id"],
            "replacement Redis identity changed")
    calls = command_calls(fields)
    after.update(commands=calls, blocked_clients=int(fields["blocked_clients"]), connected_clients=int(fields["connected_clients"]))
    check_commands(calls, expected, "replacement")
    require(after["blocked_clients"] == 0 and after["connected_clients"] == 1,
            "only the observer connection may remain")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, default=Path(shutil.which("redis-server") or "redis-server"))
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--tls", action="store_true", help="test verified TLS 1.3 recovery and certificate rotation")
    parser.add_argument("--openssl", type=Path, default=shutil.which("openssl"))
    parser.add_argument("--edit-subscriptions", action="store_true",
                        help="replace binary subscription intent while Redis is unavailable")
    parser.add_argument("--port-candidate", type=int, action="append", default=[],
                        help="Caller-screened candidate for cross-host WSL and Windows loopback")
    parser.add_argument("--lost-reply", action="store_true",
                        help="verify an executed INCR when the loopback relay cuts off its reply")
    parser.add_argument("--reply-bytes", type=int, choices=range(len(INCREMENT_REPLY) + 1),
                        help="integer reply bytes forwarded by --lost-reply (default: 3)")
    args = parser.parse_args()
    if args.reply_bytes is not None and not args.lost_reply:
        parser.error("--reply-bytes requires --lost-reply")
    if args.reply_bytes is None:
        args.reply_bytes = len(INCREMENT_REPLY) - 1
    if args.lost_reply and args.edit_subscriptions:
        parser.error("--lost-reply is separate from subscription recovery")
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
                  transport="tls1.3" if args.tls else "tcp", edit_subscriptions=args.edit_subscriptions, phases=[])
    if args.tls:
        report["tls"] = dict(cipher=TLS_CIPHER, hostname="localhost", openssl_sha256=sha(args.openssl),
                             certificate_helper_sha256=sha(Path(__file__).with_name("run_redis_tls.py")))
    if args.lost_reply:
        report["scenario"] = "lost-reply"
    try:
        with ExitStack() as stack:
            run_fixture = run_lost_reply if args.lost_reply else run
            run_fixture(args, report, stack)
        report["complete"] = True
        print(json.dumps(report["phases"][-1]))
    except Exception:
        report["failure"] = traceback.format_exc()
        raise
    finally:
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
