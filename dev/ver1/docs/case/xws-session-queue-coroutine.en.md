# XRT Case Study: XWS Session Skeleton with Queue and Coroutine

> A stable session-service mainline under the current public API: let WebSocket callbacks own protocol boundaries, let a queue own message handoff, and let coroutines own the sequential session flow.

[中文](xws-session-queue-coroutine.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you are building a long-lived WebSocket session service with these constraints:

1. connection setup, handshake, ping/pong, and close should stay inside a formal protocol layer
2. receiving one text message should not immediately turn into heavy work inside the callback
3. the application layer wants to hand messages off first, then process them at its own pace
4. the main session flow should read like "wait until open -> send -> wait reply -> decide next step"
5. the same runtime semantics should work on both Windows and Linux
6. the documentation must stay strictly on top of the public interfaces already exposed in `xrt.h`

Without one clear mainline, this usually degrades into:

- database, template, file, or HTTP work directly inside `OnText`
- callbacks that become too long to debug cleanly
- scattered flags, polling loops, and shared strings just to wait for session events
- pretending `xws` is already a full `future / wait-source` module when the current public API is not

This case explains how a WebSocket session service should be structured today on top of the stable public surface.

---

## 2. Why This Mainline

### Why not put all logic inside callbacks?

Because the current formal `xws` contract is:

- protocol and connection management happen through event callbacks
- upper-layer message organization is still your responsibility

Callbacks are a good place for:

- copying the input
- doing minimal validation
- sending fast feedback such as `ACK`, `busy`, or `closing`

They are not the right place for:

- slow business work
- blocking waits
- the whole session orchestration

### Why use both `queue` and `xcoevent`?

Because they do not solve the same problem.

| Object | Role in this case | What it cannot replace |
|--------|-------------------|------------------------|
| `xmpscqwait` | owns the actual queued message objects | it does not wake a coroutine by itself |
| `xcoevent` | wakes the coroutine scheduler across threads | it does not carry the message payload |

One-sentence version:

> the queue makes sure the message is not lost, and the event makes sure the coroutine wakes up at the right time.

### Why not describe `xws` as `future / wait-source` directly?

Because that is not the current formal public mainline.

So this page intentionally teaches the stable bridge that exists today:

> `xws callback -> queue -> coroutine`

---

## 3. What Each Layer Does

| Layer | Role in this case | What it really gives you |
|------|-------------------|--------------------------|
| `xwsserver` / `xwsclient` | handshake, send/receive, close, ping/pong | the formal WebSocket protocol layer |
| `OnText` / `OnOpen` / `OnClose` | minimum protocol boundary and quick feedback | a clean way to bridge wire events into the application |
| `xmpscqwait` | cross-thread message handoff | explicit ownership and visible full/closed states |
| `xcoevent` | wake the coroutine from the callback side | no busy polling and no thread blocking |
| `xcosched` | single-thread orchestration of the main flow | sequential session scripting and audit logic |

One-sentence version:

> `xws` owns the wire protocol, `queue` owns message handoff, and `coroutine` owns how the session flow is written.

---

## 4. Code Skeleton

The reviewed Chinese page contains the full version. The skeleton below keeps the same boundary design:

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	char sText[256];
} DemoQueuedText;

typedef struct
{
	xmpscqwait hAuditQueue;
	xcoevent hAuditEvent;
} DemoWsServerCtx;

typedef struct
{
	xmpscqwait hInboxQueue;
	xcoevent hOpenEvent;
	xcoevent hInboxEvent;
	xcoevent hCloseEvent;
	xwsclient* pClient;
} DemoWsClientCtx;

static xqueue_result procQueueTextPush(xmpscqwait hQueue, xcoevent hEvent, const char* pData, size_t iLen)
{
	DemoQueuedText* pMsg;

	if ( (hQueue == NULL) || (pData == NULL) ) {
		return XQUEUE_ERROR;
	}

	pMsg = (DemoQueuedText*)xrtMalloc(sizeof(DemoQueuedText));
	if ( pMsg == NULL ) {
		return XQUEUE_ERROR;
	}

	memset(pMsg, 0, sizeof(DemoQueuedText));
	memcpy(pMsg->sText, pData, (iLen < (sizeof(pMsg->sText) - 1u)) ? iLen : (sizeof(pMsg->sText) - 1u));

	if ( xrtMPSCQWaitTryPush(hQueue, pMsg) != XQUEUE_OK ) {
		xrtFree(pMsg);
		return XQUEUE_ERROR;
	}

	if ( hEvent != NULL ) {
		xrtCoEventSet(hEvent);
	}
	return XQUEUE_OK;
}

static void procServerOnText(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen)
{
	DemoWsServerCtx* pCtx = (DemoWsServerCtx*)pOwner;
	char sAck[320];

	(void)pServer;

	if ( (pCtx == NULL) || (pConn == NULL) || (pData == NULL) ) {
		return;
	}

	if ( procQueueTextPush(pCtx->hAuditQueue, pCtx->hAuditEvent, pData, iLen) == XQUEUE_OK ) {
		snprintf(sAck, sizeof(sAck), "queued:%.*s", (int)iLen, pData);
	} else {
		snprintf(sAck, sizeof(sAck), "busy");
	}

	(void)xrtWsConnSendText(pConn, sAck, strlen(sAck));
}

