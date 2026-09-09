---
num: 107
slug: xhttp-stream
title: Streaming Bodies: Upload, Download, and Async Files
volume: 卷十 扩展库：xhttp
type: practice
lead: xhttpbody's five sources and replay semantics, the Reader's lease-based consumption, the bounded producer stream, and async file bodies — body memory decoupled from content size.
api: xhttp-http_body, xhttp-http_body_stream, xhttp-http_body_file
---

## Orientation

HTTP's body is the battlefield of "content" versus "memory": a 2 GB upload, a streaming download, a response generated on demand — **body size should not equal memory footprint** is the starting point of the whole design. `xhttpbody` unifies five body sources: the fixed family (`Empty/Copy/Borrow/Take/Reference` — replayable, known length), the **bounded producer stream** (`HttpBodyStreamCreate` — task-thread production, budget-bounded, the foundation of SSE), **file bodies** (`http_body_file` — async file IO made into a body), custom sources (producer callbacks), and transformed bodies. On the consuming side `xhttpbodyreader` is a single-consumer streaming reader, and `xhttpbodychunk` is an **independent data lease** (`ChunkRelease` to return, releasable later than the Reader — the perfect partner of TCP reference sending). Chapter 103's server-side Body callbacks and Chapter 106's SSE producer end get their complete foundation story here.

## Introduction

Three body scenarios with memory-shape demands. Uploading 2 GB: the server must not buffer it all before processing — Chapter 103's streaming Body callbacks already solved the receiving side; but what about the **client's sending side** — reading 2 GB into memory before the POST? File bodies let it "read as you send". Streaming-generated downloads: response length unknown, content produced on demand (database cursors, live transcoding) — producer-stream bodies let it "generate as you send", capped by budget. Replay needs: Chapter 99's 307/308 redirects require a replayable body — the fixed family is replayable by nature; what about producer streams? The Body that `HttpBodyStreamCreate` produces is **consumed once** — scenarios that must resend on redirect use the fixed family or file bodies.

The lease is this chapter's most important new concept: the Chunk that `Next` yields **owns an independent data lease** — the data is not copied, but you must `ChunkRelease` to return it; a Chunk may outlive the Reader (sitting in a reference-send queue while the body object is already destroyed — managed by reference counting). This is the same idea as Chapter 95's WebSocket reference sending (released only when the write completes), landing on the body layer.

## Concepts

### Five sources and replayable semantics

| Source | Creation entry | Replayable | Typical scenario |
| --- | --- | --- | --- |
| Fixed family Empty/Copy/Borrow/Take/Reference | `xrtHttpBodyCopy` etc. | yes (known length = input bytes) | JSON/short bodies/307 resend |
| Bounded producer stream | `xrtHttpBodyStreamCreate(配置, &流)` (config, &stream) | no (consumed once) | SSE/streamed generation/progress output |
| File body | the `http_body_file` family | yes (re-openable) | large-file upload/static download |
| Custom source | producer callback | depends on implementation | database cursor/encryption pipeline |
| Transformed body | composition layer | depends on implementation | compressed responses |

Fixed-family details: `Copy` places the descriptor and the copy in **one compact allocation sized to the actual length** (no second allocation for the same body); `Borrow` does not extend the external memory's lifecycle (the original buffer must stay alive during the send); `Reference` must supply a release procedure (if you only need borrowing, use Borrow). `xrtHttpBodyView` borrows contiguous bytes of a fixed body — sources that cannot guarantee contiguity return `false` (file/streaming/transformed may be non-contiguous).

### Reader: single consumer and leases

```diagram flow
- Open: the Reader opens the body (single consumer - concurrent calls not allowed)
- Read: Next(cap, &Chunk) -> DATA (a leased chunk) / AGAIN (nothing yet, Wait on a Future) / EOF
- Lease: the Chunk is an independent data lease - ChunkRelease when done; releasable later than the Reader/Body
- Replay: the fixed family can open a Reader again and read from the start
```

Aliasing discipline (contract-literal): the output Chunk's descriptor/buffer/length slots **must not overlay** the underlying bytes of the Reader, the Body, or a fixed body — an aliasing error advances the Reader not at all and clears no output length, and a fixed Body stays immutable through replay and concurrent opens. This defense makes "one body object shared by multiple Calls" safe (Chapter 98's frozen-semantics body references).

### The bounded producer stream: budgets and finishes

`xhttpbodystreamconfig`'s `MaxBytes/MaxChunks` is the producer-side budget (reservations + queuing + active leases all covered) — SSE's AGAIN threshold comes exactly from here. Three finishes: destruction of the last stream reference = queued content then a **normal EOF**; `Close` idempotently pre-closes the input; `Fail` discards undelivered content and hands a stable Cause to the consuming side. The `Write/WriteRef/WriteTake` three write forms correspond to copy/reference/take-over — the same family as Chapter 67's five send gears.

