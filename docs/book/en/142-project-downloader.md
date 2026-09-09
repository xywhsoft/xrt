---
num: 142
slug: project-downloader
title: Project 4: An HTTPS Downloader
volume: 卷十三 实战项目
type: project
lead: From a one-line GET to a deliverable downloader: resumable transfers (Range + ETag checks), streaming writes (memory decoupled from file size), atomic completion (temp file + rename) — xhttp and the file layers applied together.
api: xhttp-http_client, xhttp-http_client_easy, file
---

## Orientation

Volume 13's Project 4: an **HTTPS downloader** — `xd <url> [-o 输出] [-c 续传]` (output, resume): fetch an HTTPS resource to disk, support resumable downloads (continue from the last breakpoint when the server supports Range), stream to disk (a 10 GB file at MB-level memory), complete atomically (temp file + rename — a mid-flight failure leaves no half file). It is xhttp's client family (Chapters 97–102) applied together with the file layers (Chapters 44–47): starting from the **easy layer** (the power of a one-line GET), **response limits and streaming consumption** (Chapter 98's ResponseBodyLimit and the Body callback), the **Range resume protocol** (Chapter 90's HTTP-framing Range semantics + Chapter 100's cache_range composition layer applied locally), **atomic writes** (Chapter 45's WriteAtomic — the primitive of the `file/whole` sample), **errors and retry** (Chapter 99: judging retryable classes of connection failures). Projects 1/2's (Chapters 137/139) seven lessons replay a third time — the network shape adds the dimension of **remote-resource uncertainty** (the server may not support resume, the ETag may change, the connection may break at 90%).

## Introduction

The requirement scene: CI must pull a 2 GB model file — the network is unstable (one drop per three tries on average), the server supports Range, and a drop must continue from the breakpoint, not start over. Naive approaches (browser/curl) work interactively but **embedding into automation** means parsing output; hand-writing "open socket + send GET" shoulders all of TLS and HTTP. xdl makes it a composable command-line part: **idempotent resume** (rerunning the same URL continues from the breakpoint automatically), **integrity checking** (resume only when ETag/Last-Modified are unchanged — otherwise redownload: the resource has been swapped, and resuming would splice a corrupt file), **atomic delivery** (during the download it is a `.part` temp file; renamed only on completion — downstream never sees half a file).

