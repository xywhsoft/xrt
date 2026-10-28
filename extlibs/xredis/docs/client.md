# Single-connection TCP client

Select `XREDIS_MODULE_XREDIS_CLIENT`. This independently implemented vertical
slice uses public XRT TCP, engine, Future, cancellation, allocation and number
APIs plus the existing command/owned reply modules. It does not create another
shared event runtime. The [RESP specification](https://redis.io/docs/latest/develop/reference/protocol-spec/)
defines ordinary ordered responses and out-of-band RESP3 pushes. Commands such
as [CLIENT REPLY](https://redis.io/docs/latest/commands/client-reply/) change
response cardinality and require a separate connection-mode contract.

## Execution and ownership

Create a running XRT engine, initialize explicit client limits and connect to a
numeric address with fixed worker affinity. Connection start is asynchronous;
calls may queue before Open. One worker owns all protocol/connection mutation.
Cross-thread admission/cancel/push transfer uses short mutex-protected queues;
an embedded XRT post coalesces wakes without allocating or using public engine
command capacity. There is no user push callback on the network worker.

CallAsync validates and copies/encodes every argument before returning ACCEPTED.
The output Future changes only on ACCEPTED; overload and closing are normal
counted admission results. Inputs are borrowed only for the call. The request
wire is submitted by reference to TCP, without another extension payload copy.
The send reference and FIFO owner independently protect it until their actual
release paths finish. Actual buffer free precedes reservation release.

Every accepted Future resolves with immutable `xredisrequestresult`, including
non-success outcomes. **XFUTURE_RESOLVED does not mean the request succeeded.**
Inspect Status and Delivery. The result box is allocated before admission so
OOM during network/parse/completion cannot erase its typed status. Reply and
optional explicit Cause borrow the Future; retain a reply before dropping that
Future. Result retention does not retain a request, client, stream or engine.
No task result is inferred from another operation's ambient error. Native
failure causes are retained directly at their failure/callback boundary.

`xrtFutureCancel` requests cancellation and wakes the owning worker. Cancellation
does not execute request completion on the cancelling thread. The worker
unregisters its cancellation watch before publishing a result; retirement may
wait for its short enqueue-only callback, with no client gate held. It never
waits for user business work. Generic Future watch notifications can run inline
on the completing worker: application code must not block it. Call, the sync
facade, rejects **any** worker of the same engine, including another affinity.
Reentrant Info, asynchronous admission and Close from a Future notification
are supported. Caller-owned references and code residency are required.

## Deadline, FIFO and uncertain execution

Timeout is signed milliseconds, -1 forever and zero already due. Its monotonic
deadline is captured at call entry and includes validation/encoding, admission
queueing, connection setup and response processing. A delayed worker does not
restart that timeout when it finally dequeues a request. Equality expires.
Timers arm on the worker using the remaining original deadline, rounded outward
to the public millisecond timer API. Cancellation/deadline is rechecked before
send and before logical response completion, rather than trusting timer order.

NOT_SENT means TCP did not accept the command. MAY_HAVE_EXECUTED means TCP
accepted its bytes; it does not prove peer receipt or execution. REPLIED records
a completely validated correlated response. Delivery describes knowledge when
the result was published and never silently changes later. Server RESP errors
are distinct SERVER_ERROR results with their full owned response. If a deadline
or cancellation is first observed while finishing a response, that status is
published with REPLIED certainty and the late reply is discarded.

Local cancellation/timeout before send removes the pending request. After send,
logical completion leaves a counted FIFO tombstone. Subsequent replies drain
their original positions, with no cross-talk into later calls. A positive
DrainTimeout bounds this physical obligation. If the reply never drains, the
connection is retired; other requests receive explicit outcomes/uncertainty.
In particular, another request's drain timeout produces DISCONNECTED for
surviving calls, while the client's CloseStatus records TIMEOUT. It does not
falsely report that each surviving call's own deadline expired.
There is no automatic replay or hidden reconnect, including for apparently
read-only commands. A TCP write-queue rejection preserves order and ownership;
the head waits for write progress. In-flight count limits pipeline depth.

## Bounds, pushes and close

Defaults: 1024 outstanding requests, 1 MiB per encoded command (clamped to the
stream's write limit), 8 MiB total retained command bytes, 128 in flight, 128
queued pushes / 16 MiB owned push capacity, 128 ingestion/send/cancel/retire work items,
64 KiB / 128 frames per read slice, and a 5000 ms tombstone drain grace. Reader
and TCP configs have their own explicit bounds. Request count includes concurrent
construction and command-buffer/timer/cancel release tails. An abandoned request
still consumes capacity until physically retired. Resource counters are coherent
under the client gate; cumulative uint64 counters use ordinary unsigned wrapping.

Command-byte bounds exclude request/result/Future/watch/timer metadata, native
TCP buffers and allocator headers/caches. Their counts are bounded by outstanding
requests and engine limits. The one partial response has its own peak owned-byte
cap. Push bytes count owned frame capacity, not merely wire payload. Transferred
pushes and completed results retained by callers lie outside the client quota;
applications must bound external retention. XRT allocation budgets can enforce
additional per-allocation ownership quotas, including cross-thread frees.

RESP3 PUSH frames, including preceding attributes, enter a separate bounded
ownership ring. TakePush transfers one reference; empty leaves output/error
unchanged. Pushes never consume the ordinary FIFO. Overflow retires the connection
with LIMIT rather than silently dropping data. An ordinary response without a
sent FIFO slot is a protocol error. A valid push already queued remains available
after close, until transferred or the client is released.

Close is idempotent and requests asynchronous retirement. New admission stops,
accepted work receives typed outcomes, and retained wire duties survive until
TCP actually closes. Closed returns a separate retained observer; it does not
initiate close. The observer resolves only after transport closure and actual
request/cancel/control data ownership has retired. Stream and parser owners are
dropped and the engine pin released before this notification. Its mechanical
callback return tail can still be running; stop/join the engine before unloading
code. Release the caller's client reference after requesting close. Keep the
engine alive through all in-progress client APIs, and library code resident
through all returned Future/result callbacks and response ownership.

## Current scope and actual costs

The module currently handles one TCP connection and ordinary one-reply
commands, with bounded pipelining. Raw Connect preserves manual setup; managed
ConnectInit adds [HELLO/AUTH initialization and its readiness barrier](client-init.md).
DNS, TLS, reconnect policy, pools, dedicated transaction/blocking/PubSub modes, Sentinel,
Cluster and Tracking are subsequent work; the whole product remains developing.
Known subscription/unsubscription, MONITOR, SYNC/PSYNC and CLIENT REPLY switches
are rejected before admission. Unknown module commands must obey the caller's
one-reply contract. Managed connections also reserve their setup controls.

Admission allocates request+wire and a separate outcome box plus XRT Future/
cancel/watch state. Timed requests also consume bounded engine timers. Payloads
are encoded/copied once into request storage, and response bytes copied into an
owned frame, with possible vector-growth copies. TCP reference/block metadata,
OS/TLS-independent socket copying, validation/alias and command-mode walks,
decimal formatting, fixed result/view copies, queue locks, refcount atomics,
Future synchronization, timer operations and cancellation-watch retirement are
real costs. Cold diagnostics can allocate. This is not a zero-overhead claim.

## Reproduction and evidence boundary

Run `tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json
--suite xredis_tests --cflag=-O2`, the standalone client single test, root examples,
trim probes and independent static/shared consumers. The
[self-contained TCP example](../examples/client/main.c) needs no external Redis;
it asserts binary echo and result survival after engine teardown, then retires
all owners. Its local peer is test infrastructure, not interoperability evidence.

The runtime workload uses a closed-loop local peer, two engine workers and a
configurable pipeline (profile 16). It measures API-entry-to-observation latency,
including encoding and queueing, and reports throughput/P50/P95/P99/P99.9. It
also includes the test peer's command parsing and response generation costs.
Batch observation can add consumer wait. It is **not** fixed-arrival-rate or a
Redis/Hiredis comparison; smoke establishes execution only. Native Linux,
applicable TSan, pinned end-to-end baselines, large connection/worker matrices,
Windows resource accounting and 72-hour mixed-load/fault runs remain required
under the full four-product contract.
