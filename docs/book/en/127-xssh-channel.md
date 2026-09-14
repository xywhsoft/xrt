---
num: 127
slug: xssh-channel
title: SSH (Part 5): Channels and Windows
volume: 卷十一 其他扩展库
type: practice
lead: RFC 4254's bidirectional flow-control windows, the open/confirm/close states, two-phase window return and the dynamic channel set — the multiplexed data plane.
api: xssh-ssh_channel_core, xssh-ssh_channel_window, xssh-ssh_channels
---

## Orientation

After authentication (Chapter 124) the connection is "yours" — and SSH's power lies in opening **multiple channels** over one connection: session (shell/exec/sftp), direct-tcpip (the basis of Chapter 126's forwarding), custom types. This chapter covers the data plane trio: **window** (`ssh_channel_window`: RFC 4254 bidirectional flow control — the remote window + max-packet bound sending, the local window returned by consumption; no allocation, no network ownership); **channel core** (`ssh_channel_core`: open/confirm/failure, EOF/CLOSE states, data commits and window transactions — owning no buffers and touching no transport; placeable in arrays/hashes for O(1) routing by recipient); **channel set** (`ssh_channels`: the dynamic ownership layer — nodes created on demand, stable addresses, hard caps; no fixed arrays, zero idle cost). The window mechanism is SSH's edition of "backpressure" — the same family as Chapter 68's write budgets and Chapter 96's flow control, but bidirectional and per-channel.

## Introduction

Why do channels need windows? One SSH connection's TCP buffers are shared by the whole connection — if channel A's peer stops reading, the TCP window shrinks and channel B's data stalls too (head-of-line blocking). **Channel-level windows** refine flow control per channel: the sender may send at most "the window the peer granted" bytes, continuing only upon WINDOW_ADJUST; the receiver returns window as the application consumes. Effect: a slow channel blocks only itself (its own window exhausts) while fast channels run on — multiplexing fairness guaranteed by the protocol.

