# In-memory session protocol state (developing)

`xmqtt_session` implements caller-driven MQTT5/3.1.1 session state using public
XRT codecs, allocation, atomics and mutexes. It owns no connection, Engine,
background retry timer, credentials, application receive queue or disk store.
The optional [Client Session factory](client_session.md) attaches it to numeric
TCP with explicit reconnect and connection-scoped Futures. Automatic reconnect,
TLS/Dial/Auth composition and durable/crash recovery remain required.
The [example](../examples/session/README.md) exercises the state boundary directly.

When driven by the optional Client Session factory, Objects/LiveBytes also count
unaliased publication snapshots reserved before CONNACK/output. Those snapshots
do not enter ActiveOutgoing or survive as protocol state until first selection.
Pending encoding runs outside the gate; physical count/bytes/id reservation stays
charged through allocation failure cleanup or actual last-reference retirement.
The standalone public Publish/Open/Next contract is unchanged.

The implementation follows OASIS [MQTT5 sections4.1/4.3/4.4/4.9](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT3.1.1 plus Errata01 sections3.2.2/4.1/4.4](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html).
Only unacknowledged outgoing QoS1/2 and incoming QoS2 identities survive a resumed
connection. New-generation send quota is reset; aliases are removed from owned
outgoing snapshots. QoS2 resumes PUBREL after successful PUBREC. MQTT5 missing
server state discards old local state; a311 mismatch fails closed until explicit
offline reset. This policy avoids treating an unrelated new session as recovery.

## Ownership and driving

Create with a nonempty ClientId. The application binds that object to a logical
broker/security principal, serializes mutating calls, and authenticates its
connection before Open. Open validates CONNECT/CONNACK and client identity. It
returns DONE or RESET for accepted state, REJECTED for a broker rejection and
typed errors without implicitly accepting an unsupported session. Borrowed
negotiation text follows the existing negotiation contract; retain those inputs
while using those text views. The session copies its own identity and stores
only scalar peer capabilities, never authentication text or credentials.

Publish copies a validated complete logical-topic packet once into an immutable
object. It strips Topic Alias rather than preserving a connection mapping.
Identifier0 assigns an available id. The caller gets a retained packet reference;
the active flow owns another. Next gives one additional retained frame reference
and holds its own selection until Sent/End. No other frame is selected while
that selection is outstanding. Send Prefix then Tail without interleaving and
report Sent only after the complete frame is accepted by the transport. End is
required after any abandoned/partial output. An exposed PUBLISH is conservatively
retried with DUP even if its first transport accepted no bytes.

Each successful Next issues a distinct `(Generation, Selection)` token. This
distinguishes repeated PUBREL frames even when their packet, identifier and
immutable wire spans are identical. Sent rejects a token from an earlier
selection without finishing the current one or releasing its selection reference.
Selection numbers do not wrap: exhausting uint64 returns RESTRICTED without
changing output or ready/quota state. A later accepted Open starts a new
generation and restarts selection numbering. Old frame references remain readable.
This appends Selection to the developing frame ABI (64 to72 bytes on x64);
rebuild consumers. The existing public field offsets remain unchanged.

ACKs correlate to live sent flows. PUBREC transitions to PUBREL, duplicate PUBREC
coalesces another PUBREL, and final/negative acknowledgements retire the flow.
Unmatched PUBCOMP never completes a new flow or creates send credit. The state
module stores scalar decisions; it does not copy ACK properties into results.
The caller must maintain request/Future semantics and validate whole inbound
packets and authentication/problem-information constraints before applying them.

End records actual network termination and the last fully sent DISCONNECT, if
any. Monotonic times are explicit seconds. Finite expiry starts at End. The
owner drives Expire or the next Open; no timer runs offline and expiry never
clears connected state. Expiry uses elapsed seconds; ExpiresAt is a floating-point
display deadline, so rounding a large clock cannot cause immediate expiration.
UINT32_MAX/311 persistent state has no local expiry.
Reset is an explicit offline administrative termination. Destroy also requires
offline state and stopped concurrent Session entries.

