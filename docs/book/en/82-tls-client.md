---
num: 82
slug: tls-client
title: The TLS Client: Dialing, Handshake, and the Application Stream
volume: 卷八 安全
type: practice
lead: DNS→TCP→TLS in one hosted dial, callback and Future consumption forms, send/receive and close after READY — the client's complete lifecycle.
api: tls, tls_verify, tls_client, net
---

## Orientation

The six chapters from here are TLS — the final confluence of Volume 8's primitives. This chapter covers the **client**: the side that initiates. `xrtTlsDial` hosts the whole chain "DNS resolution → racing TCP addresses → TLS handshake" in one call, handing you the encrypted stream (`xtlsstream`) on success; callback and Future consumption forms match event-driven and imperative code styles. The chapter walks the client state machine (`HANDSHAKE → READY → CLOSED`), the configuration trio (verifier/ServerName/ALPN), post-READY application data exchange and authenticated close — Chapter 84 then drills into the handshake's internal timing and key schedule, and Chapter 85 unfolds verifiers and policy. All prerequisites come from this volume: certificate verification (Chapters 80/81), AEAD and key derivation (Chapters 74/75), and the carrying TCP stream (Chapter 67).

## Introduction

How many steps does writing an HTTPS client string together? Resolve the hostname (Chapter 66's DNS), race-connect multiple addresses happy-eyeballs style (Chapter 63), complete the TLS handshake after TCP establishment (certificate verification + key exchange + mutual Finished), and only then send the request and receive the response — any step failing must clean up every resource already built. Hand-writing this orchestration is a classic handle-leak disaster zone; `xrtTlsDial` condenses it into one call: failure automatically rolls back intermediate resources, success transfers the stream reference to the completion callback, and the main thread handles only the final state.

The orchestration goes to the library; **the decisions stay with you**: which CA to trust (verifier, Chapter 85), which name to connect to (SNI auto-taken from the hostname), which protocol to speak (ALPN array), total timeout. This layering is the library-wide "hosted skeleton, injected policy" — Chapter 62's skeleton-industrialization idea reappearing at the TLS layer.

## Concepts

### The client state machine and the dial chain

```diagram flow
- Submit: xrtTlsDial(Engine, Resolver, hostname, port, TLS config, dial config, stream callbacks...)
- Resolve: Resolver asynchronously resolves the hostname (Chapter 66)
- Race-connect: multiple addresses tried in order for TCP (failure auto-switches to the next)
- Handshake: ClientHello -> ... -> mutual Finished (diagrammed in Chapter 84)
- READY: the Stream reference transfers to the completion callback; application send/receive begins
- Final state: authenticated close (close_notify) or failure -> release
```

The dial object `xtlsdial` itself is destroyable — `xrtTlsDialDestroy` does not affect the in-flight stream; the total timeout (`DialConfig.Timeout`) covers every stage. **The failure-path contract**: on dial or handshake failure no Stream callback is published — you never receive a half-open stream; cleanup is already done by the hosting layer.

### The configuration trio

- **Verifier** (`TlsConfig.Verifier`): a full-certificate handshake **must bind one explicitly**, otherwise creation fails — "no verification configured" is not an option but an error; this is the biggest attitudinal difference from many libraries. The verifier deep-copies the trust-store snapshot and is shareable across clients (Chapter 85 unfolds).
- **ServerName**: SNI and the certificate verification name auto-taken from the hostname parameter — no manual filling in dial scenarios; with the low-level `xrtTlsClientCreate` entry directly, specify in `xtlsclientconfig.ServerName`.
- **ALPN** (`Protocols` array): declare application protocols by preference (`h2`, `http/1.1`); the negotiated result is queryable after the handshake. A resumed connection's ALPN must match the ticket's binding.

### After READY: application data and closing

The post-READY `xtlsstream` is an encrypted bidirectional stream whose API surface is isomorphic to Chapter 67's TCP stream, plus TLS-specific pieces: `xrtTlsStreamSend` (plaintext in, ciphertext out; `iWritten` reports the accepted amount), `xrtTlsStreamAvailable`/`Buffer`/`Consume` (zero-copy consumption of received plaintext — Span by span, the same vocabulary as Chapter 65's buffer chain), `xrtTlsStreamClose` (**authenticated close**: queue close_notify, await the peer's answer, drain, then enter CLOSED), `xrtTlsStreamAbort` (immediate abort, error paths only). On receiving the peer's close_notify the client automatically queues one answer, ignores subsequent data, and enters CLOSED after draining — the close protocol is executed by the state machine; you call Close once.

Both send-budget tiers (the TLS record layer's `SendLimit` and the TCP write budget, Chapter 68) can make Send return "partial acceptance" — continuing from the `iWritten` offset is the standard form (this chapter's `exampleTlsDialSend` is the template). KeyUpdate's automatic rotation (rekeying at record-usage thresholds) is transparent to the application; manual rotation uses `xrtTlsClientKeyUpdate`.

### Two consumption forms: callback and Future

- **Callback form** (`xrtTlsDial` + event table: the four callbacks `Open`/`Read`/`Writable`/`Close`): `Open` (fires at READY — start sending the request), `Read` (plaintext arrived), `Writable` (send budget recovered — continue the unfinished prefix), `Close` (the sole final state, `xnetresult` + structured error). The main form for event-driven services and tools.
- **Future form** (`xrtTlsDialAsync`): returns an `xfuture` waitable via `xrtFutureWaitFor` on any thread; after `XFUTURE_RESOLVED`, `xrtFutureValue` takes the stream and `xrtTlsStreamRef` takes over the reference. The main form for imperative code and coroutine integration. The two forms share the same dial configuration and stream objects — consistent with the dual-form tradition of Chapter 66's DNS resolution and Chapter 68's dialing.

### Session resumption: tickets and PSK

With `XRT_FEATURE_TLS_CLIENT_RESUME` enabled, the client derives the resumption master secret from the final transcript, derives an independent PSK per `NewSessionTicket`, and deep-copies the ticket (including the actual SNI/ALPN/verified leaf identity). `ResumeLimit` (default 4, max 64, 0 disables) bounds the retained count, the full queue evicting oldest; `xrtTlsClientTakeResume` transfers the session's held reference to the caller — **the cross-connection cache is yours to manage**. The next ClientHello carries the ticket + a normal key share: if the server accepts, the certificate stage is skipped (PSK+DHE, still forward-secret); if not, a safe fallback to the full handshake — `xrtTlsClientResumed` reports the actual choice. TLS 1.2 offers no resumption.

## Examples

### First complete program: callback-form HTTPS dial, full chain

The following program comes from `examples/tls/dial/main.c` — system trust store + hostname verification + hosted dial + authenticated close, the complete template at the bare kernel layer (real business should go straight to Chapter 98's xhttp):

```embed path="examples/tls/dial/main.c" title="examples/tls/dial/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/dial/main.c -lws2_32 -liphlpapi
usage: dial <host> [port]
```

**What just happened.** (1) **Verifier assembly**: `StoreSystem` snapshot → `TlsVerifierCreate` (deep copy; the Store is freed right after) — three lines are the entirety of "HTTPS default trust", unpacked in Chapter 85. (2) **Dial submission**: `TlsDial(Engine, Resolver, host, 443, TlsConfig, DialConfig, Events...)` — one call chaining DNS/race-connect/handshake; `DialConfig.Timeout = 15s` covers all stages, the hostname entering SNI and the verification name automatically. (3) **READY callback orchestration**: in `Open`, send the minimal HTTP request — `exampleTlsDialSend` is the standard template for two-tier backpressure (partial acceptance recorded in the `Sent` offset; `XTLS_AGAIN` returns awaiting `Writable`); with the request queued, `StreamClose` authenticated-closes the write side and keeps receiving the response. (4) **Zero-copy receive**: in the `Read` callback, `Available/Buffer/Front/Consume` print Span by Span — never copying the whole response; Chapter 65's span-chain consumption reused as-is. (5) **Final state and cleanup**: the `Close` callback records the sole final state (an atomic flag lets the main thread exit safely); the Cleanup section destroys Dial/Stream/Resolver/Engine/Verifier in reverse dependency order — any step failing takes the same path, zero handle leaks.

### Second complete program: Future-form dial and resource reclamation

The second program comes from `examples/tls/dial_future/main.c`, the same chain with Future consumption:

```embed path="examples/tls/dial_future/main.c" title="examples/tls/dial_future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/dial_future/main.c -lws2_32 -liphlpapi
usage: dial_future <host> [port]
```

**What just happened.** (1) `xrtTlsDialAsync` returns a Future immediately — the initiating site doesn't wait; any thread can `xrtFutureWaitFor` (16 seconds slightly wider than the dial's 15-second timeout — the waiting window must cover the dial window). (2) The success path takes over in three steps: `WaitFor → XFUTURE_RESOLVED 检查 → xrtFutureValue 取流 + xrtTlsStreamRef 接引用`, then `FutureDestroy` immediately — the Future's and stream's references complete their handover at this point. (3) **Waiting for resource reclamation after Abort** is this chapter's unique knowledge point: `Abort` requests immediate abort, but the background network resources are still releasing — spin waiting for `XTLS_STREAM_CLOSED/FAILED` before `Destroy`; this is "the junction of the stream object's and the Future's lifecycles" (Chapter 67's "wait CLOSED before destroy" rule holds verbatim on TLS streams). (4) Read side by side with dial: the verifier/Engine/Resolver assembly is identical — the two forms differ only in "how you get the stream".

