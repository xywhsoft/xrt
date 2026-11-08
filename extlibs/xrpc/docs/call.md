# Native asynchronous call lifetime

`xrpc_call` provides a unique serialized-owner producer and an XRT Future. This
is the operation-lifetime component for the pending async client/server runtime;
it does not send a request, create an engine/connection, arm a timer, enqueue
work or supply an executor. The owner integrates `Wake`, `Poll`, `MarkSent` and
`Finish` with its transport and scheduler.

Create preallocates the producer, separate immutable terminal-result storage,
XRT Future/cancellation source. Its native producer binding adds no allocation.
No extra shared runtime is added. The independent result allocation prevents retained outcomes
from keeping producer/context/engine references alive. This is not a zero-
allocation API: count both extension allocations and core allocations/locks.
Config is copied. Output pointers remain unchanged on failure. A context Drop
callback takes one context reference only on successful creation; otherwise the
context is borrowed until final producer-reference retirement. Destroy may return
before an already-captured cancellation callback releases that reference. Callback code must remain live.

All producer access is serialized. Observers use normal XRT Future APIs from
other threads. Future cancellation requests cooperation and synchronously invokes
the optional Wake on the requesting thread once; raw cancellation-token requests are observable by Poll but do not invoke Wake
(the XRT producer capability only routes explicit Future cancellation). Wake must
only signal/post owner work, never block,
wait for that owner, finish/destroy the producer or run business/language code.
The owner calls Poll at its scheduled deadline or wake. No background poller is
created. Timeout starts at Create entry and includes construction/queueing.
Zero is immediately due; FOREVER uses an infinite deadline in xrtTimer's monotonic
domain. An elapsed deadline wins when both deadline and cancellation are visible.

Finish checks response kind/index and all retained references before publication.
The owner must already have correlated that response to this operation. It samples stop conditions and
publishes the preallocated typed outcome. Completed results resolve the Future even for remote errors,
timeout/cancel/close, transport/protocol/resource failure; callers inspect Status,
not merely XFUTURE_RESOLVED. Cause is an explicit retained diagnostic or NULL;
ambient error is never borrowed as an operation's cause. Dropping an observer
does not cancel accepted work. Destroy closes unfinished work. A checked core
retirement/publication refusal returns failure and retains the producer for retry.

MarkSent records only acceptance of some request bytes, yielding MAY_HAVE_EXECUTED.
A validated response yields REPLIED knowledge, even when local timeout/cancel wins
and its payload is discarded. Other unsent outcomes remain NOT_SENT. No request
is replayed. These are knowledge states, not guarantees about business effects.
Successful response results own a snapshot reference/index and optional cause,
surviving producer/context destruction. Retaining the result Future owns those
values; retain the snapshot independently before dropping that Future.

The producer is pinned through synchronous completion-Watch notifications and
mechanical release tails. Such Watches run on the publishing owner and must not
block an I/O worker. Terminal reentry and Destroy from a completion Watch are
supported, including destruction already in progress. Successful completion and
destruction preserve the caller's ambient error despite notification/context-drop
diagnostics. The opaque native producer/payload is not collector admission.

The [example](../examples/call/main.c) runs without a network service, records
uncertain delivery, requests cancellation and reads the typed outcome after
producer destruction. Run README's build command with suite `xrpc_call`.
Separate tests cover statuses/knowledge, explicit causes, retained payloads,
observer drop, cancellation/deadline ordering, completion reentry, nonblocking cancellation-capture retirement and construction/OOM boundaries. The benchmark includes complete
operation/Future creation and destruction; encoding, queues, timers and transport
remain excluded. Complete async client/server and production acceptance remain
work under the original four-product contract.


Terminal publication does not wait for an executing cancellation Wake. Core Future
cancellation acquires an actual producer reference before releasing its lock;
that reference protects the context until Request and its release tail return.
The Future detaches its producer reference on completion. Producer destruction
can therefore precede the cancellation callback's return without freeing its
context. Wake may arrive after logical completion; any queued post must retain
its own captured references beyond Wake return. The runtime must keep actual
transport/post duties pinned independently. The native node reports its real
reference count, but deliberately refuses ownership traversal of its opaque
activation/context rather than inventing a collector certificate.

BatchCallCreate preallocates xrpcbatchcallresult plus Count indices. It shares the
ordinary producer lifetime and cooperative stop rules. BatchCallFinish accepts
an already-correlated complete array snapshot and a request-order permutation
with SIZE_MAX entries for notifications. It checks full coverage and uniqueness,
then publishes one owned snapshot/map. A sampled stop discards both payload and
map while retaining REPLIED knowledge. NOTIFY_DONE requires MarkSent and means
transport completion with MAY_HAVE_EXECUTED. Ordinary call result size is unchanged.
