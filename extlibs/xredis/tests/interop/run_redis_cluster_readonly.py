"""Check READONLY against a disposable three-primary Redis Cluster and one replica."""
import argparse
from functools import partial
import os
from pathlib import Path
import random
import shutil
import socket
import subprocess
import tempfile
import time
import json
import traceback

from run_redis_role import frame, send, sha
from run_redis_tls import TLS_CIPHER, certificates, listener_connection, listener_exchange, tls_listener


KEY = b"xrt-owned:readonly:{fixed}\0key"
VALUE = b"value\0x"


def available_ports():
    ports = []
    for port in random.SystemRandom().sample(range(15000, 40000), 128):
        if any(abs(port - prior) in (0, 10000) for prior in ports):
            continue
        with socket.socket() as service, socket.socket() as bus:
            try:
                service.bind(("127.0.0.1", port))
                bus.bind(("127.0.0.1", port + 10000))
            except OSError:
                continue
        ports.append(port)
        if len(ports) == 4:
            return ports
    raise RuntimeError("four owned Cluster port pairs unavailable")


def wait_until(check, seconds, message):
    deadline = time.monotonic() + seconds
    while True:
        try:
            value = check()
            if value:
                return value
        except OSError as error:
            if time.monotonic() >= deadline:
                raise RuntimeError(message) from error
        if time.monotonic() >= deadline:
            raise RuntimeError(message)
        time.sleep(0.05)


def client_path(client, path):
    if os.name != "nt" and client.suffix.lower() == ".exe":
        return subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip()
    return str(path)


def identity(port, process, exchange):
    assert process.poll() is None, ("fixture exited", port, process.returncode)
    reply, _ = exchange(port, ["INFO", "SERVER"])
    fields = dict(line.split(b":", 1) for line in reply.split(b"\r\n") if b":" in line)
    assert int(fields[b"process_id"]) == process.pid, ("unowned port", port)
    return fields[b"run_id"].decode()


