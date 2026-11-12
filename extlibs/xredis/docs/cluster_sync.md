# Synchronous Cluster facades (developing)

`xredis_cluster_client_sync` adds `xredisClusterClientCallSync` for an explicit
slot without requiring command metadata. `xredis_cluster_command_sync` adds
`xredisClusterClientCallCommandSync` with the complete immutable-catalogue key
checks and explicit keyless policy of CallCommand. Both reuse the existing
operation, Future, bounded native pools, redirects and progress owner.

## Outcomes and ownership

The return value is admission. Only ACCEPTED writes the caller's
`xredisclusterwait { Wait, Future }`. Every refusal preserves the whole output,
all input ownership and the original asynchronous typed decision. Metadata
unknown/incomplete/cross-slot/arity/limit/keyless refusals admit no business.

ACCEPTED transfers one Future reference in every case. Wait is the actual
`xrtFutureWait` result, distinct from admission and from Redis execution. On
XWAIT_OK inspect the immutable `xredisclusterclientresult`, its Route status and
Query status/delivery. A server error is a completed request; a locally stopped
operation may retain a still-pending Native Future. Neither condition authorizes
blind business replay. If the native wait fails, Wait reports that failure and
the accepted Future is still transferred, possibly pending. The caller can wait,
cancel or destroy it later. No accepted operation is hidden by a failed wait.

Input descriptors/bytes and catalogue are borrowed during validation/admission;
the normal asynchronous path owns its copies. This facade adds no catalogue
reference. Callers keep stable owners through return and must not concurrently
modify borrowed storage. Result must be disjoint from Client, current map, input
descriptors/bytes and, for metadata calls, the catalogue and its retained Reply.
The complete output span is checked, including its trailing Future field.

## Deadlines, workers and retirement

Capture one monotonic timestamp at synchronous function entry. Worker/output
checks, metadata validation, allocation, bootstrap, provider/admission waits and
redirects consume the same original deadline. Timeout -1 uses Route.Timeout,
zero is immediately due, and the maximum is24 hours. Waiting adds no new
relative operation timeout. Metadata remains bounded synchronous CPU work.

Every worker of the progress Engine and the maintained Nodes Engine is refused
before admission with ERROR/XERR_STATE, including other workers in those
Engines. Nodes can use a different Engine. The private predicate reads its
Engine under the node gate, protecting the existing Engine pin against Close.
The Client gate protects the progress query and its retained Nodes owner until
Finish has marked Finished. Node gates call no Client callbacks; policy/factory/
Drop remain outside gates. Closed Client metadata is safe after Engine teardown.

Other threads can Close while the caller waits. The returned typed stop does not
join remaining native writes/FIFO obligations. Client Close, Nodes Close and
Engine physical retirement retain their separate original contracts. There is
no new external cancellation-token binding; independently cancellable producers
use the existing asynchronous calls. An unrelated Engine worker is permitted,
with application-level callback/Engine dependency cycles still the caller's
responsibility.

## Costs and verification

The facade adds one Client gate, one Nodes gate while still active, two public
Engine-current queries, full output-span/argument-alias checks and the same native
Future wait that a caller would otherwise perform. Metadata adds its existing
extraction/CRC checks. No new runtime object, worker, timer, queue, payload copy
or result allocation is added. Existing asynchronous reservation, copy, Future,
watch, atomic and physical cleanup costs still apply. No zero-overhead claim.

Tests cover cold/default/zero calls, binary retained outcomes, both Engines and
all their workers, overload, closed metadata after Engine destruction, local
typed key refusals under an exhausted allocation budget, full output aliases,
MOVED/ASK, held native timeout/Close, OOM prefixes and concurrent callers. Held
caller budgets compare async+Wait/sync live blocks and bytes while native work is
pending, preserving the same four-allocation warm bound.
Selected single tests interpose caller-only clocks and native waits, inspect
actual operation/route/native deadlines, and prove wait-error Future transfer.
The standalone examples own literal peers and require no external service.
The real Redis Cluster matrix conditionally uses both synchronous APIs while
preserving actual owner-change/ASK,32 unique INCR effects and retained results.

The benchmark uses one outstanding binary GET. Modes0/1 compare explicit
async+Wait/sync; modes2/3 compare metadata async+Wait/sync. Same configuration,
Future, peers, data, original budgets and affinity are used. It includes validation,
queueing and caller-observed completion, with setup/catalogue/bootstrap/sample
storage outside the timer. Separate heap runs include native callback overlap.
It is closed-loop local evidence, not fixed-arrival, multi-producer contention,
TLS, independent comparator, native Linux, CPU/RSS or>=72h acceptance.

Automatic catalogue fetch/replacement, authorized exotic-command extraction,
replica policy and complete four-library production/performance gates remain open.
