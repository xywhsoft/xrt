# XRT Guide: Coroutine, Future, and Task Intro

> The recommended way to understand the current async mainline: coroutines handle suspension, futures carry results, tasks describe execution, and wait-sources bridge low-level events.

[中文](coroutine-future-task-intro.md) | [Back to Guides](README.en.md)

---

## Mainline View

The current XRT async mainline is organized around:

- `xcoro`: execution and suspension
- `xfuture`: result and terminal state
- `xpromise`: write side of a future
- `xtask`: executable async work
- `xwaitsrc`: unified waiting bridge

The goal is not to keep separate async models for threads, coroutines, and networking.

The goal is to collapse them into one mainline.

---

## How to Think About Each Layer

### Coroutine

Use coroutines for:

- structured execution inside a scheduler
- cooperative waiting
- keeping business logic linear

### Future

Use futures for:

- representing eventual results
- waiting from threads or coroutines
- composition with `WhenAny / WhenAll / Race`

### Task

Use tasks for:

- turning executable work into futures
- unifying thread / coroutine / engine / delayed execution

---

## Recommended Reading Order

1. learn `xFutureWait*`
2. learn `xTaskRun*`
3. learn `Then / Catch / Finally`
4. learn `WhenAny / WhenAll / Race`
5. learn `task group / nested scope`

---

## Minimal Pattern

```c
xfuture* pFuture = xTaskRunThread(procTask, pArg, 0);

if ( xFutureWaitTimeout(pFuture, 1000) ) {
	ptr pValue = xFutureValue(pFuture);
	(void)pValue;
}

xFutureRelease(pFuture);
```

This is the most direct way to see the current formal async model in practice.

---

## Related Reading

- [Coroutine](../api/api-coroutine.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Case: Thread / Coroutine / Future Coordination](../case/thread-coroutine-future.en.md)
