---
num: 7
slug: temp
title: Temporary Memory: Arenas and Scopes
volume: 卷一 起步与核心
type: practice
lead: Allocation is a pointer bump, freeing is a no-op, reclamation returns the whole range — the standard memory posture for parsing and request handling.
api: temp, memory
---

## Orientation

Chapter 5 settled "memory held long-term"; this chapter settles its dual: **large numbers of short-lived objects**. Parsing one frame of JSON produces hundreds of small fragments; handling one HTTP request assembles dozens of temporary strings — per-block `xrtMalloc`/`xrtFree` is not only slow, worse, the failure path must clean up block by block and the code explodes. XRT's answer is the arena: one stretch of memory allocated wholesale; "allocating" inside it is just a pointer bump, "freeing" does nothing, "reclaiming" returns the whole stretch. The library's parsers, protocol stacks, and coroutine schedulers all build arenas in — understand this chapter and you understand the memory posture of XRT's data plane.

## Introduction

Anyone who has hand-written a parser remembers this: three levels into a recursive descent, already holding a dozen temporary buffers; any level failing means freeing the outer levels' buffers one by one — a chain of `Free`s before every `return`; missing one is a leak, freeing an extra one is a crash. And these objects' lifetimes are actually highly uniform: **they all live until "this frame of parsing ends"**. Paying per-block management cost for a uniform lifetime is pure waste.

The arena turns that observation into mechanism: since they are "born together, die together", let them be "allocated together, reclaimed together". Management cost drops from O(allocations) to O(scopes); the failure path's cleanup shrinks from a chain of `Free`s to a single `End`. There is also a performance ledger: per-block allocation passes through an allocator (even Chapter 22's memory pool has bookkeeping cost), while an arena allocation is one pointer addition — on data-plane hot paths with millions of temporary allocations, the difference is order-of-magnitude.

## Concepts

### The mental model: three verbs

```diagram flow
- Allocate: bump the pointer, return the range start — zero metadata, zero fragmentation
- Free: do nothing — intermediate state is never returned per-block
- Reclaim: return the whole stretch to the bookmark — once per scope
```

No per-block freeing means no free-block fragmentation; no per-block bookkeeping means allocation costs approach array addressing. The price is equally clear: **a single object cannot be returned early** — the arena reclaims only by scope. Whether a scenario fits an arena is judged by lifetime uniformity: parsing a frame, handling a request, rendering a screen are uniform; caches, configuration, sessions — long-lived objects — are not, and remain with Chapter 5's owning allocation.

A division-of-labor line running through the book takes shape: **owning manages "explicitly wanted long-lived"; the arena manages "this round's processing"**. You will see this pair in every later volume: the JSON parser builds its temporary tree in an arena (Chapter 31), network requests assemble responses in an arena (Volume 7), coroutines carry a built-in arena (Chapter 55) — the arena is XRT's default data-plane posture, and owning is the exception, not the rule. When writing code, first ask "can this memory retire with the scope?" — if yes, use the arena before considering anything else.

### Scopes: bookmarks and nesting

`xrtTempBegin` drops a bookmark (`xtempmark`) at the current watermark; subsequent allocations land past it; `xrtTempEnd` returns the watermark wholesale to the bookmark. Bookmarks nest — the inner one rewinds first, the outer later — naturally matching the shape of recursive descent and the call stack.

### Promotion: letting the result outlive the scope

The arena's classic puzzle: the **result** assembled in a sub-scope (a formatted log line, say) must survive, while the **process quantities** (intermediate buffers, temporary fragments) should all be returned. `xrtTempEndStr` / `xrtTempEndDup` copy the designated content to the parent before reclaiming the range — the result survives, everything else is returned, both in one step. This is the escape hatch against "arenas cannot selectively keep", and it is the star of this chapter's examples.

### The default arena and explicit arenas