## Contracts

- **Verification mandatory**: a full-certificate handshake must bind a Verifier or creation fails; a pure-PSK resumption build (no verification backends) may omit it.
- **Dial atomicity**: any stage failing automatically cleans intermediate resources; failure publishes no Stream; the total timeout covers all stages; the Dial may be destroyed first without affecting the in-flight stream.
- **State machine**: `HANDSHAKE → READY → CLOSED` (or FAILED), forward-only; `XTLS_AGAIN` is merely "more input or output space needed", never an error.
- **Send semantics**: `Send` reports partial acceptance via `iWritten`; the TLS record layer's `SendLimit` and the TCP write budget (Chapter 68) form two-tier backpressure; KeyUpdate automatic rotation is transparent.
- **Close semantics**: `Close` queues close_notify and waits for the peer's answer to drain; on receiving the peer's close_notify, answer automatically and ignore subsequent data; error paths always `Abort`.
- **Resumption boundaries**: ticket cache `ResumeLimit` 0..64 (default 4); `TakeResume` transfers the reference, the cross-connection cache belongs to the caller; the resumed ALPN must match the ticket's; 1.2 has no resumption; 0-RTT unimplemented.
- **Unimplemented surface**: CertificateRequest, client-certificate authentication, 0-RTT, TLS 1.2 renegotiation — scenarios needing client certificates are currently unavailable.
- **Threading**: callbacks execute on Engine Workers (Chapter 64); the Future form is waitable on any thread; the Verifier is shareable across sessions (Chapter 85).

