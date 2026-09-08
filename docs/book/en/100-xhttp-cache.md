---
num: 100
slug: xhttp-cache
title: xhttp Automatic Caching: Lookup, Validation, and Invalidation
volume: 卷十 扩展库：xhttp
type: practice
lead: Fully automatic HTTP caching once a Store hangs in the configuration: freshness, conditional validation, 304 merging, Range composition, and unsafe-method invalidation.
api: xhttp-http_cache, xhttp-http_client
---

## Orientation

After Chapter 99's three automatic behaviors, caching is the largest and most flavorful. Once the `http_client_cache` layer hangs a Store (`xhttpcache`) into the Client configuration, **lookup, hit delivery, freshness judgment, conditional validation, storage, partial-response composition, and unsafe-method invalidation are all automatic** — not one line of your code changes; the second request for the same URL comes from local. The protocol side follows RFC 9111 exactly: the request's `Cache-Control` (no-store/no-cache/only-if-cached/age constraints) and the response's storability are all handled by the shared cache-policy parser — the client invents no approximate semantics. The storage object stays public — applications, proxies, and custom persistence backends use the same Store contract directly. This chapter covers configuration, the four call modes, the storage contract, and concurrent conditional commits.

## Introduction

Why should the client have caching built in instead of "the business storing responses itself"? Three reasons.**Correctness**: storability judgment (`no-store`/`Vary: *`/authenticated responses/shared-private rules), freshness computation (max-age/Expires/age correction), conditional validation (ETag/Last-Modified 304 merging) — every one is precise RFC rulework; a hand-written "store a Map, expire by time" errs in colorful ways against real response headers.**Composability**: partial-content composition for Range requests (locally held fragments + origin backfill) must cooperate with the storage structure — impossible to bolt on.**Invalidation**: invalidating same-origin related representations after successful unsafe methods (`Location`/`Content-Location`) is part of the caching protocol. xhttp implements these rules once; configure one Store and everything benefits.

## Concepts

### Assembly and call modes

```diagram flow
- Assembly: xrtHttpCacheCreate -> ClientConfig.Cache.Store -> after Create, your own reference may go
- Request arrives: mode verdict (four modes) -> cache lookup (primary key = method + effective URI + partition + Vary declarations)
- Hit and fresh: delivered directly (no network)
- Hit but stale: conditional validation (ETag/If-Modified-Since) -> on 304, merge into the record and deliver
- Miss/disabled: go to origin -> the response enters the Store per storability
```

Call-level four modes: `XHTTP_CLIENT_CACHE_DEFAULT` (honor request/response/storage policies — the normal path), `DISABLED` (bypass reads, writes, and invalidation for this call), `RELOAD` (force origin validation or refetch; a successful response can still update the cache — the "refresh" button), `ONLY` (no network; with no record it **synthesizes 504** — offline mode). The four modes stack with request-header semantics without conflict — the `only-if-cached` request header is handled by the same policy parser.

### The storage contract: what enters the Store

- **Private cache by default**; `Shared` explicitly switches to shared semantics (fields and authorization rules differ).
- **Never stored**: `no-store`, non-cacheable status codes, `Vary: *`, authenticated/private responses forbidden to shared stores.
- **Heuristic lifetime**: with no explicit freshness, a limited heuristic on `Last-Modified` may be enabled — 10% by default, at most one day, globally disableable (conservative defaults; privacy and correctness first).
- **Grow on demand**: the encoding body grows only after truly arriving, nothing reserved per Call; `MaxBody` (default 8 MiB) is the hard cap on a single origin capture — over the cap stops capturing but **fails open by default** (the network response still delivers, just uncached); small bodies start at 256 B and double.
- **Release discipline**: after storage or replay completes, candidate records, field snapshots, and captured bodies are released before the final state is published — they do not linger with the user-held result reference.

### Conditional commits and concurrency

304 merging, HEAD metadata updates, and Range fragment composition all **do not use plain Put overwrites** — they use the Store's `Replace`/`RemoveRecord` current-version conditions: when a 304 commit conflicts (a concurrent updater won), the current request still delivers its own validated snapshot while the cache keeps the concurrent updater's result — **never lose an update to save your own view**. This optimistic concurrency shares the idea with Chapter 18's container invalidation rules: if someone changed it between your read and your write and the write condition fails, each goes its own way.

### Range composition and unsafe invalidation

