# Checkpoint a live cron service

The optional service checkpoint module supports actual and selected live
services. Create a service with its checkpointed constructor. Its arena
includes one 180-byte record and key for each configured command slot.
Ordinary construction leaves that reservation disabled. Checkpoint commands
share the existing FIFO, work limit, reply credits and command generations.
Admission copies a restore record and retains the config zone. Collection
transfers the original Data duty after a failed restore, the live Job ID
after a successful restore, or the captured record after a successful save.

The owner saves only quiescent jobs and validates only the one saved
occurrence when restoring. An active lease or unfinished calculation can
return kernel BUSY. Save captures the state at command execution. The
service can advance again before the caller receives or commits that state.
Selected service restores the delivery policy and local metadata from the
saved record. The caller must reconstruct the correct configuration and
manage one live owner for each persistent identity.

The caller can use the [checkpoint store](checkpoint_store.md) to read
before restore and commit after capture. File synchronization runs on the
calling or separate storage thread. Use one ordered producer for a key
when publishing generations. The application owns the order between
business effects, snapshot requests and durable commits. After a process
interruption, execution can repeat work following the last committed
snapshot.

Use SetPausedAsync with true and collect the ordinary pause ticket to
fence new leases for one job. Previously published leases can still
start, retry and complete. Drain their physical receipts and allow any
pending search to finish before capturing the record. The hold remains
in force across capture and file synchronization. Resume the job only
once the caller has accepted that boundary. Other jobs keep their
existing schedule, including delays from their own completion and
executor limits.

Use RestorePausedCheckpointAsync to stage a recovered job before
starting it. A due cursor is held from the instant the owner admits it.
The record contains the same counters and schedule state as a regular
snapshot. Resume is an explicit runtime operation. On resume the
preserved cursor is subject to the job's normal misfire and overlap
policies at the current sample. Ordinary RestoreCheckpointAsync allows
dispatch immediately after admission. Automatic snapshots and grouped
durable commits remain follow-up work.

## Start a service from a committed group

Use one checkpointed group constructor for startup. Supply the committed
records alongside the application's expected keys and job configurations,
in matching stable Job order. The caller keeps both input arrays and zones
valid until return. File reading and generation selection are done by the
caller before startup. Use a borrowed view from the checkpoint group
store or an array of individually validated-format records.

The owner validates and installs each record before publishing the service.
An invalid record rolls back every prior admission, releases its zone
reference, and returns a zero-based member index and scheduler issue. The
service and ID output array stay untouched. A global startup failure
reports NO_INDEX. On success, both outputs are ready before the first clock
callback. Each scheduled job starts held. A terminal record is available
for normal retirement. Application Data is borrowed only after successful
startup and is returned by ordinary retirement.

The owner allocates no per-member runtime object beyond the configured
scheduler entries. A temporary ID array costs Count times sizeof(xcronjobid).
It is charged to BytesLimit and the optional initialization budget and freed
before publication. Preparation has one validation pass with the usual
single-instant occurrence validation per member. Steady-state turns use
the existing scheduler budgets. A host can resume jobs individually after
it accepts the restored generation.
[Runnable group recovery example](../examples/service_checkpoint_group/README.md).

## Capture and hold a live group

Submit SaveCheckpointGroupAsync with distinct live IDs and strictly
increasing persistent Job keys. The owner validates the whole set at
one command boundary. It then writes every record in stable key order
and holds new dispatch for all scheduled members. An active lease or
unfinished search returns BUSY with the failed index. Other validation
errors also preserve the output and the prior holds. Hold busy jobs and
wait for physical completion or recurrence progress before retrying.

A command uses one ticket and spends one CommandWorkLimit unit per
member, or one unit for an empty group. If another command used part of
the current turn, the group stays at the queue head until a turn with
enough budget. Each member is validated and encoded once. The owner
uses caller-owned record storage, with no extra allocation on success.

Keep the input array immutable and the output array exclusively borrowed
through successful CollectCheckpointGroup, including while the service
closes. A timed-out or wrong collector leaves the borrow outstanding.
A successful collection transfers the diagnostic, if present, and ends
the array borrow. Check the reply status before using the records.

Commit the captured records through the group store on the caller or
storage thread. Hold ownership across this interval by coordinating any
other producers that could resume, cancel or reap the jobs. Release jobs
after the caller accepts the file commit outcome.
[Runnable group recovery and capture example](../examples/service_checkpoint_group/README.md).
