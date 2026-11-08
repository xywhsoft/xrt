# Complete response admission and correlation

Select `XRPC_MODULE_XRPC_RECEIVE`. One serialized connection owner calls
`xrpcReceive` on a complete document. The function validates every envelope,
builds one [owned snapshot](response.md), then calls `xrpcPendingTakeBatch` with
the snapshot's contiguous raw-ID views. Every ID is independently canonicalized
under the table's limits and looked up before any removal. Local error, malformed
JSON, invalid envelopes, a late key-limit failure, insufficient resolution storage
or allocation/diagnostic OOM leave all pending calls and final output metadata
unchanged. Decoder and resolution scratch are unpublished on failure.

After preflight, the table commits without another parse, allocation, diagnostic
or callback. The first response for a live token is MATCHED, subsequent equivalent
IDs in the same batch are REPEATED, and IDs absent at preflight are UNKNOWN. Only
MATCHED records carry a Call/User. Unknown includes replies to calls completed
before this batch; the table does not keep an unbounded history. Results remain
in wire order. All matched calls leave the table before the function returns.

The owner completes futures or invokes continuations afterward. The immutable
snapshot owns JSON bytes, not the User objects in resolution scratch. Extract
the matched application state and hold a snapshot reference/index for each
asynchronous result. Completing a call cannot reenter midway through admission,
because this function invokes no application callback. It does not supply an
executor, perform blocking user work, read a socket, enqueue callbacks, read a
clock or decide expiry-event order.

The owner must serialize the entire call with all table accesses. Fresh wire IDs,
connection epochs and response eligibility remain runtime policy. Reusing a wire
ID can still accept an old response as a new call. Explicitly pending null IDs
match null here; the runtime must decide whether null-ID errors are eligible.
Response-mode decoding permits ignored opposite-role extra fields according to
the decoder contract; a bidirectional runtime must classify its document role
before selecting its request/response path.

Scratch and resolution arrays have independent positive capacities. Their full
spans, input, config, final output pointers and table storage cannot overlap.
Info.Size must match. The complete snapshot allocation occurs before resolution
capacity and table-key checks, so those failures can allocate and release a
temporary snapshot. A success makes one snapshot allocation and one full-wire
copy. The decoder and per-ID canonicalization are separate scans. Each ID is
looked up during preflight; matched nodes make a later structural removal
traversal. Cold diagnostics and XRT reference bookkeeping remain counted.

The [example](../examples/receive/main.c) admits a two-response batch with
equivalent IDs, verifies a single match and observes stale cancellation missing.
Run the README build command with suite `xrpc_receive`. Separate public tests use
an independent batch outcome oracle, malformed/invalid late items, late table-key
limits, storage/alias errors, OOM and source/scratch overwrite. The benchmark
includes two admissions, a four-response snapshot/correlation and release. Its
locally reused IDs are a primitive workload, not a safe runtime ID policy;
queueing, Future completion and transport remain excluded and unaccepted.
