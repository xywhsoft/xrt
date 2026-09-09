---
num: 35
slug: text-pipeline
title: Text Pipeline Composition: JSON Config → Template Rendering → Protocol Output
volume: 卷四 文本与结构化数据 · 卷四收官
type: composition
lead: Stringing all six chapters' tools into one real pipeline — extraction, parsing, merging, rendering, encoding, compression, each in its place.
api: json, template, value, regex, codec, compress, html
---

## Orientation

Volume 4's closing chapter does something different from the ones before: **it teaches no new tools; it strings tools into a pipeline**. One real data pipeline — "logs/semi-structured text → regex extraction → value-tree processing → JSON reading and merging → template rendering → encode-and-compress output" — every station is the protagonist of some earlier chapter. The composition chapter's value lies in exposing "problems single-chapter teaching can't see": how the data contracts between stations are defined, at which layer errors report, where performance jams, and how boundary strictness propagates along the whole chain. After this chapter, Volume 4 turns from "six toolboxes" into "one assemblable production line".

## Introduction

A real need: an ops platform generates a "daily service report" from multiple sources. Source one: the service's JSON Lines logs (error counts and latency stats live in fields); source two: a stretch of semi-structured status text (version number and connection count in `key=value` form — regex territory); source three: the platform's default config (JSON) plus user overrides (XSON full types). Products: an HTML mail body (template rendering) + a plain-text summary (another template) + an archive copy (gzip-compressed to disk).

Six stations, six tools; any single station looks "already learned", but connected they raise new questions: how do regex-extracted strings enter the value tree (where does type conversion happen)? How do three data sets merge into the template's input (Chapter 31 ObjectMerge's nested semantics)? Do two rendered templates need two data sets (one value tree, two templates)? At which step is archival compression done (after rendering, before writing to disk)? How do error reports locate "which field of which source"? — This chapter's body answers these station by station, along the pipeline.

## Concepts

### The pipeline panorama

```diagram flow
- Source A: JSON Lines logs -> per-line JSON parsing (SAX/DOM) -> statistics value tree
- Source B: status text -> regex named-capture extraction -> into a value tree (string->number happens here)
- Source C: default JSON + user XSON -> parse each -> ObjectMerge
- Convergence: three value trees ObjectMerged into render data (one top-level key each, avoiding key conflicts)
- Render: the HTML template and the plain-text template each render once (the same data)
- Output: mail body sent directly; archive copy gzip-compressed (one-shot DeflateAll) to disk
```

### Data contracts between stations

