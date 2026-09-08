---
num: 105
slug: xhttp-auth
title: Authentication: Basic, Bearer, and Digest Sessions
volume: 卷十 扩展库：xhttp
type: practice
lead: Both ends of the three schemes: Basic's plaintext baseline, Bearer's token form, Digest's nonce challenge and replay table — authentication is a protocol, not a slogan.
api: xhttp-http_auth, xhttp-http_server, xhttp-http_digest
---

## Orientation

"Add some auth" in HTTP is a selection question across three scheme families. **Basic**: `Authorization: Basic base64(用户:密码)` (username:password) — plaintext (an acceptable form only under TLS), one line per end. **Bearer**: `Authorization: Bearer <令牌>` (token) — the vehicle of the OAuth2/JWT era; the client's only job is to carry it. **Digest**: the server issues a nonce challenge, the client answers with `H(用户:域:密码)` (user:realm:password) — a challenge-response protocol where **the password never travels the wire**, with built-in replay defense (nonce + nc counting); the server manages nonce lifetimes, the replay table, and the `Authentication-Info` receipt. xhttp implements both ends of all three schemes as structured APIs: client-side construction (`xrtHttpBasicWrite`/`BearerWrite`/the Digest response), server-side reading (`xrtHttpServerRequestBasicAuth/BearerAuth/DigestAuth` three states), and Digest's full verification chain (`xrtHttpDigestVerify`'s four verify states + a thread-safe replay table). Chapter 104's auth middleware is assembled from these.

## Introduction

Three deployment forms, each in its seat. Intranet tools: Basic + TLS is enough — one line of code per end; the password is plaintext but the tunnel is encrypted. API platforms: Bearer — token issuance/refresh is the platform's business, the server only validates tokens; the client's "authentication" is simply carrying it. Untrusted environments or protocol compliance (password protection under HTTP semantics): Digest — password digests stored server-side, challenge-response defending against replay; the price is that the server maintains nonce and replay state. **The cost of choosing wrong**: running Basic over plaintext HTTP equals broadcasting the password; using stateless Bearer on a platform that needs token revocation equals no revocation at all; using Digest as "a safer Basic" without managing nonce expiry leaves the replay window just as open.

The unified shape of the auth APIs deserves attention: everything goes through the `XHTTP_NEXT_END/ITEM/ERROR` three states (missing/valid/error) — the same contract as structured field reading (Chapters 91/103); challenge construction "appends" (AddChallenge — multiple schemes coexist) while the receipt "sets" (SetDigestInfo — a unique field) — RFC's multi-value/unique semantics land on the API's shape.

## Concepts

### Basic: one line per end and the plaintext baseline

The client `xrtHttpBasicWrite(用户, 密码, 输出, 容量, &长度)` (user, password, output, capacity, &length) produces the `Basic <base64>` field value; the server `xrtHttpServerRequestBasicAuth(请求, 解码缓冲, 容量, &长度, &描述符)` (request, decode buffer, capacity, &length, &descriptor) — **the decoded results of Basic and Digest borrow the caller's buffer** (a short buffer publishes only the exact required length; an error clears the descriptor but leaves the caller's length and body untouched — never committing half a plaintext early). Baseline discipline: Basic only under TLS; Basic over plaintext `http` is a configuration accident.

### Bearer: carrying tokens and challenges

The client `xrtHttpBearerWrite` produces `Bearer <token>`; the server `xrtHttpServerRequestBearerAuth` takes a token view (borrowing the request snapshot — the token itself belongs to your validation layer: local signature checks, database lookups, calls to an auth service). **Bearer's security boundary lives in token management**: issuance, expiry, revocation, least-privilege scopes — the protocol layer is only the vehicle; the 401 challenge (`WWW-Authenticate: Bearer error="invalid_token"`) construction entry owns the syntax, policy belongs to the application.

### Digest: challenge, response, and the four verify states

```diagram flow
- Challenge: server 401 + WWW-Authenticate: Digest realm/nonce/qop/algorithm/opaque (AddChallenge)
- Response: client caches H(A1)=H(user:realm:password); computes the request-digest answer per nonce+nc
- Verify: server looks up H(A1) (by username/userhash) -> fills the verify descriptor (method/target/challenge)
  -> xrtHttpDigestVerify -> the four-state verdict
- Receipt: after VALID, DigestRspAuth generates rspauth -> SetDigestInfo sets Authentication-Info
- Replay: only VALID commits nc; ReplayCheck is the thread-safe replay table (distributed uses ReplayKey + shared storage)
```

