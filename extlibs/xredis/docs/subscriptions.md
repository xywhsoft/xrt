# Desired subscriptions and bounded replacement replay

The developing `xredis_subscriptions` leaf stores local intent independently of
Client/Engine ownership, and restores a captured revision onto a fresh ready
connection. It introduces no Core API, private worker, timer or shared runtime.
Automatic connection factories/backoff and Cluster/Sentinel routing are later
controllers; this stage supplies their actual desired-state/replay component.

Apply accepts the six subscribe/unsubscribe commands, with binary/empty names and
family-local unsubscribe-all. Duplicates and absent removals are idempotent. A
semantic change advances revision; no-op preserves it, and revision never wraps.
The configured distinct-name, stored-name-byte and per-edit input-byte limits are
independent. One constructor runs outside Gate; another edit gets OVERLOADED.
Failure publishes nothing and preserves the revision output. Editing is local
intent, without network or persistence acknowledgement. A cancelled wire call
never implicitly rolls back a committed desired edit.

Immutable snapshots own pointer vectors and reference-counted immutable entries.
Acquire/retain is O(1), with no heap or payload copy. Cold edits sort/deduplicate
input, merge pointer vectors and retain unchanged entries; only newly added names
are copied. Costs include O(current entries + edit sorting), pointer-vector
allocation and per-entry ref operations; this is not zero cold-path overhead.
Stored-name limits bound the current set. One constructor's scratch/new snapshot
and additions are bounded too. Caller-retained old snapshots own their pinned old
entry allocations; do not sum shared ReferencedEntryBytes as unique physical RSS.
SnapshotBytes and ReferencedEntryBytes describe the current immutable publication,
not all externally retained results. They outlive Store and every network owner.

ReplayCreate captures a revision and retains Store/snapshot/Client, atomically
installs a private control token while dedicating the fresh ready connection.
Both native request admission gates reject other control scopes. No token or
Replay pointer is stored in accepted requests. Failure consumes no caller duty.
Step submits or observes at most one batch. Existing native control requests
still own copied wire, ACK correlation, quota, cancel/timeout and physical tails.
The original Entered/Timeout pair is passed directly to native admission: later
batches do not receive a fresh timeout. Caller scheduling delay counts too.

MORE requires a subsequent scheduled turn, including a short physical retirement
tail. WAIT exposes the actual pending native Future, with no wrapper Future.
Use a Watch asynchronously or wait only outside Engine workers. Result records
validated ACKs actually observed by Step, the captured revision and separate
regular/shard counts. ErrorReply/Cause borrow a retained failed native Future
through Destroy. NativeStatus is its observed failure or a corresponding local
stop class; a retained native observer can have a later independent outcome.

A desired revision change returns STALE and closes the partial connection.
Cancellation, timeout, overload, protocol/server/transport/memory failures also
retire it; it is never exposed as successfully restored. Terminal results remain
immutable. OK releases the control claim and leaves the Client subscribed to the
captured revision. Later edits need a new reconciliation decision; OK does not
mean the set can never become stale. Destroy aborts unfinished work without
waiting; native wire owns its copies and cannot retain Replay storage. The caller
keeps its original Client close/reference duty and serializes all Replay APIs.

Shard names cache their normative CRC16/XMODEM slot, with the first-pair hash-tag
rule, when edited. A shard batch contains only one slot. Recovery does not rescan
those payloads for slot selection. Endpoint choice and MOVED redirection remain
with the caller/future Cluster controller; this is not Cluster routing support.

[Redis Pub/Sub](https://redis.io/docs/latest/develop/pubsub/) is at-most-once.
Restoring subscriptions does not recover messages missed while disconnected or
provide business exactly-once delivery. See [SSUBSCRIBE](https://redis.io/docs/latest/commands/ssubscribe/)
and the [cluster specification](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/)
for shard-slot rules. Implemented from protocol facts, not upstream client code.

The [offline example](../examples/subscriptions/README.md) demonstrates every new
public API. Independent set-model, concurrency, snapshot/OOM/lifetime, stale/
cancel/error/limits, CRC oracle, exact-deadline and TLS tests live separately from
production. Real-server/package/performance evidence is recorded in the stage
report; the full four-library production and >=72h acceptance remain incomplete.

Benchmark snapshot latency samples cover a whole batch of 32 snapshot/info/view/release operations; their percentiles are not per-operation tails. Edit latency samples cover one edit plus one snapshot verification. Throughput counts operations.
