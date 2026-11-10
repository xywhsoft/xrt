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
CONNACK verification and reauthentication. Optional [TLS](client_tls.md) adds
verified TLS 1.2/1.3 and mutual authentication. Session/expiry/subscription recovery
state, reconnect, persistence and WebSocket remain pending.
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
[stage report](../tests/results/client-dial-20261109.json). External DNS/additional broker
qualification, remaining transports/sessions and production acceptance remain.

Completed Dial controllers now release the client's reference in terminal Done.
The private retirement diagnostic observes no completed controller on a live
connection, verifies pending ownership/cancellation and continues QoS1 traffic
after completion. It runs separately from product dependencies.

## Pinned real-broker volatile-client checks

The optional [broker harness](../tests/interop/README.md) pins Eclipse Mosquitto
2.1.2 and verifies the unmodified source archive, fixture dependencies and actual
binary identity. Its 68-case matrix covers MQTT5/311, TCP/TLS, three connection
factories, binary QoS0/1/2, subscriptions, outgoing aliases/No Local, retained
delivery/deletion, abnormal-close Will, basic authentication success/refusal,
fragmented work budgets and physical retirement. Retained messages survive actual
Client/Resolver/Engine destruction. No broker code/library enters the product.

Independent literal CONNECTs seed a retained broker session and verify Session
Present=1. The current client must explicitly refuse it with STATUS_SESSION.
These cases expose the session-recovery gap rather than qualifying replay or
durability. Enhanced-auth algorithms, recovery/expiry, other brokers and native
Windows-broker/native Linux deployment matrices remain separate work.

The [stage report](../tests/results/broker-20261109.json) records actual build
variants, sanitizer results, failed bootstrap attempts and topology. WSL broker
tests, including native Windows clients via localhost forwarding, do not establish
native deployment, independent comparator throughput/P99 or >=72h acceptance.

## In-memory session protocol state

The independent [session leaf](session.md) now owns bounded immutable outgoing
QoS1/2 snapshots, original identifiers, cross-connection PUBLISH/PUBREL state,
incoming QoS2 prepare/commit/deduplication and elapsed-time expiry. It uses no
Engine, Future, background retry timer, credential store or new shared runtime.
The optional [Client Session factory](client_session.md) now attaches it to
numeric TCP with a shared Inbox and connection-scoped Futures. The optional
[TLS Session leaf](client_session_tls.md) now composes TLS/Dial/Auth. Automatic
reconnect, plain Dial/Auth composition and durable recovery remain required.

The [stage report](../tests/results/session-20261110.json) records both platforms'
33-program full MQTT suites plus minimal session builds and the runnable example.
WSL uses ASan/UBSan/leak halt, ten additional lifecycle/id-space repetitions and
applicable TSan with eight concurrent retained readers. Tests exercise all65535
ids in both directions, six reconnect cuts per version, physical reservations,
large-clock expiry, byte/object bounds, peer restrictions and allocation failure.
Thirteen modular/single/root/leaf/static/shared/sanitized TCP matrices execute
156 independent literal-peer cases across two actual connections each. Package
consumers run on both OSes. Old8 MQTT and139 Core archive members per OS remain
byte-identical; the new session object is the only added member.

Performance data are in-memory state cycles, not broker throughput. The original
Windows100000-cycle matrix qualifies10/12 noise checks, WSL12/12. All12 Windows
cases rerun at1000000 cycles qualify11/12; a separate own-process CPU-affinity
experiment still qualifies11/12. Raw failures remain retained, original3% MAD/
6% central-range gates stay fixed, and Windows is not fully noise-qualified.
No new regression, comparator, P99, durable, native Linux or production acceptance
is inferred. The pure state size profile checks absence of Client/Engine/Future
symbols; measured public layouts and fixed/peak logical bytes are recorded.

## Explicit Client Session recovery

The [recovery stage](../tests/results/client-session-20261110.json) records37
MQTT root programs per OS, two minimal Client Session and two pure Session
programs, a runnable two-Engine recovery example, static/shared package suites,
ASan/UBSan/leak and two TSan forms. It covers QoS1/2 reconnect stages, incoming
deduplication, expiry/reset, alias expansion, native zero/partial output, early
ACKs, physical quota retirement and controlled replay allocation failure. That stage
uses two snapshots for new QoS1/2; replay borrows
retained payload. Local echo observations do not establish performance, real
broker recovery, native deployment or >=72h production acceptance.

## Shared unaliased publication snapshot

The [sharing stage](../tests/results/shared-publish-20261110.json) retains one
canonical payload copy for unaliased QoS1/2, with separate request/packet headers.
Both protocol versions and QoS levels have measured parent two-copy/current
one-copy tests; the outgoing-alias control remains two copies. Pending snapshots
consume physical Session bounds at admission without becoming recoverable state.
Tests cover early overload, cancellation, OOM rollback, concurrent producers and
closing while a copy is held outside both gates. Full root/trimmed/package and
sanitizer results are recorded. Aliased publication sharing, allocation/counter
profiling and complete performance/production qualification remain required.

## Shared aliased publication snapshot

The [alias sharing stage](../tests/results/shared-alias-20261110.json) extends
one-copy admission to aliased QoS1/2. A per-Client alias transaction mutex covers
capture through successful FIFO publication, including QoS0 bindings. Payload
encoding holds neither main gate. Full output gathers four spans; compact
output copies only small headers and gathers two. A bounded scalar fallback
handles gather pressure for shared publications. The immutable canonical packet
continues to support alias-free recovery. Request/packet lifetimes remain separate.

