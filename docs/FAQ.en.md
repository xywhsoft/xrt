# XRT Frequently Asked Questions

> FAQs for the current mainline. This page no longer explains retired old network/TLS/API surfaces. It answers the most common questions around the current source-tree mainline, including runtime usage, concurrency, async, and networking.

[中文](FAQ.md) | [Back to Docs Hub](README.en.md)

---

## Contents

- [Build and Integration](#build-and-integration)
- [Runtime and Thread Attach](#runtime-and-thread-attach)
- [Memory and Temporary Memory](#memory-and-temporary-memory)
- [Value and Shared Roots](#value-and-shared-roots)
- [Coroutines and Future](#coroutines-and-future)
- [Network and TLS](#network-and-tls)
- [Documentation and Mainline Reading Order](#documentation-and-mainline-reading-order)

---

## Build and Integration

### 1. How should the current mainline be built?

The current recommended way is to build against the source tree directly:

```c
#include "xrt.h"
```

Then compile:

```bash
gcc main.c xrt.c -o app
```

or:

```bash
tcc main.c xrt.c -o app.exe
```

The source-tree mainline is the default explanation model now.

### 2. Why do I get `undefined reference` or `undefined symbol`?

The most common reasons are:

- you compiled only `main.c` and forgot to compile [xrt.c](/D:/git/xrt/xrt.c)
- you included [xrt.h](/D:/git/xrt/xrt.h) but did not link the implementation
- you mixed single-header usage and source-tree usage

The first thing to verify is:

```bash
gcc main.c xrt.c -o app
```

### 3. Do I still need `XRT_IMPLEMENTATION` for the current mainline?

Not for the source-tree mainline.  
That macro belongs to the single-header distribution form, not the default explanation model of the current documentation.

---

## Runtime and Thread Attach

### 4. Why do some APIs work on the main thread but not on another thread?

Because the current mainline splits the runtime into:

- process-level state
- thread-level state

These capabilities all depend on “the current thread being attached to the XRT runtime”:

- `xrtGetError()`
- `xrtTempMemory()`
- `xrtRand32()` / `xrtRand64()`
- future / coroutine / wait-source related capabilities

### 5. Which threads are attached automatically?

- main thread: attached automatically after `xrtInit()`
- threads created by `xrtThreadCreate()`: attached automatically
- host-created threads: must call `xrtThreadAttachCurrent()` explicitly

Recommended pattern:

```c
xrtInit();

/* main thread uses XRT APIs directly */

xrtUnit();
```

or inside a host-created thread:

```c
xrtThreadAttachCurrent();
/* ... use runtime-bound APIs ... */
xrtThreadDetachCurrent();
```

### 6. Does `xrtThreadDestroy()` terminate a thread?

No.

The current formal contract is:

- `xrtThreadWait()`: wait for the thread to finish
- `xrtThreadDestroy()`: release the thread management object

Correct order:

```c
xrtThreadWait(pThread);
xrtThreadDestroy(pThread);
```

If the thread is still running, `Destroy` will refuse to proceed. It no longer silently detaches or force-stops the thread.

---

## Memory and Temporary Memory

### 7. What is `xrtTempMemory()` good for?

Use it for:

- short-lived temporary strings on the current thread
- formatting results on the current thread
- intermediate buffers on the current thread

Do not use it for:

- cross-thread sharing
- long-term storage across function boundaries
- storing into global structures, containers, or async callbacks for long-term use

### 8. Why does data returned by `xrtTempMemory()` seem to change later?

Because it is thread-local temporary memory, not long-lifetime owned memory.

If you need long-term ownership, use:

- `xrtMalloc / xrtFree`
- `xrtCopyStr`
- `xvalue`
- container-managed ownership

### 9. What does XRT memory debugging focus on now?

The current mainline focuses on more than counting. It tries to expose dangerous operations such as:

- memory leak reporting with file and line information
- double free
- invalid free
- block corruption caused by out-of-bounds writes
- use-after-free
- wrong-thread access to thread-local objects

For infrastructure development, it is recommended to enable this kind of checking in debug and test builds instead of only investigating after a final crash.

---

## Value and Shared Roots

### 10. Why does cross-thread container sharing fail?

Because after phase-2, containers are local-root objects by default.

These default constructors create local roots:

- `xvoCreateArray()`
- `xvoCreateList()`
- `xvoCreateColl()`
- `xvoCreateTable()`

If you want cross-thread sharing, create a shared root explicitly:

```c
xvalue pTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
xvalue pTags = xvoCreateCollEx(XRT_OBJMODE_SHARED);
```

### 11. Does a shared root mean it is naturally lock-free everywhere?

No.

Shared roots solve:

- whether the object may be shared across threads

They do not automatically mean that:

- walk
- iterator
- pointer view

are all naturally stable without locks. For complex traversal or stable views, explicit root locking is still recommended.

### 12. Why does inserting a local container into a shared container fail?

This is an intentional boundary tightening in the current mainline.

Correct rule:

- if an array/list/coll/table is inserted as a child into a shared container and it must participate in cross-thread sharing, that child container must itself be a real shared root

Wrong usage is rejected and sets the current thread error instead of silently mixing local and shared objects.

---

## Coroutines and Future

### 13. What is the relationship between coroutines and threads?

The current mainline recommends:

- threads for CPU parallelism
- coroutines for cooperative concurrency inside a thread

Coroutines are not a replacement for threads, and they do not migrate across threads.

### 14. What should coroutines prefer to wait on now?

Prefer:

- `future`
- `wait-source`
- `xcoevent`

Do not keep writing manual polling loops unless there is a very specific reason.

Recommended model:

- coroutines handle suspension and resumption
- future handles results and terminal states
- wait-source bridges network objects into unified waiting surfaces

### 15. Why do `ThenCurrent / CatchCurrent / FinallyCurrent` not always run immediately?

Because they are `current-thread deferred continuations`.

In the current mainline:

- if the current thread enters `xFutureWait*()`, these continuations are pumped automatically
- if the current thread is not going through the future wait mainline, you may still pump them manually:

```c
xFuturePumpCurrentContinuations(0);
```

### 16. Is `task group` just a container of futures now?

No.

In the current mainline, `task group` already has formal scope semantics:

- `Close`
- `ReapCompleted`
- `Destroy => Close + Cancel pending children`
- `JoinFuture / Join / JoinTimeout / JoinUntil`
- `CreateChild`

If what you need is “a set of async tasks that should be collected and closed as one scope”, prefer `task group` instead of hand-managing a list of futures.

---

## Network and TLS

### 17. What is the current official network mainline?

The current official mainline is:

- `xnet-v2`
- `xtlssession`
- `xhttp`
- `xhttpd`
- `xws`

The legacy network module family and the old public TLS surface are no longer the recommended mainline.

### 18. What is the current official TLS public surface?

The current official public surface is:

- `xtlssession`
- `xrtNetTlsSession*`

The old:

- `xtlsctx`
- `xrtTls*`

are no longer part of the formal public mainline.

### 19. Which waiting interfaces should network code prefer?

It is now recommended to think in terms of:

- `future`
- `wait-source`
- thread wait
- coroutine wait

For example:

- `xrtNetFutureWait*`
- `xrtNetWaitSourceWait*`
- `xrtNetWaitSourceWaitCo*`

This is more stable than memorizing a separate waiting helper for every object type.

### 20. Are listener / stream / dgram waiting APIs fully production-complete now?

The mainline has been formalized, but these paths were also heavily refactored recently. It is still recommended to continue with:

- heavier stress validation
- longer soak tests
- destroy / cancel / timeout race regression

So the more accurate statement is:

- the mainline capability is formalized
- but it is still being tightened toward fully production-grade coverage

### 21. Where should I enter the network application layer now?

The recommended order is:

1. `xnet-v2`
2. `xtlssession`
3. `xhttp`
4. `xhttpd`
5. `xws`

That means:

- for client-side work, `xhttp` is often the natural entry
- for server-side work, `xhttpd` is often the natural entry
- for bidirectional sessions, continue to `xws`

---

## Documentation and Mainline Reading Order

### 22. Why can I no longer find some old documents?

Because this documentation rebuild has moved old network/TLS mainline docs out of `docs/` and archived them under `dev/`.

This avoids having old and new mainlines appear side-by-side and misleading users.

### 23. What kinds of documents should I read first now?

Think in terms of purpose:

- API contracts: read `docs/api/`
- from-zero learning path: read `docs/guide/`
- complete combined usage: read `docs/case/`

This is usually a better way to understand the current mainline than reading a single isolated API page.

### 24. Where should I start reading?

Recommended order:

1. [Project Overview](/D:/git/xrt/README.en.md)
2. [Architecture](ARCHITECTURE.en.md)
3. [API Index](api/README.en.md)
4. [Examples](EXAMPLES.en.md)

If you want to start coding directly, read first:

1. [Base](api/api-base.en.md)
2. [Value](api/api-value.en.md)
3. [Thread](api/api-thread.en.md)
4. [Coroutine](api/api-coroutine.en.md)
5. [Future / Task / Promise](api/api-future-task-promise.en.md)
6. [XNet V2](api/api-xnet-v2.en.md)

If you want a quick “Internet + AI mainline” overview after that, continue with:

1. [Runtime and Thread Attach](guide/runtime-thread-attach.en.md)
2. [Coroutine, Future, and Task Intro](guide/coroutine-future-task-intro.en.md)
3. [xnet-v2 and TLS Mainline Intro](guide/xnet-v2-tls-intro.en.md)
4. [Streaming LLM API Intro](guide/streaming-llm-api-intro.en.md)

---

## Related Documents

- [Project Overview](/D:/git/xrt/README.en.md)
- [Architecture](ARCHITECTURE.en.md)
- [API Index](api/README.en.md)
- [Best Practices](BEST_PRACTICES.en.md)
- [Migration Guide](MIGRATION.en.md)
- [Examples](EXAMPLES.en.md)

---

**XRT FAQ Version: current mainline rebuild edition**