Two usage routes. **The default arena**: every thread/coroutine carries one (a coroutine's arena is created and reclaimed with it — see Volume 6); `xrtTemp` allocates straight from the current context's arena and `xrtTempClear` empties it — suited to "grabbing a block of scratch memory inside a function". **Explicit arenas** (`xtemparena`): `xrtTempInit` creates and `xrtTempUnit` destroys, embedded into request objects, parsers, frame loops — structures that "have a lifecycle of their own". An explicit arena's configuration is a triple: regular block size, retained watermark (the memory kept on hand after `Reset`), and a total cap — allocations past the cap fail, preventing a runaway request from eating the process. For scenarios where keys/tokens have lived in the arena, wind down with the `Secure` family (`xrtTempSecureReset` / `xrtTempSecureUnit`): securely wipe the user-area bytes before reclaiming, continuing the semantics of Chapter 5's `xrtSecureZero`.

The selection criterion between the routes is "who owns this batch of temporary memory's lifecycle". A thread that clears wholesale once per round — the default arena is easiest; a request object with its own entry and exit (possibly pooled and reused) gets an explicit arena — `Reset` after handling to keep the watermark, `Unit` at object destruction to return everything. The two arenas' API shapes are identical (the full `Alloc`/`Dup`/`Begin`/`End` family), so switching routes does not restructure business code.

## Examples

### First program: a minimal sample of scopes and promotion

First a 20-line minimal sample, stating the three things "bookmark — reclaim — promote" each on its own:

```c
/* temp_mini.c —— scopes and promotion on the default arena */
#define XRT_MODULE_TEMP_MEMORY
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	xtemparena* pArena = xrtTempCurrent();
	xtempmark tScope = xrtTempBegin(pArena);
	char* sTemp = (char*)xrtTemp(16);
	char* sKeep;

	memcpy(sTemp, "scoped", 7);
	/* promotion: copy the result to the parent watermark before reclaiming */
	sKeep = xrtTempEndStr(&tScope, (xstrview){ sTemp, 7 });
	printf("kept=%s\n", sKeep != NULL ? sKeep : "(null)");
	xrtTempClear();
	return 0;
}
```

```term
$ gcc -O1 -DXRT_MODULE_TEMP_MEMORY -DXRT_IMPLEMENTATION -I single temp_mini.c -lws2_32 -liphlpapi
$ ./a.exe
kept=scoped
```

**What just happened.** After `Begin`, `sTemp` lives inside the bookmarked range; `EndStr` copies the 7 bytes to the parent watermark before rewinding and returns the new pointer — `sTemp` is dead from then on, `sKeep` lives until `Clear`. Five short lines walk both core disciplines of the arena: process quantities retire with the scope; results survive via promotion.

### Second complete program: scopes, promotion, and an explicit arena

From the repository example `examples/memory/temp/main.c`, walking in one go the default arena's scopes and promotion and an explicit arena's creation and secure wind-down:

```embed path="examples/memory/temp/main.c" title="examples/memory/temp/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/memory/temp/main.c -lws2_32 -liphlpapi
outer=outer inner=inner
outer_after_scope=outer
promoted=promoted
blocks=1 retained=1024 current=96 peak=96
```

**What just happened.** (1) `xrtTempCurrent` fetches the current thread's default arena; `xrtTemp(32)` allocates at the parent watermark — it belongs to no bookmark range and stays valid until `Clear`. (2) After `Begin` drops a bookmark, `inner` is allocated; printing confirms the two blocks coexist; `End` reclaims the whole range. (3) Print `outer` again: intact — the bookmark only rewinds memory allocated after itself; parent-level allocations are untouched — that is the safety guarantee of "nested scopes". (4) The second scope demonstrates **promotion**: `xrtTempStr` assembles `"promoted"`, and `xrtTempEndStr` copies it into the parent before reclaiming — the third output line proves the result survives while other allocations in the range (if any) are all returned. (5) The explicit-arena section: created with the triple `{1024, 512, 2048}`, walking `Alloc`/`Dup`/`EndDup` once each, with `xrtTempGet` reading back statistics — `blocks=1` means all allocations landed in one regular block, `retained=1024` is the block holding, and `current=96` with `peak=96` are the current and peak watermarks. (6) Finally `SecureReset` → `Reset` → `Trim(0)` → `SecureUnit`: secure wipe, regular reclaim, return of all retained blocks, destruction — the explicit arena's complete wind-down sequence.

A detail worth noting: after `End` the `sInner` pointer still "exists" (C pointers don't vanish), but the range it points into has been reclaimed — using it further is a dangling access, which Pitfall 1 covers. Note also that `Clear` appears only at the end of the default-arena section: **whoever manages the scope boundary does the reclaiming** — no mid-function clearing, and not clearing to the end is fine too (the default arena is reclaimed wholesale at thread exit), but long-running loops must beware accumulation — see Pitfall 2.

## Contracts

- **Whoever resets is responsible**: scopes are managed in `Begin`/`End` pairs; the default arena's `Clear` is called by the paired owner of "this round of processing"; business functions do not `Clear` internally.
- **Bookmarks mind their own business**: `End` rewinds to the bookmark and never touches earlier allocations; nesting rewinds last-in-first-out.
- **Promotion is copying**: the results of `EndStr`/`EndDup` belong to the parent watermark, unrelated to the original range; the copy completes before returning — no dangling window.
- **The cap is the defense line**: exceeding the triple's cap fails the allocation (Chapter 4's `XERR_MEMORY` kind) — like resource limits, it is business spec expressed in code.
- **Sensitive data**: if keys have lived in the arena, wind down with the `Secure` family; a plain `Reset` does not wipe the user-area bytes.

## Pitfalls

### Pitfall 1: storing an in-scope allocation outside the scope

Symptoms: a temporary pointer handed out across functions, whose content "randomly changes" or gets overwritten by later allocations when the caller uses it; reproduction depends on the subsequent allocation sequence.

Cause: `End` reclaimed the range; any later arena allocation may reuse that memory — the pointer still points at a legal address, but the content is no longer yours.

```c bad
const char* build_label(void)
{
	xtempmark tScope = xrtTempBegin(xrtTempCurrent());
	char* s = (char*)xrtTemp(32);
	fill(s);
	xrtTempEnd(&tScope);
	return s;   /* the range was reclaimed on return: the caller gets memory awaiting reuse */
}
```

```c good
const char* build_label(void)
{
	xtempmark tScope = xrtTempBegin(xrtTempCurrent());
	char* s = (char*)xrtTemp(32);
	fill(s);
	/* promote to the parent: copy then reclaim; the return value survives */
	return xrtTempEndStr(&tScope, (xstrview){ s, strlen(s) + 1 });
}
```

### Pitfall 2: never clearing the default arena in a long-running loop

Symptoms: a resident service thread's memory creeps up; `xrtTempGet` shows `current` only ever growing.

Cause: the default arena's design expectation is "someone `Clear`s each round" — if every round only allocates and never clears, the arena keeps expanding to its cap (when explicitly configured) or keeps growing (when the default cap is generous).

```c bad
while ( running ) {
	char* sLine = (char*)xrtTemp(4096);   /* allocate every round, never Clear */
	process(sLine);
}
```

```c good
while ( running ) {
	xtempmark tScope = xrtTempBegin(xrtTempCurrent());
	char* sLine = (char*)xrtTemp(4096);
	process(sLine);
	xrtTempEnd(&tScope);                  /* rewind wholesale each round: watermark stays flat */
}
```

## Exercises

### Basic: three-part scope verification

Following the example, produce three output parts: two blocks coexisting in scope, the parent surviving after `End`, and a promoted result surviving. Three output lines matching the example verbatim completes it.

### Advanced: arena-ifying a recursive descent

Write a recursive function parsing parenthesized expressions (like `((a)(b))`): each recursion level does `Begin`/`End`, storing its own fragments with `xrtTemp` inside the level, and the innermost result is promoted level by level to the top with `EndStr`. Hint: promotion happens "before this level returns"; recursion's natural shape is nested bookmarks.

### Challenge: cap testing for an explicit arena

Create an explicit arena with the triple `{1024, 512, 2048}` and verify with Chapter 6's stats or `xrtTempGet`: allocating near the cap, further allocation returns `NULL` with `XERR_MEMORY` in the thread error slot; after `Reset`, allocation capability returns; after `Trim(0)`, `retained` reaches zero. Acceptance: all three assertions print `ok`.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Mental model | allocate = pointer bump; free = no-op; reclaim = return the whole stretch; management cost O(scopes) |
| Scope | `Begin` drops a bookmark, `End` rewinds; nesting is last-in-first-out |
| Promotion | `EndStr` / `EndDup`: results copy to the parent, process quantities are returned |
| Default arena | `xrtTempCurrent` / `xrtTemp` / `xrtTempClear`; coroutines carry one |
| Explicit arena | `Init`/`Unit` embed into objects; triple (block/watermark/cap) |
| Secure wind-down | `SecureReset` / `SecureUnit` wipe before reclaiming |
| Fitness test | uniform lifetimes use the arena; long-lived objects use Chapter 5's owning style |
| Division mnemonic | the arena manages this round's memory; owning manages the explicitly long-lived |
