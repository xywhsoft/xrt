---
num: 69
slug: net-udp
title: UDP Datagrams
volume: 卷七 网络
type: practice
lead: The connectionless and connected dual shape, event-driven send/receive, the batch family, multicast, and PMTU — the engine shape of datagrams.
api: udp, net
---

## Orientation

The UDP module fits datagrams into the engine model: **dual shape** — a connectionless endpoint (recvfrom carries the source address — any source, you filter) and a connected one (after connect, receive only that peer — the client shape); **event-driven send/receive** (engine callbacks — the same callback face as TCP); **the batch family** (the udp_batch example — amortizing multi-packet send/receive in one call); **multicast** (join/leave groups — the multicast example); and **PMTU and asynchronous errors** (udp_introspect/udp_errors examples — ICMP unreachable and other platform capabilities). UDP has no flow control and no state machine — but the engine's callbacks, reference counting, and shutdown protocol are all the same.

## Introduction

The essential difference between UDP and TCP is not "reliable versus unreliable" — it is the **data boundary**: TCP is a byte stream (message boundaries are your problem — Chapter 65's buffer chain and Chapter 72's framing exist for this); UDP is datagrams (one send = one message, one recv = one complete message — the boundary is kernel-kept). This difference makes UDP's API shape entirely different: no send budget (a message either queues or it doesn't), no receive chain (one message, one buffer), no flow control (unreliability is a feature — real-time audio/video would rather drop than back up).

Choosing between the two shapes: **the connectionless endpoint** (server shape) — after bind, receive from any source (recvfrom carries the source; replies specify the destination with sendto); **the connected form** (client shape) — after connect "locks" the peer, receive only from it (the kernel filters) and send directly (no per-call destination). UDP's connect does no handshake (it is just a filter + default destination) — semantically a completely different thing from TCP's connect (a real connection setup).

## Concepts

### The dual shape and lifecycle

```diagram flow
- Create: an xnetudp object -> bind (connectionless) or connect (connected)
- State: the UDP state machine (BINDING/BOUND/CONNECTING/CONNECTED — transitional states of async bind/connect)
- Send/receive: connectionless recvfrom (with source) / sendto (destination given); connected recv/send (fixed peer)
- Close: Close -> CLOSED -> Destroy — reference counting as with Stream (Chapter 67's contract continued)
```

**Asynchronous bind/connect**: UDP's binding and connecting are engine operations too (state-machine transitions — poll `xrtNetUdpState` or wait on events; the udp example's `WaitState` helper demonstrates deadline-bounded polling).

### The event callback face

UDP's event table (same shape as Stream's): Read (message arrived — recvfrom/recv inside the callback), Write (sendable), Close (final-state notice). **Send/receive in the callback**: a datagram arrival triggers the callback, and the callback pulls (pull-style recv — messages in a queue, the callback is a notification); batch pulling (pull several per callback — the efficient shape for draining backlogs). Worker constraints match Stream's (callbacks on Workers, cross-thread via Post — Chapter 68's contract, UDP edition).

### The batch family and the packet structure

`xnetudppacket` (the carrier of message + source address); batch variants operate on several messages at once (array submit/receive) — the amortizing entry for high-PPS scenarios (DNS servers, monitoring beacons). **The Msg family** (SendMsg/RecvMsg — kin of Chapter 64's port layer): vector sending with a target address — UDP's multi-buffer variant.

### Multicast and PMTU

**Multicast** (the multicast example): join/leave multicast groups (group address + local interface), TTL control (hops across network segments); send/receive use the same API as unicast (the group address as destination). **PMTU probing and asynchronous errors**: an oversized send is dropped en route (when fragmentation is disabled) — ICMP "frag needed" is reported asynchronously (RecvError per platform capability — Chapter 64's capability gating applied to UDP); the udp_errors example demonstrates error reception, and udp_introspect demonstrates PMTU/statistics queries.

## Examples

### Complete program: dual-shape loopback

From the repository example `examples/network/udp/main.c` — connectionless endpoint + connected form side by side:

```embed path="examples/network/udp/main.c" title="examples/network/udp/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/udp/main.c -lws2_32 -liphlpapi
UDP server: 127.0.0.1:NNNNN
received: hello UDP
```

**What just happened.** (1) The server's connectionless endpoint binds (port 0 dynamic — Local retrieves and prints `127.0.0.1:NNNNN`); state waiting (the `WaitState` helper polls BOUND within a deadline — managing the async bind's transitional state). (2) The client's connected form connects to the server — receiving only that peer (the server's reply). (3) The client sends `hello UDP` — no destination parameter (connect already locked it); (4) the server's Read event fires, recvfrom pulls (the source is the client) → prints `received: hello UDP` — each shape in its place. (5) The reply sendtos to the client's address — a connectionless reply must specify the destination.

