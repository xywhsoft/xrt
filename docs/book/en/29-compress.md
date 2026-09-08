---
num: 29
slug: compress
title: Compression: Deflate and gzip
volume: 卷四 文本与结构化数据 · 卷四收官
type: practice
lead: One-shot and streaming postures, deterministic gzip-container products, and the judgment of "when not to compress".
api: compress, codec
---

## Orientation

The compress module implements Deflate compression and its gzip container wrapping — the standard algorithm of HTTP transfer encoding, file compression, and log archiving. Two usage postures cover two scenario classes: **one-shot** (`DeflateAll`: view in, owning buffer out — the worry-free path for small data and fixed configuration data) and **streaming** (`Init/Process/Finish`: feed chunk by chunk; network streams and large files never load whole into memory). Two engineering points run through this chapter: **deterministic products** (same configuration and input yield byte-identical output — the precondition for content addressing and caching) and the **overhead ledger** (the gzip container costs a fixed 18 bytes; small data may grow when "compressed"). Volume 4 closes with this chapter — the complete text-processing toolchain is now assembled, and from the next chapter we enter structured data (value/JSON/XSON), all of it standing on the foundations of this volume and Volume 3.

## Introduction

Three typical workloads. **HTTP response bodies**: text from a few KB to a few MB; compression cuts transfer volume by 70%+ — but it must be streaming, because the server cannot hold the entire response in memory just to compress it. **Log archiving**: at night, compress the day's logs into `.gz` — large files fed chunk by chunk, memory footprint independent of file size. **Configuration snapshots**: a few KB of structured text — the one-shot API finishes in three lines; but it must first pass the overhead ledger (see below). The shared question across all three: **when to compress**. The gzip header and trailer cost about 18 bytes fixed; 34 bytes of highly repetitive text compress to 30 — the smaller and more random the data, the thinner the payoff, even negative. "Don't compress below 1KB or already-compressed formats (jpg/zip themselves)" is the common rule of thumb in HTTP middleware; this chapter's first exercise has you build your own line with numbers from your own machine — the rule-of-thumb line is a starting point; the measured line is your project's line.

## Concepts

### Two postures (memorize the shape before the functions)

```diagram flow
- One-shot: DeflateAll takes an input view + config -> owning gzip buffer; small data done in three lines
- Streaming: Init creates -> Process feeds chunk by chunk -> Finish wraps up; memory independent of total size
- Reuse: Reset zeroes the stream state and starts over - the second stretch of data rebuilds no internal structures
```

The streaming trio's three states are isomorphic with Chapter 27's `xutf8state` — "state machine + chunked feeding + end marker" is XRT's universal shape for unbounded data. `Process` may split anywhere (chunk size does not affect the product — streaming and one-shot outputs are **byte-identical**, verified empirically in the example below); `Finish` wraps up and produces the trailer.

### The gzip container and determinism

`DeflateAll` produces gzip format by default: a 10-byte header + the Deflate stream + an 8-byte CRC32/length trailer. The header carries a "magic-grade" engineering property: **same configuration and input produce byte-identical output** — no timestamps or other variable fields (unlike the gzip command-line tool, which embeds a time by default). Determinism buys three things: content addressing (Chapter 12 fingerprints as cache keys — the same data compresses to the same key at any moment), test snapshots (a compressed product can be written whole into an assertion), and distributed consistency (multi-node products comparable byte for byte).

### Compression level and resource limits

The configuration's core parameter is Level (0~9: 0 stores without compressing, 6 is the default, 9 is smallest and slowest). Level-selection experience: **6 for network transfer** (the payoff curve flattens sharply after 6), **9 for archival storage** (time-insensitive), **4 or lower when the CPU is tight**. On resource limits: the decompression side's output size is declared by the compressed data, but **decompressing untrusted input requires an upper limit** — the defense against zip bombs (tiny input decompressing to enormous output) is exactly the `xrtresourcelimits` `iMaxInputBytes` family from Chapter 3, configured for the decompression scenario; when processing compressed data from outside, setting it is mandatory.

### The junction with HTTP

Volume 9's HTTP chapters will show the complete `Content-Encoding: gzip` chain; this chapter just equips the data plane: response bodies compress streaming (compress while sending), static files precompress one-shot (`.gz` sitting on disk), and failed negotiation falls back to no compression. Chapter 28's Base64 has a combination slot here too: gzip + Base64 is the standard two-step of "compression into a text channel".

