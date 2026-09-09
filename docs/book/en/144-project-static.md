---
num: 144
slug: project-static
title: Project 6 (The Book's Finale): A Comprehensive Static File Server
volume: 卷十三 实战项目 · 全书收官
type: project
lead: A TLS security entrance, directory mapping with traversal protection, ETag conditional requests and Range resume, compression negotiation, logging and graceful shutdown — the fusion of six projects' lessons and the final gathering of the book's knowledge map.
api: xhttp-http_server, xhttp-http_server_static, xhttp-http_server_middleware
---

## Orientation

The book's final chapter. Project 6 (the **static file server** webserv): serve a frontend build-artifacts directory over HTTPS — a secure entrance (TLS + certificates), static mapping (URL prefix ↔ disk directory + traversal protection), conditional requests (ETag/304), resumable transfer (Range/206), compression negotiation (gzip for compressible text), access logs and graceful shutdown. It is **the fusion of the six projects**: logstat's line processing (logs), configd's configuration (listen address and directory mapping read from config), xdl's Range semantics (this time served), pushd's long-connection lifecycle (HTTP keep-alive and TLS sessions) — plus the static layer (Chapters 104/131) and compression (Chapter 108) as specialist parts. It is also **the final gathering of the book's knowledge map**: from line 3 of code to this chapter, thirteen volumes and one hundred forty-four chapters of knowledge converge in one really deployable service. After acceptance comes the book's closing — what you learned, where to go next.

## Introduction

The requirement scene: a frontend team's build artifacts (HTML/CSS/JS/images, ~5000 files) need a production static service — HTTPS required (automatic certificate selection), concurrency (the download flood at release time), conditional requests (client cache negotiation — 304 saves 95% of traffic), resumable downloads (large-file interruptions recoverable), compression (gzip saves 70% on text), access logs (audit and analysis). nginx can do it — but what you want is **programmability**: custom caching policy, per-tenant routing, integration with internal systems — a general server's configuration DSL quickly becomes an untunable dialect; written with XRT, every behavior is transparent C logic. **This is not a continuation of a teaching exercise but a demonstration of an engineering decision**: when a generic part's configuration complexity exceeds a self-written code complexity, writing it yourself becomes the rational choice — XRT's layering keeps the cost of "writing a production part yourself" controllable (webserv's core in this chapter is under three hundred lines; everything else is library).

**Acceptance criteria**: (1) all file types served correctly (MIME/encoding); (2) `../` and encoded traversal all 404 (verified by a security scanner); (3) ETag negotiation: second requests 304 with zero body; (4) Range: single-span 206 + multi-span multipart; (5) gzip: text compressed, images as-is (an Accept-Encoding negotiation matrix); (6) 500 concurrent connections stable in the download flood (keep-alive reuse); (7) TLS: certificates auto-matched by SNI, modern cipher suites; (8) access logs in JSON Lines (Chapter 138's shape); (9) graceful shutdown (the full Drain flow); (10) stats zero leaks throughout.

## Concepts

### Architecture: the full layered picture

```diagram flow
- Security entrance: the TLS Server (certificates + SNI) - Chapters 85/86
- HTTP layer: Server + Router + middleware (logging) - Chapters 103/104
- Static layer: http_server_static - directory mapping/traversal protection/MIME/ETag/Range - Chapter 104
- Compression layer: reply_compress negotiation - Chapter 108
- Observation: logger JSON Lines + stats - Chapters 37/6
- Lifecycle: the five-timeout defaults + Drain - Chapter 103
```

Six layers, each an earlier chapter's main subject — **webserv invents no layer**; all its work is inter-layer assembly and business glue (the configuration of directory mapping, the compression policy's whitelist). This is the book's "layering" philosophy in its final exam: **good systems are assembled, not written**.

### The static semantics checklist

| Semantics | Implementation | Chapters |
| --- | --- | --- |
| Path mapping | URL prefix ↔ directory prefix; post-normalization prefix check (anti-traversal) | 104/43 |
| MIME | extension lookup | 104 |
| ETag | content fingerprint; If-None-Match → 304 | 104/108 |
| Range | single span 206 + Content-Range; multi-span multipart | 90/108 |
| Compression | Accept-Encoding q negotiation + type whitelist | 108/91 |
| Cache headers | Cache-Control by type (HTML short / assets long) | application layer |
| Directory | trailing slash → index.html; missing slash → redirect | 104 |

**The traversal-protection recheck** (the security core): a request for `/static/../secret.key` — the static layer's mapping performs the "still inside the root after normalization" check after splicing (Chapter 104 pitfall 3); **encoded traversal** (`%2e%22%2f` — percent-encoded `../`): URL decoding happens before routing (Chapter 89) — the decoded path passes the same check. **Double defense line**: never assume a single decoding entrance at the encoding layer (an attacker can build multi-layer encodings — revalidate after each layer decodes).

### Conditional requests and caching policy

ETag generation: a content fingerprint (a hash of the file — Chapter 73) or a tuple fingerprint (size + modification time — zero hashing cost). **Decision: tuple fingerprint** — if the static file is unchanged, the content is unchanged (the build-artifact immutability assumption); content hashing is reserved for high-security scenes (anti metadata spoofing). **Cache-Control policy**: `index.html` short cache (`max-age=60` — takes effect one minute after release); fingerprinted assets (`app.a3f9.js`) long cache (`max-age=31536000, immutable` — a changed filename invalidates the cache — the frontend build's fingerprint-naming convention). **304's traffic meaning**: a second visit (cached) costs zero body — CDN/browser cache hits consume no server bandwidth (acceptance 3 quantified: a 5000-file site, 50 MB on first visit, < 100 KB of header overhead on the second).

### The compression-negotiation policy matrix

Chapter 108's compression layer configured for webserv: **type whitelist** (text/css/js/json/svg compressed; jpg/png/woff2 not — already-compressed formats); **minimum length** (< 256B not compressed — overhead exceeds the gain); **negotiation** (Accept-Encoding's q values — Chapter 91's thousandths model). **The pre-compression upgrade path**: at build time generate `.gz` sidecar files (`app.js.gz`) — at serve time send the file with `Content-Encoding: gzip` directly (zero online compression cost — the CDN convention); this chapter's base version uses online compression (acceptable for small artifact volumes); pre-compression is an exercise direction.

### Concurrency and lifecycle

**The download flood's shape**: after release, N clients pull all assets simultaneously — multiple requests per connection (keep-alive reuse — the client-side counterpart of Chapter 98's connection pool). **Server configuration**: MaxConnections (the concurrency cap), WriteSize (the zero-copy lease — large files chunked), the five-timeout defaults (slow-attack protection — Chapter 103). **File sending's zero-copy path**: the static layer's `xrtHttpConnFile` family (Chapters 104/107) — file bodies never enter user-space buffers (Chapter 71's SendFile honored at the transport layer); the **memory watermark** is independent of file size (verified with stats — acceptance 10). **Graceful shutdown**: Drain (new connections refused, in-flight completed) → log flush → destroy — the zero-interruption window for release restarts.

## Examples

### First complete program: the minimal shape of a static service

The program below is from `extlibs/xhttp/examples/http/static_file` — the minimal loop of a static file response:

```embed path="extlibs/xhttp/examples/http/static_file/main.c" title="extlibs/xhttp/examples/http/static_file/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/static_file/main.c -lws2_32 -liphlpapi
（输出静态文件服务装配的自检结果）
```

**What just happened.** (1) The static layer's assembly face: path mapping, MIME, the file response's zero-copy path — **everything of webserv's static layer** is ready in the library. (2) Conditional requests and Range are negotiated inside the layer (the server-side mirror of If-None-Match/If-Range — xdl was the client side, here is the server side — **both ends of one protocol have now been walked**). (3) webserv adds on top: configured mapping (multiple directories), the compression middleware, the TLS entrance, logging — four assembly pieces make the complete service.

### Second complete program: the full router + middleware + static assembly

The second program is from `examples/http/server_static` — a runnable static-service skeleton:

```embed path="extlibs/xhttp/examples/http/server_static/main.c" title="extlibs/xhttp/examples/http/server_static/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/server_static/main.c -lws2_32 -liphlpapi
（启动静态服务后按 Enter Drain 退出）
```

**What just happened.** (1) The three-piece assembly Router + static endpoint + Engine — webserv's skeleton shape. (2) The Drain wrap-up at the sample's end (Enter triggers it — the minimal demonstration of graceful shutdown). (3) The middleware layer's hanging point (Chapter 104's Use) — the insertion point of a logging middleware. **webserv = this sample + TLS configuration + the compression layer + the directory-mapping array** — every increment is an earlier chapter's configuration face, not one new concept.

### Implementation walkthrough: the assembly main function's source

webserv's assembly main function — the full flow from configuration reading to service start (key segments walked over the teaching samples):

```c
int main(int argc, char** argv)
{
	wsvcfg Cfg;
	xnetengine* Eng = NULL;
	xhttpserver* Srv = NULL;
	xhttpserverrouter* Router = NULL;
	xtlsidentity* Ident = NULL;
	int Result = 1;

	/* 1 configuration: the configd shape (defaults -> environment override -> validation) */
	wsv_defaults(&Cfg);
	wsv_env_override(&Cfg);            /* Chapter 42 */
	if ( !wsv_validate(&Cfg, stderr) ) { return 2; }  /* intercepted at startup */

	/* 2 Engine and identity */
	Eng = xrtNetEngineCreate(NULL);
	if ( (Eng == NULL) || !xrtNetEngineStart(Eng) ) { goto Cleanup; }
	Ident = wsv_load_identity(&Cfg);  /* Chapter 82: startup-time verification */

	/* 3 routing: static mapping + the logging middleware */
	Router = xrtHttpServerRouterCreate(NULL);
	if ( !wsv_mount(Router, &Cfg) ||          /* the directory-mapping array */
		!xrtHttpServerUse(Router, wsv_log, &Cfg) ||  /* Chapter 104 */
		!xrtHttpServerRouterFreeze(Router) ) { goto Cleanup; }

	/* 4 service: TLS entrance + the five-timeout defaults */
	{
		xhttpservertlsconfig Tls;
		xrtHttpServerTlsConfigInit(&Tls);
		Tls.Handshake.Identity = Ident;
		Srv = xrtHttpServerRouterStartTls(
			Eng, &Cfg.Net, &Tls, Router, NULL);
	}
	if ( Srv == NULL ) { goto Cleanup; }
	wsv_print_endpoint(Srv);          /* the listen-endpoint log */
	Result = wsv_wait_shutdown();     /* signal/input wait */
	xrtHttpServerDrain(Srv);          /* graceful shutdown */
	wsv_log_flush(&Cfg);

Cleanup:
	xrtHttpServerDestroy(Srv);
	xrtHttpServerRouterDestroy(Router);
	xrtTlsIdentityRelease(Ident);
	xrtNetEngineDestroy(Eng);
	return Result;
}
```
```
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c webserv.c -lws2_32 -liphlpapi
webserv listening on https://127.0.0.1:8443 (root=dist)
```

**Walkthrough points.** (1) The four-step assembly (configuration → Engine/identity → routing → service), each failure goto Cleanup — **the single-exit discipline runs all the way into main**. (2) `wsv_mount` is the directory mapping's configuration (the Cfg.Mounts array loops hanging static endpoints — multiple directories/prefixes). (3) The logging middleware `wsv_log` sits outside the routes — one JSON line per request (time/method/path/status/bytes/duration — recorded after Close with complete state). (4) The TLS entrance's `StartSecure` (identity attached — SNI auto-matching multiple certificates). (5) The log flush after Drain — **in-flight requests during shutdown also reach the log** (the last complete batch of records). (6) All destruction in reverse dependency order — Chapter 103's Shutdown semantics cashed. The main function is sixty lines — **webserv's entire "business"**: the rest is library and wsv_* helpers (configuration/mapping/logging — each < 100 lines).

### The total reconciliation against the six projects' lessons

The seventeen engineering common-sense rules' **total examination sheet** in webserv (each rule's landing point): **acceptance first** (ten criteria including the security scan and the flood); **shape decides the container** (mapping arrays / the ETag tuple / log-line builders); **views are use-and-discard** (the logging middleware's request view finished inside the callback); **layered swappability** (swapping the storage backend or the compression policy is single-layer); **platform facts enter design** (system differences of path separators/permissions/certificate locations); **budget first** (throughput = min(disk, network) — CPU negligible on both sides); **four-quadrant testing** (the semantics matrix + the traversal range + the flood stress); **remote uncertainty** (clients may send anything — revalidate after every decoding layer); **protocol atomic points** (ETag negotiation's consistency guaranteed by protocol); **temp name + rename** (atomic switching of the newly released directory — a symlink flip); **filesystem state is authoritative** (the ETag comes from file metadata — no self-maintained cache); **test doubles** (deterministic clients for local stress); **design fan-out first** (the flood's keep-alive reuse budget); **final-state reconciliation** (connection counts reaching the final state of zero after Drain); **sequence self-healing** (the release version number lets clients detect switches); **test clients are also tested** (the stress framework's request reconciliation); **operations interface** (endpoint printing + logs as observation). **All seventeen have landing points** — the lesson checklist's completeness is the project's completeness.

### The knowledge map's final gathering

All chapters walked by webserv (and the six-project volume) — inventoried by volume: **Volume 1** (Chapters 3–8: the first program to trimming — the foundation of everything); **Volume 2** (Chapters 10–12: numbers and hashing — the fingerprint principle of ETags); **Volume 3** (Chapters 13–23: containers — the base of mapping tables and request queues); **Volume 4** (Chapters 25–35: text and data — the parsing chain of URL/headers/JSON logs); **Volume 5** (Chapters 36–49: system services — the daily face of logging/files/paths/directories/asynchronous IO); **Volume 6** (Chapters 50–60: concurrency — the correctness foundation of connection concurrency); **Volume 7** (Chapters 61–71: networking — the transport base of TCP/buffers/engines); **Volume 8** (Chapters 72–87: security — every layer of the TLS entrance); **Volume 9** (Chapters 88–96: the protocol core — HTTP/WS byte-level semantics); **Volume 10** (Chapters 97–108: xhttp — this service's direct toolbox); **Volume 11** (Chapters 109–128: extension libraries — the tool sources of pushd/xdl); **Volume 12** (Chapters 129–136: engineering practice — the discipline of deployment/testing/tuning); **Volume 13** (Chapters 137–144: projects — the spiral of lessons). **One hundred forty-four chapters, six projects, seventeen common-sense rules, about 650,000 words of exposition** — that is this tutorial's entire estate. What you take away should not be an API quick reference (the website has one) but **the layered eye and the disciplined hand** — they hold on any C codebase.

## Contracts

- **Traversal double defense**: post-splice normalization prefix check; multi-layer decoding revalidated per layer — the eight-form traversal range all green is the acceptance basis.
- **ETag tuple**: size + modification time (the build-immutability assumption); 304 with zero body.
- **Dual-grade caching**: HTML entrances short (max-age=60) / fingerprinted assets long (immutable) — the filename is the invalidation mechanism.
- **Compression whitelist**: compressible text families; already-compressed formats as-is; a minimum-length threshold.
- **Range server-side**: single span 206+Content-Range; multi-span multipart — complementing xdl's client into the complete protocol face.
- **Zero copy**: file bodies via the ConnFile family — memory independent of file size.
- **Concurrency caps**: MaxConnections + the five-timeout defaults — slow-attack and flood protection.
- **TLS**: certificate SNI matching; modern suites; the verifier stance (the server has no verifier but has identity).
- **Logs as JSON Lines**: one line per request (time/method/path/status/bytes/duration) — the file_json shape.
- **Shutdown**: Drain (refuse new, finish old) → log flush → destroy — the zero-interruption release window.

### Obtaining benchmark and capacity numbers

webserv's capacity conclusions must come from Chapter 135's method (not guesswork): **throughput benchmark** — flood simulation with a 500-client workload pulling the whole site measuring req/s and the bandwidth saturation point (disk or network fills first — the budget names the scaling direction); **latency distribution** — P50/P99 request latency (aggregated from the logs' duration field — Chapter 41 timestamps' product); **comparison anchor** — under the same load, the gap versus nginx (a mature sendfile/epoll implementation) measured (gap-source analysis: library-call overhead / no kernel tuning / no multi-process — a gap under 2× is acceptable in most scenes, because programmability is the main gain); **sweep** — a two-dimensional scan of connection count × file size (small-file high-concurrency versus large-file low-concurrency, each with its own saturation point). **The capacity conclusion's shape**: "single instance 4C8G: small-file scene X 10k req/s (P99 < Y ms), large-file Z GB/s — beyond that, add instances or front a CDN". These numbers go into the deployment document — **the benchmark's output is operations' input**.

### The release process and versioning strategy

webserv's own release (meta-engineering — a project is software too): **atomic directory switching** — a new version builds into `releases/v{ts}/`, the `current` symlink flips to point at it (an atomic directory-level switch — Chapter 142's "temp name + rename", directory edition: at any moment current points at one complete version); **rollback** — the symlink points back at the old directory (seconds — two orders of magnitude faster than a restart rollback); **health check** — the `/health` endpoint (artifact validation: index.html existence) — the release script waits for health before switching traffic; **log continuity** — logs roll daily across restarts (Chapter 38 rotation) + a version field (which version handled the request — the anchor for debugging timing issues). The release automation script is itself configd's idea (a state machine of configuration/version/health) — **the project sequence's knowledge closes at the last ring of production operations**.

## Pitfalls

### Pitfall 1: hand-spliced directory mapping missing encoded traversal

Symptom: the security scanner reports `/static/%2e%2e%2fsecret` reachable — the direct `../` is blocked but the encoded form slipped through.

Cause: the URL decodes once before routing (Chapter 89) — whether decoding happens **before** or **after** your check decides the defense line's position. The static layer's mapping check runs on the decoded path (correct); a hand-spliced mapping validating before decoding — encoded traversal.

```c bad
snprintf(Path, "%s/%s", Root, RawUrlPath);  /* splicing the raw undecoded path */
check_prefix(Path);                           /* what is checked is the encoded form */
```
```c good
/* use the static layer mapping: post-decode normalization check built in */
/* self-managed: decode first (Chapter 101) -> normalize -> check prefix - recheck after every decoding layer */
```

### Pitfall 2: a 304 response carrying a body

Symptom: packet capture shows 304 responses carrying full bodies — the cache optimization entirely defeated (bandwidth consumed all the same).

Cause: after a conditional-request hit, hand-assembling the response forgot that 304 semantics are **headers only**. The static layer's 304 path is built in; when hand-assembling, the 304 branch must go through "headers-only" assembly.

```c bad
if ( etag_match ) { reply(304, body); }   /* 304 with a body: protocol violation */
```
```c good
if ( etag_match ) {
	respond_headers_only(304, ETag);   /* headers only + ETag */
	return;
}
```

### Pitfall 3: setting every file to long cache

Symptom: a new version ships — the user's HTML references new assets but the HTML itself is cached — a blank page or old-new mixing.

Cause: the cache policy did not distinguish **entrance documents** from **fingerprinted assets**. HTML is the entrance (its references change when content changes) and must be short-cached; assets carry fingerprints in their names (a change is a new URL) and only then dare immutable.

```c bad
set_header("Cache-Control", "max-age=31536000");  /* all files: every release is an incident */
```
```c good
if ( is_html ) { cc("max-age=60"); }
else { cc("max-age=31536000, immutable"); }  /* fingerprinted assets */
```

### The semantics matrix's design intent

The sixteen-group semantics matrix exercise's intent: type (HTML/JS/JPG) × condition (hit/miss) × Range (none/single span) × compression (want/not) = 3×2×2×2 = 24 groups (sixteen representative combinations taken). The matrix's value lies not in passing but **in the process of passing** — you must write the expectation for each group first (protocol by hand) then verify — any mismatch is a blind spot in protocol understanding. Real static servers' bugs almost all sit at these combinations' intersections (304+compression leaving a stale Content-Encoding header, Range+compression in the wrong order, JPG+compression double-compressing wastefully) — **the matrix is a systematic cover of the combination space**, the response-side equivalent of Chapter 132's mutation testing.

## Exercises

### Basic: the full semantics matrix

Run the semantics matrix against the static service: the sixteen combinations of type × condition × Range × compression (at least one request each). Acceptance criteria: the sixteen responses (status/headers/body presence) match the hand-derived protocol.

### Advanced: the traversal-attack range

Construct eight traversals (direct / one-layer encoding / double-layer / trailing dots / backslash / null byte / overlong / symlink) tested against both a hand-spliced mapping and the static layer. Acceptance criteria: the static layer all 404; the hand-spliced version's holes each located and fixed.

### Challenge: the complete webserv

All six projects fused: configuration-driven (configd shape), TLS entrance, compression, logging, Drain, load testing (the 500-connection flood — Chapter 135's method). Acceptance criteria: all ten criteria pass; core hand-written code < 400 lines (the rest library) — the quantified proof of "assembly, not writing".

### The dual checklists: performance and security

The last two pages of checklists before launch. **Performance checklist**: (1) zero-copy path confirmed (large-file memory watermark = a constant in stats); (2) compression whitelist verified (images not compressed, text compressed); (3) keep-alive reuse rate (the logs' connection field — reuse < 50%: check clients or timeout configuration); (4) ETag hit rate (the 304 share — below 80%: check the cache-header policy); (5) five timeouts explicit (slow-attack protection is not default-value luck). **Security checklist**: (1) traversal range all green (eight forms 404); (2) TLS suite policy (Chapter 84's policy object — old suites off); (3) directory permissions minimized (the service account read-only on the static root); (4) log scrubbing (token fields in query strings — Chapter 76 discipline); (5) error pages leak no paths (generic 404 copy — server path information is reconnaissance material). Five items each — **neither performance nor security is a feature but the execution level of a checklist**.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Positioning | the six projects fused + the book's knowledge gathered — assembly, not writing |
| Static semantics | mapping/MIME/ETag/Range/compression/cache headers — Chapter 104 owns them |
| Traversal double defense | post-decode normalization prefix check; multi-layer decoding revalidated per layer |
| ETag | tuple fingerprint (size + mtime); 304 headers-only, zero body |
| Dual-grade caching | HTML short / fingerprinted assets immutable — the filename is invalidation |
| Compression | whitelist + minimum length + q negotiation; pre-compression the upgrade path |
| Range, both ends | this server side + xdl's client — the complete protocol face |
| Zero copy | the ConnFile family — memory independent of file size |
| Flood | MaxConnections + five timeouts + keep-alive reuse |
| Finale | ten acceptance criteria + the engineering-decision demo of a programmable server + the knowledge map gathered |

### The last lesson: from reader to author

The tutorial's final advice concerns **the role change**. Having finished the book, you can now **teach** this layering and discipline outward — and teaching is the ultimate test of mastery. Three paths: **in your team** — take a real module and write a chapter in this book's format (the eight-section skeleton, acceptance first, bad/good contrasts) — writing exposes what you thought you understood but cannot articulate; **in open source** — contribute an extension or improvement to XRT (Chapter 129's extension-library list is the entry point — hang your module on a manifest); **in projects** — paste the seventeen-rule checklist into the team wiki (each rule annotated with a real incident — your incident library is your team's textbook). Tutorials age (APIs change, platforms swap) but **the layered eye and the disciplined hand** do not — passing them on is this book's best closing.

### The book's closing

One hundred forty-four chapters walked; webserv is the work you can show. Look back at the path's shape: **Volume 1**'s foundation (errors/memory/trimming — the vocabulary of everything); **Volumes 2–3**'s determinism and containers (the engineer's building blocks); **Volume 4**'s text and data (the world of information); **Volume 5**'s system services (living with the operating system); **Volume 6**'s concurrency (correctness in the time dimension); **Volume 7**'s networking (the extension into space); **Volume 8**'s security (establishing trust); **Volumes 9–10**'s protocol stack (the complete rebuilding of the Web world); **Volume 11**'s extension libraries (the ecosystem's outreach); **Volume 12**'s engineering practice (the discipline of turning code into product); **Volume 13**'s six projects (constraint combination and the spiral deepening of lessons). Three threads throughout: **layering** (one layer per chapter, projects assemble), **discipline** (acceptance first / views use-and-discard / single failure exit / budget first — the seventeen rules), **verifiability** (gates / four-quadrant tests / final-state reconciliation — every claim machine-proven at the quality gate). Where next? Three directions: **deeper** — every chapter here is a door (TLS handshake details, io_uring's kernel machinery — doors behind doors); **wider** — build your own parts with XRT (new protocols/new containers/new tools — layering gives you insertion points); **teach** — bring this layering and discipline to your team (an architecture you can explain is an architecture you own). Whichever direction — **the code waits for you on GitHub — the door is always open**.