### Complete program: batching and multicast

From `examples/network/udp_batch/main.c` and `examples/network/udp_multicast/main.c` (read together):

```embed path="examples/network/udp_batch/main.c" title="examples/network/udp_batch/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/udp_batch/main.c -lws2_32 -liphlpapi
batch: truncated prefix=16 of 64
pull batches: 3 packets in 1 batch
wait batch: 2 packets (borrow=take=ok)
future batch: 2 packets, ref kept size=2
error queue: empty pull=0/0, async terminal=4
writable: sync=1 async=1
```

**What just happened.** (1) `truncated prefix=16 of 64` — truncated-prefix semantics (a small buffer receiving a big message: truncated to what fits, message size queryable — UDP's within-boundary truncation shape). (2) `pull batches: 3 packets in 1 batch` — batch pulling: three messages in one call (proof of amortized backlog draining in a single callback). (3) The wait/future lines: blocking wait and Future forms each do one batch receive (borrow=take=ok — both takeaway ownerships of message buffers verified). (4) The error-queue line: empty pull and asynchronous terminal states of the error queue — the RecvError family tested on UDP. (5) The writable line: synchronous and asynchronous sendability probes. The multicast example adds the multicast slot (join → group-address send → receive → leave); the sync/future examples are the blocking/Future siblings — four shapes aligned with the TCP family.

## Contracts

- **Data boundaries**: one message, one buffer; no chain, no budget — the fundamental difference from TCP; boundaries are never lost, and neither is ordering (ordering was never promised).
- **Dual shape**: connectionless (any source + sendto replies) / connected (connect filter + fixed destination) — choose by role.
- **Asynchronous lifecycle**: bind/connect transitional-state management (State polling or event waiting + deadline).
- **Same callback face**: Read/Write/Close events; pull-style send/receive; Worker constraints and Post as with Stream.
- **Multicast**: join/leave + TTL; send/receive as unicast. The batch family amortizes high PPS.
- **Asynchronous errors**: ICMP/PMTU-class errors gated by platform capability (RecvError) — the "unreachable" feedback of unreliable messages.

### From examples to engineering: UDP's three hosts

**Discovery and registration class** (multicast's home turf): service discovery (the multicast example's challenge edition), configuration distribution, heartbeat broadcast — small messages + multicast + connectionless low overhead; multicast TTL controls scope (TTL=1 confines to the local segment). **Real-time streaming class** (unreliability as a feature): audio/video, games, telemetry — fixed-cadence sending, no retransmission of drops (retransmitting stale data is meaningless), seq numbers for reordering detection. **Request-response class** (reliability built at the application layer): DNS-style queries, simple RPC — timeout + retry + request-ID pairing (the standard layer of the advanced exercise). The common divide of the three hosts is **the cost of packet loss**: discovery doesn't care, real-time must not retransmit, request-response must retry — build the application layer by cost, rather than defaulting to all-TCP or bare-UDP recklessness.

### UDP's standing in the QUIC era

UDP's standing in the modern network stack deserves a chapter-level cognitive update: **QUIC/HTTP3 all run on UDP** — reliable transport, flow control, and encryption rebuilt at the application layer (above UDP). Why: the kernel rigidity of TCP (middlebox tampering, slow upgrades, head-of-line blocking) forced innovation down onto the UDP carrier. XRT's UDP module supplies the engine-shaped foundation for this (callback face/batching/multicast/PMTU) — raw material for QUIC-class protocols. It complements XRT's own TCP stack (an engine wrapper over kernel TCP): standard web uses TCP (the gold-standard chapter); custom protocols and low-latency scenarios start from UDP raw material. **The modern answer to TCP versus UDP**: standard-protocol needs take TCP; controllable protocol evolution takes UDP raw material — no longer just the old "reliable versus fast" dichotomy.

### Observation and security: UDP's two weak spots

A UDP service's weak observation spots need proactive shoring (Chapter 50's pipeline). **Loss is invisible**: UDP has no retransmission — only the application layer knows how much was lost (seq-gap analysis — seq continuity statistics into logs); **the security face**: UDP is easy to spoof (no handshake verification) — the defense points against amplification reflection attacks (DNS amplification is the classic): response-size limits (a 64-byte request must not earn a 4000-byte reply), destination verification (connected-form filtering), rate limiting. Walk both weak spots before a service ships — UDP's "simplicity" is API simplicity; the operations and security homework only grows.

