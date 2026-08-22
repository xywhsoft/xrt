# XRT Guide: Wait-Source Intro

> Goal: explain why `wait-source` exists, how it differs from `future`, how it unifies thread-side and coroutine-side waiting, and why it acts as the unified waiting bridge in the current XRT async mainline.

[中文](wait-source-intro.md) | [Back to Guides](README.en.md)

---

## 0. What to Read First

If you have not built the overall XRT multitasking model yet, read these first:

- [Multitasking Overview](multitask-overview.en.md)
- [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md)

This page focuses on one thing:

- why "what are we waiting on?" should itself become a formal object
- where the boundary between `wait-source` and `future` sits
- how to move from the simplest `future` wait to network-object waiting


## 1. Why Do We Need `wait-source`?

Many programs begin with only one kind of wait:

- wait for a thread to finish
- wait for a future to complete
- wait for a socket to become readable

At first it is easy to think:

> each object can just use its own wait API.

The problem appears when the program starts waiting on several kinds of objects together.

For example:

1. one part of the logic waits on `future`
2. another waits on `stream readable`
3. another waits on `listener accept`
4. the coroutine layer wants to express all of them in the same sequential style

If each kind of object keeps a fully separate waiting protocol, several problems appear:

- call sites become tied to concrete object types
- timeout, deadline, and coroutine waiting split into multiple semantics
- upper-layer flows keep branching on "is this a future or a network object?"
- APIs drift into multiple copies of the same idea

What is really missing then is not a new thread or more callbacks.

It is:

> one formal, unified waiting entry.


## 2. Why a Single Main Thread and Fragmented Wait APIs Get Stuck

### 2.1 The Limitation of a Single Main Thread

With one sequential main thread, waiting often looks like this:

1. start one async action
2. wait specifically for that action
3. continue only after it completes

As long as there is only one wait object kind, that is still manageable.


### 2.2 The Problem Once Multiple Wait Object Kinds Mix Together

As soon as the program contains:

- `future`
- `stream`
- `dgram recv`
- `listener accept`

and there is no unified waiting object, upper-layer code keeps branching:

- this branch uses `xFutureWait*`
- that branch uses `xrtNetStreamWait*`
- another uses `xrtNetListenerAccept*`

That leads to:

- business flow polluted by low-level object-type checks
- one set of entry points to remember on the thread side and another on the coroutine side
- increasing difficulty in closing all waits into one model later


## 3. What `future`, `wait-source`, and `task group` Each Own

These 3 concepts are easy to blur together, so separate them first.

| Object | The question it answers | What you really gain |
|--------|-------------------------|----------------------|
| `future` | what is the result? | state, value, error, cancellation, continuation |
| `wait-source` | what are we waiting on right now? | one waiting entry, one timeout/deadline model, one coroutine wait path |
| `task group` | how do we close this batch of tasks together? | scope, join, close, cancel, parent/child propagation |

For a first pass, remember it this way:

> `future` carries results, `wait-source` unifies wait objects, and `task group` closes batches of tasks.

So:

- `future` is not `wait-source`
- `wait-source` is not a lighter `future`
- `task group` is not just a future array


## 4. What the Current XRT `wait-source` Can Wrap

The current formal `wait-source` mainline already covers:

- `future`
- `stream`
- `dgram recv`
- `listener accept`

The generic entry points are:

```c
XXAPI xwaitsrc xWaitSourceNone(void);
XXAPI xwaitsrc xWaitSourceFromFuture(xfuture* pFuture);
XXAPI xwaitsrc xWaitSourceFromStream(xnetstream* pStream, uint32 iWaitKind);
XXAPI xwaitsrc xWaitSourceFromDgramRecv(xdgramsock* pSock);
XXAPI xwaitsrc xWaitSourceFromListenerAccept(xnetlistener* pListener);
```

If you are already clearly inside the network layer, there is also the network-flavored naming family:

```c
XXAPI xnetwaitsrc xrtNetWaitSourceFuture(xnetfuture* pFuture);
XXAPI xnetwaitsrc xrtNetWaitSourceStream(xnetstream* pStream, uint32 iWaitKind);
XXAPI xnetwaitsrc xrtNetWaitSourceDgramRecv(xdgramsock* pSock);
XXAPI xnetwaitsrc xrtNetWaitSourceListenerAccept(xnetlistener* pListener);
```

