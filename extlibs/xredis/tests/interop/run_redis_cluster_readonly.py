"""Check READONLY against a disposable three-primary Redis Cluster and one replica."""
import argparse
from pathlib import Path
import random
import shutil
import socket
import subprocess
import tempfile
import time
import json
import traceback

from run_redis_role import exchange, frame, send, sha


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


def identity(port, process):
    assert process.poll() is None, ("fixture exited", port, process.returncode)
    reply, _ = exchange(port, ["INFO", "SERVER"])
    fields = dict(line.split(b":", 1) for line in reply.split(b"\r\n") if b":" in line)
    assert int(fields[b"process_id"]) == process.pid, ("unowned port", port)
    return fields[b"run_id"].decode()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client", type=Path, action="append", required=True)
    parser.add_argument("--server", type=Path, default=Path(shutil.which("redis-server") or "redis-server"))
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    server = args.server.absolute()
    clients = [client.absolute() for client in args.client]
    report = args.report.absolute()
    if report.exists():
        parser.error("report already exists")
    report.parent.mkdir(parents=True, exist_ok=True)
    result = dict(
        complete=False, server_version=subprocess.check_output([str(server), "--version"], text=True).strip(),
        server_sha256=sha(server), runner_sha256=sha(__file__), nodes=[], cases=[],
    )
    children = []
    with tempfile.TemporaryDirectory(prefix="xredis-cluster-readonly-") as temporary:
        runtime = Path(temporary)
        try:
            ports = available_ports()
            for port in ports:
                directory = runtime / str(port)
                directory.mkdir()
                command = [
                    str(server), "--bind", "127.0.0.1", "--port", str(port), "--save", "", "--appendonly", "no",
                    "--dir", str(directory), "--cluster-enabled", "yes", "--cluster-config-file", "nodes.conf",
                    "--cluster-node-timeout", "1000", "--repl-diskless-sync-delay", "0",
                ]
                with (directory / "server.log").open("wb") as log:
                    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
                children.append((port, process))
                run_id = wait_until(lambda: identity(port, process), 10, "owned Redis startup")
                node_id, _ = exchange(port, ["CLUSTER", "MYID"])
                result["nodes"].append(dict(port=port, pid=process.pid, run_id=run_id, node_id=node_id.decode()))

            for (port, process), node in zip(children, result["nodes"]):
                assert identity(port, process) == node["run_id"]
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
            with socket.create_connection(("127.0.0.1", master), timeout=8) as connection, connection.makefile("rb") as reply:
                send(connection, ["SET", KEY, VALUE])
                assert frame(reply)[0] == b"OK"
                send(connection, ["WAIT", 1, 5000])
                assert frame(reply)[0] == 1
            with socket.create_connection(("127.0.0.1", replica), timeout=5) as connection, connection.makefile("rb") as reply:
                send(connection, ["READONLY"])
                assert frame(reply)[0] == b"OK"
                send(connection, ["GET", KEY])
                assert frame(reply)[0] == VALUE
            moved = f"MOVED {slot} 127.0.0.1:{master}"
            result.update(slot=slot, primary=master, replica=replica, expected_moved=moved)
            run_id = result["nodes"][3]["run_id"]
            for client in clients:
                assert identity(replica, children[3][1]) == run_id
                run = subprocess.run(
                    [str(client), str(replica), run_id, moved], text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45,
                )
                result["cases"].append(dict(client=str(client), sha256=sha(client), exit_code=run.returncode, output=run.stdout))
                assert run.returncode == 0 and "[PASS] real Cluster READONLY:" in run.stdout, run.stdout
                print(client.name, "passed", flush=True)
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
