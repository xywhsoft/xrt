---
num: 93
slug: http-decode
title: Body Decoding: Streaming Content-Encoding Restoration
volume: 卷九 Web 协议核心
type: practice
lead: Build a streaming decoder from the response head, feed segments with callback pushes, verify at Done — the network streaming posture for gzip/deflate plus content negotiation.
api: http_decode, http_encoding, http1
---

## Orientation

Chapter 91 solved "the body's boundaries"; this chapter solves "the body's content encoding". What the framing layer reads out of a `Content-Encoding: gzip` response is **compressed bytes** — the business wants plaintext. `xrtHttpDecodeCreate` builds a streaming decoder directly from the response head fields (auto-chaining multi-level encodings — declarations like `gzip, br`), `xrtHttpDecodeWrite` feeds segment by segment with decoded plaintext pushed to a callback, and `xrtHttpDecodeDone` verifies the ending (gzip's CRC and length trailer — against silent corruption). The design posture matches Chapter 91: **segments decoded as they arrive, plaintext pushed straight to the consumer** — no full-response buffering anywhere; memory usage is independent of body size. Paired with the send side's content negotiation (Accept-Encoding's q-values), this completes the compression loop and closes Volume 9's HTTP core.

## Introduction

Downloading a 50 MB gzip-compressed JSON: the naive implementation is "receive the entire response → decompress in one block → get 80 MB of plaintext" — 130 MB peak memory, and processing starts only after the last byte arrives. The streaming implementation is "each compressed segment that arrives → immediately decode a stretch of plaintext → push to the parser" — constant memory (on the order of one sliding window), first-byte latency roughly the network's first segment. The difference is not the gzip algorithm (Chapter 29's decompressor is already streaming) but the **wiring**: who connects "the segments the HTTP framing layer emits" to "the decompressor's input", who handles the chaining of multi-level encodings, and who verifies the ending?

The `http_decode` module is that wire: its input end is the Data view Chapter 91's `BodyRead` emits, its output end your callback, and in between it auto-chains every level the Content-Encoding declares. Whatever the head field declares is what gets decoded — including `identity` (no decode) and multi-level chains; a declared-but-not-compiled encoding (like br) fails creation explicitly — never a silent skip (silent = handing compressed data to the business as plaintext; everything downstream is wrong).

## Concepts

### Built from the head: Create's contract

`xrtHttpDecodeCreate(字段数组, 数量, 可选配置)` (field array, count, optional config) scans the `Content-Encoding` field (multiple entries chained in order of appearance) and builds the decoder: `gzip` (Chapter 29's Deflate family's gzip container), `deflate` (zlib wrapping), `identity` (pass-through), and multi-level chains (even the pathological but legal declaration of `gzip` over `gzip` is handled correctly). No `Content-Encoding` field = pass-through.**Failure semantics**: an unknown or not-compiled encoding makes creation return `NULL` — the caller decides on degradation (discard the response) or an error; never "guess something close and decode anyway".

The configuration (optional; `NULL` takes defaults) distinguishes **compat and safe modes**: legacy servers (old deflate implementations especially) produce some non-conformant output; compat mode accepts it through a leniency window (browser-compatible behavior), while safe mode strictly rejects — the default takes the safe side, relaxed explicitly when talking to legacy backends. This is the standard compromise of the "strict parsing" discipline facing historical baggage: strict by default, leniency must be explicit.

### Streaming feed: Write and the callback

```diagram flow
- Arrival: BodyRead emits a compressed Data view (Chapter 91)
- Feed: DecodeWrite(decoder, Data, is-this-the-last, callback, context)
- Push: the decoded plaintext segment enters the callback immediately (zero batching) - the business processes as it arrives
- Finish: DecodeDone verifies the gzip CRC and length trailer - against silent corruption
```

The `bFinal` parameter marks "this is the last segment" — the gzip stream's terminating block (with CRC32 and original length) is verified here; `Done` may also be called separately after Write (some transports separate "feeding is done" from "the last segment").**The callback shape's meaning**: the decoder does zero allocation of any buffer for you — plaintext is pushed to you, and the buffering strategy (parse directly? write to a pipe? flush to disk?) is entirely yours. In the same lineage as Chapter 29's decompression callbacks and Chapter 91's Body views.

### The loop with content negotiation

Decoding is the response side; the request side declares "what I can decode": `Accept-Encoding: gzip;q=1.0, deflate` (`xrtHttpAcceptEncodingQuality` queries the q-value — the thousandths-integer model, same as Chapters 91/92). The server picks a mutually acceptable encoding from the request head and writes it into `Content-Encoding`. The client's discipline: **declare only what you can truly decode** — writing `br` into Accept-Encoding without a compiled brotli decoder means the server picks br and your Create fails. The `http_encoding` module provides the request side's parsing and construction.

### The composition panorama: one complete response's consumption pipeline

```diagram flow
- Head: ResponseParse (Chapter 90) -> field array
- Framing: ResponsePlan + streaming Body read (Chapter 91) -> compressed segment Data views
- Decoding: DecodeCreate (from the head) -> Write per segment -> plaintext callback
- Consumption: the callback does the business (incremental JSON parsing/flushing/forwarding)
- Verification: DecodeDone + BodyDone - content and boundary doubly confirmed
```

Four layers, each at its post, with only views and callbacks between layers — the complete skeleton Volume 9's first five chapters assemble; Chapter 98's xhttp runtime wraps it into high-level APIs.

## Examples

### First complete program: from response head to plaintext callback

The program below is from `examples/http/decode/main.c` — a 42-byte gzip frame stream decoded whole:

```embed path="examples/http/decode/main.c" title="examples/http/decode/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http/decode/main.c -lws2_32 -liphlpapi
hello compressed world
```

**What just happened.** (1) The input is an embedded 42-byte gzip stream (starting with the `1F 8B` magic) plus a `Content-Encoding: gzip` field — mocking "head array + compressed body" already emitted by Chapter 91. (2) `DecodeCreate` scans the field and builds the decoder: one level of gzip. (3) `DecodeWrite` feeds once (`bFinal=true` — this sample arrives in a single segment; streaming scenes pass false per segment, true on the last), and the decoded plaintext **is pushed straight to the `printBody` callback** — on the network you would swap in a JSON parser or pipe writer here, memory not growing with the body. (4) `DecodeDone` verifies the ending: a gzip stream whose trailing CRC32 and length fields don't match fails here — **compressed data corrupted in transit: better an error than wrong data** (silent corruption is the compression chain's sneakiest failure). (5) Destroy releases — the decoder is this module's only owning object.

