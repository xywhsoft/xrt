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

Retained messages, QoS 1/2, persistent sessions, Wills, enhanced authentication,
TLS and WebSocket are in development.

Each peer stays on its original XRT worker. The handshake and connection
retirement update the bounded identity index under a short gate. The steady
PING path uses the worker-local state, a validated fixed header and XRT's
bounded send queue. A contiguous native receive span can be parsed directly;
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
python tools/measure_performance.py --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_server,xmqtt_server_routes,xmqtt_server_wildcards,xmqtt_server_shared
```

The PING benchmark verifies each literal response and reports connection rate,
round-trip throughput and latency with 1, 64 and 1024 TCP clients. Each round
queues a PING on each socket before collecting replies. Numbers include local
peer-side collection and memory instrumentation. The route benchmark verifies
128-byte QoS 0 payloads at every subscriber and measures one publisher with
one or 64 subscribers on two XRT workers. The overlap benchmark verifies one
delivery per subscriber with both an exact and a wildcard filter registered.
The shared benchmark checks one round-robin delivery per publication in a
group of one or 64 subscribers.
The protocol baselines are [MQTT 5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).
