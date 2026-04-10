# XRT Guide: Thread Intro

> Goal: explain the difference between threads and a single sequential main thread, then teach the XRT thread mainline through 4 levels: the smallest demo, the stop protocol, mutexes, and condition variables.

[中文](thread-intro.md) | [Back to Guides](README.en.md)

---

## 0. What to Read First

If you have not built the overall mental model yet, read these first:

- [Multitasking Overview](multitask-overview.en.md)
- [Runtime and Thread Attach](runtime-thread-attach.en.md)

This page focuses on one thing only:

- when threads are the right tool
- how to write steadier thread-based code in XRT


## 1. What Is the Difference Between a Thread and the Main Thread?

Do not rush into APIs first. Separate the core idea first.

### 1.1 How a Single Main Thread Works

When one main thread runs sequentially, the logic usually looks like this:

1. do A
2. if A is not done, B cannot start
3. if B is not done, C cannot start

Its advantages:

- simple
- straightforward to debug
- clear lifecycle

Its problems:

- one blocking step makes all later work wait
- one CPU-heavy step slows the whole program
- even moderately complex "do these at the same time" requirements become awkward quickly


### 1.2 What Threads Actually Add

Threads do not add a vague sense of "being async".

They add this:

> one independent execution unit

That means:

- the main thread can continue with its own work
- worker threads can process other jobs in parallel
- both sides can cooperate through sync primitives or message handoff

What you really gain is:

- independent scheduling
- an independent stack
- independent thread-local runtime state

What you do not automatically gain is:

- a clear result model
- a clear shared-data boundary
- correct lifecycle management by default

That is why threads are useful, but also easy to misuse.


## 2. When Should Threads Be the First Choice?

The following scenarios are usually good fits for threads:

### 2.1 CPU-Heavy Work

For example:

- compression
- encryption
- large text processing
- batch parsing

If this work stays on the main thread, the whole program slows down easily.


### 2.2 Independent Blocking Operations

For example:

- a library only provides blocking APIs
- one external call is slow and cannot be moved to event-driven I/O yet
- a long command or long workflow needs to run in the background


### 2.3 A Clear Worker Model

For example:

- a background logging thread
- a background compression thread
- a worker dedicated to consuming a task queue

In these cases the role of the thread is explicit:

- it is not for waiting
- it is for execution


## 3. When Should Threads Not Be the First Reaction?

There are also several cases where "just create a thread" should not be the first move.

### 3.1 You Mainly Need to Wait for Several Async Steps

If the real problem is:

- wait for this, then wait for that
- organize one wait chain
- add timeout, cancellation, or combined waiting

then you should first think about:

- coroutines
- `future / task`
- `wait-source`


### 3.2 You Mainly Need to Hand Data Off Between Threads

If the core problem is:

- one place produces data
- another place consumes data

then the real main actor is usually:

- `queue`

Threads are the executors, not the handoff protocol itself.


### 3.3 You Only Want the Main Thread to "Look Non-Blocking"

This kind of requirement easily turns into:

- threads opened everywhere
- results written back into a pile of shared fields
- polling and locks used later to stitch things together

That might run in the short term, but it ages badly.


## 4. The 4 Basic Rules of the XRT Thread Mainline

### 4.1 The Main Thread Enters the Runtime with `xrtInit()`

The main thread normally does not need manual attach. `xrtInit()` already handles:

- global runtime init
- attaching the current thread


### 4.2 Threads Created by `xrtThreadCreate()` Attach Automatically

So inside the thread function, you can directly use runtime-bound capabilities such as:

- `xrtGetError()`
- `xrtTempMemory()`
- default random helpers


### 4.3 Keep the Lifecycle Order as `Wait -> Destroy`

That means:

1. `xrtThreadWait()`
2. `xrtThreadDestroy()`

`Destroy` does not mean "stop the thread". It means "free the thread management object".


### 4.4 `Stop` Is Cooperative Stop, Not Forced Kill

That means:

- external code calls `xrtThreadStop()`
- the thread checks `xrtThreadShouldStop()` internally
- the thread exits at its own safe point

This is the recommended mainline.

`xrtThreadKill()` is only a dangerous last resort and should not be the normal design.


## 5. Demo 1: The First Thread

Start with the smallest pattern:

- the main thread creates a worker
- the worker does one slow step
- the main thread waits for it

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iJobId;
} DemoThreadCtx;

