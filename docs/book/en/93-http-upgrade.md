---
num: 93
slug: http-upgrade
title: HTTP Upgrade: From Request to 101
volume: 卷九 Web 协议核心
type: practice
lead: Parsing and answering Upgrade offers, building the WebSocket handshake request, Accept-key computation and subprotocol negotiation — the complete entrance to protocol switching.
api: http_upgrade, websocket_upgrade, websocket, http
---

## Orientation

HTTP/1.1 has a "transformation" mechanism: the `Upgrade` header proposes a protocol switch, the server answers `101 Switching Protocols`, and **the same TCP connection starts speaking a new language**. WebSocket is this mechanism's most famous user — Chapters 94~96's frames, messages, and streams all begin with one successful upgrade. This chapter covers both sides: the **offer side** (`xrtHttpUpgradeFieldCursorInit/FieldNext` iterating client offers, `xrtHttpUpgradeWrite` writing "the subset I support" as the canonical answer — not WebSocket-only; h2c and other upgrades use it alike); the **WebSocket handshake side** (`websocket/upgrade`'s request/answer construction and verification, `xrtWsAccept`'s RFC-canonical computation, `xrtWsProtocolSelect` subprotocol negotiation). The Accept value has RFC 6455's standard test vector — matching it is the interoperability proof of a correct implementation.

## Introduction

Why does WebSocket start from HTTP? The engineering answer: ports 80/443 are already open, proxies and firewalls understand HTTP, and auth and routing can reuse HTTP infrastructure. The protocol answer: the upgrade lets the "negotiation" happen in a language both sides speak — the client offers (I want to speak websocket, with these subprotocols), the server selects (101 + the key parameters I accept), and only when negotiation completes do they switch languages; either side unsatisfied keeps speaking HTTP (a normal response, no switch).

