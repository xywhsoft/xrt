# xredis (developing)

Independent C Redis client extension on public XRT APIs. The final scope is
RESP2/3, bounded incremental replies, asynchronous requests, pipelines/pools,
TLS, transactions/blocking/PubSub, Sentinel, Cluster and Tracking.
**The command encoder, incremental RESP2/3 reader, owned reply builder, one
TCP request/response connection, explicit HELLO/AUTH initialization and optional
asynchronous DNS/multiple-endpoint setup and server-authenticated TLS transport
are implemented. Optional initial client-certificate authentication is also
locally verified. Atomic transaction execution is locally verified; identity-bound
tickets, remaining
advanced modes, HA and production/performance acceptance
remain pending.**

## Binary command encoding

Select `XREDIS_MODULE_XREDIS_COMMAND` before including `<xredis.h>`.
`xredisCommandSize` measures the exact RESP array-of-bulk-strings wire size.
`xredisCommandWrite` writes it to caller storage without a trailing NUL.
Empty arguments and embedded NUL/CRLF bytes are supported. Inputs are borrowed
only for the call; output must not overlap them. All validation precedes output
mutation. Failures preserve output parameters and report `xrt.redis` errors.
If diagnostic allocation itself fails, XRT's allocation-free `XERR_MEMORY`
fallback is retained; output atomicity still holds.
Successful calls preserve the ambient XRT error and perform no allocation.
The contiguous writer copies payload bytes once; a later vectored path will be
evaluated separately, not advertised as already present.

## Incremental response reader

Select `XREDIS_MODULE_XREDIS_READER`. Create with a copied explicit limit config,
feed arbitrary fragments, and consume BEGIN/DATA/END events. All RESP2/3 types,
attributes, pushes and the RESP3 streamed string/array/map/set forms are parsed.
DATA borrows the current input only during the callback; retain bytes explicitly
when needed. One allocation at creation holds the reader and bounded depth stack;
successful parsing does not allocate or copy payload bytes. It does validate
numeric text; the owned builder supplies lazy double conversion.

`Feed` stops at one complete semantic frame, a callback pause or its byte work
budget. Resume with the unconsumed suffix, including an empty suffix after an
END pause. Attributes augment the following value without consuming array/map
items. Do not publish partial events as a successful reply until READY. Limits,
malformed wire, callback rejection and truncated EOF are terminal until Reset.
Call `Finish` at EOF; it detects orphan attributes and incomplete frames.
Readers are thread confined; mutating callback reentry is rejected. See the
[reader contract and evidence](docs/reader.md) and [fragmented-input example](examples/reader/main.c).

## Owned replies

Select `XREDIS_MODULE_XREDIS_REPLY` for immutable owned frames. Feed transfers one
reply reference only after the whole semantic frame passes validation. Source
buffers can be recycled immediately; replies survive reader Reset/Destroy.
Index-based nodes preserve all types, binary payloads, duplicate keys, streamed
flags and ordered root/nested attributes. Completed replies support concurrent
read access and atomic Retain/Release after a synchronized handoff.

Small scalars use one allocation with inline storage. Larger frames grow node
and byte vectors under a per-frame **peak** byte cap, including temporary growth
copies. No per-node allocation or final flattening is required. Failure releases
partial storage and preserves a typed cause even if diagnostic allocation fails.
`ReplyDouble` converts raw numeric text lazily through public XRT number APIs.
See the [ownership, limits and actual costs](docs/reply.md) and
[buffer-recycling example](examples/reply/main.c).

## TCP requests

Select `XREDIS_MODULE_XREDIS_CLIENT` to connect to a numeric endpoint on an
existing XRT engine. Async admission copies/encodes arguments, bounds count and
bytes, and includes setup/queue wait in one monotonic deadline. Ordinary replies
follow the sent FIFO; RESP3 pushes use a separate bounded ownership ring. A sent
request's local timeout/cancel retains a physical FIFO slot until drained or the
connection is retired, with explicit execution uncertainty and no silent replay.

Accepted Futures always resolve with a typed `xredisrequestresult`; inspect
Status/Delivery, not only generic Future state. Sync calls reject the engine's
workers. Close has a separate physical-ownership observer; retained response
results survive engine destruction. See the [client contract and real costs](docs/client.md)
and [local TCP example with cleanup](examples/client/main.c).

Managed `ConnectInit` adds a shared readiness barrier for strict RESP3 (default),
explicit AUTO fallback or legacy RESP2, with optional ACL/password authentication,
client name and database. Queued business calls keep their original deadlines
and wait for all setup steps. Authentication failure cannot trigger fallback.
Owned credential wire is wiped after its final send reference, and readiness
metadata can outlive the client/engine. See the [initialization contract and
real Redis reproduction](docs/client-init.md) and [runnable managed example](examples/client_init/main.c).

The [fixed-arrival workload](docs/client-performance.md) retains every planned
offer, separates observer-capacity rejection from client overload, includes
generator lag in latency, and reports successful/all-outcome tails. Instrumented
allocation and controlled stall/held-reply profiles remain separate from normal
latency measurement. These workloads support acceptance work still to be done.

