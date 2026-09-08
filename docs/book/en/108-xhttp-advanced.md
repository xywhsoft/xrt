---
num: 108
slug: xhttp-advanced
title: Server Wrap-Up: Compression, Range, and the Big Picture
volume: 卷十 扩展库：xhttp · 卷十收官
type: practice
lead: Content-negotiation-driven response compression, Range composition, the proxy header family, and volume ten's full picture — closing the server topics and previewing volume eleven.
api: xhttp-http_server, xhttp-http_compress, xhttp-http_cache_range
---

## Orientation

Volume ten closes. After the server chapters (103–107), the remaining advanced topics converge here: **response compression** (`reply_compress`/`server_compress` — the encoding chosen by negotiating the client's `Accept-Encoding`, response bodies compressed transparently — the mirror side of Chapter 92's decoding); **Range and composed responses** (`range_multipart` — multipart/byteranges responses for multi-range requests, the dual of Chapter 100's client-side Range composition); **the proxy header family** (`Via`/`X-Forwarded-*`'s normalized replacement `Forwarded`, `proxy_status` proxy status — the vocabulary of gateway writing). Finally the volume closes with the panorama of "one request crossing volume ten's full stack": every layer from the easy call to the compressed Range response. From Chapter 109 we enter volume eleven (the xws/xruntime/xmail/xssh extension library family).

## Introduction

Compression is a trade between bandwidth and CPU: JSON API responses often compress 70–90%, but "always gzip" runs into two client kinds — those that don't support it don't decompress (an identity fallback is mandatory), and already-compressed content should not be recompressed (compressing images again only makes them bigger and slower). So compression must be **negotiation-driven**: read `Accept-Encoding`'s q values (Chapter 91), decide by content type and size, choose the best encoding both sides support. Range composition is another trade: the client wants bytes from three ranges — one origin trip fetching segment by segment, or the server/proxy directly assembling a multipart response? The latter saves round trips, but the assembly and the boundaries (Chapter 90's smuggling defense, the multipart edition) must be correct. The shared trait of these two: **a correct implementation must read a lot of context** (negotiation headers, content metadata, cache state); xhttp makes them "configuration as policy" layers, invisible to business routing.

## Concepts

### Response compression: negotiation-driven

```diagram flow
- Negotiate: read the request's Accept-Encoding (q in thousandths - Chapter 91's model)
- Decide: content type (compressible? JSON/text yes, images no), size (small responses not worth it), existing encoding (don't compress again)
- Encode: pick the best by q and server capability (gzip/deflate/identity)
- Apply: compress as a transformed body (Chapter 107's transformed source) + Content-Encoding header + length update
```

`xhttpreplycompressconfig` carries the policy (minimum compressible length, content-type whitelist, encoding preference); the `reply_compress` layer applies compression after the Reply is built — business code writes the uncompressed body, and compression is the "exit inspection". **Identity fallback**: the client doesn't support it, or the policy judges it incompressible — identity as-is, never forced. The server runtime's `server_compress` is the same capability in service form (enabled at the middleware or runtime layer).

### Range and multipart/byteranges

A single range (`Range: bytes=0-99`) → 206 + `Content-Range`; multiple ranges (`bytes=0-99,200-299`) → 206 + `multipart/byteranges` (each part carries its own Content-Range). `range_multipart` handles the boundary assembly — the **write-side composition** dual to Chapter 90's "read-side multi-range". Chapter 100's client-side local Range composition (fragments + origin refill) ultimately rebuilds responses with this same write-side vocabulary.

### The proxy header family: gateway syntax

Every hop of a proxy chain must speak: `Via` (proxies and protocols passed through), `Forwarded` (the RFC 7239 normalized replacement for `X-Forwarded-For/Proto/Host` — `Forwarded: for=192.0.2.60;proto=https`), `proxy_status` (standardized reporting of proxy errors and upstream status). `xhttp`'s proxy-family modules (`forward`/`forwarded`/`via`/`proxy_alias`/`proxy_status` and each one's `_write`) provide both the read and write ends — the HTTP-header companion of Chapter 69's forward proxy.

### One request through volume ten's full stack

