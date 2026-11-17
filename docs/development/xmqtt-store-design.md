# MQTT durable storage execution plan

Status2026-11-17: bounded Session images, the journal, typed delta codec and
committed-state recovery applier and direct disconnected Session import are
implemented and locally verified separately.
The complete file store, live transaction driver, durable Inbox/lease API and
full recovery integration below remain required work. This plan preserves the original
four-product objective and does not label a protocol checkpoint as durable MQTT.

The developing `xmqtt_journal` now supplies a bounded, versioned transaction
envelope over an exclusively owned, already durably published borrowed `xfile`.
Data sync precedes both latest commit witnesses and publication sync. Recovery
checks committed history before suffix retirement; no silent older fallback.
This primitive uses public file/VFS/CRC APIs and opaque schema-tagged payloads;
it does not implement the MQTT delta schema, file namespace/lock policy,
asynchronous driver, compaction, group commit or durable Inbox. Public anchored
rename/creation does not yet supply a complete directory publication durability
contract. Resolve that reproduced boundary in the full store before claiming
creation/replacement survives power loss; do not infer it from file flush alone.

The developing `xmqtt_store_delta` now defines/validates the version1 MQTT-aware
batch payload (schema `0x584d4401`) over that journal: identity/open/end/clear,
outgoing PUT/attempt/PUBREL/completion, incoming acceptance/PUBREL/completion and
independent application Inbox ACK. Incoming acceptance represents payload plus
QoS2 identity in one record; protocol reset/completion must not discard accepted
delivery payloads. Epochs and nonwrapping flow/delivery keys are explicit, with
native MQTT5/311 packet validation and connection-only aliases forbidden.
This codec is not a state applier: legal transition/key/clock order, stale-id
protection, authenticated broker decisions, expiry rebasing, delivery leases,
asynchronous queue/physical retirement and the Client ACK path still require
implementation. Batches allow later bounded group commit; a driver must never
move a required durable ACK boundary past network output to fill a group.

The delta prototype reproduced four notifications of an already existing error
on one successful batch encode when restoring it through `xrtSetErrorTake`.
Core now exposes checked, quiet `xrtTrySwapErrorTake` with native TLS-failure and
ownership tests; this adds no runtime/state. The codec and journal use it. Journal
entry failure precedes I/O; exit failure leaves caller outputs untouched and
poisons an existing owner. If both append barriers completed, Info still records
COMMITTED and the advanced sequence/end despite the local ERROR. The future
driver must preserve that physical fact, reopen/recover before reuse and withhold
MQTT output until the durable state is usable; a generic local ERROR never proves
NOT_COMMITTED. Native Windows/WSL file fixtures and fresh-process recovery cover
that case. Client/Redis TLS host inference now uses the minimal public counted
`xrtNetAddrTryParse` over Core's existing private pure parser, avoiding internal
DNS-probe diagnostics and error-slot exchanges entirely. It does not create a
runtime or change the Host Dial resolver contract. RPC/Cron restoration still
needs operation-specific adoption; callbacks, promises, accepted executor work
and installed revisions may already have effects when restoration fails. These
remaining paths are not covered by the codec/journal/TLS inference fixes.

The developing `xmqtt_store_state` now applies the complete typed committed
prefix into immutable offline outgoing/QoS2-identity/Inbox views. It checks the
actual journal descriptor and original cursor, legal record order and monotonic
epoch/key/clock transitions, and enforces recovery memory/work limits. No partial
candidate is published before journal EMPTY; accepted application payloads survive
protocol reset/completion/expiry until their recorded INBOX_ACK. Native process
cuts and independent Python bytes verify the real application of recovered state,
not just delta decoding. Recovered views are not connected Sessions or delivery
leases. Direct Session import, live prepare/commit/publish ordering, worker
tickets, physical retirement, namespace/compaction and group scheduling remain
the next required integration steps. No new Core API/runtime was added.

## State and ownership

