# Explicit Client Session recovery (developing)

Select `XMQTT_MODULE_XMQTT_CLIENT_SESSION` and call `xmqttClientConnectSession`
with a caller-owned Session and Inbox. This optional numeric TCP factory joins
the existing protocol state and accepted application queue to the existing
Client sender/receiver. It creates no Engine, worker, background reconnect timer
or disk store. [Runnable example](../examples/client_session/README.md).

The optional [client_session_dial](client_session_dial.md) leaf adds hostname and
prepared TCP. Selecting Client Auth also enables numeric Session Auth without
Dial/TLS dependencies. The optional [client_session_tls](client_session_tls.md) leaf composes this
contract with numeric/hostname/prepared TLS and MQTT5 enhanced authentication.

## Identity, ownership and reconnect

Use one ClientId and MQTT version for all three objects. Fixed-id sessions use
the same nonempty id from creation. For MQTT5 server assignment, create a bounded
unassigned Session, use an empty first CONNECT, then use `xmqttSessionClientId`
for each reconnect. Assignment is committed only after successful CONNACK and
local Auth proof, before Ready. A local identity-cap refusal is LIMIT and leaves
the Session unbound. Once accepted proof and Session Open commit the id, a
later local close/deadline failure can still make Ready fail; the bound identity
is retained. Query it after physical retirement for every retry, including a
failed Ready, so the retry does not request a different server identity. Read the
[identity contract](session.md). Bind them to
one logical broker/security principal. Non-packet Client codec bounds must fit
the Session bounds, with the same UTF-8 policy. The connection packet cap may
increase to render an alias-expanded retained packet; Session's physical memory
budget still applies. The Inbox has the same four receive/object caps as Client.
Both objects are claimed exclusively until actual Client Destroy, including
after graceful native close. Stable-owner Info/Take and retained packet access
remain available; public Session mutations and owner destruction are refused.
Constructor failure leaves the output unchanged and releases both claims.

QoS1/2 publication reserves Session object/id/byte capacity at public
admission, including before CONNACK. SessionInfo.Objects/LiveBytes include that
pending immutable snapshot. It is absent from protocol lists and does not set
ActiveOutgoing/HasState. A physical limit returns OVERLOADED without publishing
a Future; cancellation or failed setup releases the pending snapshot after the
actual last request reference. It is never recovered as an unsent obligation.
The sender activates it only when selecting its first output. Reservation and
encoding rollback are separate, and payload encoding holds neither gate.

Reconnect is explicit: close and destroy the old Client, then invoke the factory
again with the same passive owners. The old Engine may be destroyed first.
Existing obligations reserve request slots and logical outgoing bytes during
setup. Successful accepted CONNACK opens Session before readiness is published.
MQTT5 missing server state discards old protocol state; MQTT3.1.1 mismatch fails
closed until an explicit offline reset. Expired local state cannot adopt a
server-present session. A failed handshake does not extend its expiry. Actual
network termination starts expiry; a fully accepted DISCONNECT expiry override
is honored. Accepted application Inbox messages survive protocol reset/expiry.

Only unacknowledged sent QoS1/2 publications and incoming QoS2 identities are
protocol recovery state. Unsent requests, subscriptions, pings, application
timeouts and Futures remain scoped to their original Client. Recovered work
has no new Future and does not inflate new application admission/completion
counters. Old result/ACK objects remain valid after both Clients and passive
owners are destroyed. Interruption never implies peer receipt.

## Protocol progress and bounds

Retained PUBLISH replays with DUP, the complete logical topic and no old Topic
Alias. QoS2 after positive PUBREC resumes PUBREL. Connection negotiation and
the conservative current-connection in-flight window still bound output;
PUBREL can progress with PUBLISH quota exhausted, and queued QoS0 can progress
while old QoS1 awaits acknowledgement. Old and new outgoing identifiers share
one bounded namespace. A cancelled new selection with zero native output is
withdrawn and refunded; accepted prefixes retain their protocol obligation.

