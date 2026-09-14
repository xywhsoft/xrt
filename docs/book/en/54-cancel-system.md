---
num: 54
slug: cancel-system
title: The Cancellation System: Tokens, Propagation, and Final States
volume: 卷六 进程与并发
type: practice
lead: The cancellation token's request-observe-propagate trio, parent-child trees with reference counting, and the unified posture of cancellable waiting — the foundation of graceful stop.
api: cancel, channel
---

## Orientation

The problem cancellation solves: **how to make "waiting/running" work stop**. XRT's answer is the **cancellation token** (`xcancel`): `xrtCancelRequest` initiates cancellation (returns true only the first time, firing the listener), `xrtCancelRequested` queries whether the token or any ancestor was hit, `xrtCancelWatch` registers an observer (callback-style wakeup — executed synchronously at most once, `xrtCancelTriggered` queries the listener hit); `xrtCancelChild` spawns a child token forming a **cancellation tree** (a parent's cancellation propagates to all descendants); `xrtCancelRef` reference counting lets multiple holders share one token. This chapter positions it as "the cancellation-system chapter" — the token is the foundation, and the cancellation interfaces of Chapters 57~60's Channels/Futures/task groups all stand on it.

## Introduction

After the service receives the stop signal (Chapter 49), the main loop halts — but five worker threads are still blocked on their Channel receives, three Futures are unfinished, two connections are still draining. Without a cancellation system the options are: add timeout polling to every wait (high latency), a global flag plus checks everywhere (invasively rewriting every wait point), or simply exit the process (all in-flight work lost). All three have been seen; all three hurt.

The cancellation token's solution: create a root token hooked to the stop signal; every worker holds it (or a child of it); **cancellable waiting** (like `xrtChannelRecvCancel`) fuses "wait for data" and "wait for cancellation" into one wait — whichever arrives first wins. The stop signal fires the root token → whole-tree propagation → every blocked wait wakes simultaneously with `XWAIT_CANCELLED` → each walks its drain path.**One request, the whole tree wakes, each drains itself** — that is the cancellation system's entire promise.

## Concepts

### The token trio: request, observe, propagate

```diagram flow
- Create: xrtCancelCreate() -> token; Ref/Destroy share it by reference counting
- Request: xrtCancelRequest(token) -> marks hit (true only the first time; one-way, irrevocable)
- Observe: Requested polls the token/ancestor chain / Watch callback (at most once, synchronous) + Triggered polls the listener
- Propagate: CancelChild(parent) -> child token - a parent hit auto-propagates to children; children never affect the parent
```

Three design points.**Cancellation is one-way and irreversible** — once requested it takes effect; there is no "cancel the cancellation"; this simplification buys a lock-free fast path (the Triggered check is one atomic read).**Request and observation are separate** — Request only marks; who gets woken on which wait depends on who registered observation; the code that initiates cancellation and the code that responds to it need not know each other.**The listener is one-shot** — Watch's callback runs synchronously at most once (inside the callback: set a flag/wake; further checking polls Triggered) — the callback never repeats and needs no reentrancy guards.

### The cancellation tree: structured propagation

`xrtCancelChild(父)` (parent) spawns a child token: parent hit → all children hit (propagation is unidirectional); child cancelled → parent and siblings unaffected (local abandonment). The tree's shape mirrors **the shape of the calls**: one request handler spawns its own child token — that request's timeout cancels only that request's work (local); the service stop cancels the root (global). Chapter 60's structured concurrency's "scoped cancellation" is exactly this tree's regularized usage — enter a scope, a token is born; leave the scope, the subtree is fully reaped.

### Cancellable waiting: the unified waiting posture

XRT's concurrency waiting interfaces follow one shape: **wait object + optional token**. `xrtChannelRecvCancel(通道, ..., 令牌)` (channel, ..., token) waits for data or cancellation; the Future wait family accepts tokens the same way; so do task-group join waits. The returned `xwaitresult` uniformly reports the wait's outcome — `XWAIT_OK` (got it), `XWAIT_CANCELLED` (cancellation hit), `XWAIT_TIMEOUT` (deadline).**One return-code enum runs through every wait** — the seed Chapter 42 planted sprouts across the whole concurrency system. To write your own cancellable wait (a custom blocking structure interfacing a token), register a wakeup callback with Watch — the cancel sample's `stopWork` is the shape template.

### Final-state semantics: cancellation is not failure