**Local Range composition**: requesting `Range: bytes=0-99` while a `0-199` record exists locally — delivered by slicing straight from cache (no network); when only fragments exist locally, the "local fragment + origin backfill" composition is bounded by `MaxRanges` (default 16) — sorting work, temporary arrays, and multipart amplification all bounded.**Unsafe invalidation**: a successful POST/PUT/DELETE response invalidates the target URI, together with same-origin `Location`/`Content-Location` representations the response permits invalidating — read-after-write consistency is guaranteed by protocol, not by application reminders.

### The partition key

`PartitionKey` is copied at submission and used to isolate the same URI per user/site/tenant — the primary key includes the partition.**Why not rely on `Vary` alone**: `Vary` distinguishes by request headers (e.g., `Vary: Authorization`), but "which headers should distinguish" is declared by the responder — undeclared means shared; the partition key is **the caller's deliberate isolation** (the bottom line of multi-tenant clients). Isolation of private data is the caller's decision; the protocol tool is only a tool.

## Examples

### First complete program: cache assembly and capacity configuration

The program below is from `examples/http/client_cache` — Store mounting and caps:

```embed path="extlibs/xhttp/examples/http/client_cache/main.c" title="extlibs/xhttp/examples/http/client_cache/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_cache/main.c -lws2_32 -liphlpapi
（示例校验缓存装配与回源-命中路径后正常退出）
```