## Examples

### Complete program: one-shot compression and the overhead ledger

From the repository sample `examples/compress/deflate/main.c`:

```embed path="examples/compress/deflate/main.c" title="examples/compress/deflate/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/compress/deflate/main.c -lws2_32 -liphlpapi
plain=34 gzip=30
```

**What just happened.** (1) `xrtDeflateConfigInit` takes the default configuration (Level 6, gzip container) — the config struct is Init'd first and then the fields you need are changed, the library-wide uniform posture. (2) 34 bytes of highly repetitive text compress to 30: the Deflate stream occupies only about 12 bytes (repetitive patterns eaten by dictionary matching), plus the container's fixed 18 — the **overhead ledger** at a glance: however short the data, the container's 18 bytes cannot be escaped; this 34→30 line is the empirical proof of "thin payoff on small data". (3) Product determinism is verifiable by hand: compress twice in a row and compare byte for byte (Chapter 12's `Hash64` gives the fastest fingerprint) — same fingerprint is determinism's machine testimony.

### Complete program: the full streaming interface and round trip

From `examples/compress/stream_tour/main.c` — chunked compression, streaming decompression, and state reuse all in one:

```embed path="examples/compress/stream_tour/main.c" title="examples/compress/stream_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/compress/stream_tour/main.c -lws2_32 -liphlpapi
compress: deflate valid/done/size ok (split=2 writes)
compress: inflate roundtrip original=51
compress: reset reuse second-stream ok
compress: config corrupt rejected
```

**What just happened.** (1) Streaming Deflate is fed in two chunks (`split=2`), and the gzip it produces is byte-identical to the one-shot version — chunking is merely a way of feeding and does not change the product; this guarantees that a server that "compresses while receiving" and a precompressed "compress-the-whole-block" product are interchangeable. (2) Streaming Inflate restores the gzip to the original 51 bytes — `roundtrip` is compression correctness's final judge. (3) After `Reset`, the same state object starts a second stream — internal dictionary and other structures are reused; high-frequency small-stream compression (one stream per log entry) need not build and tear down repeatedly. (4) The last line is configuration validation: a config with a deliberately illegal Level is rejected before compression starts — "bad configs die at the entrance" is the Nth recurrence of the same philosophy as Chapter 26's strict parsing and Chapter 27's STRICT transcoding.

## Contracts

- **Two postures**: small data finishes in one step with `DeflateAll`; streaming `Init/Process/Finish` keeps memory independent of total size; chunking does not change the product.
- **Determinism**: same configuration and input are byte-identical; usable for content addressing and test snapshots.
- **Overhead ledger**: the gzip container costs a fixed ~18 bytes; small-data compression can go negative — establish your own "minimum compressible size" line.
- **Level choice**: network 6, archive 9, CPU-sensitive 4-; the payoff curve flattens sharply after 6.
- **Decompression defense line**: decompressing untrusted input must set an output cap (resource limits), guarding against decompression bombs.
- **Streaming shape**: Init/Process/Finish plus Reset reuse — the universal streaming pattern isomorphic with `xutf8state`.

### Compression's selection map

Deflate is not the only compression algorithm; when selecting in engineering it pays to know its neighbors. **Deflate/gzip** (this chapter): king of compatibility — browsers, command lines, and every language's standard library recognize it; middling compression ratio; cheap CPU. **zstd/br** (external libraries): better ratio and speed, but they introduce third-party dependencies — under XRT's zero-dependency principle they are not in the kernel; integrate via the host when needed. **The LZ4 family**: extremely fast but low ratio, for latency-sensitive in-memory data exchange. The practical decision tree: **facing open interop → Deflate (no choice — it is the lingua franca)**; internal closed links with metric sensitivity → evaluate external algorithms; in-memory/realtime settings → don't compress, or LZ-class. The three things this chapter teaches — the streaming posture, the overhead ledger, and resource limits — hold for any algorithm: change the algorithm, not the discipline; these three are the backbone of compression engineering.

### From examples to engineering: three hosts of compression