A prior-generation ACK can arrive before replay. If it arrives while a replayed
PUBLISH is only partially accepted, protocol progress is committed and that
transport closes with SESSION status. A later explicit connection continues any
remaining PUBREL obligation. This avoids continuing an abandoned partial frame
or causing an unnecessary second application delivery.

New incoming QoS2 state commits while holding the Inbox gate, before queue
publication and PUBREC. Resumed duplicates are acknowledged without another
application message. Known old PUBREL completes without consuming or refunding
a new-connection receive slot. Physical packet/request quotas and identifiers
remain charged through the actual last allocator free. Retained native sends
therefore cannot make capacity reusable prematurely. Allocation failure during
replay construction releases temporary wrappers and preserves old obligations.

## Cost and remaining work

Outgoing QoS1/2 now takes one canonical snapshot at admission. Client
and Session share its immutable wire; a non-alias native write uses one span
and retains the existing request for the native lease. The request has no private
wire buffer. Request and packet headers remain separate allocations because their
connection/Future and protocol lifetimes differ. Relative to ordinary Client,
that extra packet/header allocation and Session synchronization still have costs.
Identifier selection keeps Client Gate and Session Gate, combining the Client
and Session byte maps in32-bit groups only when the original next-id bit check
finds that id occupied. Fallback examines at most2049 groups, including
a partial wraparound, while preserving physical reservations until actual free.
There is no extra bitmap storage, initialization, allocation or atomic operation.
This improves bounded saturated/fragmented lookup, not a demonstrated steady-state
throughput gain or production performance acceptance.
Alias admission uses one additional transaction mutex in the optional driver.
All alias publishers on that Client, including QoS0 bindings, serialize capture,
encoding and successful queue publication. Failed admission never changes the
planned binding; cancelling a successfully admitted binding preserves planned
state and later sends restore the actual topic when necessary. Payload encoding
holds neither main gate, so ordinary producers/IO/close remain independent.
The alias transaction itself can wait on another alias producer; entry deadlines
include that delay. No application callback is invoked by the submitting code
under this gate. The mutex remains alive until all request/API references retire.

Full-topic alias output gathers four spans; compact output combines only small
headers and uses two spans. The immutable property/payload tail is borrowed.
Topic Alias can precede other properties; User Property order stays unchanged,
consistent with [MQTT5 section3.3.2.3.7](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).
When a host queue refuses the gather, one bounded scalar attempt permits progress
with a single lease. Partial frames still win over all subsequent output. There
are small header copies, native retain/release operations and alias-lock costs;
this does not establish zero-overhead or production performance acceptance.
Replay allocates one request header
per retained flow, borrows immutable packet data and gathers a DUP prefix plus
tail without copying payload or creating a Future/result allocation. Native
reference leases and reference-count operations still have costs. Session adds
bounded incoming identity storage, its existing gate and fixed driver metadata;
the optional fields/code are absent from ordinary Client-only builds. Gauges
cover requested objects, not allocator/native/kernel overhead or process RSS.

Automatic reconnect, durable
storage, cross-process recovery, complete broker recovery interoperability, elimination
of remaining allocation/locking costs and production/performance acceptance remain
required work. The local recovery tests cover both protocol versions, QoS
stages, deduplication, resets, expiry, aliases, early ACKs, zero/partial native
output, physical retirement and controlled constructor/replay allocation failure.
The closed-loop Session benchmark uses the same echo fixture and observer as
ordinary Client/Inbox; it is not an independent broker or one-way receiver test.

Snapshots allocate in the scope active in PublishAsync. A sent packet retained
by Session keeps that scope charged across Client/Engine destruction until actual
packet retirement; closing the original connection does not release its budget.
A pending unsent snapshot refunds after its last request reference, independently
of the earlier logical Future settlement. This is requested-allocation accounting,
not allocator/native/kernel overhead or process RSS. The one-span/reference statement
is per native write: chunk/budget limits can split a frame across several calls
and loans. Replay may gather a DUP prefix and tail as two spans.
