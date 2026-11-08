# Incremental RESP response reader

Independent implementation of the [Redis wire specification](https://redis.io/docs/latest/develop/reference/protocol-spec/)
and [RESP3 streaming and attribute semantics](https://github.com/redis/redis-specifications/blob/master/protocol/RESP3.md).
No reference-client implementation is embedded. This module is a bounded wire
reader, not a network client or an owned reply tree.

## Event and lifetime contract

- Select `XREDIS_MODULE_XREDIS_READER`; its dependency closure is core only.
- `ConfigInit` supplies finite defaults. Zero limits are real limits; explicit
  frame/value/feed budgets must be positive. Creation copies the config and
  allocates the reader plus the entire depth stack once with XRT.
- BEGIN and END pair at the same depth/type/flags. DATA is a nonempty view into
  the current Feed input, borrowed only during the callback. The reader never
  retains an input pointer. A retaining consumer must copy data or arrange its
  own buffer ownership. Event boundaries can vary with fragmentation.
- Length counts bytes for scalar values, direct items for arrays/sets/pushes,
  and pairs for maps/attributes. NULL flags distinguish RESP2 null bulk/array
  from empty values; RESP3 null has its own type. BEGIN may have unknown length;
  END has the actual count. Numeric DATA is exact raw text; integer/boolean END
  also supplies the checked native value. Doubles/bignums are not converted.
- Verbatim DATA includes all three format bytes and the following colon. Unknown
  three-byte format tags are accepted for extensibility. Binary payload is not
  inspected, except the required verbatim colon.
- Attributes precede and describe the next value at their depth, including root
  and nested values. They do not consume parent item/pair slots. Multiple blocks
  can be exposed separately. Orphan attributes at EOF or a streamed terminator
  fail. A semantic frame includes root attributes and one ordinary root/push.
- Pushes are top-level, nonempty and begin with a non-null simple/bulk string.
  Streamed bulk strings and array/map/set aggregates follow the RESP3 terminators;
  streamed maps must contain an even number of ordinary child values.
- RESP2 mode rejects RESP3 types/streaming. RESP3 accepts legacy RESP2 null forms.
  Normalized NaN is the default. The explicit LEGACY_NAN flag accepts signed,
  case-insensitive and bounded libc `nan(payload)` forms from older peers.

## Progress, errors and bounds

Feed stops after one frame (READY), input exhaustion (MORE), its byte budget
(YIELD), callback pause (PAUSED), or terminal failure (ERROR). All consumed bytes
include framing. A pause commits that event exactly once. Resume with the
unconsumed suffix; pending closing events and READY can need no additional bytes.
An empty Feed also drains these transitions. Pending event draining is bounded
by the configured aggregate depth. Parsing uses an explicit stack, no recursion,
and linear work in consumed bytes plus depth; fragments are never rescanned.

On wire/limit/callback/EOF error, discard any partial result and Reset or destroy.
DATA can precede a bad terminator. READY is the completion boundary. Finish
accepts only an idle frame boundary, and requires a paused parser to resume.
API misuse preserves state and output parameters, including callback reentry.
Errors use domain `xrt.redis`, codes starting at 1001. Diagnostic allocation can
fall back to XERR_MEMORY; Info preserves the parser's independent terminal code
and detection offset. Info counters reset only on Reset; LastFrame describes
the last delivered READY. Completed frames leave current frame counters zero.

The reader is single-owner/thread-confined. Callers serialize all access.
Callback and User are borrowed for its lifetime; no callback runs from Destroy.
Mutating callback reentry is rejected, while Info is allowed. Success does not
clear an ambient error; callbacks may change it. Span checks detect wrapping
and aliasing, and require actual readable/writable objects supplied by callers.

## Verification matrix

The tests in `tests/test_reader.c` cover these contracts:

| Contract | Fixtures |
| --- | --- |
| RESP2/3 types, binary payload and numeric boundaries | Independently specified event trees, INT64_MIN/MAX, NUL/CRLF, doubles/bignums, booleans, nulls, verbatim/error/map/set/push |
| Fragmentation and event lifetime | Every two-part split of canonical frames, bytewise feeds, DATA addresses inside the current input |
| Work budget and pause | One/two/three-byte budgets, coalesced input, pause after every event, empty root-END resume, no event repetition |
| Attributes and pipeline alignment | Consecutive/nested attributes, known and streamed aggregates, pushes between ordinary replies |
| Bounded resources | Depth, frame/bulk/line bytes, declared/streamed items/pairs, total values, cumulative chunk length |
| Invalid wire and EOF | Malformed scalar/length/CRLF/stream/push/verbatim cases, every prefix of an attributed nested reply, orphan attributes |
| Recovery and ownership | Reset after failure, callback rejection/invalid action, reentry, bad API spans/aliasing, create/destroy quota returns to zero live bytes |
| Allocation/error contracts | 10,000 warm parse/Finish/Info/Reset operations with unchanged allocation counters, successful parse under one-byte quota, create and diagnostic OOM |
| Deterministic properties | 10,000 independently encoded binary arrays with randomized fragments, pauses and budgets |
| Untrusted mutations | 20,000 deterministic raw/mutated inputs; coalesced vs bytewise/paused results, offsets, budgets and bounds must agree |

Build evidence is recorded in the [progress ledger](../../../docs/development/service-extensions-progress.md).
The modular/single entry points, offline example, static/shared consumer and
performance profile are registered in the manifest. The reader benchmark checks
frame completion and a data-dependent checksum. An unpinned codec smoke is
neither an external-client comparison nor end-to-end throughput/tail-latency
acceptance; those remain required with the runtime.