The exact semantics of the four states: `XHTTP_DIGEST_VERIFY_VALID` (signature and proof all correct), `_STALE` (**signature and proof correct but the nonce expired** — the client should retry with a fresh nonce; not a failure), `_INVALID` (proof wrong — reject), `_ERROR` (input/structure error). **Only VALID commits the monotonic nc** — committing early would let a wrong proof seize a legitimate client's count (the replay table poisoned). Nonce management: built into the default entry; custom nonces or a rotating key-ring use the `xrtHttpDigestProofVerify`+`NonceVerify` two-layer combination. The `Authentication-Info` receipt (rspauth mutual authentication + nextnonce rotation) is generated after successful verification from the same proof context — the complete loop of challenge and response.

### Deployment forms of replay defense

Single process: `xrtHttpDigestReplayCheck` — a thread-safe replay table with a hard capacity; multi-process/distributed: `xrtHttpDigestReplayKey` generates the canonical key, and shared storage performs **atomic max-value updates with expiry** (nc is a monotonic counter — a CAS-semantics max update). The replay window equals the nonce lifetime; the lifetime is a tuning parameter between security and experience.

### Client-side challenge parsing

`xrtHttpChallengeNext` iterates the multi-scheme challenges in a 401 response (`Digest realm=..., Basic ...` mixed declarations) — the client picks a scheme by capability: with a password cache choose Digest, with a token choose Bearer, fall back to Basic. The policy layer of automatic retry (Chapter 99), after receiving a 401, assembles from here.

## Examples

### First complete program: per-scheme parsing of a server challenge

The program below is from `examples/http/auth` — structured reading of a multi-scheme challenge:

```embed path="extlibs/xhttp/examples/http/auth/main.c" title="extlibs/xhttp/examples/http/auth/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/auth/main.c -lws2_32 -liphlpapi
Digest
Basic
```

**What just happened.** (1) The input is the mixed challenge `Digest realm="api", Basic QWxhZGRpbjpvcGVuIHNlc2VtZQ==` — two scheme declarations in one `WWW-Authenticate` value. (2) `xrtHttpChallengeNext` iterates by offset, yielding each `xhttpauth` (Scheme view + parameters) — one item for Digest and one for Basic; the same shape as Chapter 91's field-family iteration (comma domain, offset-held). (3) The starting point of client policy: scan in order, match by capability — "what the server declared" is structured fact, not string guessing. Digest's parameters (realm/nonce/qop) continue with Chapter 91's parameter-layer parsing.

### Second complete program: the shortest path for both Basic ends

The second program is from `examples/http/auth_basic` — client-side construction:

```embed path="extlibs/xhttp/examples/http/auth_basic/main.c" title="extlibs/xhttp/examples/http/auth_basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/auth_basic/main.c -lws2_32 -liphlpapi
Basic QWxhZGRpbjpvcGVuIHNlc2VtZQ==
```

**What just happened.** (1) `xrtHttpBasicWrite("Aladdin", "open sesame")` produces the complete field value in one call — Base64 (Chapter 28) built in, capacity atomicity as everywhere in the library. (2) The output is exactly RFC 7617's canonical example value — standard vectors anchoring implementation correctness yet again. (3) The server counterpart `xrtHttpServerRequestBasicAuth` decodes into the caller's buffer — four calls across both ends are all of Basic. The companion example family: `auth_bearer` (token construction), `auth_digest` (full challenge-response), `auth_digest_client`/`auth_digest_session` (sessions and nonce reuse), `auth_digest_nonce`/`_replay` (nonce and replay), `digest_sha2` (the SHA-2 algorithm family) — every segment of Digest has its own sample.

## Contracts

- **Three states unified**: the reading family `END` (missing) / `ITEM` (valid) / `ERROR` (duplicate field, illegal syntax, scheme mismatch, decode failure); duplicate Authorization falls under Header protocol error.
- **Borrowing boundary**: generic and Bearer results borrow the request snapshot; Basic and Digest decoded results borrow the caller's buffer; a short buffer reports the exact length only, an error leaves the caller's state untouched.
- **Basic baseline**: plaintext (base64 is not encryption); only under TLS; one call per end on both sides.
- **Bearer boundary**: token validation/revocation/permissions belong to the application layer; challenge syntax construction belongs to the auth layer.
- **Digest four states**: VALID/STALE (signature right but nonce expired — retryable)/INVALID (proof wrong)/ERROR; only VALID commits nc.
- **Nonce management**: built in by default; custom uses the ProofVerify+NonceVerify combination; lifetime = replay window.
- **Replay table**: single-process ReplayCheck (thread-safe, hard-capped); distributed ReplayKey + shared storage with atomic max updates.
- **Challenge/receipt**: Add appends multiple schemes; Set installs the unique Authentication-Info (rspauth+nextnonce); 401/407 status is never changed automatically — policy belongs to the application.
- **Trimming**: `HTTP_AUTH`/`HTTP_DIGEST` independent; the SHA-2 algorithm family optional.