The first task of pipeline design is not choosing functions but **defining the contracts between stations** — what shape and type each station's output is. Three disciplines: **contracts use the value tree as currency** — every station's input and output is `xvalue`, types within Chapter 31's type family (don't invent intermediate forms like "stringified numbers"); **conversion completes in the extraction layer** — regex captures are naturally strings, and the "looks like a number" conversion happens explicitly in the extraction station (Chapter 26's strict parsing), leaving no type hesitation for downstream; **merge by namespaces** — the three sources each hang under one key of the top-level object (`stats`/`runtime`/`config`), so cross-source key conflicts vanish structurally and the merge strategy only handles "same-source overrides".

### At which layer errors report

Any station on the chain can fail; the attribution principle for reporting: **report at the layer that owns the context**. JSON parse failure — the log layer knows which line of which file (Chapter 32's line/column location plus source annotation); regex extraction failure — the extraction layer knows the pattern and the original text (a value that can't be extracted returns "missing" rather than an error — "field missing" is a data state, not an error); merge conflict — the config layer knows the key name and both sources; template render path miss — the render layer decides per configuration between error and empty string. The whole chain's errors finally report up through Chapter 4's cause chain: `报告生成失败 → 来源 B 提取失败 → 第 3 行缺 version 字段` (report generation failed → source B extraction failed → line 3 missing the version field) — each layer contributes its own link.

### One station at the pipeline's end: HTML entity escaping (html)

When the rendered output heads for an HTML page or a mail body, one last station remains at the chain's tail: turning `<`, `&`, and quotes into entities — **every dynamic text entering an HTML context must pass this station**; skipping it is an injection vulnerability. The html module is small and restrained, three functions in one line: `xrtHtmlEscapeSize` strictly validates UTF-8 and returns the exact escaped byte count (pass one of the two-pass approach); `xrtHtmlEscapeWrite` writes into the caller's buffer (capacity must include the trailing zero); `xrtHtmlEscape` directly produces a zero-terminated string freed with `xrtFree`. The one parameter you must get right is **context**: `XHTML_ESCAPE_TEXT` for element content, `XHTML_ESCAPE_ATTRIBUTE` for attribute values **enclosed in quotes** — the two contexts escape different character sets, and picking the wrong one is the classic cross-site-scripting variant. It depends on the UNICODE capability (Chapter 27's terrain) and strictly validates UTF-8 before escaping — malformed input is stopped here rather than carried into the product. The ordering discipline matches compression: **render → escape → compress**; escaping always lives at the encoding station and never leaks into templates.

```c
/* The last station on the output side: escape dynamic text before it enters HTML (context picks the charset) */
size_t iSize = 0;
if ( !xrtHtmlEscapeSize(UserBio, XHTML_ESCAPE_TEXT, &iSize) ) { return fail(); }
str sSafe = xrtHtmlEscape(UserBio, XHTML_ESCAPE_TEXT, NULL);
/* sSafe goes into the template product; xrtFree when done — an owning string (Chapter 25) */
```

### How performance and boundaries distribute along the chain

Performance is unevenly distributed: **parsing and rendering are the constant heavyweights** (every byte passes through once), extraction and merging are linear lightweights; the optimization order is rendering first (compile the template once), then parsing (SAX streaming for logs), and only then micro-optimization. Boundary strictness is likewise layered: **every external input port (log files, user config) gets the full gates** — JSON's three gates, the regex input-length cap, the decompression cap (Chapter 29's defense line); **between internal stations, trust** — value trees are not re-validated from extraction to rendering (validation happens once at the entrance). "Tight outside, loose inside" is the pipeline's boundary philosophy: spend the strictness budget on uncontrollable entrances, not between your own stations.

### Testability: the pipeline as jigsaw puzzle

An unexpected dividend of pipelining is testability: every station is a pure function (value tree in, value tree out) and can be tested independently; between stations, minimal contract data (a value tree of one or two records) drives integration tests; end-to-end tests run the whole chain on fixed input and assert the final text (Chapter 12's hashing gives large outputs fingerprint assertions). Compare with testing "one giant processing function" — pipelining means every station's failure locates to its stop, instead of guessing inside the blob.

## Examples

### Complete program: status-text extraction into a value tree

The pipeline's source-B minimal implementation — regex extraction + type conversion + value-tree construction, a composed use of Chapter 30's example:

```c
/* pipeline_extract.c - source B: status text -> value tree */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static xregex* gPair;

void extract_init(void)
{
	gPair = xrtRegexCompile(XRT_STR_LITERAL(
		"(?<name>[A-Za-z_]+)=(?<value>\\d+)"));
}

xvalue* extract_status(xstrview Text)
{
	xvalue* pRoot = xrtValueObject();
	xregexmatcher* M = xrtRegexMatcherCreate(gPair, Text);
	xstrview Name;
	xstrview Value;

	while ( xrtRegexMatcherNext(M) ) {
		int64 iNumber;
		xrtRegexMatcherCaptureNamed(M, XRT_STR_LITERAL("name"), &Name, NULL);
		xrtRegexMatcherCaptureNamed(M, XRT_STR_LITERAL("value"), &Value, NULL);
		/* type conversion in the extraction layer: strict parsing (Chapter 26); failures skip as missing */
		if ( xrtIntParse(Value, 10, 0, &iNumber) ) {
			xvalue* pInt = xrtValueInt(iNumber);
			xrtValueObjectSet(pRoot, Name, pInt);
			xrtValueRelease(pInt);
		}
	}
	xrtRegexMatcherFree(M);
	return pRoot;   /* owning value tree; the caller frees it */
}

int main(void)
{
	xvalue* pStatus;
	int64 iVersion;
	int64 iConnections;

	extract_init();
	pStatus = extract_status(
		XRT_STR_LITERAL("version=204 connections=32 uptime=98765"));
	if ( pStatus == NULL ) {
		return 1;
	}
	if ( xrtValueObjectGet(pStatus, XRT_STR_LITERAL("version")) == NULL ) {
		return 2;   /* version field missing: the extraction stage's failure surfaces here */
	}
	printf("extracted keys=%zu
", xrtValueCount(pStatus));
	xrtValueRelease(pStatus);
	xrtRegexRelease(gPair);
	return 0;
}
```
```term
$ gcc -O1 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single pipeline_extract.c -lws2_32 -liphlpapi
$ ./a.exe
extracted keys=3
```


**Where this code stands.** Three chapters' tools share the frame: Chapter 30's regex two-layer model (compile globally, build a matcher per pass) + named captures; Chapter 26's strict parsing for explicit type conversion ("looks like a number" becomes "confirmed to be a number"); Chapter 31's value-tree construction and reference balancing (Set then Release). This 30-line function is the pipeline's most typical station shape — **value tree in, value tree out, managing its own resources**.

### Complete program: three-source merge and dual-template rendering

The pipeline's convergence and output segments — three value trees merged, two templates rendered:

```c
/* pipeline_render.c - converge -> render -> output */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

static xtemplate* gHtml;
static xtemplate* gPlain;

void render_init(xstrview HtmlTpl, xstrview PlainTpl)
{
	gHtml = xrtTemplateCompile(HtmlTpl);
	gPlain = xrtTemplateCompile(PlainTpl);
}

int render_report(xvalue* pStats, xvalue* pRuntime, xvalue* pConfig,
	FILE* pArchive)
{
	xvalue* pData = xrtValueObject();
	xvalue* pHtml;
	str sPlain;
	int iResult = 0;

	/* namespace merge: each source owns one top-level key - conflicts vanish structurally */
	if ( pData != NULL ) {
		xrtValueObjectSet(pData, XRT_STR_LITERAL("stats"), pStats);
		xrtValueObjectSet(pData, XRT_STR_LITERAL("runtime"), pRuntime);
		xrtValueObjectSet(pData, XRT_STR_LITERAL("config"), pConfig);
	}
	pHtml = xrtTemplateRender(gHtml, pData, NULL, NULL);
	sPlain = xrtTemplateRender(gPlain, pData, NULL, NULL);
	if ( (pHtml == NULL) || (sPlain == NULL) ) {
		iResult = 1;
		goto Cleanup;
	}
	printf("%s\n", sPlain);
	/* archive: the compact-rendered result compressed whole (one-shot path) - Chapter 29 */
	{
		xbytesview View = { (cbytes)sPlain, strlen(sPlain) };
		size_t iGzSize = 0;
		bytes pGz = xrtDeflateAll(View, NULL, &iGzSize);
		if ( pGz != NULL ) {
			fwrite(pGz, 1, iGzSize, pArchive);
			xrtFree(pGz);
		}
	}
Cleanup:
	xrtFree(sPlain);
	xrtFree(pHtml);
	xrtValueRelease(pData);
	return iResult;
}

int main(void)
{
	xvalue* pStats;
	xvalue* pRuntime;
	xvalue* pConfig;
	FILE* pArchive;
	int iResult;

	render_init(XRT_STR_LITERAL("errors={%stats.errors}"),
		XRT_STR_LITERAL("plain: {+$config.name}"));
	pStats = xrtValueObject();
	pRuntime = xrtValueObject();
	pConfig = xrtValueObject();
	if ( (pStats == NULL) || (pRuntime == NULL) || (pConfig == NULL) ) {
		return 1;
	}
	{
		xvalue* pErrors = xrtValueInt(3);
		xvalue* pName = xrtValueString(XRT_STR_LITERAL("daily"));
		xrtValueObjectSet(pStats, XRT_STR_LITERAL("errors"), pErrors);
		xrtValueObjectSet(pConfig, XRT_STR_LITERAL("name"), pName);
		xrtValueRelease(pErrors);
		xrtValueRelease(pName);
	}
	pArchive = fopen("report.gz", "wb");
	if ( pArchive == NULL ) {
		return 2;
	}
	iResult = render_report(pStats, pRuntime, pConfig, pArchive);
	fclose(pArchive);
	xrtValueRelease(pStats);
	xrtValueRelease(pRuntime);
	xrtValueRelease(pConfig);
	xrtTemplateRelease(gHtml);
	xrtTemplateRelease(gPlain);
	return iResult;
}
```
```term
$ gcc -O1 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single pipeline_render.c -lws2_32 -liphlpapi
$ ./a.exe
plain: daily
$ ls -l report.gz
（归档副本已生成：紧凑渲染 + gzip 一次性压缩）
```


**Where this code stands.** (1) Namespace merging — the three sources each attach under a key; `ObjectMerge`'s override semantics never even need to appear (no cross-source conflicts). (2) One data set, two templates — HTML and plain text share the same `pData`; the templates differ only in prefixes and control structures. (3) Archival after rendering, before writing to disk — one-shot compression exactly matches the "whole block of text" shape. (4) Reference balancing is clear: pData can be released as soon as the three sources are attached (the container holds references); the two render products each `xrtFree`. This 50-line program puts the protagonists of Chapters 31/34/29 on stage in pipeline order.

### Four checkpoints of station design

Run each station function through four checkpoints, and pipeline quality gains a floor. **Checkpoint one: value trees in and out** — the station signature should be `xvalue* f(输入视图或值树)` (input view or value tree); the appearance of `char**` or proprietary `struct` types means the contract has cracked. **Checkpoint two: resources self-managed** — matchers, value trees, and buffers created inside a station balance inside it; intermediate products other than the output never leak to the caller (the caller receives only the owning product returned). **Checkpoint three: failure paths isomorphic** — every station's failure takes the same shape (return NULL plus the error slot), so the orchestration layer needn't write different failure handling per station. **Checkpoint four: unit-testable** — fed minimal input (a record or two), the station runs and asserts independently. With the four checkpoints passed, a station is a replaceable screw — the orchestration layer can reorder, parallelize, or add caching without touching the station itself.

### The orchestration layer: the pipeline's conductor

The orchestration layer outside the stations has design principles of its own. **Order made declarative** — the stations' execution order sits visible in the orchestration function (order is code), not hidden in stations' side effects; **one-way data flow** — value trees flow from one station to the next, never back-flowing, never sharing mutable state (the precondition for parallelization); **early exit and partial success** — decide which stations abort on failure (if the config can't be read, don't render) and which allow partial success (log statistics fail but status text is usable — the report is annotated "statistics missing" and keeps generating) — this decision belongs to the business, lives in the orchestration layer, and goes into comments. The orchestration layer should be short enough to read in one breath: a normal person can see "where the data comes from, which stations it passes, where it goes" in thirty seconds.

### The numbers of a real case

Give this pipeline a reference scale to build engineering intuition: the daily-report scenario — 50MB of logs (SAX streaming, peak memory in the MBs), 2KB of status text (regex extraction), 10KB of config (dual-format merge), two rendered outputs of about 200KB total (template compiled once), gzip archive of about 30KB. The whole pipeline runs seconds-level serially on one machine, parsing taking seventy percent and rendering twenty — this performance portrait doesn't contradict the "render before parse" optimization order: optimizing rendering means compiling once (saving repeated compilation), optimizing parsing means streaming (saving memory); neither changes the fact that parsing is the time heavyweight. Your pipeline's first step is always to measure your own numbers, then order priorities with this chapter's sequencing view.

## Contracts

- **The value tree is currency**: inputs and outputs between stations are always `xvalue`; type conversion completes explicitly in the extraction layer.
- **Namespace merging**: multiple sources each take a top-level key; override semantics handles same-source conflicts only.
- **Error attribution**: report at the layer that owns the context; the whole chain reports up level by level through the cause chain; "missing" is a data state, not an error.
- **Boundary philosophy**: tight outside, loose inside — every external entrance gets the gates (three gates/length/decompression caps), internal stations trusted.
- **Performance order**: rendering first (compile once), then parsing (streaming), micro-optimization last.
- **Resource balancing**: every station manages its own resources (matchers, value trees, products); failure paths are isomorphic.
- **HTML escape context**: TEXT for element content / ATTRIBUTE for quoted attribute values — the wrong context under-escapes; escape after rendering, before compression.

### Evolution path: from literal translation to pipeline

The pipeline is not the first version designed; it is the second version refactored — knowing this evolution path is more practically valuable than landing there in one step. **Version one (literal)**: one big function works from input to output and gets the business running — it is not a mistake but a starting point; its value is validating the requirement. **Version two (stationization)**: cut the big function into stations by "data-shape transformation" — every `char*`-to-`char*` paragraph becomes a "view to value tree" or "value tree to text" station — Pitfall 2's refactor is exactly this move. **Version three (contracts hardened)**: station signatures unified, error shapes unified, unit tests filled in — this chapter's "four checkpoints" realized. **Version four (orchestration evolves)**: stations unchanged, the orchestration layer upgrades — log parsing and status extraction run in parallel (the two stations have no data dependency), config loading moves to startup (unchanging data is not re-read), rendering gains a cache (output-fingerprint cache for same data, same template). Every version changes the connections, not the stations — that is the pipeline's essential meaning: **stations stable, orchestration evolving**.

### A preview of the interfaces with Volumes 5~9

This pipeline is no isolated case; it is a rehearsal for the rest of the book. Volume 5's Logger (from Chapter 37) is the next stop for "where rendered text goes" (sink output); Volume 7's network engine packs "JSON parsing + template rendering" into request callbacks — the same pipeline running in a connection's context; Volume 9's HTTP content negotiation decides "should the rendered product be compressed, with what encoding" — this chapter's gzip+Base64 combination reappears at the protocol layer. Read the later volumes with pipeline thinking: every new module is a deep expansion of some station, and you have already seen the panorama of where they stand in the composition chapter.

## Pitfalls

### Pitfall 1: type conversion scattered downstream

Symptom: template rendering reports "type mismatch" everywhere; or defensive "if it's a string, convert again" code appears all over downstream — the same data converted three times.

Cause: the extraction layer lazily put strings into the value tree as-is, pushing type responsibility onto all downstream — the contract fell.

```c bad
/* extraction layer: numbers put into the value tree as strings */
xvalue* pVal = xrtValueString(Value);   /* "128" is a string value */
/* downstream template: {%runtime.port} type mismatch - errors or conversions everywhere */
```

```c good
/* extraction layer completes the conversion: what enters the value tree is the final type */
int64 iNumber;
if ( xrtIntParse(Value, 10, 0, &iNumber) ) {
	xvalue* pInt = xrtValueInt(iNumber);
	xrtValueObjectSet(pRoot, Name, pInt);
	xrtValueRelease(pInt);
}
/* fields of uncertain type are annotated in the extraction layer (or kept out of the tree); downstream defends nothing */
```

### Pitfall 2: the whole chain in one big function

Symptom: `generate_report()` at three hundred lines — log parsing, regex extraction, merging, rendering, compression all in one function body; any station's error must be located inside the whole function; testing is end-to-end only.

Cause: the pipeline was never stationized — the "good enough to run" literal style welded the six stations together.

```c bad
str generate_report(void)
{
	/* 300 lines: parse logs -> extract status -> read config -> merge -> render -> compress... */
	/* all intermediates are locals; no segment can be pulled out and tested alone */
}
```

```c good
/* one function per station: value tree in, value tree out, resources self-managed */
xvalue* collect_stats(xstrview LogText);      /* station 1 */
xvalue* extract_status(xstrview StatusText);  /* station 2 */
xvalue* load_config(void);                    /* station 3 */
int render_report(xvalue* Data, FILE* Out);   /* stations 4+5 */
/* the main flow is pure orchestration - every station unit-testable, replaceable, reusable */
```

### Station close-up: source A's statistics station

Source A (JSON Lines log statistics) is the pipeline's performance heavyweight and deserves its own expansion. Shape choice: log files can run tens of MB — DOM per-line parsing (one JSON object per line, line-level Parse) is the standard posture; line iteration uses Chapter 25's line splitting, each line parsed independently (one corrupted line doesn't affect other lines' statistics — partial-success semantics hold inside the station). The accumulated statistics go into a value tree: error counts are ints, latency distributions arrays, per-module groups objects — the `ObjectSetNew` family accumulates while parsing. Two gates in place: a line-length cap (malformed log lines can't balloon memory) and an entry-count cap (loop iterations bounded). This station's output contract: `{errors: int, p50/p95: int, modules: {name: count}}` — the downstream template fetches by this shape, and any shape change trips the template's smoke test immediately.

### Station close-up: the render-output separation decision

Are rendering and output (compression, writing, sending) one station or two in the pipeline? It depends on output-target diversity. **Single output** (disk only) — the render function compresses and writes the file at its end, one station done; **multiple outputs** (mail body + archive copy, this chapter's scenario) — rendering only produces text, output stations consume it each in their own way — because the two outputs process the text differently (mail uses it directly, the archive compresses), merging them into one station would let "output format" choices pollute the render function. The mnemonic: **as many output processing modes as there are, that many output stations**. The same principle answers "compress before or after rendering": what gets compressed is the final text (after rendering), not the value tree (before rendering there is no text to compress) — the frequency with which this question comes up in architecture reviews is astonishing.

### Unfolding the test pyramid

The testability section gave the three-layer pyramid's skeleton; here each layer's concrete practice unfolds. **Station unit tests** (the base): every station function gets a minimal input set — one normal, one empty, one malformed, one boundary; assert the output tree's shape and key values (Chapter 31's exact reads for assertions, `ValueHash` for whole-tree fingerprints). **Contract integration tests** (the middle): adjacent stations chained — the previous station's real output feeds the next, verifying both sides understand the contract the same way (the extraction station says "port is int", and the template station thinks so too — chaining exposes the disagreement on the spot). **End-to-end fingerprint tests** (the top): fixed input runs the whole chain; the final text's `Hash64` goes into the assertion — any station's any change that alters the output turns the test red immediately. The three layers' ratio is roughly 7:2:1 — thickest base, thinnest top; which layer turns red when you change things tells you which level the problem is at (a broken station reddens the base, a changed contract the middle, a changed overall output the top).

## Exercises

### Basic: a two-station pipeline

Implement the minimal "JSON config → template rendering" pipeline: parse a config (2 fields), build a value tree, render a three-prefix template. Each of the two stations is its own function.

### Advanced: three-source convergence

Reproduce the composition chapter's convergence segment: hard-code three source value trees (2~3 keys each), merge by namespaces, render two templates (HTML and plain text) from the same data. Verify the two outputs' field consistency.

### Challenge: the complete report pipeline

Implement the introduction scenario's full pipeline: JSON Lines log statistics (SAX counting) + status-text extraction (regex) + config merging (JSON+XSON) + dual-template rendering + gzip archival. Acceptance criteria: five stations, each an independent function with its own unit test; fixed input produces deterministic text end-to-end (hash assertion); all three boundary gates in place; Chapter 6's statistics verify zero leaks; total code under 300 lines — the pipeline's meaning lives in that number.

### From the composition chapter to Volume 4's close

This chapter closes Volume 4 and is also the debut of the new "composition chapter" type — its positioning differs from single-module chapters: **it teaches no new APIs; it teaches assembly**. Later volumes will have composition chapters too (Volume 5's "debugging and diagnostics composition", Volume 6's "scheduling in practice"), all sharing the structure "panorama → data contracts → station close-ups → evolution path" — the structure this chapter establishes will be reused. Final advice to the reader: run this chapter's two complete programs by hand, then build the challenge exercise's complete pipeline — the under-300-lines constraint is not hazing but a reminder: **the pipeline's value lies in clear stations, not in code volume**; overflow means some station should be split or deleted.

### Volume 4 retrospective

Ten chapters done; Volume 4's asset list: strings (view pipeline/builder), numbers (strict/shortest round-trip/format strings), charsets (transcoding/scalar operations), codecs (three bridges), compression (two postures/the ledger), regex (linear/two layers), value trees (currency/reference counting), JSON (three paths), XSON (full types), templates (compile-render separation) — ten modules plus this chapter's assembly skill close the loop on text and structured-data processing. The next volume enters system services (logging, IO, time, files, processes) — the layer above data; the pipeline's output end and config end will both find their home there.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Currency | the value tree runs the whole chain; type conversion completes once, explicitly, in the extraction layer |
| Convergence | multi-source namespace merge (one top-level key each); override semantics handles same-source conflicts only |
| Errors | report at the layer owning the context; the cause chain reports up level by level; missing ≠ error |
| Boundaries | tight outside, loose inside: entrances fully gated (three gates/length/decompression caps), internal stations trusted |
| Performance | render: compile once → parse: streaming → only then micro-optimization |
| Structure | one function per station: value trees in and out, resources self-managed, failures isomorphic, unit-testable and replaceable |
| Testing | station unit tests + contract integration + end-to-end fingerprints — a three-layer pyramid at roughly 7:2:1 |
| Station checks | value trees in/out / resources self-managed / failures isomorphic / unit-testable — four checks passed, then a screw |
| Orchestration principles | order declarative / one-way data flow / early-exit and partial-success decided explicitly |
| Render-output separation | as many output processing modes as there are, that many output stations; compression after rendering |
| HTML escaping | dynamic text entering HTML must be escaped; TEXT/ATTRIBUTE contexts; strict UTF-8 validation first |
