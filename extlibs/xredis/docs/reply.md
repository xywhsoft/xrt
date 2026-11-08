# Owned incremental replies

`XREDIS_MODULE_XREDIS_REPLY` builds immutable owned RESP2/3 replies directly from
the [event reader](reader.md), following the public
[Redis wire specification](https://redis.io/docs/latest/develop/reference/protocol-spec/).
Its closure is reader/core, public XRT atomics and public XRT number_float.
No network client, new shared runtime or reference-client code is embedded.

## Ownership and traversal

Initialize a config, create the reader, feed arbitrary fragments, and observe
Consumed on every valid call. READY transfers one strong reference to exactly
one fully validated semantic frame. MORE/YIELD/error never overwrite Result.
Resume with the unconsumed suffix; the configured byte budget is inherited.
All payload bytes are owned before Feed returns. Source buffers may immediately
be recycled, and completed replies survive reader Reset/Destroy. Finish checks
for partial frames and orphan attributes at EOF.

ReplyInfo returns the root index and total node/payload counts. ReplyGet returns
an immutable view whose Data belongs to the frame until the last Release. Empty
Data has NULL pointer/zero size. FirstChild/Next enumerate ordered children;
NONE ends a chain. MAP/ATTRIBUTE Length counts pairs, while ChildCount counts
twice that many children. Duplicate keys and all wire order are preserved.
Attributes points to an ordered chain of metadata maps, also linked by Next;
they do not consume ordinary child slots. Root attributes can precede a PUSH.
Metadata may itself contain annotated keys/values. No map normalization occurs.

Null bulk/array keep their original types and NULL_VALUE flag; RESP3 null keeps
its own type. Streamed forms retain STREAMED and their actual final length.
All scalar text, including numeric signs/lexemes, big numbers and verbatim fmt:,
is retained without a trailing NUL. Integer/Boolean also have checked native
values. ReplyDouble lazily converts only DOUBLE nodes with locale-independent,
correctly rounded public XRT parsing. Legacy NaN payloads normalize to signed
NaN for conversion while the raw spelling stays intact. A syntactically valid
finite overflow is retained; conversion returns the XRT numeric range error
without modifying the output. Underflow follows IEEE binary64 rounding.

The builder is thread confined. Completed immutable frames may cross a properly
synchronized handoff. Retain is a relaxed atomic CAS with overflow protection;
Release is one acquire/release atomic decrement, freeing on the last owner's
thread. Each access/Retain needs an already live reference. Reference counting
does not publish pointers or permit racing final Release with unowned access.

## Bounded memory and failures

There are two control allocations at creation, bounded by MaxDepth, with no
payload buffers allocated until the first value begins. A frame header embeds
one node and 64 payload bytes, so a small scalar uses exactly one allocation.
Larger replies use geometrically grown node and byte vectors, never individual
node allocations. Indices and byte offsets survive relocation. Known bulk sizes
reserve their destination at BEGIN, avoiding fragmented reallocation for that
bulk. Unknown scalar/stream sizes grow as their bytes arrive. No final flatten,
second wire parse or publication copy is required.

MaxOwnedBytes defaults to 64 MiB. It limits **peak requested frame bytes**, including
the header/inline storage, unused capacities, and the entire destination while
the previous buffer remains alive during growth. Growth is clamped to remaining
peak capacity and uses explicit malloc/copy/free. A limit can therefore reject
a growth whose final live size would fit. Reader protocol/depth/frame/bulk/line/
container/value limits remain independent. An oversized bulk declaration can
fail before payload arrives. No per-frame successful allocation exceeds the
peak cap. Info reports current live capacity and current/last frame peak.

This byte limit excludes control stacks, transferred replies held by callers,
allocator headers/rounding and cold diagnostic allocations. Callers/runtime
must separately bound concurrent replies and external retention. An XRT
allocation budget can impose a whole-lifecycle allocator quota. Exhausting an
allocation does not manufacture a successful or partial reply.

Protocol, limit, memory and truncated-EOF failures are terminal until Reset.
All partial allocations are released immediately, including when a lower-level
callback failure is caused by local OOM. ReplyReaderInfo.Error retains the
builder's typed cause separately from the nested reader error and ambient XRT
diagnostics, even under diagnostic OOM. Reset discards pending ownership and
all counters; transferred replies remain valid. Invalid API arguments/aliases
preserve parser state, Consumed and Result. Objects must actually be readable;
address-span checks cannot validate arbitrary pointers.

## Performance and verification scope

Normal successful access/conversion/refcount/Finish/Reset calls allocate nothing;
Feed allocates owned frame storage. Each DATA byte is copied once on append;
growth can copy existing used bytes again. Node initialization, vector copies,
fixed metadata/view copies, span/alias checks, links, native scalar writes,
numeric conversion scans, refcount atomics and final frees are real costs.
Depth slots initialize lazily per encountered value, so scalar completion does
not clear the entire configured depth stack. Error diagnostics can allocate.
No zero-overhead, network latency, pinned baseline or production parity claim
follows from an owned codec benchmark.

Run suite `xredis_tests`, its standalone `test_single_xredis_reply.c`, the actual
[fragment/recycle example](../examples/reply/main.c), trim probes and independent
static/shared package consumers with `extlibs/xredis/config/modules.json`.
The owned benchmark mixes inline scalar, metadata/array and streamed/binary
replies, traverses nodes, converts doubles and releases every frame. It includes
allocation and release costs; performance smoke only establishes execution.
The [request runtime](client.md) and [managed setup](client-init.md) now provide
separate network/correlation/deadline/cancellation/pipeline evidence, including
initialization interoperability with Redis. Pools, TLS, dedicated/HA modes and
final long-run/performance gates remain pending under the full four-product
contract.