Held packet/frame data remain immutable after ACK, End, Reset and Destroy.
Identifiers, object count and bytes stay reserved through final physical release,
so stale writer references cannot be hidden by a logical completion. Held packets
pin only the bounded state allocation and library/allocator code, never an Engine.
Release frame references after the actual transport no longer borrows their data.

## Incoming delivery boundary

The owner validates wire/aliases and connection Receive Maximum, and prepares
application delivery resources before ReceivePrepare. DONE reserves a new id;
DUPLICATE suppresses another application delivery. Commit is allocation-free.
Commit before publishing the already prepared local queue entry and sending
PUBREC. Abort is valid only for a prepared, invisible delivery. End rolls back
uncommitted prepared ids. Committed ids survive resumption, including the interval
between PUBREL and full PUBCOMP emission. ReceiveComplete removes them afterward.
Unknown PUBREL still receives a PUBCOMP with the version-specific reason.

This module stores identity, not receive payload. Its volatile contract assumes
the owner actually accepted delivery at Commit. Durable application delivery
requires a store transaction and acknowledgement ordering, which is separate
pending work. No arbitrary business exactly-once guarantee is inferred.

## Resource and performance contract

Default limits are128 outgoing objects,64 incoming QoS2 ids and16MiB of owned
bytes including metadata. Create allocates a fixed state block, an outgoing id
bitmap/hash table (when enabled), a packed two-bit incoming map (when enabled),
and copied identity. Successful incoming transitions and ACK/frame/replay paths
allocate nothing. Each Publish creates one object containing descriptors and
canonical wire; packet Size/Write/Read validation and one payload copy are real
costs. An unused descriptor slot can remain after alias removal and is charged.
The selection guard adds one uint64 to fixed session state, one checked increment
per successful Next, and a token comparison at Sent. It adds no packet allocation,
payload copy or mutex acquisition. The caller's frame grows by eight bytes on x64.
No payload copy/allocation is needed to emit a retry or PUBREL.

Successfully held references have one inclusive `UINT32_MAX/2` cap shared by
active state, the selected frame, caller frames and explicit Retain calls.
Next adds two internal references with one fetch-add, checks its returned count
and refunds both on refusal before consuming ready/quota state, a selection
number or output. If two slots are unavailable it returns RESTRICTED, even when
a one-reference Retain could still succeed. Releases permit retry. Capacity may
be briefly unavailable while another Next reservation is being checked.

The protocol owner holds Gate and the ready list's active reference throughout
this reservation. There can be only one such two-reference speculation per
packet; public Retain still uses the original checked CAS. Thus a supported
internal count is at most cap+2 (far below UINT32_MAX), and concurrent releases
cannot remove the protected active reference. Refund uses a raw atomic decrement,
not PacketRelease, and cannot physically free that active packet. No over-cap
frame is published. The successful Next path uses one RMW instead of two; refusal
adds one refund RMW. There is no CAS retry loop in Next, nor any extra allocation,
copy, gate or per-object storage. These invariants depend on valid reference
ownership and the existing serialized protocol-owner contract.

List/ready queues permit constant-time admission/removal/selection; identifier
lookup uses a bounded hash and bitmap. Auto assignment scans at most65535 ids
on a fragmented bitmap. Open/reset/expiry visit retained active state; rolling
back prepared incoming ids can scan the65535-id map. These cold-path costs are
not described as zero overhead. Every state transition/snapshot takes a gate;
retained objects also use atomic references. Native/caller buffers, allocator
headers/pools and external application queues are outside logical MaxBytes.

`bench_session.c` measures in-memory validation/admission/frame/ACK cycles with
explicit version/QoS/payload. It includes allocation, one copied payload, gates
and refcount costs. Setup is outside the timer. It measures no socket, TLS,
durable store, fixed-arrival tail latency or broker performance; original release
performance and native>=72h gates remain in force.

The transport integration must preserve pending receive payload ownership across
connection destruction, separate caller cancellation/deadline outcomes from
remaining protocol duties, retain original packet ids and full logical topics,
and sequence durable store commits before network acknowledgements. Those duties
are not fulfilled merely by having this identity/state component. Existing
Client factories continue refusing an unowned retained broker session until the
complete attachment and its failure/lifetime tests are implemented.