Select `XREDIS_MODULE_XREDIS_DIAL` for hostname setup through an application-owned
XRT Resolver or an immutable list of prepared endpoints. Family/attempt/fallback
limits use XRT's existing Dial; its callbacks and losing candidates retire before
the connection's physical Close barrier. An underlying shared DNS query may
finish later. Requests retain their original deadlines
through DNS, TCP and HELLO. The numeric `xredis_client` leaf still excludes Dial
and Resolver. See the [lifetime, limits and actual cold costs](docs/dial.md) and
[self-contained hostname/address example](examples/dial/main.c).

Select `XREDIS_MODULE_XREDIS_TLS` for required, verified encryption before Redis
initialization. It snapshots native TLS configuration, supports partial record
writes without prefix replay, preserves cancelled sent commands' FIFO duties,
and consumes plaintext through TLS APIs. See [the TLS contract and actual costs](docs/tls.md)
and [client-only trust/cleanup example](examples/tls/main.c). The TLS fixed-arrival
profiles retain the same offer/accounting model; default CA/name interoperability
uses separately generated ephemeral identities and real Redis.

Select `XREDIS_MODULE_XREDIS_TLS_AUTH` for a retained client signing identity,
using the existing TLS descriptor and native `xtlsclientauthconfigv1` directly.
The three V1 factories preserve managed setup/deadlines/close behavior and
snapshot the identity before DNS. Signing backends remain separate selections.
See [client-certificate authentication](docs/tls-auth.md) and the
[RSA file/cleanup example](examples/tls_auth/main.c). Initial TLS 1.2/1.3 full
handshakes are supported; identity-bound resumption remains pending. The cold
cycle benchmark uses an independent OpenSSL certificate verifier and records
the complete connection/HELLO/PING/physical-close wall cycle.

Select `XREDIS_MODULE_XREDIS_TRANSACTION` for one exclusive, staged
MULTI/queue/EXEC operation on a managed connection. It confirms MULTI before
sending business commands, suppresses EXEC on queue error/pre-commit abandonment,
drains DISCARD before reuse and preserves explicit execution uncertainty.
See [transaction ownership and phases](docs/transaction.md) and the
[self-contained example](examples/transaction/main.c). The optional native lease
integration is described below; final transaction acceptance remains pending.

Select `XREDIS_MODULE_XREDIS_WATCH` for a finite-lifetime exclusive
WATCH/read/decision/commit scope. It claims a ready idle managed connection,
seals one accepted commit, rejects unrelated admissions and awaits validated
UNWATCH or physical closure before reuse. See [the ownership/cleanup contract](docs/watch.md)
and [self-contained example](examples/watch/main.c). The optional native lease
integration is described below; final production/performance acceptance remains pending.

