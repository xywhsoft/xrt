# XRT Guide: XWS Intro

> Goal: turn `xrtWsClientCreate()/Start()`, `xrtWsServerCreate()/Start()`, `xrtWsClientSendText()`, and `xrtWsConnSendText()` into the formal first page of the Stage 6 WebSocket session layer. After this page, you should clearly understand why the current formal `xws` mainline is still event-driven, what the client and server objects are each responsible for, and when a WebSocket session has already outgrown the "put all business logic inside `OnText`" style.

[中文](xws-intro.md) | [Back to Guides](README.en.md)

---

## 1. Why Does `xws` Need Its Own Page?

Many people first see WebSocket and instinctively reduce it to:

- "a long-lived connection that can send strings both ways"

Then the code quickly drifts into this shape:

- all business logic lives inside `OnText`
- callbacks query the database, write files, and call HTTP directly
- client reply flow and server event flow become mixed together

That may run in the short term, but it becomes hard to maintain.

Because this layer is not only about:

- staying connected

It is also about:

- HTTP upgrade
- the `ws://` / `wss://` connection path
- ping / pong
- close
- fragmented-frame reassembly
- session boundaries on both client and server sides

That is where `xws` sits.


## 2. Separate 9 Boundaries First

### 2.1 `xws` Is a Protocol Session Layer, Not a Replacement for the Lower Network Layers

A useful split of the current network mainline is:

| Layer | Responsibility |
|-------|----------------|
| `xurl` | URL, authority, target |
| `xnet-v2` | engine, stream, connection runtime |
| `xtlssession` | TLS for `wss://` |
| `xws` | WebSocket handshake, frames, messages, and sessions |

So `xws` is not:

- a new socket layer

It is:

- the WebSocket protocol layer built on top of the current network mainline


### 2.2 The Current Formal `xws` Mainline Is Still Event-Driven

This is the most important premise of the page.

In the current public API surface, the mainline is built around callbacks such as:

- `OnOpen`
- `OnText`
- `OnBinary`
- `OnClose`
- `OnError`
- `OnPing`
- `OnPong`

So the current formal documentation should not pretend that `xws` already provides:

- a fully future-based send/receive surface
- a formal WebSocket session `wait-source` interface

Those may be future directions, but they are not the formal mainline today.


### 2.3 Client and Server Have Separate Object Boundaries

In the current public surface:

- client object
	- `xwsclient`
- server object
	- `xwsserver`
- one accepted server-side connection
	- `xwsconn`

The recommended reading is:

| Object | Role |
|--------|------|
| `xwsclient` | an actively connecting session object |
| `xwsserver` | the listening and service-management object |
| `xwsconn` | one accepted client connection on the server side |

So when the server sends a message, it does not send through `xwsserver`. It sends through:

- `xwsconn`


### 2.4 A Client Connection Is a Sequence, Not a Single Step

For either:

- `ws://`
- `wss://`

the steadier sequence is:

1. parse the URL first
2. if a proxy is configured, connect to the proxy and complete proxy handshake
3. if the scheme is `wss://`, establish TLS next
4. perform HTTP upgrade
5. enter WebSocket open state

In short:

- `TCP -> proxy handshake -> TLS -> HTTP upgrade -> WS open`

That is also why the earlier learning order matters:

- `xurl`
- `proxy`
- `xnet-v2 / TLS`


### 2.5 `xws` Owns the Protocol Boundary, Not Heavy Business Work

Callbacks are best used for:

- minimum parsing after receipt
- quick ACK behavior
- copying input
- pushing messages into a queue
- updating lightweight state

They are not a good place to permanently hold:

- heavy CPU work
- blocking I/O
- long transactions
- the full orchestration of a session flow

That is when work should move to:

- `queue`
- `coroutine`
- `future/task`

That is also why the formal case page uses:

- [Case: XWS Session Skeleton with Queue and Coroutine](../case/xws-session-queue-coroutine.md) (Chinese for now)


### 2.6 `pProxy` Exists Only on the Client Side

In the current public configs:

- `xwsclientconfig`
	- has `pProxy`
- `xwsserverconfig`
	- does not have `pProxy`

So today the proxy path mainly means:

- a WebSocket client connecting outward through a proxy

not:

- a listening server also carrying `pProxy`


### 2.7 `wss://` Peer Verification Lives in Client Config

The current client config contains:

- `bVerifyPeer`

That means:

- the client explicitly decides whether peer certificates are verified strictly

In formal production scenarios, the recommended default is:

- `bVerifyPeer = true`

Only local debugging or temporary self-signed test setups justify loosening it.


### 2.8 The Current Mainline Covers Ordinary WebSocket, Not the Whole Browser-Extension Ecosystem

The current API and docs formally cover:

- text / binary
- ping / pong
- close
- fragmented-message reassembly

They should not be read as if they already formally cover:

- `permessage-deflate`
- complex extension negotiation
- the entire browser-grade extension ecosystem


