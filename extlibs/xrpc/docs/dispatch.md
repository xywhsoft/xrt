# Synchronous request dispatch

Select `XRPC_MODULE_XRPC_DISPATCH`. This caller-thread leaf connects the existing
bounded decoder and encoder to application resolution and a synchronous reply
sink. It implements the request, notification, error and batch processing rules
from the [JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification).
It does not start a server, own an executor or add transport/async state.

`xrpcDispatch` validates the complete input before invoking any application
callback. A malformed tail cannot execute an earlier valid item. Local input,
scratch, resource and diagnostic-allocation failures produce no wire error and
invoke no callbacks. Malformed JSON emits one Parse error with null id; an
invalid envelope/item emits Invalid Request with null id. The actual response
count is checked before any callback. Empty input batches get a single error
object. Nonempty batches retain array shape even for one surviving reply.

The application `Call` callback receives the decoded request and one temporary
reply handle. It resolves the method and returns false when unknown; the leaf
supplies Method not found. It passes raw, quoted method tokens to the resolver;
the optional [method index](methods.md) compares decoded UTF-8, including escapes. Names
beginning with semantic `rpc.` are reserved and do not reach business code.
Extension negotiation remains pending. A handled call without a reply supplies
Internal error; a notification can finish without calling a reply function.
Parameter semantics and authorization belong to the application. Use
`xrpcReplyError` for Invalid params or an application error, rather than an
ambient XRT diagnostic. IDs are forwarded as exact original tokens.

`xrpcReplyResult` and `xrpcReplyError` preflight a non-notification response with
the public encoder, then invoke `Emit` before returning. The application can
therefore supply local stack payloads. The sink must encode/copy every borrowed
view before returning; it must not queue the descriptor or its pointers.
Notification reply attempts are suppressed without inspecting result payloads
and never invoke Emit. Duplicate/reentrant attempts, a false Call return after
attempting a reply, invalid output and sink rejection are sticky local failures.
The first diagnostic survives callbacks clearing/replacing ambient error.

An application or resolver can call `xrpcReplyAbort` to stop locally without a
wire error. Pass the explicit borrowed XRT diagnostic, or NULL for a fresh
`XRPC_DISPATCH_ABORTED` error. NULL never adopts stale ambient error. The current
diagnostic transfers into the dispatch context; another borrowed error needs a
checked reference. An already sticky failure wins. Aborting a notification or
aborting after an accepted response also stops later handlers; prior effects and
accepted replies remain in Info. This provides the local-failure route for
resolvers and handlers instead of returning false (unknown method).

The sink receives single-object encoded size and the final batch flag. It owns
bounded storage and backpressure. For a batch it adds an opening bracket and
commas as objects arrive, then a closing bracket only after successful dispatch.
Zero replies means no output. `Info.OutputBytes` includes final framing and only
accepted Emit results. Per-object encoder limits and complete-document reply/
byte limits are distinct. Whole input limits bound all work before callbacks;
no unbounded queue is added. Config is copied, and scratch/input must remain
alive and unchanged throughout all callbacks.

Handler outputs and sink capacity cannot be known before execution. A local
failure can follow already executed business effects or accepted replies; Info
records that partial progress. Discard partial response bytes and do not blindly
replay the request. A failed Emit may itself have external side effects. This
leaf does not supply transactionality, rollback, asynchronous completion,
cancellation or an unknown-outcome retry policy. Do not invoke blocking business
code on a network worker; a later runtime must select an executor explicitly.
`Messages` counts decoded descriptors, `Visited` counts completed items and
`Invoked` includes the current failed Call. `Notifications` counts encountered
notifications, including a current failed one. `Replies` counts accepted Emit
results, including a successful outer sink that triggered a reentrant failure.

Independent dispatches can reenter or execute concurrently with separate input,
scratch and contexts. Nothing is retained after return. Reply handles are valid
only during their Call callback; using them later or on another thread violates
the contract. No locks, threads, connection objects or successful-path heap
allocations are added. Cold diagnostics can allocate; parse diagnostic OOM is a
local failure rather than a fabricated Parse error response. Ordinary success
restores ambient error identity even if the business callback changed it.

Dispatch uses the existing checked, quiet owning error exchange at entry,
first-failure capture, failure re-exposure and final restoration. Restoring an
existing diagnostic does not notify the error handler again; creating a fresh
diagnostic still uses Core's normal notification contract. An explicit borrowed
abort error is retained directly by the stack context, avoiding an install/take
round trip. Final transfer of the context's owning edge needs no further retain.

If entry cannot write the error slot, dispatch returns false before decoding or
calling business/sink callbacks and leaves Info unchanged. If capture,
re-exposure or final restoration fails after admission, dispatch returns false
with the actual partial effects in Info. Even a failure after all replies were
accepted cannot undo those effects. Do not replay the request based only on the
boolean result. The saved error edge is released exactly once; an existing
published error remains owned by the slot, which may also be empty. Dispatch
does not retry a known failed slot or create a synthetic diagnostic through it.
These checks cover dispatch's owning error exchanges. User callbacks and the
creation/publication of fresh diagnostics retain their respective Core contracts.

Regression coverage includes exact ambient identity and notification counts for
success/batches/notifications/nesting, real Windows/POSIX error-key setter faults
at entry/capture/re-exposure/restoration, accepted reentrant sink effects,
borrowed abort ownership, caller allocation-ledger refunds, existing bound
execution contexts and an artificial retain-saturation boundary. The setter
fault shim is test-only and does not replace the rest of the native TLS keys.

Costs are explicit: complete input JSON is scanned once; each emitted response
is validated by `xrpcEncodeSize`; an encoding sink independently validates it
again during Write before copying raw values. String quoting has the encoder's
existing multiple traversals. There are decoder descriptors in caller scratch,
stack control/response objects, callback calls, checked counters and error
take/ref/restore bookkeeping. Handlers and sinks can add their own allocation,
lookup and copy costs. No zero-overhead or transport-throughput claim is made.

The [example](../examples/dispatch/main.c) demonstrates bounded batch assembly.
The performance workload measures a three-item mixed batch and synchronous
single-object encoding. It deliberately has no method lookup, final batch
buffer, queue or network I/O; it is not an end-to-end server benchmark.
