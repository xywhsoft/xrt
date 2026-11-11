# Native leased ASKING groups

Developing `xredis_pool_asking` leaf, two APIs: async and sync. It uses the native
pool lease/client request/Future and existing engine, timer, cancellation and
FIFO machinery. No request proxy or additional Future, worker or timer exists.

ASKING and one ordinary query are encoded once into one adjacent wire group.
Producers sharing a lease cannot interleave between its commands. Both commands
are pipelined for one network round trip: **the query may execute even when
ASKING is denied**. Results explicitly preserve Query and Control independently;
Query is the native request-result prefix. A control error never authorizes
query replay. No routing, topology refresh or replay is performed here.

A request-only aligned state tracks the two replies and full-wire acceptance.
The existing private sensitivity byte uses a disjoint mode bit; no client,
pool/lease/request header grows. Wire wiping still tests only its original bit.
The feature-off byte remains bool. Query payloads copy directly into the native
wire once. Client and group quotas include the16-byte prefix, alignment/state;
request count is one. Future-owned results/replies retained by callers remain
their storage. The original deadline includes both replies and is capped by
lease hold. Unsupported connection/transaction/subscription controls and CLIENT
are rejected before any wire admission. Init/trust/auth remain native factories.

A sent timeout/cancellation publishes an immutable snapshot, conservatively
MAY_HAVE_EXECUTED even when only a prefix might have gone out. It never drops
physical FIFO obligations: both replies must drain and the wire/timer/cancel
owners retire before lease reuse. Drain expiry, malformed control or transport
failure closes the connection. A malformed control is not accepted as success.
This prevents a one-shot ASKING flag from leaking into a later lease. Prefix
knowledge received before logical completion remains visible afterward; late
replies never update an already published result. Control and Query replies can
be retained across all client/pool/engine destruction through their Future.

The offline [example](../examples/pool_asking/README.md) uses a deterministic
local peer. Tests separately cover real Redis Cluster interaction, TLS writes,
fragmented/held controls, ambiguity, cancellation/drain/reuse, quotas, lifetime,
worker reentry and multiple producers. Local C-peer latency/throughput is not
upstream/fixed-arrival/mixed-load/native Linux/72h production acceptance.

Protocol: [ASKING](https://redis.io/docs/latest/commands/asking/) and
[Cluster specification](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/).
Automatic Cluster connections, original-deadline redirect execution/refresh and
complete four-library production acceptance remain active work.
