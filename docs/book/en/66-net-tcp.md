---
num: 66
slug: net-tcp
title: TCP Streams (Part 1): Connections and Reading/Writing
volume: 卷七 网络
type: practice
lead: The Engine, Listener, and Stream trio — starting from a loopback echo, mastering TCP's state machine and closing semantics.
api: tcp, tcp_server, net
---

## Orientation

Chapters 62 through 65 settled "where addresses come from" — parsing, ports, buffers, and DNS. This chapter starts moving real data: building an Engine, listening on a port, dialing out, sending and receiving bytes, and finally shutting everything down cleanly. The TCP module is the foundation for everything that follows — Chapter 67's backpressure and server-side shapes, Volume 8's TLS, and Volume 9's HTTP all build on the single object `xnetstream`. This chapter first masters two faces: the blocking synchronous face (the shortest code path for utility programs) and the event-callback face (the skeleton of production services), and thoroughly explains the two most error-prone things — the "state machine" and the "closing sequence".

## Introduction

Imagine writing a small data-migration tool: read a batch of data from one local service and forward it to another. Such a tool's networking needs are plain yet complete — connect, send, receive fully, close cleanly. Anyone who has written it on raw sockets knows the real effort is never the five syscalls, but the wind-down: what if the connection is stuck half-open? Can you close while unsent data still sits in the buffer? When is it safe to release the object?

XRT's TCP layer builds these traps into object contracts: streams have an explicit state machine, `CLOSED` is the only final state, and no reference is released before it is seen; closing comes in two semantics — "drain" and "abort now"; a single Engine drives the event loops of all connections. This chapter walks the model through two complete programs.

## Concepts

### Three objects, one engine

- **`xnetengine`**: the network engine. It manages the platform event backend (IOCP / epoll / kqueue / io_uring) and a pool of Worker threads; every Listener and Stream is attached to some Engine to work. In the config, `Workers` sets the thread count. Rules of thumb for Worker counts: utilities and unit tests need just 1–2 — an Engine's threads only do event dispatch and callbacks, never business computation (to schedule small tasks directly onto the Worker loop, use Chapter 60's network task groups task_net); server-side capacity planning (Worker counts, queue depth, backpressure watermarks) is the subject of Chapter 67. Multiple Engines can coexist, but the convention in this and later chapters is one Engine per process.
- **`xnetlistener`**: the listener. Binds an address (port 0 lets the system assign one) and accepts incoming connections.
- **`xnetstream`**: the stream. The read/write face of one TCP connection; the client and the server each hold one.

The states of both Stream and Listener only move forward:

```diagram state
CONNECTING -> OPEN: connection established
OPEN -> CLOSING: Close / peer closed
CLOSING -> CLOSED: wind-down complete
```

```diagram state
OPEN -> CLOSING: Close requested
CLOSING -> CLOSED: in-flight operations finished
```

`CLOSED` is the only final state, and it is guaranteed before publication: the socket, the buffers, in-flight operations, and the active usage of the Engine have all concluded. In other words, **only when you see `CLOSED` may you release the caller-side reference**.

### One face, two calling conventions

`xnetstream` offers two calling conventions at once:

- **Synchronous face**: `xrtNetListenerAcceptWait` blocks for a connection, `xrtNetStreamRecv` blocks to receive data, and returns an owning `xnetbytes` byte result. Utility-style programs (liveness probes, one-shot migrations, simple clients) use the synchronous face for the shortest code.
- **Event face**: register a callback table (`Accept`/`Read`/`Close` function pointers), and the Engine's Worker threads invoke them as events arrive. Production services use the event face for high concurrency and hard backpressure.

Both faces share the same objects and state machine; this chapter demonstrates one complete program for each.

### Two closing semantics and reference counting

- `xrtNetStreamClose`: requests a draining close — finish sending in-flight data, complete the protocol wind-down, then enter `CLOSED`.
- `xrtNetStreamAbort`: aborts immediately — discard in-flight data and reach `CLOSED` as fast as possible. Error paths and `Cleanup` sections use it to avoid hanging.

