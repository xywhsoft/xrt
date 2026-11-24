"""Measure scheduler and direct XRT waiter cohorts at matched clock precision."""

import argparse
from hashlib import sha256
import json
import math
import os
from pathlib import Path
import platform
from statistics import median
import subprocess
import sys


COHORTS = ((64, 960), (1024, 0))
MAX_WAVES = 8192
DRIVES = ("wait", "waiter-precise")
MEASURES = ("first_lateness", "last_lateness", "callback_span")
TAIL_QUANTILES = (("99", 990), ("999", 999))
QUANTILES = (("50", 500), ("95", 950), *TAIL_QUANTILES)


def rank(samples, quantile):
    return sorted(samples)[(len(samples) * quantile + 999) // 1000 - 1]


def observe(program, cpu, waves, active, dormant, drive, require_wait_phases=False):
    arguments = [str(program), "cohort", str(waves), str(active), str(dormant), drive, "--samples"]
    process = subprocess.run(arguments, env={**os.environ, "XBENCH_PIN_CPU": str(cpu)},
                             capture_output=True, text=True, timeout=waves + 30)
    if process.returncode:
        raise ValueError(f"benchmark exited {process.returncode}\n{process.stdout}{process.stderr}")
    pairs = [line.split(": ", 1) for line in process.stdout.splitlines()]
    fields = dict(pairs)
    if len(fields) != len(pairs):
        raise ValueError("duplicate benchmark field")

    callbacks = active * waves
    expected = {"affinity": f"cpu-{cpu}", "drive": drive, "waves": str(waves),
                "active_jobs": str(active), "dormant_jobs": str(dormant),
                "verified_callbacks": str(callbacks)}
    for key, value in expected.items():
        if fields.get(key) != value:
            raise ValueError(f"expected {key}={value}, received {fields.get(key)}")

    wave_keys = [key for key in fields if key.startswith("wave_")]
    if len(wave_keys) != waves:
        raise ValueError("wave sample count differs from the reported workload")
    samples = []
    for index in range(1, waves + 1):
        first, last, span = map(int, fields[f"wave_{index}_ns"].split(","))
        if not (0 <= first <= last < 1_000_000_000 and 0 <= span < 1_000_000_000):
            raise ValueError(f"invalid wall or monotonic time in wave {index}")
        samples.append((first, last, span))

    for index, name in enumerate(MEASURES):
        unit, scale = ("us", 1000) if index == 2 else ("ms", 1_000_000)
        values = [sample[index] for sample in samples]
        for label, quantile in QUANTILES:
            observed = float(fields[f"{name}_p{label}_{unit}"])
            if not math.isclose(observed, rank(values, quantile) / scale, rel_tol=0, abs_tol=0.000501):
                raise ValueError(f"reported {name} P{label} differs from the raw samples")

    cpu_cost = float(fields["cpu_ns_per_callback"])
    elapsed = float(fields["wait_seconds"])
    if not math.isfinite(cpu_cost) or cpu_cost < 0 or not math.isfinite(elapsed) or abs(elapsed - waves) >= 1:
        raise ValueError("invalid process CPU or elapsed time")
    result = {"verified_callbacks": callbacks, "samples_ns": samples, "cpu_ns_per_callback": cpu_cost,
              "wait_seconds": elapsed, "wall_clock": fields["wall_clock"]}
    if "process_cycles" in fields:
        cycles = int(fields["process_cycles"])
        if cycles <= 0:
            raise ValueError("invalid process CPU cycle count")
        result.update(process_cycles=cycles, process_cycles_per_callback=cycles / callbacks)
    phase_keys = [key for key in fields if key.startswith("phase_")]
    if phase_keys or "waiter_backend" in fields:
        if len(phase_keys) != waves:
            raise ValueError("wait phase sample count differs from the reported workload")
        phases = []
        for index in range(1, waves + 1):
            calls, budget, entry, returned, dispatch, wait = map(int, fields[f"phase_{index}_ns"].split(","))
            if min(calls, budget, dispatch, wait) < 0 or (calls == 0 and any((budget, entry, returned, dispatch, wait))):
                raise ValueError(f"invalid wait phase in wave {index}")
            phases.append((calls, budget, entry, returned, dispatch, wait))
        backend = int(fields["waiter_backend"])
        if backend < -1 or (backend == -1 and any(phase[0] for phase in phases)):
            raise ValueError("missing measured waiter backend")
        result.update(waiter_backend=backend, wait_phases_ns=phases)
    if require_wait_phases and not any(phase[0] for phase in result.get("wait_phases_ns", ())):
        raise ValueError("benchmark did not record any required wait phases")
    return result


def summarize(runs):
    result = {}
    for index, name in enumerate(MEASURES):
        groups = [[sample[index] for sample in run["samples_ns"]] for run in runs]
        pooled = [value for group in groups for value in group]
        summary = {"pooled_ns": {label: rank(pooled, quantile) for label, quantile in QUANTILES}}
        for label, quantile in TAIL_QUANTILES:
            tail = sorted(rank(group, quantile) for group in groups)
            center = median(tail)
            mad = median(abs(value - center) for value in tail)
            central_range = tail[-2] - tail[1] if len(tail) >= 5 else None
            summary[f"p{label}_per_process_ns"] = tail
            summary[f"p{label}_mad_ns"] = mad
            summary[f"p{label}_central_range_ns"] = central_range
            summary[f"p{label}_stable"] = (
                central_range is not None and mad * 100 <= center * 3 and central_range * 100 <= center * 6
            )
        result[name] = summary
    return result


def append(report, event, **fields):
    report.write(json.dumps({"event": event, **fields}, allow_nan=False) + "\n")
    report.flush()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("--cpu", type=int, required=True, help="logical CPU enforced by the benchmark")
    parser.add_argument("--report", type=Path, required=True, help="new JSON Lines evidence file")
    parser.add_argument("--waves", type=int, default=256)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--active-tasks", type=int, choices=tuple(active for active, _ in COHORTS),
                        help="run only the job mix with this many active tasks")
    parser.add_argument("--require-wait-phases", action="store_true",
                        help="reject runs without at least one measured wait call")
    args = parser.parse_args()
    if not 0 <= args.cpu < 1024 or not 1 <= args.waves <= MAX_WAVES or args.repeats < 1:
        parser.error(f"expected CPU in 0..1023, 1..{MAX_WAVES} waves and at least one repeat")

    cohorts = tuple(mix for mix in COHORTS if args.active_tasks in (None, mix[0]))
    program = args.program.resolve(strict=True)
    identity = sha256(program.read_bytes()).hexdigest()
    runner = sha256(Path(__file__).read_bytes()).hexdigest()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open("x", encoding="utf-8") as report:
        append(report, "start", platform=platform.platform(), program=str(program), binary_sha256=identity,
               runner_sha256=runner, cpu=args.cpu, waves=args.waves, repeats=args.repeats,
               cohorts=cohorts)
        total = 0
        try:
            for active, dormant in cohorts:
                rows = {drive: [] for drive in DRIVES}
                for repeat in range(args.repeats):
                    order = DRIVES if repeat % 2 == 0 else reversed(DRIVES)
                    for drive in order:
                        key = {"active": active, "dormant": dormant, "drive": drive, "repeat": repeat + 1}
                        run = observe(program, args.cpu, args.waves, active, dormant, drive,
                                      require_wait_phases=args.require_wait_phases)
                        append(report, "run", **key, **run)
                        rows[drive].append(run)
                        total += run["verified_callbacks"]
                for drive in DRIVES:
                    append(report, "series", active=active, dormant=dormant, drive=drive,
                           metrics=summarize(rows[drive]))
            if sha256(program.read_bytes()).hexdigest() != identity:
                raise ValueError("benchmark binary changed during measurement")
        except (KeyError, OSError, subprocess.SubprocessError, ValueError) as error:
            append(report, "failed", error=str(error), verified_callbacks=total)
            print(error, file=sys.stderr)
            return 1
        except KeyboardInterrupt:
            append(report, "cancelled", verified_callbacks=total)
            return 130
        append(report, "complete", verified_callbacks=total)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
