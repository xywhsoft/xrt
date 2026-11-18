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
The optional [command adapter](cluster_command.md) calculates complete routing
keys from a caller-owned catalogue before admission, with explicit keyless
policy. Command metadata cache and server-assisted metadata fallback remain
work. Optional synchronous facades are described below.

## Replica reads

Use `xredisClusterClientCallReplica` for an explicitly stale-tolerant read.
The caller classifies the command, computes its slot and verifies that all
keys agree on that slot. Ordinary, synchronous and catalogue-based calls
select the primary. Replication is asynchronous and a replica can return
older data. [Redis Cluster specification](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/#scaling-reads-using-replica-nodes)

The slot index locates the shard. The replica call scans only that shard's
contiguous node entries, rotating the start position across calls. It chooses
the first usable replica with an eligible health, endpoint and transport
port. SHARDS requires an online replica. Legacy SLOTS reports no health and
uses the transport of the metadata query. Endpoint resolution uses the
original metadata query host for SOURCE and rejects UNKNOWN.

The exact endpoint goes through Nodes authorization and a separate maintained
pool identified by `XREDIS_CLUSTER_REPLICA_READ`. The factory receives the same
marker and uses `XREDIS_INIT_CLUSTER_READONLY`. Pool maintenance drains clients
that lack this flag, and native readiness requires the server acknowledgment.
Optional ROLE checks can also run during setup. [Connection setup](client-init.md)

When a replica is absent, denied, waiting on its provider, or has no native
admission capacity, the route can use the eligible primary under the original
deadline. The primary has its own authorization and ordinary pool. A cold
replica pool can warm while the first read goes to the primary. Once a native
attempt accepts the query, another attempt requires validated matching-slot
MOVED or ASK. Redirect destinations use ordinary pools. A timeout, disconnect
or cancellation keeps its native certainty and ownership contract.

The topology stores two bytes per shard plus a two-byte sentinel in the
existing allocation. Replica admission scans at most the selected shard's
node count and copies the eligible primary host into the same route allocation
when needed for fallback. It reuses the admission gate. Benchmark cases in
`xredis_cluster_replica_warm` measure prewarmed primary and replica requests
over loopback with pipeline depth 32. Wider replication lag policies and native
deployment performance remain qualification work.

```sh
python tools/measure_performance.py --compiler gcc --manifest extlibs/xredis/config/modules.json --config extlibs/xredis/config/performance_profiles.json --profiles xredis_cluster_replica_warm --warmups 1 --repeats 3
```

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

Close seals admission, stops client routes/metadata and retires client watches,
posts/timer and progress-engine pin before Closed. Shared Nodes remains open;
its owner separately closes Nodes and joins engines for native physical FIFO
retirement and factory release. Retained results/maps survive client teardown;
pending native work can retain independent resources. Code remains loaded until
all results are destroyed. Production/native-Linux/TLS/auth/trust/upstream/
fixed-arrival/mixed-load/CPU/RSS/72h performance acceptance remains open.

The optional [synchronous facade](cluster_sync.md) shares the original entry
deadline and native operation. It refuses both dependent Engines' workers and
returns an owned Future plus the actual native wait status on accepted calls.
