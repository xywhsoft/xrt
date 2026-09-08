---
num: 25
slug: string
title: The Complete String Toolkit
volume: 卷四 文本与结构化数据
type: practice
lead: Zero-allocation view pipelines, O(n) builder concatenation, search, editing, and formatting — the three-layer arsenal for text processing.
api: string
---

## Orientation

Volume 4 begins with strings. XRT's text philosophy is Chapter 3's view convention honored in full: **zero allocation whenever possible** — Trim, Cut, and case-insensitive comparisons merely carry `(指针, 长度)` (pointer, length) pairs around, and allocation is deferred until new memory is truly needed; **when allocation is due, use the builder** — the append chain totals O(n) copying instead of Concat's O(n²); **products are always owning** — functions return `str` and you release with `xrtFree`, seamless with Chapter 5's conventions. This chapter arranges the three layers as a progression — view pipeline → builder → the full-family cheat sheet — so that search, editing, splitting, and formatting are all stocked in one pass at the cheat-sheet layer.

## Introduction

A URL-handling function has five jobs: strip surrounding whitespace, split path from query at `?`, compare the path lowercased, delete certain characters from the query, and finally prepend a prefix. The textbook approach `strdup`s a fresh copy at every step — five steps, five allocations, five frees, and five cleanup chains hanging off the failure path. But look closely at those five steps: the first four **need no new memory at all** — whitespace trimming only slides two pointers inward, splitting merely divides one view into two views, case comparison has a dedicated IgnoreCase variant, and character deletion can write into the caller's stack buffer. Only the final prefix concatenation truly needs an allocation.

That is the starting point of the view pipeline: separate "operating on text" from "producing new text" — the former costs a few pointer additions and subtractions, and the latter is squeezed down to the minimum. On hot paths (code that runs once per request) this beats "copy at every step" by an order of magnitude — not because the algorithm is smarter, but because it **makes no unnecessary allocations**.

## Concepts

### Layer 1: the view pipeline (zero allocation)

```diagram flow
- Trim: trims surrounding whitespace - only slides pointers, original bytes untouched
- Cut: splits into two at the first separator - both halves are views; pass NULL as the out-param for the half you don't want
- FilterTo: removes bytes belonging to a set - writes into the caller's buffer, fails outright when capacity is short
- Concat: joins two views - the pipeline's first real allocation, and the product is owning
```

The design discipline of the pipeline: **the later you allocate, the better**. Every non-allocating step preserves choices for the steps that follow (a view sliced out can be sliced again, filtered again, compared again); once an allocation happens, every later operation works on a copy. The criterion for whether a step needs allocation is simple: is the output's byte set a proper substring of the input (Trim, Cut, substring slicing all are — zero allocation suffices) or does it require reassembly (concatenation, replacement, filtering — allocate, or use the caller's buffer)?

### Layer 2: the builder (O(n) concatenation)

