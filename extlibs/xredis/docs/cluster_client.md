# Asynchronous Cluster client

The developing xredis_cluster_client leaf combines shared maintained Nodes,
private metadata refresh and bounded Cluster routes on one existing XRT worker.
Create retains Nodes, copies explicit seeds, pins the progress engine, and
starts automatic SHARDS/default or explicit SLOTS bootstrap. Call returns a
typed Future; callers never drive Step. Ready observes the first valid map or a typed stop. Timer capacity/allocation
exhaustion produces RESOURCE; explicit Close preserves the first stop reason.
All commands use an explicit slot; the caller verifies all command keys agree.

The [Redis Cluster specification](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/)
allows full-map refresh on MOVED. A matching-slot MOVED from the current map
generation invalidates once per generation and triggers coalesced refresh.
Late MOVED from superseded generations cannot dirty newer maps. ASK affects
only that route and uses adjacent native ASKING/query. Missing slots request
one fresh map before a terminal no-route outcome. Metadata failures preserve
the previous map and retry at bounded exponential intervals. Each publication
updates future metadata seeds through allocation-free bounded discovery:
eligible masters, the actual query origin and original bootstrap fallback stay
under explicit caps. Failed cohorts advance their starting seed. All candidates
still pass Nodes authority and exact transport/port rules. Cold node setup can
temporarily rotate into fallback before a discovered pool becomes ready.

An accepted native attempt that reports DISCONNECTED also invalidates its map
generation. Its typed result preserves the original delivery state and the
client refreshes topology in the background. Before the first native attempt,
a confirmed outage can keep the call pending across several map generations
during an election. A topology primary reported failed, or a refused initial
primary admission to a maintained pool with zero dispatchable members and a
recorded transport disconnect, establishes the outage. Ordinary overload on
a live pool keeps its existing wait or overload outcome. The pool records
disconnect evidence through failed replacement construction and clears it
on replacement readiness.

Recovery can wait up to `1 + Route.MaxWaits` times for refreshed maps under the
original deadline. Each generation coalesces demands through the existing
refresh timer and backoff. A call keeps the same quota reservation throughout
the wait. If a newer map published before a queued call enters its map wait,
the worker schedules that call against the already published generation.
The router reuses its storage when the next endpoint fits, so
unchanged maps avoid rebuilding the command or allocating another route.
Native admission waits remain cumulative across maps. A missing slot without
confirmed outage still gets one refresh. Recovery depends on a reachable,
authorized metadata node reporting the promotion.
The [APP command adapter](cluster_command.md) calculates a slot from an owned
catalogue and calls the explicit-slot API. The application owns metadata fetch,
snapshot replacement, keyless routing and server-assisted fallback.

## Replica reads in the application

The [runnable example](../examples/cluster_client/README.md) uses the client's
retained topology snapshot to choose a replica for an explicitly
stale-tolerant read. The application classifies the command, computes its slot,
and verifies that all keys agree. It can use its own lag, authority, and
fallback policy. Redis replication is asynchronous.
[Redis Cluster specification](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/#scaling-reads-using-replica-nodes)

Use `xredisClusterNodesGetPool` with the exact endpoint and
`XREDIS_CLUSTER_REPLICA_READ`. The separate pool identity requires a managed
client initialized with `XREDIS_INIT_CLUSTER_READONLY`. Release the topology
snapshot after the lookup and keep the retained pool while needed. Use the
native pool call for the selected read. An application can use the slotted
primary client after an explicit replica rejection before native admission.
Once admitted, keep the native result and delivery state.
[Connection setup](client-init.md)

The `xredis_cluster_replica_warm` profile measures native requests to the
APP-selected READONLY pool and managed primary requests on the same loopback
fixture. Both cases use the prewarmed pools. Selection and setup are outside
the timed window.

Each operation inherits one Call-entry deadline through allocation, queue,
bootstrap, provider waits and native redirects. Cancellation stops only that
operation. COMPLETE means a terminal route and can include a server or uncertain
transport failure: inspect Route.Query.Status/Delivery. Accepted business never
replays except validated MOVED/ASK. Parent stop and observed native result can
coexist. Published fields never change when a stopped native operation later
finishes. Result Native and Route views borrow the result Future.

Admission bounds reserved/in-flight requests, encoded commands and controller
reservations, including cold input copy and overlapping route replacement.
Futures/cancel observers/native parser/FIFO and metadata snapshot storage have
independent bounded duties. Caller-retained completed outcomes leave client
quotas and remain caller-owned. Warm calls copy arguments directly into the
route; bootstrap adds one cold queue copy. Warm operations and their first route share one aligned allocation. Missing-slot
replacement can retain that initial block while its new route owns separate storage.
Completed watch cleanup avoids a second gate check once mechanical retirement
is already observed. The measured caller warm-admission allocation bound is four;
parent Future/cancel observer,
route storage, native Future/wire, gates, references, heap updates and timer
work are real costs. One preallocated min-heap and one timer serve all operations;
ready queues and embedded posts avoid full active-request scans on completion.
Each worker turn services timed waits already due before taking ready work,
within the same phase limit. A wait that becomes due during a busy turn
gets priority on the next turn.

Close seals admission, stops client routes/metadata and retires client watches,
posts/timer and progress-engine pin before Closed. Shared Nodes remains open;
its owner separately closes Nodes and joins engines for native physical FIFO
retirement and factory release. Retained results/maps survive client teardown;
pending native work can retain independent resources. Code remains loaded until
all results are destroyed. The [real Cluster fixture](client-init.md#cluster-replica-readonly)
exercises automatic promotion and replica reads over plaintext and
server-authenticated TLS 1.3. Native Linux, broader authentication and trust
profiles, upstream comparisons, and fixed-arrival, mixed-load, CPU, RSS
and 72-hour acceptance remain open.

The optional [synchronous facade](cluster_sync.md) shares the original entry
deadline and native operation. It refuses both dependent Engines' workers and
returns an owned Future plus the actual native wait status on accepted calls.

## Real slot migration

The real Cluster runner accepts `--tls` for server-authenticated TLS 1.3
on a disposable three-primary Redis Cluster. It checks migration ASK,
actual ownership change and MOVED refresh for SHARDS and SLOTS with
RESP2, RESP3 and AUTO. Every successful attempt uses the expected
transport and destination. Four producer threads submit 32 slotted INCR
calls per case, and the runner checks Redis command counts independently.

Run the optional app command catalogue and synchronous facade in the
same fixture with the static package.

```sh
suite=xredis_cluster_client_sync,xredis_tls
lib=release/gnu/native/$suite/static
probe=out/service-extensions/xredis-cluster-tls
mkdir -p out/service-extensions
python3 tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite "$suite" --kind static --verify
gcc -std=c11 -O2 -Wall -Wextra -Werror -pthread -DXREDIS_MODULE_XREDIS_CLUSTER_CLIENT_SYNC -DXREDIS_MODULE_XREDIS_TLS -Iinclude -Iextlibs/xredis/include extlibs/xredis/tests/interop/redis_cluster_client.c "$lib/libxredis.a" "$lib/libxrt.a" -lm -o "$probe"
python3 extlibs/xredis/tests/interop/run_redis_cluster_client.py --tls --expect-command-routing --expect-sync-routing --expect-command-cache --client "$probe" --report "$probe.json"
```

The runner needs a TLS-enabled Redis and OpenSSL. It generates temporary
test certificates and publishes only their digests. Build with
`XREDIS_INTEROP_EXPLICIT_SLOT_ONLY` and omit the three `--expect` options to
exercise only explicit-slot calls. Omit `--tls` for the plaintext fixture.
