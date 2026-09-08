---
num: 71
slug: net-misc
title: Framing and Async Files: The Volume 7 Finale
volume: 卷七 网络 · 卷七收官
type: practice
lead: Line framing and length-prefixed framing turn the byte stream back into messages; native async files hang the disk into the same Engine — Volume 7's two closing tools.
api: net_frame, net_file, net
---

## Orientation

Volume 7's through-line is "get data flowing": addresses (62), ports (63), buffers (64), DNS (65), TCP (66–67), UDP (68), proxies (69), interfaces (70). The closing chapter adds two **assembly-grade tools**: they introduce no new connection objects but glue existing parts into complete solutions. **The frame parsers** answer "how do I turn TCP's boundary-less byte stream back into messages" — Chapter 64's `xnetbuf` is their input, Chapter 66's receive callbacks are their drive; **native async file I/O** answers "how does a file server hang disk reads and writes into the same event loop as the network" — Chapter 63's Worker-ownership rules pay off again here. Both are small zero-engine-dependency modules, yet they respectively support text protocols (Redis/SMTP style), private binary protocols (RPC/game servers), and proxy caches and static file services — several large classes of real workloads.

## Introduction

Two scenarios, one commonality. Scenario one: you implement a text protocol over TCP; the client sends `PING\r\n`, the server replies `PONG\r\n`. Sounds simple, but TCP does not promise "one send matches one recv" — three sends may arrive glued as one (sticky packets), and one send may arrive split in two (partial packets). What if `PING\r` arrives first? Hand-rolling it is frantic index arithmetic. Scenario two: you write a proxy cache or file service; the network's reads and writes advance asynchronously on the Engine, but reading a file from disk is a blocking call — one Worker reads disk and hundreds of connections on the same Worker all stall.

The two scenarios share one thing: **the boundary problem**. The former lacks message boundaries — the frame parsers supply them; the latter lacks "a unified event boundary for disk and network" — native async files supply it — file operations submit directly to the Worker's completion port, Windows going through IOCP's `ReadFile`/`WriteFile`, Linux through io_uring's `READV`/`WRITEV`, dispatched from the same pool as socket events, so a Worker never blocks on disk.

## Concepts

### Three-state parsing: the incremental framers' shared rhythm

The two framers (line-delimited, length-prefixed) share one three-state return model:

```diagram state
MORE -> READY: input sufficient, one complete frame parsed
MORE -> MORE: input insufficient, prefix retained awaiting append
READY -> MORE: after consuming the current frame, parse the next
any -> ERROR: invalid config / broken state / frame over limit
```

`XNET_FRAME_MORE` is a **normal incremental control result** and sets no error — it is precisely "sticky/partial packets absorbed by the state machine". On `XNET_FRAME_READY`, the output `xnetframe` describes one complete frame at the head of the current input: `PayloadOffset`/`PayloadSize` locate the payload, `FrameSize` is the whole frame's byte count, and `Declared` preserves the protocol length field's raw value. `XNET_FRAME_ERROR` carries a structured error (four kinds: `FRAME_CONFIG`/`FRAME_STATE`/`FRAME_LIMIT`/`FRAME_LENGTH`), usually meaning the protocol stream is no longer trustworthy — close the connection or drop the input and reset the framer.

### Line framing: incremental search with arbitrary delimiters

`xnetlineconfig` has three elements: `Delimiter` (a **borrowing view** whose source must live as long as the framer is used; no length limit — `\n`, `\r\n`, even multi-byte sentinels work), `MaxPayload` (default 8192; `SIZE_MAX` is explicitly unbounded), and `IncludeDelimiter` (whether to keep the delimiter; removed by default). The default configuration is LF-delimited.

The framer internally keeps the block pointer and in-block offset (`xnetlineframer`), so even when input is scattered across many single-byte reference blocks, locating the old offset never re-walks the chain head — this is the watershed from "rescanning from the start every time" and the correct way to consume Chapter 64's reference-block appends. Delimiters crossing buffer blocks, self-overlapping (delimiter `\r\n` meeting input `\r\r\n`), and landing exactly on the payload limit are all supported.

