---
num: 100
slug: xhttp-redirect
title: xhttp Automatic Behaviors: Redirects, Retries, and Cookies
volume: 卷十 扩展库：xhttp
type: practice
lead: Method rewrites for 301/302/303 and body replay for 307/308, idempotency-gated automatic retries, cross-origin credential stripping — the client's three automatic gears.
api: xhttp-http_client, xhttp-http_retry, xhttp-cookie_jar
---

## Orientation

Above Chapter 99's runtime sit three **automatic-behavior layers** — they turn "glue every client has to write" into configuration switches.**Redirects** (`http_client_redirect`): at most ten hops by default; after 301/302/303 a POST becomes GET, 307/308 keep the method but require a replayable body; cross-origin strips credentials, HTTPS→HTTP downgrade refused by default.**Automatic retries** (`http_client_retry`): **off by default** — only idempotent methods with replayable bodies retry, backoff honors `Retry-After`; the decision "should side effects repeat" stays yours.**Automatic cookies** (`http_client_cookies`): hang a Cookie Jar into the configuration and Set-Cookie enters the jar automatically while later requests carry it automatically. The three layers trim independently with explicit semantics — "automatic" is not "magic": every hop's behavior is predictable and diagnosable (Chapter 99's Info Wire fields accumulating the whole redirect chain are the evidence).

## Introduction

Redirects look simple ("follow Location"), yet their pit density tops all HTTP-client topics. After a POST receives 302, send GET or POST? (Historical schism: browsers switched to GET, the spec said keep — modern semantics: 303 clearly switches to GET, 307 clearly keeps.) Redirect to another domain — still carry `Authorization`? (No — credential following is a fresh attack surface.) HTTPS redirected to HTTP — follow? (Not by default — downgrade strips the encryption.) A body redirect must resend — what if the body is streaming? (Not replayable means failure; never silently truncate.) Every one of these has fed real CVEs. xhttp turns these decisions into a **safe-by-default protocol implementation**: mainstream-client semantics, dangerous paths behind explicit switches.

The retry default is deliberately conservative: **off by default**. Because "is retrying safe" is a business judgment — retrying a GET is harmless, retrying a POST may double-charge. What the library can decide is "technically retryable" (idempotent method + replayable body); "business-permitted" must be declared by you.

## Concepts

### Redirects: method semantics and the security boundary

```diagram flow
- 301/302/303 + standard POST -> rewritten to GET (body/framing/representation type/digest fields removed)
- 307/308 -> method and body preserved - the body must be replayable, else XHTTP_CLIENT_ERROR_REDIRECT_REPLAY
- Relative Location: resolved against the current effective URL (Chapter 102's RFC 3986 reference resolution)
- Cross-origin: strips Authorization/Proxy-Authorization/Cookie by default; FORWARD_CREDENTIALS keeps them explicitly
- HTTPS->HTTP: refused by default; ALLOW_DOWNGRADE opens it explicitly
- fragment semantics: a Location without # inherits the current fragment; an explicit empty # blocks inheritance; the fragment never enters the request-target
```

The precise boundary of method rewriting: only the **standard method** `POST` matches — a custom method `post` does not (HTTP methods are case-sensitive, the same rule family as Chapter 91); 307/308's keep path requires the body to declare replayability (Chapter 99's `xhttpbody` capability bit) — a streaming producer body cannot be redone; failing explicitly beats dropping quietly. The whole chain shares one total deadline (Chapter 99) — ten hops do not inflate the budget. Per-hop choices are independent: inherit/follow/return the original response/treat as error (the four modes of the redirect field in the call options' `xhttpcalloptions`).

### Automatic retries: idempotency and the triple condition

A retry must satisfy all three at once: **quota unused** (`MaxRetries`), **method idempotent** (decided by `xrtHttpMethodIdempotent()` — GET/HEAD/PUT/DELETE and other safe-to-repeat methods; POST is not on the list), and **body replayable** (declared by `xhttpbody` when a body exists). Missing any one blocks it — that is the technical guardrail; the business guardrail is your act of setting the quota. Backoff computation reuses the pure protocol module `http_retry`: exponential backoff (`BaseDelay`/`MaxDelay`) plus honoring the server's `Retry-After`. Call-level four modes: `DEFAULT` inherits Client / `DISABLED` off for this call / `ENABLED` (default cap when Client's quota is zero); configuration errors (unknown mode, zero MaxDelay, Base>Max) **fail before any network operation** — startup-time interception.

### Automatic cookies: hanging the jar

