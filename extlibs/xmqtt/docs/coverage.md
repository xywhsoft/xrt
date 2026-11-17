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
verified TLS 1.2/1.3 and mutual authentication. Volatile QoS/expiry state and
explicit reconnect are covered by the optional Session leaves below.
A developing [automatic controller](reconnect.md) now drives bounded retry and
physical generation retirement. Persistence and WebSocket remain pending. Independent
pinned-broker evidence exists; final performance/production acceptance is pending.

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

## Explicit TCP Session Dial/Auth composition

The [Dial/Auth stage](../tests/results/session-dial-20261110.json) adds numeric
Session Auth plus hostname/prepared Session factories and their MQTT5 Auth variants.
Numeric Auth stays independently selectable without Dial/Resolver/TLS. Existing
entry deadline, passive claims, native retirement and proof-gated Session open/
reset/replay are reused. Reauthentication does not reopen Session; algorithms/
credentials remain application-owned. No new per-publication logic/data or Core
runtime is introduced.

Both OSes pass50 full,10 trimmed and8 minimal programs; WSL adds full ASan/UBSan/
leak, three strict runs and Auth-enabled TSan. Two-Engine short-read/write cases
cover three outgoing QoS stages, incoming deduplication, proof failures, DNS/
refusal/cancel/timeout, allocation prefixes and exclusive claim rollback. Twelve
package verifications,12 packaged public-API tests and16 runnable example modes
pass across both OSes. Actual packages also pass80 independent broker ACK-cut
cases using both new Dial constructors and both protocol versions. Authentication
algorithm/plugin, TLS cuts and native production scope are not inferred.

Core139 and8/9 MQTT members match parent packages; only Client changes. Public/
private layouts remain unchanged. Four registered prepared-Dial benchmark profiles
pass smoke. Matched observations retain raw failures and noise gates: Windows
wide raw throughput/P99 and WSL311 wide raw P99 failures are unqualified; no full
performance/production acceptance is claimed. The linked report records exact
source identities, initial single-entry compiler failure/correction, package input
equivalence, measurements and remaining release work.

## TLS Session recovery interoperability

The [TLS recovery stage](../tests/results/tls-session-recovery-20261110.json)
adds independent broker/proxy tests without changing runtime, configuration,
generated headers, ABI or the previously verified root packages. Windows static/
shared/single and WSL static/shared/single/ASan+UBSan+leak/TSan each pass 432 cases:
3456 total, including 576 strictly locally partial PUBLISH cases and 2880 ACK cuts.
TLS1.2/1.3, MQTT5/311, numeric/custom asynchronous Resolver/prepared factories,
full/mutual handshakes and TLS1.3 ticket reuse/full fallback compose with the
same MQTT SessionPresent, identifier/DUP/PUBREL and incoming-dedup assertions.
Every case retires two Clients/Engines; all probe/broker/reader/proxy owners are
reaped and runtime trust/key material is removed. Eight fixture boundary/cleanup
controls, 40 ordinary TCP recovery regressions and both OSes' generated/API/docs/
maturity plus 89 focused tool tests pass. A delayed 200ms cut also retains a locally
partial write. Earlier fixture failures and their corrections are recorded.

TLS terminates at the bounded Python/OpenSSL proxy; the pinned Mosquitto2.1.2
backend is local TCP. This is not broker TLS, system DNS, enhanced-auth plugin,
TLS corrupt-record, restart/durable/automatic recovery, performance or production
qualification. Shared-host WSL does not establish native Linux or>=72h acceptance.


## Bounded MQTT5 assigned ClientId

The [assigned-id stage](../tests/results/assigned-id-20261110.json) reserves an
explicit 1..65535-byte identity capacity at Create, copies an accepted server id
only after authenticated Open, and exposes an immutable borrowed view for later
CONNECTs. End/expiry/Reset preserve it. Failed Open/proof or over-cap assignment
cannot bind it; after accepted Open a later Ready failure does not roll it back.
No public/private state fields or per-publication logic are added. Fixed-id base
sizes remain unchanged, with the reserved capacity charged on unassigned objects.

