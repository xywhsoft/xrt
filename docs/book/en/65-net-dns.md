---
num: 65
slug: net-dns
title: The DNS Resolver
volume: 卷七 网络
type: practice
lead: The synchronous list form and engine-affine asynchronous form — callbacks execute on Workers, lock-free with subsequent connections on the same thread; caching and timeout control.
api: net
---

## Orientation

DNS turns domain names into address lists (Chapter 62 already met the synchronous `xrtNetResolve`). This chapter unfolds the **engine-affine asynchronous form**: the `xnetresolveop` operation object is submitted to the engine — **the callback executes on a Worker** (same thread as subsequent connection operations — naturally lock-free; that is the whole meaning of "engine affinity"); the result is still an address list (happy-eyeballs' input unchanged); timeout and cancellation are controlled through the operation object. Caching and resolution policy (hosts-first, TTL) are agreed at the module contract layer.

## Introduction

The trap of synchronous resolution lies outside the startup path: per-request resolution (direct-connection scenarios) blocks the request thread for milliseconds to seconds (slow DNS is a network service's most common tail-latency source); after resolution comes connection — joining the two asynchronous steps (initiating the connection inside the resolution callback) requires cross-thread data handoff under an ordinary thread-pool model (resolution on pool thread A, connection needing engine thread B — locks or queues).

The engine-affine answer: the resolution operation is **submitted to the engine** — the completion callback runs on a Worker, on the **same thread** as connections and send/receive — "resolve then connect directly" with zero handoff and zero locks. Chapter 60's skeleton "DNS offload slot" filled in correctly: not offloaded to a standalone thread pool, but submitted to the engine (Chapter 58's task-pool compute offload versus this chapter's in-engine resolution — the selection criterion is "where do the subsequent operations live").

## Concepts

### The division between synchronous and asynchronous

| Form | Entry | Fits |
| --- | --- | --- |
| Synchronous | `xrtNetResolve` | Startup config parsing, tool-style programs |
| Asynchronous (engine) | resolveop submitted to the engine | Per-request resolution, joining with service connections |

The asynchronous operation object `xnetresolveop`: submit (engine + domain + port + callback + data) → complete (callback on a Worker, `xrtNetResolveOpResult` fetches the list). **Callback discipline** follows the library-wide rule (light work — resolution results are usually consumed/passed on immediately).

### What engine affinity means

```diagram flow
- Submit: resolveop -> engine queue (DNS runs asynchronously on the system resolution path)
- Complete: the callback executes on an engine Worker thread
- Join: inside the callback, Connect directly (same thread — zero handoff, zero locks)
- Contrast: standalone thread-pool resolution -> result crosses threads to the engine -> locks or queues (handoff cost)
```

The essence of "affinity": **where subsequent operations execute decides where resolution executes** — to connect engine-managed connections, hand resolution to the same engine. This shares its logic with Chapter 55's "scheduler thread binding": data and operations on the same thread, and synchronization costs vanish.

### Timeout, cancellation, and caching

Asynchronous operations carry timeouts (the deadline parameter family — Chapter 41's conventions) and cancellation (the operation object is cancellable — the DNS dialect of Chapter 53's cancellation tree). Cache semantics live in the module contract: resolution results are decided by the system resolver path (hosts-first, system cache) — XRT does no application-layer DNS caching (Chapter 58's observation: upstream already caches; double caching is complexity, not benefit; if an application cache is truly needed, build one with a value tree + TTL — Chapter 31's cache host). The resolver_future example is the Future-shaped sibling — Chapter 57's delivery model slipped over resolution.

### The resolver example family's division

dns (synchronous list, Chapter 62), resolver (engine callback, this chapter's main example), resolver_future (Future shape), resolve_tour (full-interface tour) — four examples, four faces of one capability. Choose: tools use synchronous, services use the engine callback, composition (multi-domain concurrency + First) uses Future.

## Examples

### Complete program: engine-affine resolution

From the repository example `examples/network/resolver/main.c` — the callback gets the address list on a Worker:

```embed path="examples/network/resolver/main.c" title="examples/network/resolver/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/resolver/main.c -lws2_32 -liphlpapi
::1
127.0.0.1
```

**What just happened.** (1) The resolution operation was submitted to the engine (domain localhost + port + completion callback + state data) — the main thread does not block. (2) The completion **callback executes on a Worker**: `xrtNetResolveOpResult` fetches the address list and prints each entry — `::1` and `127.0.0.1` (order may swap — dual-stack list semantics identical to the synchronous version). (3) The callback's output uses printf directly — it runs in an ordinary thread context (not a restricted environment like a signal handler — the Chapter 48 contrast). (4) The main thread waits on an atomic counter (Done flag) to confirm completion — the simplest rendezvous; in service form, the callback Connects directly (zero handoff).

### Complete program: the Future shape

From `examples/network/resolver_future/main.c` — Chapter 57's delivery model slipped over resolution:

```embed path="examples/network/resolver_future/main.c" title="examples/network/resolver_future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/resolver_future/main.c -lws2_32 -liphlpapi
::1
127.0.0.1
```

**What just happened.** (1) The resolution submission returns a **Future** — the resolution result (address list) is delivered via the Future. (2) Wait/combinators/continuations (Chapter 57's full set) apply directly: multi-domain concurrent resolution + First (whichever domain resolves first wins), timeout racing (resolution versus timer) — the Future shape unlocks composition. (3) The resolver example's callback shape suits the straight-line flow of "resolve then connect directly"; the Future shape suits "resolution is one link in a larger orchestration" — choose by orchestration needs.

## Contracts

- **Engine affinity**: resolution callbacks run on Workers — same thread as subsequent connections, zero handoff and zero locks; if what follows lives on the engine, submit to the engine.
- **List semantics**: asynchronous results are address lists too — happy-eyeballs tries in order (Chapter 62's contract continued).
- **Light callbacks**: resolution callbacks consume/pass on results immediately — heavy processing moves to the next link.
- **Timeout and cancellation**: operation objects carry the deadline family and cancellation — Chapter 53's dialect.
- **Cache boundary**: XRT does no application-layer DNS cache; build your own if truly needed (value tree + TTL — Chapter 31's cache host).
- **Shape selection**: tools synchronous / services engine callback / orchestration Future — four examples, four faces.

### From examples to engineering: resolution's three hosts

**Startup configuration**: the service startup reads upstream domains from config — resolve synchronously once and store an address table (Chapter 31's value tree); synchronous on the startup path is acceptable (one-shot, timeout-protected). **Connection-pool warm-up**: at startup or periodically, asynchronously resolve upstream domains + probe connections — warm-up removes the first request's DNS tail latency; the challenge exercise is exactly this. **Per-request direct-connection scenarios** (proxy forwarding, crawler-class): every request's domain differs — resolution must be asynchronous and engine-affine (Pitfall 1's enforcement scenario); connect directly inside the resolution callback (the happy-eyeballs loop unfolds in the callback). The three hosts are chosen by "the relationship between resolution frequency and the request path" — one-shot config at startup synchronous, periodic warm-up engine-asynchronous, per-request engine-affine asynchronous.

### The full implementation slot for happy-eyeballs

Chapter 62 established the "try in order" semantics; this chapter supplies its full implementation slot as **asynchronous orchestration**: resolution completes (list in hand) → concurrently initiate the preferred connection (usually v6) + a timer (~250ms) → if the timer fires first, concurrently start the second entry (v4) → whichever succeeds cancels the rest. This is a direct application of Future combinators (Chapter 57's First + timeout racing) — the resolver_future shape exists for this. Does XRT's Connect entry accept an address list? No — a single address; the loop is unfolded by the caller (one Connect per engine) — and this unfolding is exactly the "networking strategy" layer (the xhttp extension library ships a complete implementation — Volume 10).

### The observation post: resolution latency and failure rate

Resolution is a network service's first hop, and observation (Chapter 49's pipeline) takes its first sentry post of Volume 7 here: **resolution duration** (submission-to-callback interval — the main tail-latency source); **failure rate and error classification** (NXDOMAIN versus timeout versus system error — different handling: a domain error is a configuration problem, a timeout a network problem); **list-size distribution** (the fallback headroom of multi-address domains). Three metrics into structured logs — DNS problems (the most common and most hidden service fault source) go from "feels slow" to visible curves. The resolve_tour example's statistical conventions demonstrate taking these three metrics.

### Dissolving a historical burden: the getaddrinfo thread-safety riddle

Traditional C network programming's DNS has a famous hidden trap: `getaddrinfo`'s cross-platform behavior is inconsistent (early glibc not thread-safe, some platforms needing special initialization) — occasional multi-threaded-service DNS crashes were classic bugs. XRT's resolution layer handles these platform differences internally (a wrapper over the system resolution path — invisible to users). The value of understanding this is isomorphic to Chapter 48's signal API: **the restricted-environment discipline is borne by the module, and business code keeps its normal shape**. Its limits should also be known: XRT ships no DNS protocol stack of its own (it goes through the system resolver) — scenarios needing DoH/custom resolution are implemented at the xhttp/host layer (the system resolver's policy stays transparent and controllable for enterprise networks).

### Clarifying the relation to Chapter 62

The division between this chapter and Chapter 62 blurs easily — both discuss "domain to address". Clarified: **Chapter 62 is the address-model chapter** — the xnetaddr type and operations are the volume's currency, with `xrtNetResolve` (synchronous list) appearing in passing as the convenient address-acquisition entry; **this chapter is the resolution-strategy chapter** — the asynchronous form, engine affinity, timeout/cancellation, cache boundary, happy-eyeballs orchestration. One chapter says "what an address is", the other "where addresses come from" — the contrast between the dns example (used in Chapter 62) and the resolver example (this chapter) is the contrast of the two chapters' viewpoints. Later chapters (TCP connections, proxy dialing) depend only on Chapter 62's address type — this chapter's resolution strategy is used at the service's connection-orchestration layer.

## Pitfalls

### Pitfall 1: synchronous resolution per request

Symptoms: high request tail latency — P99 stretched to hundreds of milliseconds; occasional slow DNS hangs whole requests; throughput jitters with DNS.

Cause: `xrtNetResolve` (synchronous) entered the request path — every request blocks on the system resolution (even with a system cache there is context switching and waiting).

```c bad
void handle_request(void)
{
	xnetaddrlist* List = xrtNetResolve(Host, Port, XNET_FAMILY_UNSPEC);  /* synchronous — request blocks */
	ConnectVia(List);
}
```

```c good
/* Resolve at startup / connection-pool warm-up, or wire the asynchronous form into the request path */
SubmitResolveOp(Engine, Host, Port, onResolved, Data);   /* engine-affine — connect directly in the callback */
```

### Pitfall 2: locking across threads to hand off resolution results

Symptoms: a lock between the resolution callback and the connection code — throughput jams on that lock; occasional deadlock (lock order conflicts with other engine locks).

Cause: resolution runs on a standalone thread pool (not engine-affine) — results must cross threads to the engine thread, and the handoff introduces a lock or queue.

```c bad
/* standalone-pool resolution */
xrtTaskSubmit(Pool, resolveAndHandoff, Host, NULL);
/* inside resolveAndHandoff: Lock(gAddrMutex); save list; Unlock; Post to wake the engine — handoff cost + lock */
```

```c good
/* engine-affine — the callback consumes directly on the Worker, and the handoff disappears */
SubmitResolveOp(Engine, Host, Port, onResolved, Data);
/* onResolved runs on the Worker: fetch the list, Connect directly — same thread, zero locks */
```

## Exercises

### Basic: dual-shape comparison

Resolve the same domain synchronously and via the engine callback — the result lists agree; print both versions' executing thread (CurrentId) to prove the callback runs on a Worker.

### Advanced: multi-domain concurrency

Resolve three domains concurrently with engine resolution (three operation objects) — after all complete, summarize each list's size; compare fastest/slowest durations (Chapter 41's timing).

### Challenge: a connection-pool warm-up line

Implement a service-startup warm-up line: N configured upstream domains resolved concurrently → probe-connect to each one's first address → report the usable upstream table. Acceptance: warm-up never blocks the main thread; partial domain failures don't affect the rest (failure list and cause chains into logs); an overall deadline bounds everything; the usable table goes into a value tree (Chapter 31) for later use.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Affinity principle | subsequent operations on the engine → submit resolution to the engine (callback same thread, zero locks) |
| Shape selection | tools synchronous / services engine callback / orchestration Future (four examples, four faces) |
| List semantics | asynchronous results are lists too — the happy-eyeballs contract continues |
| Timeout/cancellation | operation objects carry deadline and cancellation — Chapter 53's dialect |
| Cache boundary | XRT adds no application cache; build your own with a value tree + TTL if truly needed |
| Tail latency | per-request resolution must go asynchronous — synchronous is tail latency's first source |
