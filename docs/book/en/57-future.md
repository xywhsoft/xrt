---
num: 57
slug: future
title: Future / Promise and Combinators
volume: 卷六 进程与并发
type: practice
lead: The one-shot asynchronous result with split write/read ends — the resolved/failed/cancelled states, then-style continuations, combinators, and the wait family.
api: future, cancel
---

## Orientation

The Future is the standard abstraction of the **one-shot asynchronous result** (the shared result currency of Chapter 47's async files, Chapter 58's executor, and Volume 7's network IO): `xrtPromiseCreate` creates a pair of roles at once — the **Promise write end** (the producer publishes a value or a failure) and the **Future read end** (the consumer waits and reads); a four-state endgame (`XFUTURE_RESOLVED` success / `XFUTURE_FAILED` failure / `XFUTURE_CANCELLED` cancellation / `XFUTURE_CLOSED` valueless close); **continuations** (then-style — the source Future's result transforms through a callback and publishes a new value through an output Promise — the C form of async pipelines); **combinators** (the First family of the future_combine sample — among several Futures, the first to finish wins); and the wait family (Wait/WaitFor/WaitUntil plus the `xrtFutureWatchInit`/`WatchAdd` callbacks). The cancellation token (Chapter 53) is reachable from both ends — the Future is a first-class citizen of the cancellation tree.

## Introduction

Chapter 47's async files already showed Future's usage surface (get a Future, Wait, Value) — this chapter completes its **full model**. Three questions lead. Question one: who delivers the async operation's result — the operator holds the Promise (write end), the caller holds the Future (read end): **a pair of separated handles naturally matches the "producer/consumer" roles**; the write end cannot mistakenly wait on itself, the read end cannot forge a result. Question two: how are several async steps strung together — check cache (miss) → query database → write back → return, four async steps: continuations make each step's output the next step's input (`result: 105`'s 100→+5 is two transformations), the main line waits only for the final result. Question three: three data sources, use whichever is fastest — a combinator (First) fuses three Futures into one "first to arrive" Future.

The three questions map to Future's three capability layers: the **delivery model** (the Promise/Future pair), **composition** (continuations and combinators), **waiting** (the family + Watch). Chapter 47's file async, Chapter 58's executor tasks, Volume 7's network IO — all use Future as the result currency; this chapter teaches the issuance rules of that currency.

## Concepts

### A pair of roles and the three-state endgame

```diagram flow
- Create: xrtPromiseCreate(&Future, parent cancel token) -> write end + read end
- Publish: xrtPromiseResolve (value) / Reject (failure + error) / Close (valueless finish)
- Cancel: Cancel from either end - the Cancelled endgame (Chapter 53 token linkage)
- Read: State queries / the Wait family waits / Value borrows the value / Error borrows the error
```

**One-shot** is the Future's identity: the endgame happens exactly once — repeated publishing is refused, the endgame is immutable (Resolved never becomes Rejected).**Endgame semantics** land Chapter 53's "cancellation ≠ failure" in full: Resolved (value delivered), FAILED (failure — `xrtPromiseReject` carries Chapter 4's error object), CANCELLED (control-flow abandonment), CLOSED (the write end's valueless finish — the polite close when the writer destroys without a result). The `xfutureresult` struct uniformly carries the three states + value/error — the reading side sees the whole ending in one structure.

### Continuations: the C form of then

The continuation callback's signature (the future_continue sample): `(源结果, 输出Promise, 数据)` (source result, output Promise, data) — read the source, compute the new value, publish through the output Promise.**Chained composition**: one callback plus one output per link — a four-step async pipeline is four links; the main line waits only at the chain's tail.**Against callback hell**: nested callbacks hide "steps" in indentation; the continuation chain lays the steps in a line — errors propagate along the chain (one link Rejected puts downstream in the same state), cancellation takes effect along the chain (the token threads it).**Execution timing**: which thread a continuation runs on is decided by where it is registered (scheduler-side registration runs on the scheduler's thread, pool-side on the pool's) — composition's determinism comes from registration's determinism.