Both OSes pass 22 modular and 29 single programs. WSL adds full ASan/UBSan/leak and
two TSan programs. Standalone tests cover boundaries, unchanged outputs, invalid
ids, limits, allocation prefixes, one-byte-budget allocation-free binding/reads,
concurrent publication/readers and retained identity after reset/expiry. Connection
tests cover pre-CONNACK snapshots, proof quarantine, refusal/close/over-cap rollback,
canonical replay and outcomes beyond both Engines/passive owners. Twelve package
verifications and 24 example modes pass. Independent Mosquitto tests pass 1808
cases across eight build forms, including 288 TLS partial prefixes, full/mutual and
accepted-ticket/full-fallback TLS with three factories. Forty fixed-id regressions
also pass. Failed initial test compilation/selection and preparation logs remain.

Core root 139/numeric 30 members match prior archives; only MQTT Client and Session
objects change. Seven of eight fixed-id pure Session throughput observations meet
the noise gate and none breaches 5%; one remains unqualified. WSL5/QoS2 retains a
qualified 4.17% loss. No new first-assignment/getter latency, end-to-end P99, native
Linux, enhanced-auth algorithm/plugin or 72h production acceptance is inferred.

## Nonwaiting physical Client retirement

The [retirement-query stage](../tests/results/retirement-query-20261110.json) adds
`xmqttClientPollRetired` for either IO worker. It observes Client and native
notification tails; busy predicates return TIMEOUT without waiting. Invalid input
or real synchronization failure returns ERROR, even with an identical old static
diagnostic. The stable unique owner remains required and Wait/Destroy keep their
worker prohibition. Existing Client production code outside this new function is
byte-identical: no fields, send work or reference-drop operations are added.

Each OS passes 22 modular tests, 29 single tests and six Core examples. WSL adds
full ASan/UBSan/leak and a modular Client TSan run. Tests hold real notification
and mutex tails, retain a native alias, cover both workers, pre-stream Dial
cancellation and TLS, and check allocation-free pending/ready error preservation.
Four verified static/shared packages run eight example/consumer executions; four
extra packaged probes perform 64 poll-only retirement lifecycles. Declaration-only
minimal scope, exports and generated/API/docs/maturity checks pass on both OSes.
Automatic reconnect, durable recovery, WebSocket and complete performance/native
Linux/soak production gates remain open. No broker or end-to-end performance result
is inferred from this lifecycle stage.

## Automatic reconnect controller

The [controller stage](../tests/results/reconnect-controller-20261110.json) drives
actual bounded replacement connections, private Session/Inbox, explicit typed
factory failure policy, saturating jittered backoff, stable reset, per-outage
deadline, cancellation and physical old-generation cleanup. Caller-storage leases
prevent premature destruction; cleanup data finalization works even when its
queued Proc is cancelled. Closed/full cleanup admission retains an owner for
external Wait/Destroy assistance. No per-connection thread/runtime is created.

Each OS passes59 final root programs. WSL adds full ASan/UBSan/leak and TSan.
Four packages, eight examples/consumers and68 packaged typed-policy cases pass.
Independent Mosquitto/proxy tests pass160 automatic numeric-TCP cases over eight
build forms, both MQTT versions and five outgoing/incoming ACK cuts. Nine exports,
private-hook visibility, minimal declarations and generated/API/docs/maturity
checks pass. Two feature-excluded Client scopes retain parent machine .text.
All eight local throughput comparisons qualify with no5% loss (largest1.8%). Only
two of eight P99 comparisons qualify; six retain excessive noise. These closed-loop
observations do not complete throughput/tail/CPU/RSS/contention/native Linux/soak
production gates. Automatic TLS/DNS/Auth composition still needs dedicated evidence;
offline acceptance, subscription recovery, durable state and WebSocket remain open.

## Automatic TLS/Dial/Auth composition evidence

The [transport stage](../tests/results/reconnect-transports-20261110.json) passes
3456 independent broker/proxy cases over eight native Windows/WSL build forms,
including full ASan/UBSan/leak and TSan. It covers TLS1.2/1.3, mutual TLS, accepted
TLS1.3 tickets/full fallback, MQTT5/311, numeric/custom async DNS/prepared factories,
five ACK cuts and576 strictly locally partial PUBLISH cases. One Engine hosts both
automatic generations; the same bounded Resolver is borrowed through both and
joins after the controller. Runtime trust/identity material is removed each run.
The broker backend is TCP behind a bounded Python/OpenSSL TLS proxy.