**Acceptance criteria** (settled before code): (1) a complete download round-trips correctly (the hash reconciled against the server's Content-Length); (2) 10 GB streaming — MB-level memory (verified with stats); (3) resumable: after an interruption, rerun continues from the breakpoint (exactly one Range request on the wire — verified by capture); (4) an ETag change abandons the resume and redownloads whole (never splicing a corrupt file); (5) atomicity: kill at any moment, the disk holds only a `.part` or the complete file; (6) graceful degradation on servers without Range (whole redownload + an explicit notice); (7) every failure path a structured error (Chapter 4 — "cannot connect" distinguishable from "certificate error").

## Concepts

### Architecture: the download state machine

```diagram state
idle -> probing: start (URL -> HEAD or the first GET)
probing -> fresh download: no .part, or the ETag changed
probing -> resume: .part exists and the ETag matches (Range: bytes=N-)
fresh download -> downloading: create .part (truncate)
resume -> downloading: open .part in append mode
downloading -> done: last byte arrives (Content-Length reconciled) -> rename to deliver
downloading -> interrupted: connection drop / cancel -> .part kept (resume next time)
```

**The probe's two strategies**: HEAD first (fetch ETag/Content-Length/Accept-Ranges — one lightweight interaction) or a direct GET with `If-Range` (a matching ETag yields 206 resume, a mismatch yields 200 fresh — one interaction decides and starts the transfer). **Decision: If-Range** — saves a round trip and the semantics are protocol-guaranteed (a server-side atomic decision); the HEAD strategy is left as an extension (show information first, then decide whether to download).

### Module selection and rationale

| Step | Choice | Rationale and alternatives |
| --- | --- | --- |
| HTTP client | the xhttp client (Chapter 98's builder) | Range headers and response-limit control needed — the easy layer (Chapter 97) takes no custom headers. Alternative easy: one line but not customizable — rejected |
| TLS | client built-in (verifier mandatory) | Chapter 84's "not verifying is not an option"; the system trust store (Chapter 81's dial form) |
| Response consumption | the Body streaming callback | memory decoupled from file size (acceptance 2). Alternative "read whole then write": 2 GB of memory — rejected |
| Writing to disk | `.part` + append writes | the natural form of resume; rename on completion (atomic delivery) |
| Atomic completion | platform rename | same-directory rename is atomic (guaranteed on POSIX/Windows); cross-directory is not — `.part` sits in the target's directory |
| Resume-point record | the `.part` file size itself | the file size is the breakpoint — no separate state file (one fewer desync source). The ETag lives in the `.part.etag` sidecar |
| Progress display | Content-Length percentage | bytes received / total bytes; with no Content-Length, show bytes received |

### The If-Range resume protocol

```diagram flow
- first run: GET (no If-Range) -> 200 full body -> create .part, write from 0
- rerun (.part exists): GET + If-Range: <ETag> + Range: bytes=<part size>-
  -> 206: ETag matches - resume (write from the part size, append mode)
  -> 200: ETag changed - fresh (truncate .part, write from 0)
  -> 416: part already >= the file - abnormal state (part corrupt? redownload)
- server without Range support (200 with no Content-Range): fresh whole download (degradation notice)
```

**If-Range's semantics** (RFC 9110): the value is an ETag or a date — a match yields 206, a mismatch yields a complete 200 — **atomic on the server**: there is no "resource changed between validation and transfer" window (the HEAD-then-GET strategy has exactly that race). This is the resume correctness's protocol foundation. **416 handling**: `.part` larger than the file — only corruption or a server rollback causes it; the safe action is a fresh download (delete .part).

### Response consumption and limits

Chapter 98's two client consumption forms: the **Body callback** (streaming — each byte segment written on arrival) and **result buffering** (read whole). The downloader picks the callback: `ResponseBodyLimit` set as an anti-runaway ceiling (say Content-Length + 1KB — receive what was declared, anything more is an error); inside the callback, **sequential appends** to `.part` (a single writer, no concurrency — Chapter 44's Full write guarantees exactly N bytes). **Progress** accumulates in the callback — a percentage when Content-Length is known, a byte count otherwise. **Backpressure automatic**: slow disk → the callback returns slowly → the client stops reading → TCP shrinks (Chapter 103's mechanism, network edition) — the constant-memory guarantee.

### Atomic delivery and failure cleanup

`.part` and the target file in the **same directory** (`目标名.part`) — same-directory `rename` is atomic (a close cousin of Chapter 45's atomic-write family: WriteAtomic is the in-file "temp + rename" version; this is the cross-filename version). **The failure-cleanup policy**: connection interrupted — `.part` **kept** (the resource of the next resume); parameter/certificate errors — kept (no impact on retry); user Ctrl-C — kept (standard downloader behavior: interrupted means resumable). **Only an explicit "resource changed" (ETag change) truncates into a redownload**. Cleanup exception: `--force` explicitly demands from scratch (delete .part, start fresh).

### The resume state's authority and the race windows

The resume design's subtlest part is **which state is authoritative**: the `.part` size is the breakpoint, `.part.etag` is the resource identity — the pair talks to the server. Race-window analysis: **window one** (another process downloading the same URL concurrently) — two writers appending to one `.part` interleave into corruption; the countermeasure is a **lock file** (`.part.lock` present → refuse to start — process-level mutual exclusion; simple but sufficient; distributed scenarios exceed the tool's scope). **Window two** (the ETag sidecar and .part out of sync — killed after writing the etag, before writing the part) — the next If-Range carries the old ETag while the resource is unchanged: 206 resumes from the part's actual size — **self-healing by nature** (the part size is authoritative, the etag only a decision input — an over-conservative verdict merely redownloads a segment, never corrupts). **Window three** (filesystem crash recovery — both part and etag possibly half-written) — ext4/NTFS journaling keeps metadata consistent; half-written data keeps appending after the If-Range verdict passes — the tail may be corrupt, but **the hash reconciliation before final delivery** (checked when the server provides Content-MD5/ETag) is the last gate. The three windows' conclusion: **the lock file blocks true concurrency, the part size is the authority, the hash is the final review** — each layer relies on no other layer's perfection.

### Against Projects 1/2's lessons

The seven lessons replay a third time, two upgraded: **acceptance first** (seven items — the network's "remote uncertainty" puts ④⑤⑥, three failure-moment items, into acceptance); **shape decides the container** (download state is "file offset + ETag" — the `.part` size is the state, zero in-memory state); **views are use-and-discard** (the Body callback's segments written on arrival — no buffering); **layered swappability** (swapping the HTTP client implementation or the storage (write-to-memory/network export) is single-layer); **platform facts enter design** (same-directory rename atomic — cross-directory not guaranteed); **budget first** (throughput = the smaller of network and disk — CPU is negligible on both sides); **four-quadrant testing** (plus "server behavior simulation": normal / no Range support / ETag change / interruption at N% — four server shapes played by a local mock server (Chapter 103)).

## Examples

### First complete program: the HTTPS-fetch skeleton

The program below is from `examples/http/client_easy` — the downloader's HTTP face in a minimal loop (the tutorial Chapter 97 complete form):

```embed path="extlibs/xhttp/examples/http/client_easy/main.c" title="extlibs/xhttp/examples/http/client_easy/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_easy/main.c -lws2_32 -liphlpapi
usage: client_easy <http-url>
```

**What just happened.** (1) The three-step assembly (Engine→Client→GetSync) + the result trio (Response/Status/Body) — **the downloader's entire HTTP vocabulary** is already here. (2) xdl adds four things on top: Range/If-Range headers (swap to the builder path), Body-callback streaming (swap the consumption form), `.part` writes (add the file layer), rename delivery (add atomic completion) — **clear increments are layering's value**: Chapter 97's "above easy lies the whole runtime" cashes here as "above the runtime lies only download semantics". (3) A verifier is mandatory (the HTTPS form) — Chapter 84's discipline.

### Second complete program: the atomic-write primitive

The second program is from `examples/file/whole` — the file-layer primitive of atomic delivery:

```embed path="examples/file/whole/main.c" title="examples/file/whole/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/file/whole/main.c -lws2_32 -liphlpapi
published
```

**What just happened.** (1) `WriteAtomic`'s semantics (header comment): write a temp file, flush, rename atomically — **a mid-flight failure leaves no half file** (readers see either the old file or the new file). (2) xdl's completion action is isomorphic: `.part` fills → renamed to the target — **the download edition of atomic publication**. (3) Contrast sequential writes (writing the target directly): a kill at 90% shows downstream a 90% file — believed complete (the worst form: a partial model file silently producing wrong inference results). Atomicity is not fastidiousness but **downstream correctness**.

### Mapping onto the book's knowledge map

Every brick the downloader steps on has a main entry in the book: Chapter 4 (the three error classes driving actions — connection/certificate/resource), Chapter 44 (file open modes and Full writes — the exactly-N-bytes semantics), Chapter 45 (the atomic-write family — WriteAtomic's temp+rename philosophy crossing to filename level), Chapter 62 (addresses and ports — the URL's host resolution digested by the dial layer), Chapter 81 (TLS client dialing — verifier assembly), Chapter 84 (verification policy — the system trust store's default form), Chapter 90 (HTTP frame handling — Range's protocol semantics and the 206/200 verdict), Chapter 97 (the easy layer — this project's starting form), Chapter 98 (the runtime — the builder path's Range header, ResponseBodyLimit, the Body callback, the Info diagnostic fields (the basis of WireBytes/BodyBytes reconciliation)), Chapter 99 (automatic behaviors — retry classes in three), Chapter 100 (cache_range — the Range composition layer applied locally (fragment merging is its day job)), Chapter 103 (the server — a mock server playing four server behaviors). **Thirteen chapters each in place inside one downloader** — the same knowledge-map validation as Project 1 (fourteen chapters): a project's depth lies not in chapter count but in **the reality of inter-layer combination** — the downloader walks the boundaries of all three families "client HTTP × files × error model".

### Implementation walkthrough: the resume core's source shape

The download state machine's core — probing and the resume verdict — in source form (the complete program builds on the two teaching samples; this section walks the key segments):

```c
/* probe + start: If-Range's atomic decision */
static bool dl_begin(dl_ctx* Ctx, const char* sUrl)
{
	xhttprequest* Req;
	char Range[64];
	size_t iHave = dl_part_size(Ctx);   /* the .part size is the resume point */

	Req = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"), view(sUrl));
	if ( Req == NULL ) { return false; }
	if ( (iHave > 0u) && Ctx->Etag.Size > 0u ) {
		/* have a resume point and an ETag: If-Range lets the server decide atomically */
		xrtHttpRequestSetHeader(Req,
			XRT_STR_LITERAL("If-Range"), Ctx->Etag);
		snprintf(Range, sizeof(Range),
			"bytes=%zu-", iHave);
		xrtHttpRequestSetHeader(Req,
			XRT_STR_LITERAL("Range"), view(Range));
	}
	Ctx->Call = xrtHttpClientDo(Ctx->Client, Req,
		&Ctx->Options, dl_on_done, Ctx);
	xrtHttpRequestDestroy(Req);            /* after the freeze the original request may be destroyed */
	return Ctx->Call != NULL;
}

/* completion callback: the 206/200 fork */
static void dl_on_done(xhttpcall* Call, xhttpcallresult* Res,
	ptr Ctx)
{
	dl_ctx* C = Ctx;
	if ( Res->Result != XHTTP_CLIENT_ERROR_NONE ) {
		dl_fail_keep_part(C);   /* connection-class failure: keep .part */
		return;
	}
	if ( Res->Response->Status == 206 ) {
		dl_open_part(C, "ab");  /* resume: append */
	} else if ( Res->Response->Status == 200 ) {
		dl_open_part(C, "wb");  /* fresh or ETag changed: truncate */
	} else if ( Res->Response->Status == 416 ) {
		dl_discard_part(C);     /* abnormal state: delete part, redownload */
		dl_restart(C);
		return;
	} else {
		dl_fail_http(C, Res->Response->Status);
		return;
	}
	/* record the new ETag (the next resume's basis) + start streaming */
	dl_save_etag(C, Res->Response);
	dl_stream_body(C);          /* the Body callback writes as bytes arrive */
}
```

**Walkthrough points.** (1) `dl_begin`'s request assembly: If-Range/Range only when a breakpoint exists (a first download is a plain GET — the server has nothing to judge and needs nothing). (2) `HttpRequestDestroy` right after `Do` — the freeze semantics (Chapter 98) decouple the request's lifecycle from the call. (3) The completion callback's four-way fork is exactly the state machine's transition table: 206 resume / 200 fresh / 416 discard / other error — **one action per path**, nothing "incidental". (4) The distinction between `dl_fail_keep_part` and `dl_discard_part` is the contract "failures keep"'s two faces: connection-class keeps (the resume resource), state-abnormal discards (preventing an eternal-416 dead loop). (5) Body streaming and progress live in `dl_stream_body` (append-write + accumulate in the callback — Chapter 98's consumption form).

