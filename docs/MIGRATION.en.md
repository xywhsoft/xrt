# XRT Migration Guide

> Migration notes for the current mainline. This document no longer treats old network/TLS public surfaces as long-term compatibility targets. Its goal is to help you move onto the current formal mainline.

[中文](MIGRATION.md) | [Back to Docs Hub](README.en.md)

---

## Contents

- [Current Migration Principles](#current-migration-principles)
- [Source Tree and Single Header](#source-tree-and-single-header)
- [Runtime Migration](#runtime-migration)
- [Thread Migration](#thread-migration)
- [Container and Shared Root Migration](#container-and-shared-root-migration)
- [Coroutine and Async Mainline Migration](#coroutine-and-async-mainline-migration)
- [Network and TLS Migration](#network-and-tls-migration)
- [Migration Checklist](#migration-checklist)

---

## Current Migration Principles

This migration round follows 3 principles:

1. treat the current source mainline as the single source of truth
2. do not carry long-term compatibility baggage for old surfaces that have not been widely adopted
3. if something can be collapsed into one mainline, do not keep dual public surfaces alive

So the focus is not:

- “keep every old API alive forever”

but instead:

- move onto the current formal mainline quickly
- make future API freezing cleaner

---

## Source Tree and Single Header

### 1. The source tree is the current explanation model

Recommended:

```bash
gcc main.c xrt.c -o app
```

or:

```bash
tcc main.c xrt.c -o app.exe
```

The current docs and examples explain the mainline through the source-tree model.

### 2. Single-header distribution still exists, but it is no longer the default migration target

If old code still uses:

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

that does not mean it stops working immediately; but if you are migrating a long-lived codebase, prefer moving to the source tree first.

---

## Runtime Migration

### 1. Move from “global error/temp state” to thread runtime state

Older habits often treated errors and temporary state as globally shared.

The current mainline now separates:

- process-level state
- thread-level state

Migration focus:

- use `xrtGetError()` instead of depending on `xCore.LastError`
- treat `xrtTempMemory()` as thread-local temporary memory
- understand that the default random generators also belong to the current thread state

### 2. `xrtInit()` / `xrtUnit()` remain the formal entry and exit points

```c
xrtInit();
/* ... */
xrtUnit();
```

Any program that uses runtime-bound thread state should keep this pair.

---

## Thread Migration

### 1. Main thread and XRT-created threads are attached automatically

The current formal contract is:

- main thread: attached automatically after `xrtInit()`
- thread created by `xrtThreadCreate()`: attached automatically

### 2. Host-created threads must attach explicitly

If the thread was not created by `xrtThreadCreate()`, migrate it to:

```c
xrtThreadAttachCurrent();
/* ... use runtime-bound APIs ... */
xrtThreadDetachCurrent();
```

### 3. `Destroy` semantics are stricter now

Old habit:

```c
xrtThreadDestroy(pThread);
```

Current recommendation:

```c
xrtThreadWait(pThread);
xrtThreadDestroy(pThread);
```

`Destroy` only releases the thread management object now. It no longer stands in for “implicit detach / terminate thread”.

---

## Container and Shared Root Migration

This is one of the most important migration areas.

### 1. Default containers are now local roots

These default constructors create thread-local objects:

- `xvoCreateArray()`
- `xvoCreateList()`
- `xvoCreateColl()`
- `xvoCreateTable()`

### 2. Cross-thread sharing now requires explicit shared roots

Old style:

```c
xvalue pTable = xvoCreateTable();
xvalue pTags = xvoCreateColl();
```

New style:

```c
xvalue pTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
xvalue pTags = xvoCreateCollEx(XRT_OBJMODE_SHARED);
xvoCollSetText(pTags, "ai", 0, FALSE);
xvoTableSetValue(pTable, "tags", 4, pTags, FALSE);
```

### 3. Nested shared containers must also be shared

If an array/list/coll/table is inserted into a shared container as a child, and it must also participate in cross-thread sharing, that child container itself must be a real shared root.

Otherwise the current mainline rejects the operation instead of silently mixing local and shared objects.

### 4. Shared does not mean “everything is lock-free”

Shared roots solve whether an object may be shared across threads. They do not automatically guarantee that:

- walk
- iterator
- pointer view

are all naturally safe without locks. Complex traversal should still use explicit root locking.

---

## Coroutine and Async Mainline Migration

### 1. Coroutine high-level abstractions are now stable

You should think in terms of:

- coroutines for execution and suspension
- scheduler for orchestration
- event / deadline for native waiting
- future / task / promise / wait-source for the unified async mainline

### 2. Prefer migrating waits onto future / wait-source

If old coroutine code still implements its own polling loop, migrate it to:

- `xFutureWaitCo*`
- `xrtNetFutureWaitCo*`
- `xrtNetWaitSourceWaitCo*`

This lets coroutines, threads, and network waits all share the same async model.

### 3. Current-thread deferred continuation now auto-pumps

If you have already migrated to:

- `ThenCurrent`
- `CatchCurrent`
- `FinallyCurrent`

then `xFutureWait*()` on the current thread will automatically advance these continuations. You no longer need to manually pump every step.

### 4. The new async mainline is not just a set of future helpers

The formal migration target should be understood as:

- `xfuture`
- `xpromise`
- `xtask`
- `xwaitsrc`
- `task group`
- `nested scope`

If old code currently has:

- one callback model for thread tasks
- another callback model for coroutine tasks
- another wait model for network async

then the migration goal should be to collapse them into this unified mainline rather than preserving multiple result models.

### 5. Scope-shaped concurrency should migrate to `task group`

If old code has:

- a set of child tasks running concurrently
- a parent waiting for all of them
- cancellation that must propagate downward

then after migration you should prefer:

- `xTaskGroupRun*`
- `xTaskGroupJoin*`
- `xTaskGroupCreateChild`

instead of continuing to hand-manage arrays of futures and cancellation chains.

---

## Network and TLS Migration

### 1. The old network mainline is retired

It is no longer recommended to keep building long-term code around:

- `nettcp`
- `netudp`
- `nethttp`
- `netpoll`

Prefer moving to:

- `xnet-v2`
- `xtlssession`
- `xhttp`
- `xhttpd`
- `xws`

### 2. TLS has been collapsed into one formal public surface

Old public surface:

- `xtlsctx`
- `xrtTls*`

It is no longer the formal outward-facing mainline.

Migration target:

- `xtlssession`
- `xrtNetTlsSession*`

### 3. Recommended migration order

If you are migrating old networking logic, do it in this order:

1. move to low-level `xnet-v2`
2. move waits onto `future / wait-source`
3. switch to TLS sessions
4. then move to `xhttp / xhttpd / xws`

This minimizes risk and matches the current architecture more closely.

### 4. Do not keep migrating the application layer around old public surfaces

If you are migrating:

- an HTTP client
- an HTTP server
- a WebSocket service

then build the new mainline directly around:

- `xhttp`
- `xhttpd`
- `xws`

instead of wrapping old TCP / HTTP surfaces and extending the historical chain.

---

## Migration Checklist

### Entry and Build

- [ ] the mainline build has been moved to the source-tree model
- [ ] `xrtInit()` / `xrtUnit()` remain in place

### Runtime

- [ ] no direct dependence on `xCore.LastError`
- [ ] runtime-bound APIs are understood through `xrtGetError()` / `xrtTempMemory()`
- [ ] host-created threads explicitly use `AttachCurrent / DetachCurrent`

### Concurrency

- [ ] `xrtThreadDestroy()` usage is updated to `Wait -> Destroy`
- [ ] coroutine waits are migrated to future / wait-source where possible

### Containers

- [ ] cross-thread containers use `xvoCreate*Ex(XRT_OBJMODE_SHARED)`
- [ ] nested containers inside shared roots are also explicitly shared
- [ ] complex shared traversal uses root locking

### Networking

- [ ] new logic is built on `xnet-v2`
- [ ] TLS has been migrated to `xtlssession`
- [ ] no new code is added on top of retired old network public surfaces

---

## Related Documents

- [Project Overview](../../README.en.md)
- [Architecture](ARCHITECTURE.en.md)
- [API Index](api/README.en.md)
- [Thread](api/api-thread.en.md)
- [Coroutine](api/api-coroutine.en.md)
- [Future / Task / Promise](api/api-future-task-promise.en.md)
- [Value](api/api-value.en.md)
- [XNet V2](api/api-xnet-v2.en.md)
- [Network TLS](api/api-network-tls.en.md)

---

**XRT Migration Guide Version: current mainline rebuild edition**
