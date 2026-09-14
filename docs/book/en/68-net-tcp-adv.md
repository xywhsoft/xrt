---
num: 68
slug: net-tcp-adv
title: TCP (Part 2): Backpressure, References, and the Server Side
volume: 卷七 网络
type: practice
lead: The four send families and the write budget, read buffers and Worker constraints, flow-control pause/resume, dual-face Accept, and the shared-port server shape.
api: tcp, tcp_server, net
---

## Orientation

TCP's second half covers three advanced blocks: **the four send families** (Vec gather / Ref zero-copy + release callback / Take takeover / File kernel file sending) with the **write budget** (WriteLimit cap / Writable headroom / Pending in-flight — the send-backpressure trio); **reads and Worker constraints** (Buffer/Read/Consume inside a Worker only — the consumption face of direct-move buffers; crossing threads goes through Post); and **the server shape** (xnetserver — pull Accept (dequeuing connections), Future Accept and blocking Accept coexisting, multi-listen and shared ports). The gold standard's four states and echo (Chapter 67) are the basic form — this chapter is its full bloom.

## Introduction

Three advanced scenarios. Scenario one: a file server must send a 1GB file — reading the whole file into memory then Sending is a disaster (memory explosion); `xrtNetStreamSendFile` (kernel file sending + bounded ranges — the sendfile landing point previewed in Chapter 46) passes through no user-space buffer. Scenario two: a proxy must forward an upstream span chain downstream — copying before sending is waste; `SendRef` hands it over by reference directly (zero-copy + release callback). Scenario three: a server must cap a single connection's send backlog (a malicious client that never reads — buffers inflate without bound); the **write budget** (WriteLimit cap, Pending in-flight, Writable headroom) is backpressure's send-side enforcement.

The common theme is **ownership and budget on the send face** — Chapter 5's language in full on the IO exit: Send copies, SendVec gathers-and-copies, SendRef borrows + callback, SendTake takes over, SendFile delegates to the kernel. Five tiers chosen by "where the data comes from and who frees it" — corresponding one-to-one with Chapter 65's four buffer append kinds (those chain in, these chain out).

## Concepts

### The four send families and file sending