Tests cover property-length boundaries, ordered User Properties, one-byte writes,
synthetic one-lease refusal, cancellation, eight budget prefixes, four concurrent alias
producers, close with a holder/waiter and ordinary-producer progress. Both QoS
levels measure parent two-copy/current one-copy alias output; both protocols'
unaliased controls remain one-copy. Root/leaf/static/shared/sanitized evidence
and matched alias/ordinary observations are retained. Core queues limit total
bytes, not span count; the synthetic refusal does not establish a production
stall in legacy/QoS0 alias writes. Mutex/native/header costs,
contended performance and full production
qualification remain open; no zero-overhead or broker/native acceptance is claimed.

## Real broker Session recovery

The [broker recovery stage](../tests/results/broker-session-20261110.json) uses
the pinned unchanged Mosquitto 2.1.2 fixture through an independent frame proxy.
It cuts outgoing-flow PUBACK/PUBREC/PUBCOMP and incoming-flow PUBREC/PUBCOMP,
across MQTT5/311 and normal/fragmented IO. Each case reuses Session/Inbox across
two fresh Clients/Engines, verifies SessionPresent, preserved ids and DUP/PUBREL
progress, alias-free full-topic replay, original Future outcomes and retained
messages/results after physical destruction. QoS2 delivers once locally; QoS1
allows protocol-permitted duplicates. Wire-driven barriers and a broker round
trip precede final zero-obligation checks; timers alone never indicate recovery.

Windows native clients and WSL clients exercise modular/single/root/leaf static
and shared consumers. WSL also instruments the complete implementation closure
with ASan/UBSan/leak and TSan. Windows reaches the proxy by WSL private IPv4;
the broker remains loopback-only inside WSL. This is continuously running broker,
explicit volatile TCP recovery evidence. Broker restart/durable/automatic recovery,
Plain Dial/Auth Session composition, assigned ids, WebSocket, other brokers,
native Linux deployment, performance and >=72h production gates remain open.

## Explicit TLS Session composition

The [TLS Session stage](../tests/results/session-tls-20261110.json) adds numeric,
hostname and prepared-address TLS Session/Inbox factories and their MQTT5
enhanced-auth variants. Session opening/reset/replay waits for local final proof.
Denied proof and failed TLS/DNS preserve old offline state and expiry. Ordinary
Auth retains its bounded receive quarantine; QoS2 preflight now reserves storage
in that protocol-ready/pre-application-ready phase. TLS closure records actual
Session end time, gathers up to four shared alias spans and reserves native fence
storage on recovery wrappers.

Both platforms pass43 full programs (40 MQTT plus3 Time), six minimal programs,
root/leaf static/shared generic consumers and eight direct real-broker TLS example
runs across both OSes. WSL includes full ASan/UBSan/leak, three direct strict
sanitizer binaries and applicable TSan. Tests cover36 two-connection recovery
cases plus9 authenticated cases when selected, seven denied/missing-state proof
cases, three ordinary coalesced-auth QoS2 regressions, eight failed secure setups,
invalid claims/configs and seven allocator-budget prefixes. Single tests measure
one MQTT payload snapshot for new TLS QoS1/2; native TLS record copying is separate.

Matched seven-sample TLS observations use identical current peers and one common
Core with native TLS server fixture/MemoryStats from unchanged Core sources.
Ordinary before/after has no qualified5% throughput/10% P99 failures. Session
overhead at WSL MQTT311/window32 is a qualified7.4% throughput loss; wider Windows
raw throughput/P99 failures and unqualified noise remain recorded. Each publish
adds about one XRT block allocation and273/280 measured bytes (MQTT5/311) at
window1, across the sampled client/server/cipher window. Public layouts stay
fixed; recovery adds the existing24-byte TLS fence. No zero-overhead or full
performance/production qualification is claimed. Partial TLS-prefix/ACK-cut
interop, mutual-auth/resumption composition matrices, automatic/durable recovery,
WebSocket, native deployment and >=72h gates remain required.

## Occupied identifier group search

The [identifier stage](../tests/results/session-ids-20261110.json) preserves the
original immediate byte check when the next id is free. On collision it combines
Client/Session maps in32-bit groups, examining at most2049 groups including
partial wraparound. Existing8192-byte storage, gates, physical-free reservations,
initialization, allocations/copies and public/private layouts remain unchanged.

Both OSes pass six targeted root modular/single Session/Client Session/TLS Session
programs,10 trimmed and8 pure programs, four package forms and four direct TLS
broker examples each. WSL adds full ASan/UBSan/leak, three direct strict runs and
TSan. Tests cover complementary maps,17x17 wrap/boundary holes, all65535 held ids,
sole high-bit reuse and20-id concurrent physical retirement. Core139 members and
8/9 MQTT members match parent packages. Generated/features/API/docs/maturity and
89 tools pass on both platforms.

Atomic-map and unconditional-scan variants are rejected after cold/pure throughput
regressions. The final free-id path has no qualified5% pure-throughput failures;
WSL311/QoS2 retains a raw6.98% unqualified loss. Two qualified saturated-admission
cases show about13x/15x faster refusal; the other two are unqualified. TLS change/
parent raw and qualified bounds pass, while wide-window noise and Session/ordinary
raw overhead failures remain. This is not full performance/production acceptance.
The linked result preserves measurements, failed preparations, rejected variants,
source identities and remaining release gates.