### Combinators: First and friends

The `future_combine` sample (`winner[1] = 22` in the output): several Futures combined into "the first to finish" — the winner's index and value are both available. First's classic scenes: multi-replica reads (use whichever replica is fastest), timeout races (data versus timer — first arrival wins; the timeout version is WaitFor's composed form), redundant requests (three concurrent paths, take the first success). The All family (wait for all) appears in Chapter 59's task-group join waits — this chapter focuses on First.

### The wait family and Watch

| Shape | Entrance | Fits |
| --- | --- | --- |
| Blocking wait | `xrtFutureWait` | simple joins (Chapter 47's usage) |
| Timed wait | WaitFor / WaitUntil | timeout control (the deadline family) |
| Callback notification | `xrtFutureWatchInit`/`WatchAdd` | event loops (non-blocking - callback on completion) |
| Coroutine wait | await inside a scheduler coroutine (Chapter 55's Park/Wake) | straight-line coroutine code |

Watch's discipline matches the library-wide callback rule: light work (Chapters 37/47) — heavy processing continues in the context after wakeup.**Reference balancing**: Future and Promise each Ref/Destroy; hand the reference count of what you store to the structure holding it (Chapter 5's discipline).

### Cancellation's three-way linkage

Both the Promise side and the Future side expose a CancelToken (`xrtPromiseCancelToken`/`xrtFutureCancelToken`) — the cancellation tree passes through the pair; `xrtFutureCancel` is the direct button. The linkage shape: a parent token (Chapter 53's tree) → the pParentCancel parameter at creation — a parent cancellation ends the Future Cancelled (no manual forwarding needed); the read end's Cancel notifies the write end in reverse (the operator aborts work in the CancelToken's Watch — Chapter 53's checkpoint discipline).

## Examples

### Complete program: one asynchronous delivery

From the repository sample `examples/concurrency/future/main.c`:

```embed path="examples/concurrency/future/main.c" title="examples/concurrency/future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/future/main.c -lws2_32 -liphlpapi
future value: 42
```

**What just happened.** (1) `xrtPromiseCreate(&pFuture, NULL)` — one creation of both ends (NULL means no parent token — attach one for Chapter 53's tree linkage). (2) The Promise end publishes the value 42 — the one-shot endgame Resolved. (3) The Future end Waits for completion; `xrtFutureValue` **borrow-reads** the value (valid until the next operation — to hold it, Ref/copy it yourself). (4) `future value: 42` prints — the complete delivery chain of publish → wait → read.**This 43-line loop is the atomic form of all Future usage** — continuations, combinators, and pool-task result channels are all compositions of it.

### Complete program: a continuation chain

From `examples/concurrency/future_continue/main.c` — then-style, two links:

```embed path="examples/concurrency/future_continue/main.c" title="examples/concurrency/future_continue/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/future_continue/main.c -lws2_32 -liphlpapi
result: 105
```

**What just happened.** (1) The continuation callback `addFive(源结果, 输出Promise, 数据)` (source result, output Promise, data) — reads the source Future's value, adds five, publishes through the output Promise: **input and output are both asynchronous**, and the transformation itself is a link in the async chain. (2) Waiting at the chain's tail yields `result: 105` (100→+5) — the main line has no nesting, no intermediate waits, one straight line to the final value. (3) Failure propagation: if the source were Rejected, the continuation by convention does not publish (or forwards the error) — the error runs down the chain to the tail (when writing continuations, handle source failure explicitly — the pitfalls hold an example). (4) The future_combine sample is the combinator edition: among several sources, `winner[1]` (22 at index 1) arrives first — first-finisher wins, index and value both in hand.

## Contracts

- **One-shot**: the endgame exactly once, immutable; repeated publishing refused.
- **Three-state endgame**: Resolved/Rejected/Cancelled — uniformly carried by `xfutureresult`; cancellation ≠ failure (Chapter 53).
- **Borrowed reads**: Value/Error are borrows — use immediately or hold your own copy; used up before Destroy.
- **Continuation discipline**: handle the source's three states explicitly (forward failure or degrade — never swallow, never crash); the output must publish (else the downstream hangs).
- **Reference balancing**: both ends Ref/Destroy each; references stored into structures are managed by the structure (Chapter 5's ownership discipline).
- **Cancellation linkage**: attaching a parent token at creation links automatically; both ends expose CancelToken; the operator aborts work in the token's Watch.

## Pitfalls

### Pitfall 1: the continuation swallows the source's failure

Symptom: the chain's tail never gets a result (hangs) or receives a zero value as success — an upstream failure silently dropped by the continuation.

Cause: the continuation callback wrote only the success branch — the source's Rejected/CANCELLED was never forwarded to the output Promise.

```c bad
static void step(const xfutureresult* pIn, xpromise* pOut, ptr pData)
{
	if ( pIn->State == XFUTURE_RESOLVED ) {
		xrtPromiseResolve(pOut, Transform(pIn));   /* success only */
	}
	/* the failure branch is missing - pOut never publishes - the downstream hangs */
}
```

```c good
static void step(const xfutureresult* pIn, xpromise* pOut, ptr pData)
{
	if ( pIn->State == XFUTURE_RESOLVED ) {
		xrtPromiseResolve(pOut, Transform(pIn));
	} else {
		ForwardTerminal(pOut, pIn);   /* Reject or Cancel per the source State - the chain stays unbroken */
	}
}
```

### Pitfall 2: Value reads a stale or dangling value

Symptom: reading the previous value, or garbage from before completion; occasional crashes (the value pointing into freed memory).

Cause: reading the value before the endgame (the one-shot result not ready), or storing the borrowed value past the Future's Destroy/next operation.

```c bad
ptr Value = xrtFutureValue(pFuture);   /* no wait - reading before the endgame is undefined */
Save(Value);                            /* storing the borrowed pointer - a dangling window */
```

```c good
if ( xrtFutureWait(pFuture) != XWAIT_OK ) { return false; }
ptr Value = xrtFutureValue(pFuture);   /* borrowed after the endgame */
Use(Value);                             /* use immediately - or copy and hold */
```

## Exercises

### Basic: a four-state machine

The Promise respectively Resolve a value / Reject a failure / Cancel / Close — the read end Waits then prints State and value/error — all four states verified once (the read end's State and value/error itemized).

### Advanced: a four-link pipeline

Check cache → on miss query DB → transform → write back: a four-link continuation chain (each link mocks an async delay) — wait at the chain's tail for the final value; inject a failure in a middle link and verify propagation to the tail.

### Challenge: a multi-replica reader

Three "replica" Futures (each mocking a different delay) + a First combinator — take the first returner; cancel the losers (token linkage). Acceptance criteria: the success path yields the fastest replica's value, the failure path (all fail) an aggregate error; the losers' resources drained (Chapter 6's statistics verify zero leaks); the cancellation-tree propagation auditable.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Pair of roles | Promise write end (publishes) / Future read end (waits and reads) - Create pairs them at once |
| Three-state endgame | Resolved/Rejected/CANCELLED; xfutureresult carries them uniformly; once, immutable |
| Continuations | callback(source result, output Promise, data) - forward failures explicitly, always publish the output |
| Combinators | First - first arrival wins (winner index + value together); All is Chapter 59's task group |
| Wait family | Wait/For/Until timed + Watch zero-allocation callback + coroutine await (Chapter 55) |
| Cancellation linkage | attach a parent token at creation for auto-linkage; both ends expose CancelToken |
| Borrowing discipline | Value/Error are borrows - use immediately or copy and hold, used up before Destroy |