static uint32 procWorker(ptr pArg)
{
	DemoThreadCtx* pCtx = (DemoThreadCtx*)pArg;

	printf("worker %d start, tid=%llu\n",
		(int)pCtx->iJobId,
		(unsigned long long)xrtThreadGetCurrentId());

	xrtSleep(300);

	printf("worker %d done\n", (int)pCtx->iJobId);
	return 0;
}

int main(void)
{
	xthread hThread;
	DemoThreadCtx tCtx;

	xrtInit();

	tCtx.iJobId = 1;
	hThread = xrtThreadCreate(procWorker, &tCtx, 0);
	if ( hThread == NULL ) {
		printf("create thread failed: %s\n", (char*)xrtGetError());
		xrtUnit();
		return 1;
	}

	xrtThreadWait(hThread);
	xrtThreadDestroy(hThread);

	xrtUnit();
	return 0;
}
```

The point here is not the feature. It is to remember the lifecycle:

- create
- run
- wait
- destroy


## 6. Demo 2: Stop a Thread Correctly

The second thing you should learn is always the stop protocol.

Real threads are usually not "run once and exit". They more often:

- work in a loop
- check regularly whether they should stop

The example below uses the recommended XRT approach:

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	xthread* phThread;
	volatile int32 iCounter;
} DemoStopCtx;

static uint32 procLoop(ptr pArg)
{
	DemoStopCtx* pCtx = (DemoStopCtx*)pArg;
	int32 i;

	for ( i = 0; i < 1000; i++ ) {
		xthread hSelf = pCtx->phThread ? *pCtx->phThread : NULL;

		if ( hSelf && xrtThreadShouldStop(hSelf) ) {
			printf("worker stop requested\n");
			return 1;
		}

		pCtx->iCounter++;
		xrtSleep(10);
	}

	return 0;
}

int main(void)
{
	xthread hThread = NULL;
	DemoStopCtx tCtx;

	xrtInit();

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.phThread = &hThread;

	hThread = xrtThreadCreate(procLoop, &tCtx, 0);
	if ( hThread == NULL ) {
		xrtUnit();
		return 1;
	}

	xrtSleep(80);
	xrtThreadStop(hThread);

	xrtThreadWait(hThread);
	printf("counter = %d, exit = %u\n",
		(int)tCtx.iCounter,
		(unsigned)xrtThreadGetExitCode(hThread));

	xrtThreadDestroy(hThread);
	xrtUnit();
	return 0;
}
```

What matters here:

- `Stop` only sends a request
- the real exit point is inside the thread
- this lets the thread clean up at a safe point instead of being interrupted from outside


## 7. Demo 3: Shared Data Must Be Protected by a Mutex

As soon as there is more than one thread, shared state becomes the easiest place to break things.

This demo shows one point only:

- several threads update one counter
- without a lock the result can go wrong
- with `xmutex`, the boundary becomes clear

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmutex hLock;
	volatile int32 iCounter;
	int32 iLoopCount;
} DemoMutexCtx;

static uint32 procAdd(ptr pArg)
{
	DemoMutexCtx* pCtx = (DemoMutexCtx*)pArg;
	int32 i;

	for ( i = 0; i < pCtx->iLoopCount; i++ ) {
		xrtMutexLock(pCtx->hLock);
		pCtx->iCounter++;
		xrtMutexUnlock(pCtx->hLock);
	}

	return 0;
}

