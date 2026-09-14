---
num: 88
slug: tls-resume
title: Session Resumption: Tickets, PSK, and the Object Contract
volume: 卷八 安全 · 卷八收官
type: practice
lead: The resumption object packages "one more handshake" into an immutable value — ticket issuance, client takeover, and the next connection's PSK+DHE loop; Volume 8's closing chapter.
api: tls, tls_resume, tls_session, crypto
---

## Orientation

Volume 8's finale. Session resumption solves a pure performance problem: a full handshake costs one certificate verification plus several asymmetric operations (1-2 RTT); a resumption handshake uses the PSK derived from the last session to compress that to near one RTT with zero certificate transfer — the experience watershed for mobile networks and high-frequency short connections. Chapter 82 covered the client's usage face (ticket cache/`TakeResume`); this chapter covers the complete loop and object contract: the **`xtlsresume` resumption object** (ticket + PSK + route binding as an immutable value, cross-thread shareable, cache policy yours), the server's **issuance and acceptance** (both sides already foreshadowed in Chapter 86), and a dual-handshake loop example — session one issues a ticket, the client takes over, session two resumes via PSK+DHE. The security boundaries are fully laid out too: resumption always keeps ECDHE (forward secrecy undegraded), a binder error is an authentication failure, and route binding prevents ticket cross-use.

## Introduction

Your API gateway handles nearly ten thousand TLS connections per second, each full handshake about 2 RTT + one certificate-chain verification — handshake overhead exceeds the processing time of most requests themselves. And the client? A mobile device re-handshakes on every wakeup; on a weak network a 300-millisecond handshake is perceptible lag. TLS 1.3's answer: at handshake end the server sends a **NewSessionTicket** (ticket); the client stores "ticket + the PSK derived from the handshake"; the next connection's ClientHello carries the ticket and a **binder** (an HMAC computed with the PSK — proving "I really am that previous client"); if the server verifies the binder, the certificate stage is skipped, and both sides derive new keys from the PSK + a fresh ECDHE — fast, cheap, and sacrificing no forward secrecy.

The real engineering design question is "who owns the tickets": XRT's answer is **value object + your cache** — `xtlsresume` packages the resumption assets into an immutable object (deep copy, reference counting, zeroed on release), and whether you store them in a memory Map, on disk, or in Redis is the application's call; the protocol layer keeps no global cache (multi-process/multi-tenant/persistence needs differ too much). This "library provides the value, application provides the policy" divide is this chapter's main line.

## Concepts

### The resumption object: one immutable block of resumption assets

