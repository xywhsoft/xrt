---
num: 57
slug: channel
title: Channels and Select
volume: 卷六 进程与并发
type: practice
lead: Message channels with close semantics — exact capacity, TrySend, drain-after-close, Select multiplexed waiting, and two-in-one cancellation.
api: channel, cancel
---

## Orientation

The Channel is the concurrency system's **highest-frequency business-layer tool**: type-safe message passing (`ptr` semantics), exact capacity with backpressure (full means wait — capacity 0 is rendezvous-style synchronous handoff), Go-style **close semantics** (after Close, sends error; receives drain the backlog before ending — the shutdown guarantee of "no data lost, no sends left hanging"), **Select multiplexed waiting** (waiting on several channels and timers at once — first to arrive wins), and **cancellable receive** (RecvCancel — Chapter 54's token in a two-in-one wait). Chapter 21's lock-free queue is its transport foundation; the Channel adds the close protocol and waiting semantics on top.

## Introduction

A producer-consumer hand-assembled with Chapter 53's Mutex+Condition (conditional waiting + mutual exclusion + wakeup coordination) takes thirty lines and errs easily (forgotten Signal, spurious wakeups, buffer edges); Chapter 21's lock-free queue solves transport (lock-free, high throughput) but has no **close** — how does the consumer know "there will be no more"? What about producers still Sending at shutdown?

The Channel packages both: **transport** (a capacity queue — full means Send waits, empty means Recv waits) and **protocol** (the rules after Close: senders get an explicit error; receivers, after draining the backlog, get an explicit end signal). Close semantics are the message system's shutdown cornerstone — "no more sends" is an **event**, not something guessed by timeout. Select adds multiplexed waiting: one coroutine/thread waiting on two channels at once (whichever arrives) or with a timer (timeout first) — the event loop's basic posture.

## Concepts

### The channel trio: capacity, send, receive

```diagram flow
- Create: xrtChannelCreate(capacity) - capacity 0 rendezvous (synchronous handoff at the exchange point) / N buffered
- Send: Send (wait when full, returns xwaitresult) / TrySend (false when full - a non-blocking probe)
- Receive: Recv (wait when empty) / after close: the backlog keeps coming; drained, an end signal arrives
```

**Capacity semantics**: 0 is rendezvous — the send waits for a receiver to arrive, a one-to-one relay (the standard synchronous handoff); N is buffered — at most N cached, full means Send waits (backpressure built in).**TrySend's use**: probing on non-blocking paths (an event loop must not block) — full returns false and the caller decides to drop or retry later (it does not wait).

### Close semantics: Go's same three-party rule

| Role | After Close |
| --- | --- |
| Sender | Send/TrySend return an explicit error - no more data gets in |
| Receiver | the backlog keeps arriving - after **draining**, Recv returns the end signal (not OK) |
| Closer | Close is idempotent - closing repeatedly is safe |

