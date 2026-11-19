# Automatic Sentinel pools

The developing xredis_sentinel_pool leaf integrates bounded discovery and
whole-generation replacement into the existing native pool worker. Ordinary
acquisition, lease calls and close use native APIs directly. Every missing slot
rediscovers Sentinel and verifies ROLE; an unchanged endpoint refills only that
slot. A changed verified endpoint fences old admissions through the existing
aggregate quota gate, retires old members/discoveries in WorkItems batches,
then reopens after physical retirement. Accepted old requests preserve native
uncertainty and are never replayed. Each attempt has one original deadline;
acquisitions retain their own deadlines and cancellation through outages.

See [public discovery rules](sentinel.md) and the
[Sentinel client specification](https://redis.io/docs/latest/develop/reference/sentinel-clients/).
Factories select independent Sentinel/data credentials, TLS trust and transports.
MaxConnecting counts physical tails. Transient reachability/timeout/stale-role
failures retry with bounded jitter. Other discovery failures close the pool;
Info retains typed status after close. Notify/Drop run on the existing pool
worker outside gates and must not block. Native context/lookup/trust ownership
continues through Drop. Final watch/post release frames may return afterwards;
join the engine before unloading callback code.

No worker/runtime or per-business request Future/proxy is introduced. A
Sentinel-only cold state allocation holds configuration, bounded service/host
storage and one discovery/watch state per slot. Native pool/client public
layouts remain unchanged. ROLE verifies point-in-time identity; it cannot
establish consensus/freshness. Enable the optional [Sentinel event listener](sentinel_events.md)
to request verified refreshes from native subscriptions.
[Runnable example](../examples/sentinel_pool/README.md).

## Replica pools

Initialize the usual Sentinel pool configuration and call
`xredisSentinelReplicaPoolCreate` with the replica connection factory and a
list bound. The initial slot establishes one chosen replica before parallel
refill. Every subsequent connection rediscovers Sentinel and verifies with
ROLE that the candidate is a connected replica. A repair tries the current
exact host and port first when they remain eligible in the reply. This keeps
healthy members in place when Sentinel changes list order. If that endpoint
reports that it has become a primary before the seed reports a promotion,
the worker retires the probe and requests fresh topology before trying
other rows from that seed. When Sentinel reports a promoted endpoint,
replacement connections must already follow it.

Each generation serves one verified endpoint. A different verified replica
closes the old generation's admission gate and drains its connections before
the new generation can serve acquisitions. Accepted old requests preserve
their native outcome. An empty eligible list remains retryable with the
existing jitter and per-attempt bounds. Unknown services and fatal discovery
errors still close the pool. The status snapshot retains NO_REPLICA while a
replica is unavailable. Existing acquisition deadlines span the outage.

The pool uses native lease calls with the caller's command permissions. It
provides point-in-time verification on connection setup; replica freshness,
read-after-write expectations and routing across multiple replicas require
an explicit application policy. Missing slots trigger discovery even when
active refresh is disabled.
[Sentinel client specification](https://redis.io/docs/latest/develop/reference/sentinel-clients/#connect-to-replicas).

## Active refresh

Call `xredisSentinelPoolRefresh` to request one probe, for example from a
Sentinel event handler after filtering for the intended service. Event data
serves as a hint. The worker queries Sentinel and verifies the returned
candidate with ROLE. Call `xredisSentinelPoolSetRefreshInterval` to enable
periodic checks (100 ms to 24 hours). The default is zero. The first check
runs when the interval elapses and every subsequent interval starts after
the previous probe fully retires. If the pool is filling, the check waits
until the pool has all members.

Repeated hints coalesce before a probe starts, and during the probe they
may queue one more check. The probe uses the existing worker, wake post and
timer. The sole probe closes and retires before any required refill begins.
Its temporary seed and target use the original discovery limits. The
connection counters continue to describe pool members and their repair.
An unchanged verified endpoint keeps the current members. A new verified
endpoint fences old leases and drains the old generation before refill.
A probe failure updates LastStatus and retains the current generation.
Both APIs are safe to call from Notify. Close waits for the probe and its
native observers before Drop. An accepted hint has no completion Future.
[Sentinel event guidance](https://redis.io/docs/latest/develop/reference/sentinel-clients/#subscribe-to-sentinel-events-to-improve-responsiveness).

## Configure Sentinel seeds

Set `Discovery.SeedCount` and `Discovery.PreferredSeed`, and resolve each
stable index in `Discovery.ConnectSeed`. The pool rotates those indexes on
transient failure and remembers the last seed that led to a verified endpoint.
The APP owns the address list, endpoint policy, credentials and TLS settings
through the pool's Drop callback.

The [Sentinel pool example](../examples/sentinel_pool/README.md) issues
`SENTINEL SENTINELS` on an APP-owned client and enables a preapproved backup
while the pool runs. Applications can reserve a bounded number of indexes
and initially return UNREACHABLE for unassigned slots. Publish updates to
those slots with synchronization appropriate to the pool's callback threads.
[Redis Sentinel peer discovery guidance](https://redis.io/docs/latest/develop/reference/sentinel-clients/#sentinels-list-automatic-refresh).
