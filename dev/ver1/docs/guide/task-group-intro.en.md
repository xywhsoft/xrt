# XRT Guide: Task Group Intro

> Goal: explain why `task group` is still needed after `future` and `wait-source`, and how it uses `Close / ReapCompleted / Join / CreateChild` to close "a batch of child tasks that belong to one logical scope" in a structured way.

[中文](task-group-intro.md) | [Back to Guides](README.en.md)

---

## 0. What to Read First

If you have not built the overall XRT multitasking mental model yet, read these first:

- [Multitasking Overview](multitask-overview.en.md)
- [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md)
- [Wait-Source Intro](wait-source-intro.en.md)

This page focuses on the last layer:

- why "unified waiting" is still not enough without "structured closure"
- why `task group` is not a future array
- why `Close / Join / CreateChild` change how async code is organized


## 1. Why Are `future` and `wait-source` Still Not Enough?

First recall what the earlier layers solve:

- `future` solves "how do we represent a result uniformly?"
- `wait-source` solves "how do we wait through one unified entry?"

Together they already let you speak one more unified async language for:

- thread tasks
- coroutine tasks
- network waiting

But as soon as the program starts managing a real batch of child tasks, a new set of questions appears immediately:

1. do these futures belong to the same batch of work or not?
2. when is it still legal to add more tasks into this batch?
3. when should the code explicitly declare "this scope will not grow anymore"?
4. who performs the final join?
5. when the parent is canceled, how do child tasks close together?
6. when a child scope creates its own children, how does the outer scope know when everything is truly done?

Without a formal scope object, the code quickly degrades into:

- `xfuture*` scattered everywhere
- arrays assembled manually outside
- sometimes `WhenAll`, sometimes manual counters
- sometimes an ad-hoc "closed, do not accept new tasks" flag

That is exactly why `task group` exists.


## 2. What Problem Does `task group` Really Answer?

`task group` does not exist to:

- provide one more future combinator

It really answers this question:

> when a batch of child tasks belongs to one logical scope, how do we treat them as one scope for close, wait, cancel, reap, and parent propagation?

So the steadier mental model is:

- `future`: one result object
- `wait-source`: one waiting object
- `task group`: one scope object for a batch of child tasks

These are not competing abstractions. They are layered abstractions.


## 3. How Should `future`, `wait-source`, and `task group` Divide the Work?

| Object | The question it answers | What you really gain |
|--------|-------------------------|----------------------|
| `future` | what is the result? | state, value, error, cancellation, continuation |
| `wait-source` | what are we waiting on right now? | unified wait entry, unified timeout/deadline, unified coroutine waiting |
| `task group` | how does this batch of tasks close down? | scope, close, reap, join, cancel, parent/child propagation |

One-sentence version:

> `future` carries results, `wait-source` unifies waiting, and `task group` provides structured closure.


## 4. 7 Rules to Remember First

The most important thing on this page is not function names. It is the contract.

### 4.1 While the Group Is Open, the Scope Can Still Grow

As long as the group is not `Close`d yet, you may still:

- `xTaskGroupAddFuture()`
- `xTaskGroupRunThread()`
- `xTaskGroupRunCo()`
- `xTaskGroupRunEngine()`
- `xTaskGroupRunDelayed()`

So this is not a static array. It is a dynamic scope.


### 4.2 `Close` Means "Do Not Accept New Children Anymore"

The core meaning of `xTaskGroupClose()` is:

- close the intake side
- reject future child additions

It does **not** mean:

- destroy the group immediately
- cancel all currently pending children immediately

That distinction matters a lot.


### 4.3 `Wait*` Is Better Understood as "Observe That Current Members Reached Terminal States"

From the current API document and tests, the steadier reading is:

- `xTaskGroupWait / WaitTimeout / WaitUntil`
	- fit "wait until the currently tracked children all reach terminal states"
	- do not require the group to be `Close`d first
	- do not mean the group itself has finished final closure

So `Wait*` behaves more like a barrier than a final join.


### 4.4 `JoinFuture / Join*` Is the Close-Aware Final Barrier

The current test contract already makes this clear:

- while the group is still open, `xTaskGroupJoinFuture()` does not complete early
- children added later are still included in the join
- the join only completes after `Close` and after no pending child remains

