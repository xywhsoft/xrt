# Immutable method index

Select `XRPC_MODULE_XRPC_METHODS`. `xrpcMethodsCreate` copies decoded UTF-8 names,
callbacks and borrowed context pointers into one bounded immutable allocation.
Registration accepts unsorted input, empty names and embedded NUL, and rejects
duplicate names, invalid UTF-8, NULL callbacks and the reserved `rpc.` prefix.
No callback runs during construction/destruction; context/code lifetime remains
the application's responsibility. Copying a context pointer does not make its
contents safe for concurrent mutation.

Name equality is case sensitive, interprets equivalent JSON escapes and preserves
distinct normalization forms. This follows the name comparison and reserved-name
rules in [JSON-RPC 2.0](https://www.jsonrpc.org/specification) and the escape-aware
comparison guidance in [RFC 8259 section 8.3](https://www.rfc-editor.org/rfc/rfc8259.html#section-8.3).
Like the existing codec, lookup accepts only strict Unicode scalar strings;
unpaired surrogate escapes are rejected locally.

`xrpcMethodsFind` independently validates one complete raw JSON string before
looking it up, including any trailing whitespace/data. Even an empty index does
not turn malformed input into NOT_FOUND. Limits distinguish decoded bytes per
name, raw input bytes (including surrounding whitespace), method count, total
copied names and requested owned bytes. FOUND copies a binding whose Name borrows
the index; NOT_FOUND and ERROR leave the output unchanged. Local diagnostics may
come from the existing JSON/Unicode leaf or the xrt.rpc domain, and diagnostic
allocation failure remains local OOM. Ordinary hit/miss preserves ambient error.
Registration/query bounds do not override dispatch admission. Coordinate
`Decode.MaxMethodBytes` and the document limits with the intended raw spellings:
the decoder defaults to 4096 raw method-token bytes, while the index permits
4096 decoded name bytes and 24578 raw query bytes. Long or heavily escaped names
can therefore require an explicitly larger decoder limit.

The sorted key is a noncryptographic 32-bit FNV-1a fingerprint followed by the
complete decoded name. Hash equality is never sufficient to route a call.
Construction uses deterministic in-place heapsort, with no libc qsort scratch or
growth allocation. Lookup uses binary search, including complete-name ordering
inside collision groups; deliberate collisions cannot create a linear probe
chain. Plain queries hash a contiguous string view; escaped queries decode into
a four-byte iterator buffer while hashing and again for collision comparisons.
Worst-case work remains O(L log N), ordinary hash comparisons need not read names.
No decoded query buffer or successful hit/miss allocation is added. Validation,
hashing, comparison, Unicode encoding calls and error bookkeeping are real costs.

The single allocation contains the prefix, exact entry count and copied name
bytes without terminators. Info.OwnedBytes counts requested bytes, including
entry/prefix padding, while allocator headers/rounding and cold diagnostics are
additional. Names copy once, validation/hash passes scan them, and heapsort moves
entries. Config is copied; constructor inputs can be freed after success.

The object starts with one reference. Concurrent Find/Info need a live reference
covering the call and every borrowed name. Retain/Release add actual atomic work;
Find/Info add no index reference operation. Retain requires an existing reference
or protected handoff; atomically loading an unprotected pointer and then retaining
it does not solve a reclamation race. Last Release frees the one block without
calling user code. Callback code/context must outlive all uses.

Pass `xrpcMethodsCall` and the index as Call/CallUser to `xrpcDispatch`. Keep one
strong index reference for that entire dispatch and its callbacks. The adapter
does not retain per item. A normal miss returns false; local lookup errors call
`xrpcReplyAbort` and stop later handlers without a wire Method not found error.
Found handlers receive their copied context pointer and the existing synchronous
reply scope. Runtime scheduling, live publication/replacement, async completion,
authorization, cancellation and negotiated system extensions remain separate work.

The [example](../examples/methods/main.c) routes an escaped spelling of echo and
preserves a huge numeric ID. The workload profiles separately measure validated
plain/escaped/missing lookups and decode/index/handler/encode dispatch against
4096 registrations. They exclude construction, name lookup caching, queues and
transport. The throughput phase excludes per-operation clocks; a separate latency
phase includes two xrtTimer calls per measured operation. Neither phase establishes
end-to-end server performance or production acceptance.
