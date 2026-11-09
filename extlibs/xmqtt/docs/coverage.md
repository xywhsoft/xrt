# Wire contracts and pending scope

The full [service-extension target](../../../docs/development/service-extensions-plan.md)
remains required. The wire leaf implements data representations and fixed-header
framing; the properties leaf implements explicit MQTT 5.0 sections. The packet
and codec5/codec311 leaves cover shared descriptors/grammar and all MQTT 5/3.1.1
packet bodies. A separate negotiation leaf checks CONNECT/CONNACK relationships
and client per-packet capabilities; its [contracts](negotiation.md) distinguish
those pure checks from negotiated inbound/runtime state. The developing
[TCP client](client.md) integrates handshake, basic credentials, keep-alive,
QoS-0/1/2 send/receive, current-connection SUBSCRIBE/UNSUBSCRIBE, bounded ACK flow, bounded automatic MQTT5 error
DISCONNECT, bounded application graceful DISCONNECT and physical shutdown.
Optional [enhanced authentication](client_auth.md) integrates initial AUTH/final
CONNACK verification and reauthentication. Session/expiry/subscription recovery state, reconnect, persistence and TLS/WebSocket remain pending.
There is no pinned-broker or final performance/production acceptance yet.

| Contract | Verification |
| --- | --- |
| Variable integer | Canonical boundary fixtures, every 2-byte prefix for both versions, exhaustive 0..65535 round trips, nonminimal compatibility, fourth continuation error |
| Fixed header | Independent literal valid-first-byte matrix across all 256 bytes/both versions, AUTH compatibility and PUBLISH flag combinations |
| Total packet bounds | All integer-length transitions through the protocol maximum, exact cap and cap-minus-one, malformed header rejection before body buffering |
| Complete frame | Every prefix of 0/1/2/127/128/16383/16384-byte opaque bodies, borrowed views, coalesced suffix and 3.1.1 nonminimal header byte accounting |
| UTF-8 strings | Every Unicode scalar generated with an independent encoder; all 65536 two-byte patterns; controls/noncharacters, NUL, overlong/surrogate/out-of-range/truncated sequences and error offsets |
| Field representations | Maximum 65535-byte string and every input prefix; arbitrary binary, empty fields and coalesced suffix; borrowed pointers and retained BOM |
| Atomicity/API | Capacity below required size, protocol overflow, unknown config/flags, wrapped/null spans, input/config/output/size aliases and unchanged outputs |
| Adversarial | 20,000 deterministic arbitrary frames, header/frame status agreement, bounded borrowed spans and publication invariants |
| Allocation/fault | 10,000 warm primitive cycles preserve allocation counters/ambient error; successful operation under one-byte quota; diagnostic OOM leaves output unchanged and no live budget bytes |
| Distribution | Modular/single tests, real offline example, independent package consumer and dedicated performance/size profiles |

Reads publish only DONE; MORE/ERROR leave all output values unchanged. A frame's
opaque body may still be an invalid MQTT packet. Read views require the caller's
original bytes to remain alive/unchanged. No partial prefix is retained, no body
buffer is allocated, and no business callback or network action occurs here.

Success allocation counters do not claim zero CPU/scan/copy costs. The README
lists actual header re-parsing, UTF-8 decoder calls, span/alias checks and small
header/field copies. Workload smoke does not prove performance parity, queueing
latency, TLS/QoS/durability equivalence or production stability. Current platform
evidence is in the [progress ledger](../../../docs/development/service-extensions-progress.md).

The [property section contracts](properties.md) cover all standard ids/types,
scope/direction/repetition rules, local-vs-peer status, section publication,
text/data lifetimes and remaining connection-level requirements. Property tests
use literal wire fixtures and a complete scope/direction matrix, count/byte/field
boundaries, duplicate/numeric/UTF-8 failures, exact ordered mutation round trips,
atomic writes and diagnostic-OOM-independent classifications.

The [full MQTT 5 codec](codec.md) adds literal fixtures for every packet type,
all prefixes and coalesced suffixes, complete CONNECT optionals/Will/auth fields,
every CONNECT flag byte, reason byte/sender combinations and subscription option
bytes. Shared filter levels, empty-topic aliases, PFI payload NUL versus encoded-
string NUL, malformed scalar sequences, mandatory/abbreviated sections and late
failures are covered. Packet/payload/property/topic caps, CONNECT+Will aggregate
limits, maximum uint16 fields, VBI transitions, API/alias errors, all short write
capacities, stable canonical forms for 30,000 raw/mutated packets, 10,000 zero-
allocation warm cycles and own/nested diagnostic OOM are tested. Modular/single
tests, offline examples, trimming and independent static/shared consumers include
the new APIs. Packet validation itself is stateless; client interoperability,
session/QoS recovery and production performance acceptance remain required.