One key contract: after `Next` returns `MORE`, the caller **may only keep the original input prefix and append at the tail of the same `xnetbuf`**; before consuming, replacing, switching buffers, or reordering input, you must call `xrtNetLineReset` (keeps the config, discards incremental progress). `Reset`'s other use is reusing the framer when the same connection "restarts the conversation", saving one initialization.

### Length-prefixed framing: one formula fits all

The most common framing for private binary protocols: a 1–8 byte length field in the header + payload. The total frame size follows one formula:

```text
FrameSize = LengthOffset + LengthSize + Declared + Adjustment
```

- `LengthOffset`/`LengthSize`: the length field's position and width within the frame (1..8 bytes); `Order` fixes endianness explicitly, independent of host byte order.
- `Adjustment`: a signed adjustment. If the protocol's length field already includes the header, subtract it back with a negative value.
- `Strip`: the payload start — keep the whole header, remove only the length field, or remove the entire application header; one field, three choices.
- `MaxFrame`: the hard total-frame cap (default 1 MiB) — malicious or corrupt declared lengths are stopped here.

The default configuration is "4-byte big-endian payload length, length field removed, 1 MiB cap" — exactly most RPCs' format. Every boundary — field end, unsigned declared value, signed adjustment, `size_t` conversion, strip and cap — is validated before the payload is accessed; there is no "trust the length first, read out of bounds later" path.

### Two ways to take a frame: copy and zero-copy

After `Next` produces a frame description, the payload can be taken two ways. The **convenience path** `xrtNetFrameCopy`: copies the payload bytes (up to output capacity) into the caller's buffer — small messages in one line. The **zero-copy path**: use Chapter 64's `xrtNetBufSpans`/`xrtNetBufPeek` directly for payload views — big frames need no copy. After taking, `xrtNetFrameConsume` removes exactly the whole frame from the input head (it first verifies the frame range is still wholly at the head; if the input was altered, it refuses with `FRAME_STATE`). Note the frame description **borrows the current input**: after the input prefix is consumed, replaced, or reordered, old frame descriptions must not be used.

### Native async files: the disk on the completion port

The `net_file` module submits positioned reads and writes of ordinary files to the Network Engine's completion port, for file services, proxy caches, and custom storage pipelines — no Futures created, no TaskPool occupied, no caller-payload copies. Five key points:

- **Open**: `xrtNetFileOpen` automatically adds the `XFILE_ASYNC` flag; options are the same set as Chapter 44's `xrtFileOpen` (read-only by default; creating a file needs explicit `XFILE_CREATE|XFILE_WRITE`).
- **Worker ownership**: `Read`/`Write` may be submitted only on the **owning Worker thread** — after a file's first submission, it is permanently pinned to the same Worker; a cross-thread submission gets `XERR_STATE`. For the main thread to initiate, use `xrtNetEnginePost` to deliver the submission action onto a Worker (the standard posture of this chapter's examples).
- **Absolute offsets**: reads and writes carry `iOffset` and don't depend on the file cursor — multiple concurrent reads of the same file never interfere.
- **The sole terminal state**: every operation returns a non-zero identifier, and the `xnetcompletion` callback receives **exactly one** final-state event on the Worker (described by the `xnetportevent`'s `Result`/`Bytes`/`Id`). File, buffers, and the completion must be kept alive until the terminal state arrives. `xrtNetFileCancel` only requests cancellation; the original operation still concludes through the completion — the terminal state may be `XNET_RESULT_CANCELLED`, or the operation may already have finished and be `OK`.
- **Capability bit**: when the Worker's port backend has no native file I/O capability (e.g. the SELECT fallback backend), submission gets `XERR_UNSUPPORTED` — reported explicitly, never degraded to blocking the Worker.

After all operations conclude, close the file with `xrtClose` — the handle type is Chapter 44's `xfile`; the whole file family shares this one close entry, with no separate close function for async files.

## Examples

### First complete program: CRLF line framing and partial-packet absorption

The following program comes from `examples/network/frame_line/main.c`: the input deliberately arrives split as `first\r` and `\nsecond\r\n` (a partial packet straddling the delimiter); the framer restores each frame and demonstrates Reset reuse:

```embed path="examples/network/frame_line/main.c" title="examples/network/frame_line/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/frame_line/main.c -lws2_32 -liphlpapi
first
second
reset=ok
```

**What just happened.** (1) The delimiter is set to `\r\n` (a borrowed view pointing straight at the literal); the first block `first\r` fed in yields `MORE` — only half the delimiter has arrived, and the framer records the pending prefix in its incremental cursor. (2) The second block `\nsecond\r\n` is appended to the tail of the **same** `xnetbuf` (the MORE contract used correctly), and the `Next` loop resolves `first` and `second` in turn as two complete frames. (3) Each frame walks the three steps "Copy into a stack buffer → print → Consume precisely", with `sizeof(sLine) - 1u` leaving room for the trailing zero. (4) At the end, `xrtNetLineReset` keeps the config and discards progress — a new session reuses the framer. This is the framing foundation of Redis/SMTP-class text protocols: **sticky and partial packets are absorbed by the parser's state machine, and business code sees only complete messages one by one**.

### Second complete program: four-byte length-prefixed deframing

The second program comes from `examples/network/frame_length/main.c`, the standard sample of a private binary protocol — `00 00 00 05` + `hello`:

```embed path="examples/network/frame_length/main.c" title="examples/network/frame_length/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/frame_length/main.c -lws2_32 -liphlpapi
hello
```

**What just happened.** The default configuration (`xrtNetLengthConfigInit`) is exactly four-byte big-endian length, length field removed, 1 MiB cap — hitting this protocol's format directly. `LengthNext` resolves the frame description in one step, and `FrameCopy` copies the payload into a 5-byte buffer (returning 5, exactly the payload length, one byte over for the trailing zero). Compare with the line-framing example's return model: `LengthNext` returns `MORE` equally for "length field incomplete" or "declared length not fully arrived", and the caller's append-retry loop is fully isomorphic with line framing — **switch frame formats without changing the driving rhythm**.

### Third complete program: file read/write and cancellation on the completion port

The third program comes from `examples/network/file_tour/main.c`, walking "submit — terminal state — cancel" completely: absolute-offset write, read-back verification, and a unique terminal state still arriving after cancelling an in-flight read:

```embed path="examples/network/file_tour/main.c" title="examples/network/file_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/file_tour/main.c -lws2_32 -liphlpapi
file: open(async) + write completed at offset 0 ok
file: read-back verified + reopen append ok
file: cancel in-flight read -> terminal event ok
```

**What just happened.** (1) The file options explicitly use `XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE` — read-only by default; creating a file must be declared, and `xrtNetFileOpen` automatically adds the async flag on top. (2) Submission takes the **standard two hops**: the main thread `xrtNetEnginePost`s `exampleWriteTask` onto Worker 0, and the task function calls `xrtNetFileWrite` **on the Worker** and registers the completion — this is the landed form of the "submit only on the owning Worker" contract. The terminal callback `exampleDone` records `Result`/`Bytes`/`Id`, and the main thread spins waiting for `bDone` and cross-checks three things: result OK, byte count matching, and the event `Id` equal to the submission's return. (3) The read-back uses the same posture, with `memcmp` verifying contents. (4) The cancel section first submits a big 4096-byte read, spins briefly to confirm it hasn't finished, then `xrtNetFileCancel` — the terminal state **always arrives** and the result can only be `OK` (the read happened to finish) or `CANCELLED`; note that reading 4096 bytes from an 8-byte file completes immediately at EOF, so the example waits first before deciding whether to cancel — this "cancellation may lose the race condition to completion" handling is exactly what real code looks like. (5) The wind-down `xrtClose(File)` → stop the Engine → delete the temp file, in an order that must not be swapped.

## Contracts

- **Framer lifecycle**: framers are value objects on the caller's stack, no shared state; different instances run concurrently on any threads, and one instance is safe under sequential calls; the config (delimiter view) must live until the framer is discarded.
- **The MORE contract**: after `MORE`, only keeping the original input prefix and appending to the same `xnetbuf` is allowed; before consuming/replacing/switching buffers/reordering, `Reset` is mandatory; `MORE` sets no error.
- **Frames borrow input**: an `xnetframe`'s offsets are relative to the current input head; old frames die once the prefix changes; `Consume` verifies the range and then consumes precisely.
- **Frame limits**: a line payload over `MaxPayload` reports `FRAME_LIMIT`; a length declaration overflowing/over-limit/an illegal adjustment reports `FRAME_LENGTH`; a config that cannot possibly be legal reports `FRAME_CONFIG`.
- **File ownership**: `Read`/`Write`/`Cancel` are valid only on the owning Worker thread (`XERR_STATE`); after a file's first submission it stays pinned to one Worker; the main thread hands over via `EnginePost`.
- **File terminal states**: operations return a non-zero `Id`; the completion receives exactly one terminal state (`Result`/`Bytes`/`Id`); file, buffers, and completion are kept until the terminal state; `Cancel` only requests — the terminal is `OK` or `CANCELLED`.
- **File capability**: a backend without native file I/O (SELECT fallback) reports `XERR_UNSUPPORTED`, never degrading to blocking; Windows IOCP, Linux io_uring.
- **Close**: `xrtClose` after all operations conclude; offset + length beyond the expressible range reports `XNET_ERROR_PORT_SUBMIT`.

## Pitfalls

### Pitfall 1: touching the input prefix after `MORE` and still using the framer

Symptoms: occasional `XNET_FRAME_ERROR` + `FRAME_STATE`, or garbled frame contents — usually in code that "parses halfway and goes off to process other bytes".

Cause: after `MORE`, the framer's incremental cursor is anchored to the **original input prefix**. Consuming the head, replacing the buffer, switching to another `xnetbuf`, or even moving the prefix elsewhere invalidates the anchor — the contract mandates `Reset` before any of these.

```c bad
if ( xrtNetLineNext(&Framer, &Input, &Frame) ==
	XNET_FRAME_MORE ) {
	xrtNetBufConsume(&Input, 8);   /* touched the prefix: the incremental cursor dangles */
	xrtNetLineNext(&Framer, &Input, &Frame); /* FRAME_STATE */
}
```

```c good
if ( xrtNetLineNext(&Framer, &Input, &Frame) ==
	XNET_FRAME_MORE ) {
	/* the only legal action: keep the prefix, append new data at the tail */
	xrtNetBufAppend(&Input, pChunk, iSize);
}
/* to truly discard the current input (e.g. protocol reset): Reset first, then feed the new stream */
(void)xrtNetLineReset(&Framer);
```

### Pitfall 2: submitting file operations directly on the main thread

Symptoms: the submission returns 0 with `XERR_STATE` in the thread error slot — the file opened just fine, yet every submission fails.

Cause: `xrtNetFileRead`/`Write`/`Cancel` are Worker-exclusive interfaces, callable only on the owning Worker thread; the main thread is not a Worker. This is not a defect but design — completion-port operations must be submitted on the pump thread, and the module uses the error code to state the boundary clearly.

```c bad
/* main thread: not on the owning Worker, XERR_STATE, returns 0 */
iId = xrtNetFileRead(pWorker, File, 0, Buffer,
	sizeof(Buffer), &Completion);
```

```c good
/* the standard two hops: Post delivers the task to the Worker, and the task function submits */
static void submitRead(xnetworker* pWorker, ptr pUserData) {
	exampletask* pTask = (exampletask*)pUserData;
	pTask->iId = xrtNetFileRead(pWorker, pTask->File,
		pTask->iOffset, (void*)pTask->pData, pTask->iSize,
		&pTask->pIo->Completion);
}
xrtNetEnginePost(pEngine, 0u, submitRead, (ptr)&Task);
```

### Pitfall 3: running file I/O on the SELECT fallback backend

Symptoms: the same code works on Windows (IOCP), but after migrating to an embedded target where only SELECT is available, every file submission returns 0.

Cause: the SELECT backend has no native async file capability, and the module **explicitly refuses** per the capability bit (`XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT`) rather than quietly degrading into blocking the Worker — otherwise one disk read would jam the whole event loop, exactly the failure mode the design exists to eliminate.

```c bad
iId = xrtNetFileRead(pWorker, File, 0, Buffer,
	sizeof(Buffer), &Completion);
if ( iId == 0u ) {
	/* assuming a transient error, retry in a dead loop: forever UNSUPPORTED */
}
```

```c good
iId = xrtNetFileRead(pWorker, File, 0, Buffer,
	sizeof(Buffer), &Completion);
if ( iId == 0u && xrtErrorIs(xrtGetError(),
	XERR_UNSUPPORTED) ) {
	/* backend has no native file capability: switch to Chapter 47's
	   async file module or a dedicated disk thread, not retries */
}
```

## Exercises

### Basic: change the line framer's delimiter

Change the line-framing example to `IncludeDelimiter = true` with the delimiter still `\r\n`: what ending should the two output lines carry? Then switch to the single byte `\n` and split the second input block into `\nsec` and `ond\n`, verifying `second` still resolves. Acceptance: both changes' outputs match hand-derived results and the program returns 0.

### Advanced: length frames with a message header

Design the protocol "1-byte type + 2-byte little-endian length + payload": configure `LengthOffset = 1`, `LengthSize = 2`, `Order = XNET_FRAME_LITTLE_ENDIAN`, `Strip = 3`, construct two glued messages fed in at once, resolve them in a loop, and print separately by type byte. Hint: the `FrameSize` formula here is 1 + 2 + Declared; two frames just need consecutive `Next`/`Consume`.

### Challenge: a minimal static file server skeleton

With Chapter 66's event-face TCP service + this chapter's length-prefixed framer + async files, implement the skeleton "client sends an 8-byte file-offset request → server reads a 4 KiB block back": request framing via `xrtNetLengthNext` (incremental parsing of the `Recv` buffer inside the Read callback), disk reads via `xrtNetFileRead` (submitted inside the Worker), responses framed as "4-byte length + data" and sent via `xrtNetStreamSend`. Acceptance: two concurrent clients alternating requests never cross streams; on the SELECT backend, report the missing capability gracefully and exit; Engine destruction leaves no leaks (cross-check with Chapter 6's stats).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three states | `READY` (complete frame) / `MORE` (await append, no error) / `ERROR` (four frame-error kinds, stream untrusted) |
| Line framing | arbitrary-length delimiter (borrowed); MaxPayload default 8192, SIZE_MAX unbounded; IncludeDelimiter toggles retention |
| MORE contract | only keep the prefix + append at the tail of the same `xnetbuf`; `Reset` before touching the prefix (keep config, drop progress) |
| Length-frame formula | `FrameSize = LengthOffset + LengthSize + Declared + Adjustment`; Adjustment may be negative |
| Length-frame defaults | 4-byte big-endian, length field removed, 1 MiB cap; width 1..8, endianness chosen, Strip sets the payload start |
| Taking payloads | small messages `FrameCopy` (truncated by capacity); big frames `BufSpans`/`BufPeek` zero-copy; `FrameConsume` consumes precisely |
| Async files | `NetFileOpen` auto XFILE_ASYNC; absolute-offset reads/writes; no Futures, no TaskPool, zero payload copies |
| Worker ownership | Read/Write/Cancel on the owning Worker only (cross-thread XERR_STATE); the main thread submits via `EnginePost` two hops |
| Terminal states | non-zero Id ↔ event Id correspondence; completion exactly once; Cancel only requests, terminal is OK or CANCELLED |
| Backend capability | IOCP (Win) / io_uring (Linux) native; SELECT explicitly `UNSUPPORTED`, no blocking degradation |
| Close | `xrtClose(File)` after all terminal states; framers/buffers are stack values or `BufClear`, no separate destruction |
