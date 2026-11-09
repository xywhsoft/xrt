# Bounded hostname and prepared-address Dial (developing)

Select `XMQTT_MODULE_XMQTT_CLIENT_DIAL`. `xmqttClientDial` accepts a counted,
nonempty hostname without embedded NUL and a nonzero port. `xmqttClientDialAddresses`
accepts at most64 immutable prepared endpoints, preserves eligible endpoint ports,
and avoids DNS. `xmqttClientDialConfigInit` defaults to dual stack, eight attempts,
250ms fallback, 30000ms additional Dial timeout and1024 host bytes. Config.Stream
is the sole native TCP buffer/timeout configuration. The finite SetupTimeout from
public factory entry covers validation/copies, DNS, candidates, CONNECT/CONNACK and
any final authentication proof. No fallback restarts it. Dial Timeout0 disables
only its additional timer; -1 is infinite. Neither disables the MQTT setup limit.

Core supplies asynchronous bounded Resolver requests, family interleaving,
candidate racing and losing-socket retirement. Eligible endpoints must have a
nonzero port, Family must match at least one, attempts/address counts cannot
exceed64 and host bound cannot exceed65535. Core/Resolver limits may reject sooner.
Resolver and Engine stay alive through physical Client retirement. The immutable
address list may be released after successful factory return. Host/CONNECT/config
are copied before return; the output remains unchanged on failure. Output storage
must not overlap Dial or borrowed hostname/endpoint storage.

Ready has the existing owned typed result. DNS/refusal, native memory/resource
failures, Dial/setup timeout, Future cancellation and explicit Close remain
separate outcomes; the first closing reason wins. Queued application requests
use their original deadlines and cannot send before accepted CONNACK. During
pending DNS, no Stream exists: Close cancels Dial, readiness may resolve logically,
but WaitFor/Destroy still retire the actual client callbacks and losing sockets.
Core detaches a cancelled subscriber from its shared query: an independent lookup
may continue under Resolver ownership after Client/Engine retirement. Resolver
Destroy joins that work; its LookupData must remain alive until then. The client
does not wait for unrelated subscribers or retain a fabricated DNS-worker owner.
A late winning Stream is always consumed and aborted if Close already won.
Wait/Destroy reject host Engine workers. No reconnect, request/session replay,
TLS, WebSocket or durability is implied.

With `XMQTT_MODULE_XMQTT_CLIENT_AUTH` also selected, DialAuth and DialAddressesAuth
retain the same method/challenge/final-proof gates and independent reauth results.
Ordinary Dial continues to reject enhanced-auth CONNECT. Authentication deadlines
share the original setup budget. Expensive proof computation belongs off the IO
worker as described in [authentication](client_auth.md).

The selected Dial leaf adds one private pointer; old public config/info/results
stay unchanged. Numeric connections allocate no Dial object or host scratch. A
counted host requires one temporary bounded NUL copy charged against MaxPrivateBytes,
then Core copies/canonicalizes it; scratch retires before factory return. Native
Dial/Resolver/Stream allocations use their existing separate bounded policies,
not the MQTT object/private quotas. Only the winner receives one owned event
binding, after physically retiring losers; buffered input is pumped explicitly.
The handoff snapshots the public Stream stats: EOF recorded before event
installation fails setup promptly instead of silently waiting for SetupTimeout.
Ownership moves from the owned Dial callback to that binding, with a queued
client reference spanning the Done/Drop callback tail. No per-request transport
vtable, extra shared runtime or payload copy is introduced. Factory copies/scans,
Core canonicalization, candidate allocation, posts, synchronization and the event
binding are real costs; no zero-overhead claim. The completed Dial handle and
its bounded cold control storage remain retained until Client Destroy; this
lifetime benchmark does not establish zero extra steady retained bytes.

See [example](../examples/dial/README.md) and `tests/test_client_dial.c`. Tests
exercise MQTT5/3.1.1 byte writes, DNS/prepared/family/max-attempt paths, queued QoS1,
missing DNS/refusal, blocked lookup Close/cancel/timeouts, constructor allocation
budgets, retained outcomes and optional enhanced auth. A single-header diagnostic
injects winner event-binding OOM on a real Stream. The lifecycle benchmark uses
one local Engine/Resolver and numeric/prepared/hostname variants, including actual
CONNECT, QoS1, Client/peer physical retirement and Future disposal per cycle.
Hostname samples include the first cold fixture lookup then cached resolution;
this is not an external resolver/broker or fixed-arrival production comparison.

Stage evidence: [validation and cost report](../tests/results/client-dial-20261109.json).