- One store owns one broker/security-principal/logical-slot binding, immutable
  identity, format version and nonwrapping storage generation/transaction sequence.
  Session images already require exact binding; their CRC provides neither
  authentication nor rollback protection. Protect storage and validate sequence.
- Persist canonical full-topic outgoing QoS1/2 and their original identifiers,
  attempt/PUBREC/PUBREL decisions; exclude connection-only aliases/flow windows.
  Persist committed incoming identities together with accepted payload/delivery
  records. Subscription intents and application outbox policy remain explicit.
- The current Session image is a cold checkpoint under the passive owner's gate.
  It rejects Client claims. A live driver needs bounded immutable transaction
  inputs and owner tickets; it must not copy the whole Session or perform file
  synchronization in a network callback. Reuse the host Engine/pool and existing
  native ownership, not a new general shared runtime.
- Retained records/results/commit tickets hold physical byte/object/identifier
  credits through their actual storage/native tail. Close/timeout cannot hide
  an in-flight write. Library/callback/backend code remains live through join.

## Protocol commit boundaries

| Transition | Required ordering |
| --- | --- |
|New outgoing QoS1/2|Durably commit canonical packet/id before first possible network output. Keep original API-entry queue/storage/network deadline.|
|Accepted PUBREC|Commit the PUBREL state before emitting PUBREL; restart must not revert it to PUBLISH.|
|PUBACK/PUBCOMP|Order durable completion/removal and any reuse of the same id in one sequenced stream. A late completion must not remove a newer record.|
|Incoming QoS1|Commit accepted payload/delivery before PUBACK; duplicate application effects remain an application concern.|
|Incoming QoS2|Commit payload/delivery plus receive identity atomically before PUBREC; preserve deduplication through PUBREL/PUBCOMP.|
|Application delivery acknowledgement|Provide an explicit durable delivery lease/completion boundary. Plain memory Release does not attest completed business effects.|
|New authenticated broker session|Apply actual CONNACK/proof/SessionPresent and existing reset policy before advertising recovered readiness; never fabricate an old Future outcome.|

Local cancellation before commit/output can have no effect. Once storage or
network output may have happened, report its actual uncertainty and retain
physical duties. Do not silently turn old requests into new application publishes.
MQTT protocol retransmission is distinct from application retry/business deduplication.

## Backend and recovery

- Use explicit checksummed, bounded, sequenced transaction records and commit
  boundaries, with complete writes followed by the documented native file-sync
  barrier before publishing commit success. Prototype public XRT file/anchored
  directory/rename/sync/retirement capabilities first; add Core only for a
  reproduced gap. Do not substitute CRT flush for durable synchronization.
- Replay only complete committed transactions. A proven incomplete final
  transaction can be abandoned; corruption of committed data is a typed failure,
  never silent empty-state fallback. Bound image/record lengths, nesting/counts,
  replay work, active bytes, queued commits and all retained generations.
- Closed finite expiry consumes offline time on a trusted wall clock and rebases
  to the new monotonic epoch; rollback is explicit. A live-at-checkpoint Session
  has unknown disconnect time and must await a fresh broker decision. The current
  image codec implements those state rules without restoring connection readiness.
- Compaction needs a sequenced cut and immutable references, bounded concurrent
  old/new storage and explicit final publication/sync. Count its copies, scans,
  allocations, I/O and pauses. Group commit has count/byte/maximum-wait limits;
  latency mode must not wait merely to fill a group.

## Required evidence

Extend the existing real-broker ACK-cut/image tests with a durable Inbox and
actual file backend. Inject each append/partial-write/sync/commit/publication/
quota/OOM/cancel/close point. Kill child processes at explicit barriers and
recover with a fresh process/Engine/store. Cover corrupt/unsupported/truncated
records, stale generation, wrong binding, assigned identity, clock rollback,
expiry and compaction with retained references. Process kill is not proof of
power-loss behavior; record native backend/filesystem/device assumptions.