### Acceptance walkthrough and the test matrix

The seven criteria's test-matrix design (Chapter 132's four quadrants, downloader edition): **positive** — a complete download with hash reconciliation (acceptance 1); **boundary** — a streaming response without Content-Length (progress degrades to a byte count), an empty file (0 bytes, completes instantly), a redirect to HTTPS (Chapter 99's follow + unchanged verification); **negative** — 404/403 (user-error reports), certificate failure (the mock using the wrong certificate — configuration-error class), a 200 without Range support (the degradation path — acceptance 6); **race/interruption** — kill-and-rerun resume at 25%/50%/75% (acceptance 3), disk holding only .part right after a kill (acceptance 5), redownload on ETag change (acceptance 4 — the mock switching resource versions); **resources** — OOM point by point (Chapter 133: an allocation failure on the download path keeps .part uncorrupted). **The mock server's behaviors configured** (normal / no Range / ETag change / cut at a designated byte — Chapter 103's server as test double) makes the whole matrix run offline in CI — **a network project's test independence rests on service doubles, not the real network**.

## Contracts

- **If-Range semantics**: ETag match → 206 resume / mismatch → 200 fresh — atomic on the server, no validation race.
- **The breakpoint is the file size**: the `.part` size is the sole resume state — no separate state file (zero desync sources).
- **The ETag sidecar**: `.part.etag` stores the last ETag — builds If-Range on rerun; if the resource changed, truncate and redownload.
- **416 handling**: part ≥ file — abnormal state; delete part, redownload.
- **Same-directory rename**: the completion action is atomic; cross-directory rename is not — `.part` sits in the target's directory.
- **Streaming guarantee**: the Body callback writes on arrival; ResponseBodyLimit = declared length + tolerance — memory decoupled from file size.
- **Failures keep .part**: interruption/network errors keep it (the resume resource); only an ETag change or --force truncates.
- **Degradation path**: no Range support (200 without Content-Range) — whole download + notice.
- **Verifier mandatory**: the HTTPS form's default assembly; no `--insecure` switch (Chapter 84's "not verifying is not an option" stance).
- **Errors distinguishable**: connection failure (retryable) / certificate failure (configuration problem) / 416 (state abnormal) — classes drive actions.

### Delivery form and the CLI contract

The downloader's CLI contract and delivery choice: **the parameter face** (`xdl <url> [-o 输出] [--force] [--timeout 微秒]` (output, microseconds) — four parameters covering every state-machine entrance: URL required, -o sets the target name (defaulting to the URL's tail), --force ignores .part and starts fresh, --timeout overrides the default budget); **the exit-code contract** (0 success / 1 retryable failure (connection class — scripts can back off and rerun) / 2 non-retryable (certificate/parameters) / 3 resource error (4xx) — **the exit code is the script's API**: CI retry logic branches on it, the same synchronous lineage as Chapter 137's "the tool's acceptance language"); **the output contract** (progress to stderr (silenceable), a final line "saved <path> <bytes> <elapsed>" to stdout (parseable) — the human/machine split). **Delivery form**: the single-header form (a standalone tool's easiest distribution — Chapter 131's consumer choice); the trimming declares the xhttp client + file closure (no server/TLS-server symbols — size-profile verified). **Against curl/wget**: xdl does not aim to replace general downloaders — it is **a composable part embedded in the XRT ecosystem** (directly reusing this book's every lesson), teaching value and practical value in one.

### Retrospective: Project 4's transferable lessons

The downloader's new transferable lessons (atop Projects 1/2's seven): **remote uncertainty is a design dimension** — a local tool's failure is an anomaly, a network tool's failure is the norm (three classes, three actions + backoff + kept state make "failure" a composable intermediate state, not a termination); **protocol-given atomicity beats home-made** (If-Range's server-side atomicity versus HEAD+GET's self-made window — find the protocol's atomic point and use it); **the temp-name + rename delivery pattern** (applicable to every "product of a long write" — downloads / serialization of big objects / report generation — eliminating "in progress" from the observer's worldview); **filesystem state over separate state files** (the .part size is the breakpoint — one fewer desync source; the ETag sidecar is auxiliary, not authoritative); **test doubles make network projects offline-testable** (mock behaviors configured — CI never touches the real network). Five added to the original seven — **twelve engineering common-sense rules** continue replaying and deepening in Project 5 (the WebSocket service) and Project 6 (the static-server finale).

