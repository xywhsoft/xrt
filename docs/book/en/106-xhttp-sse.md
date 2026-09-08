---
num: 106
slug: xhttp-sse
title: SSE: Server Push
volume: 卷十 扩展库：xhttp
type: practice
lead: An EventSource without a dedicated state machine: SSE Reply rides the one and only response state machine, structured event framing, a client-side parser, and Last-Event-ID resume.
api: xhttp-http_sse, xhttp-http_server, xhttp-http_client
---

## Orientation

For one-way push from server to client, the Web world has two options: WebSocket (Chapters 94–96, bidirectional, a separate protocol) and **SSE** (Server-Sent Events — an event protocol on an HTTP response stream). xhttp's implementation philosophy in one sentence: **no dedicated SSE connection, thread, queue, or network state machine** — `xrtHttpSseReplyCreate` creates an ordinary `xhttpreply` with status 200, `Content-Type: text/event-stream`, and a one-shot body of unknown length; the actual sending is driven by the HTTP Server's **one and only** response state machine, TCP/TLS backpressure, and the write deadline; the returned `xhttpbodystream` is an independent producer end, handed to a background task or any thread to write. The client side is the same mirror: a connection layer plus an event parser (the incremental syntax machine for `text/event-stream`), with `Last-Event-ID` disconnect resume built in. The selection rule: whenever push is one-way, use SSE — the protocol is simple, it rides port 80, and automatic reconnection is built into browsers.

## Introduction

Progress push, market-data streams, log tailing, token-by-token output from AI generation — "the server keeps talking, the client only listens" covers most push needs. Bringing WebSocket to such needs is anti-aircraft guns for mosquitoes: one-way use of a bidirectional protocol, plus the full burden of handshake upgrade, heartbeats, and the close protocol. SSE's answer is "don't switch protocols": still an HTTP response, just one that **never ends** — on a chunked stream with `Content-Type: text/event-stream`, lines of `event:/data:/id:/retry:` fields compose events, separated by blank lines.

The engineering difficulty sits on both ends. Server: the response stream must cooperate with background producers — events come from a task thread, the connection writes on a Worker, so how does backpressure travel? Client: the event syntax is **incremental** (one data field may be split across multiple data: lines; events span chunk boundaries) — the parser must be streaming. xhttp gives structured answers on both ends: server-side `SseSendEvent` (one complete event per call, validate before encoding, never commit half an event on failure) + the `xhttpbodystream` producer end (AGAIN backpressure + `WaitWritable` recovery); the client-side parser advances field by field under cap constraints.

## Concepts

### Server: riding the one and only state machine

```diagram flow
- Create: SseReplyCreate(config, &stream) -> an ordinary Reply (200/text-event-stream/body of unknown length)
- Customize: before commit, use ordinary Header APIs to add Cache-Control/CORS/X-Accel-Buffering (deployment-layer duties)
- Commit: after HttpConnRespond the Server freezes and retains the body source - the Reply may be destroyed
- Produce: hand the stream to a task thread -> SseSendEvent/SendComment/Write family writes
- Backpressure: AGAIN retains no input reference -> wait on WaitWritable and retry
- Finish: last-reference destruction = queued events then a normal EOF; Close is an idempotent pre-close; Fail discards undelivered + a stable Cause
```

Three design disciplines. (1) **The boundary between protocol-required and application-decided**: `ReplyCreate` sets only Content-Type; `Cache-Control: no-cache`, authentication, CORS, and proxy buffering policy (`X-Accel-Buffering: no`) are set by the application before commit with ordinary Header APIs — the SSE layer does not guess your deployment environment. (2) **Atomicity**: every Send fully validates and meters first, then encodes directly into one Body Stream node — **no temporary event string is built, no half event committed on failure**; byte and Chunk hard budgets cover concurrent reservations, queuing, and active leases. (3) **Lifecycle decoupling**: after Respond commits, the Reply may be destroyed while the Stream lives on — the "response object" and the "producer end" are two lifetimes.

### Event structure and framing

The four elements of `xhttpsseevent`: `Type` (the event: line — the event type name), `Data` (the data: lines — may be multi-line), `Id` (the id: line — the resume cursor), `Retry` (the retry: line — reconnection milliseconds suggested to the client); `Flags` declares which elements are present. `SseSendEvent` frames a complete event in one call; `SseSendComment` (the `: heartbeat` comment line — keep-alive); `xrtHttpBodyStreamWrite/WriteRef/WriteTake` write raw bytes directly (pre-encoding events, proxy forwarding, custom extensions — the structured Writer is not a mandatory path).

### Backpressure and waiting

