---
num: 85
slug: tls-server
title: The TLS Server: The Session-Layer Protocol Machine
volume: 卷八 安全
type: practice
lead: A transport-agnostic bare protocol machine: Feed ciphertext in, Drive to advance, take flights from the Send queue — SNI dynamic identity selection and ticket issuance, the complete server side.
api: tls, tls_verify, crypto
---

## Orientation

TLS's object stack has two layers: the **session layer** (`xtlssession` — a bare protocol machine taking ciphertext blocks in and producing ciphertext blocks out, transport entirely the caller's choice) and the **Stream layer** (Chapter 86 — hanging the session machine on a TCP stream). This chapter covers the session layer's server side: `xtlsserverconfig` configures identity and ALPN, `xrtTlsServerCreate` builds the session, `Feed*` feeds received ciphertext, `xrtTlsServerDrive` advances the state machine, and the public Send queue yields outbound flights — what the transport is (memory bridge, TCP, serial port, custom IoT link) the protocol machine cares nothing about. Three server-specific things also live in this chapter: **SNI dynamic selection** (the `Select` callback swapping identity and protocol by domain), **ticket issuance** (`xrtTlsServerTicket`), and **resumption acceptance** (the `Resume` callback, PSK+DHE). This chapter's examples use "memory hauling" as the transport, the server sparring with a real client in-process — also the standard posture for protocol testing.

## Introduction

When do you want the bare protocol machine instead of the Stream layer? Three real scenarios. One: protocol testing — you want to drive client and server simultaneously in memory, checking flights byte by byte, touching no network. Two: custom transport — running TLS over an IoT serial link, Bluetooth BLE, even a message queue; where the transport bytes come from is your choice. Three: extreme control — a gateway forwarding ciphertext under custom scheduling, where the Stream layer's event model doesn't fit.

The session-layer server has one more "waiting" dimension than the client: the client sends its first flight on creation, while the server after creation **waits for the ClientHello** — which SNI, which ALPN it receives may decide which identity answers (the standard scenario of one server serving several domains). This "order after reading the menu" capability is carried by the `Select` callback; if no routing is needed, give a static `Identity` — one of the two must exist, guaranteeing every client (including resumption fallbacks) a clear authentication path.

## Concepts

### The three-step session-driving loop

```diagram flow
- Feed: hand the transport's received ciphertext to the session (Feed borrow / FeedTake owning / FeedRef reference / FeedBuffer zero-copy chain)
- Drive: xrtTlsServerDrive advances the state machine within record and handshake budgets
- Send: take outbound ciphertext from the public Send queue (SendSize/SendFront/SendConsume / SendSpans)
```

The three-step loop is **fully isomorphic** between client and server (the client uses `xrtTlsClientDrive`) — master one side and the other comes free. The four feed forms correspond to four ownerships (borrow/take/reference/buffer chain — Chapter 64's four append kinds replayed here); the Send queue's Size/Front/Consume consumption trio is the same vocabulary as Chapter 64's buffer chain. The TLS 1.3 full-certificate flight generates ServerHello, EncryptedExtensions, Certificate, CertificateVerify, Finished in sequence inside Drive (Chapter 83's timeline), and after verifying the client's Finished it **atomically switches** to the application epoch and READY.

### Server configuration and the first-flight Arena

`xtlsserverconfig` borrows the context and static identity during creation; on success the session holds references and **deep-copies the ALPN list**. With `RequireProtocol = true`, no common ALPN is an explicit failure — protocol ambiguity dies at handshake. The first flight's extension tables, certificate entries, signature input, and temporary encoding streams come from the session's **lazy temporary Arena**: idle connections and un-handshaked sessions allocate no Arena blocks, and the first flight's completion immediately securely zeroes and releases — no fixed handshake buffer per connection; 1.2/1.3 share the same Arena and cleanup path. This memory shape is a key contract for high-connection servers.

### SNI dynamic selection: the Select callback