Objects returned by creation functions carry **one caller-side reference**; the runtime holds another internal reference until the sole `Close` callback finishes. So the correct exit order is: request the close (Close or Abort) → wait for the state to reach `CLOSED` → `xrtNetStreamDestroy` to release the caller reference. `Ref`/`Destroy` may be used across threads, but `Ref` cannot resurrect an already-finalized object from zero.

## Examples

### First complete program: synchronous-face echo

The following program comes from the repository example `examples/network/tcp_sync/main.c`: start a loopback listener, connect to it and send a `hello`, and have the server receive and print it. This is the canonical sample of "the least code that walks the full lifecycle":

```embed path="examples/network/tcp_sync/main.c" title="examples/network/tcp_sync/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/tcp_sync/main.c -lws2_32 -liphlpapi
received: hello
```

**What just happened.** Six points deserve attention. (1) `Workers = 2` — two Workers are enough for a utility; the Engine itself does no business work. (2) The listen address uses `xrtNetAddrLoopback` for the loopback, **with port 0**; afterwards `xrtNetListenerLocal` retrieves the real port assigned by the system — the standard posture for concurrency tests that must not collide. (3) The third argument `1` of the client's `xrtNetStreamConnect` is the address count; on the synchronous face the connection completes in the background on the Engine. (4) `xrtNetListenerAcceptWait` takes an `xrtDeadlineAfter` deadline; if no connection arrives in time it returns `NULL` and leaves a timeout category in the thread error slot (Chapter 4's model cashed in here). (5) `xrtNetStreamRecv` returns an **owning** `xnetbytes`; borrow a view with `xrtNetBytesView` to read it, and call `xrtNetBytesDestroy` when done. (6) The wind-down first `Abort`s both streams and the listener, **spins until all three reach `CLOSED`**, then `Destroy`s them one by one, and finally calls `xrtNetEngineDestroy`.

### Full event-face shape: a callback-driven echo service

The second program comes from `examples/network/tcp/main.c` — the standard skeleton of a production TCP service: event-table-driven reads and writes, the server echoing by moving the receive buffer directly (zero-copy), and a clean drain-and-close after the peer disconnects:

```embed path="examples/network/tcp/main.c" title="examples/network/tcp/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/tcp/main.c -lws2_32 -liphlpapi
listening on 127.0.0.1:52173
reply: hello TCP
```

**Three key points of the event face.** (1) The `Accept` callback takes over every new connection: it calls `xrtNetStreamSetData` to attach the business context to the stream, and every later callback can retrieve it — no hand-rolled "stream → context" map needed. (2) In the server's `Read` callback, `xrtNetStreamSendBuffer` **hands the receive buffer back by reference** — data moves from the receive buffer into the send queue with zero copies; this is the correct way to write echo-style services. The client's `Read` demonstrates plain reading (`xrtNetBufRead` copying into a stack buffer). (3) The `Close` callback is the sole final-state notification: the `xnetresult` parameter and the error object distinguish "clean close" from "error close"; the sample counts only clean closes. Callbacks run on the Engine's Worker threads, and the main thread observes progress with an atomic counter and a deadline — Chapter 67 upgrades this waiting into proper synchronization primitives and backpressure control.

## Contracts

- **State**: Stream and Listener states only move forward; `CLOSED` is the only final state; before entering `CLOSED`, the socket, buffers, and in-flight operations have all concluded.
- **References**: creation returns the caller reference; the runtime holds an internal reference until the `Close` callback ends; request the close first, wait for `CLOSED`, then `Destroy`; `Ref` is thread-safe but cannot resurrect.
- **Closing**: `Close` drains, `Abort` is immediate; error paths always `Abort`, the happy path lets the data finish.
- **Threading**: event callbacks execute on Engine Workers; the `Close` callback may still be winding down on a Worker, and `xrtNetEngineDestroy` waits for the Workers to exit normally.
- **Errors**: failures are declared by return values (`NULL` / non-`XNET_RESULT_OK`), with details in the thread error slot; categories and domains follow the Chapter 4 model.