def check_primary_loss(clients, children, metadata, master, replica, slot, report, during_election,
                       exchange, connect, ca):
    by_port = dict(children)
    run_ids = {node["port"]: node["run_id"] for node in metadata["nodes"]}
    node_ids = {node["port"]: node["node_id"].encode() for node in metadata["nodes"]}
    surviving = [port for port in by_port if port != master]
    witness = next(port for port in surviving if port != replica)
    signal = report.with_name(f"{report.stem}-promoted")
    down = report.with_name(f"{report.stem}-primary-down")
    evidence = dict(witness=witness, cases=[])
    metadata["primary_loss"] = evidence
    probes = []
    kinds = ("inflight", "unsent", "election") if during_election else ("inflight", "unsent")
    cases = [(client, f"c{index}-{kind}", kind) for index, client in enumerate(clients) for kind in kinds]
    try:
        for port, process in children:
            assert identity(port, process, exchange) == run_ids[port]
        with connect(master, timeout=8) as connection, connection.makefile("rb") as reply:
            for _, label, _ in cases:
                send(connection, ["SET", f"xrt-owned:failover:{{fixed}}:{label}", 0])
                assert frame(reply)[0] == b"OK"
            send(connection, ["WAIT", 1, 5000])
            assert frame(reply)[0] == 1

        for client, label, kind in cases:
            log = report.with_name(f"{report.stem}-{label}.log")
            signal_path = client_path(client, down if kind == "election" else signal)
            command = [str(client), "promotion", str(master), str(replica), str(witness), kind, label, signal_path]
            if ca:
                command.append(client_path(client, ca))
            with log.open("wb") as output:
                process = subprocess.Popen(
                    command, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT,
                )
            probes.append((client, label, kind, process, log))

        def clients_ready():
            ready = True
            for _, label, kind, process, log in probes:
                output = log.read_text(encoding="utf-8")
                assert process.poll() is None, (label, output)
                ready = ready and f"READY {kind} {label}" in output
            return ready

        def blocked_clients():
            text, _ = exchange(master, ["CLIENT", "LIST"])
            return sum(b"cmd=blpop" in line for line in text.splitlines())

        wait_until(clients_ready, 20, "all native clients observed the old topology")
        wait_until(lambda: blocked_clients() == len(clients), 5, "all blocking requests reached the old primary")
        evidence["observed_blpop"] = blocked_clients()
        assert identity(master, by_port[master], exchange) == run_ids[master]
        began = time.monotonic()
        by_port[master].kill()
        evidence["old_primary_exit_code"] = by_port[master].wait(timeout=5)
        if during_election:
            down.touch()
            submissions = [(label, log) for _, label, kind, _, log in probes if kind == "election"]
            wait_until(lambda: all(f"OFFERED {label}" in log.read_text(encoding="utf-8") for label, log in submissions),
                       2, "native election writes submitted")
            assert exchange(replica, ["ROLE"])[0][0] == b"slave", "election write offered after promotion"
            evidence["submitted_before_promotion"] = len(submissions)

        def ownership_changed():
            if exchange(replica, ["ROLE"])[0][0] != b"master":
                return False
            for port in surviving:
                slots, _ = exchange(port, ["CLUSTER", "SLOTS"])
                if not any(first <= slot <= last and owner[1:3] == [replica, node_ids[replica]]
                           for first, last, owner, *_ in slots):
                    return False
            return True

        wait_until(ownership_changed, 25, "survivors converge on automatic election")
        assert identity(replica, by_port[replica], exchange) == run_ids[replica]
        evidence["kill_to_survivor_convergence_ms"] = (time.monotonic() - began) * 1000
        evidence["promoted_role"] = exchange(replica, ["ROLE"])[0][0].decode()
        evidence["metadata_wire"] = {
            name: exchange(witness, ["CLUSTER", name], protocol=3)[1].decode("ascii")
            for name in ("SHARDS", "SLOTS")
        }
        signal.touch()
        failures = []
        for client, label, kind, process, log in probes:
            exit_code = process.wait(timeout=15)
            output = log.read_text(encoding="utf-8")
            case = dict(client=str(client), label=label, kind=kind, exit_code=exit_code, output=output)
            evidence["cases"].append(case)
            counter, _ = exchange(replica, ["GET", f"xrt-owned:failover:{{fixed}}:{label}"])
            case["server_counter"] = None if counter is None else int(counter)
            passed = exit_code == 0 and f"[PASS] real Cluster primary loss: {kind} {label}" in output and counter == b"1"
            if kind == "inflight":
                passed = passed and "DISCONNECTED MAY_HAVE_EXECUTED attempts=1" in output
            if passed:
                print(client.name, kind, "primary-loss recovery passed", flush=True)
            else:
                failures.append(case)
        assert not failures, failures
    finally:
        for _, _, _, process, _ in probes:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
        evidence["probes_terminal"] = all(process.poll() is not None for _, _, _, process, _ in probes)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client", type=Path, action="append", required=True)
    parser.add_argument("--server", type=Path, default=Path(shutil.which("redis-server") or "redis-server"))
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--primary-loss", action="store_true", help="kill the owned primary and observe automatic election")
    parser.add_argument("--during-election", action="store_true", help="also offer unsent writes before the replica promotes")
    parser.add_argument("--tls", action="store_true", help="require server-authenticated TLS 1.3 on every node")
    parser.add_argument("--openssl", type=Path, default=shutil.which("openssl"))
    args = parser.parse_args()
    if args.during_election and not args.primary_loss:
        parser.error("--during-election requires --primary-loss")
    if args.tls and args.openssl is None:
        parser.error("--tls needs OpenSSL to generate fixture certificates")
    server = args.server.absolute()
    clients = [client.absolute() for client in args.client]
    report = args.report.absolute()
    if report.exists():
        parser.error("report already exists")
    report.parent.mkdir(parents=True, exist_ok=True)
    result = dict(
        complete=False, transport="tls" if args.tls else "tcp", server_version=subprocess.check_output([str(server), "--version"], text=True).strip(),
        server_sha256=sha(server), runner_sha256=sha(__file__), nodes=[], cases=[],
    )
    children = []
    with tempfile.TemporaryDirectory(prefix="xredis-cluster-readonly-") as temporary:
        runtime = Path(temporary)
        try:
            tls = ca = None
            if args.tls:
                openssl = args.openssl.absolute()
                hashes = certificates(openssl, runtime, cluster_peer=True)
                ca = runtime / "ca.pem"
                tls = tls_listener(runtime, "server", "ca")
                result["tls"] = dict(protocol="TLSv1.3", cipher=TLS_CIPHER, hostname="localhost",
                                     ca_sha256=hashes["ca.pem"], leaf_sha256=hashes["server.pem"],
                                     openssl_sha256=sha(openssl))
            exchange = partial(listener_exchange, tls=tls)
            connect = partial(listener_connection, tls=tls)
            service_port = b"tls-port" if args.tls else b"port"
            ports = available_ports()
            for port in ports:
                directory = runtime / str(port)
                directory.mkdir()
                command = [
                    str(server), "--bind", "127.0.0.1", "--save", "", "--appendonly", "no",
                    "--dir", str(directory), "--cluster-enabled", "yes", "--cluster-config-file", "nodes.conf",
                    "--cluster-node-timeout", "2000" if args.during_election else "1000", "--repl-diskless-sync-delay", "0",
                ]
                if tls:
                    command += [*tls[2], "--tls-port", str(port), "--tls-cluster", "yes",
                                "--tls-replication", "yes", "--cluster-announce-ip", "127.0.0.1"]
                else:
                    command += ["--port", str(port)]
                with (directory / "server.log").open("wb") as log:
                    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
                children.append((port, process))
                run_id = wait_until(lambda: identity(port, process, exchange), 10, "owned Redis startup")
                node_id, _ = exchange(port, ["CLUSTER", "MYID"])
                node = dict(port=port, pid=process.pid, run_id=run_id, node_id=node_id.decode())
                result["nodes"].append(node)
                if tls:
                    config, _ = exchange(port, ["CONFIG", "GET", "port", "tls-port", "tls-cluster",
                                                "tls-replication", "tls-protocols"], protocol=3)
                    assert config == {b"port": b"0", b"tls-port": str(port).encode(), b"tls-cluster": b"yes",
                                      b"tls-replication": b"yes", b"tls-protocols": b"TLSv1.3"}, config
                    node["tls_settings"] = {key.decode(): value.decode() for key, value in config.items()}

            for (port, process), node in zip(children, result["nodes"]):
                assert identity(port, process, exchange) == node["run_id"]
            for port in ports[1:]:
                assert exchange(ports[0], ["CLUSTER", "MEET", "127.0.0.1", port])[0] == b"OK"
            ranges = [(ports[0], 0, 5460), (ports[1], 5461, 10922), (ports[2], 10923, 16383)]
            for port, first, last in ranges:
                assert exchange(port, ["CLUSTER", "ADDSLOTSRANGE", first, last])[0] == b"OK"
            wait_until(
                lambda: all(b"cluster_state:ok\r\n" in exchange(port, ["CLUSTER", "INFO"])[0] for port in ports),
                20, "Cluster coverage",
            )
            slot, _ = exchange(ports[0], ["CLUSTER", "KEYSLOT", KEY])
            master = next(port for port, first, last in ranges if first <= slot <= last)
            replica = ports[3]
            master_id = result["nodes"][ports.index(master)]["node_id"]
            assert exchange(replica, ["CLUSTER", "REPLICATE", master_id])[0] == b"OK"

            def replica_ready():
                role, _ = exchange(replica, ["ROLE"])
                return role[:4] == [b"slave", b"127.0.0.1", master, b"connected"]

            wait_until(replica_ready, 15, "replica link")
            with connect(master, timeout=8) as connection, connection.makefile("rb") as reply:
                send(connection, ["SET", KEY, VALUE])
                assert frame(reply)[0] == b"OK"
                send(connection, ["WAIT", 1, 5000])
                assert frame(reply)[0] == 1
            with connect(replica, timeout=5) as connection, connection.makefile("rb") as reply:
                send(connection, ["READONLY"])
                assert frame(reply)[0] == b"OK"
                send(connection, ["GET", KEY])
                assert frame(reply)[0] == VALUE
            replica_id = result["nodes"][3]["node_id"].encode()

            def replica_advertised():
                shards, _ = exchange(master, ["CLUSTER", "SHARDS"], protocol=3)
                slots, _ = exchange(master, ["CLUSTER", "SLOTS"])
                for shard in shards:
                    bounds = shard[b"slots"]
                    if not any(first <= slot <= last for first, last in zip(bounds[::2], bounds[1::2])):
                        continue
                    if not any(
                        node.get(b"id") == replica_id and node.get(b"role") == b"replica" and
                        node.get(b"health") == b"online" and node.get(service_port) == replica
                        for node in shard[b"nodes"]
                    ):
                        return False
                    return any(
                        first <= slot <= last and any(node[1:3] == [replica, replica_id] for node in nodes[1:])
                        for first, last, *nodes in slots
                    )
                return False

            wait_until(replica_advertised, 15, "owning primary advertises the online replica in SHARDS and SLOTS")
            moved = f"MOVED {slot} 127.0.0.1:{master}"
            result.update(slot=slot, primary=master, replica=replica, expected_moved=moved)
            result["metadata_wire"] = {
                command: exchange(master, ["CLUSTER", command])[1].decode("ascii")
                for command in ("SHARDS", "SLOTS")
            }
            run_id = result["nodes"][3]["run_id"]
            for client in clients:
                assert identity(replica, children[3][1], exchange) == run_id
                command = [str(client), str(replica), run_id, moved, str(master)]
                if ca:
                    command.append(client_path(client, ca))
                run = subprocess.run(
                    command, text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45,
                )
                result["cases"].append(dict(client=str(client), sha256=sha(client), exit_code=run.returncode, output=run.stdout))
                assert run.returncode == 0 and "[PASS] real Cluster READONLY:" in run.stdout and \
                    "[PASS] real Cluster replica routing:" in run.stdout, run.stdout
                print(client.name, "passed", flush=True)
            if args.primary_loss:
                check_primary_loss(clients, children, result, master, replica, slot, report, args.during_election,
                                   exchange, connect, ca)
            result["complete"] = True
        except BaseException:
            result["failure"] = traceback.format_exc()
            raise
        finally:
            for _, process in children:
                if process.poll() is None:
                    process.terminate()
            for port, process in children:
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
                shutil.copy2(runtime / str(port) / "server.log", report.with_name(f"{report.stem}-{port}.log"))
            result["processes_terminal"] = all(process.poll() is not None for _, process in children)
            report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
