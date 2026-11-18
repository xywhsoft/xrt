"""Verified offer ledger for the real Redis Sentinel fixed-arrival fixture."""

from bisect import bisect_right
from collections import Counter
import gzip
import hashlib

NS = 1_000_000_000
STAGES = ("pending", "generator_capacity", "admission_deadline", "acquire_rejected",
          "acquire_result", "lease_deadline", "call_rejected", "call_result")
STATUSES = ("ok", "server_error", "timeout", "cancelled", "closed", "disconnected",
            "protocol", "limit", "memory", "initialization")
DELIVERY = ("not_sent", "may_have_executed", "replied")
ADMISSION = {-1: "error", 1: "overloaded", 2: "closed"}


def quantiles(samples):
    samples.sort()
    return {f"p{label}_us": samples[(len(samples) * rank + 999) // 1000 - 1] / 1000 if samples else None
            for label, rank in (("50", 500), ("95", 950), ("99", 990), ("999", 999))}


def load_report(output, config, control, artifact):
    count, rate, slots, timeout = config
    horizon = count * NS // rate
    markers = [line.split()[1:] for line in output.splitlines() if line.startswith("LOAD-MARKERS ")]
    assert len(markers) == 1 and "[PASS] fixed-arrival Sentinel pool load" in output
    end, trigger, event, online = map(int, markers[0])
    assert end >= horizon > trigger >= NS
    if control:
        assert event == online == 0
        cuts = [0, trigger, horizon]
        names = ("before_marker", "after_marker")
    else:
        assert trigger <= event <= end and trigger <= online <= horizon - NS
        cuts = [0, trigger, online, horizon]
        names = ("before_trigger", "trigger_to_pool_online", "after_pool_online")
    phases = {name: {"start_ns": start, "end_ns": stop, "offered": 0, "successful": 0,
                     "deadline_missed": 0, "completed_successfully_in_window": 0,
                     "outcomes": Counter(), "delivery": Counter(), "all": [], "success": [], "generator": []}
              for name, start, stop in zip(names, cuts, cuts[1:])}

    compressed_rows = ["index,due_ns,submitted_ns,finished_ns,stage,status,delivery"]
    successes = []
    tail_success = 0
    prior_finish = 0
    for line in output.splitlines():
        if not line.startswith("LOAD "):
            continue
        i, due, submitted, finish, stage, status, delivery = map(int, line.split()[1:])
        assert i == len(compressed_rows) - 1 < count and due == i * NS // rate
        assert due <= submitted <= finish <= end and 0 < stage < len(STAGES) and 0 <= delivery < len(DELIVERY)
        assert delivery == 0 or stage == 7
        if stage in (3, 6):
            outcome = f"{STAGES[stage]}:{ADMISSION[status]}"
        elif stage in (4, 7):
            assert 0 <= status < len(STATUSES) and (stage != 4 or status != 0)
            outcome = f"{STAGES[stage]}:{STATUSES[status]}"
        else:
            assert status == 0
            outcome = STAGES[stage]
        phase = phases[names[bisect_right(cuts, due) - 1]]
        phase["offered"] += 1
        phase["outcomes"][outcome] += 1
        phase["delivery"][DELIVERY[delivery]] += 1
        elapsed = finish - due
        phase["all"].append(elapsed)
        phase["generator"].append(submitted - due)
        phase["deadline_missed"] += elapsed >= timeout * 1_000_000
        if stage == 7 and status == 0:
            assert delivery == 2
            phase["successful"] += 1
            phase["success"].append(elapsed)
            successes.append(finish)
            if finish < horizon:
                phases[names[bisect_right(cuts, finish) - 1]]["completed_successfully_in_window"] += 1
            else:
                tail_success += 1
        compressed_rows.append(",".join(map(str, (i, due, submitted, finish, stage, status, delivery))))
        prior_finish = max(prior_finish, finish)
    assert len(compressed_rows) == count + 1 and prior_finish > 0 and successes
    for phase in phases.values():
        duration = phase["end_ns"] - phase["start_ns"]
        assert duration > 0 and phase["offered"] == sum(phase["outcomes"].values()) > 0
        phase["success_latency"] = quantiles(phase.pop("success"))
        phase["all_latency"] = quantiles(phase.pop("all"))
        phase["generator_lag"] = quantiles(phase.pop("generator"))
        phase["success_completions_per_sec"] = phase["completed_successfully_in_window"] * NS / duration
    assert sum(phase["offered"] for phase in phases.values()) == count
    assert phases[names[0]]["successful"] and phases[names[-1]]["successful"]
    timeline = [0] + sorted(t for t in successes if t < horizon) + [horizon]
    artifact.write_bytes(gzip.compress(("\n".join(compressed_rows) + "\n").encode("ascii"), mtime=0))
    return {
        "schedule": {"count": count, "rate_per_sec": rate, "generator_slots": slots, "timeout_ms": timeout,
                     "payload_bytes": 32, "command": "PING", "horizon_ns": horizon},
        "elapsed_ns": end, "trigger_marker_ns": trigger, "switch_event_observed_ns": event or None,
        "full_replacement_observed_ns": online or None,
        "longest_success_completion_gap_ns": max(stop - start for start, stop in zip(timeline, timeline[1:])),
        "successes": len(successes), "drain_successes": tail_success, "phases": phases,
        "ledger": artifact.name, "ledger_sha256": hashlib.sha256(artifact.read_bytes()).hexdigest(),
    }