The [3.1.1 codec](codec311.md) has separate fourteen-type literal fixtures with
all 1..4-byte header widths, every prefix, coalescing and canonical output. All
CONNECT flag bytes, CONNACK flag/return-byte combinations, SUBACK codes and
subscription option bytes are checked. Exact fixed-body lengths reject before
buffering; filters, empty persistent ClientId, credential dependencies, reserved
AUTH, client-only DISCONNECT and rejected 5.0 producer fields are covered.
Borrowed views stay in the first packet/caller arrays. Opaque data includes NUL,
invalid UTF-8 and v5-looking bytes. Tests exercise zero/property-independent
budgets, caller capacity, uint16 field maxima, >2 MiB complete packets at the
Errata 01 four-byte transition, protocol maximum header-only bounds, wrapped
spans/aliases, atomic failures, 30,000 raw/mutated canonical forms, 10,000 warm
allocation/error-identity cycles and quota/diagnostic OOM. The single trim asserts
no properties/codec5 feature; root package consumers exercise both versions.

## Connection negotiation constraints

The negotiation leaf checks cross-packet CONNECT/CONNACK consistency and client
per-packet capabilities. Tests cover literal wire peers, every version/clean/
session-present/local-state combination, client-id assignment/lifetimes,
successful/refused authentication (including present empty methods), directional
packet/receive/alias bounds, server keep-alive/expiry overrides and response-info
request rules. All 32 capability combinations exercise independent publish/
subscribe expectations. Requested subscription QoS and unsubscribe cleanup retain
their distinct rules. Original CONNECT expiry still constrains DISCONNECT after
a server override. Output/source/config aliases, wrapped spans, unchanged outputs,
10,000 allocation/error-identity cycles, one-byte budgets and typed semantic
errors under diagnostic OOM are covered. It provides no flow-credit/alias map,
authentication sequence, session store, transport or broker interoperability.


## Application graceful DISCONNECT

The client exposes asynchronous and blocking shutdown with preencoded immutable
reason/properties, shared request/wire/object quotas, one drain/native deadline,
explicit cancellation and abortive Close interruption. The worker finishes prior
QoS-0 writes, QoS-1 PUBACK obligations and partial data/control frames first;
normal native close is distinct from any broker acknowledgement. Modular/single
fault suites and the separate stdlib peer verify reasons/expiry/caps, input
snapshot lifetime, final-frame order, fragmented ACK/data/disconnect, peer EOF,
first protocol failure, timeout/cancel/abort, admission OOM and retained results.
The developing session/store scope and pinned broker/production/performance
qualification remain required; this row does not establish those capabilities.


## QoS-2 sender

The developing TCP client now sends fresh QoS-2 PUBLISH through PUBREC/PUBREL/
PUBCOMP on MQTT 5.0 and 3.1.1. It holds send credit through PUBCOMP or negative
PUBREC; shared QoS-1/2 ids remain reserved through actual native reference
retirement. Positive duplicate receipts coalesce/repeat the immutable inline
PUBREL, with no original PUBLISH replay or timer retry. It retains both phase
ACKs, scalar decisions under metadata quota failures, and finishes a partial
repeated frame before graceful close. Unit/single tests and an independent
stdlib socket peer cover windows, negative/duplicate/late/premature/wrong-kind
ACKs, byte writes, phase timeout/cancel/abort, graceful ordering, quotas and
metadata lifetime beyond Client/Engine. A separate offline sender example is
part of the client suite. Subscriptions/shared subscription ids,
session persistence/recovery and pinned broker/native Linux/72h/performance
qualification remain required. This row does not establish those capabilities.


## Volatile QoS-2 receiver

