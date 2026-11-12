# Metadata-checked asynchronous Cluster submission

`xredis_cluster_command` composes the existing immutable
[command catalogue](command_keys.md) and [asynchronous client](cluster_client.md).
CallCommand evaluates all routing keys before native admission, selects their
common slot and reuses the existing request/Future/redirect/close machinery.
It performs no metadata request or hidden server fallback. Applications obtain
and replace authorized catalogue snapshots and retain an owner during each call.

Typed refusals distinguish unknown, unsupported/incomplete, cross-slot, invalid
arity/extraction and bounded-work/argument failures. None admits business or
changes the client admission counters. Keyless commands require an explicit
fallback slot; NO_SLOT returns KEYLESS. This separates absence of keys from
unknown metadata. Variable access flags never select replicas or authorize replay.
Native capacity/close/error decisions retain their existing counted behavior.

Only ACCEPTED writes Future and transfers one caller-owned Future reference.
It contains the existing immutable client result; inspect Route/Query/Delivery,
not only future resolution or COMPLETE. The catalogue is borrowed synchronously
and is not retained after return. Native admission copies command data as before;
warm calls retain the same four-allocation bound and original byte ownership.
Cold bootstrap still adds its existing extra copy. Retained outcomes/maps and
native physical duties follow the client contracts.

The entry monotonic timestamp precedes spans, metadata and slot calculation and
is passed unchanged into the common admission path. Timeout/default handling
does not reset after metadata. Original Entered/Timeout survives route creation,
queue/bootstrap, provider waits and native MOVED/ASK redirects. Metadata CPU is
bounded by the catalogue's argument/search/key-byte limits and is not a separate
interruptible operation. An expired admitted operation publishes typed TIMEOUT;
it never receives a fresh timeout interval after evaluation.

The adapter adds an immutable-owner/output check, an argument/output alias pass,
catalogue lookup/extraction/CRC, a typed-status switch and the shared admission
call. It adds no payload copy, catalogue reference, worker, timer, queue or runtime.
Metadata has real scan/CPU costs. Selecting the leaf also enables a private
common admission helper; the explicit-slot API remains available. The feature-
excluded client keeps the original function body and code path.

The dedicated workload uses the same literal-peer GET, binary key/value, queue,
workers, payload and Future checks for explicit slot, explicit catalogue-then-Call
and integrated CallCommand modes. Explicit slot omits metadata checking; manual
composition captures its Call deadline after evaluation. These distinctions
prevent a broad same-function or zero-overhead claim. Full real TLS/auth/trust,
automatic command metadata/cache policy and server-assisted exotic extraction,
replica policy, sync facade and native/performance/72h acceptance remain open.

The optional [synchronous facade](cluster_sync.md) shares the original entry
deadline and native operation. It refuses both dependent Engines' workers and
returns an owned Future plus the actual native wait status on accepted calls.