A good way to read this is:

- `xWaitSourceFrom*` fits unified-waiting tutorials and cross-module code
- `xrtNetWaitSource*` fits code that is explicitly inside the `xnet` mainline

That is a usage recommendation based on the current interface layering.


## 5. How to Read the Unified Waiting API

### 5.1 Thread / Synchronous Side

The generic waiting entry points are:

```c
XXAPI bool xWaitSourceWait(const xwaitsrc* pSrc);
XXAPI bool xWaitSourceWaitTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
XXAPI bool xWaitSourceWaitUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
```

If you also want the value directly:

```c
XXAPI ptr xWaitSourceWaitValue(const xwaitsrc* pSrc);
XXAPI ptr xWaitSourceWaitValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
XXAPI ptr xWaitSourceWaitValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
```


### 5.2 Coroutine Side

The coroutine side keeps the same semantics, but changes "block the current thread" into "suspend the current coroutine":

```c
XXAPI bool xWaitSourceWaitCo(const xwaitsrc* pSrc);
XXAPI bool xWaitSourceWaitCoTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
XXAPI bool xWaitSourceWaitCoUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);

XXAPI ptr xWaitSourceWaitCoValue(const xwaitsrc* pSrc);
XXAPI ptr xWaitSourceWaitCoValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
XXAPI ptr xWaitSourceWaitCoValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
```


### 5.3 Network-Flavored Versions

The network layer also provides:

```c
XXAPI xnet_result xrtNetWaitSourceWait(const xnetwaitsrc* pSrc);
XXAPI xnet_result xrtNetWaitSourceWaitTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs);
XXAPI xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc);
```

The point of this family is:

- keep `xnet_result` and other network-oriented status semantics
- fit code that is already deeply inside `xnet` and wants to preserve network-state detail

If the goal is to learn the unified waiting model first, understand `xWaitSource*` before this family.


## 6. When Should You Wait on `future` Directly, and When Should You Use `wait-source`?

This is the most common question.

### 6.1 There Is Only One `future`, and the Call Site Clearly Knows That

Prefer:

- `xFutureWait*`
- `xFutureWaitValue*`
- `xFutureWaitCo*`

because that is the most direct path.


### 6.2 The Call Site Does Not Want to Care About the Underlying Object Type

If the same call site wants one unified waiting style:

- today it waits on a `future`
- tomorrow it waits on a `stream`
- later it waits on `listener accept`

then prefer:

- `wait-source`


### 6.3 You Are Already Writing Network Waits and Just Want the Shortest Path

Then it is still reasonable to prefer:

- `xrtNetStreamWaitReadableCo*`
- `xrtNetStreamWaitWritableCo*`
- `xrtNetStreamWaitDrainCo*`

because these helpers are shorter and look closer to business code.

But remember:

> underneath those helpers, the mainline is still the unified waiting model.


## 7. Demo 1: Learn `wait-source` Through `future` First

Start with the smallest and safest entry:

- run one task in a thread
- have the task return a result
- instead of calling `xFutureWaitValueTimeout()` directly, convert it into `wait-source` first

```c
#include "xrt.h"
#include <stdio.h>

static int32 procSquare(ptr pArg, xfuture_result* pOut)
{
	int32* pInput = (int32*)pArg;
	int32* pResult = NULL;

	if ( (pInput == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pResult = (int32*)xrtMalloc(sizeof(int32));
	if ( pResult == NULL ) {
		return XRT_NET_ERROR;
	}

	*pResult = (*pInput) * (*pInput);
	pOut->pValue = pResult;
	return XRT_NET_OK;
}

int main(void)
{
	int32 iInput;
	xfuture* pFuture;
	xwaitsrc tSrc;
	int32* pResult;

	xrtInit();

	iInput = 12;
	pFuture = xTaskRunThread(procSquare, &iInput, 0);
	if ( pFuture == NULL ) {
		xrtUnit();
		return 1;
	}

	tSrc = xWaitSourceFromFuture(pFuture);
	pResult = (int32*)xWaitSourceWaitValueTimeout(&tSrc, 3000);

	if ( pResult != NULL ) {
		printf("square = %d\n", (int)*pResult);
		xrtFree(pResult);
	}

	xFutureRelease(pFuture);
	xrtUnit();
	return 0;
}
```