`XWAIT_CANCELLED` and errors (Chapter 4) are two dimensions: cancellation is a **control-flow decision** ("we decided not to do it"), an error is an **operation failure** ("we did it and it failed"). The price of confusing them: reporting cancellation as error (monitoring false-alarms failure rates), swallowing errors as cancellation (real failures silenced). Chapter 60's task groups will merge the two into a complete final-state classification (success/failure/cancelled); this chapter first establishes the idea "cancellation ≠ failure".

## Examples

### Complete program: propagation and observers

From the repository sample `examples/concurrency/cancel/main.c`:

```embed path="examples/concurrency/cancel/main.c" title="examples/concurrency/cancel/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/cancel/main.c -lws2_32 -liphlpapi
operation stopped: yes
watch-triggered=1
```

**What just happened.** (1) `xrtCancelCreate` builds a token, `xrtCancelChild(父)` (parent) spawns a child — the observing side's Requested walks the ancestor chain (on a child token it asks "have you or any ancestor been cancelled"), propagation needing no explicit notification. (2) The working side uses both observation postures: polling `Requested` (the verdict behind `operation stopped: yes` — ancestor chain included) and registering a callback with `xrtCancelWatch` (`stopWork` sets a stop flag — **observer callbacks only wake or mark**, in line with this volume's "light callbacks" discipline; the callback fires at most once, then Triggered polling takes over); the `watch-triggered=1` assertion checks the listener hit. (3) `xrtCancelRef/Destroy` reference balancing — multiple holders sharing a token each Ref and each Destroy. (4) The stop path completes: Request → propagate → observer hit → work drains — the cancellation system's four-beat rhythm.

### Complete program: a cancellable channel receive

From `examples/concurrency/channel_cancel/main.c` — the standard cure for blocking on a channel:

```embed path="examples/concurrency/channel_cancel/main.c" title="examples/concurrency/channel_cancel/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/channel_cancel/main.c -lws2_32 -liphlpapi
cancelled: yes
```

**What just happened.** (1) The receiving thread waits on `xrtChannelRecvCancel` — **one wait watching two events at once** (data arrives / cancellation hits), first to arrive wins; this is not polling — it is kernel-level multiplexed waiting. (2) After the main thread fires Request, the receive returns immediately with `XWAIT_CANCELLED` — the source of the `cancelled: yes` verdict; no risk of lost data (cancellation arriving first just means "never got the data"). (3) Against the "timeout polling" scheme: RecvCancel has no latency (cancellation wakes instantly), no spinning (no CPU burned), no invasion (one change at the wait site). The deadline sample is the same idea's sibling — `xrtDeadlineAfter/Expired` combined with a cancellation token makes a three-way wait of "data, or cancellation, or timeout" (the standard complete form of concurrent waiting).

## Contracts

- **One-way and irreversible**: Request takes effect immediately (true only the first time), no revocation; Requested includes the ancestor chain — propagation is query.
- **Propagation direction**: parent→child one-way; a child's cancellation leaves parent and siblings untouched — the semantic basis of local abandonment.
- **Light observation**: the Watch callback runs synchronously at most once and only wakes/marks; Triggered polling takes over afterward — heavy work drains on the waiting side.
- **Unified waiting**: wait-family interfaces = wait object + optional token; the outcome via `xwaitresult` (OK/CANCELLED/TIMEOUT).
- **Cancellation ≠ failure**: control-flow decision and operation failure are two dimensions — report and count them separately (Chapter 60's final-state classification).
- **Reference balancing**: multiple holders Ref/Destroy each; the token tree closes with its holders' lifecycles.

### From examples to engineering: three hosts of cancellation

**Service shutdown** (Chapter 49's signal chapter, second half): the root token hooked to the stop signal → every worker's waits converted to cancellable form (RecvCancel/Future with token) → the stop signal Requests the root → the whole tree wakes → each drains → the main thread joins (Wait on every thread) and exits. Shutdown's "wait in-flight" deadline (Chapter 49's five-second convention) falls out naturally in the token system: after Request, join with WaitFor under a deadline, force-quit on timeout.**Request timeout**: each request handler spawns a child token (`CancelChild(根)` (root)) hooked to the request deadline — one request's timeout cancels only its own work tree, root and siblings unaware; Chapter 65's TCP AcceptWait deadline is its network edition.**User cancellation**: the CLI's Ctrl+C, the UI button — a Watch callback translates the UI event into a Request; the entire implementation of "the cancel button" is one line of Request.

### The cancellation tree and scopes: a rehearsal of structured concurrency

The cancellation tree's most powerful use is **scope binding**: a stretch of concurrent work spawns a token at its start and destroys it at its end — the token's lifecycle coincides with the code block, and the semantics "everything in the block either all completes or all cancels" is guaranteed by the tree structure. Chapter 60's structured concurrency regularizes this usage (task groups, scoped cancellation, final-state classification), but the primitives it depends on are all here — **first learn to plant the tree (Child), then to prune it (scopes)**. Two preparatory disciplines: child tokens are created and destroyed with the work (Ref balancing — a leaked token keeps Triggered forever false); before the scope's exit, all child work must have landed ("cancellation requested" is not "work stopped" — joining is half of draining).

### A design view: the "checkpoint economics" of cancellation

Cancellation's promptness depends on checkpoint density — and density has a cost (each Requested is an atomic read, cheap but cumulative; logic sliced by checkpoints loses readability). The economic balancing act: **layered checking** — coarse inter-segment checks for millisecond responsiveness, stepping checks on critical paths (inside loops), pure compute-dense stretches broken by Watch async interruption (where the platform supports it) or segmented self-checks; **the caller decides density** — library functions expose cancellable versions (the RecvCancel shape) rather than secretly checking inside — the caller knows the latency budget, the library does not; **measure to verify** — cancellation latency (Request to actual work stop) as an observable metric into the logs (Chapter 50's fields), drift shows immediately. The three together turn "how fast can we stop" from voodoo into a designable, measurable, regressible engineering property.

