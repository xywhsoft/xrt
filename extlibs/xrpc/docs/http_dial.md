# Asynchronous HTTP connection sources

`xrpcHttpDial` resolves an explicit DNS name through an application-supplied
bounded Core resolver, then races TCP candidates using Core's dial controller.
`xrpcHttpDialAddresses` instead retains a prepared immutable endpoint list, with
each endpoint's port preserved. No resolver is created or consulted on that path.
Both reuse the ordinary HTTP client for calls, notifications, batches, result
quotas, response validation and shutdown. No application operation is replayed.

DNSName is independent of the HTTP Host authority and request Target. Name bytes
are copied and NUL-terminated before return, with default 1024/hard 65535 bytes;
embedded NUL is rejected and there is no IDNA conversion. Prepared lists contain
at most 64 entries; family filters candidates and eligible ports must be nonzero.
Defaults are dual stack, eight attempts, 250 ms fallback and 30 s connection time.
TCP limits come from the HTTP config's Stream, not a second competing policy.

The additional monotonic connection deadline begins at factory entry, covering
copying/admission, owner-worker queueing, DNS and losing-candidate retirement.
Zero means immediate asynchronous timeout; -1 disables the additional deadline.
The adapter uses one absolute owner-worker timer; native Dial's relative timeout
is disabled rather than restarted after factory work. Each queued RPC also keeps
its own API-entry deadline/cancellation. An unsent queued stop does not poison a
healthy pending connection. DNS/connection failure, resource admission failure
and timeout remain distinct typed outcomes, with immutable native cause chains.

The connection source is product-private cold state, not another runtime or hot
IO dispatch layer. The client stores one source pointer; descriptor/name/list and
timer state live in one separate allocation. The client and native owned-Done
callback each hold a real source reference. Its trace refuses opaque ownership
graphs. Core's physical callback retirement clears the client source pointer and
releases its creator credit. Source metadata and the completed Dial owner leave
immediately, instead of remaining for the whole open connection lifetime.

All mutable client/connection source work runs on its network worker. DNS work is
on resolver workers. An early stream Open event does not publish readiness until
the native controller has transferred the winner after losing-resource drain.
Close requests dial cancellation and observes source retirement as well as actual
winning-stream closure, including a late winner after close won the public race.
No polling loop is added for pending DNS. A running noncooperative lookup can
delay physical Closed; request Futures can still time out or cancel independently.

Resolver ownership remains with the application through Client Closed. Retire and
join it separately afterward, including lookup callbacks and worker/TLS cleanup.
Future results retain no resolver, client or engine after their operation retires.
Failures before factory success preserve caller ownership/output; successful
construction owns one creator client reference, consumed with Close then Release.
Plain TCP is explicit. TLS, verification/authentication, retries after connection
loss and pooling remain separate product work; there is no cleartext fallback.

The runnable example uses OS localhost resolution and a self-contained HTTP peer;
an optional port argument targets an external localhost HTTP peer for interop.
Tests add deterministic resolver callbacks, worker barriers, name reuse, candidate
failure, queued ordinary/notification/batch work, request/connection deadlines,
cancellation/close, resolver overload and allocation thresholds. The cold-connect
benchmark uses serial closed-loop IPv4 localhost lookups with the app cache off;
its complete client lifecycle observations are not external DNS performance or
fixed-arrival service capacity. Full native Linux/long-run/performance acceptance
remains pending.

Per-cycle cold timing ends at client Closed observation. Mechanical callback
return tails retire at final engine join outside that timing; it is not a claim
that every byte of final teardown is timed within its originating cycle.

A deterministic close-publication probe is runnable after the modular test build:
`python -X utf8 extlibs/xrpc/tests/interop/http_dial_close_race.py`. It injects a
public Close between completion state inspection and publication by interposing
only the test translation unit's mutex calls. The production source is included
and linked with the other actual modular objects; no production hook is added.
It exposed and now guards against reopening a client after concurrent Close.
