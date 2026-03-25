# Queue Module

> XRT's current formal bounded pointer-queue mainline, providing `SPSC / MPSC / MPMC` plus an optional `MPSC wait wrapper`.

[中文](api-queue.md) | [English](api-queue.en.md) | [Back to Index](README.en.md)

---

## 1. Positioning

This module is meant for:

- FIFO handoff between threads
- `xnet` worker command queues
- high-concurrency pointer passing
- bounded queues with explicit close / drain / reset lifecycle

Current mainline traits:

- fixed capacity
- capacity rounded up to a power of two
- pointer FIFO only, no payload ownership management
- separate `SPSC / MPSC / MPMC` concurrency families
- optional `MPSC wait wrapper` for blocking consumers

Explicitly out of scope today:

- unbounded queues
- priority queues
- delayed queues
- built-in object destruction or refcounting


## 2. Core Types

### `xqueue_result`

```c
typedef enum xqueue_result
{
	XQUEUE_OK = 0,
	XQUEUE_EMPTY = 1,
	XQUEUE_FULL = 2,
	XQUEUE_CLOSED = 3,
	XQUEUE_TIMEOUT = 4,
	XQUEUE_ERROR = -1
} xqueue_result;
```

Common meanings:

- `XQUEUE_OK`: operation succeeded
- `XQUEUE_EMPTY`: queue is currently empty
- `XQUEUE_FULL`: queue is currently full
- `XQUEUE_CLOSED`: queue has been closed for that operation path
- `XQUEUE_TIMEOUT`: wait-based API timed out
- `XQUEUE_ERROR`: invalid argument or internal failure


### `xqueue_kind`

```c
typedef enum xqueue_kind
{
	XQUEUE_KIND_SPSC = 1,
	XQUEUE_KIND_MPSC = 2,
	XQUEUE_KIND_MPMC = 3
} xqueue_kind;
```

Used to identify the underlying queue family.


### `xqueuebase`

```c
typedef struct xqueuebase_struct
{
	uint32 iKind;
	volatile uint32 bClosed;
} xqueuebase;
```

Used for:

- generic close/drained helpers
- the common header embedded in concrete queue structs


### `xqueue_config`

```c
typedef struct xqueue_config
{
	uint32 iCapacity;
	uint32 iFlags;
} xqueue_config;
```

Notes:

- `iCapacity`: requested capacity, rounded up to a power of two
- `iFlags`: currently reserved, keep it at `0`


### Queue Handles

- `xspscq`
	- single producer, single consumer
- `xmpscq`
	- multi producer, single consumer
- `xmpmcq`
	- multi producer, multi consumer
- `xmpscqwait`
	- waitable wrapper built on top of `xmpscq`


### `xqueue_drain_fn`

```c
typedef void (*xqueue_drain_fn)(ptr pItem, ptr pUserData);
```

Used by `Drain` APIs to consume remaining items.


## 3. Common Contracts

- `iCapacity == 0` or exceeding the implementation limit fails initialization
- actual capacity is rounded up to the next power of two
- this is a pointer queue; payload lifetime stays with the caller
- `Close` forbids further `Push`
- after `Close`, already published items can still be `Pop`ped or `Drain`ed
- `Drained` means "closed and already emptied"
- `ApproxCount` is observational only under concurrency, especially for `MPSC / MPMC`
- `Reset` is only valid after the queue is closed and drained

Trimming macros:

- `XRT_NO_QUEUE`
	- removes the queue module
- `XRT_NO_QUEUE_WAIT`
	- keeps core queues but removes the wait wrapper


## 4. Common Helper API

```c
XXAPI bool xrtQueueIsClosed(const xqueuebase* pQueue);
XXAPI bool xrtQueueIsDrained(const xqueuebase* pQueue);
```

Recommended use:

- shutdown / drain flow checks
- not as a replacement for proper synchronization


## 5. SPSC

Good for:

- one producer thread
- one consumer thread
- the lightest hot path

```c
XXAPI bool xrtSPSCQInit(xspscq pQueue, const xqueue_config* pCfg);
XXAPI void xrtSPSCQUnit(xspscq pQueue);
XXAPI xspscq xrtSPSCQCreate(const xqueue_config* pCfg);
XXAPI void xrtSPSCQDestroy(xspscq pQueue);
XXAPI xqueue_result xrtSPSCQTryPush(xspscq pQueue, ptr pItem);
XXAPI xqueue_result xrtSPSCQTryPop(xspscq pQueue, ptr* ppItem);
XXAPI uint32 xrtSPSCQApproxCount(xspscq pQueue);
XXAPI void xrtSPSCQClose(xspscq pQueue);
XXAPI uint32 xrtSPSCQDrain(xspscq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
XXAPI bool xrtSPSCQReset(xspscq pQueue);
```


## 6. MPSC

Good for:

- many concurrent producers
- one consumer
- worker command-queue style traffic

