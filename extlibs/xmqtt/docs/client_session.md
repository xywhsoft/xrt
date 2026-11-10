# Explicit Client Session recovery (developing)

Select `XMQTT_MODULE_XMQTT_CLIENT_SESSION` and call `xmqttClientConnectSession`
with a caller-owned Session and Inbox. This optional numeric TCP factory joins
the existing protocol state and accepted application queue to the existing
Client sender/receiver. It creates no Engine, worker, background reconnect timer
or disk store. [Runnable example](../examples/client_session/README.md).

## Identity, ownership and reconnect

Use one nonempty ClientId and MQTT version for all three objects. Bind them to
one logical broker/security principal. Non-packet Client codec bounds must fit
the Session bounds, with the same UTF-8 policy. The connection packet cap may
increase to render an alias-expanded retained packet; Session's physical memory
budget still applies. The Inbox has the same four receive/object caps as Client.
Both objects are claimed exclusively until actual Client Destroy, including
after graceful native close. Stable-owner Info/Take and retained packet access
remain available; public Session mutations and owner destruction are refused.
Constructor failure leaves the output unchanged and releases both claims.

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

New outgoing QoS1/2 currently creates a canonical Session snapshot in addition
to the existing Client wire snapshot: one extra packet allocation and payload
copy. This is explicit performance debt. Replay allocates one request header
per retained flow, borrows immutable packet data and gathers a DUP prefix plus
tail without copying payload or creating a Future/result allocation. Native
reference leases and reference-count operations still have costs. Session adds
bounded incoming identity storage, its existing gate and fixed driver metadata;
the optional fields/code are absent from ordinary Client-only builds. Gauges
cover requested objects, not allocator/native/kernel overhead or process RSS.

TLS/Dial/Auth composition, assigned-id adoption, automatic reconnect, durable
storage, cross-process recovery, broker recovery interoperability, elimination
of the extra new-publication copy and production/performance acceptance remain
required work. The local recovery tests cover both protocol versions, QoS
stages, deduplication, resets, expiry, aliases, early ACKs, zero/partial native
output, physical retirement and controlled constructor/replay allocation failure.
The closed-loop Session benchmark uses the same echo fixture and observer as
ordinary Client/Inbox; it is not an independent broker or one-way receiver test.