Both OSes pass61 root programs and final2-program transport reruns; WSL adds full
memory instrumentation and a final TSan run. Sixteen literal-peer scenarios cover
proof quarantine, two authenticated generations, reauth, held tickets, default
proof-failure retry policy, TLS rejection and DNS/TLS deadlines. These fixed-token
authentication fixtures establish protocol/lifetime behavior, not a real algorithm
or enhanced-auth broker-plugin interoperability. Production source/ABI/closures
are unchanged; broader performance, real system DNS, broker-native TLS/other brokers,
native Linux and soak acceptance remain open.

## Optional automatic subscription intent recovery

`xmqtt_reconnect_subscriptions` maintains a bounded volatile registry alongside
the private reconnect Session/Inbox. MQTT 5 and 3.1.1 intent snapshots preserve
exact filters/options and group properties, reject duplicates atomically, and
reconcile initial/fresh/resumed sessions, uncertain controls and offline removals.
Known confirmed resumed subscriptions are carried forward. Original ACKs cannot
confirm newer revisions. Per-filter reasons, grant downgrade, partial rejection,
local peer restrictions and full retained ACK metadata are observable.

One Client control Future is in flight, using separately owned bounded watch
registrations and the existing worker; no new thread or publication callback is
added. Concurrent builders reserve snapshot bytes before allocation. Filter,
snapshot, batch, timeout and retry bounds are explicit. Hash lookup, dirty queues,
maintained counters and allocation-free duplicate sorting avoid an unbounded
offline queue or per-update full-table scan. Session transitions scan the bounded
table. Client/native budgets and allocator/kernel overhead remain separate.

Dedicated tests cover 1024-filter copy/order/batching, quota/duplicate rejection,
negotiated packet splitting/restriction, live and outage deadlines, stop with a
pending watch, caller and exact worker allocation-budget failures, concurrent
updates, full result callback retention and actual physical joins. Separate
examples, numeric broker/proxy consumers and a closed-loop control benchmark are
provided. Evidence records actual final runs and preparatory failures; no release
or performance parity is implied by those assets alone. Automatic TLS/DNS/Auth
subscription composition, durable/crash/WebSocket/offline-publication scope,
native Linux, fixed-arrival/contention/CPU/RSS and >=72h acceptance remain open.


## Committed state recovery qualification (2026-11-17)

The developing [store-state applier](store_state.md) validates all12 typed
operations across transactions and publishes an immutable offline state only
after journal EMPTY. State is bound to the exact descriptor/version/schema/
generation/identity, with increasing epochs/keys, stale-id protection, legal QoS
stages, closed expiry and independent pending Inbox ownership.

Windows/WSL modular/single/root/stats-off/static/shared and standalone declaration
consumers pass. Evidence includes88 prefix cuts,8 expiry cuts,44 illegal
histories,26 resource/identity limits,9 actual allocation-prefix failures,
368 real native error-key write failures per platform, two bound contexts,
late-prefix atomicity and8 concurrent readers after Journal/file release.
Each integration program checks448 storage-failure models. Six forms x two
protocol levels x seven real child-process cuts give84 fresh-process recoveries
per platform. An independent Python literal-packet/struct/zlib oracle supplies
and checks exact bytes. These process cuts do not qualify physical power loss.
Assigned-identity/version refusals, four resumed epochs, six early application-
ACK cuts and independent QoS1 deliveries pass. All48 public-read boundaries and
the two final suffix resize/sync failures reject early state publication and
refund earlier payloads. WSL ASan/UBSan/leaks and a separate TSan reader run pass.

Trimmed size probes, five public exports and three-form public ABI probes pass;
the private replay descriptor/cursor helper remains hidden. Session import, live
durable ACK/Inbox/lease/driver integration, namespace/compaction/group commit,
controlled throughput/tail latency and native72h qualification remain required.


## Direct State-to-Session import qualification (2026-11-17)

