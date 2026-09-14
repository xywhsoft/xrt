---
num: 111
slug: xws-send
title: xws (Part 2): The Send Path — Writer, References, and Compression
volume: 卷十一 其他扩展库
type: practice
lead: The xwswriter for fragmenting long messages, the same backpressure semantics for compressed sends, and the complete matrix of three-state ownership — every shape of the sending side.
api: xws-websocket_runtime, xws-websocket_http
---

## Orientation

Chapter 110 established the three send-ownership states (copy/ref/take); this chapter walks the send path to completion. **writer** (`xwswriter`): streaming fragmentation of long messages — `xrtWsConnBeginText` opens, `WriterWrite` goes chunk by chunk, `WriterFinish` concludes; the writer's header and compression state have stable sizes (content never changes the network object's layout); **compressed sending**: the compression writer of `BeginTextCompressed` (the send-side form of the permessage-deflate negotiated in Chapter 97 — when compression is on, **the same backpressure and ownership semantics apply**); **the ownership matrix**: copy/ref/take × text/binary × synchronous/Future, the complete combination — the tour's "nine send kinds" of line 3 are exactly this matrix. Send failures obey atomic ownership: unaccepted stays with the caller; after acceptance the connection releases.

## Introduction

Sending a 10 MB log snapshot: the copy path first copies 10 MB (memory ×2, then queuing); the ref path requires "one contiguous block of memory" (the snapshot already is one — a fit); but what if the snapshot is **generated streaming** (not fully accumulated yet)? The writer path: generate and `WriterWrite` as you go — each chunk goes straight into the frame stream, memory constant, fragmentation automatic (first-frame opcode, continuation, FIN — the send-side automation of Chapter 95's fragmentation rules). One step further: the snapshot is highly repetitive text, compression saves 90% — `BeginTextCompressed` opens a compression writer, the producer interface unchanged (still Write/Finish), compression happens inside the writer — **"streaming" and "compression" combine orthogonally**.

The writer's engineering details show their quality: "the header and compression state have stable sizes; hover, events, or dynamic content never change the network object's layout" — the network object's memory shape is predictable (Chapter 64's Worker economics extended to the object-layout layer); the failure path's atomicity (a write fails halfway — accepted chunks are released by the connection, unaccepted chunks are yours) lets error recovery proceed without guessing state.

## Concepts

### The writer lifecycle

```diagram state
idle -> writing: BeginText/BeginBinary/BeginTextCompressed (on the connection's owning Worker)
writing -> writing: WriterWrite x N (any chunking - protocol fragmentation is automatic)
writing -> done: WriterFinish (last chunk written + FIN + message concluded)
writing -> abandoned: WriterDestroy (the discard path for an unfinished message)
```

Three entrances correspond to message type and compression: `BeginText`/`BeginBinary` (plain), `BeginTextCompressed`/`BeginBinaryCompressed` (compressed — requires the connection to have negotiated permessage-deflate). **Chunk boundaries are unrelated to frame boundaries**: you Write at your business rhythm, the writer frames per protocol policy — big chunks split automatically, small chunks may coalesce; FIN appears only at Finish (exactly one conclusion per message).

### The compression writer: the same semantics

When compression is on, the writer carries a deflater internally (Chapter 97's `xwsdeflater` integrated into the connection): each chunk is compressed into the frame stream, RSV1 marks only the first frame, no-context-takeover resets the dictionary at message boundaries per the negotiation — **the caller-facing Write/Finish interface is identical to a plain writer**. "When compression is on, the same backpressure and ownership semantics hold" — AGAIN is still that AGAIN, release still that exactly-once — Chapter 97's compression-object behavior carried losslessly onto the connection's send path.

### The ownership matrix and the atomic rule

| Shape | copy | ref | take |
| --- | --- | --- | --- |
| At enqueue | copies immediately | records the reference, zero copy | takes over allocated memory |
| While queued | the connection holds the copy | the original block borrowed | the connection owns it exclusively |
| Done/failed | - | release exactly once | the connection releases |
| Failed unaccepted | caller loses nothing | caller keeps it | returned to the caller |

The nine synchronous sends (the tour) = three ownerships × {text, binary, ping/control}; the Future forms add a waiting dimension. **Selection rule**: short messages take copy (copy cost negligible); static/shared big blocks take ref (one allocation, many sends — the primitive of Chapter 112's broadcast); already-allocated handovers take take (saving the last copy).

### The writer and buffer chains

The take/buffer forms can hand an **XRT network buffer chain** (Chapter 65) directly to the connection — a generated chunk chain reaches the send queue without "flattening copies"; the writer's framing header assembles with the chain head in one Vec send (Chapter 68). The send path's zero-copy extends from "references" to "chains".

## Examples

### First complete program: chunked streaming send

The program below is from `examples/websocket/writer` — the writer template for multi-chunk messages:

```embed path="extlibs/xws/examples/websocket/writer/main.c" title="extlibs/xws/examples/websocket/writer/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/writer/main.c -lws2_32 -liphlpapi
WebSocket Writer example is ready
```

**What just happened.** (1) `xrtWsConnBeginText(连接)` (connection) opens a text writer; the loop calls `WriterWrite` per chunk, **the last chunk uses `WriterFinish`** (write + FIN in one) — the standard shape of the writer loop. (2) Mid-way failure uses `WriterDestroy` — the discard path for an unfinished message (the connection stays usable; only this message went unfinished). (3) The function signature takes a chunk array — this sample is a template shape (main verifies readiness); real calls happen in Worker callbacks (Chapter 110's discipline). **Why writer instead of accumulating then sending**: the chunks come from streaming generation (cursors/pipelines/dumps) — accumulating the whole message is the memory peak; the writer decouples "the generation rhythm" from "the send rhythm".

### Second complete program: the compressed send path

The second program is from `examples/websocket/writer_deflate` — an isomorphism check of the compression writer:

```embed path="extlibs/xws/examples/websocket/writer_deflate/main.c" title="extlibs/xws/examples/websocket/writer_deflate/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/writer_deflate/main.c -lws2_32 -liphlpapi
（压缩 writer 就绪自检通过后正常退出）
```

**What just happened.** (1) The only difference is the entrance: `xrtWsConnBeginTextCompressed` — afterwards the Write/Finish/Destroy loop is **verbatim identical** to a plain writer. "Compression is a property of the writer, not a rewrite of the send flow" — caller code gains compression with zero changes. (2) Precondition: the connection negotiated permessage-deflate (Chapter 97's negotiation layer, Chapter 113's routing configuration); opening a compression writer on an un-negotiated connection fails at the entrance. (3) Compression semantics are guaranteed by the writer: RSV1 on the first frame, dictionary reset per negotiation, decompression cap on the peer — **the sending side need not care about Chapter 97's compression-rule details**; once negotiation fixes behavior, behavior is fixed. With `writer_ref` (the reference-chunk writer variant — a streaming conclusion for static big blocks), the send path's three samples form the complete matrix.

## Contracts

- **Writer lifecycle**: Begin (three entrances: text/binary/compressed) → Write×N → Finish (FIN) or Destroy (discard) — one of the two is mandatory; called on the connection's owning Worker.
- **Chunk-frame decoupling**: the caller's chunking rhythm is unrelated to protocol framing; FIN exactly once; control frames may not be interleaved inside your own writer's message (the send side of Chapter 95's bypass rule — to send a Ping, first Finish or use the connection-level control entrance).
- **Compression writer**: the entrance is the only difference; the Write/Finish interface matches the plain writer; negotiation is the precondition; RSV1/dictionary/cap are guaranteed by the writer.
- **Stable sizes**: the writer's header and compression state are fixed — the network object layout never varies with content.
- **Ownership atomicity**: unaccepted stays with the caller; after acceptance the connection releases exactly once; take can hand over buffer chains (the chain head reaches zero-copy).
- **Three-state selection**: short = copy; static/shared = ref; allocated handover = take — nine synchronous combinations plus the Future family.
- **Unified backpressure**: writer writes are equally bound by watermarks — on AGAIN the chunk is unaccepted (retry verbatim).
- **Trimming**: `websocket_writer`/`websocket_deflater` independent — connections that skip the writer pay zero.

## Pitfalls

### Pitfall 1: a writer opened but never concluded (leak or half message)

Symptom: no new message can ever be sent on the connection — the previous writer still hangs (message unconcluded); or Destroy forgotten — the writer state occupies the connection.

Cause: a writer is an **exclusive send transaction** on the connection: after Begin you must choose Finish (sent) or Destroy (discarded) — there is no third state "leave it for now".

```c bad
pWriter = xrtWsConnBeginText(pConn);
write_some(pWriter);
/* Finish/Destroy forgotten: the connection's send side is locked up */
```

```c good
pWriter = xrtWsConnBeginText(pConn);
if ( pWriter == NULL ) { return XNET_RESULT_ERROR; }
for ( ... ) {
	if ( WriterWrite(...) != XNET_RESULT_OK ) {
		xrtWsWriterDestroy(pWriter);   /* the discard path is mandatory */
		return Result;
	}
}
return xrtWsWriterFinish(pWriter, LastChunk);
```

### Pitfall 2: a compression writer on an un-negotiated connection

Symptom: `BeginTextCompressed` returns NULL — the connection never negotiated permessage-deflate; the compression entrance fails outright.

Cause: compression is a **post-negotiation capability** (Chapter 97): the extension negotiation at connection establishment decides whether this connection has compression. The send side's "want to compress" cannot conjure the capability out of thin air.

```c bad
/* deflate never offered during negotiation */
pWriter = xrtWsConnBeginTextCompressed(pConn);  /* NULL: the capability does not exist */
```

```c good
/* the server/client configuration declares deflate (Chapter 113 routing/client configuration) */
/* after successful negotiation, choose the entrance by connection capability */
if ( conn_deflate_negotiated(pConn) ) {
	pWriter = xrtWsConnBeginTextCompressed(pConn);
} else {
	pWriter = xrtWsConnBeginText(pConn);   /* fall back to plain */
}
```

### Pitfall 3: a ref block touched after release

Symptom: occasional crashes after the release callback — the caller read/wrote that block again after release.

Cause: ref's contract is "after release, ownership has fully transferred" — the release callback is the last step. Saving across callbacks (Chapter 110's view discipline) combined with ref's release timing is easy to trip over on async paths.

```c bad
xrtWsConnBinaryRef(pConn, &Ref);
/* ... somewhere on an async path ... */
use(Buf);   /* release may already have happened: dangling */
```

```c good
/* never touch the block after release; to reuse it, return it to a pool inside the release callback
   (Chapter 22) and take it again only through the pool */
static void releaseToPool(ptr pCtx, cbytes pData, size_t iSize) {
	pool_return((pool*)pCtx, (ptr)pData);   /* return, not free */
}
```

## Exercises

### Basic: comparing the three entrances

Send the same multi-chunk content via BeginText/BeginBinary/BeginTextCompressed (on a deflate-negotiated connection); the peer verifies by type and compression. Acceptance criteria: all three messages decode to identical content on the peer; the compressed version's wire bytes are markedly fewer (measured).

### Advanced: a stream-generate → send pipeline

A cursor-like source (simulating 4 KB per millisecond) sends directly through a writer: Write follows the generation rhythm, Finish concludes at the source; measure peak memory. Acceptance criteria: the peak is independent of total volume (MB scale); a mid-flight AGAIN handled without losing chunks (retry semantics).

### Challenge: a three-state cost shoot-out

Send the same 1 MB message 100 times each in copy/ref/take: tally allocations, copied bytes, and time (Chapter 6 tools). Acceptance criteria: the three data sets match the contract's derivation (copy copies, ref zero-copy one allocation, take zero-copy zero allocation); write your scenario's selection conclusion.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Writer's three entrances | BeginText / BeginBinary / Begin*Compressed (negotiation as precondition) |
| Lifecycle | Write×N → Finish (FIN) or Destroy (discard) — one of the two is mandatory |
| Chunk-frame decoupling | caller rhythm ≠ protocol framing; FIN exactly once; control frames via the connection-level entrance |
| Compression isomorphism | the entrance is the difference; Write/Finish interface identical; RSV1/dictionary/cap guaranteed by the writer |
| Stable sizes | header and compression state fixed — the network object layout is predictable |
| Ownership matrix | copy (copies) / ref (release once) / take (handover/buffer chains); unaccepted stays with the caller |
| Three-state selection | short copy, static-shared ref, already-allocated take |
| Backpressure | the writer is equally bound by watermarks; an AGAIN chunk is unaccepted and retryable |
| Trimming | writer/deflater independent macros |
