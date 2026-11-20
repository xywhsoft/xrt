"""Compare loopback MQTT 5 QoS 0 fan-out with an identical fixed-arrival observer."""
import argparse
from contextlib import contextmanager
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import socket
import statistics
import subprocess
import tempfile
import time

MOSQUITTO = "mosquitto version 2.1.2"
METRICS = ("offered", "deliveries", "measurement_seconds", "ingress_per_sec", "deliveries_per_sec",
           "delivery_p50_us", "delivery_p95_us", "delivery_p99_us", "delivery_p999_us", "offer_lag_p99_us")


if os.name == "nt":
    from ctypes import wintypes as win

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    kernel.OpenProcess.argtypes = [win.DWORD, win.BOOL, win.DWORD]
    kernel.OpenProcess.restype = win.HANDLE
    kernel.CloseHandle.argtypes = [win.HANDLE]
    kernel.GetProcessAffinityMask.argtypes = [win.HANDLE, ctypes.POINTER(ctypes.c_size_t),
                                             ctypes.POINTER(ctypes.c_size_t)]
    kernel.SetProcessAffinityMask.argtypes = [win.HANDLE, ctypes.c_size_t]
    kernel.GetProcessTimes.argtypes = [win.HANDLE] + [ctypes.POINTER(win.FILETIME)] * 4
    psapi.GetProcessMemoryInfo.argtypes = [win.HANDLE, win.LPVOID, win.DWORD]

    @contextmanager
    def handle(pid):
        opened = kernel.OpenProcess(0x400 | 0x200 | 0x10, False, pid)
        if not opened:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            yield opened
        finally:
            kernel.CloseHandle(opened)

    def affinity(pid):
        mask, system = ctypes.c_size_t(), ctypes.c_size_t()
        with handle(pid) as process:
            if not kernel.GetProcessAffinityMask(process, ctypes.byref(mask), ctypes.byref(system)):
                raise ctypes.WinError(ctypes.get_last_error())
        return {cpu for cpu in range(ctypes.sizeof(mask) * 8) if mask.value & (1 << cpu)}

    def pin(pid, cpus):
        with handle(pid) as process:
            if not kernel.SetProcessAffinityMask(process, sum(1 << cpu for cpu in cpus)):
                raise ctypes.WinError(ctypes.get_last_error())

    class Memory(ctypes.Structure):
        _fields_ = [("size", win.DWORD), ("faults", win.DWORD), ("peak", ctypes.c_size_t),
                    ("rss", ctypes.c_size_t), ("unused", ctypes.c_size_t * 6)]

    def usage(pid):
        times = [win.FILETIME() for _ in range(4)]
        memory = Memory()
        memory.size = ctypes.sizeof(memory)
        with handle(pid) as process:
            if not kernel.GetProcessTimes(process, *(ctypes.byref(t) for t in times)):
                raise ctypes.WinError(ctypes.get_last_error())
            if not psapi.GetProcessMemoryInfo(process, ctypes.byref(memory), memory.size):
                raise ctypes.WinError(ctypes.get_last_error())
        cpu = sum((t.dwHighDateTime << 32) | t.dwLowDateTime for t in times[2:]) / 10_000_000
        return cpu, memory.rss, memory.peak

else:
    affinity = os.sched_getaffinity
    pin = os.sched_setaffinity

    def usage(pid):
        process = Path("/proc") / str(pid)
        stat = (process / "stat").read_text().rsplit(") ", 1)[1].split()
        status = (process / "status").read_text()
        rss, peak = (int(re.search(rf"^{field}:\s+(\d+) kB", status, re.M)[1]) * 1024
                     for field in ("VmRSS", "VmHWM"))
        return (int(stat[11]) + int(stat[12])) / os.sysconf("SC_CLK_TCK"), rss, peak


def start(command, cpus, **kwargs):
    pid = os.getpid()
    original = affinity(pid)
    pin(pid, cpus)
    try:
        process = subprocess.Popen(command, **kwargs)
    finally:
        pin(pid, original)
    try:
        if affinity(process.pid) != cpus:
            raise RuntimeError(f"unexpected affinity for process {process.pid}")
    except BaseException:
        stop(process)
        raise
    return process


def stop(process, graceful=False):
    if process.poll() is None:
        if graceful:
            process.stdin.write(b"\n")
            process.stdin.close()
        else:
            process.terminate()
    try:
        return process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        return process.wait(timeout=5)