## Pitfalls

### Pitfall 1: resuming without checking the ETag (blind Range)

Symptom: the server's resource updated (a version shipped) — the resume splices **the old file's back half** onto **the new file's front half**: the artifact's hash is wrong, model inference silently errs — the worst corruption form (it looks like a complete file).

Cause: a Range request only says "give me bytes from N" — it does not promise the resource equals last time's. **If-Range is the protocol's atomic solution**: not using it opens your own race-condition window (between HEAD validation and GET transfer the resource can still change).

```c bad
/* blind resume: never knows the resource changed */
snprintf(hdr, "Range: bytes=%zu-", part_size());
get(url, hdr);   /* resumes on any 206 - the result is an old-new hybrid */
```
```c good
/* If-Range: the server decides atomically */
set_header("If-Range", saved_etag);
set_header("Range", "bytes=%zu-", part_size());
if ( status == 206 ) { 续传追加(); }
else if ( status == 200 ) { 截断全新(); }   /* ETag changed - redownload */
```

### Pitfall 2: writing the target file during the download

Symptom: the process is killed — downstream sees a 90% "target file": the script believes the download finished and moves on — silent data corruption.

Cause: writing the target exposes "in progress" to observers. The atomic-delivery form: **write a temp name (.part), rename on completion** — in the observer's worldview there is only "absent" and "complete".