## Pitfalls

### Pitfall 1: reading UDP connect as TCP connect

Symptoms: believing "connect succeeded = the peer is reachable" — in fact connect succeeds even when the peer doesn't exist (UDP connect only sets a filter); or believing post-connect reliability — drops continue as usual.

Cause: UDP connect is a local filter setting — no handshake, no confirmation, no change to the unreliable nature.

```c bad
if ( xrtNetUdpConnect(Udp, &Peer) ) {
	AssumeReachable(Peer);   /* wrong — UDP connect does not verify the peer exists */
}
```

```c good
xrtNetUdpConnect(Udp, &Peer);   /* sets filter + default destination — a local operation */
SendRequest(Udp);
if ( !WaitResponse(Udp, Deadline) ) {   /* reachability is verified by application-layer request-response */
	RetryOrFail();
}
```

### Pitfall 2: message size exceeding the path MTU

Symptoms: fine on local loopback, dropped across network segments — big messages discarded (fragmentation disabled or an intermediate router's smaller MTU); occasional partial arrival (partial fragment loss).

Cause: the UDP message exceeds the path MTU — kernel fragmentation (partial loss = whole message lost) or a PMTU black hole (DF bit + filtered ICMP).

```c bad
char Big[65000];
Fill(Big);
xrtNetUdpSend(Udp, Big, sizeof(Big));   /* over MTU — near-certain whole-message loss across segments */
```

```c good
/* application-layer limit within a safe size (commonly below 1200-1400 bytes); big messages go over TCP or are app-fragmented */
char Packet[1200];
xrtNetUdpSend(Udp, Packet, sizeof(Packet));
```

## Exercises

### Basic: dual-shape loopback reproduced

Reproduce the main example; then add a "second client" sending to the server — verify the connectionless endpoint receives from any source while the connected form receives only its locked peer (the second client's packets are filtered).

### Advanced: request-response timeout retry

The standard reliability layer for a UDP client: send request → wait with deadline → retry on timeout (exponential backoff) → report after N failures; the server simulates loss (randomly dropping 50%) to verify retry convergence. Hint: the deadline family + Chapter 54's cancellation.

### Challenge: a multicast discovery service

LAN service discovery: servers join a multicast group and broadcast themselves periodically; clients join the group to receive discovery messages; unicast connection confirms. Acceptance: with multiple servers broadcasting concurrently, the client enumerates all; join/leave clean (no leaks); broadcast-storm control (interval + TTL); the whole discover-to-connect chain logged and auditable.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Data boundaries | one message, one buffer — the kernel keeps boundaries; no chain, no budget |
| Dual shape | connectionless (any source + sendto) / connected (connect filter + fixed destination) |
| Lifecycle | async bind/connect state machine + deadline polling / event waiting |
| Callback face | Read/Write/Close as with Stream; pull-style send/receive; same Worker constraints |
| Batch family | multi-message send/receive at once — high-PPS amortization; Msg family with address vectors |
| Multicast | join/leave + TTL; send/receive as unicast |
| MTU | keep messages within a safe size (~1200); PMTU errors gated by capability |
