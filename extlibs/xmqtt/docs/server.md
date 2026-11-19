# MQTT server

Select `XMQTT_MODULE_XMQTT_SERVER` for the server-only build. Select
`XMQTT_MODULE_XMQTT_CODEC311` and set `Allow311` for legacy peers. The APP owns
its XRT listener. Return `xmqttServerAttach` from its Accept callback to
transfer the accepted stream. Keep the listener's receive limit at least as
large as the MQTT packet cap. Choose appropriate XRT accept, read and write
limits for the aggregate connection budget.

The TCP stage implements bounded CONNECT admission, one active binding per
Client ID, keep alive, and QoS 0 routing for exact, wildcard and shared
subscriptions. The APP can inspect borrowed CONNECT credentials and authorize individual publishes
and subscriptions in worker-local callbacks. The default policy is anonymous.
Run the example on loopback and install application policy before enabling
external access. A peer supplies a nonempty Client ID and a connection-scoped
session.

SUBACK and MQTT 5 UNSUBACK return results in packet order. Repeat SUBSCRIBE
replaces that connection's options without consuming another slot. Exact
matching preserves case and bytes. MQTT 5 No Local excludes the same Client ID.
MQTT 5 properties follow MQTT 5 deliveries, while 3.1.1 deliveries carry the
topic and payload. MQTT 5 capability declarations cover the current QoS,
retain, wildcard, shared subscription and subscription identifier limits.
Wildcard filters support `+`, `#`, empty topic levels and the leading-dollar
namespace rule. Each eligible connection receives one copy across overlapping
ordinary filters. A zero `MaxWildcardNodes` disables wildcard subscriptions. Other values
set the aggregate trie node limit. A wildcard filter can contain up to 128 topic
levels. MQTT 5 shared subscriptions use the pair of share name and underlying
filter as the group. Each matching group independently selects one member
with a round-robin cursor. A matching ordinary subscription can deliver a
separate copy. Each group consumes subscription capacity and retires with its
last subscription. A selected member whose native queue refuses a QoS 0
delivery drops that delivery. MQTT 3.1.1 treats the shared prefix as literal
filter text.

The in-memory QoS 0 store keeps the latest retained publication for each topic.
An empty retained publication clears the record and is still sent to matching
live subscribers. Either retention limit set to zero disables retention.
When a replacement exceeds the budget or allocation fails, the old record is
removed and the replacement is counted in `DroppedRetained`. The live routing
decision remains independent of retained admission. The byte cap covers wire
data and publisher IDs, and topic and packet caps bound indexing overhead.

Ordinary subscriptions replay retained messages with RETAIN set according to
MQTT 5 Retain Handling or MQTT 3.1.1 subscription rules. No Local also compares
the retained publisher's original Client ID when replaying. On live delivery,
MQTT 5 Retain As Published can preserve the inbound flag. Matching ordinary
filters with different live wire flags can produce two copies on one connection.
Shared subscriptions receive live deliveries and skip retained replay.

Retained topics are sharded by first level with a separate lock from routing.
Exact replay uses one lookup in the shard's AVL index, ordered by keyed
hash and full topic bytes. Routing uses the same kind of index. Lookups
leave tree state untouched. Wildcard replay scans the relevant shards,
or all retained shards for a leading wildcard. Replay skips expired entries.
Retained writes reclaim due records in their shard and try other shards under
quota pressure. Idle expired entries can remain resident until reclamation.
Unmodified same-version replay shares the immutable stored bytes with the
native send queues. Elapsed expiry and protocol conversion require encoding.

MQTT 5 and 3.1.1 publishers can send QoS 1 and 2. `ReceiveMaximum` limits
in-flight reliable publications per connection. `MaxIncomingPublishes` limits
those publications across the broker. MQTT 5 advertises its receive window
and reports 0x93 on receive-window exhaustion. MQTT 5 reports preflight
resource failures and policy denials with a negative PUBACK or PUBREC.
MQTT 3.1.1 closes on a refused reliable publication. The receiver keeps the
QoS 2 ID from its first PUBREC through PUBREL and native PUBCOMP retirement.
A repeated QoS 2 PUBLISH in that interval gets the same PUBREC before PUBREL,
with onward delivery and retention applied once. A failed reliable retained
replacement preserves the previous value.

Live delivery and retained replay use the lower of source QoS and granted
subscription QoS. Overlapping ordinary subscriptions use the highest grant
within each retained-flag variant. Shared subscriptions keep their own delivery.
The destination worker owns its FIFO and assigns independent packet IDs. QoS 1
releases its protocol window on PUBACK. QoS 2 releases it on PUBCOMP or a
negative PUBREC. A successful or repeated PUBREC produces PUBREL and preserves
the ID. Control replies continue while the publish window is full. Native write
backpressure resumes on stream drain. A shared immutable packet and a small
recipient-specific prefix carry the payload across peers. Pending messages can
expire before delivery and the outgoing expiry interval reflects time queued.