## Pitfalls

### Pitfall 1: no verifier configured (or a verifier with an empty trust store)

Symptoms: wanting to "connect first, worry later", skipping verification — `xrtTlsClientCreate`/dial fails outright; or a hand-built verifier with an empty store likewise fails at creation.

Cause: XRT's position is **no-verification is not an option**. Libraries offering "optional verification" contribute a steady crop of man-in-the-middle incidents yearly; here the default path is welded shut — either give a verifier, or say explicitly what you are doing (a PSK resumption build).

```c bad
xrtTlsClientConfigInit(&Config);
/* no Verifier: under the full-handshake build, creation fails outright */
Session = xrtTlsClientCreate(&Config, pPool);
```

```c good
xx509store* pStore = xrtX509StoreSystem();       /* system trust */
xrtTlsVerifierConfigInit(&VerifierConfig);
VerifierConfig.Store = pStore;
pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
xrtX509StoreFree(pStore);                        /* deep-copied — free yours */
xrtTlsClientConfigInit(&Config);
Config.Verifier = pVerifier;
Session = xrtTlsClientCreate(&Config, pPool);
xrtTlsVerifierRelease(pVerifier);                /* the session holds a reference; release your own */
```

### Pitfall 2: Send judged by return value, ignoring iWritten

Symptoms: large requests occasionally truncated — `Send` returned `XTLS_OK` so all was assumed sent, but the two-tier budget accepted only part.

