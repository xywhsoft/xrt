# Subscriber receive notification and batch ownership

`xredis_subscriber_notify` is a developing optional leaf over the existing native
subscriber. It adds `xredisSubscriberReadable` and `xredisSubscriberTakeBatch` to
existing numeric, DNS/prepared and TLS Clients. No Core API, worker or additional
runtime is introduced. The basic subscriber can be selected without this leaf.

`xredisSubscriberReadable` returns one owned reference to a shared epoch Future.
Its only terminal result is RESOLVED with NULL value. Wait or register a Core
Future Watch, release that Future reference, and recheck the queue. A hint never
reserves a message: another consumer can drain it first. CLOSED means physical
close with an empty queue. Queued messages remain readable after physical close
and after Engine destruction while the caller retains the Client.

An empty open queue has at most one pending epoch and one in-progress replacement
constructor. Repeated observers retain the existing Future without allocating.
After an epoch is signalled and drained, the next observer lazily constructs its
replacement outside Gate. Concurrent construction returns OVERLOADED; allocation
failure returns ERROR. Refusal leaves the output unchanged. Current epoch state
is stored only in the optional subscriber allocation. Its Core Promise/Future
storage is separate from RegistryBytes; previous completed epochs are owned by
callers that retain them. Readiness observers do not occupy request slots or
consume the Client's accepted-command quota.

A message detaches the pending Promise under the existing queue Gate and resolves
it outside Gate on the owning IO worker. Physical close resolves any remaining
pending epoch before releasing Engine activation. Newly ready epochs resolve
before publication, so no user callback executes under Gate or on a constructor
thread. Core WatchAdd on an already-ready Future returns READY without invoking
Notify/Release. A constructor flag blocks physical-close publication until its
unexposed allocations or previous stored Future have been mechanically released.
Never block an Engine worker or reenter the same Core Watch from its callback.

The Future's cooperative cancel token is shared and is ignored by this producer.
Use `xrtFutureWaitForCancel` with a caller-owned token, or detach a caller-owned
Watch, to abandon one observer. This does not cancel subscriptions, a sibling
observer or a physical connection. Signal Futures retain no Client/Engine pointer
or payload; retained terminal hints outlive both physical owners.

`xredisSubscriberTakeBatch` transfers up to Capacity queue-owned reply references
in FIFO order with one Gate acquisition. It performs no allocation, retain,
payload copy or payload scan. Only the actual output prefix and Taken are written.
A valid empty queue returns false and preserves outputs and the ambient error.
Invalid/overlapping output spans fail without modifying the queue. It shares the
receive cursor with ClientTakePush; release every transferred reply exactly once.
The existing message/sideband count and actual reserved-byte bounds still apply;
overflow retires the connection explicitly. There is no automatic message loss,
reconnect or desired-subscription replay added by this leaf.

The standalone example uses a deterministic owned fixture. Focused tests cover
shared epochs, private wait cancellation, two reply envelopes, ring wrap, callback
reentry, cold/rearm allocation prefixes, concurrent close and retained owners.
Single-TU probes additionally isolate late parsed-frame delivery during logical
close and allocation-free signal/batch transfer. Real Redis transport matrices
exercise the same APIs; exact evidence and unresolved performance/production
gates are recorded in the progress log and stage report.

The message-only benchmark excludes the initial subscription ACK from timing,
includes server enqueue-to-owned-consumption latency and reports P50/P95/P99/P99.9.
It uses bounded application bursts; it is not a fixed-arrival or production-load
qualification. Epoch construction, Future synchronization and caller-held replies
have explicit costs. No zero-runtime-overhead or production-readiness claim is made.

A terminal hint is a logical wake, not a resolver-retirement barrier. Its worker
may still hold the detached Promise while returning from completion callbacks.
This bounded physical tail retires on the same IO worker; resource-accounting
tests observe a subsequent worker turn before requiring its allocations to be
zero. Physical Close still resolves pending hints before releasing activation.
