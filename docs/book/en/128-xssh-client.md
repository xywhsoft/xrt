---
num: 128
slug: xssh-client
title: SSH (Part 7): The Client Runtime
volume: 卷十一 其他扩展库 · 卷十一收官
type: practice
lead: A client composition with no hidden Engine, the ReadyTimeout whole-journey budget, host-trust and authentication as event interactions, Drain semantics and the Packet escape hatch — the final assembly of seven chapters' parts; Volume 11 concludes.
api: xssh-ssh_client, xssh-ssh_client_core, xssh-ssh_session_stream
---

## Orientation

The SSH series concludes — the previous six chapters' parts are finally assembled here. `ssh_client` composes core/stream-session/dynamic-channels over **a caller-provided `xnetstream`**: **it creates no hidden Engine, blocks no Worker, pre-allocates no channels or message buffers, and introduces no second state machine** (four no's — SSH's edition of "no second implementation"). Connection lifecycle: after `ClientInit`, the event table and data pointer go straight to `xrtNetStreamConnect` (or, on an already-built stream, `ClientAttach`) — once TCP opens, `ReadyTimeout` (default 30 seconds) uniformly bounds version exchange → KEX → host trust → authentication; **the Ready event is the sole publication point of SSH-usable state**. Three interaction points: host trust (rejected by default — after DEFER, the HostKey event hands over the decision), authentication (the Authenticate event when a provider returns NEED_MORE), unknown protocol extensions (the Packet callback retains full handling capability). The Dial convenience layer (DNS/Happy-Eyeballs/timeout/cancellation) is subject to independent trimming — proxies and custom transports go through Attach, unconstrained.

## Introduction

The hard part of a "final assembly layer" is **closing responsibilities**: all connection state (version/KEX/authentication/channels/forwarding) has its owner among the six chapters' parts; the client strings them into one event-driven whole — but each layer's behavioral boundary is unchanged (the core is still that core, the window still that window). The closing glue is the **event table** (the client's events configuration struct): Ready/Error/Close/HostKey/Authenticate/Data/Drain/Global/Packet — the application's entire interaction surface; isomorphic to Chapter 103's HTTP server event chain.

Backpressure's **two-layer chaining** deserves a first look: when the TCP queue returns AGAIN, the complete packet of the encryption remains solely held by transport while new SSH input is paused; the LowWater/Drain callbacks retry the internal packet first, and **only after the internal transaction has committed and the TCP queue has truly drained is the client Drain published** — new messages the application submits inside Drain never overtake the previously retained packet (an ordering guarantee — the correctness root of forwarding tunnels). OOM is equally precise: a failed internal DATA reservation holds the connection; after memory is freed, `PacketRetry` — "OOM never commits half a packet".

## Concepts

### Connection and the timeout budget

```diagram flow
- Assembly: ConfigInit + ClientInit -> the event table goes to NetStreamConnect (or Attach on an already-built stream)
- TCP opens: Worker binding (the channel/control-message scratch only now joins the buffer pool)
- ReadyTimeout (default 30s, microseconds) covers: version exchange -> KEX -> host trust -> authentication
- Ready: SSH usable - the starting point for channels/forwarding
- Error/timeout: structured errors (XSSH_ERROR_TIMEOUT/XERR_TIMEOUT/domain) reach Error/Close/all pending Futures together
- TCP side: DNS/connect deadline belongs to xnetdialconfig.Timeout (independently controlled)
```

### The three interaction points

- **Host trust**: **rejected** by default — after the core's HostKey returns DEFER, the application gets the decision moment via the `HostKey` event, answering with `xrtSshClientHostKeyAccept/Reject` (Chapters 121/122's two-step trust in its client-side interactive form — the event is not a synchronous judgment inside a callback; you may query the store, ask the user, verify out-of-band, and decide asynchronously).
- **Authentication**: a provider returning NEED_MORE triggers the `Authenticate` event — once credentials are ready (asked the user / fetched from the agent), `xrtSshClientContinue` proceeds (Chapters 123's methods' asynchronous mouth).
- **The Packet escape hatch**: the `Packet` callback runs before the underlying read transaction commits — full handling capability for unknown protocol extensions and custom channel types (Chapter 125's forwarding parse hangs exactly here) — **the client never swallows unknown messages**.

### The data and control-message paths

`xrtSshClientSend` is the payload fast path: it adds only SSH packet encoding + one ownership transfer into the TCP queue (zero transit copies). `xrtSshClientBuild` provides grow-from-zero, reusable, hard-capped contiguous scratch for mutable control messages (the building buffer for custom messages). Standard DATA/stderr first enter channel I/O staging; after committing, the `Data` event fires (the application zero-copy-inspects or reads explicitly). **Global replies**: `GlobalReplies/ReplyReserve`'s dynamic bounded FIFO paired with the Global event (token correlation — seen in Chapter 125).

### Channels and the lifecycle finish

`xrtSshClientChannelOpen` is **the base entrance for custom channel types**: the caller writes the type-specific fields; the client manages dynamic numbering/windows/reply correlation/commit/rollback — the classic session and direct-tcpip helpers are both built on it (**no second set of channel state maintained**). `ChannelFlush` builds fragments from the channel's send queue, bounded jointly by "remote window + max-packet + TCP backpressure" — the confluence of three budget layers. The finish: `OwnsChannel` validates ownership (preventing handing another connection's pointer to the send queue); Drain/Abort follow Chapter 109's same semantics.