**Transport middleware**: attached to the response path, compressing streaming and deciding whether to compress by content type and size — Pitfall 1's judgment line lands here. **Archival batch jobs**: nightly tasks scan directories and produce `.gz` files — the challenge exercise is its prototype; deterministic products make incremental archives verifiable. **Single-message compression**: IM messages, telemetry entries — pass the overhead ledger first; most single messages should not be compressed, and batching before compressing is the right answer. Beyond the three hosts, one general discipline: **compress as close to the source as possible** — data compressed once at the source and never decompressed along the way beats "decompress and recompress at every hop" by an order of magnitude in CPU.

## Pitfalls

### Pitfall 1: compressing small data reflexively

Symptom: after adding "gzip everything" middleware, small responses get slower and transfer volume actually grows; monitoring shows CPU up with no bandwidth saved.

Cause: fixed container overhead plus small data's low redundancy — the `plain=34 gzip=30` ledger is the norm, not the exception, on the small end; recompressing already-compressed formats (images, zip) is pure loss.

```c bad
if ( Response.Size > 0 ) {   /* compress as long as there is data */
	Compress(&Response);      /* 300 bytes of JSON may compress to 320 */
}
```

```c good
if ( Response.Size >= 1024 && !AlreadyCompressed(Type) ) {
	Compress(&Response);      /* compress only past the rule-of-thumb line and only if compressible */
}
```

### Pitfall 2: decompressing untrusted data without a cap

Symptom: while processing externally uploaded archives, the service's memory balloons until it is taken down — the security report reads "decompression bomb".

Cause: Deflate's compression ratio can be extreme (the same content repeated ten-thousandfold); tiny input decompresses to enormous output — uncapped decompression hands the allocation decision to the attacker.

```c bad
bytes pPlain = xrtInflateAll(pGzipView, &iSize, NULL, 0);   /* no cap */
/* the attacker hands over 10KB of input, 4GB comes out - the process drops on the spot */
```

```c good
xrtresourcelimits Limits;
xrtResourceLimitsInit(&Limits);
Limits.iMaxInputBytes = 64 * 1024 * 1024;   /* decompression output capped at 64MB */
bytes pPlain = xrtInflateAll(pGzipView, &iSize, &Limits, 0);
if ( pPlain == NULL ) {
	RejectUpload();   /* over the limit means reject - the defense line is at resource limits, not the business layer */
}
```

## Exercises

### Basic: the overhead ledger

Compress highly repetitive text and random bytes at 10, 50, 100, 500, and 1000 bytes each, recording a ten-row table of "original/compressed/ratio" — find the size line where "compression starts to pay" for your own data shape, compare it against the chapter's 34→30 ledger, and write one sentence of conclusion: "where my scenario should draw the line".

### Advanced: streaming vs one-shot side-by-side (proof of determinism)

Compress the same data once with `DeflateAll` and once streaming in "five Process chunks"; assert the products are byte-identical; then compute each product's `Hash64` fingerprint for the fastest comparison. Hint: this is precisely the precondition proof for trustworthy cache keys.

### Challenge: an HTTP static-asset precompressor (the archive host's full form)

Scan a directory's text assets (.html/.css/.js) and produce `.gz` companion files for those past the size line: compress streaming (memory independent of file size), keep the originals, and sync the `.gz` timestamps with the originals. Acceptance criteria: precompressing a 100MB directory peaks under 10MB of memory; the `.gz` products are deterministic (fingerprints unchanged on rerun); files below the size line are skipped and listed.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two postures | one-shot `DeflateAll` / streaming Init-Process-Finish (chunking changes nothing, bad configs rejected at entry) |
| Container overhead | gzip fixed at ~18 bytes (10 header + 8 trailer); small data can go negative |
| Determinism | same config and input, byte-identical; the precondition for content addressing, test snapshots, multi-node comparison |
| Level | network 6 / archive 9 / CPU-sensitive 4-; payoff curve flattens sharply after 6 |
| Decompression defense | untrusted input gets an output cap (resource limits); bombs are rejected at the boundary |
| Reuse | `Reset` zeroes the stream state and reuses internal structures; high-frequency small streams need no repeated build/teardown |
| Combination slot | gzip+Base64 into text channels; the HTTP chain is in Volume 9 |
| Selection map | open interop = Deflate; internal links may evaluate zstd/br; realtime = LZ-class or none |
| Three hosts | transport middleware (streaming + size line) / archival batch / single message (ledger first) |
| General discipline | compress near the source, never decompress along the way; per-hop decompress-recompress is CPU waste |
