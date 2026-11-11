# Automatic shared pools

The developing `xredis_pool_shared_connect` leaf combines native shared dispatch
and the existing pool connector. `xredisSharedPoolConnect` starts/refills a fixed
number of slots. Use `SharedPoolCall[Async]`, `PoolConnectInfo`, and the ordinary
pool Info/Close/Closed/Retain/Release operations on the returned native handle.
There is no new runtime, pool handle type or per-business-request proxy.

The connector owns bounded attempts, setup deadlines, exponential jittered
retry and physical retirement, using the original factory/Data/Drop contract.
Success consumes Data/Drop; rejection preserves output and the caller's context
obligation. A factory runs serially on the pool worker, outside gates, and
transfers one private managed client reference. It must maintain a common
service, credentials, database and negotiated RESP profile across generations.

A replacement becomes eligible only after native readiness validates the profile
and initialization storage retires. The shared owner activates before the pool
worker publishes its ready-list entry. A retiring member is removed under the
same gate used for selection. A selected caller retains that client; native
admission rechecks its exact claim/state. Old constructor/request/timer/cancel
obligations prevent unclaim/reuse. A stale selected client cannot acquire the
same slot's successor generation. No pool/client/request/slot/lease header grows.

Every accepted shared business call remains a single native request/Future.
The steady shared call path is unchanged. Reconnect never replays accepted
requests. Sent timeout/cancel preserves uncertainty, native FIFO/quota and drain
obligations; drain expiry retires the member before replacement. Completed
immutable outcomes pin neither the pool nor engine.

An outage, no ready member or saturated bounded probe returns OVERLOADED
immediately with output untouched. There is no acquisition queue or outage wait
and an API timeout does not wait for repair. Rejected constructors may probe
another eligible member; accepted requests never migrate. Callers choose their
own bounded admission policy. Exclusive Acquire rejects this mode. Blocking
traffic should use separate pools; there is no key affinity or cross-member order.

PoolClose fences admission and stops attempts, then closes members/candidates,
retires physical obligations/timers/callbacks, calls Drop once outside gates and
resolves Closed. Timer allocation failure closes the pool rather than spinning.
Factory/context/TLS/resolver/verifier/signer material remains valid until Drop.
Retained pool metadata keeps connector counters but no live context ownership.

See the [runnable example](../examples/pool_shared_connect/README.md). This is
shared node connection maintenance; full Cluster routing, topology refresh,
redirect/deadline orchestration and production acceptance remain separate work.