## Examples

### First complete program: the no-hidden-runtime statement

The program below is from `examples/client` — the minimal client's resource shape:

```embed path="extlibs/xssh/examples/client/main.c" title="extlibs/xssh/examples/client/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/client/main.c -lws2_32 -liphlpapi
（输出初始状态与通道上限的自检结果）
```

**What just happened.** (1) `xrtSshClientConfigInit + xrtSshClientInit(&Client, &Config, NULL, NULL)` — a client on the stack; the `xrtSshClientState` initial state and `Channels.MaxChannels` (default 1024) printed. (2) The header comment is the design manifesto: "driven by an external Engine and Stream; creates no hidden runtime" — Chapter 120's transport core "no buffers", Chapter 121's KEX "no socket", here "no Engine" — **all seven layers zero-hidden**; the assembly's transparency is the accumulation of the parts' transparency. (3) `ClientClear` finishes — an unconnected client's cleanup is zero-burden. The real assembly (the event table into NetStreamConnect) is in the `client_session`/`client_core` samples.

### Second complete program: the Dial convenience-layer configuration

The second program is from `examples/client_dial` — the pre-connection budgets:

```embed path="extlibs/xssh/examples/client_dial/main.c" title="extlibs/xssh/examples/client_dial/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/client_dial/main.c -lws2_32 -liphlpapi
（输出拨号超时、回退延迟与客户端状态的配置自检结果）
```