So:

- `Wait*` fits "is this round done yet?"
- `Join*` fits "when is this whole scope truly finished?"


### 4.5 `ReapCompleted` Is for Long-Lived Open Groups

If the group stays open for a long time but some children have already completed, use:

- `xTaskGroupReapCompleted()`

to remove finished children from the group's tracked set and avoid piling up completed members in statistics and memory.


### 4.6 `Destroy` Is Not a Plain Free

In the current mainline, understand:

- `xTaskGroupDestroy()`

as:

- `Close`
- cancel still-pending children
- destroy the group itself

But remember:

> destroying the group does not mean caller-held `xfuture*` references stop needing `xFutureRelease()`.

If you still hold child futures yourself, you still release them yourself later.


### 4.7 Prefer `CreateChild` for Nested Scopes

If the program is clearly building a parent/child scope structure, prefer:

- `xTaskGroupCreateChild()`

instead of manually assembling:

- `BindParent`
- `AddFuture`
- custom parent join tracking

That is the more formal and less fragile entry in the current mainline.


## 5. First Recognize the Core API Set

The most frequently used entry points are:

```c
XXAPI xtaskgroup* xTaskGroupCreate(void);
XXAPI void xTaskGroupDestroy(xtaskgroup* pGroup);
XXAPI void xTaskGroupClose(xtaskgroup* pGroup);

XXAPI bool xTaskGroupAddFuture(xtaskgroup* pGroup, xfuture* pFuture);
XXAPI int xTaskGroupCount(xtaskgroup* pGroup);
XXAPI int xTaskGroupReapCompleted(xtaskgroup* pGroup);

XXAPI xfuture* xTaskGroupJoinFuture(xtaskgroup* pGroup);
XXAPI bool xTaskGroupJoin(xtaskgroup* pGroup);
XXAPI bool xTaskGroupJoinTimeout(xtaskgroup* pGroup, int64 iTimeoutMs);

XXAPI bool xTaskGroupWait(xtaskgroup* pGroup);
XXAPI bool xTaskGroupWaitTimeout(xtaskgroup* pGroup, int64 iTimeoutMs);

XXAPI void xTaskGroupCancel(xtaskgroup* pGroup);
XXAPI xtaskgroup* xTaskGroupCreateChild(xtaskgroup* pParent);

XXAPI xfuture* xTaskGroupRunThread(xtaskgroup* pGroup, xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize);
XXAPI xfuture* xTaskGroupRunCo(xtaskgroup* pGroup, xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize);
XXAPI xfuture* xTaskGroupRunEngine(xtaskgroup* pGroup, xnetengine* pEngine, uint32 iAffinityKey, xtask_engine_fn pfnTask, ptr pArg);
```

At the first stage, prioritize:

- `Create / Close / Destroy`
- `RunThread / AddFuture`
- `Wait* / Join*`
- `ReapCompleted`
- `CreateChild`


## 6. Demo 1: The Smallest Scope, Learn `Close + Join` First

Do not start with nested scopes immediately.

First learn the simplest version of "these tasks belong to one scope, and later they must be closed together".

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iInput;
	int32 iOutput;
} DemoTaskGroupJob;