## Pitfalls

### Pitfall 1: running Basic over plaintext HTTP

Symptom: packet capture shows `Authorization: Basic <base64(密码)>` (password) — the password is effectively broadcast.

Cause: base64 is encoding, not encryption. All of Basic's security comes from the transport layer — under TLS it is merely "shoulder-surfing prevention" grade.

```c bad
/* http:// intranet address + Basic: intranet captures/proxy logs full of plaintext passwords */
xrtHttpRequestSetBasicAuth(pReq, User, Pass);
send_http(url);   /* no TLS */
```

```c good
/* Basic goes only with https */
if ( !url_is_https(pUrl) ) {
	return error("basic auth requires TLS");
}
xrtHttpRequestSetBasicAuth(pReq, User, Pass);
```

### Pitfall 2: Digest verification treats STALE as failure

Symptom: legitimate clients whose nonce expired receive 401 and get logged out — the experience breaks; or conversely, treating INVALID as STALE and allowing retries — brute-force proofs loop forever.

Cause: the four states must be branched: STALE means "start over with a fresh nonce" (the client already has H(A1) and can retry seamlessly), INVALID means "the proof is simply wrong" (reject and count it).

```c bad
switch ( xrtHttpDigestVerify(&V) ) {
default:
	reject_and_logout();   /* STALE is also treated as failure */
}
```

```c good
xhttpdigestverifycheck R = xrtHttpDigestVerify(&V, Key, Ctx, Now, Life, Skew, NULL);
if ( R == XHTTP_DIGEST_VERIFY_VALID ) { accept(); }
else if ( R == XHTTP_DIGEST_VERIFY_STALE ) {
	challenge_with_fresh_nonce();   /* the client retries with a fresh nonce */
} else {
	reject_and_count();             /* INVALID/ERROR: reject + count */
}
```

### Pitfall 3: committing the nc counter before verification

Symptom: the replay-table count gets seized by wrong proofs — the legitimate client's next request is judged a replay and rejected.

Cause: nc is the monotonic count of "requests already used under this nonce"; an INVALID request must not take a number — the contract is explicit that "only VALID commits".

```c bad
verify_partial(&V);                 /* only the signature verified */
replay_commit(nonce, nc);           /* take a number first - wrong or not */
if ( full_verify(&V) ) { accept(); }
```

```c good
if ( xrtHttpDigestVerify(&V, Key, Ctx, Now, Life, Skew, NULL) ==
		XHTTP_DIGEST_VERIFY_VALID ) {
	replay_commit(nonce, nc);       /* take a number only when the proof is fully correct */
	accept();
}
```

## Exercises

### Basic: the three-scheme challenge matrix

Construct a challenge containing Digest+Basic+Bearer; the client parses it and picks a scheme by configuration (has password/has token/neither), printing the choice. Acceptance criteria: all three configurations select the right scheme; the challenge parameters (realm/nonce) are readable as structure.

### Advanced: the Digest challenge-response loop

The server (Chapter 103's server + middleware) issues a Digest challenge; the client parses the nonce, computes the response, submits; the server's `DigestVerify` runs the four-state branches. Repeat the request once and verify nc increments. Acceptance criteria: first request VALID; same-nonce replay rejected by the replay table; expired nonce goes through STALE and the retry succeeds.

### Challenge: a Bearer gateway with token revocation

A Bearer gateway middleware: token lookup against a local cache (Chapter 18's Map + TTL), on miss against a backend auth service (simulated); a revocation list supported (revocation invalidates the cache). Acceptance criteria: valid tokens pass, invalid ones 401, revoked ones 401 immediately; the cache-hit path makes zero backend calls (verified by counting); tokens never enter logs (Chapter 76 discipline).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three schemes | Basic (plaintext + TLS baseline) / Bearer (token vehicle) / Digest (challenge-response) |
| Three-state reading | END missing / ITEM valid / ERROR duplicate·illegal·mismatch·decode failure |
| Borrowing boundary | generic and Bearer borrow the request; Basic/Digest decoding borrows the caller's buffer (short buffer reports exact length) |
| Challenge building | AddChallenge appends multiple schemes; the Basic/Bearer/Digest entries each fully validated |
| Digest four states | VALID / STALE (nonce expired, retryable) / INVALID (proof wrong) / ERROR |
| nc discipline | only VALID commits the monotonic counter - committing first lets wrong proofs take numbers |
| Nonce | built in by default; custom ProofVerify+NonceVerify; lifetime = replay window |
| Replay table | single-process ReplayCheck; distributed ReplayKey + shared storage atomic max |
| Receipt | after VALID, RspAuth generates and SetDigestInfo installs the unique Authentication-Info |
| Client | ChallengeNext iterates multi-scheme and picks by capability; H(A1) cached locally |
