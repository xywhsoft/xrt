# Bounded Cluster route execution

The developing `xredis_cluster_route` leaf is a nonblocking routing state machine
for an explicit hash slot. It selects an eligible master from an immutable
SHARDS/SLOTS snapshot and automatically follows matching-slot MOVED/ASK replies
within one original monotonic deadline and bounded hop/wait counts. ASK uses
one adjacent native shared ASKING/query request. The implementation follows the
[Redis Cluster redirection specification](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/#redirection-and-resharding).

Create copies command descriptors/payload once into one bounded controller
allocation. The topology and caller input can then be released/changed. Current
and last-attempt endpoint buffers initially reserve their actual host length;
a longer redirect grows them once within explicit host/storage caps. The copy,
allocation, scans, clock checks, provider pool refs and Step work are real costs.
Native admission additionally copies its wire per accepted attempt. No zero-copy
or zero-overhead claim is made. Native Future/reply storage has separate limits.

The caller serializes all APIs. Each Step advances one bounded phase without
blocking. MORE needs another immediate turn; WAIT observes Pending or NextWake,
whichever comes first. Pending retains the native handle even when completion
races the preceding WAIT/NextWake observation, so callers can subscribe without
losing that wake. A resolved handle waits immediately; Step consumes it.
One creation-entry deadline covers caller scheduling,
provider/admission waits and all accepted attempts. Native calls inherit the
same Entered/Timeout pair and arithmetic, so redirects never reset the budget.
Cancellation/timeout publishes an immutable parent snapshot and asks its native
operation to stop. Independent native FIFO/wire/timer/cancel duties survive the
controller; Destroy is nonblocking and never closes a shared member itself.

GetPool authorizes exact endpoint/transport/credentials and returns one retained
fixed or maintained shared pool. READY transfers that ref; WAIT transfers
nothing. RetryDelay/MaxWaits bound nonaccepted admission wait. No callbacks run
inline during Create. Provider code/Data are borrowed through DONE/Destroy;
DONE retains neither pool, engine nor context reference. Pools keep their own
close/security-material obligations. No hidden resolver/port/TLS downgrade is
performed. SOURCE uses the actual topology/request origin; UNKNOWN and missing
ports/slots fail closed. Legacy SLOTS Port belongs to its query transport.

Only a well-formed REPLIED MOVED/ASK with the expected slot can trigger another
native attempt. ASK applies to this request only. Failed ASKING control, other
server errors, uncertain transport results, timeout/cancel and malformed replies
never authorize replay. COMPLETE denotes a terminal native outcome, including
server/transport failure: inspect Query.Status/Delivery and optional Control.
A parent stop can coexist with an already observed successful native query.
RequestObserved never changes after DONE. ResultFuture retains the last native
attempt, which can still be pending after a stop; it is not a proxy success
Future. Endpoint views borrow Route; native reply/cause views also borrow that
Future. Address is the current candidate, AttemptAddress identifies the native
observation. Before DONE copy views before the next Step.

[Maintained node caches](cluster_nodes.md) provide a bounded GetPool adapter.
Coalesced refresh, persistent MOVED-map updates, seed/bootstrap,
replica policy and a fully managed asynchronous facade remain subsequent work.
The provider receives validated MOVED/ASK reason so those layers can reuse this
executor; the input topology is never mutated. The broader four-library
performance/upstream/native Linux/mixed-load/fixed-arrival/72h gates remain open.
See the [runnable example](../examples/cluster_route/README.md).