The developing TCP client enqueues one owned message before PUBREC, tracks the
incoming id independently of application Take/Release and outgoing ids, and
returns shared receive credit only after complete PUBCOMP output. Direct-index
state suppresses duplicate PUBLISH delivery regardless of DUP; queued duplicate
ACKs coalesce, unknown/repeated PUBREL is completed without delivery and a new
PUBLISH may reuse an id after completion. A bounded control queue, native copied
output and frame-boundary rules preserve partial packet ordering and physical
retirement. Graceful shutdown also drains accepted incoming flows or reaches
its existing finite deadline/cancellation. The example and separate stdlib peer
cover both protocol versions, 66 wire cases, limits/windows, fragmented/control
and duplex traffic, metadata lifetime, reentrancy and interrupted drain. Unit
allocation-budget cases cover state allocation, message allocation and a success
control with no remaining live budget owners after destruction. The old error-
diagnostic fixture now waits for the peer observation before inspecting its
reason; local physical close cannot prove a different worker already ran.

This is volatile current-connection local enqueue, not crash-safe session or
business exactly-once processing. Persistence/reconnect/session/expiry/auth/
alias/subscription/transports, dedicated QoS-2 load/CPU/copy/allocation accounting,
pinned comparators, native Linux and >=72h production qualification remain.


## Current-connection subscription control

The developing client now admits immutable SUBSCRIBE/UNSUBSCRIBE snapshots,
including before CONNACK. Controls and QoS1/2 publications share the bounded
outgoing id bitmap/hash map and physical reservations, while controls bypass
publish Receive Maximum credit. Typed per-filter results retain ordered reasons,
properties and partial rejection counts; metadata exhaustion preserves scalar
acknowledgement decisions. ACK kind/id/count/QoS relationships and Request
Problem Information are validated against the actual sent request. Unsent
cancel/zero deadline/Close and queued graceful abort have deterministic worker
barriers; construction-budget failures refund physical reservations. The
independent byte peer covers 107 version/case combinations, including hostile
ACKs, reverse correlation, fragmented IO, caller snapshots, capabilities,
quotas, concurrent publication, duplex receive and graceful drain. The offline
subscription example documents commands/assertions/cleanup separately.

This adds packet operations on one connection, without a resubscription registry
or durable session. Subscription-specific load/CPU/copy/allocation measurements,
pinned broker/client parity, native Linux and >=72h production qualification
remain required. Existing QoS2 measurement observations remain unaccepted; this
feature does not replace or weaken those gates.


## Outgoing alias snapshots

Full-topic and explicit compact inputs snapshot the latest accepted logical
intent, separately from actual TCP packet order. The sender chooses a compact
frame only for a matching actual mapping; cancellation/timeout/flow reordering
can safely require a captured full topic. Optional metadata, sparse pages and
immutable topic owners have independent private-byte accounting and native
segment reference duties. No admission failure changes planned state. The
standalone byte peer covers 30 send cases and checks actual wire bytes against
results, alongside deterministic cancel/deadline and allocator unit cases.
This is current-connection functionality. Session/store/reconnect, detailed
alias/subscription performance and native production qualification remain.


## Enhanced MQTT5 authentication

The optional leaf covers multiple challenge rounds, method equality/empty methods,
binary data, initial/final verification gates, coalesced early publications,
reauthentication alongside ordinary traffic, one-byte output frame boundaries,
single-use cross-thread ticket lifetimes, whole/decision deadlines, cancellation,
rejection and server protocol/close failures. Allocation-prefix tests cover factory,
reauth producer/result/wire and worker message/ticket/native failures, with physical
quota retirement. Round/aggregate/private/object quotas remain distinct. An
independent stdlib socket peer validates literal protocol bytes and negative
sequences. Algorithm and pinned-broker interoperability remain required.

Deterministic completion-reservation scheduling proves that a new reauth admission
cannot replace a producer whose final proof is being completed. Independent
Keep Alive cases pause application work for multiple intervals before CONNACK,
defer server PINGRESP until CONNACK and verify heartbeats during final proof.


## Bounded hostname/prepared-address setup

The optional [Dial leaf](client_dial.md) reuses Core Resolver/Dial without changing
old public client layouts. MQTT5/3.1.1 byte writes, queued QoS1, family/fallback/
attempt limits, counted-host lifetime, prepared-list release, cancelled shared
lookup detachment, actual client callback barriers, OOM, retained outcomes and
enhanced-auth composition are covered. Winner event installation preserves a
native EOF snapshot; a deterministic red/green case prevents setup-timeout masking.
Each platform passes23 literal-byte cases through each Auth Dial factory, with
ASan/UBSan/leak and applicable TSan results in the
[stage report](../tests/results/client-dial-20261109.json). External DNS/broker
qualification, transport/session completeness and production acceptance remain.
