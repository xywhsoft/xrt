# Sentinel event listening

The optional xredis_sentinel_events module adds a reconnecting Redis Pub/Sub
listener to automatic master and replica pools. Start it with
xredisSentinelPoolListen, then inspect xredisSentinelPoolEventInfo.
The existing subscription session and pool use the same engine worker.
The listener connects through the pool's indexed seed factory.
On reconnect it rotates through the configured seed indexes.

The listener subscribes to +switch-master. It checks the exact service
token and the documented host/port syntax. The announced addresses stay
unused. Accepted messages request a separate Sentinel lookup and ROLE
verification before the existing pool can change generations. The first
subscription ACK and each restored subscription also request a lookup.
This covers startup and reconnect gaps in at-most-once Pub/Sub delivery.
Periodic refresh and reconnect-triggered discovery are independently
available if no event arrives.
[Redis Sentinel client guidance](https://redis.io/docs/latest/develop/reference/sentinel-clients/#subscribe-to-sentinel-events-to-improve-responsiveness).

The listener reuses the Client's bounded push queue and drains at most
WorkMessages per pool turn. A burst collapses into one pending signal,
paced by HintInterval. Retry backoff and its setup deadline belong to the
existing subscription session. Fatal session setup, ACL and memory failures
are visible through EventInfo while data pool leases keep their existing
behavior. The pool waits for the subscription session and receive Watch
before calling the shared Drop callback. The terminal listener snapshot
can be read after the engine shuts down.

[Runnable local example](../examples/sentinel_events/README.md).
