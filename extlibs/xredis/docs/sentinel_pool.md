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
establish consensus/freshness. Optional Sentinel refresh/events, replica
selection, Cluster and complete production acceptance remain separate work.
[Runnable example](../examples/sentinel_pool/README.md).
