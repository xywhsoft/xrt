# MQTT durable storage execution plan

Status2026-11-16: the bounded Session image component is implemented and locally
verified separately. The file store, live transaction driver, durable Inbox and
crash recovery below remain required work. This plan preserves the original
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
