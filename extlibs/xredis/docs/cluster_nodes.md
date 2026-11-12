# Bounded maintained Cluster nodes

The developing xredis_cluster_nodes leaf supplies a thread-safe GetPool adapter
for bounded Cluster routes. One cache owns one immutable credentials/session/
transport policy; Authorize checks every lookup, including hits, outside gates.
An exact copied host/port/TLS key shares one maintained native node pool across
all slots and MOVED/ASK reasons. Per-node factory callbacks run on pool workers;
different nodes may call concurrently. All must be nonblocking. Stable copied
factory addresses have Slot/Reason zero. There is no hidden DNS or TLS downgrade.

Create preallocates bucket/entry/host storage with explicit node/host/byte caps;
no factory runs until a lookup. Concurrent misses reserve one entry and coalesce
creation; opening/retiring/full nodes return WAIT without ownership transfer.
MaxLookups bounds concurrent authorization/constructor duties; saturation invokes
neither policy nor setup. Cache Close can add one mechanical API duty.
READY transfers a retained pool ref and promises pool existence, not readiness.
Native overload and route waits share the original route deadline. Maintenance
has separate finite attempt/retry bounds and can outlive one route. A caller
with Remaining0 starts nothing. Pool/member/request/command/attempt maxima have
checked aggregate bounds: MaxNodes times the configured per-node limits, with
MaxConnections enforcing the product. Retained caller results have independent
storage obligations. A hostile collision chain still has at most MaxNodes
entries; hashing is not cryptographic and no constant-time adversarial claim is
made. Warm lookup validates/scans/hashes the address, calls policy, takes two
cache gates and a pool ref; it allocates no storage. Misses allocate ordinary
native pool/connector/Future state. These are real costs, not zero overhead.

Explicit Retire fences the endpoint and closes its native pool. Accepted work
keeps typed possibly uncertain outcomes and never replays. Already selected pool
refs may reject a later admission CLOSED. Entry reuse waits for physical native
PoolClosed notification release, so a new generation never overwrites live
factory context. Capacity does not cause LRU aborts of busy business calls.
Capacity exhaustion waits within caller limits; full topology refresh/retirement
policy will coordinate unused nodes in the managed client layer.

Close seals lookup, includes concurrent authorizations/constructors, closes all
current or racing pools and invokes Data/Drop exactly once outside gates. Closed
publishes after those duties and the engine pin retire. Mechanical callback/
reference release tails can still be returning; join the engine before unloading
code. Retained closed cache metadata/Futures own no engine, factory data or pool.
Caller holds a cache ref for each API; Release alone does not close an open cache.

Automatic SHARDS refresh/coalescing, bootstrap, persistent MOVED map updates,
replica policy and a managed asynchronous client facade remain work. Native Linux,
real TLS/auth/trust, fixed-arrival/mixed-load/CPU/RSS/72h production acceptance
also remain open. See the [runnable example](../examples/cluster_nodes/README.md).
