# Typed transactions on a borrowed host pool

`xmqtt_store_write` is a developing mutable semantic storage owner. Successful
Create exclusively consumes an immutable recovered State and its exact matching
READY Journal. Version, typed schema, broker/principal/slot binding, generation,
complete sequence, chain and end are checked. Failure consumes neither. Stop
all old State readers and borrowed views before transfer; direct aliases refuse
Info/View/destruction/import after transfer. Pool and the already-published
exclusive file/provider remain borrowed until physical destruction.

This component creates no thread/pool/shared runtime or Core API. It emits no
network MQTT ACK, authenticates no broker, grants no delivery lease and attests
no business effect. The host's live Client driver remains required.

## Preparation, commit and publication

AppendAsync takes one encoded typed-delta batch. The existing bounded Journal
worker owns one immutable input snapshot and serial FIFO task stream. On that
native task, the existing delta decoder and legal State applier validate the
entire batch and prepare all model resources before append. Successful admission
order is FIFO; later queued batches validate against preceding committed state.
An invalid batch can be followed by a valid dependent batch without poisoning.

Preparation copies one fixed State metadata header and only touched entry
metadata into a hash overlay, retaining existing canonical wire pointers. Only
new payload entries copy their wire. A reset may touch all entries; it never
copies all retained payloads. Invalid records, quotas, staging OOM or refusal
leave the original model untouched. Recovered live-at-cut state is a candidate:
a new recorded OPEN is required before live protocol flow operations. Independent
application Inbox ACKs remain legal before that OPEN. The host must derive OPEN
from actual authentication/CONNACK and physically retire an old transport.

The original monotonic deadline is captured before wrapper validation/locking
and passed as an absolute value into native admission. Cancel/expiry/Close are
sampled again after preparation, before IO, and after semantic retirement before
result resolution. No running syscall is interrupted.
Two Journal sync barriers still apply to every transaction. No batch-fill delay,
automatic application retry or group commit is added.

| Result | Model publication | Reuse |
| --- | --- | --- |
| INVALID or policy LIMIT, no append | Original state | Valid later work allowed |
| Cancel/expiry/Close before append | Original state | Follow the worker's close state |
| Storage failure, NOT/MAY committed | Original state | Stop, destroy, fresh recovery |
| Both barriers, OK/COMMITTED | Prepared state before result/watch | Continue |
| Late timeout/cancel/Close with COMMITTED | Prepared state before result/watch | Preserve physical fact; never blindly retry |
| Error restoration fails after COMMITTED | Prepared state installed, owner stopped | Withhold output; destroy/recover |

There is no fallible allocation/error-slot publication after the physical commit
that can prevent model installation. Results use the ordinary immutable
`xmqttcommitresult`, including independent Status/AppendStatus/Effect/Sequence.
INVALID is an added result status for the private semantic policy; the ordinary
opaque worker supplies no semantic validation by itself.

## Bounded memory and work

State's original count, retained/staged-payload memory, cumulative transaction/
operation/replay-byte and native codec limits remain real caps. Zero means zero.
Metadata staging belongs to the separate joint MaxScratchBytes budget (default
1MiB), after fixed writer plus reusable operation/property scratch. Create checks
that the remaining capacity can cover the worst configured entry-plus-batch
reset plan. At most one plan is active because the borrowed worker serializes it.
This reservation is capacity, not a permanently allocated maximum-size arena.
Info.ScratchBytes reports only fixed requested allocation; active plan bytes are
additional within the same configured cap. State.PeakMemoryBytes retains cold
recovery history and online payload peaks; it is not a complete process gauge.

Separating metadata is a tested progress requirement: recovery at its exact
observed peak can leave only216 bytes (MQTT5) or58 bytes (3.1.1) free. Charging a
new metadata plan to that same payload cap previously refused an Inbox ACK that
would release payloads. The raw shared-library reproduction is retained. The
fixed implementation commits that ACK under the same recovered cap, and refuses
insufficient reset metadata capacity at construction without consuming sources.

MaxModelWork defaults65536 records plus base/overlay entries visited during reset.
It is separate from packet byte caps, entry caps and the worker's lifetime pump
admission budget. Work exhaustion during partial virtual reset discards the
whole candidate. Hash matching, output alias validation and native decoder/IO
costs remain bounded by their own entry/byte caps; no realtime guarantee is made.
Normal operations do not scan/copy the whole State to construct a transaction.

