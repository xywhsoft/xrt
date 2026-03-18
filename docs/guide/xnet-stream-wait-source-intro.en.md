# xnet-v2 Stream Wait-Source Intro

> Goal: understand why the current network mainline pulls stream state into the unified wait-source / future / coroutine model instead of letting the program fragment into scattered callback-only logic.

[Back to Guides](README.en.md)

---

## 1. What a Stream Wait-Source Is

In the current `xnet-v2` mainline, a stream is not just a byte transport. It also exposes waitable states such as:

- readable
- writable
- drain
- close

These states turn “something happened on the connection” into “something the program can wait on”.

---

## 2. Why This Matters

Without a unified waiting model, network code usually becomes a mix of:

- callbacks
- flags
- condition variables
- polling

The wait-source layer exists to prevent that drift.

In the current XRT mainline, this means:

- stream state can be bridged into futures
- stream state can be consumed by coroutines
- timeout, deadline, and cancel can live on the same semantic chain

---

## 3. Main Waiting Surfaces

The key stream waiting surfaces are:

- readable
- writable
- drain
- close

Together they describe both the state of the connection and what the caller can safely do next.

---

## 4. Why Callback-Only Style Is No Longer Enough

Callbacks are not forbidden. The problem appears when the program also needs:

- timeout
- deadline
- cancel
- combined waiting
- coroutine orchestration

At that point, a unified waiting model is much easier to keep correct.

---

## 5. A Good Mental Model

The recommended mental model is:

- stream: the network connection object
- wait-source: a unified waiting entry
- future: the result carrier after waiting completes
- coroutine: a way to express the flow as sequential logic

---

## 6. Why This Fits Modern Network Programs Better

A modern network program needs more than read/write. It also needs:

- timeout control
- error propagation
- cancellation
- graceful shutdown
- cooperation with HTTP / WebSocket / TLS / AI call chains

The wait-source mainline exists so these concerns do not scatter into unrelated layers.

---

## 7. Suggested Reading

- [XNet V2 API](../api/api-xnet-v2.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [xnet-v2 and TLS Session Intro](xnet-v2-tls-intro.en.md)
- [Thread, Coroutine, and Future Coordination](../case/thread-coroutine-future.en.md)

---

**One-sentence summary:** Stream wait-source turns “what happened on the connection” into “what the program can wait for”, and that is one of the key foundations that lets XRT networking cooperate naturally with futures, coroutines, timeout, and cancel.