Three details of upgrade semantics are worth establishing first: **offers can repeat** (multiple Upgrade fields, comma lists — Chapter 91's field-family rules); **protocol names are case-insensitive** (`WebSocket`/`websocket` synonymous); **a version may ride along** (the `protocol/version` shape like `HTTP/2.0`). All are handled by the upgrade layer's iterators — another "don't hand-slice" story.

## Concepts

### The offer side: iteration and answer

```diagram flow
- Client: Upgrade: h2c, websocket (repeatable fields, comma lists, case-insensitive)
- Server iteration: FieldCursorInit + FieldNext -> yields protocol/version offers one by one
- Server decision: intersect with the local support list (order per client preference)
- Answer: UpgradeWrite writes "the supported subset" as the canonical Upgrade value + the 101 response
- Switch: after the 101 goes out, the connection speaks the new protocol; no match -> a normal HTTP response (no switch)
```

`xrtHttpUpgradeFieldNext` yields `xhttpupgradeitem` — a `Protocol` view plus an optional `Version` view — the iterator merges repeated fields, splits commas, and normalizes case (at comparison). `xrtHttpUpgradeWrite(支持列表, 数量, 输出, 容量, &长度)` (support list, count, output, capacity, &length) generates the canonical value in reverse — capacity atomicity as everywhere.**Answer semantics**: the upgrade answer writes only "the selected protocols" (not the whole support list); the `101 Switching Protocols` response must also echo `Connection: Upgrade` — response construction is done by the upgrade layer's WebSocket variant (next section) or assembled by hand.

### The WebSocket handshake: request, key, and answer verification

The WebSocket upgrade stacks dedicated fields on the generic mechanism: the client sends `Upgrade: websocket` + `Connection: Upgrade` + `Sec-WebSocket-Key` (Base64 of 16 random bytes) + optional `Sec-WebSocket-Protocol` (a subprotocol list); the server must answer `Sec-WebSocket-Accept` — **computed per RFC 6455**: `Base64(SHA1(Key + 魔法串))` (Key + magic string). This computation is not "verifying a secret" (the Key holds no secret) but **proving the server truly speaks the protocol** — an implementation that computes the right Accept has necessarily implemented the spec. `xrtWsAccept(Key, 输出, 容量)` (Key, output, capacity) does it in one step, outputting the 28-character Base64.

Request construction (demonstrated by `examples/websocket/upgrade`): the complete request head of `GET 路径 HTTP/1.1` (GET path HTTP/1.1) + Host + Upgrade/Connection + Key/Version(+Protocol); answer verification (`upgrade_tour`'s `response check binds key+protocol`): **verification must bind this session's Key** — the peer's echoed Accept is compared against the value computed from your own sent Key (against man-in-the-middle session substitution), and the selected subprotocol must be within your offered list.

### Subprotocol negotiation

`Sec-WebSocket-Protocol` is the application-layer protocol choice (something like `chat, superchat, binary`): `xrtWsProtocolSelect(客户端提议, 服务端支持, &选中)` (client offers, server supports, &selected) takes **the first, in the client's preference order, that the server also supports** — order-sensitive (the client's priority expression). No intersection returns failure (the handshake continues without a subprotocol, or the application chooses to refuse — the policy is yours).

### The upgrade's position in the pipeline

```diagram flow
- HTTP request arrives: RequestParse (Chapter 89)
- Upgrade check: FieldTokenFind sees Connection contains upgrade (Chapter 91) + the Upgrade field exists
- Offer match: UpgradeFieldNext iterates ∩ local support -> websocket hits
- Handshake handling: Key verification/Accept computation/subprotocol selection (this section)
- 101 answer: UpgradeWrite + Accept + the selected protocol
- Takeover: WsStreamAttach takes over the transport after the 101 (Chapter 95)
```

Five layers in series — every layer an API from earlier chapters. That is Volume 9's building-block structure: the upgrade is "HTTP's last act, WebSocket's first".

## Examples

### First complete program: offer iteration and answer generation

The program below is from `examples/http/upgrade/main.c` — the complete handling of repeated fields, comma lists, and version shapes:

```embed path="examples/http/upgrade/main.c" title="examples/http/upgrade/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http/upgrade/main.c -lws2_32 -liphlpapi
protocol: h2c
protocol: websocket
protocol: HTTP/2.0
field: websocket, HTTP/2.0
```

**What just happened.** (1) The input is two Upgrade fields (differently-cased names, one with a comma list) — the iterator merges them into three offers: `h2c`, `websocket`, `HTTP/2.0` (the protocol/version shape split for display).**No hand-slicing** — repeated fields, commas, case all handled by `FieldCursorInit/FieldNext`. (2) `UpgradeWrite` writes "the supported subset" (websocket, HTTP/2.0) as the canonical value `websocket, HTTP/2.0` — the answer writes only the selected set, order from the provided list. (3) This sample is pure field-layer (no network) — the unit-test shape of upgrade semantics; wedged between RequestParse and 101-response construction, it is the complete upgrade-server path.

### Second complete program: the Accept vector and subprotocol negotiation

The second program is from `examples/websocket/handshake/main.c` — RFC standard vectors and preference negotiation:

```embed path="examples/websocket/handshake/main.c" title="examples/websocket/handshake/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/handshake/main.c -lws2_32 -liphlpapi
accept=s3pPLMBiTxaQ9kYGzzhZRbK+xOo= protocol=superchat
```

**What just happened.** (1) The input nonce `dGhlIHNhbXBsZSBub25jZQ==` is RFC 6455 §1.3's canonical sample — `xrtWsAccept` computes `s3pPLMBiTxaQ9kYGzzhZRbK+xOo=`, character-for-character identical to the standard vector — **matching it is the interoperability proof of a correct implementation** (Chapter 75's RFC vectors, applied again). (2) `ProtocolSelect("chat, superchat", "superchat, binary")` selects `superchat` — in the client's preference order (chat first) the server doesn't know chat; superchat is the first both support. (3) Zero network objects: the handshake computation layer is fully standalone — tests, proxies, and debuggers can all use it directly. The companions `examples/websocket/upgrade` (constructing the complete handshake request) and `upgrade_tour` (four self-checking lines: configuration/request verification/answer binding/stream-config handoff) cover both sides of the handshake's full API surface.

## Contracts

- **Offer semantics**: Upgrade fields repeat, values may be comma lists, names case-insensitive; the iterator yields structured `protocol/version` offers.
- **Answer semantics**: `UpgradeWrite` writes only the selected subset; the 101 response must echo `Connection: Upgrade`; capacity atomic.
- **Accept computation**: `Base64(SHA1(Key + RFC 魔法串))` (Key + RFC magic string); the Key is Base64 of 16 random bytes (24 characters); the computation itself holds no secrecy — it is a proof of spec conformance.
- **Answer-verification binding**: the client must verify the Accept corresponds to **the Key it sent** and the selected protocol is within its offers — against substitution.
- **Subprotocol negotiation**: client preference order ∩ server support, taking the first in client order; handling no-intersection is application policy.
- **The upgrade-failure path**: no match means a normal HTTP response (no switch) — an upgrade is an offer, not a demand.
- **Trimming**: `HTTP_UPGRADE` independent; the WebSocket handshake depends on `CRYPTO_SHA1` (Accept computation) and Base64.
- **Composition boundary**: upstream RequestParse/FieldTokenFind (Chapters 89/91); downstream WsStreamAttach (Chapter 95).

## Pitfalls

### Pitfall 1: answer verification not bound to your own Key

Symptom: the client passes the Accept because it "looks like the format" (28-character Base64) — an intermediate device re-handshakes, and the connection now belongs to a different session.