```c bad
f = fopen("model.bin", "w");   /* writing the target directly */
write_chunks(f);                /* killed at 90%: a half file impersonates completion */
```
```c good
f = fopen("model.bin.part", append_mode);
write_chunks(f);                /* kill: .part remains - resumable */
rename("model.bin.part", "model.bin");  /* complete: atomic delivery */
```

### Pitfall 3: treating "cannot connect" as "download failed", never retrying

Symptom: one network hiccup — the download errors out; the CI job fails and reruns everything from scratch (a large-file disaster).

Cause: error classes not driving actions (Chapter 4). **Connection-class failures (TIMEOUT/AGAIN) are retryable** (exponential backoff); **certificate/parameter failures must not be retried** (ten thousand retries change nothing); **HTTP 4xx** (404/403) is a resource problem (report to the user). Three classes, three actions — Chapter 99's retry guardrail, downloader edition.

```c bad
if ( !download(url) ) { exit(1); }   /* always fail: a hiccup = start over */
```
```c good
for ( attempt = 0; attempt < 3; attempt++ ) {
	if ( download(url) ) { break; }
	if ( !error_is(xrtGetError(), XERR_TIMEOUT) &&
		!error_is(xrtGetError(), XERR_AGAIN) ) { break; }
	backoff(attempt);   /* only retryable classes + backoff */
}
```