## Pitfalls

### Pitfall 1: cancellation checks only at the loop head

Symptom: after cancellation the work "still finishes the current big stretch" before stopping — tens of seconds late; or never stops at all (no checkpoints inside a long operation).

Cause: cancellation is **cooperative** — it only marks and wakes; "stopping" relies on the work's code actively checking Triggered at checkpoints; checkpoints too sparse equal no checkpoints.

```c bad
while ( running ) {
	ProcessOneBigChunk();   /* a 30-second stretch - cancellation waits it out */
	if ( xrtCancelRequested(pToken) ) break;   /* checkpoint only at the loop head */
}
```

```c good
while ( running ) {
	for ( int i = 0; i < ChunkSteps; ++i ) {
		ProcessStep(i);
		if ( xrtCancelRequested(pToken) ) {   /* step-level checkpoint */
			goto Cleanup;
		}
	}
}
Cleanup:
	ReleasePartialWork();   /* drain the partial work */
```

### Pitfall 2: reporting cancellation as an error

Symptom: every release (normally cancelling a batch of in-flight work) makes monitoring report "error rate spike"; rollbacks and faults mixed into one alert channel.

Cause: `XWAIT_CANCELLED` shares the error reporting path — a control-flow event counted as a fault.

```c bad
if ( xrtChannelRecvCancel(&Ch, ..., pToken) != XWAIT_OK ) {
	ReportError("recv failed");   /* cancellation lands in errors - a false alarm */
}
```

```c good
xwaitresult R = xrtChannelRecvCancel(&Ch, ..., pToken);
if ( R == XWAIT_OK ) { Handle(item); }
else if ( R == XWAIT_CANCELLED ) { LogInfo("recv cancelled"); /* control flow */ }
else { ReportError("recv failed"); /* real failure */ }
```

## Exercises

### Basic: verify the propagation tree

Build a three-level token tree (root→middle→leaf); Request the root and assert all three levels' Requested hit; then Request the middle and assert only middle and leaf hit, root unaffected — both faces of the propagation direction verified once each.

### Advanced: the three-way wait

Data, cancellation, timeout in a three-way fusion: RecvCancel with a deadline (or the Watch+DeadlineAfter combination) — trigger each of the three outcomes once and assert the return codes.

### Challenge: a stoppable batch job

A long batch task: step-level checkpoints + partial-progress saving (recording the interruption point) + resumable from the breakpoint after cancellation. Acceptance criteria: cancellation at any moment stops within 10 seconds; progress intact (rerun skips completed segments); the cancellation reason (which token, why) into structured logs.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Trio | Request (true only the first time) / Requested polling (ancestors included) / Watch listener (at most once) |
| Propagation | CancelChild parent→child one-way; a child's cancellation is local abandonment |
| References | Ref/Destroy balanced across holders - the same discipline as value trees |
| Unified waiting | wait object + optional token → xwaitresult (OK/CANCELLED/TIMEOUT) |
| Checkpoints | cooperative - step-level Requested checks; checkpoints embedded in long operations |
| Semantics | cancellation = control-flow decision ≠ error = operation failure - report separately |
| Reason | Request carries a reason → readable by observers - diagnostic fields propagate with the tree |