The implementation has a refined spot: **two-phase return** — after the application consumes data, the quota first enters `ReceivePending` (the remote not yet told); when a threshold is reached or the window exhausts, a WINDOW_ADJUST is assembled, and only **after the message is reliably queued** does `AdjustCommit` run — "avoiding enlarging the local window before a send failure" (announcing early = over-granting = protocol scrambling). **uint64 accounting**: internal unconsumed/pending-return bytes use 64-bit counters (long connections don't wrap past 4 GiB of accumulation), while the wire window stays strictly uint32 (RFC requirement).

## Concepts

### The window: bidirectional flow-control state

```diagram flow
- Send: SendLimit = min(remote window, remote max-packet) -> data enters the reliable queue -> SendCommit deducts
  (no commit on queue failure) -> receive WINDOW_ADJUST -> SendAdjust (wraparound is a protocol error)
- Receive: data arrives -> ReceiveCommit (validate local window and max-packet) -> application consumes -> ReceiveConsume
  (quota enters Pending) -> at threshold/window exhaustion -> AdjustReady -> AdjustLimit (the amount one ADJUST may safely carry)
  -> message reliably queued -> AdjustCommit (two phases: the window enlarges only after the queue succeeds)
- Dynamic capacity: ReceiveGrantCommit adds receive capacity - zero-window startup and dynamic memory budgets
```

Zero-window startup is an elegant capability: the initial local window may be 0 — wait until the application has actually readied buffers, then `Grant` — lazy initialization of "build the channel first, budget later".

### The channel core: open and the state machine

```diagram state
CLOSED -> OPEN_PENDING: OpenInit (this side initiates; after wire commit, wait for the peer's response)
OPEN_PENDING -> OPEN: ConfirmationCommit (peer confirms) / FailureCommit (failure is final)
CLOSED -> ACCEPT_PENDING: AcceptInit (peer initiates; the policy layer copies the type fields)
ACCEPT_PENDING -> OPEN: AcceptCommit / RejectCommit
OPEN: the only stage where the data plane is open (EOF/close advance independently)
OPEN -> CLOSED: bidirectional close complete (CloseSend + CloseReceive)
```

**EOF and CLOSE separated**: EOF closes one data direction only ("I'm done sending" but can still receive) — Chapter 97's close semantics, channel edition; CLOSE ends both directions. **Data after close**: data already handed to the application remains consumable, but no more WINDOW_ADJUST is produced (the return mechanism dies with the channel). **Request capability queried independently** (CanSendRequest/CanReceiveRequest) — shell/exec/x11 requests on a channel and the data plane are two orthogonal capability axes. **No built-in request token**: each channel may independently compose an `xsshreplyqueue` (the want-reply correlation queue) — capacity and storage decided by the application per concurrency; idle channels pay zero FIFO cost.

### Data transactions: receive-commit separated from consumption

On data: `DataReceiveCommit` (committed **before** the application takes the view — window accounting) → application consumes → `DataConsume` (quota Pending) → the window transaction returns. The send side is symmetric: `SendLimit` bounds length → packet reliably queued → `DataSendCommit`. **Commit-consumption separation is backpressure made mechanical**: not consuming = not returning = the remote's window exhausts = the remote stops sending — Chapter 110's pause/resume, window edition, executed automatically by the protocol.

### The channel set: dynamic ownership

`ssh_channels` stores address-stable `xsshchannel` entries by **local channel id** (each = core + optional dynamic I/O + reply FIFO): **created on demand** (mapping nodes only on Open/Accept — an empty connection costs zero, an empty channel has zero fixed buffers); **stable addresses** (safe for borrowing until deletion — connection sessions/asynchronous waits/application state hang pointers); **hard caps** (default 1024 channels / 64 pending replies per channel / 2 MiB window / 32 KiB packet / 2 MiB + 2 MiB send/receive budgets — tune per workload, never rely on unbounded growth); **directly usable as a resolveproc** (O(1) routing of recipient → channel).

## Examples

### First complete program: the window trio

The program below is from `examples/channel_window` — direct operation of the flow-control state:

```embed path="extlibs/xssh/examples/channel_window/main.c" title="extlibs/xssh/examples/channel_window/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/channel_window/main.c -lws2_32 -liphlpapi
（输出 chunk=32768 remaining=100000 形态的发送限额自检结果）
```

**What just happened.** (1) `xrtSshChannelWindowInit(&窗, 100000, 32768, 100000, 32768, 50000)` (window) takes six parameters: send window / send max-packet / receive window / receive max-packet / return threshold — local and remote capabilities declared at once. (2) `xrtSshChannelSendLimit` takes the first send quota — `min(远端窗口 100000, 远端 max-packet 32768)=32768` (min of remote window 100000 and remote max-packet 32768): **max-packet is the per-message cap, the window the total**; the smaller of the two decides a single send's size. (3) Printing chunk and remaining window — the send loop's "how much may go at once" is driven by this query. The real sequence (send→commit→adjust→consume→adjust-ready→adjust-commit) is covered path by path in the tests; this sample verifies the deterministic shape of the quota computation.

### Second complete program: the channel core's open transaction

The second program is from `examples/channel_core` — open and the budget declaration:

```embed path="extlibs/xssh/examples/channel_core/main.c" title="extlibs/xssh/examples/channel_core/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/channel_core/main.c -lws2_32 -liphlpapi
（输出 channel 结构尺寸、本地编号与阶段的自检结果）
```

**What just happened.** (1) The init entrance (parameters include the local recipient start, initial window/max-packet, request budgets) — a channel core on the stack; the sizeof output (the same zero-burden statement as Chapters 118–121). (2) Printing `local` (the local number) and `phase` (the initial stage) — the core's public state is exactly the input for routing and scheduling. (3) The open transaction's shape: `OpenInit` → build the wire CHANNEL_OPEN → after commit wait for confirmation → `ConfirmationCommit` enters OPEN — **the data plane is open only in OPEN**; the stage query (phase) is the check before sending. Companion samples: `channels` (the set and stable addresses), `channel_request` (shell/exec requests), `channel_pty` (terminal negotiation), `channel_io` (dynamic I/O staging), `channel_state`/`channel_message` (state and messages), `reply_queue` (the want-reply FIFO).

## Contracts

- **Windows allocation-free**: no payload stored, no fixed buffers — views go straight to consumers / ring buffers / zero-copy queues.
- **Send quota**: `SendLimit=min(远端窗口, 远端 max-packet)` (remote window, remote max-packet); `SendCommit` only after reliable queuing; wraparound is a protocol error.
- **Two-phase return**: consume→Pending→(threshold/exhaustion)→AdjustLimit→AdjustCommit **after reliable queuing** — failure never enlarges the window early.
- **Dynamic capacity**: Grant adds receive quota; zero-window startup; already-consumed quota is not re-spent.
- **64-bit accounting**: internal bytes counted in uint64 without wrapping; the wire window strictly uint32 (RFC).
- **open transaction**: OpenInit→wire commit→Confirmation/FailureCommit; Accept/Reject symmetric — each step advances only after a reliable commit.
- **Stage discipline**: the data plane open only in OPEN; EOF closes one direction; CLOSE both; after close, existing data is consumable but no more ADJUST.
- **Requests orthogonal**: CanSend/ReceiveRequest queried independently; tokens composed on demand from replyqueue (idle costs zero).
- **The set**: created on demand, addresses stable (until deletion), hard caps (default 1024/64/2MiB/32KiB/2MiB+2MiB), O(1) routing.
- **No border-crossing**: the core touches no transport; the set builds no socket/Engine/future/queue — layers meet only through explicit calls.

## Pitfalls

### Pitfall 1: consuming data without returning the window

Symptom: after one batch the peer "sticks" — its window exhausted waiting for your ADJUST, and you consumed without triggering the return logic.

Cause: Consume only accounts, it doesn't return — returning waits for a threshold or exhaustion trigger (you may also poll AdjustReady actively). Forget this layer and flow control fails on one side.

```c bad
while ( receive_commit(&Ch, Data) == OK ) {
	consume_app(Data);
	xrtSshChannelCoreDataConsume(&Ch->Core, Data.Size);
	/* AdjustReady never checked: the remote's window withers */
}
```

```c good
while ( ... ) {
	consume_app(Data);
	xrtSshChannelCoreDataConsume(&Ch->Core, Data.Size);
	if ( xrtSshChannelCoreAdjustReady(&Ch->Core) ) {
		n = xrtSshChannelCoreAdjustLimit(&Ch->Core);
		if ( send_window_adjust(&Conn, Ch, n) ) {
			xrtSshChannelCoreAdjustSendCommit(&Ch->Core, n);
		}
	}
}
```

### Pitfall 2: window return committed before queuing (order reversed)

Symptom: occasional over-sends by the peer — the local window was "enlarged" but the ADJUST was lost in a queue failure.

Cause: the contract is explicit two-phase — AdjustCommit only **after the message is reliably queued**. Commit-first then queue failure = window already enlarged, message never sent — the peer sends by the enlarged window, local validation rejects, protocol scrambling.

```c bad
AdjustCommit(&Ch, n);      /* enlarge first */
if ( !send_adjust(n) ) { /* failure: the window cannot go back */ }
```

```c good
if ( send_window_adjust(&Conn, Ch, n) ) {   /* queue reliably first */
	xrtSshChannelCoreAdjustSendCommit(&Ch->Core, n);  /* account only on success */
}
```

### Pitfall 3: saving a channel pointer after the set deleted it

Symptom: accessing the channel after Remove/Discard crashes — a stable address's "stable" has a boundary.

Cause: addresses are stable **until the corresponding entry is deleted** — after the set clears (connection close) everything is invalid. Keeping a channel pointer across connections is out-of-design usage.

```c bad
g_SavedChannel = xrtSshChannelsGet(&Set, Id);   /* saved across connections */
/* still using g_SavedChannel after the connection closed and the set cleared */
```

```c good
/* a channel's lifecycle follows the connection: hang state in the channel's application area (managed by the set),
   finish cleanup in the connection-close callback - leave no dangling references */
```

## Exercises

### Basic: a quota sweep

With window 100000 / max-packet 32768, simulate sending: loop SendLimit→deduct→until the window exhausts; inject an ADJUST and recover. Acceptance criteria: each quota's min correct; exhaustion and recovery paths clear; an injected wraparound rejected.

### Advanced: two-phase return timing

Simulate both paths "consume a batch→threshold triggers→queue succeeds/fails" — verify that Commit advances only after the queue succeeds (on failure Pending is kept, retryable). Acceptance criteria: both paths' Pending/window internal states match the contract.

### Challenge: three-channel fair multiplexing

Open three channels on one connection (different windows); A's peer consumes slowly (big ADJUST delay), B/C normal — pump data concurrently. Acceptance criteria: B/C throughput unaffected by A's window exhaustion (channel-level isolation); A continues after recovery; the set's routing error-free throughout (recipient O(1)).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| The trio | window (flow control) / core (open + states + data transactions) / channels (the set) |
| Send quota | min(remote window, remote max-packet); Commit after queuing; wraparound is an error |
| Two-phase return | Consume→Pending→threshold→queue→Commit — failure never enlarges the window early |
| Zero-window startup | initial 0 window + lazy Grant — build the channel first, buffer later |
| Accounting width | internal uint64 no wrap; wire uint32 strictly RFC |
| open transaction | OpenInit→commit→Confirm/Failure; Accept/Reject symmetric |
| EOF/CLOSE | EOF one-way; CLOSE bidirectional; after close consumable but no more ADJUST |
| Requests orthogonal | capability queried independently; tokens composed on demand from replyqueue |
| The set | created on demand / addresses stable (until deletion) / hard caps default 1024·64·2MiB·32KiB |
| Flow-control essence | not consuming = not returning = the remote stops sending — pause/resume automated by protocol |
