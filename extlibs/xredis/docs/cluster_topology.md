# Cluster topology and redirections

Developing leaf `xredis_cluster_topology`. Computes binary key slots, classifies
MOVED/ASK replies, and builds immutable SHARDS/SLOTS maps. Automatic Cluster
connections, redirect execution, refresh and failover remain subsequent work.

One allocation holds a fixed32KiB index and node/range descriptors, plus one
retained immutable Reply reference; there is no text copy. MaxOwnedBytes counts
the complete retained Reply ReservedBytes too. Construction scans shapes twice
and visits assigned slots twice. Hot lookup validates output spans/aliases,
then reads the slot index and copies a descriptor. It performs no allocation,
lock, reference-count update or text scan. No worker/runtime/network is added.

SHARDS accepts RESP2 alternating arrays/RESP3 maps, discontiguous ranges,
replicas, health, signed64-bit offsets and separate TCP/TLS ports. SLOTS accepts
old two-field nodes and optional IDs/network metadata. Repeated per-range
nodes remain separate descriptors in wire order; identity deduplication and
address/credential selection are not inferred. Unknown health/role text remains
visible and is not assumed eligible. SLOTS health is explicitly UNREPORTED.
Null/empty endpoint uses the actual host used for the topology query; `?` is an
unknown node and must not use that fallback. Keep the query-origin association,
select eligible health and a nonzero transport port before dialing. SLOTS Port
belongs to its query transport; SHARDS Port/TlsPort are separate. Endpoint text
rejects control/space/URI delimiter bytes; real IP/DNS validation is the dial
factory's responsibility. Snapshot/node views require a live topology ref.

Overlapping slots, bad ranges, duplicate known map fields and missing/multiple
masters fail atomically. Empty shard ranges and partial maps are valid unless
RequireFullCoverage is set. Future fields/attributes remain in the reply owner
and count toward limits. Zero is a real cap; the bounded Reply reader still owns
its parsing costs. See the public header for all defaults and output rules.

MOVED/ASK views borrow their Reply. Empty host uses the original request host.
Both raw IPv6 and bracketed hosts are represented; brackets are stripped and
count toward the endpoint cap. Slot/port decimal fields have at most5 digits;
ports are1..65535. ASK requires exclusive ASKING+one query and must not change
permanent slot ownership. This component executes/retries no request, so no
transport error, timeout or cancellation silently causes business replay.

Independent protocol implementation; no Redis implementation source included.
Sources: [Cluster specification](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/),
[SHARDS](https://redis.io/docs/latest/commands/cluster-shards/),
[SLOTS](https://redis.io/docs/latest/commands/cluster-slots/).

[Runnable offline example](../examples/cluster_topology/README.md).
Production/native Linux/mixed-load/72h acceptance remains open.
