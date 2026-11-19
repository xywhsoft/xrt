# Scheduler checkpoints

Select XCRON_MODULE_XCRON_CHECKPOINT and include <xcron/checkpoint.h>.
The module saves one job from the actual-time or the policy-aware selected
scheduler. The calls run on the existing serialized owner. Saving takes a
bounded snapshot of a completed job position. Restoration validates the
record and admits a job into the supplied scheduler.

A job can be checkpointed after its physical completions and while its next
occurrence is known. Take the checkpoint before closing the owner or reaping
the job. Explicit cancellation and exhaustion can be saved and restored as
reapable terminal jobs. A live lease, initial provisional admission or
unfinished recurrence returns BUSY. Recorded calculation faults are
reported to the caller instead of being silently reactivated.

The record binds two nonzero application keys to the job. Job identifies the
persisted definition. Revision must change when the definition, timezone
data or intended behavior changes. Keep the matching timezone revision and
job configuration available after restart. Restoration compares scheduling
fields and the keys. Restore each stable key once per scheduler. The application supplies fresh process-local Data.
The selected scheduler takes its fold and missing-time policy from the
record and rebuilds the selected occurrence's local label and multiplicity.
A pending RUN_AT_TRANSITION compensation restores at its saved UTC anchor
with the original absent label and zero candidates. A committed exhausted
record restores the completed job without rerunning it.
The restored instant is revalidated against the supplied expression and
zone. Old job IDs, run IDs, running leases, monotonic clocks and
scheduler-wide statistics are scoped to the previous process. The six
per-job counters continue from the saved values.

## Version 1 record

The file or database field contains exactly 180 bytes. Integers are little
endian. Signed instants use the XRT signed millisecond timeline encoded in
two's complement. Padding in C structures is excluded.

| Byte range | Contents |
| --- | --- |
| 0–7 | XCCP marker, version 1, mode, state and a reserved zero byte |
| 8–23 | Job and revision keys |
| 24–55 | Begin, end, next and grace |
| 56–103 | Six expression masks in field order |
| 104–127 | Expression flags, dialect, search flags, running limit, misfire, overlap, fold, missing and four reserved zero bytes |
| 128–175 | Dispatch, success, failure, cancellation, skipped-range and coalesced-range counters |
| 176–179 | CRC-32/ISO-HDLC over bytes 0–175 |

RUN_AT_TRANSITION uses missing-policy value 2 in the existing layout. Upgrade
checkpoint readers before enabling it. Older readers reject that value.
A two-slot reader can select an earlier compatible generation during recovery.

CRC checks accidental corruption. Authenticate or otherwise protect records
when trust matters. The owner rejects corrupt, mismatched and off-schedule
records before retaining the zone or occupying a job slot. The v1 snapshot
adds no scheduler allocation, recurrence bookkeeping, callback or I/O to
the normal dispatch path.

## Commit boundary

Publish an immutable record through the host's durable store after its
chosen physical completion boundary. Restart replays any occurrence after
the last committed checkpoint, including an interrupted occurrence whose
business effects may already have happened. Applications can coordinate
updates with their business transaction or use idempotent effects.
The checkpoint API leaves locking, atomic replacement, multi-job
transactions, live service integration and power-loss guarantees to the
storage layer. The process-kill test exercises replay and advancement with
caller-synchronized files. [Runnable example](../examples/checkpoint/README.md).