`Select` executes after the ClientHello is strictly parsed and SNI/ALPN extracted, but **before any server output is generated**. The callback receives the requested SNI and full ALPN payload (borrowed only during the callback); the `xtlsserverchoice` initially holds the static identity, the protocol index pre-computed by server preference, and a zero `Cookie`; the callback may swap identity or protocol and write a **64-bit host routing identifier** XRT never interprets. `xrtTlsServerCookie` returns that identifier after a successful selection — the transport adapter uses it to associate the handshaked connection back to "the configuration generation at selection time" (say, the tenant-config version consulted then). Constraints: the context is borrowed only until the first flight completes; you must not recursively drive the same session. `xrtTlsServerName` returns a deep-copied SNI (the view stable to session destruction), and `xrtTlsSessionProtocol` returns the final ALPN.

### Ticket issuance and resumption acceptance

With `XRT_FEATURE_TLS_SERVER_RESUME` enabled, resumption plugs into both server sides:

- **Issuance**: after READY, `xrtTlsServerTicket(会话, 不透明ticket, 寿命, &恢复对象)` encodes the caller-provided ticket into a NewSessionTicket and sends it, transferring **ownership of the corresponding server resumption object to the caller**; the `TicketNew` convenience entry uses a 32-byte securely random ticket and an 86400-second default lifetime. XRT **maintains no process-global ticket cache** — choose a Map per tenant/capacity/expiry/persistence (Chapter 18), a sharded cache, or external storage. The hard send cap is pre-checked before randomness and derivation; on `XTLS_AGAIN` the output object is `NULL` and the write sequence and resumption state are unchanged — retry as-is after draining.
- **Acceptance**: the `Resume` callback receives the SNI, ALPN, opaque ticket, and obfuscated age, returning a borrowed immutable `xtlsresume` (Chapter 87's object contract); the session adds a reference and validates version/suite/SNI/ALPN/validity/age tolerance. **Ticket not found or route mismatch safely falls back to the full handshake**; but "ticket metadata matching with a binder error" is an **authentication failure** — it must be a fatal Alert, never a degraded bypass (an attacker who cannot produce the PSK should be refused). Resumption always keeps ECDHE (PSK+DHE, forward secrecy undegraded); pure PSK and 0-RTT are unsupported.

### The TLS 1.2 path and KeyUpdate

The server's 1.2 path requires EMS, performs the ECDHE certificate full handshake, and atomically switches epochs at the ChangeCipherSpec boundary; no resumption, no renegotiation, no KeyUpdate (1.2's historical decisions). 1.3's active/passive KeyUpdate both follow "old epoch fully queued, new epoch committed once" — send backpressure and allocation failure produce no half-updated state.

## Examples

### First complete program: transport-agnostic server sparring

The following program comes from `examples/tls/server/main.c` — the server and a real client completing handshake and bidirectional data over a memory bridge:

```embed path="examples/tls/server/main.c" title="examples/tls/server/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/server/main.c -lws2_32 -liphlpapi
usage: server <rsa|p256|p384|ed25519> ...
```