static int32 procSquareTask(ptr pArg, xfuture_result* pOut)
{
	DemoTaskGroupJob* pJob = (DemoTaskGroupJob*)pArg;

	if ( (pJob == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(100);
	pJob->iOutput = pJob->iInput * pJob->iInput;
	pOut->pValue = pJob;
	return XRT_NET_OK;
}

int main(void)
{
	xtaskgroup* pGroup;
	xfuture* pA;
	xfuture* pB;
	DemoTaskGroupJob tJobA;
	DemoTaskGroupJob tJobB;

	xrtInit();

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		xrtUnit();
		return 1;
	}

	tJobA.iInput = 10;
	tJobA.iOutput = 0;
	tJobB.iInput = 20;
	tJobB.iOutput = 0;

	pA = xTaskGroupRunThread(pGroup, procSquareTask, &tJobA, 0);
	pB = xTaskGroupRunThread(pGroup, procSquareTask, &tJobB, 0);
	if ( (pA == NULL) || (pB == NULL) ) {
		if ( pA ) xFutureRelease(pA);
		if ( pB ) xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	xTaskGroupClose(pGroup);

	if ( !xTaskGroupJoinTimeout(pGroup, 3000) ) {
		xFutureRelease(pA);
		xFutureRelease(pB);
		xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	printf("job-a = %d\n", (int)((DemoTaskGroupJob*)xFutureValue(pA))->iOutput);
	printf("job-b = %d\n", (int)((DemoTaskGroupJob*)xFutureValue(pB))->iOutput);

	xFutureRelease(pA);
	xFutureRelease(pB);
	xTaskGroupDestroy(pGroup);
	xrtUnit();
	return 0;
}
```

The main mental model here is:

1. `RunThread` only adds child tasks into the group
2. `Close` announces "this scope will not accept new tasks anymore"
3. `JoinTimeout` is what finally closes this batch structurally

If you remember only one sequence, remember:

> create group -> add children -> `Close` -> `Join`


## 7. Demo 2: Why `JoinFuture` Is Not a Plain `WhenAll`

This part is crucial because it determines whether you misread `task group` as a future array.

This demo intentionally avoids `RunThread` and uses `future + promise` directly so the real behavior is easier to see:

- while the group is open, join does not complete early
- newly added futures continue to extend the join
- the true completion point appears only after `Close`

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xtaskgroup* pGroup;
	xfuture* pA;
	xfuture* pB;
	xfuture* pJoin;
	xpromise* pPromiseA;
	xpromise* pPromiseB;
	int iValueA;
	int iValueB;

	xrtInit();

	pGroup = xTaskGroupCreate();
	pA = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	iValueA = 1301;

	if ( (pGroup == NULL) || (pA == NULL) || (pPromiseA == NULL) ) {
		if ( pPromiseA ) xPromiseDestroy(pPromiseA);
		if ( pA ) xFutureRelease(pA);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	xTaskGroupAddFuture(pGroup, pA);
	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pJoin == NULL ) {
		xPromiseDestroy(pPromiseA);
		xFutureRelease(pA);
		xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	(void)xPromiseResolve(pPromiseA, &iValueA);

	if ( !xFutureWaitTimeout(pJoin, 20) ) {
		printf("join is still pending because group is still open\n");
	}

	pB = xFutureCreate();
	pPromiseB = xPromiseCreate(pB);
	iValueB = 1302;

	if ( (pB == NULL) || (pPromiseB == NULL) || !xTaskGroupAddFuture(pGroup, pB) ) {
		if ( pPromiseB ) xPromiseDestroy(pPromiseB);
		if ( pB ) xFutureRelease(pB);
		xFutureRelease(pJoin);
		xPromiseDestroy(pPromiseA);
		xFutureRelease(pA);
		xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	(void)xPromiseResolve(pPromiseB, &iValueB);

	xTaskGroupClose(pGroup);

	if ( xFutureWaitTimeout(pJoin, 1000) ) {
		printf("join completes only after close\n");
	}

	(void)xTaskGroupJoinTimeout(pGroup, 1000);

	xFutureRelease(pJoin);
	xPromiseDestroy(pPromiseA);
	xPromiseDestroy(pPromiseB);
	xFutureRelease(pA);
	xFutureRelease(pB);
	xTaskGroupDestroy(pGroup);
	xrtUnit();
	return 0;
}
```

What matters most:

- `JoinFuture` observes the whole scope
- if the scope is still open, it still assumes new children may appear
- so it does not behave like a static `WhenAll` that completes as soon as the current set finishes

That is the real meaning of a close-aware dynamic join.


## 8. Demo 3: Why Long-Lived Open Groups Need `ReapCompleted`

If your group is not "create one batch and close immediately", but more like a long-lived coordinator:

- it keeps accepting new children over time
- some children have already completed
- some children are still running

then completed members should not just pile up inside the group forever.

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xtaskgroup* pGroup;
	xfuture* pDone;
	xfuture* pPending;
	xpromise* pDonePromise;
	xpromise* pPendingPromise;
	int iValue;
	int iReaped;

	xrtInit();

	pGroup = xTaskGroupCreate();
	pDone = xFutureCreate();
	pPending = xFutureCreate();
	pDonePromise = xPromiseCreate(pDone);
	pPendingPromise = xPromiseCreate(pPending);
	iValue = 1201;

	if ( (pGroup == NULL) || (pDone == NULL) || (pPending == NULL) || (pDonePromise == NULL) || (pPendingPromise == NULL) ) {
		if ( pDonePromise ) xPromiseDestroy(pDonePromise);
		if ( pPendingPromise ) xPromiseDestroy(pPendingPromise);
		if ( pDone ) xFutureRelease(pDone);
		if ( pPending ) xFutureRelease(pPending);
		if ( pGroup ) xTaskGroupDestroy(pGroup);
		xrtUnit();
		return 1;
	}

	xTaskGroupAddFuture(pGroup, pDone);
	xTaskGroupAddFuture(pGroup, pPending);

	(void)xPromiseResolve(pDonePromise, &iValue);

	iReaped = xTaskGroupReapCompleted(pGroup);
	printf("reaped = %d\n", iReaped);
	printf("count after reap = %d\n", xTaskGroupCount(pGroup));

	(void)xPromiseResolve(pPendingPromise, &iValue);

	xTaskGroupClose(pGroup);
	(void)xTaskGroupJoinTimeout(pGroup, 1000);

	xPromiseDestroy(pDonePromise);
	xPromiseDestroy(pPendingPromise);
	xFutureRelease(pDone);
	xFutureRelease(pPending);
	xTaskGroupDestroy(pGroup);
	xrtUnit();
	return 0;
}
```

There are 2 main points here.

### 8.1 `ReapCompleted` Is Not `Join`

It does:

- remove already completed children from the group's current tracked set

It does not:

- declare the scope closed
- wait for the entire scope to finish


### 8.2 It Fits Long-Lived Open Coordinators

If the group still intends to accept new children, then:

- `Close + Join` is too early

The steadier order is usually:

1. keep accepting children
2. call `ReapCompleted` periodically
3. only at a real end point do `Close + Join`


## 9. Demo 4: Nested Scope, and Why `CreateChild` Should Be Preferred

What turns `task group` from "manage one batch of tasks" into "the entry to structured concurrency" is the child scope.

This demo focuses on:

- both parent and child being scopes
- child work being pulled into the parent's final join
- parent closure pulling the child scope into structured shutdown

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xtaskgroup* pParent;
	xtaskgroup* pChild;
	xfuture* pLeaf;
	xfuture* pParentJoin;
	xpromise* pLeafPromise;

	xrtInit();

	pParent = xTaskGroupCreate();
	pChild = xTaskGroupCreateChild(pParent);
	pLeaf = xFutureCreate();
	pLeafPromise = xPromiseCreate(pLeaf);

	if ( (pParent == NULL) || (pChild == NULL) || (pLeaf == NULL) || (pLeafPromise == NULL) ) {
		if ( pLeafPromise ) xPromiseDestroy(pLeafPromise);
		if ( pLeaf ) xFutureRelease(pLeaf);
		if ( pChild ) xTaskGroupDestroy(pChild);
		if ( pParent ) xTaskGroupDestroy(pParent);
		xrtUnit();
		return 1;
	}

	xTaskGroupAddFuture(pChild, pLeaf);
	pParentJoin = xTaskGroupJoinFuture(pParent);
	if ( pParentJoin == NULL ) {
		xPromiseDestroy(pLeafPromise);
		xFutureRelease(pLeaf);
		xTaskGroupDestroy(pChild);
		xTaskGroupDestroy(pParent);
		xrtUnit();
		return 1;
	}

	xTaskGroupClose(pParent);

	if ( xFutureWaitTimeout(pParentJoin, 1000) ) {
		printf("parent scope joined\n");
	}

	if ( xFutureState(pLeaf) == XFUTURE_CANCELLED ) {
		printf("leaf child was cancelled by parent scope close\n");
	}

	xFutureRelease(pParentJoin);
	xPromiseDestroy(pLeafPromise);
	xFutureRelease(pLeaf);
	xTaskGroupDestroy(pChild);
	xTaskGroupDestroy(pParent);
	xrtUnit();
	return 0;
}
```

From the current API document and test contract, the most important things to remember are:

- the child automatically binds into the parent scope
- the child join is automatically included in the parent join
- parent `Close / Cancel / Destroy` propagates closure semantics to the child

That is why the current mainline strongly prefers:

- `CreateChild`

instead of manually assembling the parent/child relationship.


## 10. How Should `Wait`, `Join`, `Cancel`, and `Destroy` Be Chosen?

These 4 actions are the easiest to blur together.

| Action | Better fit | What it should not be mistaken for |
|--------|------------|------------------------------------|
| `Wait*` | I only want to know whether the current members all reached terminal states | the final scope join |
| `JoinFuture / Join*` | I am ready to close the whole scope structurally | a plain `WhenAll` |
| `Cancel` | I want to send cancellation to the currently pending children | automatic destruction of the group |
| `Destroy` | I no longer need this scope object | a plain free with no closure semantics |

If you remember only one rule of thumb:

- use `Wait*` for "is this round done yet?"
- use `Close + Join*` for "this whole scope ends here"


## 11. When Would `BindParent` Still Be Used?

For most formal nested-scope cases, prefer:

- `xTaskGroupCreateChild()`

But if you already have an external parent future and want its cancellation to propagate into this child scope, then consider:

- `xTaskGroupBindParent()`

For example:

```c
xtaskgroup* pGroup = xTaskGroupCreate();

xTaskGroupBindParent(pGroup, pParentFuture);
xTaskGroupAddFuture(pGroup, pChildFuture);
```

From the current test contract:

- one group may bind to multiple parent futures
- cancellation of any one of them may propagate into the current child scope

So this is better suited to:

- integrating with an existing upstream future tree

and not as a first learning entry.


## 12. Windows / Linux Cross-Platform Notes

### 12.1 The Group Does Not Own the Futures You Still Hold Yourself

For example:

- `xTaskGroupRunThread()` returns an `xfuture*`
- if you still hold it in caller code, you still `xFutureRelease()` it yourself later

Do not assume:

- the group was destroyed
- so your own reference stops needing cleanup


### 12.2 `Close` Is Not `Cancel`

This is one of the easiest mistakes to make.

- `Close`
	- do not accept new children anymore
- `Cancel`
	- actively send cancellation to the currently pending children

If both are treated as the same operation, cross-platform debugging becomes painful because you will see:

- the group is already closed
- but a child is still legitimately running toward its terminal state


### 12.3 Long-Lived Groups Should Periodically `ReapCompleted`

Windows and Linux may differ in scheduling timing, but this structural advice does not change:

- if the group stays open for a long time
- and children keep completing

then do not let it grow forever without reaping.


## 13. Common Mistakes

### 13.1 Mistake 1: Treat `task group` as a Future Array

Then you ignore:

- `Close`
- `JoinFuture`
- `CreateChild`
- cancellation propagation

and those are exactly the most important parts.


### 13.2 Mistake 2: Forget `Close`, but Wait on `JoinFuture`

If the group remains open forever, then according to the current contract:

- `JoinFuture` keeps assuming new children may still appear

That often gets misread as "the program is stuck".


### 13.3 Mistake 3: Assume `Close` Automatically Cancels Current Children

It does not.

If you really want cancellation:

- use `xTaskGroupCancel()`
- or eventually `Destroy()`


### 13.4 Mistake 4: Keep Long-Lived Open Groups Without `ReapCompleted`

Then completed members keep hanging around in the group's tracked set, and it becomes harder to see:

- how many pending children are truly left
- what state the scope is really in now


### 13.5 Mistake 5: Manually Assemble Parent/Child Relationships for a Real Nested Scope

If the shape is naturally a parent/child scope, prefer:

- `xTaskGroupCreateChild()`

That is steadier than manually maintaining parent bind, join add, and cancel propagation.


## 14. What to Read Next

Recommended order:

1. [Future / Task / Promise](../api/api-future-task-promise.en.md)
2. [Case: Thread / Coroutine / Future Coordination](../case/thread-coroutine-future.en.md)
3. [Wait-Source Intro](wait-source-intro.en.md)
4. [Multitasking Overview](multitask-overview.en.md)

---

**One-sentence summary:** `task group` is not a future container. It is the structured scope of a batch of child tasks. `future` carries results, `wait-source` unifies waiting, and `task group` decides when no new work may appear, when the scope truly joins, and how parent and child scopes close together.