Select `XREDIS_MODULE_XREDIS_POOL` for bounded FIFO acquisition of exclusive
leases over existing ready managed TCP/TLS clients. Native calls retain hard
aggregate physical request/byte quotas; graceful Return waits for real storage
retirement, and old handles cannot address a reused connection. See [pool ownership
and actual costs](docs/pool.md) and [the two-member example](examples/pool/main.c).
Select `XREDIS_MODULE_XREDIS_POOL_TRANSACTION` or `XREDIS_MODULE_XREDIS_POOL_WATCH`
for [native leased transactions and nested WATCH sessions](docs/pool-sessions.md).
Their physical child/parent barriers preserve quotas and safe handoff.
Select `XREDIS_MODULE_XREDIS_POOL_CONNECT` for optional automatic connection
creation and replacement, bounded setup concurrency and jittered retry through
the existing managed numeric/DNS/TLS/auth factories. See the
[factory, retry and close contract](docs/pool-connect.md) and
[self-contained repair example](examples/pool_connect/main.c).
These modules and the product remain developing pending full scope and final
production/performance acceptance.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_examples --no-single
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --trim-only
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --kind shared --verify
python tools/check_release_maturity.py --manifest extlibs/xredis/config/modules.json
python tools/amalgamate.py --manifest extlibs/xredis/config/modules.json --check
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --smoke
```

`xredis_examples` builds and runs every registered example, including the
certificate-file TLS-auth example and its optional RSA identity dependency.
The `xredis` library selection keeps its existing dependency closure. TLS and
TLS-auth examples print usage without arguments; exercise their live PONG path
with the documented disposable Redis/CA/client-certificate prerequisites in
[TLS auth](docs/tls-auth.md). Passwords come from a runtime environment variable.

Normal maturity audits may pass for the currently registered assets; `--release`
must fail while the overall product is developing. Performance smoke verifies
the workload, not parity with another client or production throughput.

See [the offline example](examples/command/main.c),
[public reference](docs/api/reference.md), and
[command evidence](docs/coverage.md). The final target is recorded in
[the full development contract](../../docs/development/service-extensions-plan.md).
The public command frame follows the
[Redis protocol specification](https://redis.io/docs/latest/develop/reference/protocol-spec/).

Optional [Tracking observations](docs/tracking.md) preserve command-reply and
invalidation wire order on existing Client transports, with copied configuration,
borrowed event views and bounded opt-in storage. The [example](examples/tracking/README.md)
shows collection in a different order and retention after physical shutdown.

[Adjacent CACHING/query groups](docs/caching.md) let concurrent producers submit
OPTIN/OPTOUT queries with one queue credit, deadline and Future. Prefix and query
outcomes remain independent, including a prefix error followed by an executed
query. The [runnable example](examples/caching/README.md) retains both results
after physical shutdown; no application cache or uncertain replay is implied.

The developing [Pub/Sub wire module](docs/pubsub.md) encodes all six subscription
controls and PING, and classifies binary RESP2/RESP3 messages, ACKs and contextual
PONG without allocating. [Offline example](examples/pubsub/README.md). The native
subscriber supplies dedicated scheduling, ACK correlation and bounded delivery;
recovery remains pending. Ordinary Client calls reject subscription mode changes.

The developing [native subscriber](docs/subscriber.md) dedicates an idle ready
Client, pipelines validated controls and transfers separately owned messages.
Confirmed/reserved names, requests and delivery bytes are bounded; sent local
stops drain their remaining ACKs. Native reconnect/replay and final production
qualification remain pending. [Runnable example](examples/subscriber/README.md).

Optional `xredis_subscriber_notify` provides shared readability Futures and owned
batch delivery. See [receive ownership](docs/subscriber_notify.md) and the
[offline example](examples/subscriber_notify/README.md).

The developing [desired subscriptions/replay](docs/subscriptions.md) leaf commits
versioned local intent and restores it in bounded native batches onto replacement
Clients. Snapshots share immutable names. Stale/cancel/error paths retire partial
connections; automatic factories/backoff and Cluster/Sentinel routing remain open.
[Runnable offline example](examples/subscriptions/README.md).

`xredis_subscription_session` autonomously reconnects a private subscriber with
bounded jittered backoff and restores desired intent. Live edits apply incremental
deltas on the same transport; native receive remains direct. See
[session contracts](docs/subscription_session.md). HA topology and the complete
production/long-run gates remain open.

Optional [ROLE inspection](docs/role.md) supplies bounded borrowed role/list views; role-gated initialization and discovery are described below.

Optional [role-verified initialization](docs/role_client.md) withholds readiness and queued business until the requested role is validated under the original setup deadline.

Optional [Sentinel discovery](docs/sentinel.md) implements bounded master discovery,
ROLE validation and stale-role re-resolution; full automatic HA remains developing.

[Automatic Sentinel pools](docs/sentinel_pool.md) discover each replacement and
fence/retire all old members when the verified master endpoint changes.

[Cluster topology](docs/cluster_topology.md) provides bounded immutable SHARDS/
SLOTS snapshots, constant-time slot lookup, binary key slots and MOVED/ASK views.
Bounded request execution is available in [Cluster routes](docs/cluster_route.md);
coalesced metadata fetching is described below.

[Leased ASKING groups](docs/pool_asking.md) execute an adjacent ASKING/query with
one native request/Future and explicit control/query outcomes. The native FIFO
and physical lease boundary contain one-shot state through timeout/cancel.
[Cluster routes](docs/cluster_route.md) provide bounded address/redirect execution.

[Native shared pools](docs/pool_shared.md) multiplex ordinary calls over fixed
ready members with one native request/Future and bounded selection, without
per-call leases. Maintained pools and bounded Cluster routes are described below.

[Automatic shared pools](docs/pool_shared_connect.md) maintain and refill shared
node connections with bounded setup/retry and physical generation retirement.
Accepted business calls never replay. Coalesced metadata fetching is described below.

[Shared ASKING groups](docs/pool_shared_asking.md) execute adjacent native
ASKING/query pairs on fixed or maintained shared pools, with one Future and
separate control/query outcomes. Bounded Cluster routing is described below.

[Cluster route execution](docs/cluster_route.md) selects a master and follows
bounded MOVED/ASK redirects under one original deadline using authorized shared
node pools. Maintained node pools and coalesced refresh are described below; the managed facade remains work.

[Maintained Cluster nodes](docs/cluster_nodes.md) cache authorized node pools with
bounded aggregate quotas, coalesced setup, allocation-free hits and explicit
physical retirement. Coalesced metadata fetching is described below.

[Cluster metadata refresh](docs/cluster_refresh.md) fetches SHARDS or explicit SLOTS,
coalesces concurrent demands, publishes bounded immutable generation/origin maps,
and retains old views through failure. Explicit-seed bootstrap and invalidation
are caller-driven; full automatic MOVED wiring and the managed facade remain work.

[Asynchronous Cluster clients](docs/cluster_client.md) drive explicit-seed bootstrap,
coalesced MOVED refresh and native routes on an existing worker. Admission bounds
requests and bytes; parent cancellation and close preserve independent physical
duties. Seed evolution, replica policy and synchronous Cluster calls remain work.