Empty-prefix recovery owns an expected ClientId. Matching BIND borrows it during
preparation and transfers that existing allocation only after COMMITTED; abort
keeps the original expectation. An unassigned/unknown identity still needs its
own retained counted copy. No allocator boundary or raw CRT adoption is involved.

Journal memory, worker input/result/ledger and native tails, opaque Core Promise/
token/task/diagnostic ownership, host file/provider/kernel state and caller
buffers are additional. There is no claim of a hard aggregate process byte cap.

## Reads and lifetime

Read copies one committed key's metadata and canonical wire under the model gate
into caller storage without allocation. Required changes on DONE/LIMIT only;
View only on DONE, with Wire pointing into caller Buffer. EMPTY preserves both.
A sizing query and later copy can observe removal between them. Source identity
is a stable counted borrowed string until Destroy. Info's model/worker snapshots
are sampled separately, not one linearizable pair. Pointer alias validation can
visit owned entries when an output lies inside their conservative address range.
Neither Read nor a key is an application delivery claim/lease.

Model publication invokes no backing allocator callback. Old entries and the
metadata plan are freed outside the model gate after complete publication. The
public backing allocator Free callback may inspect Info/Read/Close; pending
payload credits remain charged until actual destruction returns. Results resolve
after that retirement/refund. A real custom-allocator regression exercises large
payload frees and reads the newly committed state from the callback on both
protocol levels. Constructor and serialized Destroy lifetime requirements remain.

No user diagnostic/completion callback runs under the model gate. Committed
state is visible before synchronous result watches. Reentrant Info/Read/Close/
Append are supported; same-pool and every enclosing admission/callback Wait or
Destroy are rejected by the worker's existing scope stack. A blocked observer
still owns a physical native tail even after its Future is logically terminal.

Close prevents new/queued IO and does not interrupt active IO. Wait requires
Close and retires extension native DataDrop and terminal task Future ownership.
Destroy releases model, scratch and transferred Journal after that barrier.
Serialize Destroy with API calls; stop readers first. Held result Futures keep
only the inert worker credit ledger and immutable input, surviving all live
owners. The host still closes its file/provider and joins/destroys its own pool;
remaining internal executor bookkeeping belongs to that host join. Keep Core/
extension callback/destructor code resident through all retained results/tails.

## Qualification scope and costs

The [real-file example](../examples/store_write/README.md) commits a recovered
application Inbox ACK and verifies fresh replay after physical shutdown. Tests
separate adversarial histories, quotas/OOM, observer/native tails, actual post-sync
error-key failures, concurrent reads/producers and complete file failure models.
Independent literal Python bytes and process-cut checks qualify storage only.
Power loss, file namespace publication/locking and a live durable Client are
separate requirements.

`xmqtt_store_write` performance profiles compare the same native two-sync
256-byte accept-plus-application-ACK transactions through an opaque worker or
this semantic owner. Setup and a real resumptive OPEN/native task initialization
are outside timing. Both include native encoding, job calloc zeroing, input copy,
secure wipe, Core allocations/tasks/retries/result release and extension retirement.
The semantic mode adds native decoding, legal checks, touched metadata/hash staging,
new-wire copy and atomic installation. There is no hidden whole-State image or
zero-overhead claim. Closed-loop shared-host diagnostics do not qualify fixed-
arrival throughput/tails, product parity or native Windows/Linux72h operation.

The [durable Inbox](store_inbox.md) supplies application delivery claims on an
explicit Writer mode. The developing numeric TCP Client uses the Writer for
receive ACKs. Outgoing durability, transport fencing, namespace and locking,
compaction, group commit and four-product production acceptance remain open.

The [dated revalidation](../../../docs/development/xmqtt-store-write-revalidation-20261118.json)
records final Windows/WSL feature,legacy,delivery,ABI,process-cut and cross-OS
checks,WSL ASan/UBSan/leaks and Clang21 TSan. Its matched benchmarks use a256-byte
payload (401 encoded delta bytes in this workload),not a256-byte transaction.
Actual allocation costs are reported separately; diagnostic noise failures are
retained and do not qualify production throughput or tails.