The drain semantics (the channel sample's loop shape) are the key to shutdown: Close is not "flush" — already-enqueued data is consumed as usual; "closing" only declares the **future** closed. This guarantees "no data lost, no sends hanging" — the message edition of Chapter 21's queue Close section (Chapter 48's shutdown protocol).

### Select: multiplexed waiting

`xrtChannelSelect` (demonstrated in the select sample): register receive intent on several channels at once plus an optional timer — whichever arrives first returns (with a marker of **which channel arrived**). The shape: an array of waits + timeout → the ready item returned.**Fairness**: when several are ready simultaneously, registration order or implementation policy decides — random fairness is not promised (weighted fairness needs the business layer to rotate registration order). Select is the standard posture for "one consumer serving many sources" and "waits with timeouts" — the backbone of event loops and timeout control.

### Confluence with the cancellation token

`xrtChannelRecvCancel` (the channel_cancel sample): receive and cancellation two-in-one — Chapter 54's cancellable wait, Channel instance. Compose it into the **three-way wait**: data arrives / channel closed (end signal) / cancellation hit — the return `xwaitresult` reports uniformly. The Send side has a cancellable variant too (at shutdown, a sender stuck on a full channel can also be cancellation-woken).**Every wait shape of the Channel (Send/Recv/Select) takes a token** — the cancellation tree runs through the channels.

### Channel vs queue: which when

The division between Chapter 21's lock-free queue and this chapter's Channel: the **queue** is a transport primitive — maximum throughput (lock-free), batch interfaces, dedicated to SPSC/MPSC/MPMC topologies; the **Channel** is a protocol primitive — close semantics, Select multiplexing, cancellation integration, scheduler affinity (a coroutine's wait occupies no thread — Chapter 56's dividend). Selection: hot-path data plane (network sends/receives, task dispatch) — queues; business cooperation plane (request-response, event dispatch, shutdown draining) — Channels. The two compose — the Channel uses a queue underneath (implementation layer), with each interface keeping its own post.

## Examples

### Complete program: capacity, TrySend, and drain-after-close

From the repository sample `examples/concurrency/channel/main.c`:

```embed path="examples/concurrency/channel/main.c" title="examples/concurrency/channel/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/channel/main.c -lws2_32 -liphlpapi
10
20
```

**What just happened.** (1) `xrtChannelCreate(2)` — exact capacity 2: both `xrtChannelTrySend` calls (10, 20) enqueue successfully (true while not full); a third would return false (full — this sample sends no third). (2) `xrtChannelClose` closes — **the backlog remains**: two Recvs take out 10 and 20 as usual (draining); (3) after the backlog drains, Recv returns the end signal (not OK) — the receive loop terminates naturally. (4) This is the complete empirical proof of the "close three-party rule": sends stop (after Close), the backlog flows (10/20 consumed), the end is explicit (the signal after draining). The channel_select sample demonstrates Select: two channels (fast and slow) waited at once — the fast one wins (`20` comes from the earlier arrival).

### Complete program: Select multiplexed waiting

From `examples/concurrency/channel_select/main.c`:

```embed path="examples/concurrency/channel_select/main.c" title="examples/concurrency/channel_select/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/channel_select/main.c -lws2_32 -liphlpapi
20
```

**What just happened.** (1) Two channels each hold one item, but with **different arrival timing** — Select registers receive intent on both; the first to arrive (the one carrying 20) wins and is received. (2) The output `20` proves Select's verdict: **first-come-first-served among the many** — not polling both (that would be busy waiting), but one wait watching several events. (3) The other channel's data remains (unconsumed — a later Select or Recv can take it); Select never consumes the not-ready lanes. (4) Drop this shape into an event loop: many sources (timers, IO notifications, a command channel) before one Select — the event loop's standard backbone. The channel_cancel and deadline samples complete the third dimension: the fused wait of cancellation and timeout.

## Contracts

- **Capacity is backpressure**: full means Send waits; capacity 0 is rendezvous (the synchronous handoff point); TrySend is a non-blocking probe, not a wait.
- **Close's three parties**: senders error / receivers drain the backlog then get the end signal / Close is idempotent — the standard protocol of shutdown draining.
- **Select is first-come-first-served**: several simultaneously ready follow implementation policy; not-ready lanes are not consumed — fairness needs rotate at the business layer.
- **Cancellation integration**: every shape (Send/Recv/Select) takes a token — the three-way wait (data/close/cancel) uniformly returns xwaitresult.
- **Message ownership**: passing `ptr` — the sender constructs, the receiver handles; reference-counted objects pass directly (Chapter 31's value reference discipline).
- **Queue division**: hot-path data plane uses Chapter 21's queues; the cooperation protocol plane uses Channels — compose without confusing.

### From examples to engineering: three hosts of the Channel

**Task dispatch** (the request-handling shape): a receive coroutine RecvCancel-ing requests → a handler coroutine pool consuming → a result channel writing back — the Channel strings the pipeline together, capacity is the concurrency cap (backpressure), close is the shutdown drain (Chapter 49's five-second convention, message edition).**Event bus**: several event sources (signals, timers, IO notifications), each a coroutine producing events into one Channel — one consumer's Select tends all sources (fan-in); the central hub dispatching by event type.**Actor mailbox**: one command channel per Actor (small, fixed capacity), the Actor looping Recv to handle — natural serialization (no locks inside the Actor); closing the mailbox is the Actor's retirement. The three hosts share one mental model: **the Channel is the concurrent world's "method call"** — passing messages instead of sharing memory, waiting instead of locking, closing instead of lifecycle flags (the CSP school's maxim, landed in XRT).

### CSP versus shared memory: two concurrency styles

The school behind the Channel (CSP — Communicating Sequential Processes) deserves a formal comparison with Chapter 53's shared-memory style.**The shared-memory school**: data in shared structures, locks guarding access — direct, high-performance, but lock correctness rests wholly on discipline (ordering, granularity, deadlock).**The message school**: data not shared, ownership passed through channels — a lock-free mindset (single-threaded data ownership), clear composition (channels as interfaces), but copying/passing costs latency. Engineering reality is a mix: **inside data structures, locks or lock-free primitives (Chapter 21's queues)**; **between modules, messages (Channels)** — "tight inside, clear outside". XRT provides both in full (Chapter 53's primitives + this chapter's Channels); selection is decided by layer, not faith.

### A design view: the three numbers of capacity

Channel capacity is not one number — it is a design decision among three.**0**: rendezvous — the synchronous handoff wanting "the other side confirms receipt" (RPC semantics);**a small fixed value** (1~16): backpressure conducts fast — upstream feels a slow downstream almost immediately (small capacity is the warning line; overload surfaces early);**a large value or a dedicated queue**: peak shaving — buffering bursts (but pair it with observation: the queue-depth curve visible before it runs away). Three numbers, three promises: synchronous, early backpressure, peak shaving. Starting points: cooperation channels small (8); request pipelines sized to downstream latency and memory budget; never "unlimited" (Chapter 21's backpressure-valve discipline holds for Channels too — CreateLimit's submission cap is the scheduler-side sibling).

## Pitfalls

### Pitfall 1: using Close as flush

Symptom: after closing, the receiver "occasionally loses data" — in fact it exited the receive loop before draining Close's backlog; or a sender's post-Close data silently vanishes.

Cause: reading Close as "voiding the channel" — it actually "declares the future closed"; the backlog still arrives, and only after draining does the end signal come.

```c bad
xrtChannelClose(pChannel);
/* the receiver assumes closed means no data - exits directly, the backlog is lost */
```

```c good
xrtChannelClose(pChannel);
/* the drain loop: the backlog keeps arriving until the end signal */
while ( xrtChannelRecv(pChannel, &Item) == XWAIT_OK ) {
	Consume(Item);   /* OK during the backlog; the end signal after draining */
}
/* exit on the end signal - zero data loss (the sample's shape) */
```

### Pitfall 2: using a rendezvous channel as a buffer

Symptom: on a capacity-0 channel, Send is followed immediately by the next step — the data "seems sent" but the receiver hadn't arrived; throughput far below expectation, or timing scrambled.

Cause: capacity 0 is **rendezvous** — Send waits for a receiver to complete the handoff; used as an "async buffer", semantics and expectation misalign.

```c bad
xchannel* Ch = xrtChannelCreate(0);   /* rendezvous */
xrtChannelSend(Ch, Item);              /* waits for a receiver - the "send and go" expectation fails */
DoNextThing();                          /* the timing dependency never happened */
```

```c good
xchannel* Ch = xrtChannelCreate(8);  /* buffered: 8 slots of headroom */
xrtChannelSend(Ch, Item);             /* enqueued and returns (not full) - send and go holds */
DoNextThing();
/* only use capacity 0 when synchronous handoff (continue only after confirmed receipt) is wanted - choose capacity by semantics */
```

## Exercises

### Basic: verify the three-party rule

A capacity-3 channel: enqueue 3 → Close → drain-receive 3 → the fourth receive yields the end signal; then Send on the closed channel to verify the error — the three-party rule asserted item by item.

### Advanced: a fan-in collector

Three producer coroutines (5 numbered messages each) converge into one Channel; a consumer's Select tends the channel and a timer (2-second timeout) — verify both paths: collecting all 15, and the timeout.

### Challenge: a stoppable request pipeline

A full pipeline: a receive coroutine (RecvCancel for requests or cancellation) → a handler coroutine (Park waiting for an external DB callback to Wake) → a response channel; the stop signal fires the cancellation tree, the whole pipeline drains and finishes. Acceptance criteria: zero in-flight requests lost at shutdown (either handled or explicitly cancelled); every coroutine's final state and drain order auditable; Chapter 50's observation fields complete.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Capacity semantics | 0 rendezvous (synchronous handoff) / N buffered (full means wait = backpressure); TrySend non-blocking |
| Close's three parties | senders error / receivers drain then end signal / Close idempotent - Go's same |
| Select | multiplexed receive + optional timer, first-come-first-served; not-ready lanes not consumed |
| Cancellation | Send/Recv/Select take tokens - the three-way wait uniformly xwaitresult |
| Message ownership | ptr passing - sender constructs, receiver handles; reference-counted objects pass directly |
| Division with queues | hot-path data plane queues / cooperation protocol plane Channels - compose without confusing |