### The protocol boundaries of segmented download

The challenge exercise's design intent deserves spelling out: concurrent segmentation is not merely "open a few more connections" — the **protocol boundaries** are three: each segment's Range must not overlap (only then does the splice come out right); the server may change between segments (the rigorous approach gives every segment an If-Range — any 200 abandons the whole and starts over); the `Accept-Ranges: bytes` declaration is only a necessary condition (whether a 206 truly comes must be tested — the test matrix's business). **Engineering boundaries**: a connection-count cap (be kind to the server — 4–8 segments is convention); a per-segment retry policy (retry only the failed segment — but an ETag change means all over). This exercise walks Chapter 90 (the Range protocol) and Chapter 100 (cache_range's fragment merging — local edition) again under a **real network** — composition-layer knowledge in its final project form.

## Exercises

### Basic: the complete download loop

On top of client_easy, add a Body callback streaming to `.part` + rename on completion: download 1 MB from a local mock server (Chapter 103). Acceptance criteria: the artifact's hash matches; a mid-run kill leaves only .part; rerun resumes to completion.

### Advanced: the full If-Range state machine

Implement the four states (fresh / resume / ETag changed / 416): the mock server's behaviors configured (Range supported/unsupported, ETag fixed/changing). Acceptance criteria: each state takes its correct path (206 resume / 200 fresh / 200 redownload / delete part); the machine proof of acceptance 4/6.

### Challenge: concurrent segmented download

Multi-connection segmentation (Range: bytes=a-b, each fetching a span) downloading in parallel into the same `.part` at each offset (pwrite-style positioned writes). Acceptance criteria: the segment hashes splice correctly; total throughput rises with connection count (verified on the local mock); a single segment's failure affects no other (the failed segment retried alone).

### A checkpoint before the book's finale

With Project 4 done, Volume 13 has two chapters left: Project 5 (the WebSocket service — Chapter 143, connection management and multicast in practice) and Project 6 (the comprehensive static server — Chapter 144, the book's finale). Looking back at the four completed projects' progression: **logstat** (a tool — one-shot streaming), **configd design** (a service — long-lived state and concurrent handover), **xdl** (a network tool — remote uncertainty and atomic delivery) — each project introduces one new class of **system constraint** (data scale / lifecycle / network), with twelve to twenty chapters' knowledge repeatedly tempered under different constraints. The arrangement validates an engineering intuition: **real systems' complexity lies not in a single technique but in the constraint combinations between techniques** — and the best a tutorial can do is let you walk each combination once in a controlled setting. The last two chapters add **long-connection concurrency** (WebSocket) and **full-stack synthesis** (the static server stringing HTTP/TLS/files/cache together) — the book's knowledge spiral completes its final turn in the project sequence.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Shape | xdl: idempotent resume / integrity checking / atomic delivery — a composable command-line part |
| Resume protocol | If-Range+Range: match 206 resume / mismatch 200 fresh — atomic on the server |
| Resume state | the .part size is the breakpoint — zero separate state; the ETag sidecar .part.etag |
| Atomic completion | same-directory .part→rename — downstream sees only "absent or complete" |
| 416 | part≥file — abnormal state, delete part, redownload |
| Streaming | the Body callback writes on arrival; limit = declared + tolerance — memory decoupled |
| Failure retention | interruption keeps .part (the resume resource); only ETag change/--force truncates |
| Three error classes | connection class (retry + backoff) / certificate & parameter class (configuration error) / HTTP 4xx (resource error) |
| Degradation | no Range support (200 without Content-Range) — whole download + explicit notice |
| Verifier | mandatory for HTTPS — no --insecure provided |
