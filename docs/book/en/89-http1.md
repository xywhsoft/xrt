---
num: 89
slug: http1
title: HTTP/1 Messages: Parsing and Packing
volume: 卷九 Web 协议核心
type: practice
lead: One-shot parsing of request/response heads, status-line packing with lookup and traversal — the zero-per-request-allocation core path built on caller-bound arrays.
api: http1, http
---

## Orientation

On Chapter 88's foundation, this chapter assembles the complete **HTTP/1 message layer**: `xrtHttp1RequestParse`/`xrtHttp1ResponseParse` parse on-the-wire request/response heads into a "start line + field array" structure (three-state return, borrowed views), `xrtHttp1ResponseWrite` packs the status line and fields in one shot, and field lookup uses the foundation's `xrtHttpFieldFind`/`FieldNext` directly (case-insensitive search over the parsed field array). The design's keyword is the **bound array**: `xhttp1head` binds a caller-provided field array — one Head structure per thread, reused forever, zero per-request allocation; this is the foundation path of the xhttp runtime and homegrown gateways. Bodies (chunked and friends) are Chapter 90's framing topic; this chapter stops at "the empty line ending the head block".

## Introduction

An HTTP/1 server handles tens of thousands of requests per second, each head about 1 KB with a dozen-plus fields. If every parse allocates a field array and frees it afterward — the allocator's locks and fragmentation become the bottleneck first, and the post-OOM recovery path twists the knife. XRT's answer is visible in the API shape: `xrtHttp1HeadInit(&Head, Fields, 8)` binds **the caller's stack array** to the Head — parsing merely fills the array with views; the next request reuses the same Head and array, allocation count zero.

The second design decision is the **three-state return**: `XHTTP1_READY` (a complete head has arrived), `XHTTP1_MORE` (insufficient input, wait for more), `XHTTP1_ERROR` (illegal structure). With the "failure does not advance" convention, a streaming server can mindlessly "receive a stretch, try parsing, on MORE receive again" — Chapter 71's framing loop, HTTP edition.

## Concepts

### Parsing: from bytes to structure

