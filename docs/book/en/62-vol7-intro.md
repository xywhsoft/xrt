---
num: 62
slug: vol7-intro
title: Volume 7 · Networking: Introduction
volume: 卷七 网络
type: intro
lead: The five-backend engine, the three foundations of addresses/ports/buffers, the three transports of TCP/UDP/proxies, plus framing and NIC tools — the industrialization of Volume 6's skeleton.
api: net, tcp
---

## Orientation

Volume 7 industrializes Volume 6's concurrency skeleton into a network stack: **three foundations** (the address model — this volume's currency; the event port — the heart of the five-backend engine; the buffer chain — the data plane's currency) → **two transports** (three TCP chapters, with the gold standard in ch66; one UDP chapter) → **the middle layer** (proxies — hosted dialing for SOCKS5/CONNECT) → **the finale** (net-misc — framing, port files, and NICs). Chapter 61 said "bolt TCP/TLS/HTTP onto the skeleton and you get Volume 7" — this volume delivers the first half (TCP/UDP/proxies bolted on); TLS and HTTP come in Volumes 8/9.

### The volume's knowledge map

```diagram flow
- Foundations: address model (62) + event port with five backends (63) + buffer chain (64)
- Services: DNS resolver — engine-affine (65)
- TCP: connections and I/O (66 gold standard) + backpressure/send family/server side (67)
- UDP: two shapes / batching / multicast (68)
- Middle layer: hosted dialing for two proxy protocols (69)
- Finale: framing, port files, and NIC info (70 — P11)
```

### One through-line: the skeleton's industrial map

Every chapter of Volume 7 maps back to a component of Chapter 61's skeleton — that is this volume's reading map. **Scheduler pump → event port** (ch63: PollFor as the underlying wait; the readiness/completion dual shape corresponds to the skeleton's callback-versus-submission postures); **connection coroutine → Stream callbacks** (ch66: the Accept/Read/Close event table is the engine's expression of the coroutine's straight-line logic); **compute offload → SendFile / Worker-side operations** (ch67: the IO version of offloading heavy work — kernel file sending and Post delivery); **message bus → buffer-chain direct handoff** (ch64: the segment-chain edition of statistics messages — SendBuffer zero-copy); **service-group shutdown → state machine + Close protocol** (ch66-67: CLOSED as the sole final state, the shutdown trio — the engine form of group Cancel). Before each chapter, look back at the skeleton diagram — **the skeleton is the map, the engine is the terrain**.

### Two cross-cutting disciplines

Two disciplines carried over from Volume 6 run through the whole volume. **The zero-copy discipline**: data flows from port to handler to sender — carried by reference chains (ch64's four append kinds + ch67's five send tiers — the data-plane ownership collection); copies happen only at boundaries (Pullup for protocol-header contiguity, Send for cross-boundary copying). **The Worker discipline**: engine callbacks run on Worker threads — direct-move operations like Buffer/Read stay inside a Worker, and crossing threads goes through Post (the engine version of the scheduler binding in Chapter 56) — the price of zero locks is locational discipline. Both disciplines get enforced repeatedly across the TCP/UDP/proxy chapters — they are the chapter-by-chapter expansion of the skeleton's three-zero metrics (pump zero-blocking / pool zero-waiting / message zero-locking).

### Links to the volumes before and after

Looking back: all of Volume 6's concurrency tools take up their posts here (Futures collect IO results, cancellation trees thread through ports, task pools carry heavy computation — the gold-standard chapter is the family portrait); Volume 5's observation pipeline monitors the engine (watermarks/states/statistics fields — the observation posts of ch63/67). Looking forward: Volume 8's TLS slides over the Stream (ch66's transparency guarantee), Volume 9's HTTP slides over TLS, Volume 10's xhttp brings the whole family — **the transport layer's quality decides the whole stack's quality**. The framing/NIC tools of the net-misc finale are logistics for the protocol layer and operations.

### Three kinds of readers, three routes

**Server developers** (the main force): read the three foundation chapters (62-64) carefully in order — they are the vocabulary of every transport chapter; read the TCP gold standard (66) line by line (it is the complete paradigm of engine usage); take UDP/proxy chapters as the business requires. **Embedded/IoT developers**: cherish the select fallback (ch63 — platforms without a dedicated backend) and UDP's lightness (ch68 — the first transport choice under constrained resources); take the advanced TCP chapters by memory budget (SendFile's block budget, WriteLimit's cap). **Protocol developers** (writing custom protocols / TLS implementations): the buffer-chain chapter (64) and the port chapter (63) are your direct tools — segment chains + zero-copy + dual shape are protocol-foundation raw material; the TCP chapters' state machine and shutdown are your behavioral reference. All three kinds share the address model (62) — the volume's first lesson.

### One reminder: the network layer's "simplicity" is a gift of layering

Novices feel sockets are simple (a few lines to connect); veterans know the network layer is the hardest (reordering/retransmission/buffering/backpressure/shutdown all live in the details) — the root of this contrast is that **layering hides the complexity below the interface**. XRT's network layer brings the hidden back into view: the state machine has names (four states), backpressure has numbers (Pending/WriteLimit), shutdown has a protocol (the trio), ownership has types (five send tiers) — **visible complexity is controllable complexity**. If Volume 7 feels "harder than sockets" — you are looking at what has always existed beneath the sockets. With this lens, the complexity of Volume 8's TLS (deeper layering) and Volume 9's HTTP (protocols over protocols) will feel natural — every layer surfaces the next layer's hidden contracts.

### A performance view: the network stack's three optimization tiers

Network performance optimization lives at three tiers — know the positions before tuning. **Engine tier** (ch63): backend selection (automatic in the engine — the io_uring versus epoll difference), Worker count (ch60's economics), batched interfaces (the Vec family — amortizing syscalls); **connection tier** (ch66-67): write budget (too small hurts throughput, too large loses backpressure), buffer pool configuration (ch64 — segment sizes and watermarks), Nagle/latency-class platform items (Stream configuration); **application tier** (the skeleton layer): connection-pool reuse (amortizing TCP handshakes), pipelining (overlapping requests), protocol framing efficiency (ch70's frame tools). Bottom-up returns diminish; top-down controllability grows — **measure before tuning** (Chapter 137 returns with tools). The "benchmark" sections of this volume's chapters are data-taking demonstrations.

### Learning self-checks (before leaving this volume)

With the book closed, answer seven questions: what do the address model's zone ID and list semantics each solve? How do you choose the port's readiness/completion, and how do you query capabilities? Which four ownerships do the four buffer append kinds correspond to? The TCP four states and shutdown trio? How do you pick among the five send tiers, and how does the write budget defend against slow clients? Where is the UDP dual-shape boundary and the MTU trap? What do the proxy's hosted semantics and transparency guarantees promise? Finally: for a service of "one coroutine per connection + file download + proxy egress", draw the component diagram and mark each layer's backpressure position — if the seven questions flow and the diagram draws, Volume 7 is half done (net-misc concludes in the next phase).