static void procClientOnOpen(ptr pOwner, xwsclient* pClient)
{
	DemoWsClientCtx* pCtx = (DemoWsClientCtx*)pOwner;

	(void)pClient;

	if ( (pCtx != NULL) && (pCtx->hOpenEvent != NULL) ) {
		xrtCoEventSet(pCtx->hOpenEvent);
	}
}

static void procClientOnText(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen)
{
	DemoWsClientCtx* pCtx = (DemoWsClientCtx*)pOwner;

	(void)pClient;
	(void)procQueueTextPush(pCtx->hInboxQueue, pCtx->hInboxEvent, pData, iLen);
}

static void procClientOnClose(ptr pOwner, xwsclient* pClient, xnet_result iReason)
{
	DemoWsClientCtx* pCtx = (DemoWsClientCtx*)pOwner;

	(void)pClient;
	(void)iReason;

	if ( (pCtx != NULL) && (pCtx->hCloseEvent != NULL) ) {
		xrtCoEventSet(pCtx->hCloseEvent);
	}
}

static void procClientScriptCo(ptr pArg)
{
	DemoWsClientCtx* pCtx = (DemoWsClientCtx*)pArg;

	if ( (pCtx == NULL) || (pCtx->pClient == NULL) ) {
		return;
	}

	if ( !xrtCoWaitEventTimeout(pCtx->hOpenEvent, 3000u) ) {
		return;
	}

	(void)xrtWsClientSendText(pCtx->pClient, "login:user-1", 12u);
	(void)xrtCoWaitEventTimeout(pCtx->hInboxEvent, 3000u);

	(void)xrtWsClientSendText(pCtx->pClient, "say:hello-from-client", 21u);
	(void)xrtCoWaitEventTimeout(pCtx->hInboxEvent, 3000u);

	(void)xrtWsClientClose(pCtx->pClient, XWS_CLOSE_NORMAL, "demo-done");
	(void)xrtCoWaitEventTimeout(pCtx->hCloseEvent, 3000u);
}
```

---

## 5. How to Read the Skeleton

### 5.1 Start with the server `OnText`

This callback intentionally does only 3 things:

1. copy the incoming text into a heap-owned message
2. push that message into the audit queue
3. send an immediate `queued:*` or `busy` reply

The real point is:

- callbacks own protocol boundaries
- queues own message handoff
- heavy business logic should not block the WebSocket callback

### 5.2 Then look at `queue + event`

The most important boundary in this page is not merely "whether we use coroutines".

It is the pair:

- `xmpscqwait` owns queued messages
- `xcoevent` wakes the coroutine when those messages arrive

If you only have a queue without an event, the coroutine side tends to fall back to busy polling.
If you only have an event without a queue, the wake-up arrives but no formal payload exists.

### 5.3 Why the client flow is written as a coroutine

The client flow is intentionally written as:

1. wait for `OnOpen`
2. send `login`
3. wait for inbox
4. send `chat`
5. actively close

That is the real value of coroutines in this chain:

- not to replace the WebSocket protocol layer
- but to turn "wait for an event, then continue" into sequential code

### 5.4 Why quick replies stay in the callback

The current public docs do not formally promise that:

- `xwsconn*` can be used as an arbitrary cross-thread send handle

So this sample deliberately follows the conservative rule:

- keep quick replies inside `OnText`
- hand slow work over to the coroutine side through the queue

---

## 6. How to Upgrade This Mainline

If the audit coroutine later needs slower work such as:

- file writes
- database access
- HTTP calls

the stable upgrade is to keep:

- `xws` for protocol events
- `queue` for message handoff
- `coroutine` for sequential orchestration

and then attach:

- `future / task`

for the genuinely slow execution unit.

If the connection must move to TLS or proxy routing, the next upgrades are:

- `wss://` plus `tServerCfg.pTlsConfig` on the server side
- `tClientCfg.pProxy = pProxy` on the client side

That keeps the session service on the same networking mainline used by the HTTP client case.

---

## 7. Common Mistakes

### Mistake 1: doing slow work directly inside `OnText`

That tangles protocol handling and business logic together immediately.

### Mistake 2: using events without a formal message queue

Then you wake up but still have no real payload to consume.

### Mistake 3: keeping callback-owned `pData` and using it later across threads

The safer pattern is the one shown here: copy it into your own message object first.

### Mistake 4: writing `xws` docs as if the module is already wait-source based

That is not the current public mainline.

### Mistake 5: calling `Close` and then destroying a queue that still contains undrained messages

Protocol close and application-side queue drain are not the same thing.

---

## 8. Suggested Reading

- [XWS API](../api/api-xws.en.md)
- [Guide: When to Use Queue Instead of Shared State](../guide/queue-intro.en.md)
- [Guide: Coroutine, Future, and Task Intro](../guide/coroutine-future-task-intro.en.md)
- [Guide: xnet-v2 and TLS Session Intro](../guide/xnet-v2-tls-intro.en.md)
- [Case: XHTTP Client Chain with URL, Proxy, and TLS](xhttp-client-proxy-tls.en.md)
- [Case: Streaming LLM API with XRT](streaming-llm-api.en.md)

---

**One-sentence summary:** the key is not simply "connect a WebSocket"; the key is to let `xws` own protocol boundaries, let `queue` formally take over message ownership, and let `coroutine` turn the session flow into sequential code.