**What just happened.** (1) **Identity and verifier assembly**: after reading DER files, `exampleTlsIdentity` constructs the identity (Chapter 82's startup-time validation); the verifier uses the minimal `Verify` callback (the sparring scenario trusting itself). (2) **Dual session creation**: `ClientCreate` and `ServerCreate` each take a configuration — the server configures `Identity + Protocols + RequireProtocol`; neither session **touches any socket**. (3) **Memory-bridge handshake**: `exampleTlsHandshake` is the template of the session-layer driving loop — `ClientDrive + ServerDrive` advancing alternately, `exampleTlsMove` Feeding one side's Send-queue ciphertext Span by Span to the peer — "transport" is just the hauling between these two loops. To go on a real network, swap Move for socket reads/writes and nothing else changes. (4) **Bidirectional data**: `exampleTlsTransfer` demonstrates application plaintext's `SessionWrite/Read` (through the same Drive-and-Move loop); finally `SessionProtocol` queries the negotiated ALPN — `RequireProtocol` guarantees it non-empty. (5) The cleanup destroys both sessions → releases verifier and identity → frees file buffers — the session layer's objects all have the simple "Destroy/Release once" lifecycle.

### Second complete program: the session-layer full-feature tour

The second program comes from `examples/tls/session_tour/main.c` — a six-line self-check of the public session API:

```embed path="examples/tls/session_tour/main.c" title="examples/tls/session_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -I examples/tls -include xrt.h impl.c examples/tls/session_tour/main.c -lws2_32 -liphlpapi
session: handshake via feed-borrow -> both ready ok
session: role/version/cipher/wait/context ok
session: feed-take + feed-ref (release once) ok
session: feed-buffer zero-copy chain ok
session: send spans gathered + plain spans consumed ok
session: close_notify -> peer eof ok
```

**What just happened.** (1) Line 1 is the server example's memory-bridge handshake (the borrow-form Feed); line 3 verifies the other three feed forms one by one — Take (taking ownership), Ref (feed by reference, released exactly once), Buffer (Chapter 64's buffer chain hung on directly, zero-copy) — all four ownerships covered. (2) Line 2 queries the session's public facts: role (client/server the same object type), negotiated version and suite, wait reason (input or output — the two-state decomposition of `XTLS_AGAIN`), shared context. (3) Line 5 verifies bidirectional Spans: the send side's Span family gathers sends, the receive side consumes plaintext by Span — the application layer's zero-copy vocabulary extended to the session boundary. (4) Line 6 walks the authenticated close: close_notify sent, the peer receives EOF — Chapter 81's close protocol as it looks at the session layer. Six lines together are "this chapter's entire session-layer API running" — treat it as the chapter's **executable cheat sheet**.

## Contracts

- **Identity mandate**: a static `Identity` or a synchronous `Select` — at least one; unknown SNI and resumption fallbacks still have a clear authentication path.
- **Driving isomorphism**: `Feed`'s four forms (borrow/Take/Ref/Buffer zero-copy) + Drive + the Send queue — client and server share the same public session API.
- **Atomic switching**: 1.3 atomically switches to the application epoch and READY after the client's Finished verifies; 1.2 atomically switches at the CCS boundary; failure publishes no half state.
- **Lazy Arena**: first-flight temporary memory allocated on demand, securely zeroed and released after use; idle connections carry zero handshake buffers.
- **Select timing**: after the ClientHello's strict parsing, before any output; SNI/ALPN borrowed only during the callback; Cookie is a 64-bit host routing identifier (XRT never interprets it); the context borrowed until the first flight completes; recursive driving forbidden.
- **Ticket issuance**: object ownership transfers to the caller; no global cache; on AGAIN the state is unchanged and retryable; hard caps pre-checked.
- **Resumption acceptance**: not-found/route-mismatch falls back to the full handshake; a binder error is an authentication failure (fatal, no degradation); always PSK+DHE; no pure PSK/0-RTT.
- **1.2 boundary**: EMS mandatory; no resumption/renegotiation/KeyUpdate.
- **KeyUpdate**: old epoch fully queued, new epoch committed once; backpressure and OOM leave no half-update.
- **Unimplemented**: client-certificate authentication, 0-RTT, asynchronous identity selection; TCP composition entries belong to Chapter 86's Stream layer.

## Pitfalls

### Pitfall 1: driving the session inside the Select callback (recursive Drive)

Symptoms: casually calling `xrtTlsServerDrive` inside Select to "advance early" — scrambled state or assertion failures.

Cause: Select runs inside Drive (before first-flight generation); recursively Driving the same session then re-enters the state machine. The callback's duty is only "pick identity, pick protocol, write the Cookie"; advancing belongs to the outer loop.

```c bad
static void onSelect(xtlsserverchoice* pChoice, ...) {
	pick_identity(pChoice);
	xrtTlsServerDrive(pSession);   /* re-entering the state machine: undefined behavior */
}
```

```c good
static void onSelect(xtlsserverchoice* pChoice, ...) {
	pick_identity(pChoice);        /* selection only */
	pChoice->Cookie = tenant_generation();  /* the routing identifier */
	/* advancing is done by the outer feed/drive/send loop */
}
```

### Pitfall 2: treating both "ticket not found" and "binder error" as fallback

Symptoms: for "high availability", the Resume callback returns fallback when the ticket isn't found and also when binder validation fails — an attacker forging ticket metadata bypasses PSK authentication probing.

