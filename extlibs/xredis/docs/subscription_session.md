# Automatic desired subscription sessions

The developing xredis_subscription_session leaf owns one reconnecting subscriber
on an existing Engine. A serialized nonblocking factory can use the existing
numeric, DNS, prepared-address, TLS and authentication factories. No worker,
shared runtime, proxy message queue or ordinary-command retry layer is added.

Creation retains Store and exclusively claims its edit authority. Its full
configured count/name bounds must fit the native registry, including future edits.
Read-only snapshots remain available. Direct Store Apply is OVERLOADED while
claimed. SessionApply commits local intent and coalesces a controller wakeup;
its acknowledgement is local, not a server acknowledgement. Inputs are copied by
the bounded immutable store before return. Accepted edits retain a close duty;
close cannot release Store or factory context while their constructors remain.

One candidate connects, negotiates the selected RESP profile and restores a
captured desired revision. Factory delay, initialization, controller queueing and
all restore batches share the original attempt deadline. Each live reconciliation
has its own complete AttemptTimeout budget. A persistent service can run forever;
MaxFailures=0 explicitly allows unlimited consecutive transient failures with
bounded resources, while a positive value closes at that consecutive-failure cap.
Exponential retry with non-cryptographic jitter is bounded by RetryMin/RetryMax.
A managed candidate that closes before factory return is classified by its
native transport outcome, rather than mistaken for a permanent profile error.
No next generation starts before the previous native physical close and ownership
retirement. Readiness resets the consecutive failure exponent after restoration.

Live updates merge immutable base/target pointer vectors. Only absent keys are
unsubscribed; only new keys are subscribed. Removal precedes addition so temporary
registry occupancy fits configured bounds. Unchanged names remain subscribed on
the same transport. Each scheduled turn inspects at most1024 merge entries and
submits OR observes one bounded native batch. Shard batches contain one cached
slot. Finish a captured revision even when newer intent arrives, then reconcile
the latest snapshot; churn does not force reconnection or abort partially applied
live changes. Explicit unsubscribe-all becomes a concrete desired-set delta.
Endpoint selection, MOVED handling and Sentinel/Cluster topology remain outside
this controller and are not claimed as implemented HA routing.

Only the controller holds the native control claim through the generation. Native
requests borrow its token during admission and retain no controller pointer.
Current returns a retained native Client only at a fully applied desired revision;
use it for receive/observation and Release it. Do not issue controls, adopt or
claim it. Native Readable/TakeBatch/TakePush run directly. Old retained generations
can still be drained after retirement; no stream order across reconnections is
invented. Messages, returned snapshots and terminal metadata outlive Engine.

Notify is a coalesced state snapshot on the controller worker, outside gates.
It can reenter Info/Apply/Current/Close; callbacks must not block. No per-message
controller callback or notification allocation is introduced. Public Close clears
current admission immediately and schedules retirement. Shared Closed observers
ignore cooperative token cancellation; Close is explicit. Fatal server/protocol/
profile/authentication/limit/memory errors close the controller. Transient factory
failures, transport loss and attempt timeout back off. Maintenance OOM fails closed
without retry-spinning. LastFailure is historical; Closed.Status is the terminal
stop reason, with OK for explicit close even after recovered outages.

The controller has three embedded Watch slots, one coalesced post and at most one
controller timer. Existing native Client/control/resolver resources keep their own
bounds. Watch reuse waits for its Release publication, uses nonwaiting Detach,
and never waits in a worker. Physical close releases generation/Store/context
access, calls Drop once on the controller worker, unpins Engine and publishes its
typed observer. Final post/Watch reference-release frames can finish after that
publication; they retain no factory context or Engine access. Quota-zero checks
join the owning worker turns, rather than equating logical signalling with release.

[Redis Pub/Sub](https://redis.io/docs/latest/develop/pubsub/) remains at-most-once:
automatic resubscription cannot recover offline messages. Real tests use
[CLIENT KILL](https://redis.io/docs/latest/commands/client-kill/) only on a disposable
PID/run-id-verified fixture to induce actual server-side transport loss.

The [offline example](../examples/subscription_session/README.md) executes every
session API. Independent state peers, overlapping target changes, large unchanged
sets, concurrent edits/current readers, original-deadline probes, accepted-edit
close, OOM, native EOF and TLS tests are separate from production. Benchmarks time
whole end-to-end live edit or EOF-to-restored-current operations, including queue
wait and configured retry delay. Heap instrumentation covers the whole loopback
fixture; its bounded peer ring grows cold on early generations. These are closed
loop component baselines, not fixed-arrival/upstream or >=72h production acceptance.