## Pitfalls

### Pitfall 1: destroying before CLOSED

Symptoms: occasional crashes or assertions, especially when a busy server exits; sometimes `xrtNetEngineDestroy` hangs.

Cause: before `CLOSED` is published the object is still in use by in-flight operations and Workers; calling `Destroy` early frees memory that is still referenced.

```c bad
xrtNetStreamClose(pStream);
xrtNetStreamDestroy(pStream);   /* state may still be CLOSING */
```

```c good
xrtNetStreamClose(pStream);
while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
	xrtThreadYield();
}
xrtNetStreamDestroy(pStream);
```

### Pitfall 2: forgetting to free the Recv result

Symptoms: memory keeps growing in long-running programs; in allocation stats, `xnetbytes` only ever increases.

Cause: `xrtNetStreamRecv` returns an **owning** byte result; unlike the buffers of Chapter 15, its lifetime belongs entirely to the caller.

```c bad
xnetbytes* pBytes = xrtNetStreamRecv(pStream, 0, deadline, NULL);
printf("%.*s\n", (int)xrtNetBytesView(pBytes).Size,
	(cstr)xrtNetBytesView(pBytes).Data);
/* missing Destroy: leaks one result object per receive */
```

```c good
xnetbytes* pBytes = xrtNetStreamRecv(pStream, 0, deadline, NULL);
if ( pBytes != NULL ) {
	printf("%.*s\n", (int)xrtNetBytesView(pBytes).Size,
		(cstr)xrtNetBytesView(pBytes).Data);
	xrtNetBytesDestroy(pBytes);
}
```

### Pitfall 3: hard-coding the listen port

Symptoms: bind failures when tests run in parallel or a colleague starts the same service — or worse, connecting to someone else's process on that port.

Cause: a fixed port turns "which process owns which port" into a global coordination problem.

Correct approach: set the listen port to `0` and let the system assign, then retrieve the real endpoint with `xrtNetListenerLocal` and hand it to the client — both examples use exactly this posture.

## Exercises

### Basic: change the echo payload

Turn the synchronous-face program into "reply with what you received, uppercased": the server converts the received text to uppercase and sends it back; the client prints the result. Touch only the send/receive logic in `main.c`; keep the lifecycle code untouched.

### Advanced: classify timeouts on receive

Call `xrtNetListenerAcceptWait` with a tighter deadline, and in the timeout branch use `xrtErrorIs(xrtGetError(), XERR_TIMEOUT)` to classify and print `accept timeout`. Hint: confirm the function declared failure (returned `NULL`) before reading the thread error slot — this is Chapter 4's "Pitfall 3" in practice.

### Challenge: write a port-scanning tool

`xrtNetStreamConnect` across a range of ports on `127.0.0.1` (say 51000–51050) one by one, and output the list of open ports; distinguish the two failure modes "connection refused" and "timed out" (by error category). Acceptance: with this chapter's echo service running, the scanner lists its port accurately; explicitly refused ports and ports swallowed by a firewall get different labels; after scanning everything, the Engine destroys normally with no leaks.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Object trio | Engine (thread pool + event backend) / Listener (listen) / Stream (connection) |
| Stream states | `CONNECTING → OPEN → CLOSING → CLOSED`, forward-only |
| Exit order | request close (Close/Abort) → wait for `CLOSED` → `Destroy` → destroy the Engine last |
| Synchronous face | `AcceptWait` / `Send` / `Recv` (owning `xnetbytes`, Destroy when done) |
| Event face | callback table + `SetData` to attach context; `SendBuffer` zero-copy handoff; `Close` is the final-state notice |
| Port assignment | listen on port 0 + `ListenerLocal` for the real endpoint; no collisions in concurrency |
| Errors | return values declare failure; anything other than `XNET_RESULT_OK`, check the thread error slot |