Cause: the two failure classes have entirely different security semantics. Not found/route mismatch: the client may hold an old ticket — a normal business path where fallback is reasonable. Binder error: the client claims to hold a ticket's PSK but cannot prove it — either the ticket was stolen or it is an attack; a fatal Alert is mandatory.

```c bad
static const xtlsresume* onResume(...) {
	const xtlsresume* pFound = cache_lookup(Ticket);
	if ( (pFound == NULL) || !quick_check(Ticket) ) {
		return NULL;   /* fallback for everything: binder errors swallowed too */
	}
	return pFound;
}
```

```c good
/* the callback's only duty is "return it if found":
   not found -> return NULL, the session automatically falls back to the full handshake;
   metadata matching but binder error -> the state machine sends the fatal Alert (protocol-built-in,
   neither needed nor allowed as callback intervention) — XRT has this boundary right;
   your job is simply never to forge a "found" inside the callback */
```

### Pitfall 3: fixed handshake buffers per connection at high connection counts

Symptoms: a server with tens of thousands of idle connections balloons in memory — a few hundred KB of "pre-allocated handshake area" per connection, times the connection count.

Cause: misreading the session layer's memory model. First-flight temporary memory is a **lazy Arena**: not allocated before the handshake, securely zeroed and released right after; only the session state itself stays resident long-term.

```c bad
/* hand each connection its own malloc'ed "handshake workspace" */
struct conn { uint8 HandshakeArena[262144]; ... };  /* all wasted */
```

```c good
/* create the session directly; handshake temporary memory is managed by the lazy Arena;
   idle connections carry near-zero handshake memory — capacity planning watches active handshakes only */
pSession = xrtTlsServerCreate(&Config, pPool);
```

## Exercises

### Basic: the full sparring flow

Generate an Ed25519 self-signed certificate with OpenSSL, export DER, and run `server ed25519 cert.der key.der` — confirm the output `ready, protocol=h2, client=ping, server=pong` (the ALPN preference hit). Switch `RequireProtocol` to false and trim Protocols to `h2` only, watching the protocol index's selection change. Acceptance: both configurations' negotiated protocols match hand derivation.

### Advanced: dual-domain SNI routing

Implement the `Select` callback: prepare two identities (self-signed certificates for two domains), selecting by the ClientHello's SNI; write the "tenant generation" into the Cookie and read it back after READY via `xrtTlsServerCookie` to compare. Acceptance: both SNIs each receive the right certificate; Cookie write and read agree; an unknown SNI takes your defined default policy (static identity or explicit failure).

### Challenge: fragmented stress over the memory bridge

Change `exampleTlsMove`'s "haul whole Spans" into "at most 7 bytes per haul" random fragmentation — simulating a hostile transport. Tally the upper bound of Drive rounds to handshake completion. Acceptance: at any fragment size (1 to a whole flight) the handshake succeeds (the protocol machine supports cross-record reassembly — Chapter 83); the worst round count is bounded and explainable (flight size ÷ fragment-size order); no memory growth (Chapter 6's stats).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Session-layer position | the bare protocol machine: Feed ciphertext / Drive to advance / take flights from Send, transport-agnostic |
| Driving loop | Feed×4 (borrow/Take/Ref/Buffer) → Drive → Send (Size/Front/Consume/Spans) |
| Server configuration | Identity or Select, one required; ALPN deep-copied; RequireProtocol makes ambiguity a failure |
| First-flight Arena | lazy allocation, zeroed and released after use; idle connections zero handshake buffers; 1.2/1.3 share the path |
| Select | after ClientHello parsing, before output; SNI/ALPN borrowed during the callback; Cookie = 64-bit routing identifier |
| Queries | ServerName (deep-copied SNI) / SessionProtocol (final ALPN) / ServerCookie |
| Ticket issuance | Ticket/TicketNew; object ownership transferred; no global cache; AGAIN leaves state unchanged |
| Resumption acceptance | not-found falls back to the full handshake; binder error fatal, no degradation; PSK+DHE; no 0-RTT |
| 1.2 boundary | EMS mandatory; no resumption/renegotiation/KeyUpdate |
| Isomorphism | client and server share the public session API — learn one side, get both |