### File bodies: async IO as a body

The `http_body_file` family wraps Chapter 71's completion-port file IO into a body: `xrtHttpBodyFileConfigInit` + file-body creation — **reads take the async file path** (driven by the Engine's Workers), no blocking, no whole-file reads; `xrtHttpBodyFileFuture`/`FileRangeFuture` provide the "wait for the body to be ready" Future form. Range variants naturally support resumable upload/download. File bodies are **replayable** (reopen the file, reread) — the safe combination of large files + redirect resends.

### The complete shapes of upload and download

- **Upload** (client): a file body + Chapter 98's builder `SetBody` — the client memory of a 2 GB upload is roughly one read block; the server receives via Chapter 103's streaming Body callbacks.
- **Download** (client): Chapter 98's `ResponseBodyLimit` manages the budget, streaming consumption (Body callbacks or the Reader) — memory decoupled from download size; Chapter 92's decoders chain onto the consumption path.
- **Generated responses** (server): a producer-stream body + `Respond` — a database cursor yields step by step; once the budget caps out, AGAIN makes the producer wait for the consuming side. Running all of this over a TLS service (Chapter 85), request-head parsing goes through Chapter 89's parser TLS binding (http1_tls) — no connection-level buffer across records; the streaming discipline runs end to end.

## Examples

### First complete program: producer-stream write-out and read-back

The program below is from `examples/http/body_stream` — the minimal loop of a bounded producer stream:

```embed path="extlibs/xhttp/examples/http/body_stream/main.c" title="extlibs/xhttp/examples/http/body_stream/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/body_stream/main.c -lws2_32 -liphlpapi
first chunk
second chunk
```

