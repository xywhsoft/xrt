# Coroutine Runtime

> Stackful coroutine runtime, scheduler, event waiting, and lifecycle management

[English](api-coroutine.en.md) | [中文](api-coroutine.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Overview](#overview)
- [Constants](#constants)
- [Core Types](#core-types)
- [Lifecycle APIs](#lifecycle-apis)
- [Scheduler APIs](#scheduler-apis)
- [Event and Deadline Waiting](#event-and-deadline-waiting)
- [Result, User Data, Cleanup](#result-user-data-cleanup)
- [Backend Introspection](#backend-introspection)
- [Coroutine and XNet Wait Bridge](#coroutine-and-xnet-wait-bridge)
- [Usage Patterns](#usage-patterns)
- [Notes](#notes)

---

## Overview

XRT coroutines are thread-bound, stackful, cooperative tasks.

They are intended for:

- high-concurrency task orchestration inside a single thread
- explicit scheduler-driven sleeps, deadlines, and event waits
- future xnet/event-loop integration without blocking the owner thread

They are not:

- preemptive threads
- cross-thread movable execution objects
- a replacement for `xrtThreadCreate()`

Runtime contract:

- call `xrtInit()` before using coroutine APIs
- threads created by `xrtThreadCreate()` are attached automatically
- host-created threads must call `xrtThreadAttachCurrent()` before using runtime-bound APIs
- a coroutine belongs to exactly one owner thread and one scheduler at a time

They are also part of the current async mainline together with:

- `xfuture`
- `xpromise`
- `xtask`
- `xwaitsrc`

In current XRT, coroutine code should not be thought of as an isolated subsystem.  
It is one execution model inside the broader runtime-wide async design.

---

## Constants

### Coroutine State

```c
#define XRT_CO_READY         0
#define XRT_CO_RUNNING       1
#define XRT_CO_SUSPENDED     2
#define XRT_CO_DEAD          3
```

### Termination Reason

```c
#define XRT_CO_TERM_NONE         0
#define XRT_CO_TERM_RETURNED     1
#define XRT_CO_TERM_CANCELLED    2
```

### Backend Tier / Style

```c
#define XRT_CO_BACKEND_TIER_COMPAT       1
#define XRT_CO_BACKEND_TIER_PRODUCTION   2

#define XRT_CO_BACKEND_STYLE_COMPAT      1
#define XRT_CO_BACKEND_STYLE_INLINE_ASM  2
```

### Stack and Wait Defaults

```c
#define XRT_CO_STACK_DEFAULT (64 * 1024)
#define XRT_CO_STACK_MIN     (8 * 1024)
#define XRT_CO_STACK_MAX     (8 * 1024 * 1024)
#define XRT_CO_WAIT_INFINITE UINT32_C(0xffffffff)
```

---

## Core Types

### Entry and Cleanup

```c
typedef void (*xco_entry)(ptr pParam);
typedef void (*xco_cleanup_proc)(ptr pArg);
```

### Create Arguments

```c
typedef struct {
    size_t iStackSize;
    ptr pUserData;
    uint32 iFlags;
    uint32 iReserved;
} xco_create_args;
```

Use `xrtCoCreateEx()` when you need a custom stack size or initial user data.

### Main Handles

```c
typedef struct xcoro_struct* xcoro;
typedef struct xrt_co_scheduler xcosched;
typedef struct xcoevent_struct* xcoevent;
```

- `xcoro`: a single coroutine handle
- `xcosched`: scheduler that owns runnable/waiting coroutines on one thread
- `xcoevent`: minimal coroutine wait object, currently single-waiter

---

## Lifecycle APIs

### Create / Destroy

```c
XXAPI xcoro xrtCoCreateEx(xco_entry pfnEntry, ptr pParam, const xco_create_args* pArgs);
XXAPI xcoro xrtCoCreate(xco_entry pfnEntry, ptr pParam, size_t iStackSize);
XXAPI void xrtCoDestroy(xcoro pCo);
```

`xrtCoDestroy()` is not a force-free API. The handle must already be detached from any scheduler and be in a safe final state.

### Run / Yield / Exit

```c
XXAPI bool xrtCoResume(xcoro pCo);
XXAPI void xrtCoYield();
XXAPI void xrtCoExit(int64 iExitCode);
```

- `xrtCoResume()` is the direct caller-to-coroutine switch path
- `xrtCoYield()` returns control cooperatively
- `xrtCoExit()` terminates the current coroutine immediately

### Cancel / Close / Join

```c
XXAPI bool xrtCoCancel(xcoro pCo);
XXAPI bool xrtCoClose(xcoro pCo);
XXAPI bool xrtCoJoin(xcoro pCo);
```

Current contract:

- cancellation is cooperative, not forced stack unwinding
- `join` currently supports one waiter
- `close` is the safe “I am done with this handle” path; it does not mean “kill now”

### Query State

```c
XXAPI int xrtCoGetState(xcoro pCo);
XXAPI xcoro xrtCoGetCurrent();
XXAPI bool xrtCoIsCancelRequested();
XXAPI bool xrtCoWasCancelled(xcoro pCo);
XXAPI int64 xrtCoGetExitCode(xcoro pCo);
```

---

## Scheduler APIs

```c
XXAPI xcosched* xrtCoSchedCreate();
XXAPI xcosched* xrtCoSchedCurrent();
XXAPI void xrtCoSchedDestroy(xcosched* pSched);
XXAPI xcoro xrtCoSchedSpawn(xcosched* pSched, xco_entry pfnEntry, ptr pParam, size_t iStackSize);
XXAPI bool xrtCoSchedPost(xcosched* pSched, xcoro pCo);
XXAPI bool xrtCoSchedPollOnce(xcosched* pSched, uint32 iTimeout);
XXAPI bool xrtCoSchedStep(xcosched* pSched);
XXAPI void xrtCoSchedRun(xcosched* pSched);
XXAPI int xrtCoSchedGetAlive(xcosched* pSched);
```

Use the scheduler when a coroutine needs:

- timed sleep
- deadline wait
- event wait
- cross-thread wake via post queue

`xrtCoSchedPost()` may be called from another thread. The owner thread picks the wake request up at a safe scheduler point.

---

## Event and Deadline Waiting

### Time-Based Waiting

```c
XXAPI void xrtCoSleep(uint32 iMs);
XXAPI void xrtCoSleepUntil(int64 iDeadlineMs);
XXAPI bool xrtCoWaitDeadline(int64 iDeadlineMs);
```

### Event Waiting

```c
XXAPI xcoevent xrtCoEventCreate(bool bManualReset, bool bInitialState);
XXAPI void xrtCoEventDestroy(xcoevent pEvent);
XXAPI void xrtCoEventSet(xcoevent pEvent);
XXAPI void xrtCoEventReset(xcoevent pEvent);
XXAPI bool xrtCoWaitEvent(xcoevent pEvent);
XXAPI bool xrtCoWaitEventTimeout(xcoevent pEvent, uint32 iTimeoutMs);
XXAPI bool xrtCoWaitEventUntil(xcoevent pEvent, int64 iDeadlineMs);
```

Notes:

- `xcoevent` is the basic coroutine-native wait object
- `xrtCoEventSet()` may wake a waiter across threads
- timeout / deadline waits return `false` on timeout or cancellation

---

## Result, User Data, Cleanup

### Result and User Data

```c
XXAPI void xrtCoSetResult(ptr pResult);
XXAPI ptr xrtCoGetResult(xcoro pCo);
XXAPI void xrtCoSetUserData(xcoro pCo, ptr pData);
XXAPI ptr xrtCoGetUserData(xcoro pCo);
```

### Cleanup Stack

```c
XXAPI bool xrtCoPushCleanup(xco_cleanup_proc proc, ptr pArg);
XXAPI bool xrtCoPopCleanup(xco_cleanup_proc proc, ptr pArg, bool bExecute);
```

Cleanup callbacks run in LIFO order when the coroutine exits through return, cancel, close, or explicit exit.

Important rule:

- cleanup callbacks must not `yield` or wait

---

## Backend Introspection

```c
XXAPI str xrtCoGetBackendName();
XXAPI int xrtCoGetBackendTier();
XXAPI int xrtCoGetBackendStyle();
```

These APIs are useful for:

- confirming that the target machine selected the expected backend
- distinguishing production backends from compatibility fallbacks
- recording benchmark baselines per backend

---

## Coroutine and XNet Wait Bridge

XRT already exposes coroutine-native waiting helpers on top of the `xnet-v2`
future / stream / datagram / listener surfaces.

Representative APIs:

```c
static xnet_result xrtNetFutureWaitCo(xnetfuture* pFuture);
static xnet_result xrtNetFutureWaitCoTimeout(xnetfuture* pFuture, uint32 iTimeoutMs);
static xnet_result xrtNetFutureWaitCoUntil(xnetfuture* pFuture, int64 iDeadlineMs);

static xnet_result xrtNetStreamWaitReadableCo(xnetstream* pStream);
static xnet_result xrtNetStreamWaitWritableCo(xnetstream* pStream);
static xnet_result xrtNetStreamWaitDrainCo(xnetstream* pStream);
static xnet_result xrtNetStreamWaitCloseCo(xnetstream* pStream);

static xnet_result xrtNetDgramRecvCo(xdgramsock* pSock, xnetdgrampkt** ppPacket);
static xnet_result xrtNetListenerAcceptCo(xnetlistener* pListener, xnetstream** ppStream);

static xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc);
static xnet_result xrtNetWaitSourceWaitCoValue(const xnetwaitsrc* pSrc, ptr* ppValue);
```

Stable contract to keep in mind:

- future waits suspend the current coroutine without blocking the owner thread
- stream `readable` wait is defined for the buffered paused-read path, not for raw socket readiness
- stream `writable` wait means backpressure relief, not generic socket writable polling
- datagram receive is one-shot packet delivery and currently conflicts with `OnRecv`
- listener accept is tied to the real listener/worker lifecycle, not a polling shim

For the most stable mental model, treat `xnetwaitsrc` as the unified abstraction:

- future
- stream wait-kind
- datagram receive
- listener accept

The networking layer is still evolving, so the coroutine document deliberately
keeps this section at the contract level instead of freezing every helper variant
as a long API catalog.

---

## Coroutine and Future / Task / Promise

Current XRT already aligns coroutine usage with the formal async mainline:

- coroutine code can wait on futures
- scheduler and event waiting integrate with wait-source driven designs
- current-thread deferred continuation and scheduler-targeted continuation make coroutine orchestration part of the same async model, not a separate island

The practical recommendation is:

- use coroutines for cooperative execution flow
- use futures/tasks/promises to represent asynchronous results and ownership
- use wait-source bridges when the underlying event originates from networking or other runtime subsystems

---

## Usage Patterns

### Direct Resume / Yield

```c
static void CoMain(ptr pArg)
{
    int* pCounter = (int*)pArg;
    (*pCounter)++;
    xrtCoYield();
    (*pCounter)++;
}

int main(void)
{
    int iCounter = 0;
    xcoro pCo;

    xrtInit();

    pCo = xrtCoCreate(CoMain, &iCounter, 0);
    xrtCoResume(pCo);
    xrtCoResume(pCo);

    xrtCoDestroy(pCo);
    xrtUnit();
    return 0;
}
```

### Scheduler + Sleep

```c
static void Worker(ptr pArg)
{
    (void)pArg;
    xrtCoSleep(100);
}

xcosched* pSched = xrtCoSchedCreate();
xrtCoSchedSpawn(pSched, Worker, NULL, 0);
xrtCoSchedRun(pSched);
xrtCoSchedDestroy(pSched);
```

### Event Wait

```c
static void Waiter(ptr pArg)
{
    xcoevent pEvent = (xcoevent)pArg;
    if ( xrtCoWaitEventTimeout(pEvent, 1000) ) {
        /* event signaled */
    }
}
```

---

## Notes

- A coroutine cannot be resumed from the wrong thread.
- A scheduler is thread-private; do not migrate it across threads.
- Use threads for CPU parallelism; use coroutines for cooperative concurrency inside one thread.
- Coroutine-specific xnet wait bridges exist, but they are documented from the networking side as that API continues to evolve.
- When mixing coroutines with futures, prefer the formal async mainline instead of inventing coroutine-only result carriers.
