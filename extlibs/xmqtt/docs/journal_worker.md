# Bounded Journal submission on a borrowed task pool

`xmqtt_journal_worker` is a developing storage-owner component. It adds no
thread, pool, network transport or general runtime. The host supplies a running
XRT task pool and transfers one recovered READY Journal on successful Create.
The Journal still borrows its already published file/provider; keep both alive
and exclusively owned until worker physical retirement and destruction. Direct
Journal aliases refuse append/replay/Info/Destroy after transfer. Failure to
Create consumes nothing. Constructors perform no file IO.

## Submission and outcomes

AppendAsync copies one opaque immutable transaction into a bounded result/job
allocation before ACCEPTED. Use the typed store-delta codec separately and
validate legal MQTT transitions in the future live owner. Successful admission
order defines FIFO. A reserved scheduling phase returns normal OVERLOADED to
competing callers until host task admission is known. Native pool rejection
returns ERROR and publishes no Future. It rolls back all reservations; no
previously accepted sibling can be stranded by that rejection.

A pump has a lifetime admission budget WorkTransactions. Replenishing a hot
queue cannot keep the same native task alive forever: further calls receive
normal OVERLOADED once the turn budget is exhausted. Admitted requests drain,
then new work gets a fresh native task at the host queue tail. Zero turn budget
admits nothing. A blocked backend/callback can still occupy its host thread;
this is bounded-work scheduling, not a realtime guarantee. Callers retry only
unaccepted work under their original deadline, and never spin inside an own
completion callback waiting for capacity that callback must return.

The result Future always resolves to `xmqttcommitresult`. Status, AppendStatus
and Effect have independent meaning:

| Event | Local result | Physical effect |
| --- | --- | --- |
| Cancel/expiry/Close before append | CANCELLED/TIMEOUT/CLOSED | NOT_COMMITTED, AppendCalled=false |
| Native task cancelled before Proc | INITIALIZATION; worker stops | NOT_COMMITTED for every queued request |
| Native data write/sync failure | STORAGE, IO | NOT_COMMITTED; stop and recover |
| Witness write/publication-sync failure | STORAGE, UNCERTAIN | MAY_BE_COMMITTED; stop and recover |
| Both barriers completed | OK/DONE normally | COMMITTED with exact Sequence |
| Cancel/expiry/Close during native append | Corresponding local status | Preserve actual result, including COMMITTED |
| Error restoration fails after commit | INITIALIZATION or STORAGE/ERROR | Preserve actual COMMITTED sequence; stop |
| Local journal capacity refusal | LIMIT | NOT_COMMITTED for this request; previous commit is not inherited |

Only the actual new attempt advances AttemptSequence. Sequence is nonzero only
when this request is physically COMMITTED. Journal in Info is the last completed
snapshot; Busy means a newer append may already have effects. Cause is an
optional retained fresh operation error, never a stale callback ambient error.
A failed native error slot may supply no Cause. DONE/LIMIT use checked quiet
ambient restoration. A poisoned or locally unusable worker stops later writes.

Timeout is relative milliseconds from API entry; -1 means no deadline. Cancel
requests (Future or raw token) are sampled before/after native append. No syscall
is interrupted. Waiting on the Future can time out while its append still owns
physical duties. A typed result waits for that append to return; dropping a
Future does not cancel it. A fresh recovery resolves uncertain storage. Never
blindly replay a failed/timed-out/cancelled application operation. This component
does not authorize network output or attest business exactly-once effects.

## Resource and callback lifetime

Defaults are128 outstanding Requests,256 retained Objects,4MiB extension requested
memory,2 native Tasks including retirement tails, 1MiB per transaction and32 successful admissions per pump turn. Zero
limits are real caps. MaxBytes counts the ledger, input/result allocations and
pump contexts before allocation, through their actual free. Requests return
when physically completed, while Objects/input bytes remain charged through the
last Future destructor and synchronous result-watch callbacks. The result's
Data is the owned original snapshot. Supply caller-owned writable output storage;
borrowed Core error, pool and Future internals are never valid writable outputs. Immutable held results can outlive worker,
Journal, file and pool: they retain an inert ledger and payload only.

The task barrier retires this extension's Data/observer/pump ownership. Remaining
internal executor bookkeeping is still owned by the host pool; host shutdown/join
includes that tail. Keep its code and any separately installed task cleanup alive.

Opaque Core Promise/cancel/task allocations, diagnostics, the transferred
Journal and backend/system storage are additional. Their counts are bounded by
Objects/Tasks and the host pool. Diagnostics and external provider allocations
are not a claim of hard aggregate byte accounting. Observed allocation edges
and the matched benchmark count these costs explicitly.

Completion runs outside the ledger gate on a host worker, or on the thread
cancelling/pruning a queued native task. Native submission can prune cancelled
neighbours synchronously, so that thread can be another submitting caller.
Diagnostic and completion callbacks must be nonblocking; return on backpressure.
The host storage pool must not run unrelated blocking destructors in submission. Reentrant Info/Close/Append are supported.
An allocation-free TLS scope stack tracks every enclosing owner, including
nested submissions and synchronous native cancelled-neighbour destructors.
Wait/Destroy reject same-pool workers and enclosing admission/callback scopes.
Caller outputs must be caller-owned writable storage; borrowed Core error/pool/
Future internals are not writable outputs. No blocking file synchronization runs
in the submitting/network thread.

Close stops admission and prevents queued appends. It does not interrupt active
IO or close the file. WaitFor requires Close and waits until native task Data
Drop has returned and its Core Future is terminal and destroyed. It sleeps on
condition signals during native IO/reservations, polling only the final Core
publication tail at1ms. Held terminal result Future handles do not delay
retirement; executing synchronous completion watches must return first.
Destroy performs Close+infinite Wait, destroys the transferred Journal and
releases the creator. Serialize Destroy with other API calls. The host owns
pool shutdown/join. Keep extension/Core callback/destructor code resident through
all native tails and retained results.

## Costs and remaining integration

There is one transaction copy, one input/result allocation (currently the whole
allocation, including payload, is zero initialized), and a final secure wipe.
All three payload memory passes are real costs. Core Promise/cancel ownership and bounded queue bookkeeping per admitted request. An idle-to-active
pump additionally allocates one context and a native task/Future. One pump drains
ready requests without waiting to fill a batch. Each request still uses its own
Journal transaction: two CRC payload scans, frame/witness writes and two native
syncs. No group commit or whole-State serialization is hidden. Mutex/atomic/TLS,
native task retirement and Future/destructor costs remain real.

The [real-file example](../examples/journal_worker/README.md) commits one recovered
Inbox delivery ACK asynchronously and proves it with fresh typed recovery. Tests
separate network-free storage qualification from future live Client integration.
The live legal-transition owner, ACK-before-output ordering, delivery leases,
namespace/locking/compaction/group-commit policy, controlled queue-inclusive
throughput/tails/comparators and native Windows/Linux72h acceptance remain open.
The matched direct/borrowed-pool native-file benchmark is an unpinned component
diagnostic; it does not certify production performance or full durable MQTT.