### 2.9 `xws` and `xhttpd` Share Upgrade Semantics, but They Do Not Replace Each Other

The handshake stage does reuse:

- HTTP upgrade

But once the session reaches WebSocket open state, the semantics are no longer ordinary HTTP request/response semantics.

So do not reduce it to:

- "`xhttpd`, but with a long connection"


## 3. Minimal Demo: Bring Up the Smallest WebSocket Client

Do not build both client and server at once yet.  
First learn the minimum client path:

1. configure the URL
2. fill the event callbacks
3. `Create -> Start`
4. send after `OnOpen`

```c
static void procClientOnOpen(ptr pOwner, xwsclient* pClient)
{
	(void)pOwner;
	(void)xrtWsClientSendText(pClient, "hello", 5u);
}

static void procClientOnText(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen)
{
	(void)pOwner;
	(void)pClient;
	printf("recv: %.*s\n", (int)iLen, pData);
}

int main(void)
{
	xwsclientconfig tCfg;
	xwsclientevents tEvents;
	xwsclient* pClient;

	xrtInit();
	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnOpen = procClientOnOpen;
	tEvents.OnText = procClientOnText;

	xrtWsClientConfigInit(&tCfg);
	strcpy(tCfg.sURL, "wss://example.com/ws");
	tCfg.iConnectTimeoutMs = 10000u;
	tCfg.bVerifyPeer = true;

	pClient = xrtWsClientCreate(pEngine, &tCfg, &tEvents, NULL);
	xrtWsClientStart(pClient);
}
```

The point of this step is not business logic.  
It is to grasp the client lifecycle first.


## 4. Upgraded Demo: Add the Server-Side Boundary

Only in the second step should the server side enter:

1. bind an address
2. create the server
3. keep `OnOpen / OnText / OnClose` focused on protocol work
4. hand real business logic off through a queue

```c
static void procServerOnText(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen)
{
	(void)pOwner;
	(void)pServer;
	printf("server recv: %.*s\n", (int)iLen, pData);
	(void)xrtWsConnSendText(pConn, "queued", 6u);
}

int main(void)
{
	xwsserverconfig tCfg;
	xwsserverevents tEvents;
	xwsserver* pServer;

	xrtInit();
	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnText = procServerOnText;

	xrtWsServerConfigInit(&tCfg);
	xrtNetAddrInitAny(&tCfg.tBindAddr, AF_INET, 8080u);

	pServer = xrtWsServerCreate(pEngine, &tCfg, &tEvents, NULL);
	xrtWsServerStart(pServer);
}
```

The main thing to remember here is:

- server sends through `xwsconn`
- not through `xwsserver`


## 5. When Should Business Logic Move Out of the Callback?

As soon as your session logic needs any of the following, it should not all remain in `OnText`:

- queued message processing
- sequential orchestration of several session steps
- disk I/O, HTTP calls, database work, templating
- flows like "open connection -> send -> wait for reply -> decide the next step"

The steadier current mainline is:

- `xws callback -> queue -> coroutine`

The case page is the best next read:

- [Case: XWS Session Skeleton with Queue and Coroutine](../case/xws-session-queue-coroutine.md) (Chinese for now)


## 6. One Selection Table: Which Layer Should This Stop At?

| Scenario | Preferred entry |
|----------|-----------------|
| stable WS client/server infrastructure only | event-driven `xws` mainline |
| outbound WebSocket client through a proxy | `xwsclientconfig.pProxy` |
| secure session | `wss://` + `bVerifyPeer` |
| slow business work and message handoff | `xws + queue` |
| sequential orchestration of session flow | `xws + queue + coroutine` |


## 7. Common Mistakes

### 7.1 Presenting `xws` as If It Were Already Formally `future / wait-source` Based

That is not the current mainline.

The current formal public path is still callback-driven.


### 7.2 Doing All Heavy Work Inside `OnText`

That glues the protocol layer and the business layer together.


### 7.3 Sending from the Wrong Object on the Server Side

The server-side send/control path is:

- `xrtWsConnSendText()`
- `xrtWsConnSendBinary()`
- `xrtWsConnPing()`
- `xrtWsConnClose()`

not sending through `xwsserver` itself.


### 7.4 Ignoring the Certificate-Verification Boundary of `wss://`

Production code should not disable `bVerifyPeer` by default.


### 7.5 Treating Proxy Support as "Add One More HTTP Header"

The proxy really inserts itself into the connection sequence. It is not just one extra text fragment at the application layer.


## 8. What to Read Next

Follow this line next:

- [Case: XWS Session Skeleton with Queue and Coroutine](../case/xws-session-queue-coroutine.md) (Chinese for now)
- [XWS API](../api/api-xws.en.md)
- [xnet-v2 and TLS Session Intro](xnet-v2-tls-intro.en.md)
- [Proxy Intro: When the Proxy Is Just a Shared Object, and When It Becomes Part of the Connection Sequence](proxy-intro.en.md)