The important line here is not that a square is computed. It is this chain:

1. `task` executes
2. `future` carries the result
3. `wait-source` turns "wait for this result" into one unified wait object

So `wait-source` does not replace `future`. It adds one more abstraction at the waiting-entry layer.


## 8. Demo 2: Move the Same Waiting Entry into a Coroutine

The second step is not "compute another square".

It is:

> the same `wait-source` can be waited on from a thread or from a coroutine.

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iInput;
} DemoWaitSourceCoCtx;

static int32 procSquare(ptr pArg, xfuture_result* pOut)
{
	int32* pInput = (int32*)pArg;
	int32* pResult = NULL;

	if ( (pInput == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pResult = (int32*)xrtMalloc(sizeof(int32));
	if ( pResult == NULL ) {
		return XRT_NET_ERROR;
	}

	*pResult = (*pInput) * (*pInput);
	pOut->pValue = pResult;
	return XRT_NET_OK;
}

static void procMainCo(ptr pArg)
{
	DemoWaitSourceCoCtx* pCtx = (DemoWaitSourceCoCtx*)pArg;
	xfuture* pFuture;
	xwaitsrc tSrc;
	int32* pResult;

	pFuture = xTaskRunThread(procSquare, &pCtx->iInput, 0);
	if ( pFuture == NULL ) {
		return;
	}

	tSrc = xWaitSourceFromFuture(pFuture);
	pResult = (int32*)xWaitSourceWaitCoValueTimeout(&tSrc, 3000);

	if ( pResult != NULL ) {
		printf("co square = %d\n", (int)*pResult);
		xrtFree(pResult);
	}

	xFutureRelease(pFuture);
}

int main(void)
{
	xcosched* pSched;
	DemoWaitSourceCoCtx tCtx;

	xrtInit();

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		xrtUnit();
		return 1;
	}

	tCtx.iInput = 21;
	if ( xrtCoSchedSpawn(pSched, procMainCo, &tCtx, 0) == NULL ) {
		xrtCoSchedDestroy(pSched);
		xrtUnit();
		return 1;
	}

	xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);
	xrtUnit();
	return 0;
}
```

There are 3 important things to notice.

### 8.1 The `wait-source` Did Not Change

You are still waiting on:

- `xWaitSourceFromFuture(...)`

Only the consumer-side entry changed:

- thread side uses `xWaitSourceWait*`
- coroutine side uses `xWaitSourceWaitCo*`


### 8.2 The Coroutine Is Not Doing Heavy Work

The square computation still runs as a thread task.

The coroutine does:

- orchestration
- waiting
- collecting the result


### 8.3 This Is the Real Value of `wait-source`

It turns "wait object" from a private detail of one API family into one interface the whole async mainline can recognize.


## 9. Demo 3: Use the Same Waiting Entry with `stream`

Only now is it worth bringing network objects in.

The fragment below is not a full connection example. It shows:

> after switching to `stream`, the waiting style itself does not change.

```c
xwaitsrc tSrc;

tSrc = xWaitSourceFromStream(pStream, XNET_STREAM_WAIT_READABLE);

if ( xWaitSourceWaitCoTimeout(&tSrc, 5000) ) {
	/* the stream is now readable; continue with recv / read logic */
}
```

If you want to preserve the network-flavored return status, you can also write:

```c
xnetwaitsrc tNetSrc;

tNetSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_READABLE);

if ( xrtNetWaitSourceWaitCoTimeout(&tNetSrc, 5000) == XRT_NET_OK ) {
	/* readable */
}
```

The real point here is:

- from the upper-layer flow's point of view, "wait for future" and "wait for stream readable" now speak the same language
- from the lower-layer object's point of view, each module still handles its own concrete work

In other words:

- `wait-source` unifies the waiting entry
- the real read, write, recv, and accept work still belongs to the corresponding module


## 10. Demo 4: Switch to `listener accept` or `dgram recv`, but Keep the Same Mental Model

This is the most important part to remember:

> change the wait source, not the waiting mental model.

For a listener:

```c
xwaitsrc tSrc;

tSrc = xWaitSourceFromListenerAccept(pListener);