Do not use a `Concat` chain for more than two or three segments — every Concat produces a new intermediate string, and an n-segment chain means O(n²) total copying. The `xstrbuf` builder grows its internal buffer by doubling (the same strategy as Chapter 15's containers), so the append chain totals O(n) copying. `Take`'s hand-over semantics is isomorphic to the buffer container (Chapter 15): buffer ownership transfers directly, and the builder zeroes out for reuse. The builder also offers `AppendRepeat` (repeated appending — a generator for separator lines and indentation) and formatted appending.

### Layer 3: the full-family cheat sheet

| Family | Functions | Notes |
| --- | --- | --- |
| Search | `xrtStrFind` / `CaseFind` / `RFind` | Return positions; a miss is `XRT_NPOS` |
| Byte-level | Any / Byte search | Locate any byte from a set |
| Prefix/suffix | `xrtStrStarts` / `Ends` / `CutPrefix` / `CutSuffix` | Pathway tests and view trimming, each with a Case variant |
| Editing | `Insert` / `Remove` / `Replace` | Owning products |
| Splitting | `xrtStrSplit` / `Cut` iterative | Split goes to a callback; Cut loops the slicing |
| Joining | `Join` / `Repeat` / `Dup` | Owning products |
| Case | `xrtStrUpper` / `Lower` (with To caller-buffer variants) / Case-family comparison | For case-insensitive work use the dedicated Case family |
| Trimming | `Trim` / `TrimLeft` / `TrimRight` / `Pad` / truncation | Zero allocation trimming; Pad needs a buffer |
| Metrics | `Length` / `IsEmpty` / whitespace tests | Direct reads on the view |
| Distance | `xrtUtf8Distance` / `xrtUtf8Similarity` | Edit distance counted in Unicode scalars (the Chapter 27 charset family) |
| Wildcards | `xrtStrGlob` | glob-style `*?` matching |
| Formatting | `xrtFormat` / `FormatV` | printf rules, owning products |

Two columns deserve special mention: the **Case family** (`CaseFind`, `CaseEqual`, `CaseStarts`, and friends) completes a case-insensitive comparison in one call — two allocations fewer than "ToLower both copies first, then compare"; **distance/similarity** (the charset family's `xrtUtf8Distance`/`xrtUtf8Similarity`, counting Unicode scalars) hands fuzzy matching an out-of-the-box answer — the distance example demonstrates how to read "edit distance 2, similarity 0.6".

### The division of labor with the C standard library

Why not just use `strstr`/`strdup`? Three hard reasons: the standard functions presume zero termination (binary-safe settings are disqualified outright), failure paths go unreported (`strdup` on OOM can only return NULL, no diagnostics), and product ownership is left implicit (who allocates and who frees is pure convention). The XRT string family runs entirely on `(视图, 长度)` (view, length) as currency, `bool` returns plus the error slot, owning products paired with `xrtFree` — all three are Volume 1's conventions honored at the text layer.

## Examples

### Complete program: a four-step view pipeline in one breath

From the repository sample `examples/string/basic/main.c`:

```embed path="examples/string/basic/main.c" title="examples/string/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/string/basic/main.c -lws2_32 -liphlpapi
alpha.txt
```

**What just happened.** (1) `xrtStrTrim` returns a **sub-view** of the original string — two pointers draw inward, not one original byte moves, zero allocation. (2) `xrtStrCut` splits at the first `/`: `Name` receives the front segment `alpha`; the back segment is not needed, so the second out-param takes `NULL` — unwanted products are simply not received, Cut's convenience convention. (3) `FilterTo` strips `-` and `_` from the view and writes the rest into **the caller's stack buffer** — even at the step that "needs writable memory", the allocation authority stays with the caller (insufficient capacity returns failure rather than allocating behind your back). (4) The pipeline's first and only heap allocation happens when `Concat` appends `.txt` — the product is owning, freed with `xrtFree`. Three of four steps allocation-free: the complete demonstration of "allocate as late as you can".

### Complete program: the builder and hand-over

From `examples/string/builder/main.c`:

```embed path="examples/string/builder/main.c" title="examples/string/builder/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/string/builder/main.c -lws2_32 -liphlpapi
items=ababab
```

**What just happened.** (1) `xrtStrBufInit` opens shop with a stack handle — the builder's internal buffer grows on demand (doubling strategy); between the two append batches `items=` and `ab`×3 there are no intermediates. (2) `AppendRepeat` is the dedicated entry for repeated appending — "one segment, n times" needs such as separator lines, indentation, and padding never require you to write a loop. (3) `Take` hands the buffer's **ownership** over to the caller: no copy, no sharing; the builder returns to empty and is ready for reuse — semantics fully isomorphic with Chapter 15's `xrtBufferTake`; Chapter 15's "accumulate — assemble — take" rhythm holds verbatim in the string world. (4) On the failure path `xrtStrBufFree` returns the builder — the three container conventions apply here as usual.

## Contracts

- **Views first**: trimming, splitting, and prefix/suffix tests are zero allocation; proper-substring outputs are always views.
- **Owning products**: Concat/Join/Dup/Replace/Format return `str`, freed exactly once with `xrtFree`.
- **Builder discipline**: for more than two or three segments use `xstrbuf`; after `Take` hands over, the builder zeroes out for reuse; on failure paths, `Free`.
- **The Case family**: for case-insensitive work use the dedicated `CaseFind`/`CaseEqual`, never convert-then-compare.
- **Miss semantics**: searches return `XRT_NPOS`, tested with `== XRT_NPOS` (Chapter 3's sentinel discipline).
- **Binary-safe**: every function runs on `(视图, 长度)` (view, length) currency; content may contain zero bytes.

### From examples to engineering: three host forms of strings

Parallel to the containers' three hosts (Chapter 23), strings in engineering have three typical hosts. **Request-scoped temporary text**: the view pipeline plus a stack buffer, zero allocation throughout — the toolkit for URL handling and header parsing, code that runs on every request. **Cumulative output**: a builder attached to the processing context, appending across callbacks and taking once at the end — the standard shape for log lines, report lines, and serialization output. **Long-lived stored fields**: parsed fields that must outlive the request (config entries, cache keys) go into containers as Dup/Join owning products — ownership thereafter belongs to the structure the container manages. The three hosts map to the three tiers "zero allocation, late allocation, owning", cross-referenceable at a glance against Chapter 5's memory language.

### One habit: first ask whether the output is a substring

Before writing any text function, ask: **is the output a proper substring of the input?** If yes — return a view, zero allocation (the world of Trim/Cut/Slice); if not but the content is bounded — write into the caller's buffer (FilterTo/To family); if the length is open-ended — an owning product or the builder. This three-question habit blocks most of the waste of "casual Dup" and most of the accidents of "returning a dangling view" — the single most takeaway-worthy sentence of this chapter.

## Pitfalls

### Pitfall 1: concatenating many segments with a Concat chain

Symptom: functions that assemble URLs, SQL, or log lines get slower and slower on long inputs; the profiler shows hotspots in memcpy and the allocator.

Cause: an n-segment Concat copies all existing content each time, for O(n²) total copying; the intermediate strings' allocations and frees each happen n times besides.

```c bad
str s = xrtStrDupView(Prefix);
s = xrtStrConcat((xstrview){ s, strlen(s) }, Host);   /* copies all existing content */
s = xrtStrConcat((xstrview){ s, strlen(s) }, Path);   /* copies it all again */
s = xrtStrConcat((xstrview){ s, strlen(s) }, Query);  /* and again - O(n²) is already locked in */
```

```c good
xstrbuf tBuf;
xrtStrBufInit(&tBuf);
xrtStrBufAppend(&tBuf, Prefix);
xrtStrBufAppend(&tBuf, Host);
xrtStrBufAppend(&tBuf, Path);
xrtStrBufAppend(&tBuf, Query);
str s = xrtStrBufTake(&tBuf);     /* one hand-over, O(n) total copying */
```

### Pitfall 2: a view stored past its source's lifecycle

Symptom: the same family as Chapter 3, Pitfall 3 — the "processed result" a function returns occasionally turns to garbage or an empty segment at the caller.

Cause: the views Trim/Cut produces are borrowings of the original memory; when such a view is stored into a struct, returned to an upper layer, or passed across threads, the source may already be freed or reused.

```c bad
xstrview getName(const char* sRaw)
{
	xstrview Text = xrtStrTrim((xstrview){ sRaw, strlen(sRaw) });
	xstrview Name;
	xrtStrCut(Text, XRT_STR_LITERAL("/"), &Name, NULL);
	return Name;   /* Name points inside sRaw - dangling once the caller replaces sRaw */
}
```

```c good
str getName(const char* sRaw)
{
	xstrview Text = xrtStrTrim((xstrview){ sRaw, strlen(sRaw) });
	xstrview Name;
	xrtStrCut(Text, XRT_STR_LITERAL("/"), &Name, NULL);
	return xrtStrDupView(Name);   /* to live long, own a copy yourself */
}
```

## Exercises

### Basic: reproduce the pipeline

Reproduce the main example's four-step pipeline and output `alpha.txt`; then change the input to `"  x/y/z  "` (two separators), work out on paper which segment Cut takes, and run to verify.

### Advanced: a query-string parser

Using a Cut loop plus Find plus CaseEqual, parse `?a=1&b=2&c=3`: slice out each key-value pair, match key names case-insensitively, strip `%` from values — allocating exactly once, at the final output assembly. Hint: feeding Cut's back-segment view into Cut again is iterative splitting.

### Challenge: a structured log-line assembler

Implement a structured log line with the builder: timestamp, level, module, message, and several key-value pairs (values may be strings or integers). Requirements: the append chain has no intermediates; field separators and indentation use `AppendRepeat`; the whole line is taken with a single `Take`. Acceptance criteria: assembling 1000 log lines costs a number of allocations on the order of the line count (not the field-count product); the output format is fixed and assertable.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| View pipeline | Trim/Cut/prefix-suffix at zero allocation; allocation deferred to Concat |
| Builder | For more than two or three segments use `xstrbuf`; `Take` hands over for reuse; O(n) total copying |
| Search | Find/CaseFind/RFind; a miss is `XRT_NPOS`; test with `== XRT_NPOS` |
| Editing | Insert/Remove/Replace, owning products |
| Splitting | Cut slices iteratively / SplitInit+SplitNext walks items cursor-style |
| Case | Dedicated Case-family functions; never convert-then-compare |
| Fuzzy | xrtUtf8Distance edit distance / similarity; glob wildcarding with xrtStrGlob |
| Ownership | Views borrow without owning; owning products freed exactly once with `xrtFree` |