Cause: both the TLS record layer and the TCP write budget can make one `Send` accept only a prefix — `XTLS_OK` means "call succeeded", not "all queued". Fully isomorphic to Chapter 68's write-budget semantics.

```c bad
if ( xrtTlsStreamSend(pStream, pRequest, iSize,
		&iWritten) == XTLS_OK ) {
	/* when iWritten < iSize, the back half is lost */
}
```

```c good
while ( Sent < RequestSize ) {
	if ( xrtTlsStreamSend(pStream, pRequest + Sent,
			RequestSize - Sent, &iWritten) ==
			XTLS_AGAIN ) {
		return;   /* budget full: await the Writable callback to continue */
	}
	Sent += iWritten;
}
```

### Pitfall 3: destroying the Engine before the stream's final state

Symptoms: at program exit `xrtNetEngineDestroy` hangs, or stream callbacks execute on already-freed context.

Cause: the stream's callbacks run on Engine Workers; destroying the Engine before the stream reaches CLOSED/FAILED pulls the execution thread out from under the callbacks. Chapter 67's order "request close → wait CLOSED → Destroy → destroy Engine last" holds verbatim on TLS streams, plus waiting for background release to finish after Abort.

```c bad
xrtTlsStreamAbort(pStream);
xrtTlsStreamDestroy(pStream);
xrtNetEngineDestroy(pEngine);   /* the stream may still be winding down */
```

```c good
xrtTlsStreamAbort(pStream);
while ( (xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pStream) != XTLS_STREAM_FAILED) ) {
	xrtThreadYield();
}
xrtTlsStreamDestroy(pStream);
xrtNetEngineDestroy(pEngine);
```

## Exercises

### Basic: minimal HTTPS GET

Run `dial <某真实站点>` (some real site) and watch the output: request sent, response printed span by span, authenticated close. Set `DialConfig.Timeout` to 1 microsecond to verify the total-timeout path and error message. Acceptance: the normal path prints the full response and exits 0; the timeout path's error message carries stage information.

### Advanced: ALPN negotiation probing

Configure `TlsConfig.Protocols` as `{"h2", "http/1.1"}`; in the `Open` callback query and print the negotiated protocol name. Then switch to only `{"http/1.1"}` and compare on the same site. Hint: the negotiated result is a session's public property; an ALPN mismatch during the handshake fails the handshake — record the difference between the two failure shapes.

### Challenge: two connections with resumption

Enable `XRT_FEATURE_TLS_CLIENT_RESUME`: after the first connection ends, `TakeResume` extracts the ticket; the second dial hands the ticket to the configuration, and `xrtTlsClientResumed` confirms the PSK path was taken (no certificate messages). Compare the two handshakes' time and byte counts. Acceptance: the resumed connection really skips the certificate stage; a ticket ALPN inconsistent with the second configuration's failure explainable; all ticket memory eventually freed (Chapter 6's stats).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Hosted dial | `TlsDial`: DNS → race-connect → handshake in one; failure auto-cleans, publishes no half-open stream |
| Configuration trio | Verifier (explicitly required) / ServerName (auto from hostname) / ALPN array |
| Dual forms | callback (Events: Open/Read/Writable/Close) / Future (DialAsync + WaitFor) |
| Send semantics | partial acceptance via `iWritten`; TLS SendLimit + TCP write budget — two-tier backpressure |
| Receive consumption | `Available/Buffer/Front/Consume` zero-copy Span by span, Chapter 65's vocabulary |
| Close protocol | `Close` authenticated (close_notify drained); `Abort` for error paths |
| Exit order | request close → wait CLOSED/FAILED → Destroy the stream → destroy Engine last |
| KeyUpdate | automatic rekey at record thresholds; manual via `TlsClientKeyUpdate`; AGAIN has no half-updated state |
| Session resumption | ticket + PSK + key share; `TakeResume` transfers the reference; ALPN must match the ticket; 1.2 no resumption |
| Unimplemented | client certificates, 0-RTT, 1.2 renegotiation — those scenarios currently unavailable |
