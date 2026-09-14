---
num: 128
slug: xssh-forward
title: SSH (Part 6): Port Forwarding
volume: 卷十一 其他扩展库
type: practice
lead: The two shapes direct-tcpip and forwarded-tcpip, the tcpip-forward global request and dynamic ports, port-semantic boundaries and address borrowing — every part of the tunneling protocol.
api: xssh-ssh_client_forward, xssh-ssh_forward_message
---

## Orientation

SSH's most everyday use is not opening a shell but **digging tunnels**: `ssh -L 8080:db:80` (local forwarding — local 8080's traffic delivered through SSH to db:80, which the server can reach) and `ssh -R 9000:localhost:9000` (remote forwarding — the server sends 9000's traffic back to your side). The protocol has two pieces: the **direct-tcpip** channel (local forwarding's carrier — a channel the client opens proactively, carrying the target address) and the **forwarded-tcpip** channel (remote forwarding's carrier — a channel the server opens proactively after receiving your `tcpip-forward` global request). xssh's layering as before: `ssh_forward_message` (pure protocol payload — message encoding/decoding, port-semantic boundaries, building no listener and resolving no DNS) and `ssh_client_forward` (the client composition layer — `DirectTcpipOpen`/`ForwardedTcpipAccept`/`TcpipForward`/`TcpipForwardCancel`, four entrances, hiding no local listener/thread/wait). A real forwarding service composes XRT listener/stream with channel windows in the upper layer — this chapter gives you the parts and the assembly.

## Introduction

The forwarding protocol's address semantics deserve a close read: a `direct-tcpip` channel carries `要连的地址:端口 + 来源地址:端口` (the address:port to connect to + the origin address:port) — the target declared by the **client** (the server's policy layer decides whether to allow it — "target access control remains with the server's policy layer" is the contract's own wording); `forwarded-tcpip` is the reverse — the server-opened channel carries `被连的目标 + 发起者来源` (the connected target + the originating source). **Port-semantic boundary**: RFC 4254 carries ports as uint32 on the wire, but xssh uniformly bounds them to 0..65535 at the semantic encode/decode boundary — an illegal write does not advance the writer, an illegal read is a protocol error (normalizing "carried in uint32 but actually 16-bit" — a semantic layer stricter than the wire format). **Port 0's dynamic allocation**: a `tcpip-forward` request with port 0 = let the server assign — the success response carries the real port (`TcpipForwardSuccessRead` retrieves it — how to get a remote dynamic port).

The global request's **token correlation**: `tcpip-forward` is a want-reply global request — take a token when sending; success/failure returns via the client's Global event carrying the same token — the correlator when several global requests are in flight (Chapter 125's replyqueue, global edition).

## Concepts

### The two shapes and their data flows

```diagram flow
- Local forwarding (-L): the local listener accepts a connection -> open one direct-tcpip channel per connection
  (target = the configured host:port, origin = the local connecting side) -> the channel data plane carries both ways
- Remote forwarding (-R): the TcpipForward(listen address, port) global request -> the server starts a listener
  -> an external connection arrives -> the server opens a forwarded-tcpip channel back (target = the incoming connector)
  -> ForwardedTcpipAccept parses -> confirmation sent automatically -> data-plane carrying
- Data plane: uniformly channel I/O (Chapter 125's windows/backpressure/close) - forwarding adds nothing separate
```

### The message layer: five payload classes and borrowing semantics

`ssh_forward_message` implements five: `tcpip-forward` (send) / `cancel-tcpip-forward` (cancel) / the dynamic-port success response (read) / `direct-tcpip` open (send+read) / `forwarded-tcpip` open (read). **Address borrowing**: addresses and origins are borrowed verbatim as SSH strings — **never forced into a socket address first** (the target may be a host name from the server's perspective — resolving it on the client is both meaningless and usually impossible). **Reuse of the common builders**: the writer reuses the global/channel common prefixes directly — the final payload completes in one pass, building no temporary Fields buffer (the zero allocation path). **Strict reading**: the generic envelope parses first, the type-specific fields read strictly, **trailing content rejected** (the Nth appearance of smuggling defense).

### The client composition layer: four entrances

- `xrtSshClientDirectTcpipOpen`: open a direct-tcpip (the starting point of local forwarding/custom tunnels).
- `xrtSshClientForwardedTcpipAccept`: during the `Packet` callback or HOLD, parse a forwarded-tcpip the server opened proactively — **after the read transaction commits, the client automatically sends the confirmation** (the ordering contract: "commit the read transaction, send the response, commit the channel state" — strictly three steps; the application must not send the confirmation directly inside the Packet callback).
- `xrtSshClientTcpipForward`: send the want-reply global request (port 0 for dynamic allocation); the token comes from a dynamic bounded FIFO.
- `xrtSshClientTcpipForwardCancel`: cancel the same remote address.
- Rejecting unknown/disallowed peer channels: `xrtSshClientChannelReject`; **when the Packet is accepted without a decision, the client answers `UNKNOWN_CHANNEL_TYPE` by default** — the peer never waits forever (protocol politeness, made default).

### Assembling a complete forwarding service

The parts are in place; the service shape = upper-layer assembly: local forwarding = Chapter 67's listener + DirectTcpipOpen per connection + bidirectional carrying (channel I/O ↔ TCP stream); remote forwarding = TcpipForward + a local listener (for the server's incoming opens) + ForwardedTcpipAccept. The ways the carrying can die (backpressure linkage, half-close, cancellation) all reuse the existing vocabulary — this chapter's new things are only "protocol messages and the two ends' semantics".

## Examples

### First complete program: building the direct-tcpip message

The program below is from `examples/forward_message` — the local-forwarding channel-open message:

```embed path="extlibs/xssh/examples/forward_message/main.c" title="extlibs/xssh/examples/forward_message/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/forward_message/main.c -lws2_32 -liphlpapi
（输出 direct-tcpip open 报文构建的自检结果）
```

**What just happened.** (1) `xrtSshDirectTcpipOpenWrite(&Writer, 通道号, 初始窗口 1 MiB, max-packet 32 KiB, 目标 "db.internal":5432, 来源 "127.0.0.1":50000)` (channel number, initial window 1 MiB, max-packet 32 KiB, target, origin) — typical local-forwarding parameters: the target is the database address **from the server's perspective** (the host name borrowed verbatim — the server resolves it); the origin is the local connecting side. (2) The window/max-packet are exactly Chapter 125's channel capability declaration — the forwarding channel's initial budget. (3) All port parameters fall inside the 0..65535 semantic boundary — passing 65536 is rejected at the write layer (the writer does not advance). This message plus the channel framing (Chapter 125) is one complete tunnel request.

### Second complete program: the forwarding configuration face

The second program is from `examples/client_forward` — the composition layer's resource statement:

```embed path="extlibs/xssh/examples/client_forward/main.c" title="extlibs/xssh/examples/client_forward/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/client_forward/main.c -lws2_32 -liphlpapi
（输出全局回复 FIFO 上限的配置自检结果）
```

**What just happened.** (1) Printing `xrtSshClientConfigInit`'s default `GlobalReplyLimit` — **the capacity of the global-request token FIFO** (the ceiling on in-flight tcpip-forward/cancel): a bounded FIFO is the global requests' budget, locking the volume of in-flight forwarding (one more budget line after Chapter 124's auth guard — forwarding requests are also rationed). (2) The header comment reiterates the design boundary: "the forwarding initiator introduces no hidden listener or thread" — the local listener is your assembly part, not the library's private property. (3) The full assembly of a real forwarding service lives in the tests and Chapter 127's runtime — this sample pins the resource fact that "forwarding overhead = one FIFO ceiling".

## Contracts

- **Two shapes**: direct-tcpip (client opens, target declared) / forwarded-tcpip (server opens back, originating from its listener).
- **Global requests**: tcpip-forward want-reply; tokens from a dynamic bounded FIFO; port 0 = server-side dynamic allocation (SuccessRead retrieves it).
- **Cancellation**: TcpipForwardCancel revokes the same remote address.
- **Port boundary**: the semantic layer uniformly 0..65535; an illegal write does not advance, an illegal read is a protocol error (normalization atop the wire uint32).
- **Address borrowing**: strings verbatim — never forced into socket addresses; the right to resolve the target and the access control belong to the server's policy layer.
- **confirm order**: read transaction committed → response → channel state committed — the application never sends the confirmation inside the Packet callback.
- **Default rejection**: a Packet accepted without a decision answers UNKNOWN_CHANNEL_TYPE by default — the peer never waits on nothing.
- **Data plane reuse**: channel I/O/backpressure/windows/close uniformly Chapter 125's vocabulary — forwarding sets up no second state machine.
- **Zero hiding**: no local listener/thread/wait built — service assembly belongs to the upper layer.
- **Messages zero-allocation**: the common-prefix builders finish in one pass; the reader strictly rejects trailing data.

## Pitfalls

### Pitfall 1: resolving the target address as a local address

Symptom: the client tries to resolve `db.internal`, fails — refuses to build the tunnel; or resolves it to the same-named machine from the client's view — connects to the wrong target.

Cause: direct-tcpip's target address is **from the server's perspective** (host name/IP borrowed verbatim). The client does not resolve — the right to resolve and the access control are both server-side.

```c bad
if ( !xrtNetAddrParse(Target, &Addr) ) {
	fail();   /* the client resolving a server-perspective address: meaningless and usually failing */
}
```

```c good
/* the address goes into the message verbatim - the server resolves and authorizes */
xrtSshDirectTcpipOpenWrite(&Writer, Id, Win, MP,
	TargetHostView, TargetPort, SourceView, SourcePort);
```

### Pitfall 2: firing the confirmation early inside the Packet callback

Symptom: occasional channel-state scrambling — your confirmation races and duplicates the client's automatic one.

Cause: the ordering contract is the client's strict three steps (commit the read transaction → send the response → commit the channel state); after `ForwardedTcpipAccept` parses, the confirmation is **automatic**. The application firing early breaks the transaction boundary.

```c bad
onPacket(...) {
	parse_forwarded(&Msg);
	send_confirmation(Chan);   /* fired early: races the automatic path */
}
```

```c good
onPacket(...) {
	if ( xrtSshClientForwardedTcpipAccept(Client, &Msg, ...) ) {
		/* the confirmation has already been sent automatically after the read transaction committed */
		register_tunnel(&Msg);   /* the application only registers */
	}
}
```

### Pitfall 3: disconnecting without cancel (server listener leak)

Symptom: after the client disconnects, the server still listens on the remote forwarding port — occupying the port, leaving an attack surface.

Cause: the `tcpip-forward` registration persists on the server; before disconnecting, run `TcpipForwardCancel` (most servers clean up on a normal disconnect, but explicit cancellation is the deterministic polite path — especially in long-lived connection pools that repeatedly build forwards).

```c bad
/* built a remote forward, then disconnected straight away */
```

```c good
/* close sequence: cancel the forward first, then close the connection */
xrtSshClientTcpipForwardCancel(Client, BindAddr, Port, ...);
drain_and_close(Client);
```

## Exercises

### Basic: round trips of the five message classes

Build/parse the five forwarding message classes (forward/cancel/success/direct/forwarded) — including both port boundaries, 0 and 65536. Acceptance criteria: all five round-trip with identical fields; 65536 rejected at write without advancing.

### Advanced: a minimal local-forwarding service

Assemble: local listener (Chapter 67) → DirectTcpipOpen per incoming connection → bidirectional carrying (channel I/O ↔ TCP) → close propagation on both ends. Against a real SSH server, forward to an HTTP port on its localhost. Acceptance criteria: curl fetches normally through the tunnel; with slow consumption, memory constant on both ends; half-close propagation (one side EOF, the other still finishes cleanly).

### Challenge: a remote forwarding table

`TcpipForward(端口 0)` (port 0) takes a dynamic port → record the mapping → dispatch each incoming ForwardedTcpipAccept by origin to a local target → cancel to clean up. Multiple forwards concurrently. Acceptance criteria: dynamic ports retrieved and recorded correctly; after cancel the server stops listening (reconnecting refused); multiple tunnels concurrent without interfering.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two shapes | direct-tcpip (-L, client opens) / forwarded-tcpip (-R, server opens back) |
| Global requests | tcpip-forward want-reply; token FIFO; port 0 = dynamic allocation |
| Getting the dynamic port | TcpipForwardSuccessRead — the uint32 in the success response |
| Port boundary | semantic layer 0..65535; illegal writes don't advance / illegal reads are protocol errors |
| Address borrowing | strings verbatim; the server resolves and access-controls — the client touches nothing |
| confirm order | read commit → response → channel state; the application never fires early |
| Default rejection | a Packet accepted without a decision → UNKNOWN_CHANNEL_TYPE |
| Data plane | channel I/O/windows/backpressure all reused from Chapter 125 |
| Zero hiding | no listener/thread built — service assembly belongs to the upper layer |
| Assembly parts | -L = listener + Direct + carrying; -R = Forward + listener + Accept |
