# Committed MQTT state recovery

Status: developing. Select `XMQTT_MODULE_XMQTT_STORE_STATE`. This is the actual
cross-record applier for the typed journal: recover an immutable offline view of
outgoing QoS obligations, incoming QoS2 identities and pending Inbox payloads.
Live Client storage/ACK integration, Session import, application delivery leases,
namespace publication/locking, compaction and bounded group commit remain required.

`xmqttStoreStateRecover` requires an unconsumed Journal with the exact MQTT level,
typed payload schema, broker/principal/slot binding and storage generation. It
consumes each committed transaction, validates typed bodies and legal transitions,
and stages all owned state until journal EMPTY verifies the complete prefix.
Later semantic corruption cannot publish earlier payloads. Errors release the
whole candidate; output remains unchanged. Reopen the Journal before retry after
any consumption. Storage suffix retirement may already have changed the file.

The first record is BIND; identity is immutable. An explicit expected ClientId
must match. Empty expected identity permits MQTT5 assigned-id recovery, not311.
First OPEN shares BIND's nonzero epoch. A resumed OPEN retains the epoch and
requires available local state; each subsequent new broker-session OPEN advances
exactly one epoch. Epochs and strictly increasing global flow/delivery keys never
wrap. Resets cannot revive an old key, and completions must match key, original
epoch, MQTT identifier, QoS and stage. A311 lost-server-state mismatch requires
an explicit offline reset or clean start, matching the existing Session policy.
OPEN records attest a decision the live driver authenticated; file CRC/binding
alone do not authenticate a broker or prevent storage forgery/rollback.

New outgoing canonical PUBLISH records retain attempt metadata. An attempt is
required before a correlated ACK. Successful PUBREC enters PUBREL permanently;
only PUBCOMP completes that stage. QoS1 completes with PUBACK; a negative PUBREC
can complete a QoS2 PUBLISH. Incoming acceptance owns payload plus QoS2 identity
together. PUBREL progresses that identity; complete PUBCOMP retires only protocol
state. Independent INBOX_ACK removes the application delivery, including from an
older epoch after protocol reset. Early application ACK retains the receive
identity and its allocation until protocol completion. No ACK is sent here.

END validates effective expiry changes and records actual disconnect wall time.
Closed expiry consumes elapsed milliseconds, including downtime. A live-at-cut
record has no known actual disconnect time: it retains a candidate until a new
authenticated broker decision rather than inventing an expiry start. Zero-expiry
protocol state is discarded. CLEAR is offline only; expiry CLEAR needs actual
elapsed finite expiry. Protocol clearing never removes accepted pending Inbox
payloads. Application completion remains explicit; plain memory release does
not attest business effects. These client policies follow
[MQTT5 session/QoS rules](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT311 persistence](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).

Count, retained bytes, per-transaction syntax limits and total replay transaction,
operation and payload-byte limits all apply. MaxMemoryBytes includes fixed hash/
index tables, identity, every retained entry/wire allocation and reusable operation/
property scratch. Scratch is released before publication. The caller's separately
bounded Journal, file buffers and allocator/CRT/kernel overhead are additional.
Info reports current/base/peak requested bytes. Work is synchronous storage-owner
work; never call it inside a network callback. Independent immutable getters need
a stable State lifetime; stop readers before unique-owner Destroy.

The applier copies each retained packet once from the Journal borrow. It does not
copy the whole Session per transaction. Key/id matching uses bounded hash tables;
final ordered view indices give direct record access. Getters first check an
immutable address envelope: ordinary stack outputs outside it need no allocation
scan; outputs inside it scan owned allocations to reject aliases. Hash collisions,
reset scans, final indexing, allocator costs and codec/journal scans remain real.
No throughput, zero-overhead or full durable-Client claim is made.

Run `python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite
xmqtt_store_state_tests --cflag=-O2`. The separate adversarial/OOM suite exercises
illegal histories, bounded resources, late-prefix failures and actual allocator
rejections. The [example](../examples/store_state/README.md) recovers accepted
payload after protocol reset. Performance workloads must count journal reads,
validation, owned copies, reset/indexing and storage retirement separately from
network and physical-sync latency; production/native72h acceptance remains open.

Evidence also covers assigned-id/version refusal before cursor consumption,
resumed live/closed epochs, early Inbox ACK while QoS2 identity stays active,
independent QoS1 deliveries, all48 public-read boundaries, final suffix resize/
sync failure, eight immutable readers with physical joins, WSL ASan/UBSan/leaks
and a separate TSan run. Windows/WSL each kill84 real child processes at the
seven journal write/sync barriers across two versions and six delivery forms;
fresh processes apply the state and independent Python bytes match exactly.
Process termination is not evidence of physical power-loss survival.