**What just happened.** (1) `xrtHttpCacheCreate(NULL)` builds the in-memory Store (`NULL` takes the default configuration — capacity and eviction live in `xhttpcacheconfig`); placed into `ClientConfig.Cache.Store` — the Client holds the reference from creation. (2) `Cache.MaxBody = 16 MiB` overrides the default 8 MiB — the **single-origin capture cap** is the anchor of capacity planning: it decides "how large a response is worth caching", in concert with the Store's total capacity (another configuration). (3) Request code afterward is identical to no-cache — hits stay off the network, stale entries validate automatically, `Info`'s Wire bytes of 0 means hit (the diagnostics face's way to spot a cache hit). (4) `HttpClientCache(pClient)` lends the same handle back (statistics, explicit eviction); without a Store configured, the layer never enters the hot path.

### Second complete program: the cache-policy tour

The second program is from `examples/http/cache_policy` — the protocol layer of policy parsing and verdicts:

```embed path="extlibs/xhttp/examples/http/cache_policy/main.c" title="extlibs/xhttp/examples/http/cache_policy/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/cache_policy/main.c -lws2_32 -liphlpapi
（输出策略解析与可存储性判定的自检结果）
```

**What just happened.** (1) Cache policy is an **independent protocol module** (`http_cache_policy`): request Cache-Control directives, response storability, freshness and age — the client cache layer is merely its consumer (the same pattern as Chapter 90's Body reusing Plan and Chapter 96's compression reusing the negotiation layer). (2) This sample validates policy verdicts offline: given request/response headers — storable? fresh for how long? needs validation? — **every step of caching behavior is a unit-testable pure function**, the engineering guarantee that "automatic behavior never runs wild". (3) The tour family also includes `cache_control` (directive parsing), `cache_time` (age computation), `cache_validate` (validator selection), `cache_range` (Range composition), `cache_store` (storage contract), `cache_status` (status-header generation) — six protocol modules hold up automatic caching, all independently trimmable.

## Contracts

- **Assembly**: Store into the Client configuration makes everything automatic (lookup/delivery/validation/storage/composition/invalidation); the Client holds the reference; unconfigured, the layer never enters the hot path.
- **Four modes**: DEFAULT/Disabled/Reload/Only (no record synthesizes 504); unified with request-header semantics by one parser, no approximations.
- **Primary key**: method + effective URI + partition key + the Vary fields the record actually declares.
- **Not stored**: no-store/non-cacheable statuses/Vary:*/authenticated-private forbidden to shared stores.
- **Heuristics**: default 10% of Last-Modified, one-day cap, disableable.
- **Caps**: MaxBody is the hard cap on a single origin capture (default 8 MiB), fail-open beyond; MaxRanges default 16 bounds composition complexity; grow on demand, zero reservation.
- **Conditional commits**: 304/HEAD/Range use versioned conditional Replace — on conflict, deliver your snapshot, keep the concurrent updater.
- **Invalidation**: unsafe success invalidates the target URI + same-origin Location/Content-Location the response permits.
- **Partitioning**: PartitionKey copied at submission; private isolation relies on partitions, not Vary.
- **Trimming**: `HTTP_CLIENT_CACHE` depends on the shared cache protocol family (policy/control/time/validate/range/store/status independent).

## Pitfalls

### Pitfall 1: reading MaxBody as the Store's total capacity

Symptom: setting `MaxBody = 16 MiB` and believing the cache occupies at most 16 MiB — after hours of running, memory climbs to hundreds of MiB.

Cause: `MaxBody` is the **single-origin capture** cap (how large a response still gets cached); the Store's total capacity and eviction live in `xhttpcacheconfig` (the configuration at Store creation). Two knobs, two jobs.

```c bad
ClientConfig.Cache.MaxBody = 16u * 1024u * 1024u;   /* assumed this is total capacity */
pCache = xrtHttpCacheCreate(NULL);                    /* default total capacity untouched */
```

```c good
xhttpcacheconfig CacheConfig;
xrtHttpCacheConfigInit(&CacheConfig);
CacheConfig.MaxBytes = 64u * 1024u * 1024u;   /* Store total capacity + eviction */
pCache = xrtHttpCacheCreate(&CacheConfig);
ClientConfig.Cache.Store = pCache;
ClientConfig.Cache.MaxBody = 16u * 1024u * 1024u;   /* single-capture cap */
```

### Pitfall 2: multi-user clients sharing an unpartitioned cache

Symptom: a private resource requested by user A gets a direct hit when user B requests the same URL — **cross-user data leak**.

Cause: the cache primary key has no user dimension by default; `Vary: Authorization` works only if the responder declares it — undeclared means shared. Multi-user/multi-tenant must supply partition keys.

```c bad
/* all users, one Client, one Store, no partitions */
pClient = xrtHttpClientCreate(pEngine, &Config);   /* B hits A's private response */
```

```c good
/* one partition key per user (or tenant) - the primary key includes the partition */
xrtHttpCallOptionsInit(&Options);
Options.Cache.PartitionKey = user_partition(user);
pCall = xrtHttpClientDo(pClient, pReq, &Options, on_done, pCtx);
/* private isolation is the caller's deliberate decision - the tool doesn't guess for you */
```

### Pitfall 3: handling ONLY-mode 504 as a network error

Symptom: in offline mode (`CACHE_ONLY`) with no record, 504 returns — the code walks the "gateway failure" alert path.

Cause: this 504 is **synthesized local semantics**: "network forbidden and no cache" — it is not a network error but the deterministic outcome of offline policy. Handling belongs in the "offline empty" branch (prompt/degraded UI), not retry or alerts.

```c bad
if ( status == 504 ) {
	alert_gateway_down();   /* an offline miss taken as a network fault */
}
```

```c good
if ( offline_mode && status == 504 ) {
	show_offline_empty(url);   /* locally synthesized 504: offline and uncached */
}
```

## Exercises

### Basic: hit vs origin side-by-side

Fire twice at the same URL on a local service: compare the second request's `Info.ResponseWireBytes` (should be 0 — hit) and the age information in the response headers. Then a third with `RELOAD`. Acceptance criteria: the three Wire byte counts match mode semantics; the hit body is byte-identical to the origin's first response.

### Advanced: observing conditional validation

The server returns a response with an ETag; request again after expiry (or forced RELOAD) — via packet capture or server logs, confirm the second request carries conditional headers, the response is 304, and the client delivers the **merged complete representation** (status 200, not 304). Acceptance criteria: the caller never sees the internal 304 (contract verified); the hit body matches the origin's.

### Challenge: an offline-first client

Implement `fetch_offline_fallback(Url)`: first `CACHE_ONLY` (use on hit); on 504, fall back to normal mode, go to network, and write the result to cache. Test the whole flow with the network cut. Acceptance criteria: with cache, works offline; without, an explicit offline error (distinguishing "disconnected" from "server 5xx"); cache writes respect the response's storability (no-store responses never enter — verifiable from later offline behavior).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Assembly | Store into the Client configuration makes everything automatic; the Client holds the reference; unconfigured, zero cost |
| Four modes | DEFAULT / Disabled / Reload (source refetch may still update) / Only (no record synthesizes 504) |
| Primary key | method + effective URI + partition + declared Vary; the partition is the caller's isolation tool |
| Not stored | no-store/non-cacheable statuses/Vary:*/authenticated-private forbidden to shared |
| Heuristics | Last-Modified 10% - one-day cap - disableable (conservative defaults) |
| Two cap knobs | MaxBody = single capture (fail-open beyond); Store config = total capacity + eviction |
| Conditional commits | 304/HEAD/Range use versioned conditional Replace; conflicts keep the concurrent updater |
| Range composition | local fragments + origin backfill; MaxRanges default 16 bounds complexity |
| Invalidation | unsafe success -> target URI + same-origin Location/Content-Location |
| Protocol family | policy/control/time/validate/range/store/status - independently trimmable, unit-testable |