| Entry | Ownership | Fits |
| --- | --- | --- |
| Send | Copy | Small-data safe path (gold-standard chapter) |
| SendVec | Gather-copy | Multiple buffers sent at once (Chapter 21's batching) |
| SendRef / SendRefs | Reference + release callback | Direct span-chain handoff (zero-copy — kin of the gold standard's SendBuffer) |
| SendTake | Takeover | An xrtMalloc block (engine frees when sent) |
| SendFile | Kernel delegation | File-range sending (sendfile/mmap backend) |

**SendRef's release callback**: fired exactly once when the data is sent (or the stream destroyed) — the same discipline as Chapter 65's AppendRef; SendRefs is the multi-span variant (a whole span chain in one send). **SendFile's range**: offset + length bounded — chunked file sending controls memory (a kernel buffer per chunk, not the whole file).

### The write budget: the send-backpressure trio

```diagram flow
- WriteLimit(N): per-connection send budget cap (in-flight + queued <= N)
- Pending(): current in-flight/queued bytes — the watermark gauge
- Writable(): headroom = Limit - Pending — the safe amount to submit
- Send over budget: fails or blocks (per call form) — where backpressure bites
- Flow control to match: Pause/Resume — after Pause the engine holds data (memory backstop)
```

**The complete backpressure chain**: the peer stops reading → the TCP window shrinks → engine buffers back up → Pending approaches Limit → new Sends are refused → **the upstream notices** (the handler sees send failure / must wait) — a slow consumer cannot drag down service memory. Pause/Resume is the manual gear (Pausable semantics: after Pause the engine retains data without advancing — the upstream may decide to drop or disconnect). Chapter 21's queue backpressure, connection edition.

### Reads and Worker constraints

**In-Worker APIs** (Buffer/Read/Consume etc. — must be called on the Stream's owning Worker thread): the consumption face of direct-move buffers (Buffer fetches the chain, Consume advances — Chapter 65's operations used directly on engine data); **the cross-thread entry** (`xrtNetPost` delivers a task to the Stream's Worker — the engine edition of Chapter 56's scheduler Post); **the wait family** (Wait/WaitAvailable blocking — rendezvous from non-Worker threads; WaitAvailableAsync Future-style — for orchestration). **Why the constraint**: the precondition of lock-free direct-move buffer operations is "one thread" — the Worker constraint is the engine edition of Chapter 56's scheduler-binding discipline; crossing threads transfers execution via Post rather than adding locks — the zero-lock discipline throughout.

### The server: dual-face Accept and shared ports

`xnetserver` (TCP Server): **pull Accept** (dequeue connections — take one as one arrives; upper-layer rate limiting/dispatch controls directly — the tcp_server example's shape); **Future Accept** (event-loop side — orchestration-friendly); **blocking Accept** (non-Worker threads — a management thread waiting for connections). **Coexistence of both faces** is the real migration-era shape (the tcp_server_sync example: Future and blocking takes on the same Server, side by side). **Multi-listen and shared ports**: one Server, multiple endpoints (v4 + v6 dual-stack); SO_REUSEPORT-style shared ports (server_tour's `shared-port=1` assertion — platform support for multi-process shared listening). **Accept statistics** (accepted/queued — service observation fields).

### Shutdown and storms: the server's wind-down

Listener Close (stop admissions — drain in-flight accepts), the connection cap (ServerConfig's queue depth — admission backpressure: when full, refuse new connections rather than queueing forever), shutdown order (Listener → Streams → Engine — Chapter 67's order, server edition). **Connection storms' handling point**: Accept queue-depth limits + fast refusal (low error cost) — rather than accepting then disconnecting (resources already allocated).

## Examples

### Complete program: the Stream full-family tour

From the repository example `examples/network/tcp_stream_tour/main.c` — the complete acceptance of the four send families + budget + flow control + half-close:

```embed path="examples/network/tcp_stream_tour/main.c" title="examples/network/tcp_stream_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/tcp_stream_tour/main.c -lws2_32 -liphlpapi
stream-tour: config write-limit>=64k writable>=0
sends: vec+ref+refs+take+file = 53 bytes verified
worker-read: buffer=8 read="buf-" consume=4 socket+setevents+setdata=ok
waits: read=1 available=1 available-async=1 write=1
flow: pause=held resume=delivered
shutdown: peer-eof=1 stats(sent=65) error=(none)
```

**What just happened.** (1) Configuration tier: WriteLimit at least 64k, Writable starting at 0 — the write-budget trio's starting point. (2) **The four send families in joint performance**: Vec+Ref+Refs+Take+File each send one stretch, 53 bytes verified in full — all five ownership tiers walked on one stream. (3) The worker-read line: the in-Worker read trio (Buffer = 8 bytes available, Read fetches "buf-", Consume advances 4) + Socket/SetEvents/SetData — hands-on with the Worker-constraint APIs. (4) The waits line: four wait forms (blocking read / Available snapshot / Async Future / write wait), one each. (5) The flow line: after Pause, data held (engine retains, doesn't advance); after Resume, delivered — the two beats of flow control. (6) The shutdown line: the peer reads EOF, stats' sent count (65 = 53 + 12 assorted extra bytes), error=(none) — a clean final state. This tour is the Stream data plane's full-operation acceptance — a checklist for production code.

### Complete program: dual-face Accept, side by side

From `examples/network/tcp_server_sync/main.c` — Future and blocking takes coexisting:

```embed path="examples/network/tcp_server_sync/main.c" title="examples/network/tcp_server_sync/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/tcp_server_sync/main.c -lws2_32 -liphlpapi
accepted Future and synchronous connections on port NNNNN
```

**What just happened.** (1) A Server started on a dynamic loopback port (port 0 + Local retrieval — Chapter 67's posture). (2) **Future Accept**: the event-loop side takes the first connection — orchestration-friendly (later operations compose). (3) **Blocking Accept**: the management-thread side takes the second — the rendezvous shape for ordinary threads. (4) Both postures on **the same Server at once** — proof of the real migration-era shape (moving from the blocking to the event model needs no one-step switchover). The server_tour example adds multi-endpoint/shared-port/Accept statistics — the server's full interface.

## Contracts

- **Five send tiers**: Send copy / Vec gather / Ref reference + callback exactly once / Take takeover / File kernel delegation — chosen by source (corresponding to Chapter 65's four chain-in kinds).
- **Write budget**: WriteLimit cap + Pending watermark + Writable headroom — send-side enforcement against slow consumers.
- **Worker constraint**: Buffer/Read/Consume inside a Worker only; cross-thread via Post — the zero-lock discipline.
- **Flow control**: Pause/Resume as the manual gear — pause retains data; the upstream decides to drop or disconnect.
- **Dual-face Accept**: pull/Future/blocking coexisting — gradual migration.
- **Admission backpressure**: Accept queue-depth limit + fast refusal — storms stopped at the door.

## Pitfalls

### Pitfall 1: freeing data early after SendRef

Symptoms: the receiver occasionally gets dirty data — the send callback hasn't arrived and the data source is already freed; same family as Chapter 65's Pitfall 1.

Cause: SendRef is reference sending — a data freeze window before the release callback (the engine is reading it).

```c bad
char* Block = xrtMalloc(1024);
Fill(Block);
xrtNetStreamSendRef(Stream, Block, 1024, releaseCb, Ctx);
xrtFree(Block);   /* freed before the callback — the engine is still reading */
```

```c good
xrtNetStreamSendTake(Stream, TakeBlock, 1024);   /* takeover — the engine frees when sent */
/* or SendRef + callback: the callback fires exactly when the data is no longer referenced — don't touch it until then */
```

### Pitfall 2: budget-less sending blowing up memory

Symptoms: service memory grows with slow clients — one client never reads, engine buffers back up without bound; an OOM crash at the late-night traffic peak.

Cause: no WriteLimit set — sending is uncapped, and the backlog from the shrinking TCP window all piles into service memory.

```c bad
xnetstreamconfig Config;
xrtNetStreamConfigInit(&Config);   /* no budget limit by default */
xrtNetStreamSend(Stream, HugeData, HugeSize);   /* unbounded backlog */
```

```c good
xnetstreamconfig Config;
xrtNetStreamConfigInit(&Config);
Config.WriteLimit = 1u << 20;   /* 1MB budget */
/* check headroom before sending */
if ( xrtNetStreamWritable(Stream) < Need ) {
	WaitOrPause(Stream);   /* backpressure bites — wait or pause, don't pile on */
}
```

## Exercises

### Basic: five-tier comparison

Send the same data through each of the five tiers (File tier via a temp file) — verify in full at the receiver; print and confirm each tier's release timing (callback/takeover).

### Advanced: a backpressure experiment

A slow client (read 1 byte, sleep 100ms) + the server sending heavily: with WriteLimit versus unlimited — watch the service memory curve (Chapter 6's stats) and the Pending watermark; verify the over-budget handling path.

### Challenge: a file-distribution service

A static file service: multiple clients concurrently downloading the same file — SendFile chunked by range (chunk budget) + per-client WriteLimit + a global admission cap. Acceptance: serving a 1GB file keeps peak service memory <100MB (proof of chunking + budget); slow clients don't affect the others; shutdown winds down fully (the trio protocol); per-client downloads verify identical Hash64.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Five send tiers | Send copy / Vec gather / Ref reference + callback / Take takeover / File kernel |
| Write budget | WriteLimit cap + Pending watermark + Writable headroom — backpressure against slow clients |
| Worker constraint | Buffer/Read/Consume inside a Worker only — cross-thread via Post delivery |
| Flow control | Pause/Resume — the manual gear that retains data |
| Half-close | ShutdownWrite — the peer reads EOF (one-way end) |
| Dual-face Accept | pull/Future/blocking coexisting — gradual migration |
| Admission backpressure | Accept queue-depth limit + fast refusal — storms stopped outside the door |