```diagram flow
- Call: xrtHttpClientGetSync (Chapter 97 easy) -> runtime freeze (98)
- Connection: pool shard takes/reuses a connection (98) -> TLS (volume eight) -> request write-out (volume nine, 89)
- Automatic behavior: redirect/cookie/retry (99) -> cache lookup/validation (100)
- URL/query: parse and build (101/102) -> auth injection (105)
- Server: event chain (103) -> middleware onion (104) -> static/routing
- Response: body source (107) -> compression negotiation (this chapter) -> framing (volume nine, 90) -> transport
- Client receives: decoding (volume nine, 92) -> cache storage (100) -> result snapshot (97)
```

Every station is one chapter of this volume — this is xhttp's building-block acceptance map.

## Examples

### First complete program: negotiation-driven compression decisions and output

The program below is from `examples/http/reply_compress` — compressing responses by client capability:

```embed path="extlibs/xhttp/examples/http/reply_compress/main.c" title="extlibs/xhttp/examples/http/reply_compress/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/reply_compress/main.c -lws2_32 -liphlpapi
（输出压缩协商与产出响应的自检结果）
```

**What just happened.** (1) `xrtHttpAcceptEncodingInit/Add("gzip, identity;q=0.5")` builds the client capability declaration — q in thousandths (gzip full score, identity half) — a direct use of Chapter 91's negotiation model. (2) The ordinary `ReplyCreate/SetBytes` builds the uncompressed JSON response — **business code is unaware of compression**. (3) The compression layer reads capability + policy, decides, and produces the compressed Reply — the higher-q gzip wins, a transformed body (Chapter 107's source) replaces the original, and the headers update. (4) The contrast-experiment path: rerun with the capability switched to `identity` — identity fallback, as-is; paired with the client's `client_decompress` (Chapter 92's decoding) it forms the complete compression loop.

### Second complete program: the multi-range composed response

The second program is from `examples/http/range_multipart` — the write side of multipart/byteranges:

```embed path="extlibs/xhttp/examples/http/range_multipart/main.c" title="extlibs/xhttp/examples/http/range_multipart/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/range_multipart/main.c -lws2_32 -liphlpapi
（输出多区间响应组合的自检结果）
```

**What just happened.** (1) Parse the range set of the multi-range request (Chapter 90's Range vocabulary) → slice each range out of the body → assemble multipart/byteranges by boundary: boundary lines, each part's Content-Range, part bodies, the closing boundary — **the write-side boundary discipline symmetric with Chapter 90's read-side smuggling defense** (a boundary off by one position is a smuggling window). (2) Echoing Chapter 100: what the client cache's local composition (owned fragments + origin refill) produces is exactly this kind of response — one chapter on the write side, one on the read side, together a complete Range system. (3) Companion inspections: `vary` (Vary-driven cache key partitioning — Chapter 100's Vary field on the protocol side), `proxy_status`/`proxy_status_write` (proxy status reporting), `via`/`forwarded` (the proxy-chain header family) — the complete vocabulary of gateway writing.

### Three closing routes for readers

**Backend service developers**: 103→104→105→107 is the trunk (event chain, onion, auth, bodies); SSE (106) as business demands — this chapter's compression and Range are "come back before launch" optimization chapters. **Client/tool developers**: 97→98→99→101→102 are ready at P18; here you need only the compression-negotiation section (paired with Chapter 92's decoding). **Gateway authors**: this chapter's proxy header family + Chapter 103's server + Chapter 98's client runtime are all your raw materials — the `exchange`/`forward` examples demonstrate both forms of "receive a request and forward it" assembled in one process. Whichever route, the closing self-check has a single standard: **for every station of volume ten's panorama you can point to the chapter** — if you can point, the building blocks are in place; if not, go back to that chapter — an acceptance more honest than "finished reading".

## Contracts

- **Compression negotiation**: read Accept-Encoding's q (thousandths); decide by content type/size/existing encoding; identity fallback never forced; output is a transformed body + header updates.
- **Compression policy**: minimum length/type whitelist/encoding preference belong to configuration; business routing unaware (the exit-inspection shape).
- **Range write side**: single range 206+Content-Range; multi-range 206+multipart/byteranges; atomicity and correctness of boundary assembly are the symmetric face of smuggling defense.
- **Proxy header family**: Via/Forwarded (RFC 7239)/proxy_status on both read and write ends; the normalized replacement for X-Forwarded-*.
- **Vary**: the response-declared key-partitioning field — a cache-primary-key component (Chapter 100); `Vary: *` is not cacheable.
- **Composition discipline**: the compression layer changes no semantics (Content-Length updates/framing adaptation automatic); Range and compression stack (Range-then-compress or the reverse — explicit policy).
- **Trimming**: `HTTP_COMPRESS`/Range/proxy-family independent macros — zero cost for deployments that don't compress.

## Pitfalls

### Pitfall 1: recompressing already-compressed content

Symptom: JPEG/MP4 responses grow after compression and the CPU burns for nothing — a 0% compression ratio plus overhead.

Cause: the content-type whitelist is the policy's core ingredient — images/videos/content already carrying `Content-Encoding` are skipped. The compression layer's default whitelist excludes them, but playing clever with "compress everything" walks right into it.

```c bad
Config.MinLength = 0;   /* compress everything: JPEG too - bigger and slower */
```

```c good
/* the type whitelist by default holds only compressible text families; the length threshold blocks small responses */
/* when customizing, explicitly exclude image/* video/* and already-encoded content */
```

### Pitfall 2: writing the Range boundary string wrong (a smuggling window)

Symptom: a hand-spliced multipart boundary collides with the body — the peer parses body bytes as the boundary, ranges misaligned.

Cause: boundary uniqueness is multipart's security premise (Chapter 102's exercise boundary requirement applies equally) — `range_multipart`'s boundary generation has anti-collision built in; hand-spliced strings do not.