int main(void)
{
	DemoMutexCtx tCtx;
	xthread arrThreads[4];
	int32 i;

	xrtInit();

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hLock = xrtMutexCreate();
	tCtx.iLoopCount = 10000;

	if ( tCtx.hLock == NULL ) {
		xrtUnit();
		return 1;
	}

	for ( i = 0; i < 4; i++ ) {
		arrThreads[i] = xrtThreadCreate(procAdd, &tCtx, 0);
	}

	for ( i = 0; i < 4; i++ ) {
		if ( arrThreads[i] ) {
			xrtThreadWait(arrThreads[i]);
			xrtThreadDestroy(arrThreads[i]);
		}
	}

	printf("counter = %d\n", (int)tCtx.iCounter);

	xrtMutexDestroy(tCtx.hLock);
	xrtUnit();
	return 0;
}
```

What to remember:

- parallel threads do not make shared mutable state safe
- a lock protects the critical section
- keep the lock short; do not hold it across long blocking work


## 8. Demo 4: Condition Variables Solve "Wait Until the Condition Becomes True"

If your problem is not "fight over one counter", but:

- one thread waits until another thread prepares something
- sleep while the condition is not ready
- wake up when the condition becomes true

then a condition variable is a better fit than busy waiting.

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	xmutex hLock;
	xcond hCond;
	volatile int32 bReady;
	str sMessage;
} DemoCondCtx;

static uint32 procWaiter(ptr pArg)
{
	DemoCondCtx* pCtx = (DemoCondCtx*)pArg;

	xrtMutexLock(pCtx->hLock);
	while ( !pCtx->bReady ) {
		if ( xrtCondWaitTimeout(pCtx->hCond, pCtx->hLock, 1000) != XRT_WAIT_OK ) {
			xrtMutexUnlock(pCtx->hLock);
			return 1;
		}
	}

	printf("message = %s\n", (char*)pCtx->sMessage);
	xrtMutexUnlock(pCtx->hLock);
	return 0;
}

int main(void)
{
	DemoCondCtx tCtx;
	xthread hThread;

	xrtInit();

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hLock = xrtMutexCreate();
	tCtx.hCond = xrtCondCreate();

	if ( (tCtx.hLock == NULL) || (tCtx.hCond == NULL) ) {
		if ( tCtx.hCond ) xrtCondDestroy(tCtx.hCond);
		if ( tCtx.hLock ) xrtMutexDestroy(tCtx.hLock);
		xrtUnit();
		return 1;
	}

	hThread = xrtThreadCreate(procWaiter, &tCtx, 0);
	if ( hThread == NULL ) {
		xrtCondDestroy(tCtx.hCond);
		xrtMutexDestroy(tCtx.hLock);
		xrtUnit();
		return 1;
	}

	xrtSleep(100);

	xrtMutexLock(tCtx.hLock);
	tCtx.sMessage = (str)"hello from main thread";
	tCtx.bReady = 1;
	xrtCondSignal(tCtx.hCond);
	xrtMutexUnlock(tCtx.hLock);

	xrtThreadWait(hThread);
	xrtThreadDestroy(hThread);
	xrtCondDestroy(tCtx.hCond);
	xrtMutexDestroy(tCtx.hLock);

	xrtUnit();
	return 0;
}
```

The key point:

- `cond` is not the data itself
- a condition variable is always used with a condition protected by a mutex
- `while` is safer than `if`, because the condition should still be rechecked after wake-up


## 9. Semaphore or Condition Variable?

For a first-pass split, use this:

### 9.1 "There Are N Tokens to Consume"

Prefer:

- `xsem`

Good for:

- producer / consumer counting
- throttling
- fixed resource counts


### 9.2 "Some Shared Condition Becomes True"

Prefer:

- `xcond + xmutex`

Good for:

- waiting until config is ready
- waiting until one shared state becomes available
- waking multiple waiters on one condition


## 10. Common Mistakes

### 10.1 Mistake 1: Use Threads as a Waiting Tool

Threads are best for execution, not for orchestrating waits.

If your main problem is a wait chain, look first at:

- coroutines
- `future / task`


### 10.2 Mistake 2: Treat `xrtThreadDestroy()` as Stop

That is the wrong responsibility.

`Destroy` releases the thread object. It does not end the thread.


### 10.3 Mistake 3: Leave Shared Variables Unprotected

As soon as multiple threads read and write the same mutable state, you must decide clearly:

- does it need a lock?
- should it become message handoff instead?
- should it become a `future` result instead of shared writes?


### 10.4 Mistake 4: Reach for `xrtThreadKill()` Too Easily

Forcing a thread down breaks cleanup order and usually makes bugs harder to diagnose.

The recommended priority is always:

1. cooperative stop
2. wait for normal exit
3. destroy the thread object


## 11. When Should You Upgrade from Threads to Another Model?

If you start hitting these problems, threads alone are no longer enough:

- you want cleaner handoff between threads
- you need backpressure and batch delivery
- you want to write wait chains as sequential code
- you want one result model for timeout, cancellation, and completion

The next step is usually:

- [Queue Intro: When Message Handoff Is Better Than Shared State](queue-intro.en.md)
- [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md)


## 12. What to Read Next

Recommended order:

1. [Runtime and Thread Attach](runtime-thread-attach.en.md)
2. [Thread API](../api/api-thread.en.md)
3. [Queue Intro: When Message Handoff Is Better Than Shared State](queue-intro.en.md)
4. [Queue API](../api/api-queue.en.md)
5. [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md)
6. [Case: Thread / Coroutine / Future Coordination](../case/thread-coroutine-future.en.md)

---

**One-sentence summary:** threads solve "independent execution units", not every concurrency problem; in the current XRT mainline, if you make thread lifecycle, stop protocol, and shared-data boundaries clear first, it becomes much easier to add queues, coroutines, and futures later.