Keep volatile and durable workloads separate. Measure queue-inclusive fixed-
arrival throughput/tails/CPU/memory/copies/synchronization/syscalls/recovery with
equivalent broker/QoS/TLS/sync policy and pinned comparators. Existing shared-host
image microbenchmarks are diagnostics. Native Windows/Linux >=72h mixed load and
the original Redis/RPC/Cron and four-product release gates remain outstanding.


## Disconnected Session import delivered (2026-11-17)

`xmqtt_store_session` now imports an immutable complete-prefix State into one
independent ordinary Session. Exact slot/generation/version/bound identity,
additional trusted-wall downtime, new local resource/codec limits and checked
quiet error restoration precede publication. Explicit MQTT5 assigned identity
is a separate operation. Outgoing id/order/attempt/PUBREL and incoming QoS2
identity stages survive; connection generation/readiness/aliases/old Futures
are not reconstructed. Independent Inbox payloads remain with State.

The private packet/index/expiry construction is shared with image restore.
Store import uses one native parse, one retained wire copy and descriptor
rebasing per outgoing, plus one Session and at most one bounded scratch
allocation. It avoids serializing a whole intermediate image and its CRC
passes. These remain measured cold-path costs, not a zero-overhead claim.

The actual Client/Engine literal-peer and separate real-file passive tests
qualify only this offline bridge. A host must retire the old transport owner;
the bridge cannot fence a live Client or persist new ACK/completion transitions.
The following phase still requires bounded live prepare/commit/network-output
ordering, delivery leases and physical retirement before reopening storage.
See [stage evidence](xmqtt-store-session-20261117.json). Earlier prototype
paragraphs above retain their historical scope; RPC/Cron error-restoration
followups have separate dated commits and do not supply this live driver.


## Bounded host-pool journal owner delivered (2026-11-17)

`xmqtt_journal_worker` now consumes one exclusive READY Journal and submits
opaque transactions to a borrowed XRT task pool. Request/object/byte/task/turn
budgets include real ownership tails; every result preserves local status and
actual append effect independently. A32-admission lifetime pump turn prevents
callback replenishment from monopolizing one host thread indefinitely. Native
file operations are serialized off the submitting thread. Cancellation/timeout/
Close do not interrupt running IO or authorize blind retries. Physical extension
retirement includes DataDrop and terminal native Future destruction; the host
still joins its own remaining executor bookkeeping. Results can outlive all
live owners with only an inert ledger and immutable copied bytes.

This component validates no MQTT state transitions and emits no network ACK.
The real-file example appends a recovered Inbox application ACK and verifies it
through fresh typed recovery. That is storage qualification, not a live durable
Client driver. The next phase still needs the bounded mutable semantic owner,
prepare/commit/network-output ordering, delivery leases and old-transport fencing.
Namespace publication/locking, compaction/group commit and controlled/native72h
acceptance also remain open. See [stage evidence](xmqtt-journal-worker-20261117.json).

## 2026-11-18: typed mutable store on the borrowed host pool

The developing `xmqtt_store_write` now consumes a matching recovered State and
READY Journal and applies encoded legal transactions on the existing bounded
host-pool worker. Decode and resource preparation precede file IO. A touched-entry
metadata overlay retains existing wires, stages only new payloads, and installs
the complete model before result/watch publication when the physical result is
COMMITTED, including late local failures. INVALID and normal limits refuse before
append without changing the committed model. No Core API/runtime/thread is added.

Two reproduced gaps were fixed: metadata scratch has a separate reserved budget
so a model recovered at its exact payload-memory peak can still ACK/free entries;
and backing allocator Free callbacks run outside the model gate after complete
publication, with payload credits retained until physical destruction returns.
A late Close from that actual callback preserves COMMITTED and is sampled before
result resolution. Reads copy committed canonical bytes without allocating.

