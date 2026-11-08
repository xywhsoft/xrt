# Policy-aware scheduling kernel (developing)

Select `XCRON_MODULE_XCRON_SCHEDULER_SELECTED` and include
`<xcron/scheduler_selected.h>`. The implementation shares the scheduler's existing
slot, lease, heap and ownership code. It directly uses the complete occurrence
engine and adds no runtime, callback, thread or per-query allocation. Actual-only
builds keep their original query dependency and storage closure.
An extended build adds a mode flag check to actual-mode admission/poll/report
operations and16 owner bytes on verified x64. Its actual query slots remain416
bytes; selected slots are792 bytes. Selected leases also copy56 nominal bytes
beyond the plain run. These costs remain visible; compatibility is not a claim
of identical runtime cost in an extended build.

`CreateSelected` creates the policy-aware mode. Use `AddSelected`, `PollSelected`,
`JobSelected` and `ReapSelected` with it. The corresponding actual-mode operations
reject this mode atomically. Complete, Cancel, Wake, Inspect, Close and Destroy
retain the same physical lease and externally serialized owner contracts.

Admission validates and snapshots the job and policy, retains its immutable zone,
and borrows Data through Reap. It performs no recurrence traversal. An admitted
job starts SEARCHING, or EXHAUSTED for an empty rounded window. Calculation errors,
including an initial ambiguous/missing/unknown label, become typed job faults
during Poll. They do not retroactively reject admission or release Data early.

All five traversal budgets are global across jobs. Zeros are valid; a known ready
lease can publish at zero search budget and leave its successor pending. FIFO
one-unit queries alternate with heap/ready work. Eager successor calculation
receives one quantum only when no older query waits. Capacity zero permits real
calculation progress while output or lease slots are full. Work units are not an
elapsed-time or scheduler-turn CPU guarantee.

Fold and missing policies follow [complete occurrence selection](occurrence.md).
Each lease snapshots canonical nominal Local and complete Candidates before
successor calculation changes the job's latest issue. Future faults preserve
all physical leases and borrowed Data until completion and retirement. Pending
issues expose only counters. Fault recording does not allocate diagnostics.
Next is meaningful only with HasNext. The ordinary executor adapter can submit
the embedded Run. Use the optional [selected adapter](executor_selected.md) to
provide an immutable nominal callback view and complete copied receipts.

Misfire/overlap policies match the [actual kernel](scheduler.md). Discarded ranges
are not enumerated or validated. Missing REJECT covers retained recurrence search
windows; range-skipping/coalescing can deliberately omit gaps inside a discarded
range. Use catch-up when every selected occurrence must remain eligible. No gap
compensation or durable recovery is claimed here. The optional
[selected live service](service_selected.md) integrates command tickets, retries
and complete selected output/retirement on the existing owner.

Run the [offline example](../examples/scheduler_selected/README.md). Standalone,
single-header and package tests exercise the same exported entry points. Production
performance and native long-run acceptance remain open.
