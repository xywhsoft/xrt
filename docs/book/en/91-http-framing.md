---
num: 91
slug: http-framing
title: Body Framing Illustrated: Fixed-Length, Chunked, and Boundaries
volume: 卷九 Web 协议核心
type: practice
lead: Plan first, streaming Body, Chunk writing with trailer collection — HTTP/1's three kinds of delimiting and a framing state machine you can read in one pass.
api: http1, http_te, http
---

## Orientation

Chapter 90 stopped at the empty line ending the head block; the body starts there — and "how the body ends" is HTTP/1's most error-prone question. This chapter illustrates three delimittings: **fixed-length** (Content-Length — read exactly N and stop), **chunked** (Transfer-Encoding: chunked — per-chunk length lines, terminated by the 0 chunk), and **close-delimited** (no declaration; connection close is the end — HTTP/1.0 semantics). The core design is **Plan first**: `xrtHttp1RequestBodyPlan`/`ResponseBodyPlan` derive "how to read" from the head first (method semantics included — HEAD/204 have no body), then initialize the streaming reader `xrtHttp1BodyInit/Read/Done` per the Plan; the write side symmetrically provides `ChunkWrite/ChunkEndWrite`. Trailers (the field block at chunked's tail) and TE negotiation (the client's capability declaration) live here too. This is Chapter 72's generic framing fully specialized for HTTP — a finer state machine, but the "incrementally absorb sticky and partial packets" skeleton unchanged.

## Introduction

A proxy forwards a 2 GB response: the upstream arrives chunked, the downstream client speaks only HTTP/1.0? Stream-read and forward chunk by chunk, or buffer all 2 GB first? How does a server declare delimiting for dynamically generated content (total length unknown before sending)? How does a client receive trailers at chunked's tail (cross-proxy integrity digests)? — The answers all hang on the **framing layer**: it decides whether "forward as it arrives" is possible (memory independent of body size), how "dynamic length" is expressed (chunked), and how "post-hoc metadata" is received (trailers).

The price of misused framing is not abstract: **delimiting ambiguity is request smuggling** — two consecutive requests on one connection understood differently at the boundary; the attacker hides the second request inside the first's "body" and slips it through the proxy. What when CL and TE both appear (must reject)? What when CL values are multiple (must reject)? — This chapter's contracts write these rules in stone.

## Concepts

### Three delimittings and Plan derivation

```diagram flow
- Fixed-length: Content-Length: N - read N bytes and done (simplest; the total must be known before sending)
- chunked: Transfer-Encoding: chunked - "hex length\r\ndata\r\n" per chunk, "0\r\n" terminates, a trailing trailer block
- Close-delimited: neither present - connection close is the end (HTTP/1.0 semantics; forbidden in 1.1 server responses, read-to-close by 1.1 clients)
- Plan: derived from head fields + method - per-role rules (response-side HEAD/204/304 have no body)
```

`xrtHttp1ResponseBodyPlan(&Head, 方法, &Plan)` (&Head, method, &Plan) and `RequestBodyPlan(&Head, &Plan)` produce the complete "how to read" decision: the delimiting kind, the total length (if fixed), and whether trailers are allowed.**Method participates in semantics**: a HEAD response and 204/304 responses declare no body even with a CL — the Plan zeroes them by role.**Ambiguity rejection**: CL coexisting with TE, multiple inconsistent CLs — the Plan returns failure (smuggling defense's first gate sits at the derivation layer).

### Streaming reads: the Body state machine

```diagram state
XHTTP1_BODY_DATA -> XHTTP1_BODY_DATA: 消费一段正文（Data 视图 + Consumed 字节）
XHTTP1_BODY_DATA -> XHTTP1_BODY_DONE: 定界满足（定长读够 / 0 块到达）
任意 -> XHTTP1_BODY_ERROR: 超上限 / 非法块行 / trailer 块超限
```

`xrtHttp1BodyRead(&Body, 输入视图, 是否终结, &消费, &数据视图, NULL)` (&Body, input view, final?, &consumed, &data view, NULL) is the core loop: feed the current buffer; it returns one of three states — "data out" (a Data borrowed view — direct zero-copy reading), "done", "error" — and `Consumed` reports how much was consumed this round (the remaining bytes belong to the next message/outside the body).**Limit defense** (`xhttp1bodylimits`): MaxBody (body total), chunk-line length, trailer count — maliciously huge chunks are stopped before entering. Trailers are collected via a bound array (pass the trailer array to `BodyInit`), queryable after `Done` — the same shape as Chapter 90's field array.

### Write side: generating chunks and the terminator

`xrtHttp1ChunkWrite(数据, 输出, 容量, &长度)` (data, output, capacity, &length) generates a complete "hex length\r\n + data\r\n" chunk; `xrtHttp1ChunkEndWrite(trailer数组, 数量, ...)` (trailer array, count, ...) generates the "0\r\n" terminator plus an optional trailer block. The two tools compose any stream: dynamic content generated and sent as it goes (with Chapter 68's Vec send amortizing syscalls); large files looped in fixed chunks. Fixed-length bodies are simpler — write CL in the head, send the body directly.**Selection rule**: total length known → CL (saving the 6-byte-per-chunk line overhead and parsing cost); unknown or streamed → chunked.

### Trailers: metadata after the body

chunked allows a field block after the terminator — `Digest` (content integrity), cross-proxy tracing IDs, the standard home of post-hoc metadata. Two disciplines: **read side** — trailer array capacity exceeded is an error (same logic as the field cap); **send-side negotiation** — only a client's `TE: trailers` declares trailer awareness (next section); the server should use them after confirmation; a non-supporting proxy stripping trailers is legal.

### TE negotiation: transfer-encoding capability declaration

`xrtHttpTeParse(字段数组, 数量, &Info)` (field array, count, &Info) aggregates the (repeatable) TE fields: the accepted transfer-coding list, the `XHTTP_TE_ACCEPTS_TRAILERS` flag (the client declares trailer awareness), and per-coding q-values. `xrtHttpTeQuality(字段, 数量, 编码名)` (fields, count, coding name) returns a **thousandths integer** (`gzip;q=0.5` → 500) — eliminating floating-point comparison's precision and platform drift. The server picks a mutually acceptable coding from this; q=0 means an explicit refusal. This "integer q-values + flags" negotiation model matches the other field families.

## Examples

### First complete program: Plan, streaming read + chunked write

The program below is from `examples/http/http1_body/main.c` — the complete read-side and write-side loop:

```embed path="examples/http/http1_body/main.c" title="examples/http/http1_body/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/http1_body/main.c -lws2_32 -liphlpapi
chunked body
5
hello
0
```

**What just happened.** **Read side** (line 1 of the first four outputs): (1) the response head declares `Transfer-Encoding: chunked`; after `ResponseParse`, `ResponseBodyPlan` (with method GET) derives the chunked plan; `BodyLimitsInit` sets MaxBody=1024, then `BodyInit` binds the trailer array. (2) The input is "`7\r\nchunked\r\n5\r\n body\r\n0\r\nDigest: ok\r\n\r\n`" — the `BodyRead` loop emits the two segments `chunked` and ` body` (printed joined as `chunked body`); after the `0` chunk arrives, the trailer `Digest: ok` lands in the bound array, state DONE. Zero-copy: the Data view points straight into the original buffer.**Write side** (the last three outputs): (3) `ChunkWrite("hello")` yields `5\r\nhello\r\n` (printed as `5` and `hello` on two lines); `ChunkEndWrite(带 Digest trailer)` (with a Digest trailer) yields `0\r\nDigest: sha-256=:demo:\r\n\r\n` (the leading `0`) — **the byte sequence the write side generates is exactly the format the read side consumes**; this is Chapter 72's "read side and write side share one framing state machine", HTTP edition.

### Second complete program: TE negotiation read

The second program is from `examples/http/te/main.c` — the aggregated query of client capabilities:

```embed path="examples/http/te/main.c" title="examples/http/te/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http/te/main.c -lws2_32 -liphlpapi
codings=1 trailers=yes gzip=500
```

**What just happened.** (1) Two TE fields (HTTP allows repetition) are aggregated by `TeParse` into one capability: 1 transfer coding (gzip — `trailers` is a capability flag, not a coding) and trailer acceptance. (2) `TeQuality("gzip")` returns 500 — the thousandths-integer form of `q=0.5`; floating-point negotiation's precision traps (the 0.1+0.2≠0.3 family) are eliminated at the representation layer. (3) The server's decision chain: `trailers=yes` → allowed to send Digest at chunked's tail; `gzip=500` → gzip usable but low priority. The companions `examples/http1/variants` and `examples/http/trailer` cover delimiting variants and trailer details respectively — this chapter's exercises will use them.

## Contracts

- **Plan first**: before reading a body, a Plan is mandatory (head-derived); method participates in semantics (response-side HEAD/204/304 have no body); ambiguous delimiting (CL+TE coexisting, inconsistent multiple CLs) is rejected at the Plan layer — smuggling defense's first gate.
- **Body three states**: DATA (Data borrowed view + Consumed) / DONE (delimiting satisfied) / ERROR; loop feeding until DONE; on error, stop.
- **Limit defense**: three caps — MaxBody/chunk-line length/trailer count; over-limit rejection before memory is consumed — limits are a deployment decision.
- **Trailer collection**: collected into a bound array; capacity exceeded is an error; queryable after Done; the read side does not check "did the client declare trailers" (that is send-side negotiation).
- **Write-side symmetry**: ChunkWrite (chunk) + ChunkEndWrite (terminator + optional trailer); shares the format state machine with the read side; capacity atomicity as in Chapter 89.
- **Delimiting choice**: total length known → CL; streamed → chunked; a 1.1 server must not bare-close-delimit.
- **TE negotiation**: repeatable fields aggregated; q as thousandths integers (no floats); the `ACCEPTS_TRAILERS` flag; unmentioned codings q=0.
- **Zero-copy**: the Data view borrows the input; the body never transits a staging buffer — a proxy's "forward as it arrives" keeps memory independent of body size.
- **Message layer**: `MessageParse` combines head + body in one pass (the small-message convenience path); streaming scenarios use the Body family.

## Pitfalls

### Pitfall 1: "picking one to trust" when CL and TE coexist

Symptom: a homegrown proxy handles a request carrying both Content-Length and Transfer-Encoding by CL while the backend goes by TE — the next request on the connection is "hidden" inside the body: request smuggling.

Cause: two delimittings coexisting is ambiguous input — front and back ends understanding it differently is exactly how smuggling attacks are constructed. The only safe action is to **reject**.

```c bad
if ( has_content_length(Head) ) {
	plan_as_fixed(...);      /* I read by CL */
} else {
	plan_as_chunked(...);
/* if the downstream understands the opposite: boundary misaligned -> smuggling window */
}
```

```c good
/* the Plan layer has the rejection built in: let the derivation result speak */
if ( !xrtHttp1RequestBodyPlan(&Head, &Plan) ) {
	return reject("ambiguous framing");  /* ambiguous delimiting: the only safe response */
}
```

### Pitfall 2: BodyRead's Consumed never wired to the consuming offset

Symptom: a streaming proxy drops bytes or forwards duplicates — each round's feed starts at the wrong place.

Cause: `BodyRead`'s input is "where the current buffer starts is your decision" — how much it consumed (`iConsumed`) must accumulate into the next round's offset; the unconsumed tail of the buffer may belong to the next message (pipelining).

```c bad
size_t iOff = Head.Bytes;
while ( !Done ) {
	BodyRead(&Body, view(In), false, &iUsed, &Data, NULL);
	forward(Data);
	/* iUsed lost: next round feeds from the original offset again */
}
```

```c good
size_t iOff = Head.Bytes;
while ( !xrtHttp1BodyDone(&Body) ) {
	xhttp1bodystatus St = xrtHttp1BodyRead(&Body,
		(xbytesview){ In.Data + iOff, In.Size - iOff },
		bFin, &iUsed, &Data, NULL);
	iOff += iUsed;                 /* consumption wired to the offset */
	if ( St == XHTTP1_BODY_DATA ) { forward(Data); }
	else if ( St != XHTTP1_BODY_DONE ) { return false; }
}
/* data in In beyond iOff belongs to the next message */
```

### Pitfall 3: server-side dynamic content "buffered fully, then fixed-length"

Symptom: to write Content-Length, the entire dynamic response is buffered in memory — a 2 GB export endpoint buffers the server into OOM.

Cause: CL requires the total before sending. When content is generated as a stream, the only correct answer is chunked — each chunk is sent as generated; memory is independent of the total.

```c bad
buffer_whole_response(&Body);   /* buffered all 2 GB */
set_content_length(Body.Size);
send_all(Body);
```

```c good
send_header_chunked();
while ( next_chunk(&Data) ) {
	xrtHttp1ChunkWrite(Data, Out, sizeof(Out), &iSize);
	send(Out, iSize);            /* generate and send as it goes */
}
xrtHttp1ChunkEndWrite(Trailers, 1, Out, sizeof(Out), &iSize);
send(Out, iSize);               /* 0 chunk + trailer finish */
```

## Exercises

### Basic: walk each of the three delimittings

Construct three response byte streams (CL=11, chunked with two chunks, no declaration); Plan + Body each stream to DONE, printing the action sequences (consumption rounds/data segments). Acceptance criteria: the three conclusions match hand-derivation; the undeclared group's "close delimiting" reaches DONE when bFin=true (peer closed).

### Advanced: a smuggling-sample experiment

Construct four malicious samples: CL+TE coexisting, inconsistent double CL, over MaxBody, over-long chunk line — verify the rejections at the Plan/Body layers one by one. Tabulate which layer rejected each sample. Acceptance criteria: all four rejected; you can point at which layer of the parsing pipeline each gate sits in.

### Challenge: a streaming transcoding proxy core

The upstream chunked response arrives chunk by chunk: Body streams it → each data segment is "uppercased" → ChunkWrite re-chunks (fixed 16-byte chunks) → forward downstream; trailers forwarded verbatim (recomputing Digest optional). Acceptance criteria: memory usage independent of the upstream total (against Chapter 6's statistics); upstream and downstream chunk boundaries differ (7+5 in, 16×N out) yet semantics hold; when the upstream breaks, the downstream gets a clean error, not half a frame.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three delimittings | fixed-length CL / chunked (0-chunk terminator) / close-delimited (1.0 semantics) |
| Plan first | derived from head + method; HEAD/204 no body; ambiguous delimiting (CL+TE, inconsistent CL) rejected |
| Body three states | DATA (view + Consumed) / DONE / ERROR; the consumed offset must accumulate |
| Three limit lines | MaxBody / chunk-line length / trailer count; reject before consuming; a deployment decision |
| Trailers | field block after the terminator; collected in a bound array; the sender needs the client's TE trailers |
| Write-side duo | ChunkWrite (length line + data) / ChunkEndWrite (0 chunk + trailer); same format machine as the read side |
| Delimiting choice | total known → CL; streaming → chunked; bare close forbidden for 1.1 servers |
| TE negotiation | repeatable fields aggregated; q thousandths integers; ACCEPTS_TRAILERS flag; unmentioned q=0 |
| Zero-copy | Data borrows the input; forward-as-arrived memory independent of body size |
| Message layer | head + body in one pass (small messages); the Body family is the streaming primitive |