`xhttp1head` after parsing holds: `Method`/`Target` (requests) or `Status`/`Reason` (responses), `Version`, the field array (`Fields`/`FieldCount` — the bound array's fill area), and `Bytes` (total head-block bytes including the terminating empty line — the body starts there). The three-state return model:

```diagram state
输入不足 -> XHTTP1_MORE: 等待更多数据（不消费、不报错）
结构完整 -> XHTTP1_READY: 视图发布，Bytes 给出正文偏移
结构非法 -> XHTTP1_ERROR: 字段数超限/起始行非法/头不完整语义
```

Requests and responses share the Head structure (the role is decided by the call entrance: `RequestParse`/`ResponseParse`). `ResponseParse` takes a method parameter — because response semantics depend on the request method (a HEAD response has no body; this context becomes a hard rule in Chapter 90's Plan).

### Lookup and traversal

The common operations after parsing: on the `Head.Fields` array, use Chapter 88's `xrtHttpFieldFind`/`FieldNext` directly (case-insensitive — HTTP field names are semantically case-insensitive; same-named multi-value fields traverse in order, Set-Cookie style), `xrtHttpFieldGet`/`FieldGetUnique` (value views and uniqueness semantics). Combined with Chapter 88's token iterator, these cover composite judgments like "does Connection contain upgrade" — Chapter 93's HTTP upgrade (the WebSocket handshake) uses exactly this combination.

### Packing: status line + fields in one write

`xrtHttp1ResponseWrite(版本, 状态码, 短语, 字段数组, 数量, 输出, 容量, &长度)` (version, status code, phrase, field array, count, output, capacity, &length) — the status line and field block are written **in one shot**: "HTTP/1.1 200 OK\r\nContent-Type: ...\r\n\r\n". Capacity atomicity as in Chapter 88: insufficient means zero writes. The request counterpart is `xrtHttp1RequestWrite`.**Why not assemble with printf**: every step of an assembly path can fail midway (half a message), and format details (status-line spaces, field terminators) scatter everywhere — one-shot packing collects them into a single fuzz-tested entrance.

### Limits and defense

`xhttp1limits` (optionally passed to Parse) controls: the field-count cap (the bound array's capacity is natively one), the head-block total-length cap, and method/target length caps.**Why they are mandatory**: a malicious client can send a 2 GB head block — a parser without limits is a memory amplifier. Defaults are conservative; proxy scenarios relax as needed — limits are a deployment decision, not a global constant.

### A preview of the Message and Body layering

This chapter's APIs stop at "the head block ends". Chapter 90's `MessageParse` combines "head + body" in one pass (fits small messages), and the `Body` family streams the body (fits a proxy forwarding as it receives) — both layers share this chapter's head parsing.

## Examples

### First complete program: request parsing and response packing

The program below is from `examples/http/http1/main.c` — the shortest path of "receive a request, send a response":

```embed path="examples/http/http1/main.c" title="examples/http/http1/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/http1/main.c -lws2_32 -liphlpapi
GET /health
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 11
```

**What just happened.** (1) The input is complete request bytes (a static array mocking one arrival); `HeadInit` binds an 8-element stack array — **parsing allocates nothing**. (2) `RequestParse` returns `READY`: `Head.Method`/`Target` are borrowed views (pointing inside the input), printing `GET /health`. (3) `ResponseWrite` packs in one shot: status line (version/status/phrase) + two fields + the empty line; `iSize` is the full byte count — the body `{"ok":true}` follows directly.**This is the entirety of a server reply**: no per-line printf, no hand-assembled CRLF. In a real service the input comes from Chapter 66's receive buffer (NEXT → keep receiving), and the output goes to the send path (Chapter 67's write budget).

### Second complete program: the Head full-feature tour

The second program is from `examples/http1/head_tour/main.c` — five lines covering limits, lookup, iteration, chunk lines, and Message for you:

```embed path="examples/http1/head_tour/main.c" title="examples/http1/head_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http1/head_tour/main.c -lws2_32 -liphlpapi
http1: limits + target valid +/- ok
http1: field lookup + transfer-coding iteration ok
http1: request body plan fixed/chunked ok
http1: chunk line 5 -> "5\r\n" (+ext) ok
http1: message parse -> borrowed body "ok" ok
http1: body trailers rebind + trailer parse ok
```

**What just happened.** (1) Line 1 verifies the limits model: target length within the limit passes, over the limit refuses — both sides of the defense parameter. (2) Line 2 is the lookup combination: find a field by name + transfer-coding token iteration (Chapter 88's TokenNext applied to a Head). (3) Line 3 previews Chapter 90: the request body's Plan (fixed-length/chunked verdict) is available immediately after head parsing. (4) Line 4 verifies chunk size line generation (`5\r\n` — extensions allowed). (5) Line 5 is the Message layer: one parse of "head + body", the body a borrowed view `ok` — the thriftiest path for small messages. (6) Line 6 verifies trailer array rebinding and trailer block parsing — a Chapter 90 preview. Six lines together: this chapter's plus the next's API surface.

## Contracts

- **Bound array**: the Head binds the caller's field array; the field count is natively capped by the array capacity; no cleanup before reuse (every Parse rebuilds).
- **Three-state return**: `READY`/`NEXT`/`ERROR`; `NEXT` consumes nothing and reports no error; failure does not advance (retry semantics as in Chapter 71's framing).
- **Borrowed views**: Method/Target/Status/Reason/fields all borrow the input; `Bytes` is the head-block length including the terminating empty line (the body's start).
- **Lookup semantics**: field names case-insensitive; same-named multi-value fields traverse in order; a miss is an explicit failure, not a NULL value.
- **Packing atomicity**: status line + fields written in one shot; insufficient capacity writes zero; success publishes the total length.
- **Limit defense**: limits control head-block/method/target lengths; unlimited parsing is a memory amplifier; defaults conservative.
- **Method semantics**: response parsing carries the request-method context (the HEAD/204 no-body rule is executed by Chapter 90's Plan).
- **Zero per-request allocation**: the parsing and packing paths allocate no heap (Head/array/buffers all caller-provided).
- **Trimming**: `HTTP1_HEAD`/`HTTP1_BODY`/`HTTP1_MESSAGE` independent feature macros, depending on the HTTP foundation.

## Pitfalls

### Pitfall 1: after NEXT, re-Parsing from scratch (or not advancing the offset)

Symptom: a streaming server's "try a stretch per arrival" becomes an infinite loop or re-parses the same bytes — CPU spinning or duplicated fields.

Cause: `NEXT` means "this input is insufficient" — the correct action is to **append** new data to the same buffer and Parse again (the input is retried from the start; the parser guarantees the NEXT path has no side effects); or advance by the consumed offset it returns. Treating it as an "error" and dropping the buffer returns you to assembly hell.

```c bad
while ( recv(buf) > 0 ) {
	if ( Parse(buf) == XHTTP1_MORE ) {
		continue;   /* only the newly received bytes are tried: never complete */
	}
	break;
}
```

```c good
/* append-style buffer (Chapter 64): everything received accumulates in one span */
while ( recv_append(&In) > 0 ) {
	if ( xrtHttp1RequestParse(view(&In), &Head, NULL, NULL) ==
			XHTTP1_READY ) {
		break;   /* bytes after Bytes are the body/next request */
	}
}
```

### Pitfall 2: sizing the field array "exactly enough"

Symptom: slightly longer requests (legal, just two more fields) fail to parse — the server "randomly" rejects a subset of clients.

Cause: the bound array's capacity is a hard cap. Healthy browsers' request heads commonly carry 15-25 fields; proxy chains append the X-Forwarded-* family besides.

```c bad
xhttpfield Fields[4];   /* rejects most real requests */
xrtHttp1HeadInit(&Head, Fields, 4);
```

```c good
xhttpfield Fields[32];   /* rule of thumb: real requests 25 + forwarding headroom */
xrtHttp1HeadInit(&Head, Fields, 32);
/* genuinely oversized scenarios: ERROR distinguishes "field count exceeded";
   the proxy enlarges or explicitly rejects (itself an anti-abuse decision) */
```

### Pitfall 3: a packed response missing delimiting beyond Content-Length

Symptom: a homegrown response carries neither Content-Length nor a chunked declaration — the client cannot tell when the body ends, hanging until timeout or connection close.

Cause: an HTTP/1 body must carry a delimiting declaration (Content-Length, chunked, or "ends at connection close" — reliably HTTP/1.0 semantics only). The packing API writes the fields for you, but **the delimiting field is semantics you must provide**.

```c bad
xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, 200, OK,
	NoFields, 0, Out, sizeof(Out), &iSize);
send(Out, iSize);
send(Body, BodySize);   /* no delimiting: the client can't tell when it ends */
```

```c good
static const xhttpfield F[] = {
	{ XRT_STR_INIT("Content-Length"), XRT_STR_INIT("11") }
};
xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, 200,
	XRT_STR_LITERAL("OK"), F, 1, Out, sizeof(Out), &iSize);
send(Out, iSize);       /* the head declares 11 bytes */
send(Body, 11);         /* the body is exactly 11: delimiting complete */
```

## Exercises

### Basic: contrasting two methods

Change the http1 sample's request to POST (with `Content-Length: 5` and a 5-byte body); after parsing print `Head.Bytes` and verify "the body starts at Bytes". Acceptance criteria: Bytes matches a hand count; the body view corresponds to the original bytes.

### Advanced: sharded-arrival stress test

Deliver a 41-byte request in random shards of 1..N bytes (mocking TCP segmentation), appending and Parsing per shard — count from which shard READY arrives. Then deliberately inject malformation (a field name with two consecutive colons) and verify ERROR with prior state unpolluted. Acceptance criteria: any shard order eventually READY; the erroneous input reports ERROR when the malformed shard arrives.

### Challenge: a pipelined request parser

One buffer holds two complete requests glued together (HTTP/1.1 pipelining): parse request one (Bytes locates the boundary) → consume → parse request two on the remainder. Implement `parse_pipelined(输入, 回调)` (input, callback) looping until NEXT. Acceptance criteria: both requests parse correctly; an incomplete second stops at NEXT with the buffer continuable; throughout, one Head and one array, zero allocations.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Core entrances | RequestParse / ResponseParse (three-state); ResponseWrite / RequestWrite (one-shot packing) |
| Bound array | HeadInit binds the caller's field array; capacity is the field cap; reuse with zero allocation |
| Three states | READY (Bytes = body start) / MORE (await data, no side effects) / ERROR (does not advance) |
| Lookup | HeadFind case-insensitive; multi-value fields traverse in order; pair with TokenNext for list judgments |
| Packing | status line + fields in one write; insufficient capacity writes zero; delimiting fields (CL/chunked) are the caller's semantic duty |
| Limits | limits control head-block/method/target lengths; defaults conservative; against memory amplification |
| Method context | response parsing carries the request method; the HEAD/204 no-body rule is executed by the Body Plan |
| Message layer | head + body in one pass (small messages); the Body layer streams (Chapter 90) |
| Zero allocation | no heap allocation on the parse/pack paths; Head/array/buffers all from the caller |