```c
XXAPI bool xrtMPSCQInit(xmpscq pQueue, const xqueue_config* pCfg);
XXAPI void xrtMPSCQUnit(xmpscq pQueue);
XXAPI xmpscq xrtMPSCQCreate(const xqueue_config* pCfg);
XXAPI void xrtMPSCQDestroy(xmpscq pQueue);
XXAPI xqueue_result xrtMPSCQTryPush(xmpscq pQueue, ptr pItem);
XXAPI xqueue_result xrtMPSCQTryPop(xmpscq pQueue, ptr* ppItem);
XXAPI uint32 xrtMPSCQPushBatch(xmpscq pQueue, ptr* arrItems, uint32 iCount);
XXAPI uint32 xrtMPSCQPopBatch(xmpscq pQueue, ptr* arrItems, uint32 iCap);
XXAPI uint32 xrtMPSCQApproxCount(xmpscq pQueue);
XXAPI void xrtMPSCQClose(xmpscq pQueue);
XXAPI uint32 xrtMPSCQDrain(xmpscq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
XXAPI bool xrtMPSCQReset(xmpscq pQueue);
```

Batch notes:

- the return value is the number of items actually pushed or popped
- no extra result enum is introduced
- partial success is a normal outcome


## 7. MPMC

Good for:

- many producers
- many consumers
- general concurrent message passing

```c
XXAPI bool xrtMPMCQInit(xmpmcq pQueue, const xqueue_config* pCfg);
XXAPI void xrtMPMCQUnit(xmpmcq pQueue);
XXAPI xmpmcq xrtMPMCQCreate(const xqueue_config* pCfg);
XXAPI void xrtMPMCQDestroy(xmpmcq pQueue);
XXAPI xqueue_result xrtMPMCQTryPush(xmpmcq pQueue, ptr pItem);
XXAPI xqueue_result xrtMPMCQTryPop(xmpmcq pQueue, ptr* ppItem);
XXAPI uint32 xrtMPMCQPushBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCount);
XXAPI uint32 xrtMPMCQPopBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCap);
XXAPI uint32 xrtMPMCQApproxCount(xmpmcq pQueue);
XXAPI void xrtMPMCQClose(xmpmcq pQueue);
XXAPI uint32 xrtMPMCQDrain(xmpmcq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
XXAPI bool xrtMPMCQReset(xmpmcq pQueue);
```


## 8. MPSC Wait Wrapper

Available only when `XRT_NO_QUEUE_WAIT` is not defined.

```c
XXAPI bool xrtMPSCQWaitInit(xmpscqwait pQueue, const xqueue_config* pCfg);
XXAPI void xrtMPSCQWaitUnit(xmpscqwait pQueue);
XXAPI xmpscqwait xrtMPSCQWaitCreate(const xqueue_config* pCfg);
XXAPI void xrtMPSCQWaitDestroy(xmpscqwait pQueue);
XXAPI xqueue_result xrtMPSCQWaitTryPush(xmpscqwait pQueue, ptr pItem);
XXAPI xqueue_result xrtMPSCQWaitTryPop(xmpscqwait pQueue, ptr* ppItem);
XXAPI xqueue_result xrtMPSCQWaitPop(xmpscqwait pQueue, ptr* ppItem);
XXAPI xqueue_result xrtMPSCQWaitPopTimeout(xmpscqwait pQueue, ptr* ppItem, uint32 iTimeoutMs);
XXAPI uint32 xrtMPSCQWaitApproxCount(xmpscqwait pQueue);
XXAPI void xrtMPSCQWaitClose(xmpscqwait pQueue);
```

Notes:

- storage remains `MPSC`
- the producer side stays concurrent
- the blocking pop path introduces wait primitives
- the internal pop path is serialized, so this is not a fully lock-free blocking queue
- `Close` wakes blocked waiters
- timeout APIs return `XQUEUE_TIMEOUT`


## 9. Common Usage

### 9.1 Basic SPSC Flow

```c
#include "xrt.h"
#include <string.h>

int main()
{
	xqueue_config tCfg;
	xspscq hQueue;
	ptr pItem = NULL;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 1000u;

	hQueue = xrtSPSCQCreate(&tCfg);
	if ( hQueue == NULL ) {
		xrtUnit();
		return 1;
	}

	xrtSPSCQTryPush(hQueue, (ptr)"hello");
	if ( xrtSPSCQTryPop(hQueue, &pItem) == XQUEUE_OK ) {
		/* pItem == "hello" */
	}

	xrtSPSCQClose(hQueue);
	xrtSPSCQDestroy(hQueue);
	xrtUnit();
	return 0;
}
```


### 9.2 Waitable MPSC Consumer

```c
xqueue_config tCfg;
xmpscqwait hQueue;
ptr pItem = NULL;

memset(&tCfg, 0, sizeof(tCfg));
tCfg.iCapacity = 1024u;

hQueue = xrtMPSCQWaitCreate(&tCfg);

if ( xrtMPSCQWaitTryPush(hQueue, pJob) == XQUEUE_OK ) {
	/* producer published successfully */
}

switch ( xrtMPSCQWaitPopTimeout(hQueue, &pItem, 500u) ) {
	case XQUEUE_OK:
		/* got one item */
		break;
	case XQUEUE_TIMEOUT:
		/* no item within 500ms */
		break;
	case XQUEUE_CLOSED:
		/* queue is closed and drained */
		break;
	default:
		break;
}

xrtMPSCQWaitClose(hQueue);
xrtMPSCQWaitDestroy(hQueue);
```


## 10. Usage Guidance

- choose the smallest concurrency family that matches the real topology
- prefer `TryPush / TryPop` on hot paths
- evaluate `PushBatch / PopBatch` only when throughput pressure justifies it
- payload lifetime stays with the caller
- if you only need a blocking single-consumer path, use `xmpscqwait` instead of mixing wait logic into the core queue