A producer write that hits a full budget returns `AGAIN` — **no input reference is retained**, the caller retries verbatim; `xrtHttpBodyStreamWaitWritable` returns the shared Future representing next-generation writability — **one waiter cancelling affects the other waiters**, so it must not face direct cancellation (Chapter 57's shared-Future semantics). This "AGAIN + WaitWritable" is the same vocabulary as Chapter 95's WebSocket Stream.

### Client: connection and parser

The client side has two layers: the **connection layer** (`http_sse_client` — initiates the GET, maintains reconnection and the Last-Event-ID header) and the **parser** (the incremental syntax machine for `text/event-stream` — field lines, event boundaries, multi-line data joining). The parser configuration caps single lines, event data, type, and ID — **a structural memory bound** (Chapter 98's "what to cap is decided by content structure", SSE edition: the representation body is chosen to be unbounded, the structured memory bounded). Disconnect resume: the `id:` sent by the server is recorded by the client and carried back as the `Last-Event-ID` header on reconnection — the server resumes after the cursor.

### Choosing between SSE and WebSocket

| Dimension | SSE | WebSocket (Chapters 94–96) |
| --- | --- | --- |
| Direction | one-way (server → client) | bidirectional |
| Protocol | a plain HTTP response stream | a separate protocol after upgrade |
| Infrastructure | transparent to any HTTP middleware | needs Upgrade support |
| Reconnect | built into browser/client + cursor resume | application-built |
| Fits | push / streaming output / progress | chat / collaboration / bidirectional telemetry |

## Examples

### First complete program: events, heartbeats, and direct writes — three paths

The program below is from `examples/http/sse_server` — the complete face of structured event sending:

```embed path="extlibs/xhttp/examples/http/sse_server/main.c" title="extlibs/xhttp/examples/http/sse_server/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/sse_server/main.c -lws2_32 -liphlpapi
（读回自检通过后正常退出）
```

**What just happened.** (1) `BodyStreamConfigInit` sets MaxBytes/MaxChunks (the producer budget — the source of the AGAIN threshold), then `SseReplyCreate` yields the Reply and Stream object pair. (2) Three write forms fired together: `SseSendEvent` (a progress event with all four elements — type/data/id/retry framed at once), `SseSendComment` (the heartbeat comment line — keep-alive without producing an event), `BodyStreamWrite` (the pre-framed `data: ready\n\n` bytes written directly — the path for proxy/static events). (3) **Read-back verification** (the sample's `exampleReadReply`): reads the Reply's body out streaming with Chapter 107's body reader and prints chunk by chunk — **the bytes the server frames are exactly the syntax the client parser eats**; one sample checks both ends against each other. (4) The two-object lifetime: the Stream is Destroyed when done (EOF published), the Reply Destroyed after — consistent with the contract "after commit the Reply may be destroyed, the Stream independent" (this sample does not commit to a connection; it reads back locally instead).

### Second complete program: the waiting form of client-side stream reading

The second program is from `examples/http/sse_client` — the standard loop of the client's consuming side:

```embed path="extlibs/xhttp/examples/http/sse_client/main.c" title="extlibs/xhttp/examples/http/sse_client/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/sse_client/main.c -lws2_32 -liphlpapi
（示例校验客户端事件消费路径后正常退出）
```

**What just happened.** (1) The body-read loop's three states: `XHTTP_BODY_DATA` (take the chunk, consume, `ChunkRelease`), `XHTTP_BODY_AGAIN` (nothing yet — `xrtHttpBodyReaderWait` takes a Future and waits for arrival), `EOF` (the stream ends normally). The **AGAIN + Wait** pair is the universal skeleton of streaming consumption (symmetric to the server's AGAIN + WaitWritable). (2) The SSE client's connection layer maintains this on top: reconnect per the Retry suggestion on disconnect, the `Last-Event-ID` cursor carried back; the event parser turns the byte stream into structured event callbacks. (3) The companion `examples/http/sse_http` (the bare HTTP form without the client layer — the response stream consumed directly) covers the "want the stream, not the reconnection" scenario.

## Contracts

- **No dedicated state machine**: the SSE Reply is an ordinary Reply (200/text-event-stream/unknown length); sending rides the one and only response state machine with the existing backpressure and write deadline.
- **Protocol/application boundary**: ReplyCreate sets only Content-Type; Cache-Control/CORS/buffering policy are yours to set before commit.
- **Lifecycle**: after Respond the Reply may be destroyed, the Stream independent (held by a task thread/publisher); last-reference destruction = EOF after queued events; Close idempotent; Fail discards the undelivered + a stable Cause.
- **Write atomicity**: validate and meter first, then encode into one node; no temporary event string; no half event on failure; byte/Chunk budgets cover reservations + queuing + leases.
- **Backpressure**: AGAIN retains no input; WaitWritable is a shared Future (no unilateral cancellation); Write/WriteRef/WriteTake direct byte writes optional.
- **Event four elements**: event/data/id/retry + Flags; the comment line keeps alive; id is the resume cursor.
- **Client parsing**: an incremental syntax machine; single-line/data/type/ID four caps — structured memory bounded; Last-Event-ID resume.
- **Consume loop**: DATA (chunk + Release) / AGAIN (Wait Future) / EOF, a three-state skeleton.
- **Trimming**: `HTTP_SSE` server/client independent; the parser usable alone over custom transports.

## Pitfalls

### Pitfall 1: putting data with bare newlines into an SSE event

Symptom: what the client receives as "one event" gets split into several or parses into garbage — newlines inside data changed the event boundary.

Cause: SSE separates events with blank lines, and multi-line data is expressed as multiple `data:` lines — `SseSendEvent` handles multi-line data correctly when framing (splitting it into multiple data: lines), but bypassing the structured entrance and splicing strings by hand easily writes the newlines bare.

```c bad
xrtHttpBodyStreamWrite(pStream, XRT_BYTES_LITERAL(
	"data: line1\nline2\n\n"));   /* line2 without a data: prefix: a syntax error */
```

```c good
xhttpsseevent Event = { 0 };
Event.Data = XRT_STR_LITERAL("line1\nline2");   /* the framer splits multi-line data: */
Event.Flags = XHTTP_SSE_EVENT_DATA;
xrtHttpSseSendEvent(pStream, &Event);
```

### Pitfall 2: ignoring AGAIN — busy-retrying or dropping

Symptom: the producing task writes at full speed, spinning and burning CPU once the budget fills; or AGAIN is treated as an error and events dropped — the client misses events.

Cause: AGAIN is a backpressure signal (the same semantics as Chapter 95): the correct action is to wait for writability then send **the same event** — AGAIN retained no input; the data in your hands is the only copy.

```c bad
while ( xrtHttpSseSend(pStream, Data) == XHTTP_BODY_STREAM_AGAIN ) {
	/* tight loop: burns CPU and may never squeeze in */
}
```

```c good
xhttpbodystreamresult R;
while ( (R = xrtHttpSseSend(pStream, Data)) ==
		XHTTP_BODY_STREAM_AGAIN ) {
	xfuture* pW = xrtHttpBodyStreamWaitWritable(pStream);
	if ( pW == NULL ) { break; }
	xrtFutureWaitFor(pW, Timeout);   /* wait, do not cancel the shared Future */
	xrtFutureDestroy(pW);
}
/* exiting the loop with R as OK/CLOSED: handle each */
```

### Pitfall 3: forgetting X-Accel-Buffering — events batched up by the proxy

Symptom: events arrive in real time on a direct local connection, but behind nginx the client "gets a clump after ages" — the proxy treats the response stream as a bufferable download and batches it up.

Cause: SSE's real-time nature depends on intermediaries **not buffering**; `X-Accel-Buffering: no` is the standard header informing the proxy — it is deployment-environment knowledge, so xhttp does not add it automatically (the protocol/application boundary); you add it.

```c bad
pReply = xrtHttpSseReplyCreate(NULL, &pStream);
respond(pConn, pReply);   /* deployed behind a proxy: events batched up */
```

```c good
pReply = xrtHttpSseReplyCreate(NULL, &pStream);
xrtHttpReplySetHeader(pReply,
	XRT_STR_LITERAL("X-Accel-Buffering"),
	XRT_STR_LITERAL("no"));
xrtHttpReplySetHeader(pReply,
	XRT_STR_LITERAL("Cache-Control"),
	XRT_STR_LITERAL("no-cache"));
respond(pConn, pReply);
```

## Exercises

### Basic: both ends checked against each other

Run sse_server and sse_client through; change the server event to a custom type + multi-line data, and have the client print the parsed structure. Acceptance criteria: multi-line data joined correctly; the id cursor's increments visible.

### Advanced: a progress-push service

Add a `/progress` routing entry to Chapter 103's server: after a task is submitted, SSE pushes progress (a percent event every 100 ms, a heartbeat every 15 s); the producing task runs on its own thread, writing through the Stream. Acceptance criteria: with a slow client (rate-limited consumption) the server's memory stays constant (backpressure working — watch the AGAIN path); task completion sends a done event with id, then EOF.

### Challenge: disconnect resume

The client consumes N events then disconnects; the reconnection carries `Last-Event-ID`; the server resumes after the cursor (cursor storage of your choice: an in-memory ring or Chapter 18's Map). Acceptance criteria: the client's event stream resumes seamlessly (no repeats, no gaps); when the server restarts (cursor lost) the client receives from the start (record this behavioral difference); heartbeats hold the connection through idle periods.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Design philosophy | no dedicated state machine: an ordinary Reply riding the one and only response state machine and existing backpressure |
| Two objects | Reply (destroyable early) + Stream (independent producer); EOF/Close/Fail three finishes |
| Event four elements | event/data/id/retry + Flags; SseSendEvent frames in one call; id = resume cursor |
| Atomic writes | validate and meter first, then encode one node; no half event on failure; budgets cover reservations/queuing/leases |
| Backpressure | AGAIN retains no input; WaitWritable shared Future, no unilateral cancel |
| Direct-write path | Write/WriteRef/WriteTake — pre-encoded/proxy/extensions need not go through the structured Writer |
| Protocol/application boundary | Content-Type belongs to the layer; Cache-Control/CORS/X-Accel-Buffering belong to you |
| Client | connection layer (reconnect + Last-Event-ID) + parser (four caps, structured memory bounded) |
| Consume loop | DATA (chunk + Release) / AGAIN (Wait) / EOF three-state skeleton |
| Selection | one-way push uses SSE; bidirectional goes WebSocket — stronger is not better |