**What just happened.** (1) `xrtNetDialConfigInit`'s `Timeout=10s`/`FallbackDelay=250ms` — the TCP-side budget (DNS/connecting/Happy-Eyeballs fallback), **independent at two levels** from `ReadyTimeout` (the SSH side): TCP 10 seconds + handshake-and-authentication 30 seconds; the total budget is clear and tunable (Chapter 98's two deadline kinds, SSH edition). (2) The Dial completion callback means only the TCP outcome — **SSH usability is still published only by Ready** (callback semantics not swapped). (3) Proxies/custom transports are not constrained by Dial — the `ClientAttach` path stays open (the composition mouth of Chapter 69's proxy tunnel + SSH). Companions: `client_core` (core + password-authentication assembly), `client_session` (the session stream), `client_future` (the Future form), `client_auth_ed25519` (public-key authentication), `client_pty` (terminal negotiation).

## Contracts

- **Four no's**: no hidden Engine / no blocking Workers / no pre-allocated channels or message buffers / no second state machine.
- **Connection assembly**: the event table goes to NetStreamConnect; an already-built stream does `ClientAttach` on its owning Worker; scratch binds the Worker buffer pool after TCP opens.
- **Two-level timeout**: ReadyTimeout (default 30s, zero disables) governs version→KEX→trust→authentication; Dial's Timeout governs DNS/connecting — independent.
- **Ready is unique**: SSH-usable state is published only by the Ready event; the Dial completion callback ≠ SSH usable.
- **Host rejected by default**: DEFER → HostKey event → Accept/Reject decided asynchronously.
- **Authentication's asynchronous mouth**: NEED_MORE → Authenticate event → Continue.
- **The Packet escape hatch**: runs before the read transaction commits; full capability for unknown extensions and custom channels; the client never swallows unknown messages.
- **Send paths**: Send = zero-transit fast path; Build = reusable scratch; three budget layers confluence in ChannelFlush.
- **Drain ordering**: internal packets retried first; published only after the transaction commits and the queue drains — new messages never overtake.
- **OOM precise**: never commits half a packet; reservation failure holds; after freeing, PacketRetry.
- **Global replies**: a dynamic bounded FIFO + the token-correlated Global event.

## Pitfalls

### Pitfall 1: treating Dial completion as SSH readiness

Symptom: opening a channel right in the Dial callback — an error or crash; SSH is still handshaking.

Cause: the Dial callback only means TCP is up; version/KEX/trust/authentication come after — **Ready is the usable signal**.

```c bad
on_dial_done(...) {
	xrtSshClientChannelOpen(Client, "session", ...);  /* mid-handshake: wrong */
}
```

```c good
on_dial_done(...) { /* TCP only - wait for Ready */ }
on_ready(...) {
	/* only here open channels / send forwarding requests */
	xrtSshClientChannelOpen(Client, "session", ...);
}
```

### Pitfall 2: forgetting to answer the HostKey event (hung to timeout)

Symptom: the connection sticks and finally ReadyTimeout — nobody Accepted/Rejected the event.

Cause: the HostKey event is a decision point that **must be answered** (the default rejection protects against "no trust policy configured"; it does not mean "the event may be ignored"). After querying the store / asking the user, be sure to answer.

```c bad
on_host_key(...) {
	audit_log(Key);   /* logged but never answered: the connection hangs to timeout */
}
```

```c good
on_host_key(...) {
	audit_log(Key);
	if ( known_hosts_check(Key) == TRUST ) {
		xrtSshClientHostKeyAccept(Client);
	} else {
		xrtSshClientHostKeyReject(Client);
	}
}
```

### Pitfall 3: misusing the channel budget defaults around READY

Symptom: at ten-thousand-concurrency targets, the 1024-channel cap silently intercepts — half the connections cannot open new channels; or the reverse, on embedded targets the untuned budgets squeeze memory.

Cause: defaults like `Channels.MaxChannels` are general-purpose values (Chapter 124's set contract: "tune per workload, never rely on unbounded growth") — deployment parameters are not the library's private business.

```c bad
xrtSshClientConfigInit(&Config);   /* all defaults into production */
```

```c good
xrtSshClientConfigInit(&Config);
Config.Channels.MaxChannels = my_model.channels;
Config.Channels.ReceiveWindow = my_model.window;
Config.GlobalReplyLimit = my_model.global_tokens;
/* declared explicitly per the workload model - the budget is a deployment contract */
```

## Exercises

### Basic: state-machine observation

Against a real SSH server (local sshd or a container), print the event sequence for a whole connection: Dial→(TCP)→version→KEX→HostKey→authentication→Ready→channel→Close. Acceptance criteria: the sequence matches this chapter's flow diagram; the two-level timeouts each trigger independently (short-value experiments).

### Advanced: asynchronous trust decisions

Inside the HostKey event, start an asynchronous verification (simulating an out-of-band check with 200 ms delay) then Accept — verifying the event model's asynchronous decision capability. Contrast with the synchronous immediate-reject path. Acceptance criteria: both paths' Ready/Close timing correct; the connection stays alive during the decision (no timeout).

### Challenge: a complete tunnel tool

The assembly exercise: Dial + password authentication + HostKey (known_hosts integration) + local forwarding (the `-L` shape) + 3 concurrent channels + graceful Drain — a usable mini ssh -L. Acceptance criteria: HTTP requests round-trip through the tunnel; three channels concurrent without interference; Ctrl-C triggers Drain and the whole chain exits cleanly (budget and reference reconciliation).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four no's | no hidden Engine / no blocking Workers / zero pre-allocation / no second state machine |
| Assembly | the event table to NetStreamConnect, or Attach on the owning Worker |
| Two-level timeout | ReadyTimeout (whole SSH journey, default 30s) independent of Dial Timeout |
| Ready is unique | SSH usable published only by Ready — Dial completion ≠ ready |
| Host trust | rejected by default; DEFER → HostKey event → Accept/Reject always answered |
| Authentication async | NEED_MORE → Authenticate → Continue |
| Packet escape hatch | runs before commit; unknown messages never swallowed — the custom-channel capability |
| Sending | Send zero-transit / Build scratch / Flush conflues three budgets |
| Drain ordering | internal packets first; published only when drained — new messages never overtake |
| Global replies | bounded FIFO + token for the Global event |
| Assembly map | 117 transport → 118 KEX → 119 trust → 120 authentication → 121 channels → 122 forwarding → 123 client |
