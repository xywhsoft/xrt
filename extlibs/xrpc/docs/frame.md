# Bounded stream framing (developing)

JSON-RPC2.0 is [transport agnostic](https://www.jsonrpc.org/specification).
`xrpc_frame` provides two explicit framing choices for subsequent stream/stdio
bindings. It implements no connection, executor, pending-call correlation,
automatic error reply, timeout/retry or process ownership. Those runtime duties
remain required. The product and this leaf remain developing pending qualification.

The default Content-Length profile follows the
[LSP3.17 base header rules](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/specification.md#base-protocol):
ASCII fields, CRLF line endings, a mandatory byte count, optional Content-Type
and UTF-8/legacy utf8. This is framing compatibility, not LSP application or
lifecycle support. Application/json is also accepted. HTTP-style token names,
optional spaces/tabs, quoted media parameters and escaped quoted values are
validated. Duplicate lengths/types/charsets, folded fields, non-ASCII/control
bytes, conflicting content/transfer codings, invalid decimal lengths and
unsupported media/charset fail before body publication. Unknown syntactic fields
and media parameters carry no policy. This is not an HTTP message parser.

The alternate UINT32_BE profile is exactly four unsigned big-endian byte-count
bytes then that many body bytes. Both ends must select it explicitly; a malformed
header never triggers a format fallback. Neither profile inspects body JSON or
UTF-8. Empty and binary bodies are intentional; a JSON binding subsequently
classifies an empty/invalid JSON document using its existing rules.

## Ownership and progress

Create copies configuration and allocates one small reader. A serialized owner
calls Feed/Info/Reserve/End/Reset/Destroy; no internal locks or callbacks exist.
Feed consumes at most one frame and publishes Used on every valid API call,
including a consumed failing prefix. Only READY publishes Frame. Coalesced suffix
bytes remain with the caller. MORE retains no caller span and exposes no partial
body. Input and outputs are disjoint from each other and all reader allocations.
API ERROR changes neither state nor outputs.

A body wholly available in the current input is borrowed directly, even when
its header arrived earlier. Buffered=false means the caller must keep those
bytes alive/unchanged. Fragmented body bytes are copied into reusable reader
storage, with Buffered=true and lifetime through the next Feed, successful
Reserve, Reset or Destroy. Never retain those views asynchronously without an
owned snapshot. Next Feed after READY invalidates the old frame before starting
another. EOF is explicit: an empty Feed is ordinary MORE. End at a boundary is
END; incomplete head/body is TRUNCATED. Protocol, unsupported, quota, allocation
and truncation failures remain sticky. Reset discards the failed frame and
requires an independently trusted boundary; no resynchronization scan occurs.

## Bounds and costs

Default body1MiB, header8KiB, line4KiB including CRLF,64 fields and total requested
storage4MiB. Hard body1GiB, header/line1MiB, fields65536, storage4GiB subject to
size_t. MaxBodyBytes0 intentionally accepts only empty bodies. Storage includes
the state block, both retained buffer capacities and conservative old+new
replacement reservations. Caller/native buffers, allocator metadata/caches and subsequent JSON/result
storage are separate. Peek Info for base/capacity/current/peak accounting.

Whole headers are scanned then parsed directly in caller input; no header buffer
is needed. Fragmented headers scan each newly offered byte once, append once and
parse the complete header once. No partial-prefix reparsing loop is used. Body
chunks copy once into geometrically grown reusable storage; realloc may move
earlier capacity, so this is not a universal single-copy claim. Complete current
bodies incur no framing payload copy. Reserve supports predictable preallocation
and commits both candidate buffers atomically. Failure preserves protocol state,
old buffers and views; observed allocation peak may still increase. Small storage
caps can accept a complete borrowed frame but refuse its fragmented form.

HeaderWrite supports a separate prefix without copying body. Complete Write
preflights all spans/limits before any publication and copies opaque body once.
Both write canonical decimal Content-Length or a binary prefix, without a NUL.
Successful reader/writer paths preserve ambient error. Typed protocol/quota
decisions allocate no diagnostic; actual allocation failure may set XRT error.
No zero-copy transport, zero total cost or production performance is inferred.

## Verification and remaining work

Tests use independently authored bytes, every split point, poisoned copied input,
coalesced/empty/binary frames, valid/invalid media grammar, caps and old+new peaks,
reserve after borrowed and partial frames, full aliases/wrapped spans, constructor
and buffer failures, sticky EOF/failure,12000 mutations and10000 warmed no-heap
cycles per profile. The selected single proves no JSON, Future or Engine closure.
Real stdin/stdout tests independently encode/decode framing and JSON in Python.
The benchmark separates complete borrowed, fragmented buffered and writer paths;
its known-layout raw control omits safety checks and is only a cost reference.

Native Windows/Linux mixed-load performance, upstream comparators, P99 transport
latency,>=72h stability, bidirectional runtime, process/stdin shutdown/backpressure,
WebSocket and negotiated deadline/cancellation remain required product work.