Windows final verification passes20 modular/single/example programs,20 separate
static/shared clients/examples/declarations,root/statistics-off,three-form ABI,
eight exports/seventeen hidden helpers,and trimmed single/static/shared delivery.
Coverage includes44 illegal histories,22 quota refusals,3 constructor and6 native
preparation real allocation-prefix failures,4 actual post-sync native error-key
failures,448 storage-failure models/integration program,and128 typed commits with
four producers/two allocation-free readers/four host workers. Independent literal
Python bytes and84 real native process cuts across six forms/two protocol levels
pass. Legacy41-program Windows results remain applicable by real feature-closure
CPP equivalence,with only one exact free-statement line join normalized in State
and import selections; raw hashes and the initial failed comparison are retained.

Final Linux revalidation is pending: an external `wsl --manage Ubuntu --move`
operation prevents WSL startup. The submitted final Linux suite is retained and
must be observed,not duplicated; do not stop that migration. Earlier19-program
WSL ASan/UBSan/leak and Clang21 TSan successes precede the callback-retirement fix
and are historical evidence only. Final Linux unit/sanitizer/delivery/ABI/trim/
legacy/process-cut,cross-OS and local-ext4 performance checks remain required.
Windows diagnostic performance measurement is also pending at this checkpoint;
no production throughput/tail/CPU or allocator-cost result is claimed here.

Strict release still rejects27 developing MQTT modules. This is a storage owner,
not the live Client prepare/commit/network-ACK driver. Delivery leases,transport
fencing,namespace publication/locking,compaction/group commit,the remaining four-
product scope,and controlled/native Windows+Linux >=72h acceptance remain open.
Work stays on `D:/GIT/xrt` master; no branch/worktree operation or remote push.
See [stage evidence](xmqtt-store-write-20261118.json) and the
[component guide](../../extlibs/xmqtt/docs/store_write.md).

### 2026-11-18 follow-up: final Linux and cross-OS revalidation

The external WSL storage migration completed without intervention here. The
already-queued final suite ran once after startup and passes20 programs with
ASan/UBSan/leak checks. Clang21 TSan concurrency,41 legacy regression programs,
20 static/shared clients/examples/declarations,root/statistics-off,three-form ABI,
eight exports/seventeen hidden helpers,and trimmed delivery pass. Both Windows
and WSL now pass84 killed-child recovery cases plus12 opposite-OS file readers
per platform. This supersedes the pending-Linux status at the earlier Windows
checkpoint; that immutable checkpoint and historical failures remain retained.

Windows completed one matched native two-sync diagnostic cohort with five
repetitions and one warmup per mode,with27 metric noise-gate failures.
Linux attempted the same cohort on local ext4; all five baseline repetitions
and the semantic warmup completed,but the first semantic measured repetition
hit the original single-run watchdog. No completed Linux timing report or
median is inferred from that failed attempt. The timeout and unchanged3% MAD/
6% central-span gates are retained; no timing cohort was repeated for green.
Actual100-operation allocation probes ran separately on both OSes. Windows timing
straddled external VHD migration and concurrent compiler activity; it cannot
establish a causal throughput comparison. Linux attempted timing on local ext4,after
other stage compiler jobs completed. Actual allocation probes separately count
Core/extension heap blocks/bytes,tasks and overloads. No zero-overhead,controlled
performance,physical-power-loss or native Windows/native Linux72h acceptance is
claimed. The live Client prepare/commit/network-ACK driver,delivery leases,
transport fencing,namespace/locking,compaction/group commit and the original
four-product production scope remain required. All work stays on master.
See [follow-up evidence](xmqtt-store-write-revalidation-20261118.json).


## 2026-11-18: first live Client storage checkpoint integration

A private developing path now joins the actual numeric TCP Client to the bounded
store Writer. Accepted CONNACK stages BIND/OPEN before Session mutation and Ready;
actual native close captures wall time and writes END. The original setup deadline
includes storage. Late Close/timeout preserve the physical IO duty and committed
history; Writer ownership lasts until Client destruction. Synchronous Ready
observers and callbacks on the storage pool cannot cause premature retirement or
same-pool blocking joins. No Core API, executor, thread or shared runtime is added.

