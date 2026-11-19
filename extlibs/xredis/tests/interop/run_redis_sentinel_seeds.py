"""Verify native repair from a Sentinel selected by the APP example."""

import argparse
from contextlib import ExitStack
import json
from pathlib import Path
import random
import select
import shutil
import socket
import subprocess
import tempfile
import time
import traceback

from run_redis_role import exchange, sha


def ports_for_fixture(exclude=()):
    # Stay below the native Windows ephemeral range for the WSL-to-Windows runner.
    available = [port for port in range(38000, 48000) if port not in exclude]
    candidates = random.sample(available, min(len(available), 64))
    ports = []
    with ExitStack() as stack:
        for port in candidates:
            listener = stack.enter_context(socket.socket())
            try:
                listener.bind(("127.0.0.1", port))
            except OSError:
                continue
            ports.append(port)
            if len(ports) == 4:
                return ports
    raise RuntimeError("could not reserve loopback ports for the Sentinel fixture")


def identity(port):
    info, _ = exchange(port, ["INFO", "SERVER"])
    fields = dict(line.split(b":", 1) for line in info.split(b"\r\n") if b":" in line)
    return int(fields[b"process_id"]), fields[b"run_id"].decode()


def owns_peer(port, alternate):
    rows, _ = exchange(port, ["SENTINEL", "SENTINELS", "cache"])
    for row in rows:
        entry = dict(zip(row[::2], row[1::2]))
        flags = set(entry[b"flags"].split(b","))
        if int(entry[b"port"]) == alternate and b"sentinel" in flags:
            return not flags & {b"s_down", b"o_down", b"disconnected"}
    return False


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def one_case(args, case, workdir, logdir):
    master, replica, seed, alternate = ports_for_fixture()
    children = {}
    logs = []
    probe = None
    try:
        for name, port in zip(("master", "replica", "seed", "learned"), (master, replica, seed, alternate)):
            folder = workdir / name
            folder.mkdir()
            log = (folder / "server.log").open("wb")
            logs.append((name, log))
            command = [str(args.server), "--bind", "127.0.0.1", "--port", str(port), "--dir", str(folder),
                       "--save", "", "--appendonly", "no", "--repl-diskless-sync-delay", "0"]
            if name == "replica":
                command += ["--replicaof", "127.0.0.1", str(master)]
            if name in ("seed", "learned"):
                config = folder / "sentinel.conf"
                config.write_text(f"bind 127.0.0.1\nport {port}\ndir {json.dumps(str(folder))}\n"
                                  f"sentinel monitor cache 127.0.0.1 {master} 2\n"
                                  "sentinel down-after-milliseconds cache 5000\n", encoding="utf-8")
                command = [str(args.server), str(config), "--sentinel"]
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
            children[name] = process
            deadline = time.monotonic() + 10
            while True:
                assert process.poll() is None, (name, process.returncode)
                try:
                    pid, run_id = identity(port)
                    assert pid == process.pid
                    break
                except OSError:
                    assert time.monotonic() < deadline, (name, "listener deadline")
                    time.sleep(.02)
            case["fixtures"][name] = dict(port=port, pid=pid, run_id=run_id)

        deadline = time.monotonic() + 15
        while True:
            value, _ = exchange(replica, ["ROLE"])
            peers, _ = exchange(seed, ["SENTINEL", "REPLICAS", "cache"])
            if value[3] == b"connected" and peers and owns_peer(seed, alternate) and owns_peer(alternate, seed):
                break
            assert time.monotonic() < deadline, "Sentinel peers and replica must be online"
            time.sleep(.05)

        name = args.role
        member = case["fixtures"][name]
        command = [str(args.client), str(seed), str(member["port"]), member["run_id"], member["run_id"],
                   str(case["factory"]), str(case["protocol"]), "3", "seed-" + name, str(alternate)]
        probe = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        deadline = time.monotonic() + 15
        prefix = []
        while True:
            if select.select([probe.stdout], [], [], .2)[0]:
                line = probe.stdout.readline()
                prefix.append(line)
                if line.strip() == "READY":
                    break
            if probe.poll() is not None:
                raise RuntimeError("C probe ended early: " + "".join(prefix) + probe.stdout.read())
            assert time.monotonic() < deadline, "APP-selected seed list not ready"

        case["ready"] = True
        for label, fixture in case["fixtures"].items():
            assert children[label].poll() is None and identity(fixture["port"]) == (fixture["pid"], fixture["run_id"])
        stop(children["seed"])
        assert children["seed"].returncode == 0
        try:
            with socket.create_connection(("127.0.0.1", seed), timeout=1):
                raise AssertionError("configured seed listener survived")
        except ConnectionRefusedError:
            case["configured_seed_refused_connection"] = True
        assert identity(alternate) == (case["fixtures"]["learned"]["pid"], case["fixtures"]["learned"]["run_id"])

        output, _ = probe.communicate("GO\n", timeout=30)
        case["output"] = "".join(prefix) + output
        assert probe.returncode == 0, case["output"]
        assert identity(member["port"]) == (member["pid"], member["run_id"])
    finally:
        if probe is not None:
            stop(probe)
            case["exit_code"] = probe.returncode
            case["probe_reaped"] = probe.poll() is not None
        for name, process in children.items():
            stop(process)
            case["fixtures"].setdefault(name, {})["exit_code"] = process.returncode
        case["servers_reaped"] = all(process.poll() is not None for process in children.values())
        for name, log in logs:
            log.close()
            shutil.copy2(workdir / name / "server.log", logdir / f"{case['factory']}-{case['protocol']}-{name}.log")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--role", choices=("master", "replica"), required=True)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--server", type=Path, default=Path(shutil.which("redis-server") or "redis-server"))
    args = parser.parse_args()
    args.client, args.report, args.server = args.client.absolute(), args.report.absolute(), args.server.absolute()
    logdir = args.report.with_suffix("")
    logdir.mkdir(parents=True, exist_ok=False)
    result = dict(schema=1, complete=False, role=args.role, client_sha256=sha(args.client), runner_sha256=sha(__file__),
                  server_version=subprocess.check_output([str(args.server), "--version"], text=True).strip(),
                  server_sha256=sha(args.server), cases=[])
    try:
        for factory in range(3):
            for protocol in (2, 3, 0):
                case = dict(factory=factory, protocol=protocol, ready=False, fixtures={})
                result["cases"].append(case)
                directory = None
                try:
                    with tempfile.TemporaryDirectory(prefix="redis-seed-") as directory:
                        one_case(args, case, Path(directory), logdir)
                finally:
                    case["runtime_material_removed"] = directory is None or not Path(directory).exists()
                print(args.role, factory, protocol, "learned Sentinel recovery passed", flush=True)
        result["complete"] = True
    except BaseException:
        result["failure"] = traceback.format_exc()
        raise
    finally:
        args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