def run_observer(args, port, workload):
    kind, peers, rate = workload
    command = [str(args.observer), "--remote", str(port), str(peers), str(args.rounds),
               str(rate), str(args.warmup), kind]
    process = start(command, args.load_cpus, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        output, errors = process.communicate(timeout=20 + (args.rounds + args.warmup) / rate)
    except BaseException:
        stop(process)
        raise
    if process.returncode:
        raise RuntimeError(f"observer exit {process.returncode}: {errors.strip()}")
    result = {key: float(value) for key, value in (line.split(": ", 1) for line in output.splitlines())}
    if set(result) != set(METRICS) or not all(math.isfinite(v) and v >= 0 for v in result.values()):
        raise ValueError(f"invalid observer output {output!r}")
    if result["offered"] != args.rounds or result["deliveries"] != peers * args.rounds:
        raise ValueError("publication and delivery counts do not match the workload")
    return result


def run_sample(args, implementation, workload, directory):
    path = directory / "broker.log"
    if implementation == "mosquitto":
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        config = directory / "mosquitto.conf"
        config.write_text(f"listener {port} 127.0.0.1\nallow_anonymous true\npersistence false\n"
                          "max_connections 1024\nmax_packet_size 16384\nset_tcp_nodelay true\n"
                          "sys_interval 0\nlog_dest stderr\nlog_type error\nlog_type notice\n", encoding="ascii")
        command = [str(args.mosquitto), "-c", str(config)]
        pattern = None
    else:
        port = None
        command = [str(args.xmqtt), "--compare"]
        pattern = re.compile(r"^127\.0\.0\.1:(\d+)$", re.M)
    with path.open("wb") as log:
        broker = start(command, {args.broker_cpu}, stdin=subprocess.PIPE, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 8
            while True:
                message = path.read_text(errors="replace")
                if broker.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError(f"broker did not start: {message.strip()}")
                if pattern and (match := pattern.search(message)):
                    port = int(match[1])
                if port is not None:
                    try:
                        with socket.create_connection(("127.0.0.1", port), timeout=0.05):
                            break
                    except (ConnectionError, TimeoutError):
                        pass
                time.sleep(0.01)
            time.sleep(0.05)
            cpu, _, _ = usage(broker.pid)
            start_time = time.perf_counter()
            result = run_observer(args, port, workload)
            seconds = time.perf_counter() - start_time
            after, rss, peak = usage(broker.pid)
            result.update(broker_cpu_seconds=after - cpu, observer_process_seconds=seconds,
                          broker_rss_bytes=rss, broker_peak_rss_bytes=peak)
        finally:
            status = stop(broker, graceful=implementation == "xmqtt")
            if implementation == "xmqtt" and status and broker.returncode is not None:
                raise RuntimeError(f"xmqtt retirement exit {status}: {path.read_text(errors='replace')}")
    return result


def fingerprint(path):
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def noise(samples):
    middle = statistics.median(samples)
    mad = statistics.median(abs(value - middle) for value in samples)
    central = sorted(samples)[1:-1] if len(samples) >= 5 else samples
    span = max(central) - min(central)
    return {"median": middle, "relative_mad": mad / middle if middle else 0,
            "relative_central_span": span / middle if middle else 0,
            "stable": mad <= middle * 0.03 and span <= middle * 0.06}


def parse_workload(value):
    try:
        kind, peers, rate = value.split(":")
        peers, rate = int(peers), int(rate)
        if kind not in ("exact", "wildcard") or not 1 <= peers <= 64 or not 1 <= rate <= 100000:
            raise ValueError(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected exact|wildcard:subscribers:rate") from error
    return kind, peers, rate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for label in ("xmqtt", "mosquitto", "observer", "report"):
        parser.add_argument(f"--{label}", required=True, type=Path)
    parser.add_argument("--broker-cpu", type=int, required=True)
    parser.add_argument("--load-cpus", type=lambda value: {int(cpu) for cpu in value.split(",")}, required=True)
    parser.add_argument("--workload", action="append", type=parse_workload, required=True)
    parser.add_argument("--rounds", type=int, default=6000)
    parser.add_argument("--warmup", type=int, default=512)
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()
    if not ({args.broker_cpu} | args.load_cpus) <= affinity(os.getpid()) or args.broker_cpu in args.load_cpus:
        parser.error("broker and observer need disjoint available logical CPUs")
    if not 1 <= args.rounds <= 100000 or not 0 <= args.warmup <= 100000 or not 3 <= args.repeats <= 31:
        parser.error("invalid round, warmup, or repeat count")
    for label in ("xmqtt", "mosquitto", "observer"):
        setattr(args, label, getattr(args, label).resolve(strict=True))
    version = subprocess.check_output([args.mosquitto, "-h"], stderr=subprocess.DEVNULL, text=True, timeout=8).splitlines()[0]
    if version != MOSQUITTO:
        parser.error(f"expected {MOSQUITTO!r}, obtained {version!r}")
    report = {"platform": platform.platform(), "broker_cpu": args.broker_cpu, "load_cpus": sorted(args.load_cpus),
              "rounds": args.rounds, "warmup": args.warmup, "repeats": args.repeats,
              "mosquitto_version": version, "mqtt": 5, "qos": 0, "payload_bytes": 128, "security": "plaintext",
              "persistence": False, "complete": False,
              "programs": {label: fingerprint(getattr(args, label)) for label in ("xmqtt", "mosquitto", "observer")},
              "runner": fingerprint(Path(__file__)),
              "observer_source": fingerprint(Path(__file__).with_name("bench_server.c")),
              "workloads": []}
    args.report = args.report.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    try:
        for workload in args.workload:
            group = {"filter": workload[0], "subscribers": workload[1], "scheduled_rate": workload[2], "samples": []}
            report["workloads"].append(group)
            for repeat in range(args.repeats):
                order = ("xmqtt", "mosquitto") if repeat % 2 == 0 else ("mosquitto", "xmqtt")
                for label in order:
                    row = {"repeat": repeat, "broker": label}
                    group["samples"].append(row)
                    with tempfile.TemporaryDirectory(prefix="xmqtt-broker-compare-") as temp:
                        try:
                            row.update(run_sample(args, label, workload, Path(temp)))
                        except Exception as error:
                            row["failure"] = str(error)
                    print(workload, repeat, label, row.get("failure", row.get("delivery_p99_us")), flush=True)
            if not any("failure" in row for row in group["samples"]):
                group["summary"] = {
                    label: {metric: noise([row[metric] for row in group["samples"] if row["broker"] == label])
                            for metric in ("ingress_per_sec", "delivery_p99_us")}
                    for label in ("xmqtt", "mosquitto")
                }
        report["complete"] = all("summary" in group for group in report["workloads"])
    finally:
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
