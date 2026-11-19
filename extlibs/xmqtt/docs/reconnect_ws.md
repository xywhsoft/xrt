# Automatic WS/WSS recovery

Use the existing `xmqtt_reconnect` or `xmqtt_reconnect_subscriptions` controller
with the ordinary [native WS/WSS](client_ws_connect.md) or [URI](client_ws_uri.md)
connection factories. Pass the supplied Session, Engine and affinity to each
connection. Factories use the copied Client config and can supply their own
receive queue policy.

The [runnable example](../examples/reconnect_ws/README.md) includes a small
application-owned [endpoint adapter](../examples/reconnect_ws/endpoint.h). Bind
WebSocket.Session to the controller Session. WebSocket.Inbox is an optional
APP owner. URI/HTTP/TLS/CONNECT/auth
snapshot per attempt; the persistent endpoint's URI, policy, Resolver, trust and
callback/code lifetimes still extend through controller Destroy. Do not release
its verifier after Create if a later Factory will reuse it. Construction refusals
return an explicit typed status from a fresh diagnostic scope; stale ambient
errors do not select retry policy. No second protocol runtime or payload/Future
wrapper is added.

## Generations, clocks and state

Acquire ordinary Client leases only when ONLINE. A batch can share a lease;
per-call leases incur their real mutex/check/release costs. Old results remain
scoped to their original Client. Replacement waits for all old leases AND real
Client/WS/TLS/TCP callback/owner/alias duties, including off-worker cleanup task
finalization. A held native WS alias or enhanced-auth ticket can delay replacement
after application leases return. Stop/Wait preserves those owners rather than
forcing an early free; closed cleanup Pools retain external Wait assistance.

Original request/setup clocks still cover HTTP/URI/DNS/TLS and local auth proof;
one recovery deadline covers an entire outage, backoff and attempts. HTTP reject,
authentication and local-capacity failures remain terminal by default. An
application-triggered CLOSED retry must be explicitly enabled when desired.
Initial AUTH and local final proof quarantine ONLINE/leases; reauthentication of
an already accepted generation retains ordinary concurrent traffic.

For persistent311 state, use Clean Session=false from the FIRST connection.
Clean Session=true discards that session on disconnect; changing a subsequent
attempt to false cannot recover its old QoS obligations. MQTT5 Clean Start and
Session Expiry are independent. Subscription recovery keeps confirmed state only
when the accepted session actually retained it; stateless311 retries use explicit
always-clean policy. References: [MQTT3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html)
and [MQTT5](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).

## Evidence and remaining acceptance

Run the separate protocol/fault/auth suites and their singles:

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_reconnect_ws_tests
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_reconnect_ws_tls_tests
```

The [source-bound report](../../../docs/development/xmqtt-websocket-reconnect-20261116.json)
records actual final checks and original/compressed binary identities. Literal
native peers exercise numeric/DNS/prepared and three URI routes, both MQTT versions,
three outgoing ACK cuts, conservative old outcomes, canonical identifier/DUP/
PUBREL replay, Inbox, retained/lost subscriptions, original HTTP/DNS deadlines,
stop, actual OOM/owner balance, held tickets/native aliases and proof/reauth.

The independent [gateway](../tests/interop/reconnect_ws_gateway.py) implements
bounded HTTP/WS transport from public [RFC6455](https://www.rfc-editor.org/rfc/rfc6455.html),
using stdlib/OpenSSL, to a pinned real Mosquitto TCP broker. The existing independent
MQTT proxy drops one of five outgoing/incoming QoS ACK packets. This checks actual
automatic retained broker sessions, topics/aliases, incoming duplicate identity,
WS fragments/Ping/Pong, TLS1.2/1.3 trust/close and real native joins. It is a
TCP-broker-behind-gateway result, not broker-native WS qualification. The previous
native Mosquitto missing-WS-Close finding remains unresolved.

`bench_reconnect_ws.c COUNT VERSION(4|5) WINDOW INSTRUMENT MODE SECURE` compares
manual Session setup, one batch lease (MODE1), and per-call leases (MODE2), under
the SAME native URI/HTTP/WS/WSS/Session selection and workload. Setup/real final
retirement are outside the timed publish loop. There is no fixture Notify or
initial incoming publication. Latency ends at caller-observed PUBACK and includes
submission/lease work; process CPU/XRT allocation counters include both peers.
CRT/native/kernel costs are not fully counted. P50/P95/P99/P99.9, max, checksum and
actual physical shutdown are recorded. Modes are closed-loop diagnostics, not
fixed-arrival or native production acceptance; quality failures are never hidden.

Optional [post-Upgrade TLS credential handoff](client_ws_tls_resume.md) composes
the same factory/controller and existing cache APIs. The application owns any
cross-generation credential and supplies it to a subsequent factory. Without
that optional leaf, WSS ResumeLimit must be zero. Durable protocol state,
additional broker-native/slow-peer/partial-write/load matrices,
controlled comparator throughput/tails/CPU/copies and native Windows/Linux>=72h
soak remain required. WSL sanitizer evidence is distinct from native Linux
acceptance. The original Redis/RPC/Cron and complete four-product scope remain active.
