# Immutable response snapshots

Select `XRPC_MODULE_XRPC_RESPONSE`. `xrpcResponsesRead` first runs the complete
response-mode decoder with caller scratch. Every item must be a result/error.
A malformed tail, invalid envelope/item, request, notification, scalar or empty
batch publishes no snapshot. Parse errors, valid-JSON invalid envelopes and local
resource/API/OOM failures have distinct status values. Diagnostic OOM is local.
The caller must compare explicitly with `XRPC_RESPONSE_DONE`.

After validation, one checked allocation holds the header, actual-count message
descriptors, an actual-count contiguous raw-ID view array, and a complete wire
copy. Every descriptor view is relocated into that copy; no field is reparsed,
unescaped or numerically narrowed. Original whitespace, unknown fields, raw IDs,
result/error data and escaped NUL strings remain available. Scratch and source
buffers can be reused as soon as Read returns DONE.

The extra ID vector copies one Data/Size view per item, allowing bounded batch
correlation without another traversal of the descriptors or unsafe strided casts.
It does not copy each ID string again. Owned bytes include header, descriptors,
ID views and wire bytes; the default ceiling is 2 MiB, separate from decoder input,
message and nesting limits. Allocator headers/caches are excluded. This is a
per-document bound; a runtime still needs an aggregate retained-result budget.

`xrpcResponsesInfo` exposes immutable borrowed metadata, messages and IDs. Hold a
strong reference for every concurrent reader or asynchronous result. Safely publish
the initial reference, retain before sharing, and release after physical reader
completion. An index plus a retained snapshot can represent one future's result
without another payload allocation. Keeping one item keeps the whole batch wire
alive. Reference increments are checked for overflow. Never mutate a snapshot's
views or descriptors by casting away const.

The reader does not correlate IDs, own application call state, complete futures,
read sockets or select an executor. Valid input incurs full envelope validation,
one allocation, one full-wire copy, one descriptor copy/rebase pass and ID-view
publication; reference/JSON bookkeeping can synchronize. No zero-copy or zero
allocation claim is made. Cold diagnostics can allocate. Success preserves the
ambient diagnostic. All failure paths leave the output object pointer unchanged;
scratch remains unpublished and unspecified.

The [example](../examples/response/main.c) overwrites its input immediately and
checks retained result data before releasing the snapshot. Run the README build
command with suite `xrpc_response`. Separate tests cover complete publication,
all view rebasing, owned-input destruction, exact limits/aliases/OOM, generated
wire fixtures, allocation counts and retained concurrent readers. The dedicated
benchmark includes validation/allocation/copy/release and excludes correlation,
queues, futures and transport. Source and platform evidence are in the progress
ledger; this leaf does not establish async runtime or production acceptance.