`MaxOutgoingPublishes` and `MaxOutgoingPublishBytes` limit broker-wide
outstanding reliable deliveries. They include cross-worker handoff and native
buffer ownership. `MaxOutgoingPublishesPerConnection` caps each destination's
outstanding delivery count. The byte counter charges the full size of each
packet, including deliveries that share the immutable body. Allocation or quota
failure ends the affected clean session, increments `OutgoingPublishOverloads`
and attempts MQTT 5 DISCONNECT 0x97. A packet exceeding the recipient's
negotiated Maximum Packet Size is discarded. Broker ACKs describe the broker's
inbound outcome. The previous quota and counter names are aliases for these
shared QoS 1 and 2 limits.

MQTT 5 Session Expiry and MQTT 3.1.1 Clean Session control how long the broker
keeps a session in memory. `MaxPersistentSessions` bounds the number of retained
sessions. A reconnect using the same Client ID and protocol version resumes
only after the previous TCP worker has retired. CONNACK sets Session Present
on resume. Clean Start creates new state. A concurrent resume request receives
Server Busy while that Client ID still has a TCP owner. Resuming a saved session
requires the same protocol version. An MQTT 5 DISCONNECT can shorten, extend or end a
nonzero interval. The expiry timer releases detached state. After native retirement, offline
admission and reconnect check the session deadline directly.

Offline ordinary subscriptions queue QoS 1/2 within the same count and byte
budgets. QoS 0 requires a live recipient. Shared rotation selects connected
members. An accepted in-flight exchange to a shared member can resume with
that member. Reconnect sends outstanding PUBLISH packets with their original
packet IDs and DUP, or continues a QoS 2 exchange with PUBREL. The current
connection's Receive Maximum gates replayed PUBLISH packets. Pending and newly
queued messages follow the original exchanges. Expired unsent records are
removed before transmission. Inbound QoS 2 retains the duplicate-detection ID
and pending PUBREL phase across reconnect.

Offline admission exhaustion ends that session and increments
`OutgoingPublishOverloads`. Server shutdown releases in-memory state and drains
its expiry callback. In-process resume coverage includes orderly and abrupt TCP
closure. Durable restart recovery, live persistent takeover, Wills, enhanced
authentication, TLS and WebSocket are in development.

Each peer stays on its original XRT worker. An authorized CONNECT registers
one session for its Client ID. That session owns its subscriptions and QoS
records. The peer owns the stream, negotiated transport limits and native
connection timers. Identity replacement seals the old session before closing
its stream. Exact, wildcard and shared routing skip sealed sessions while
their indexed links retire. The steady PING path uses the worker-local state,
a validated fixed header and XRT's bounded send queue. A contiguous native receive span can be parsed directly;
XRT coalesces a fragmented complete control packet when needed. One queued
timer per connection observes updated deadlines. Close seals admission and
requests stream aborts. Subscriptions are released before the connection
quota is returned, after the native event owner and its pending timer retire. Join listener callbacks and
then join the broker outside its workers before destruction.

Sixteen keyed shards index exact topics and filters whose first level is
literal. A separate trie holds filters that begin with a wildcard. Publishers
share read locks on the relevant shards while updating a subscription takes a
write lock. The library
builds one immutable packet per required protocol version and shares it across
the native send queues. The per-connection send limits and the subscriber limit
bound queued work. A destination with a full queue or smaller Maximum Packet
Size drops its QoS 0 delivery independently. The service records native
admissions, pre-admission drops and rejected inbound publications. Subscribers
can receive delivery already accepted by their native queue before UNSUBSCRIBE
took effect. ServerInfo combines the connection snapshot and atomic route
counters.

Run the loopback example as `examples_server_main wait`. It writes a numeric
endpoint, then accepts test peers until a line arrives on stdin. Both the
MQTT 5-only and dual-version module configurations have standalone tests.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server_dual_tests
python tools/measure_performance.py --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_server,xmqtt_server_routes,xmqtt_server_wildcards,xmqtt_server_shared,xmqtt_server_retained,xmqtt_server_inbound_qos1,xmqtt_server_outbound_qos1,xmqtt_server_outbound_qos2,xmqtt_server_session_resume
```

The PING benchmark verifies each literal response and reports connection rate,
round-trip throughput and latency with 1, 64 and 1024 TCP clients. Each round
queues a PING on each socket before collecting replies. Numbers include local
peer-side collection and memory instrumentation. The route benchmark verifies
128-byte QoS 0 payloads at every subscriber and measures one publisher with
one or 64 subscribers on two XRT workers. The overlap benchmark verifies one
delivery per subscriber with both an exact and a wildcard filter registered.
The shared benchmark checks one round-robin delivery per publication in a
group of one or 64 subscribers. The retained benchmark measures SUBSCRIBE
through the verified SUBACK and retained PUBLISH on one or 64 subscribers.
It reuses the same stored frame for all measured requests and includes
native queue and peer-side receive costs.
The inbound QoS 1 profile verifies every publisher PUBACK and every QoS 0
subscriber delivery, collecting subscriber latency after verifying PUBACK.
The outbound QoS 1 profile grants QoS 1 to each subscriber with Receive Maximum
1. It verifies a QoS 1 frame and emits PUBACK on every subscriber, then checks
that all delivery credits retire. The QoS 2 profile validates the complete
four-packet exchange on the publisher and on each subscriber. The session
resume profile publishes while the subscribers are offline and verifies Session
Present, the delivery and PUBACK on each resumed connection.
The protocol baselines are [MQTT 5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).