The integration fixture runs16 scenarios per program across MQTT3.1.1 and5.0,
including a native temporary file and fresh recovery, held final sync, late Close,
setup timeout, blocked Ready observer, native write refusal, trusted-clock rollback,
closed host-pool submission, and an actual native allocation-budget rejection.
Windows and WSL modular/single, root/statistics-off, ASan/UBSan/leaks and Clang21
TSan are recorded in the stage evidence. Existing Client/Session/Writer regressions
and standard static/shared public consumers are recorded separately. Package
consumer tests do not exercise the hidden private connection factory.

No public durable Client factory is exposed. The private bootstrap refuses all
application traffic until outgoing/incoming ACK ordering, durable delivery leases,
old-transport fencing and all auth/transport adapters are implemented. The host
Engine and storage pool must remain running through Client retirement. Native
Future-watch registration failure fallback and END clock failure are not directly
fault-injected in this stage. Enabling the developing integration adds8 bytes to
the private Client and24 to the private Writer on both tested x64 platforms;
the four sampled public result/info layouts remain unchanged. This is neither a
zero-overhead claim nor a throughput/tail-latency or production acceptance result.
Strict release correctly refuses28 developing MQTT modules. Prior performance
noise/watchdog failures, namespace/locking, compaction/group commit, full four-
product scope and native Windows/native Linux >=72h qualification remain open.
All development and tests use D:/GIT/xrt master; no branch/worktree action or push.
See [stage evidence](xmqtt-client-store-open-20261118.json).


## 2026-11-18: live receive commit and ACK ordering

The developing numeric TCP Client now persists each accepted PUBLISH before ACK
on MQTT 5.0 and 3.1.1. QoS 2 duplicates reuse committed receive identities.
PUBREL and identifier retirement commit atomically before PUBCOMP. The delivery
remains in the durable store until explicit application acknowledgement. The
next connection can repeat the appropriate PUBCOMP for a lost response and
reuse the retired packet identifier. This follows the broker retry ordering in
[MQTT 5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html).

The live fixture checks literal packets at the TCP peer and stops actual file
operations at the final sync. It verifies no premature ACK, late Close across
both acceptance and retirement, native file recovery, fresh Session import and
resumption, MQTT 5 alias canonicalization, storage refusal and full Inbox refusal.
One transaction per received message or known PUBREL is the current policy.
The private driver still needs durable leases, outgoing flows, and the full
transport and authentication integration. Controlled throughput and tail latency
and the native 72-hour acceptance remain outstanding.
See [stage evidence](xmqtt-client-store-receive-20261118.json).


## 2026-11-18 Durable application delivery leases

The Writer now offers an explicit lease mode for MQTT 5.0 and 3.1.1. Its ready
queue grows only with committed deliveries and shares the existing MaxInbox
bound. Application buffers own the copied bytes. An opaque value fences stale
claims and earlier Writer instances without holding the file, pool or Writer alive.
An accepted ACK consumes the claim and publishes its actual durable effect. A
definite non-commit makes the record eligible for another claim. Recovery restores
unacknowledged deliveries in acceptance order.

The same Writer sequences application ACKs and live protocol transitions. The
fixture holds an application ACK at its final file sync and queues an incoming
PUBREL on the actual TCP Client. The application effect commits first. PUBCOMP
follows independent receive-identity retirement. Raw Inbox ACK deltas remain
available on the legacy Writer and cannot bypass leases in the explicit mode.

See the native file example and stage evidence. Outgoing durability, full
transport and authentication integration, namespace and locking, compaction,
controlled throughput and tail latency, and native 72-hour acceptance remain open.
[Stage evidence](xmqtt-store-inbox-20261118.json).