if ( xWaitSourceWaitTimeout(&tSrc, 3000) ) {
	/* accept is ready; continue with the listener accept API */
}
```

For datagram receive:

```c
xwaitsrc tSrc;

tSrc = xWaitSourceFromDgramRecv(pSock);

if ( xWaitSourceWaitCoTimeout(&tSrc, 3000) ) {
	/* dgram recv can now move forward */
}
```

These are short fragments, but the lesson is important:

- `wait-source` is not a tiny trick for one module
- it is the unified waiting bridge in the current XRT async mainline


## 11. One Selection Table: Which Waiting Layer Should You Use?

| Scenario | Prefer | Reason |
|----------|--------|--------|
| I only wait on one `future` | `xFutureWait*` | most direct, least abstraction |
| I want the call site not to care about the underlying object type | `xWaitSource*` | unifies future / stream / listener waits |
| I am already clearly inside `xnet` code | `xrtNetWaitSource*` or `xrtNetStreamWait*` | closer to network contracts and statuses |
| I am waiting inside a coroutine | `xWaitSourceWaitCo*` or `xFutureWaitCo*` | suspend the coroutine instead of blocking the thread |

One-sentence version:

> The more explicit the object type is, the more specialized the API can be. The more you want waiting semantics to converge, the more you should move toward `wait-source`.


## 12. Windows / Linux Cross-Platform Notes

### 12.1 Do Not Confuse Waiting with Object Ownership

`wait-source` handles:

- waiting

It does not handle:

- object lifetime ownership

For example:

- `xWaitSourceFromFuture()` does not take ownership of `xfuture*`
- after waiting, you still need to `xFutureRelease()` yourself


### 12.2 Keep Timeout and Deadline Semantics Consistent

If one flow has already decided:

- the outer layer uses timeout

then try to keep timeout semantics through the whole chain.

If the outer layer has decided:

- the outer layer uses deadline

then keep using `until` semantics through the whole chain.

That makes cross-platform behavior easier to align and easier to debug.


### 12.3 Do Not Fall Back to Busy Polling Inside Coroutines

If you already have:

- `xWaitSourceWaitCo*`

then do not reintroduce code like:

```c
while ( !bReady ) {
	xrtCoYield();
}
```

That may run for a while, but it fragments the waiting model again.


## 13. Common Mistakes

### 13.1 Mistake 1: Treat `wait-source` as an Alias for `future`

It is not.

`future` is the result object. `wait-source` is the waiting object.


### 13.2 Mistake 2: After `wait-source`, the Object's Own API Is No Longer Needed

Also wrong.

`wait-source` only tells you "you may continue waiting or continue the next step now".

The real:

- read
- write
- accept
- recv

operations still go back to the corresponding module API.


### 13.3 Mistake 3: Memorize Many Specialized Helpers but Forget the Unified Mainline

If you only remember helpers like:

- `xrtNetStreamWaitReadableCoTimeout`
- `xrtNetStreamWaitDrainCoUntil`

then it is easy to keep treating each waiting style as a separate world.

The steadier approach is:

- first understand `wait-source` as the unified waiting bridge
- then treat specialized helpers as thin wrappers


### 13.4 Mistake 4: Use `WaitCo*` from an Ordinary Thread

Coroutine waiting entry points only make sense inside coroutine context.

The thread side and coroutine side have similar semantics, but they are not the same execution environment.


### 13.5 Mistake 5: Forget to Close the Real Result Object After Waiting

For example:

- a `future` is waited on, but `xFutureRelease()` is forgotten

`wait-source` will not reclaim that lifecycle for you.


## 14. What to Read Next

Recommended order:

1. [Task Group Intro: From Unified Waiting to Structured Closure](task-group-intro.en.md)
2. [xnet-v2 Stream Wait-Source Intro](xnet-stream-wait-source-intro.en.md)
3. [Future / Task / Promise](../api/api-future-task-promise.en.md)
4. [XNet V2 API](../api/api-xnet-v2.en.md)
5. [Case: xnet-v2 Stream Wait-Source Walkthrough](../case/xnet-stream-wait-source.en.md)

---

**One-sentence summary:** `wait-source` does not solve how results are stored; it solves how waiting entry points are unified. In the current XRT mainline, it pulls `future`, `stream`, `dgram recv`, and `listener accept` into one language so both threads and coroutines can keep moving along the same semantic chain.