**What just happened.** (1) `Config.MaxBytes=1024/MaxChunks=16` sets the budget, then `HttpBodyStreamCreate` yields the **object pair** in one call: `xhttpbody` (handed to the request/response) and `xhttpbodystream` (held by the producer). (2) Two `Write` segments, then the stream destroyed — **last-reference destruction = normal EOF**: queued content delivers first, EOF follows. (3) The read-back loop (the same skeleton as Chapter 106's client): `Next(8, &Chunk)` segments with a small cap, DATA prints + `ChunkRelease`, AGAIN waits on `xrtHttpBodyReaderWait`'s Future, EOF finishes. (4) The two output lines mean "the byte stream the producer wrote arrives at the consumer whole" — **the body object is the decoupling point between production and consumption**: the two ends may live on different threads at different moments. In real deployments the producer is a task thread (SSE events/cursor yields) and the consumer is the HTTP transport.

### Second complete program: file-body upload

The second program is from `examples/http/body_file` — the upload shape for large files:

```embed path="extlibs/xhttp/examples/http/body_file/main.c" title="extlibs/xhttp/examples/http/body_file/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/body_file/main.c -lws2_32 -liphlpapi
（示例校验文件正文创建与读回后正常退出）
```

**What just happened.** (1) `xrtHttpBodyFileConfigInit` configures path and options, then the file body is created — **the length comes from file metadata, the content is read asynchronously on demand**: client memory independent of file size (contrast `Copy`'s whole read — 2 GB is 2 GB). (2) File bodies are replayable (reopen and reread) — Chapter 99's 307/308 redirect resend requirement is satisfied here by nature; a fixed streaming producer is not (consumed once). (3) The `xrtHttpBodyFileFuture` Future form brings "wait for the file to be ready/read done" into Chapter 57's waiting system. (4) Companions: `examples/http/body` (the fixed family's five sources), `body_compose` (composed bodies), `body_inflate/deflate` (transformed bodies — the sending-side compression matching Chapter 92's decoding), `server_body_async` (server-side async body responses) — the body family's full inspection face.

## Contracts

- **Five sources**: the fixed family (replayable, known length), the producer stream (consumed once, bounded), files (replayable, async reads), custom, transformed — one unified `xhttpbody` shape.
- **Fixed-family discipline**: Copy single allocation (descriptor + copy in one compact block); Borrow extends no external lifetime; Reference must carry a release procedure; View succeeds only for sources guaranteeing contiguity.
- **Producer stream**: MaxBytes/MaxChunks budget covers reservations + queuing + leases; EOF/Close/Fail three finishes; Write three forms (copy/reference/take-over).
- **Reader**: single consumer; DATA (lease)/AGAIN (Wait Future)/EOF three states; the Chunk is an independent lease (ChunkRelease; releasable later than Reader/Body — reference-send friendly).
- **Aliasing defense**: outputs never overlay the body's underlying bytes; aliasing errors advance nothing and break nothing — fixed Bodies stay immutable across replay and concurrent opens.
- **File bodies**: async reads, no blocking, no whole reads; replayable; Range variants; Future readiness form.
- **Replay matrix**: 307/308 resend → fixed family/files; generate once → producer stream.
- **Budget ownership**: the client's ResponseBodyLimit (Chapter 98) caps the representation body; the producer-stream budget caps the producer — two layers, each bounded.
- **Trimming**: `HTTP_BODY`/`HTTP_BODY_STREAM`/`HTTP_BODY_FILE` independent macros.

## Pitfalls

### Pitfall 1: the Borrow body's original buffer modified/freed during the send

Symptom: occasionally garbled content is sent — a stack buffer, or a heap buffer freed before the send, was paired with Borrow.

Cause: Borrow's contract is "no extension of the external memory's lifecycle" — the body only records the pointer; during the send (possibly async, possibly a resend) the original data must stay alive as-is. When unsure, use Copy or Reference.

```c bad
uint8 Buf[256];
fill(Buf);
xrtHttpRequestSetBody(pReq, xrtHttpBodyBorrow(view(Buf)));  /* stack buffer */
send_async(pReq);   /* the stack frame dies after return: the send reads garbage */
```

```c good
/* either Copy (small bodies) or Reference + release callback (your own heap buffer) */
xrtHttpRequestSetBody(pReq, xrtHttpBodyCopy(view(Buf)));
/* or xrtHttpBodyReference(view(HeapBuf), release_cb, ctx) - released automatically when the send completes */
```

### Pitfall 2: dropping data and producing on after the stream budget fills

Symptom: the streaming response drops segments — after AGAIN the producer skips this item and continues with the next ("to avoid blocking").

Cause: AGAIN is backpressure, not an error — **no input is retained**; the item in your hands is the only copy; skipping = data loss. Correct action: wait for `WaitWritable`, then send the same item.

```c bad
for ( each_item ) {
	if ( send(stream, item) == AGAIN ) {
		continue;   /* dropped - the client misses this segment */
	}
}
```

```c good
for ( each_item ) {
	while ( send(stream, item) == AGAIN ) {
		wait_writable(stream);   /* the waiting template from Chapter 106, Pitfall 2 */
	}
}
```

### Pitfall 3: forgetting to Release the Reader's Chunk (or holding it like a view)

Symptom: the lease count in allocation stats only grows — the budget exhausts and everything becomes AGAIN.

Cause: a Chunk is a lease, not a view — it holds producer budget; Release is the return action. To hold long-term, copy (except reference sending — that is precisely the lease's design scenario: released when the send completes).

```c bad
while ( next(&Reader, &Chunk) == DATA ) {
	queue_push(g_Q, Chunk);   /* lease not returned: budget exhausted */
}
```

```c good
while ( next(&Reader, &Chunk) == DATA ) {
	if ( want_keep(Chunk) ) {
		queue_push(g_Q, copy_chunk(Chunk));  /* keep a copy */
	}
	xrtHttpBodyChunkRelease(&Chunk);          /* return the lease */
}
```

## Exercises

### Basic: comparing the five sources

Create and send bodies from the same text with Copy/Borrow/Take/Reference/producer stream respectively (against a local service); compare allocation stats and replayability (read twice). Acceptance criteria: allocation counts match the contract; the fixed family's second read succeeds, the producer stream's second open fails (or behaves per contract).

### Advanced: a large-file upload pipeline

Client file-body POST of a 2 GB file (generate a temp file) → the server's streaming Body callbacks receive and write to disk → compare hashes. Record both ends' memory peaks throughout. Acceptance criteria: both peaks independent of file size (MB scale); hashes match; with mid-flight rate limiting (simulating a slow disk) the pipeline stays stable.

### Challenge: a transformed body (an encryption pipeline)

A custom producer body: read file chunks → AES-GCM encrypt each chunk (Chapter 74) → encoding it into the body — forming an "encrypted file download" service. The client consumes by reverse decryption with Chapter 92's decoding approach, turning ciphertext back into the original. Acceptance criteria: the round trip restores the original; the key lives only in both sides' memory (SecureZero at the end — Chapter 76 discipline); memory peak constant.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Five sources | fixed family (replayable) / producer stream (once) / file (async, replayable) / custom / transformed |
| Fixed family | Copy single allocation; Borrow extends no lifetime; Reference carries release; View only for contiguous sources |
| Producer stream | object pair (Body + Stream); MaxBytes/MaxChunks budget; EOF/Close/Fail three finishes |
| Reader | single consumer; DATA (lease)/AGAIN (Wait)/EOF; ChunkRelease returns |
| Lease | the Chunk is an independent data lease, releasable later than the Reader — reference sending's design scenario |
| Aliasing defense | outputs never overlay the body's underlying bytes; fixed Bodies immutable across replay/concurrent opens |
| File bodies | async reads, no whole reads; replayable; Range variants; Future readiness |
| Replay matrix | redirect resend → fixed/file; generate once → producer stream |
| Two budget layers | client ResponseBodyLimit (representation) + producer-stream budget (producer side) |
| Trimming | BODY/BODY_STREAM/BODY_FILE independent macros |
