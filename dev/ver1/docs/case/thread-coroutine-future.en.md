# XRT Case Study: Thread, Coroutine, and Future Coordination

> A practical way to divide responsibilities between threads, coroutines, and the formal async mainline.

[中文](thread-coroutine-future.md) | [Back to Case Studies](README.en.md)

---

## Core Idea

The current XRT mainline does not treat threads, coroutines, and futures as competing features.

Instead:

- threads are for parallelism
- coroutines are for cooperative orchestration
- futures unify results and waiting

This case explains why that split is recommended.

---

## Recommended Division of Work

### Threads

Use threads for:

- CPU-heavy work
- real parallel execution
- isolated worker logic

### Coroutines

Use coroutines for:

- linear business flow
- waiting without blocking the whole scheduler
- sequencing async steps inside one execution context

### Futures

Use futures for:

- cross-layer result passing
- waiting from threads or coroutines
- composition and scope management

---

## Typical Chain

A common chain looks like this:

1. a coroutine drives the request flow
2. some work is dispatched as a task
3. the task resolves a future
4. the coroutine waits on the future and continues

This is cleaner than giving each layer its own callback protocol.

---

## Related Reading

- [Coroutine](../api/api-coroutine.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Guide: Coroutine, Future, and Task Intro](../guide/coroutine-future-task-intro.en.md)