With a `xcookiejar` hung into `xhttpclientconfig`'s Cookie configuration: response Set-Cookies enter the jar automatically (domain/path/expiry/secure attributes handled per RFC 6265 — the semantics from Chapter 32, automated here), and later requests automatically match and carry. The jar is an independent object (persistence, sharing across Clients, partitioned isolation are all yours — the same shape as Chapter 101's cache Store). No jar hung means no automatic behavior — explicit assembly.

### The common shape of the automatic behaviors: explicit, diagnosable, independently trimmable

The three layers share three design disciplines: **safe defaults** (redirects ten hops + strip credentials + refuse downgrade; retries off; cookies off unless hung); **explicit failure** (non-replayable, invalid configuration, downgrade — all are clear error categories, never silent skips); **visible diagnostics** (Info's `ResponseWireBytes` accumulates the whole chain, `Phase` retains the actual ending phase — "how many redirect hops" is queryable from the result object: a final effective URL differing from the request URL means it followed).

## Examples

### First complete program: explicit retry configuration

The program below is from `examples/http/client_retry` — default-off versus explicit-enable:

```embed path="extlibs/xhttp/examples/http/client_retry/main.c" title="extlibs/xhttp/examples/http/client_retry/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_retry/main.c -lws2_32 -liphlpapi
retries=3 base=250000 max=5000000
```

**What just happened.** (1) `ConfigInit` defaults `MaxRetries=0` — **automatic retries do not exist by default**; after setting 3 tries/250ms base/5s cap, the retry layer comes online. (2) The output echoes the configuration — this sample is itself the configuration face's documentation: three parameters (quota/base backoff/max backoff) are all the knobs. (3) Call-level temporary overrides: `Options.Retry.Mode`'s four modes let a single call turn off or force retries — "this one must not retry" in a batch task needs no second Client. (4) Against the retry judgment guardrails: this configuration expresses only the "technical ceiling"; whether each request actually retries still walks the idempotency + replayability verdict — configuration and judgment are layered, and neither oversteps.

### Second complete program: hanging the Cookie Jar

The second program is from `examples/http/client_cookies` — the assembly face of automatic cookies:

```embed path="extlibs/xhttp/examples/http/client_cookies/main.c" title="extlibs/xhttp/examples/http/client_cookies/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_cookies/main.c -lws2_32 -liphlpapi
（示例校验 Jar 挂载后 Set-Cookie 自动进罐与后续自动携带路径）
```

**What just happened.** (1) The jar is created and placed into the Client's Cookie configuration — the Client holds the reference; your own reference may go. (2) Requests build and submit as usual — **Set-Cookie collection and Cookie return are entirely automatic**: responses enter the jar on arrival (domain/path/expiry matching semantics by the jar), and the next request to the same site automatically carries the matches. (3) The meaning of explicit assembly: without a jar the behavior simply does not exist (no hidden global jar); with one, the jar's persistence (to disk), sharing (one jar, many Clients), and isolation (partition keys) are ordinary object operations in your hands — Chapter 32's cookie semantics in the same mold as Chapter 101's cache Store: "the library provides the object, you provide the policy".

## Contracts

- **Redirect defaults**: at most ten hops; 301/302/303 + standard POST becomes GET; 307/308 keep the method (body must be replayable, else REPLAY failure).
- **Credential boundary**: cross-origin strips Authorization/Proxy-Authorization/Cookie by default; `FORWARD_CREDENTIALS` keeps explicitly; downgrade refused by default, `ALLOW_DOWNGRADE` opens explicitly.
- **fragment semantics**: no # inherits, explicit empty # blocks; the fragment stays in the final URL only, never in the target.
- **Retries off by default**: enabling requires a quota; one retry requires quota + idempotent method + replayable body simultaneously.
- **Retry configuration validation**: unknown modes/Flags, zero MaxDelay, Base>Max fail before network operations (startup interception).
- **Call-level overrides**: retry four modes and redirect four modes (inherit/follow/return original/treat as error) selectable per call.
- **Cookie mounting**: jar into config enables automatic send/receive; no jar, no behavior; the jar is a public object (persistence/sharing/isolation belong to the caller).
- **Chain budget**: the whole redirect chain shares the total deadline; Wire bytes accumulate across the chain.
- **Trimming**: `HTTP_CLIENT_REDIRECT`/`_RETRY`/`_COOKIES` independent macros.

## Pitfalls

### Pitfall 1: enabling automatic retries for non-idempotent requests

Symptom: retries enabled for POST; a network blip double-orders — inventory deducted twice, complaints arrive.

Cause: among the retry triple conditions "idempotent method" is a **technical verdict** that cannot block business semantics: your POST may be safely replayable behind a unique-key guard, but you never declared a replayable body; changing it to "a GET-ified dedup query" changes the semantics. The correct path: off by default; for non-idempotent requests that truly need retries, after confirming the business has idempotent protection, **enable explicitly for that call** (the contract allows the caller to set it after confirmation).

```c bad
Config.Retry.MaxRetries = 3;   /* on globally - POST enters the retry verdict too (a replayable body really retries) */
```

```c good
Config.Retry.MaxRetries = 3;   /* quota at the Client layer */
/* non-idempotent calls turn off one by one: */
Options.Retry.Mode = XHTTP_RETRY_DISABLED;
pCall = xrtHttpClientDo(pClient, pOrderReq, &Options, ...);
/* enable explicitly only for calls confirmed replayable */
```

### Pitfall 2: credential leaks when following cross-origin redirects

Symptom: an api.example.com request (with Authorization) gets 302'd to attacker.net — under default configuration the header is stripped (safe); but "for that legacy gateway" you turned on FORWARD_CREDENTIALS — the token reaches a third-party domain.

Cause: cross-origin credential following extends trust to the redirect target. Default stripping is protection; when opening explicitly you must also trust **your own redirect source** (it controls where Location points).

```c bad
/* on globally: every hop carries the token */
Options.Redirect.Flags |= XHTTP_REDIRECT_FORWARD_CREDENTIALS;  /* inherited globally */
```

```c good
/* keep default stripping; open per-call for the few controlled follows that truly need it,
   and only for same-site controlled redirect sources */
if ( is_trusted_redirect_source(Req) ) {
	Opts.Redirect.Flags |= XHTTP_REDIRECT_FORWARD_CREDENTIALS;
}
```

### Pitfall 3: 307/308 redirects with a streaming body

Symptom: a PUT with a producer-stream body receives 307 — the call fails `REDIRECT_REPLAY`; someone "fixes" it by truncating the already-read part and resending — the server receives a corrupt body.

Cause: 307/308 semantics require resending the body as-is; a streaming body (producer callback) cannot rewind — the library's correct behavior is failure. Truncated resend turns a protocol error into data corruption.

```c bad
/* too clever: resend the already-read part of the stream buffer as the full body */
redirect_with_partial_body(...);   /* server-side data corruption */
```

```c good
/* two right roads:
   1) requests that must follow 307/308 use a replayable body (SetBytes/SetBody with replay declared)
   2) on REPLAY failure with a streaming body: buffer the whole body, then resend as a new call */
if ( error_is(REDIRECT_REPLAY) ) {
	buffer_body_then_retry_as_new_call();
}
```

## Exercises

### Basic: the behavior matrix of the four status classes

A local service returns 301/302/303/307/308 for both GET and POST; the client follows and prints each hop's final method, final URL, and body presence. Acceptance criteria: the ten combinations' behavior matches the contract matrix; `Info.ResponseWireBytes` tallies with the hand-counted hops.

### Advanced: verifying the retry guardrails

Configure 3 retries; submit GET (idempotent), POST with byte body (replayable), POST with streaming body (non-replayable) — against an intermittently 503-ing service, measure each one's retry count. Acceptance criteria: GET and replayable POST retry to success; streaming POST reaches its final state on first failure; when `Retry-After` is present the backoff honors the server's value.

### Challenge: a login-session flow

Implement a three-step flow with a Cookie jar: login endpoint (receives the session Set-Cookie) → request a protected resource carrying the session → logout. Insert one cross-origin redirect mid-flow to verify credential stripping (no cookie to the third party by default). Acceptance criteria: the session continues automatically across the three steps; the cross-origin jump's request carries no session cookie; the jar's lifecycle separates correctly from the Client (Destroy order).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Redirect defaults | ten hops; 301/302/303+POST→GET; 307/308 keep the method |
| Body replay | 307/308 require a replayable body; non-replayable = REPLAY failure (no truncation) |
| Credential boundary | cross-origin strips the three credential kinds; FORWARD_CREDENTIALS explicit; downgrade refused by default |
| fragment | no # inherits / empty # blocks; never enters the request-target |
| Retry defaults | off; enabling sets a quota; one retry = quota + idempotent + replayable, three conditions |
| Retry backoff | Base/MaxDelay exponential + honors Retry-After; config errors fail before the network |
| Call-level overrides | retry four modes / redirect four modes - per-item control inside batches |
| Cookies | jar into config enables automatic send/receive; no jar, no behavior; persistence/sharing yours |
| Chain budget | the whole chain shares the total deadline; Wire accumulates; the final URL tells the hop outcome |
| Trimming | REDIRECT/RETRY/COOKIES independent macros |
