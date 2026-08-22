# Future / Task / Promise Unified Async Model

> XRT's unified async runtime model: future, promise, task, wait-source, and task-group based structured concurrency.

[Back to Index](README.en.md)

---

## 1. Design Goal

This model unifies:

- thread task results
- coroutine task results
- engine worker task results
- wait-source results
- continuation chains
- combinators
- structured concurrency

The formal core objects are:

- `xfuture`
- `xpromise`
- `xtask`
- `xwaitsrc`

---

## 2. Core Objects

### `xfuture`

Represents a result that will complete later. It carries:

- state
- value
- error
- waiting
- continuation
- cancel request

### `xpromise`

Represents the write side of a future. It resolves, rejects, cancels, or closes a future, and one promise can only push one future into one terminal state.

### `xtask`

Represents a schedulable async task. The current formal execution entries are:

- `xTaskRunThread`
- `xTaskRunCo`
- `xTaskRunEngine`
- `xTaskRunDelayed`

### `xwaitsrc`

Represents a unified waiting source. In the current mainline it is mainly used for:

- futures
- stream wait surfaces
- dgram recv

---

## 3. Unified Waiting Rules

The standard waiting trio is:

- `Wait`
- `WaitTimeout`
- `WaitUntil`

This same semantic shape is carried across futures, wait-sources, thread waits, and coroutine waits.

---

## 4. Execution Mainline

The current mainline already supports:

- thread task -> future
- coroutine task -> future
- engine task -> future
- delayed engine task -> future

So different execution environments already share one result model.

---

## 5. Continuations

The current formal continuation families are:

- `Then*`
- `Catch*`
- `Finally*`

Current execution targets:

- `Inline`
- `Current`
- `Engine`
- `Co`

The current-thread deferred path is automatically pumped by the main waiting path, so it does not always require explicit manual pumping.

---

## 6. Combinators

Current formal combinators:

- `xFutureWhenAny`
- `xFutureWhenAll`
- `xFutureRace`

The richer-result contract already includes:

- group source index
- group source future
- aggregated all-value object for `WhenAll(success)`

---

## 7. Structured Concurrency

`xtaskgroup` is not just a container. It already has formal scope semantics:

- `Close`
- `ReapCompleted`
- `Destroy => Close + Cancel pending children`
- `JoinFuture / Join / JoinTimeout / JoinUntil`
- `CreateChild`
- parent-bind cancellation propagation

The formal nested-scope entry is:

- `xTaskGroupCreateChild`

---

## 8. Ownership Rules

The explicit rule is:

- `Get*` => retained reference
- `Peek*` => borrowed reference

Examples:

- `xPromiseGetFuture` vs `xPromisePeekFuture`
- `xFutureGetGroupSource` vs `xFuturePeekGroupSource`

---

## 9. Recommended Understanding Order

The recommended design / learning order is:

1. `xfuture / xpromise`
2. `Wait / WaitTimeout / WaitUntil`
3. `xtask`
4. `Then / Catch / Finally`
5. `WhenAny / WhenAll / Race`
6. `task group / nested scope`

---

## 10. Bounded Blocking Executor

Network workers, coroutine schedulers, and continuation dispatchers must not run blocking system calls or long CPU work directly. Use `xtaskexecutor` for that work:

```c
xtaskexecutorconfig tCfg;
xTaskExecutorConfigInit(&tCfg);
tCfg.iThreadCount = 4;
tCfg.iQueueLimit = 1024;

xtaskexecutor* pExecutor = xTaskExecutorCreate(&tCfg);
xfuture* pFuture = xTaskExecutorSubmit(pExecutor, procBlockingTask, pArg);
```

Contract:

- The thread count is fixed at creation; tasks do not create threads.
- `iQueueLimit` is a hard waiting-queue limit. Submission returns `NULL` when full or closed.
- A successful submission returns a caller-owned Future; the task keeps its own reference.
- Cancellation before dequeue prevents the callback from running and resolves as `XRT_NET_CANCELLED`.
- A running C callback is never forcibly terminated. Cooperative cancellation requires an application cancel token.
- `Destroy` rejects new work, drains accepted work, joins all workers, and then releases the executor.
- An executor cannot destroy itself from one of its workers; destruction must be delegated to another thread.
- `xTaskExecutorGetStats` snapshots submitted, completed, rejected, queued, running, and thread count.
- Configuration follows the append-only `iSize/iVersion` ABI rule.

---

## 11. Suggested Reading

- [Coroutine](api-coroutine.en.md)
- [Thread](api-thread.en.md)
- [XNet V2](api-xnet-v2.en.md)
- [Guides](../guide/README.en.md)
- [Case Studies](../case/README.en.md)