The developing [store-session importer](store_session.md) copies immutable
recovered protocol obligations into an independent disconnected owner. It
checks exact binding/generation/version/identity, further wall elapsed time,
codec/count/byte/scratch limits and checked quiet error ownership. Explicit
MQTT5 assigned-id import is separate. Source Inbox payloads stay independent.

Windows/WSL modular/single/root/statistics-off/static/shared/declarations and
real-file passive example pass, including5 imports,3 expiry cuts,40 refusals,
9 actual allocation-prefix failures,24 native error-slot failures plus2 bound
contexts/platform. Legacy Session/image/State regressions pass; preprocessed
regression selections are identical across later test/comment registration.
WSL ASan/UBSan/leaks pass. An actual Client+Engine handles exact recovered
PUBLISH/PUBREL and incoming replay/dedup against an independent literal Python
TCP peer in4 MQTT5/311 full/fragmented cases/platform. The source journal in
that wire fixture uses test VFS; native-file recovery/import is separately
exercised by the runnable example. No combined live durable driver is inferred.

Three exports/three hidden helpers, x64 three-form ABI and trimmed delivery pass.
64-packet benchmarks include validation, scratch, wire/property copies/rebasing,
Session indices, allocations,Info and Destroy. Separate instrumentation counts
65 property-free/scratch0 or66 two-property/default-scratch XRT allocations.
Raw noise-gate failures remain; no controlled performance acceptance is claimed.
The stage report is `docs/development/xmqtt-store-session-20261117.json`.
Bounded asynchronous Journal submission now has separate qualification below.
Live durable ACK/Inbox leases, physical retirement integration,
namespace/locking/compaction/group commit and native Windows/Linux72h remain open.


## Bounded asynchronous Journal owner qualification (2026-11-17)

The developing [Journal worker](journal_worker.md) transfers one READY Journal
to a borrowed host pool and adds7 APIs. It serializes opaque commits, bounds
requests/retained objects/bytes/native tails/lifetime pump admissions, preserves
late COMMITTED outcomes, and joins physical extension ownership. No thread/pool,
Core API, automatic retry or MQTT ACK/semantic-state driver is added.

Windows/WSL modular/single/root/stats-off/static/shared/declarations and a
[runnable real-file example](../examples/journal_worker/README.md) pass. Tests
cover8 producers/4 workers/256 exact commits, seven actual linked admission
allocator-prefix failures plus constructor OOM, six zero caps, shared-pool turn
progress, rejection rollback, nested pruned-neighbour callback protection,
retained results and physical observer/task retirement. Four actual post-sync
native error-key failures/platform preserve commit effects with fresh typed
recovery; this dedicated harness temporarily detaches and restores the native
task's real bound error context. Each integration program checks448 storage
failure models. Two versions x six forms x seven native barriers produce84
killed children/fresh typed recovery cases/platform. Independent Python literal
oracle bytes and cross-OS file readers pass; power loss remains unqualified.
WSL ASan/UBSan/leaks and Clang21 TSan concurrency pass. The GCC15 TSan compiler
fence failure is retained, without suppressing warnings. Legacy29-program
regressions/platform pass. Corrected preprocessor equivalence explicitly checks
real selected definitions; earlier empty-selection probes are invalid evidence.

Seven exports/four hidden helpers, identical three-form two-OS ABI and trimmed
single/static/shared pass; worker-only excludes State/Session/Client/Engine/VFS.
The matched direct/borrowed-pool256-byte two-sync native-file diagnostics count
copy/whole-allocation zeroing/final secure wipe, allocations/tasks/synchronization
and physical retirement. Noise failures remain; there is no controlled performance
acceptance. The report is `docs/development/xmqtt-journal-worker-20261117.json`.
The legal live mutable owner, durable prepare/commit/ACK ordering, delivery leases,
namespace/locking/compaction/group commit and native Windows/Linux72h remain open.

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
See [stage evidence](../../../docs/development/xmqtt-store-write-20261118.json) and the
[component guide](store_write.md).

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
See [follow-up evidence](../../../docs/development/xmqtt-store-write-revalidation-20261118.json).


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
See [stage evidence](../../../docs/development/xmqtt-client-store-open-20261118.json).


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
See [stage evidence](../../../docs/development/xmqtt-client-store-receive-20261118.json).