```c bad
snprintf(Out, "----boundary\r\nContent-Range: ...");  /* fixed boundary: collides if the body contains the same string */
```

```c good
/* use the Range composition layer: boundary generation/escaping/termination built in correct */
/* when hand-splicing: random boundary + body scan for collision + correct "--" prefixes and suffixes */
```

### Pitfall 3: losing Forwarded semantics in the proxy chain

Symptom: after multiple proxy layers the client IP the server sees is the previous hop's — auditing and rate limiting all distorted.

Cause: each hop's proxy must append (not overwrite) Forwarded/X-Forwarded-For; overwriting discards history. The `forwarded` module's append semantics are built in — hand-splicing easily writes an overwrite.

```c bad
set_header("X-Forwarded-For", current_peer_ip);   /* overwrite: the prior history evaporates */
```

```c good
/* append the current hop, preserving the existing chain */
forwarded_append(reply, current_peer_ip, proto);
```

## Exercises

### Basic: the compression negotiation matrix

Construct four client capabilities (gzip full score/identity/no header at all/`*;q=0`), run the same JSON response through the compression layer, and record the four outputs. Acceptance criteria: negotiation results match the q derivation; identity fallback when unsupported.

### Advanced: a multi-range downloader

The client requests three ranges → the server responds via `range_multipart` → the client parses the multipart and reassembles the file (Chapter 90's Body + Chapter 91's parameter layer). Acceptance criteria: the reassembled file's hash matches the original; boundary handling with no overlap and no gap.

### Challenge: a two-hop proxy gateway

An intermediate proxy (server + client forms combined: receive request → append Forwarded → forward upstream → receive response → append Via → return to client). Acceptance criteria: after two hops the server sees the complete Forwarded chain; Via accumulates in order; on upstream 5xx, proxy_status reports per the standard — verify header by header with curl.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Compression negotiation | Accept-Encoding q (thousandths) driven; identity fallback never forced; the exit-inspection shape |
| Compression policy | minimum length/type whitelist/encoding preference in configuration; business unaware; already-compressed skipped |
| Range write side | single range 206+Content-Range; multi-range multipart/byteranges; boundary anti-collision |
| Proxy header family | Via appends/Forwarded (RFC 7239)/proxy_status — every hop appends, never overwrites |
| Vary | the response-declared cache key partitioning; `Vary: *` not cacheable (Chapter 100) |
| Composition discipline | compression auto-updates length and framing; Range×compression stackable with explicit policy |
| Volume ten panorama | easy→runtime→pool→TLS→automatic behavior→cache→URL→server chain→bodies→compression — chapter for chapter |
| Trimming | COMPRESS/Range/proxy-family independent macros |
