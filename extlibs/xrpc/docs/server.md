# Bounded server execution

`xrpc_server` owns a bounded Core executor and accepts complete JSON-RPC request
documents. It is the execution path for upcoming HTTP/WebSocket/stdio adapters;
it does not open a listener or implement those server transports yet. It uses
public XRT APIs, with no new Core API or shared service runtime.

Submit copies input before returning, then a worker performs complete validation
through xrpcDispatch before invoking business code. One document, including a
mixed batch, is one admission, executor job and deadline/cancellation unit.
Handlers run on executor workers and can execute concurrently across documents.
Within a document they execute in request order. Reply is synchronous and borrowed
only through the handler; deferred handlers are not provided by this API. Keep
callback code/User alive through successful Destroy.

Normal terminal outcomes resolve a Future with an immutable xrpcserverresult.
DONE means complete local dispatch, including valid RPC errors, malformed-input
Parse error and Invalid Request responses. Inspect Wire/Dispatch for business
outcomes. Notification-only documents produce empty Wire. Replies preserve raw
ids and mixed batches omit notification replies. Failure or sampled stop discards
all partial output. MayHaveExecuted records actual server callback entry; it is
neither success nor rollback. There is no automatic retry/replay. A checked Core
publication refusal increments PublicationFailures and closes the Future without
a typed result, so consumers must inspect Future state as well as Value.Status.

The monotonic deadline starts at Submit entry, including input copy/construction
and queueing. Cancellation and deadlines are cooperative: sampled before/after
handlers and at response emission. Queued stopped work resolves when a worker
starts it, or when server cancellation drops the queued job. No timer or forced
interruption runs behind the API. Long handlers must poll the borrowed token and
deadline, or they may delay drain. Server cancellation requests all job tokens and
drops queued work; ordinary Close drains accepted jobs. User cancel watches and
completion watches may run synchronously on cancellation/drop threads, outside
native locks. They must not block those threads.

Limits cover live operations/construction, owned input/job bytes, pending AND
caller-retained result count/header/capacity bytes, whole/per-object output and
preallocated per-worker decode scratch. Result capacity starts at 256 bytes,
grows geometrically within per-document and aggregate caps. Reallocation reserves
the full new block/header alongside the old block, covering allocate/copy/free
peaks; after success old credit is returned, and failure returns the new credit. Result growth can fail after
business effects; the outcome is typed LIMIT/MEMORY and does not undo those
effects. The accounting boundary is requested bytes, excluding allocator metadata,
rounding and Core Future/executor/thread internal allocations. Separate finite
operation/result counts bound their number; executor configuration bounds its
preallocated queue/thread storage.

Each worker borrows an exclusively claimed preallocated scratch slot for a whole
dispatch. A protected free list makes slot acquisition/release constant-time;
there is no per-request descriptor allocation or thread-local scratch leak.
Input uses one job allocation/copy. A preallocated result header and initial
output share one block, which can move before publication. Publication transfers
it into the Future. Retained Futures own only this response/cause and a separate
quota ledger, so they neither pin the server nor delay worker drain/join.

WaitFor closes admission and uses one remaining timeout across executor drain and
constructor/drop-tail retirement. It rejects own-worker and cancel-cleanup waits.
Destroy additionally joins workers, requires the caller to stop concurrent API
entries, and consumes the unique owner only on success. Callback self-destruction
fails and leaves the owner valid for later retry. Retained result Futures remain
readable afterward and release quota bookkeeping on final payload destruction.
The opaque callback/job/result graph is not collector admission.

This stage does not establish independent transport interoperability, final
aggregate transport/connection limits, deferred business-handler lifetime,
negotiated deadline/cancel extensions, native Linux qualification or production
performance. The full product remains developing.


The [offline example](../examples/server/main.c) executes a mixed call/notification
batch, joins the server and prints the retained response. It requires no service.
Run with the README build command and suite `xrpc_server`. Tests use explicit
blocking/cooperative handlers only as controlled lifetime fixtures. The separate
benchmark uses windowed document throughput and a distinct one-in-flight latency
phase; it includes input copy, queueing, validation/dispatch/encode, Future reads
and destruction. Throughput and latency have different concurrency, and neither
is a network-server/production-performance result.


Observed costs include startup scratch zeroing, one copied-input job block, a
zero-initialized initial response block, Core Future/cancel/queue state, quota
locks/refcounts, repeated dispatch/encoder validation and potential response
reallocation copies. InitialOutputBytes can be raised to the complete output
limit to reserve/allocate capacity before handlers, trading admission footprint
for fewer later growth failures; allocator/user/diagnostic failures and remote
business uncertainty still cannot be eliminated by that setting.


Explicit per-document [copied context](server_context.md) is available through
`xrpcServerSubmitWithContext`. Its bytes and alignment padding share the request
budget; handlers borrow an aligned immutable snapshot, independent of results.