Cause: Accept verification is an **equality assertion**: `服务端回的 Accept == 我用我的 Key 算的值` (the server's returned Accept == the value computed from my Key). A format check is not verification.

```c bad
if ( looks_like_base64(AcceptField, 28) ) {
	proceed();   /* passes on "format looks right": a man-in-the-middle's Key+Accept pair passes too */
}
```

```c good
char Mine[XWS_ACCEPT_CAPACITY];
if ( !xrtWsAccept(MyKey, Mine, sizeof(Mine)) ||
		!equal(AcceptField, Mine) ) {
	abort_handshake();   /* equality assertion failed: not my session */
}
```

### Pitfall 2: subprotocol negotiation using the server's preference order

Symptom: the server selects `binary` by its own support-list order while the client clearly wanted `chat` — the negotiated result violates client priority, behavior diverges.

Cause: RFC 6455 requires selection to respect **the client's offered order** — `ProtocolSelect` has it built in; writing your own "walk the server list for the first common item" reverses it.

```c bad
for ( each server_proto ) {        /* server order: violates the protocol */
	for ( each client_proto ) {
		if ( equal ) return server_proto;
	}
}
```

```c good
xstrview Selected;
if ( xrtWsProtocolSelect(ClientOffered, ServerSupported,
		&Selected) ) {
	/* client preference order ∩ server support: protocol semantics built in */
}
```

### Pitfall 3: judging upgrade by the Upgrade field alone, ignoring Connection

Symptom: a request with only `Upgrade: websocket` but `Connection: keep-alive` gets its connection switched — the client never asked for an upgrade, and the connection state is scrambled.

Cause: the upgrade is a **joint semantics of two fields** — `Upgrade` says what to switch to, `Connection: Upgrade` says "I request the switch". Looking at only one side decides on the client's behalf.

```c bad
if ( has_field(Fields, "Upgrade") ) {
	switch_protocol();   /* Connection didn't say upgrade: a mistaken switch */
}
```

```c good
if ( xrtHttpFieldTokenFind(Fields, iCount,
		XRT_STR_LITERAL("upgrade")) &&       /* Connection contains upgrade */
		has_upgrade_websocket(Fields) ) {    /* Upgrade offers websocket */
	switch_protocol();
}
```

## Exercises

### Basic: run the handshake-request construction

Run both samples, `websocket/upgrade` and `handshake`; use the OpenSSL command line to compute the Accept for the same Key and compare with the sample. Acceptance criteria: the Accept matches the RFC vector; the constructed request contains every mandatory field.

### Advanced: an upgrade-detection middleware

Over a field array, implement `is_upgrade_request(Fields, 数量, &提议协议)` (Fields, count, &offered protocol): Connection contains upgrade (token lookup) + Upgrade-offer iteration, returning whether to upgrade and the first offer. Verify against four inputs: a standard upgrade, missing Connection, an empty Upgrade list, a non-websocket offer. Acceptance criteria: the four verdicts match hand-derivation; throughout, Chapter 91's family APIs with zero hand-slicing.

### Challenge: a handshake answerer

Implement `accept_handshake(请求字段, 支持的子协议, 输出, 容量, &长度)` (request fields, supported subprotocols, output, capacity, &length): extract the Key, compute the Accept, negotiate the subprotocol, generate the complete 101 response (status line + Upgrade/Connection/Accept/Protocol fields). Verify end-to-end with the request built by `websocket/upgrade`. Acceptance criteria: the response passes `upgrade_tour`'s answer verification; with no matching subprotocol, degrade by policy (no Protocol field); insufficient capacity writes zero.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Generic upgrade | offers repeat/comma/case-insensitive; iterate FieldCursorInit+FieldNext; the answer UpgradeWrite writes only the selected set |
| 101 semantics | the answer echoes Connection: Upgrade; no match means a normal HTTP response (an offer, not a demand) |
| WS-specific fields | Upgrade: websocket + Connection + Sec-WebSocket-Key + Protocol (optional) |
| Accept computation | `Base64(SHA1(Key+魔法))` (Key+magic); the RFC vector s3pPLMBiTxaQ9kYGzzhZRbK+xOo= |
| Answer verification | equality assertion bound to your own Key + the selected protocol within your offers - a format check is not verification |
| Subprotocol negotiation | client preference order ∩ server support, first in client order; no-intersection policy belongs to the application |
| Joint detection | Connection contains upgrade AND Upgrade has an offer - both fields indispensable |
| Pipeline position | HTTP's last act, WebSocket's first; five layers chaining Chapters 89->95 |
| Trimming | HTTP_UPGRADE independent; the WS handshake depends on SHA1+Base64 |
