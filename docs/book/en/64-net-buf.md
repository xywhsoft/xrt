---
num: 64
slug: net-buf
title: Buffers and Buffer Pools
volume: 卷七 网络
type: practice
lead: The xnetbuf reference-chain buffer — four append kinds (copy/borrow/take/callback), coalescing and consuming, pooled reuse, and release callbacks.
api: net
---

## Orientation

`xnetbuf` is the network layer's **reference-chain buffer**: data is organized as a chain of spans rather than one contiguous block — the natural shape of network data (arriving in segments, partially consumed); the **four append kinds** divide by ownership (copy/borrow/take/callback release) — Chapter 5's ownership language fully landed on IO buffers; **coalescing and consuming** (Pullup for prefix contiguity, Consume for prefix consumption — zero-copy advancement); and the **buffer pool** (xnetbufpool — segment-block reuse, allocation choked down). It is the engine's direct carrier for send/receive — Chapter 66's gold-standard `SendBuffer` zero-copy echo is exactly its stage.

## Introduction

The life of a stretch of network data: arriving in segments (TCP segments, UDP datagrams) → assembled into a message → partially consumed (header read, body remains) → sent downstream. A contiguous buffer (Chapter 15's xbuffer) suits "accumulate then take", but the network's "segmented arrival + partial consumption" shape makes contiguity copy at every step — a copy when the header arrives, another when the body appends, another after consuming the header.

The reference-chain buffer changes the model: data lives as a chain of spans — **arrival becomes a span (zero-copy), consumption advances a pointer (zero-copy), and only when a contiguous view is needed does Pullup (local copy)**. The four append kinds unify every source (a received stretch, a header on the stack, a malloc'ed block, external memory with a callback) into one chain — ownership differences are declared on entry and released each in its own way on exit (Clear). Pooling lets segment blocks (chain nodes) be reused — allocation counts on high-frequency send/receive paths drop to near zero.

## Concepts

### The four append kinds: the complete ownership set

| Entry | Ownership | Typical source |
| --- | --- | --- |
| `xrtNetBufAppend` | Copy | Safe chaining of arbitrary memory |
| `xrtNetBufAppendBorrow` | Borrow | Caller stack/static data (not freed on exit) |
| `xrtNetBufAppendTake` | Take | An xrtMalloc block (xrtFree on exit) |
| `xrtNetBufAppendRef` | Callback | External memory + release function (callback on exit) |

The four kinds are the buffer edition of Chapter 5's **owning/borrowing/managed** complete set (copy is a form of owning, Take is ownership transfer, Borrow is borrowing, Ref is custom management). The buf_tour example assembles "hello world" from one span of each kind — with an assertion of **release correctness**: the AppendRef callback must fire exactly once after Clear. **Prepend** (insert at chain head) and **Move** (whole-buffer transfer) round out the shape operations.

### Shape and consumption

```diagram flow
- Shape queries: Empty / Size / SpanCount / Spans — the chain overview
- Pullup(prefix N): make the first N bytes contiguous (local cross-span copy — the precondition for header parsing)
- Peek(offset): copy-out view at an offset (chain untouched)
- Find(byte): cross-span search (delimiter location; XRT_NPOS on miss)
- Consume(N): consume an N-byte prefix (advance the chain head — zero-copy advancement)
```

**Pullup is the bridge between chain and contiguous view**: protocol parsing needs contiguous memory to read headers (memcmp/struct reads) — Pullup pulls the first N bytes into the first span (copying to merge if insufficient), leaving the rest of the chain untouched; **Consume is zero-copy advancement**: consuming N bytes merely frees the spans walked past — the protocol state machine's read-header → consume loop runs with zero surplus copies. Together the two form the **standard engine of protocol parsing** (Chapter 85's HTTP header parsing is assembled from them).

### Reserve and commit

`Reserve + Commit/Cancel`: reserve N bytes of **writable space** at the chain tail (the direct output path to the network — the engine reserves, fills, and commits into a span when sending); Cancel abandons an uncommitted reservation. This is the send-side zero-copy write channel — the fill posture before the engine's Send.

### The buffer pool

`xrtNetBufPoolCreate` (ConfigInit configures segment size/watermarks) — segment blocks come from the pool and Clear returns them — allocation counts on high-frequency paths approach zero (Chapter 22's pooling idea landed on IO segment blocks). `PoolTrim` tightens empty segments; the pool's info queries carry live/idle counts (observation fields). **Pool and buffer relationship**: buffers (chain logic) do not mandate a pool — a bare chain works too (a malloc per span); engines and high-frequency scenarios pair with a pool — `PoolGet` fetches a buffer with segments and Clear auto-returns to the pool.

## Examples

### Complete program: four append kinds and chain operations

From the repository example `examples/network/buf_tour/main.c` — six assertion groups over the full interface:

```embed path="examples/network/buf_tour/main.c" title="examples/network/buf_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/buf_tour/main.c -lws2_32 -liphlpapi
buf: pool create + info ok
buf: append x4 borrow/take/ref -> 11 bytes in >=4 spans
buf: prepend+pullup+peek+find+consume ok
buf: reserve-cancel keeps 11 bytes ok
buf: move source->target ok
buf: trim released >=1 block, release-cb fired once
```

**What just happened.** (1) Pool creation and info query — `pool create + info`. (2) **The four append kinds in joint performance**: copy 5 + borrow 2 + take 2 + callback 2 = 11 bytes in at least 4 spans — ownership declared on entry, with double assertions on span count and byte count. (3) The consumption trio: Prepend (insert at head), Pullup (prefix contiguity — the precondition for cross-span header parsing), Peek/Find/Consume (view/search/advance) — the complete operational surface of the protocol-parsing engine. (4) Reserve+Cancel: reserve then abandon; the chain keeps 11 bytes (a reservation never pollutes committed data). (5) Move: the source chain transfers wholly into the target and the source returns to empty — zero-copy buffer handoff (the foundation of Chapter 66's reference handoff). (6) Trim reclaims empty segments + **release-cb fired once** — the AppendRef callback fired exactly once, testimony of release correctness (ownership's acceptance enforcement).

### Complete program: basic send/receive

From `examples/network/buffer/main.c` — a simple verification printing written contents:

```embed path="examples/network/buffer/main.c" title="examples/network/buffer/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/buffer/main.c -lws2_32 -liphlpapi
（写入内容逐段打印——链的内容遍历）
```

**What just happened.** (1) Basic Append with content printing — the minimal chain lifecycle (Init → Append → inspect → Unit). (2) This is the atomic edition of buf_tour — the four append kinds and the consumption trio all grow out of here. **The two examples' division**: beginners run this one first (Init/Append/Unit in three steps); buf_tour is the full-interface acceptance (six ok groups for six operation families). Chapter 66's gold-standard `SendBuffer` (zero-copy echo by direct chain handoff) meets this type again in engine practice — the buffer chain is the engine data plane's currency.

## Contracts

- **Four ownership kinds**: copy/borrow/take/callback — declared on entry, each released its own way on exit (the Ref callback exactly once).
- **Chain shape**: segmented zero-copy; contiguous views via Pullup (local), consumption via Consume (zero-copy advancement).
- **Reserve channel**: Reserve → fill → Commit into a span / Cancel to abandon — zero-copy writing on the send side.
- **Pooling**: segment blocks return to the pool for reuse; Trim tightens; pool info carries live/idle (observation).
- **Move handoff**: whole-chain transfer with zero copies — the buffer edition of reference handoff (Chapter 66's SendBuffer foundation).

### From examples to engineering: the buffer chain's three hosts

**The engine send/receive face** (the hottest path): the engine callback hands over an `xnetbuf` (span chain — data already in spans, zero-copy), the handler consumes (Consume, zero-copy advancement), the echo moves directly (SendBuffer reference handoff — the gold standard's core demonstration); the send side fills via Reserve/Commit. Not a single copy anywhere — the reference chain in full bloom. **Protocol parsers**: Pullup(header length) → parse → Consume(header) → loop — the foundation of Chapter 85's HTTP header parsing; Find does the delimiting (delimiter protocols), Peek views (dispatch after type detection). **Data hauling**: file → network (the block read out by Chapter 47's asynchronous read AppendTakes into the chain — zero-copy takeover), network → file (the chain Moves into the write channel) — cross-IO-domain data handoff uses the chain as the container. The three hosts share one point: **the chain is the data-plane currency** — in contrast with the value tree being Volume 4's control-plane currency (control plane: value trees; data plane: chains — two currencies each on their own channel, the handshake going through the domain name (resolved result addresses)).

### Division of labor with Chapter 15's xbuffer and Chapter 47's async

Three buffer tools are easily confused; one comparison sets the boundaries. **xbuffer (Chapter 15)**: a contiguous byte accumulator — "accumulate then take" (file reads, the accumulate stage of protocol decoding); copy semantics, Take removes the whole contiguous block. **xnetbuf (this chapter)**: a reference span chain — "segmented arrival, partial consumption" (network send/receive, protocol state machines); zero-copy advancement, per-span ownership. **Async file results (Chapter 47)**: `xfiledata` — one block of data from one IO (owning) — its Data can AppendTake into the chain (file → network, zero-copy). Choose by data shape: contiguous needs use xbuffer, chain consumption uses xnetbuf, single-block handoff uses filedata → Take into the chain. A protocol stack's typical pipeline: port receives spans → chain → header parsing → body accumulates in xbuffer → value tree — each tool in its place.

### A design view: the boundaries of zero-copy

This chapter is all about zero-copy — but know its boundaries. **The right place for zero-copy**: data movement on hot paths (executed per message) — engine to handler, handler to downstream; **the reasonable place for copying**: boundary crossings (protocol-boundary contiguity — Pullup locally), ownership conversions (an unstable Borrow source becomes an Append copy), small data (copying a few header bytes is cheaper than span management). **The criterion**: large data with clear lifetimes → zero-copy; small data or complex lifetimes → copy for simplicity. The 11 bytes in four spans in buf_tour is pedagogical exaggeration — real chain granularity is decided by the engine's buffer policy (segment size in pool config); the chain a handler sees is already sensibly segmented. **The price of abusing zero-copy is mental load** (freeze windows, borrow contracts, callback releases) — every lesson of Chapter 5's ownership language pays off here.

## Pitfalls

### Pitfall 1: a borrowed span outliving its source

Symptoms: dangling data read from the chain — the Borrowed span becomes a wild pointer after the source is freed; timing-related and hard to reproduce.

Cause: Borrow's contract is "the source lives as long as the chain" — freeing the source early violates the contract (the chain edition of Chapter 3's borrowing discipline).

```c bad
void appendStack(xnetbuf* Buf)
{
	char Header[4] = { 0, 1, 2, 3 };
	xrtNetBufAppendBorrow(Buf, Header, 4);   /* borrow a stack array */
}   /* function returns — the stack frame is reclaimed, the chain's span dangles */
```

```c good
void appendCopy(xnetbuf* Buf)
{
	char Header[4] = { 0, 1, 2, 3 };
	xrtNetBufAppend(Buf, Header, 4);   /* copy into the chain — source lifetime irrelevant */
	/* or Take (take over a malloc block) / Ref (custom release) by source */
}
```

### Pitfall 2: treating the chain as contiguous without Pullup

Symptoms: occasional garbled protocol-header parsing — a header crossing spans reads spliced-wrong values; fine with one span, broken with several.

Cause: the chain guarantees no contiguity — a direct `memcpy(buf, Spans[0].Data, HeaderSize)` sees only the first span (the header may straddle two).

```c bad
xnetspan Spans[8];   /* caller-provided array (returns the actual span count) */
size_t nSpans = xrtNetBufSpans(Buf, Spans, 8);
memcpy(Header, Spans[0].Data, 8);   /* copies only the first span — garbled when the header crosses spans */
```

```c good
if ( xrtNetBufSize(Buf) >= 8 ) {
	xrtNetBufPullup(Buf, 8);   /* make the 8-byte prefix contiguous (local cross-span copy) */
	xnetspan Out[1];
xrtNetBufPullup(Buf, 8, Out);
memcpy(Header, Out[0].Data, 8);
}
```

## Exercises

### Basic: reproduce the four kinds

Assemble "hello world" from one span of each append kind; print Size and SpanCount; after Clear, verify the Ref callback fired exactly once.

### Advanced: a protocol-header consumption loop

Simulate a TLV stream: a randomly segmented byte stream enters the chain → Pullup(4) to read the length → Find the delimiter → Consume the whole record → loop until less than one remains — the standard chain-parsing loop (a rehearsal of Chapter 85).

### Challenge: a pooled send/receive benchmark

Run the same receive workload (1 million 1KB segments appended/consumed) with a bare chain and a pooled chain — compare allocation counts (Chapter 6's stats) and elapsed time. Acceptance: the pooled version allocates two orders of magnitude less; time clearly wins; after Trim the pool's idle count returns to zero (clean finish); the comparison table goes into the report.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four append kinds | Append copy / Borrow borrow / Take take / Ref callback — the complete ownership set |
| Shape | Size / SpanCount / Spans (fills a caller array) — chain overview; Prepend inserts at the head |
| Consumption trio | Pullup prefix contiguity / Peek view / Find search (XRT_NPOS) / Consume zero-copy advance |
| Reserve | Reserve → fill → Commit / Cancel — zero-copy writing on the send side |
| Pool | PoolGet fetches a buffered chain / Clear returns to pool / Trim tightens — allocation choked |
| Move | whole-chain transfer, zero copies — the SendBuffer foundation (Chapter 66) |