`xtlsresume` deep-copies ticket, PSK, SNI, ALPN, and optional peer identity at creation — one exact allocation, then **immutable**; reference-counted sharing across threads; the whole block zeroed before the last reference releases (reusing `xrtSecureZero`). Constraints (currently TLS 1.3 only): ticket 1..65535 bytes, PSK length equal to the suite's digest length, lifetime 1..604800 seconds, ALPN ≤255 bytes, SNI rejects embedded null bytes. The validity `[IssuedAt, ExpiresAt)` is a half-open interval, and **wall-clock rollback to before issuance safely invalidates** (no lifetime extension); `xrtTlsResumeTicketAge` validates validity first, then rounds to milliseconds and adds `AgeAdd` modulo 32 bits — the protocol's age-obfuscation arithmetic built in. The views published by `ResumeInfo` are valid only while a reference lives, and **`Secret` is a sensitive read-only view** — never modify, never log unprotected (log hygiene, Chapter 77's discipline).

The optional `PeerIdentity` is a caller identity domain that never enters the wire encoding — store the last session's verified leaf-certificate digest, and a resumed connection "inheriting an authenticated identity" has its landing point (Chapter 82: new tickets inherit the original `PeerIdentity`).

### The loop: issue → take over → resume

```diagram flow
- Session one (full handshake): certificate verification + ECDHE + mutual Finished -> READY
- Issue: server ServerTicket(ticket, lifetime, &resumption object) -> NewSessionTicket sent to the client; resumption-object ownership handed to the server caller (storing it is yours)
- Takeover: client receives ticket -> deep-copies ticket/SNI/ALPN/identity -> TakeResume transfers the reference to the application
- Cache: application-defined policy (memory Map / disk / Redis; capacity, eviction, tenant isolation)
- Session two: ClientConfig.Resume = object -> ClientHello carries ticket + binder + a normal key share
- Accept: server's Resume callback looks up the ticket -> validates version/suite/SNI/ALPN/validity/age -> PSK+DHE -> certificate skipped
```

Four key points. (1) **The binder is inside the ClientHello** — "proving possession of the PSK" happens when the Hello goes out (`accepted=not yet`: whether the server accepts waits for the ServerHello, Chapter 82's semantics). (2) **The server may refuse**: ticket expired, route mismatch, capacity policy — the client safely falls back to the full handshake (`TlsClientResumed` becomes true only after the server accepts). (3) **Always PSK+DHE**: the resumed connection still carries an x25519 key share, and the fresh ECDHE mixes into derivation — even if the PSK leaks, the session keys remain protected by that run's ECDHE (forward secrecy undegraded). (4) **Route binding**: with SNI/ALPN omitted, the client **exactly inherits** the object's binding; an explicit SNI must match completely, an explicit ALPN list must contain the ticket's protocol — tickets cannot "cross over" to another domain or protocol.

### The API faces at both ends

- **Server issuance** (Chapter 86): `xrtTlsServerTicket` (bring your own ticket) or `TicketNew` (32 bytes securely random + 86400-second default); on `XTLS_AGAIN` the output is `NULL` with state unchanged — drain and retry.
- **Client cache**: `ResumeLimit` (0..64, default 4) bounds retention, evicting oldest when full; `TakeResume` transfers references in arrival order; `ResumeDropped` counts disabled/evicted/OOM drops — cache OOM never affects already-ready connections.
- **The next connection**: `ClientConfig.Resume = 对象` (the session holds an independent reference; yours may be released immediately); expired/not-yet-valid/suite-policy-disabled/route-mismatch are all rejected **before session allocation** — bad tickets never enter the state machine.

## Examples

### First complete program: the resumption object's lifecycle

The following program comes from `examples/tls/resume/main.c` — the minimal loop of create, query, validate, release:

```embed path="examples/tls/resume/main.c" title="examples/tls/resume/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/resume/main.c -lws2_32 -liphlpapi
ticket=4 bytes, secret=32 bytes, lifetime=3600 seconds
```

**What just happened.** (1) The configuration's five elements complete: Cipher (the resumption suite), Ticket (opaque bytes), Secret (the PSK — length must equal the suite's digest length, here 32), ServerName and Protocol (route binding), Lifetime. (2) The `ResumeInfo` view checks: ticket 4 bytes, secret 32 bytes, lifetime 3600 — the object turns "everything resumption needs" into queryable immutable facts. (3) This is the **value object's unit specimen**: no network involved, pure create→read→release; your cache policy (where to store, how long, how to evict) starts designing from this object.

### Second complete program: the next connection with a ticket

The second program comes from `examples/tls/client_resume/main.c` — the first ClientHello's inspection after handing the resumption object to ClientConfig:

```embed path="examples/tls/client_resume/main.c" title="examples/tls/client_resume/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/client_resume/main.c -lws2_32 -liphlpapi
client hello=~290 bytes, accepted=not yet
```

**What just happened.** (1) The ClientHello is about 290 bytes (floating 1–2 bytes with key material) — slightly larger than a full handshake's Hello (the pre_shared_key extension and binder added), but what it buys is the server skipping the entire certificate flight. (2) **`accepted=not yet` is this chapter's most chew-worthy output**: the binder is already in the Hello (the client proves PSK possession), but "does the server accept" waits for the ServerHello — resumption is **negotiation, not command**; the client accordingly also carries a normal key share, falling back to the full handshake if refused. (3) With SNI/ALPN omitted, the client exactly inherits from the resumption object — route binding turns from "configuration" into "carried by the ticket".

### Third complete program: the dual-handshake loop

The third program comes from `examples/tls/resume_tour/main.c` — a full-chain self-check of two handshakes in one process:

```embed path="examples/tls/resume_tour/main.c" title="examples/tls/resume_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -I examples/tls -include xrt.h impl.c examples/tls/resume_tour/main.c -lws2_32 -liphlpapi
resume: handshake #1 + ticket-new issued ok
resume: client took ticket + retain/validat/ticketage ok
resume: custom ticket via server-ticket ok
resume: handshake #2 resumed + dropped counted ok
```

**What just happened.** Four lines for the loop's four stations. (1) **Session one**: full certificate handshake + `TicketNew` issuance (random ticket + default lifetime). (2) **Client takeover**: after `TakeResume` gets the reference — `Retain` (a cached copy), `Info` validation, `TicketAge` age arithmetic — the value object's full operational face. (3) **Custom ticket**: the server's `ServerTicket` issues with the caller's own ticket bytes — when multiple instances share backend storage, the ticket format is yours to define. (4) **Session two**: the resumption handshake holds (`Resumed` true) and "drops after explicit disable are counted" — verifying `ResumeDropped`'s counting semantics. No global state between the two handshakes — the loop runs purely on "the object passing between the two ends". The companion `examples/tls/session_tour` (6 output lines) is the session machine's full-feature tour: four Feed forms, Span send/receive, close_notify — a supplementary specimen beyond this chapter's three examples.

## Contracts

- **Object contract**: one deep-copy allocation, immutable, reference-counted sharing, zeroed before the last release; TLS 1.3 only; field boundaries (ticket 1..65535 / PSK = suite digest length / lifetime 1..604800 / ALPN ≤255 / SNI no embedded nulls).
- **Time semantics**: half-open validity `[IssuedAt, ExpiresAt)`; wall-clock rollback safely invalidates without extension; `TicketAge` validates first then does the 32-bit modular addition, leaving output untouched on failure.
- **Sensitive views**: `Info().Secret` is read-only sensitive — never modify, never log unprotected; `PeerIdentity` is a caller domain never on the wire.
- **Server issuance**: object ownership transferred; no global cache; on AGAIN output NULL with state unchanged, retryable; hard caps pre-checked.
- **Client cache**: `ResumeLimit` 0..64 default 4, evicting oldest when full; `TakeResume` transfers references; drop counting never false-positives; cache OOM never affects ready connections.
- **Resumption negotiation**: the binder proves PSK possession with the ClientHello; acceptance is the server's call; refusal safely falls back to the full handshake (the key share always carried).
- **PSK+DHE mandatory**: ECDHE always kept, forward secrecy undegraded; no pure PSK, no 0-RTT.
- **Route binding**: omitted means exact inheritance; explicit SNI must fully match, explicit ALPN must contain the ticket's protocol; mismatch rejected before session allocation.
- **Authentication boundary**: ticket metadata matching with a binder error = authentication failure (fatal, no degradation) — built into the server's state machine; the callback must never forge a "found".

## Pitfalls

### Pitfall 1: logging the PSK like an ordinary buffer / metric

Symptoms: while troubleshooting, printing the resumption object's contents — the PSK secret enters the logging system, permanently archiving "the next handshake's proof of identity".

Cause: `ResumeInfo`'s Secret is a sensitive read-only view; a value object being "immutable" does not mean "insensitive". Chapter 77's keys-never-in-logs discipline applies to it verbatim.

```c bad
xrtTlsResumeInfo(pResume, &Info);
log_debug("ticket=%.*s psk=%.*s",   /* the PSK into logs: a leak */
	(int)Info.Ticket.Size, Info.Ticket.Data,
	(int)Info.Secret.Size, Info.Secret.Data);
```

```c good
xrtTlsResumeInfo(pResume, &Info);
/* the ticket is not secret — a digest is printable; the PSK gets only length and existence */
log_debug("ticket=%zu bytes psk=%zu bytes expires=%lld",
	Info.Ticket.Size, Info.Secret.Size,
	(long long)Info.ExpiresAt);
```

### Pitfall 2: erroring on resumption refusal without retrying

Symptoms: after a server restart (in-memory tickets lost), all clients fail — "falling back to the full handshake" mistaken for an error path.

Cause: resumption is negotiation, not guarantee — the server's tickets lost, expired, or refused by capacity policy are all normal; the protocol's expected behavior is exactly **automatically taking the full handshake** (the ClientHello always carries a key share too). Treating it as an exception means not understanding `accepted=not yet`.

```c bad
if ( !xrtTlsClientResumed(pSession) ) {
	return error("resume rejected");   /* a normal fallback treated as a fault */
}
```

```c good
/* query after the handshake completes: resumed is a performance hint; the connection is equally secure */
bool bResumed = xrtTlsClientResumed(pSession);
metrics_record("tls_resumed", bResumed);
/* resumed or not, the connection has passed full or PSK authentication — use it directly */
```

### Pitfall 3: reusing tickets across "routing domains"

Symptoms: in an internal service mesh the same client code connects to several environments (prod/staging), tickets cross over — connections fail or (worse) identity is mis-inherited.

Cause: tickets bind SNI/ALPN, but **if several environments share one SNI**, the object layer cannot distinguish — the crossed-over binder may happen to pass, and `PeerIdentity` inherits the other environment's identity. Routing isolation is the application layer's responsibility.

```c bad
/* one global cache serving all environments */
g_ResumeCache = cache_create();
resume = cache_take(g_ResumeCache, host);   /* same host, different environment */
```

```c good
/* add the routing domain (environment/tenant/cluster) into the cache key, or a separate cache per domain */
resume = cache_take(cache_for(env), host);
/* more thorough: store an environment tag in PeerIdentity at construction,
   and only use the resumption after verifying the tag matches */
```

## Exercises

### Basic: the object-boundary probe

Run three negative experiments on the resume example: PSK length changed to 31 (≠ suite digest length), lifetime 0, SNI with an embedded `\0` — verify creation rejection and read the error chain. Acceptance: all three fail at creation; the error messages distinguish the field categories.

### Advanced: an in-memory ticket cache with TTL

Implement `resume_cache`: keyed by (SNI, ticket hash), capacity 64, TTL expiry eviction, thread-safe (Chapter 53's primitives); reconcile eviction counts against `ResumeDropped`. Wire into the dual-handshake loop (a resume_tour retrofit): session two takes its ticket from the cache. Acceptance: 10 repeated round trips all resume successfully; with capacity squeezed to 1, eviction behaves as expected; the PSK never leaves the cache boundary (code review).

### Challenge: a resumption-performance comparison benchmark

Over Chapter 86's memory bridge, measure full handshake vs resumed handshake, 1000 each, for (a) Drive rounds, (b) total bytes both ways, (c) presence of certificate messages. Output a comparison table and explain each difference's origin (the certificate flight's presence, round-trip count, Hello size). Acceptance: data reproducible (fixed time source); difference directions consistent with TLS 1.3's design expectations; the conclusion states clearly "which workloads benefit most from resumption".

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| The resumption object | ticket + PSK + route binding (SNI/ALPN) + optional identity; immutable value, reference-shared, zeroed on release |
| Object boundaries | TLS 1.3 only; ticket 1..65535; PSK = suite digest length; lifetime 1..604800; ALPN ≤255 |
| Time semantics | `[IssuedAt, ExpiresAt)` half-open; wall-clock rollback safely invalidates; TicketAge 32-bit modular addition |
| The loop's five stations | full handshake → ServerTicket issuance → TakeResume takeover → application cache → next connection PSK |
| Binder semantics | proves PSK possession with the ClientHello; acceptance waits for the ServerHello (accepted=not yet) |
| Fallback semantics | refusal automatically takes the full handshake (key share always carried); resumed is a performance hint, not a security verdict |
| PSK+DHE | resumption still carries ECDHE; forward secrecy undegraded; no pure PSK/0-RTT |
| Route binding | omitted means inherited; explicit must fully match/contain; mismatch rejected before allocation; cross-domain isolation is the application's |
| Cache boundary | the library provides the value object: Limit 0..64/eviction/drop counting on the client; after server issuance, yours to manage |
| Authentication boundary | metadata matching + binder error = authentication failure, fatal; protocol-built-in, unbypassable |
