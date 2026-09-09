---
num: 112
slug: xws-server
title: xws: Server Routing and Sessions
volume: 卷十一 其他扩展库
type: practice
lead: One registration serving both plaintext and TLS on a fixed endpoint, Origin policy and the authorize callback, staged low-level upgrade APIs, and the coroutine session form — xws concludes.
api: xws-websocket_server_router, xws-websocket_http, xhttp-http_server_router
---

## Orientation

xws's four chapters conclude. The previous three provided connections (109), sending (110), grouping (111); this chapter wires them onto the HTTP server: **the fixed WebSocket endpoint** (`xrtWsServerRoute` — one registration hangs on Chapter 104's HTTP Router; `ws` and `wss` are decided by how you start it, **no second registration needed**); **secure defaults** (Origin policy — the first gate against browser cross-origin attacks; the Authorize callback — the business authorization point); **the low-level upgrade** (`xrtWsServerCheck/Reply/Reject/Upgrade` staged APIs — when the fixed endpoint isn't enough, assemble directly inside an HTTP route callback); **the session form** (`session_channel_coroutine` — connection + Channel + coroutine turns per-connection business into sequential code; where Chapter 56 and this chapter meet). The endpoint's automatic responses (405/403/400/426/500) cover the full classification of protocol errors.

## Introduction

The "entrance engineering" of a WebSocket server holds more than appears: an arriving request first passes the HTTP layer (the method must be GET; no body declaration); then Origin validation (the server-side enforcement of the browser same-origin policy — cross-origin connections with a missing or forged Origin are the WebSocket variant of CSRF); then business authorization (may this user enter this channel); then subprotocol negotiation (Chapter 93); then the upgrade handshake; and only then does your Open callback receive the `xwsconn`. The fixed endpoint standardizes this pipeline — **secure by default** (same-origin policy out of the box), **errors automatic** (each failure class has a canonical response code), **lifecycle clear** (Open borrows the connection; to keep it you must `xrtWsConnRef` — the upstream of Chapter 111's Add-holds-a-reference).

When to sink to the low-level API? The contract's list is practical: authorization needing custom responses (forms other than 403), asynchronous authorization, per-connection state keyed by path parameters, custom handshake fields — these "framework-style" needs are exactly what the fixed endpoint does not do (avoiding a second protocol state machine); assemble them in stages with `ServerCheck/Reply/Reject/Upgrade` inside an ordinary HTTP route callback.

## Concepts

### The fixed endpoint: registration and automatic behavior

```diagram flow
- Register: WsServerRouteConfigInit (default limits + empty event table + same-origin policy)
  -> fill Protocols/Events/Open/Data/Release -> WsServerRoute(Router, path, &config)
- Request arrives: the Header stage rejects wrong methods and body declarations (illegal bodies never read)
- Validate: Origin policy -> Authorize callback (optional) -> handshake protocol/version
- Automatic responses: non-GET -> 405+Allow / authorization failed -> 403 / protocol error -> 400 / version error -> 426+13 / internal -> 500
- Upgrade succeeds: Open borrows the xwsconn - to keep it you must ConnRef; after the callback returns the adapter releases the delivered reference
```

**Registration copies**: `WsServerRoute` reads the fixed configuration once before registration, copying the configuration, events, and subprotocol list — after registration, the caller modifying or freeing the configuration and strings does not affect the endpoint (Chapter 98's freeze semantics, registration edition). **Plaintext and TLS as one**: when the Router starts via `StartTls`, the same route takes over TLS streams automatically, building the `wss` endpoint — zero registration-code changes.

### Origin policy: three default tiers

`XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT` (default): native clients may omit Origin; when a browser provides it, it must match the current request's scheme/host/effective port — a pragmatic default of **lenient to native, strict with browsers**. `SAME_HOST`: rejects even a missing Origin — strict scenarios that trust only browsers. `ANY`: wide open — **only suitable for compositions where origin validation already happened in an outer layer** (gateway verified, intranet isolation). Origin is WebSocket's CSRF defense line — Chapter 84's "secure by default" honored at the endpoint layer.

### Authorization and lifecycle

`Authorize` runs after the protocol handshake and Origin pass: it reads the full request, route parameters, and negotiation results — returning false makes the router answer a uniform 403 (custom rejection forms go through the low-level API). Three lifecycle callbacks: `Open` (borrows the connection — keeping it needs `ConnRef`), `Error` (only observes synchronous handshake and asynchronous upgrade errors, **and must not commit a response here** — it is already the "after the response" stage), `Release` (the sole release point for `Data`: executed exactly once after the Router **and all upgraded connections** stop using it — not at Router destruction! In-flight connections extend Data's life). Error domains use `XWS_SERVER_ROUTER_ERROR_AUTHORIZATION` to distinguish Origin rejection from business-authorization rejection — audits can tell "who rejected".

### The staged low level: Check/Reply/Reject/Upgrade

Beneath the fixed endpoint sit four independent entrances: `xrtWsServerCheck` (validates upgradeability — Origin/version/protocol), `Reply` (builds the 101 answer), `Reject` (builds a rejection response — the form is customizable), `Upgrade` (performs the upgrade and yields the connection). They compose freely **inside ordinary HTTP route callbacks** — authorize against a database (asynchronously) then Upgrade, give each connection state by path parameter, custom 426 answers… Chapter 104's onion model with a WebSocket-specialized layer.

### The session form: Channel + coroutine

The `session_channel_coroutine` sample shows the **sequential style** of per-connection business: connection events post to a Channel (Chapter 56); a consumer coroutine takes events from the Channel and handles them in order — callback hell becomes straight-line code. The finishing order is the template: close the Channel → stop the scheduler → Run to drain → destroy coroutine/scheduler → `ChannelDrain` to clear leftovers — Chapter 59's structured concurrency in WebSocket-session form.

## Examples

### First complete program: the fixed endpoint as one registration

The program below is from `examples/websocket/server_router` — the minimal compilable registration:

```embed path="extlibs/xws/examples/websocket/server_router/main.c" title="extlibs/xws/examples/websocket/server_router/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/server_router/main.c -lws2_32 -liphlpapi
WebSocket route /chat is ready
```

**What just happened.** (1) Chapter 104's `xhttpserverrouter` is the carrier — the WebSocket endpoint is a **path endpoint** hung on it: it coexists with HTTP routes (one Router carrying both an `/api` HTTP endpoint and a `/chat` WS endpoint is the norm). (2) `WsServerRouteConfigInit` takes the defaults (limits + same-origin policy) → fill `Protocols` ("chat.v2, chat.v1" — Chapter 93 negotiation), `Open`, (the sample omits Data/Release) → `WsServerRoute` registers — registration copies; the stack Config is discarded after use. (3) After `RouterFreeze` freezes, the endpoint is ready — **the one-line comment where the Open callback receives the connection** (Chapter 111's group Add is written exactly here). Real loopback, TLS variants, and OOM rollback each have dedicated tests — the sample focuses on the registration shape.

### Second complete program: the full lifecycle of the upgrade callback

The second program is from `examples/websocket/http_server` — the standard shape of the Upgrade-completion callback:

```embed path="extlibs/xws/examples/websocket/http_server/main.c" title="extlibs/xws/examples/websocket/http_server/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/http_server/main.c -lws2_32 -liphlpapi
（升级回调演示完成：连接发送-关闭-销毁路径执行后正常退出）
```

**What just happened.** (1) The upgrade callback's **three-step finishing template**: `ConnText` (a welcome message — Chapter 110's copy form) → `ConnClose` (normal close with NORMAL code) → `ConnDestroy` (release your own reference) — this sample closes immediately, so the three steps are written together; a long-lived connection would Ref-and-keep (join a group/session) and take the same path later. (2) The header comment spells out the architecture: "the HTTP Request event can first finish routing and authorization, then upgrade the current connection" — exactly the use case of the staged low-level API (authorize at the HTTP layer, then Upgrade). (3) `onRequest`'s shape is identical to Chapters 103/104 — a WebSocket upgrade is one kind of "response" to HTTP request handling.

## Contracts

- **Registration copies**: configuration/events/subprotocols are copied once at registration; later caller changes don't affect the endpoint; unaligned addresses are allowed but must be complete without wraparound.
- **Plaintext and TLS as one**: the same route builds `ws` under `RouterStart` and `wss` under `RouterStartTls` — no second registration.
- **Origin's three tiers**: SAME_HOST_OR_ABSENT default (native may omit / browsers must match) / SAME_HOST strict / ANY only when an outer layer already validated.
- **Automatic responses**: non-GET → 405+Allow; Origin/authorization → 403; protocol → 400; version → 426 + `Sec-WebSocket-Version: 13`; internal → 500 (abnormal close when committing fails).
- **Header-stage rejection**: wrong methods and body declarations are rejected at the Header stage without reading the illegal body; body-less requests upgrade at the full-request stage.
- **Authorize timing**: after handshake and Origin pass; it may read request/parameters/negotiation results; false → uniform 403; custom/async goes low-level.
- **Open borrows**: after the callback returns the adapter releases the delivered reference — keeping it requires `ConnRef`.
- **Release semantics**: exactly once after the Router and all upgraded connections exit — in-flight connections extend Data's life.
- **Error boundary**: observe only, never respond — it is the notifier of handshake failure, not the handler.
- **Four low-level entrances**: Check/Reply/Reject/Upgrade compose independently — the escape hatch for custom authorization/responses/state.

## Pitfalls

### Pitfall 1: saving the connection in Open without a Ref

Symptom: at some point after Open, touching the connection crashes — the adapter released the delivered reference after the callback returned.

Cause: Open is **borrow** semantics. To hold long-term (join a group, enter a session table) you must `xrtWsConnRef` inside the callback — Chapter 111's Add does exactly this internally.

```c bad
static void onOpen(xwsconn* pConn, ptr pData) {
	g_Session = pConn;   /* stored the pointer without a Ref: dangling once the callback returns */
}
```

```c good
static void onOpen(xwsconn* pConn, ptr pData) {
	if ( xrtWsConnRef(pConn) != NULL ) {
		g_Session = pConn;      /* save after incrementing */
		xrtWsGroupAdd(g_Group, pConn);  /* the group holds another share */
	}
}
```

### Pitfall 2: picking Origin ANY for convenience

Symptom: a third-party page establishes a WebSocket connection with the user's browser credentials — cross-site WebSocket hijacking (CSRF's WS edition).

Cause: browsers attach cookies/credentials automatically; `ORIGIN_ANY` never validates origin — unless an outer layer (gateway/intranet isolation) already validated, ANY is an open door. The default tier already balances native (may omit) and browsers (must match) — there is no reason to drop to ANY.

```c bad
Config.Origin = XWS_SERVER_ORIGIN_ANY;  /* "just get it running first" */
```

```c good
/* keep the default SAME_HOST_OR_ABSENT; tighten to SAME_HOST for strict scenarios */
Config.Origin = XWS_SERVER_ORIGIN_SAME_HOST;
Config.Authorize = check_session;       /* add business authorization as another gate */
```

### Pitfall 3: assuming Release runs at Router destruction

Symptom: after Router Destroy, Data is still used by in-flight connections — premature free crashes; or the reverse: Release never comes and it leaks.

Cause: Release's contract is "exactly once after the Router **and all upgraded connections** stop using it" — in-flight connections extend Data's life. Cleanup order: Router destroyed → connections close one by one → the final Release.

```c bad
xrtHttpServerRouterDestroy(pRouter);
free(my_data);   /* in-flight connections still using it: too early */
```

```c good
/* free inside the Release callback - the timing is guaranteed by the contract */
static void releaseState(ptr pData) {
	free(pData);   /* runs exactly once after the Router and all connections exit */
}
Ws.Release = releaseState;
/* destroy order: Drain server -> connections finish -> Router Destroy -> Release arrives on its own */
```

## Exercises

### Basic: the registration and automatic-response matrix

Run server_router; use curl to POST `/chat` (405) and send no Origin with a wrong version (426) — compare against the automatic-response table. Acceptance criteria: all five automatic responses reproduced; the `Allow: GET` header present.

### Advanced: an authorized endpoint

Add an `Authorize` callback: check a token (simulated) to decide pass/403. Browser-shape tests: matching Origin passes; foreign-domain Origin is rejected (403 with the AUTHORIZATION error domain). Acceptance criteria: Origin rejection and business rejection distinguishable by error domain; legitimate connections upgrade normally.

### Challenge: the channel-service full stack

Chapter 111's challenge landed: the Router mounts a `/chat/{channel}` endpoint (path parameters select the group), Open does Ref + join, messages broadcast to the channel group, kick and the shutdown flow (Seal → drain → close per channel → destroy → Release concludes). Acceptance criteria: multiple channels under concurrent load testing with zero scrambling; Release executes exactly once after shutdown (verified by counting); zero leaks throughout.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Fixed endpoint | WsServerRoute hangs on the HTTP Router; registration copies; plaintext/TLS as one |
| Origin's three tiers | SAME_HOST_OR_ABSENT default / SAME_HOST strict / ANY only when validated outside |
| Automatic responses | 405+Allow / 403 / 400 / 426+13 / 500 — five classes fully covered |
| Header-stage rejection | method/body declaration rejected without reading the body; upgrade at the full-request stage |
| Authorize | runs after handshake and Origin; false → uniform 403; custom goes low-level |
| Open borrows | keeping requires ConnRef; after return the adapter releases the delivered reference |
| Release | exactly once after the Router and all connections exit — in-flight connections extend Data |
| Four low-level entrances | Check/Reply/Reject/Upgrade — the escape hatch for custom authorization/responses/state |
| Session form | Channel + coroutine sequentializes per-connection business; finish in order: close-drain-destroy |
| Companion chapters | 109 connections / 110 sending / 111 grouping / 112 the entrance — four puzzle pieces joined |