### Second complete program: the decoder full-feature tour

The second program is from `examples/http/decode_tour/main.c` — three lines covering modes, content mode, and reset:

```embed path="examples/http/decode_tour/main.c" title="examples/http/decode_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http/decode_tour/main.c -lws2_32 -liphlpapi
decode: config compat vs safe + create ok
decode: gzip content mode + input/output sizes ok
decode: reset to identity passthrough ok
```

**What just happened.** (1) Line 1 contrasts the two configuration modes and verifies creation — the compat/safe divide is Chapter 91's "strict by default, leniency explicit". (2) Line 2 verifies the gzip content mode and input/output size accounting — `input/output sizes` give directly usable data for compression-ratio computation and monitoring metrics. (3) Line 3 **resets to identity pass-through** — the next response on the same connection may be uncompressed: the decoder state zeroes and switches modes, the object reused without rebuilding (one more notch down in per-response cost for high-connection services).

## Contracts

- **Built from the head**: `DecodeCreate` scans Content-Encoding (multiple entries chained in order); no field = pass-through; unknown/not-compiled encodings fail creation — no guessing, no silent skipping.
- **Modes**: safe default (strict); compat relaxes explicitly (a leniency window for legacy servers); a `NULL` config takes the safe default.
- **Streaming**: Write per segment (bFinal marks the last); plaintext pushed to the callback immediately; a failed callback fails that segment's processing — the decoder buffers nothing for you.
- **Ending verification**: Done verifies the gzip CRC and length trailer; failure = corrupted content; the correct action is discarding the whole response, not using a part.
- **Reset**: Reset zeroes and switches to identity — one object reused across multiple responses on a connection.
- **Content negotiation**: the Accept-Encoding declaration must match decoding capability; the `http_encoding` module (AcceptEncodingParse/Quality/Select) provides the request side's parsing/construction; q thousandths integers.
- **Object model**: Create/Destroy owning (this module's only allocation point); Write/Done allocate nothing extra.
- **Trimming**: `XRT_MODULE_HTTP_DECODE` depends on COMPRESS and the HTTP foundation — non-decompressing clients trim it at zero cost.
- **Composition boundary**: upstream connects to Body's Data view (Chapter 91); downstream to your callback — zero-copy runs through the layers.

## Pitfalls

### Pitfall 1: bFinal always true (or always false)

Symptom: always true — from the second segment of a multi-segment stream everything is wrong (the decoder finish-verifies each segment as a complete stream); always false — Done forever reports "stream incomplete", and the CRC check is decoration.

Cause: `bFinal` is the "the transport layer knows this is the last segment" signal, coming from your judgment of Chapter 91's Body DONE state — not something to fill in arbitrarily.

```c bad
while ( read_chunk(&Data) ) {
	DecodeWrite(pDec, Data, true, cb, ctx);  /* every segment treated as the last */
}
```

```c good
while ( (St = xrtHttp1BodyRead(&Body, In, bEof,
		&iUsed, &Data, NULL)) != XHTTP1_BODY_DONE ) {
	if ( St != XHTTP1_BODY_DATA ) { return false; }
	/* last segment: the Body layer is about to DONE (next round is DONE) */
	DecodeWrite(pDec, Data, false /* except the true last */, cb, ctx);
}
xrtHttpDecodeDone(pDec);   /* verify the decode layer after the framing layer closes */
```

### Pitfall 2: after Done fails, "using the data already pushed out"

Symptom: the CRC verification fails (transport corruption), but the callback has already pushed 90% of the plaintext downstream — downstream treats the incomplete data as complete.

Cause: streaming's native tension — the data is already out before you know the ending is bad. The fix is **treating downstream transactionally too**: on CRC failure the downstream must be able to void (discard/rollback), or you choose the conservative "buffer everything, confirm, then commit" mode (trading streaming away for safety).

```c bad
/* the callback writes plaintext straight into the database */
static bool onPlain(xbytesview Data, ptr pCtx) {
	db_append((db*)pCtx, Data);   /* when Done fails, it's already written: dirty data */
	return true;
}
```

```c good
/* the callback writes into voidable staging (file/memory segment); commit once Done passes */
static bool onPlain(xbytesview Data, ptr pCtx) {
	staging_append((staging*)pCtx, Data);
	return true;
}
/* ... */
if ( !xrtHttpDecodeDone(pDec) ) {
	staging_discard(&Staging);   /* void the whole: a clean failure */
} else {
	staging_commit(&Staging);
}
```

### Pitfall 3: Accept-Encoding declares br while the build has no brotli

Symptom: the server negotiates brotli compression; the client's `DecodeCreate` fails — the download feature "randomly" breaks for a subset of sites.

Cause: a content-negotiation declaration is a **capability promise**. Every encoding written into Accept-Encoding, your build must truly decode. XRT currently supports gzip/deflate/identity — don't write br.

```c bad
/* a "standard header" copied from somewhere */
send_header("Accept-Encoding: gzip, deflate, br, zstd");
/* the server picks br -> Create fails */
```

```c good
/* declaration matches capability: this build supports gzip/deflate */
send_header("Accept-Encoding: gzip, deflate");
/* br not declared -> the server won't pick it */
```

## Exercises

### Basic: a segmented-feed experiment

Feed the decode sample's 42-byte stream in three segments of 7/13/22 bytes (the first two with bFinal=false) and verify the output unchanged. Then deliberately flip one byte of the last segment and verify Done fails. Acceptance criteria: segmented output matches whole; the CRC failure is caught and reported as a decoding error.

### Advanced: chaining the complete pipeline

String Chapters 90/91/93 into a complete consumption chain: a complete chunked+gzip response byte stream (self-provided; curl can generate it) → ResponseParse → BodyPlan/Body streaming read → DecodeCreate/Write → the callback tallies total plaintext length → double Done verification. Acceptance criteria: the plaintext matches `curl --compressed`; the whole chain buffers no full response (against Chapter 6's statistics); on any layer's failure, later layers do not run.

### Challenge: a conditional-negotiation client

Implement `fetch(Url, 支持的编码列表)` (Url, supported encodings): the request side builds Accept-Encoding per capability (q: gzip 1000, deflate 900); the response side runs the complete pipeline; a response whose `Content-Encoding` is outside your capability (a server ignoring negotiation) returns an explicit error rather than decoding wrongly. Acceptance criteria: correct for all three responses — gzip/deflate/none; out-of-capability encodings report "unsupported content-encoding"; 8 concurrent downloads with constant memory.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three entrances | Create (built from head fields) / Write (per-segment callback push) / Done (CRC ending verification) |
| Encoding support | gzip / deflate / identity / multi-level chains; unknown or not-compiled fails creation |
| Modes | safe default strict; compat relaxes legacy leniency explicitly; a NULL config takes the safe default |
| bFinal | the last-segment mark - from the Body layer's DONE judgment; always-true/always-false are both wrong |
| Callback shape | plaintext pushed immediately; the buffering strategy is yours; the decoder allocates nothing |
| Ending semantics | Done failure = void the whole response; downstream must be voidable (staging mode) |
| Reset | Reset zeroes and switches to identity - reused across responses on a connection |
| Content negotiation | declare only what you truly decode; q thousandths; the encoding module builds the request side |
| Composition boundary | upstream the Body's Data view, downstream your callback - zero-copy throughout |
| Trimming | XRT_MODULE_HTTP_DECODE depends on COMPRESS; non-decompressing trims at zero cost |
