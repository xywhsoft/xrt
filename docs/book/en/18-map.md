---
num: 18
slug: map
title: Hash Maps: xmap and xintmap
volume: 卷三 容器与数据结构
type: practice
lead: Two map families — byte keys and integer keys — with GetOrAdd taking a slot in one step, and two iteration orders: insertion order and key order.
api: map
---

## Orientation

Maps solve "find the value by key": a routing table finds statistics by path, a session table finds state by ID, a cache finds entries by key. XRT provides two map families: **`xmap`** keys are arbitrary byte strings (`xbytesview`, embedded zeros included), iterated in **insertion order**; **`xintmap`** keys are native `int64` (no hashing at all), iterated in **ascending key order** — and sparse keys make it the fastest "sparse array" around. The two share one usage rhythm — this chapter's protagonist is `GetOrAdd`: "find or create" in a single step, returning a pointer to a **zero-initialized value slot**; counting scenarios just `++`. This is the high-frequency container second only to the array, and the core cell of Chapter 23's selection table.

## Introduction

Count route hits of an HTTP service: each request arrives, finds that route's counter by path and increments it; a path never seen before gets a new counter. Written as "first check existence, then insert if absent, then initialize after inserting", every link has a failure path and two-thirds of the code is fingernail-sized defense. Worse is "searching twice" — `Has` looks once, `Add` locates again internally, and the hot path burns double the hashing for nothing.

`GetOrAdd` fuses the three steps into one: if the key exists it returns the existing slot; if not it **inserts a zero-initialized new slot** and returns it — an out-parameter tells you whether this was a creation. The counting scenario becomes two lines: take the slot, increment. This is the intuitive case of "the API's shape decides the code's shape" — a good interface makes the correct form the shortest form.

## Concepts

### Two map families, two key kinds

| Dimension | `xmap` byte keys | `xintmap` integer keys |
| --- | --- | --- |
| Key type | `xbytesview` (arbitrary binary) | `int64` (native) |
| Key computation | hashing (SipHash family) | direct bit mapping, zero computation |
| Iteration order | **insertion order** (an internal order chain) | **ascending key order** |
| Typical scenarios | string keys, binary keys, configs, encodings and reports | session IDs, sparse indices, export by ID |

In the byte-key world "paths, field names, fingerprints" are all legal keys — `xbytesview` does not require zero termination; Chapter 3's binary-safe convention holds for keys too. Integer keys skip hashing and locate directly; with small contiguous keys it is the fastest possible sparse array; iteration is ascending by key — "export all sessions in ID order" needs no sorting step.

### Values inlined: zero per-entry allocation

Consistent with `xarray`'s value semantics, the map's **values are stored inline in the slots** — `Init` only needs the value size. Inserting ten thousand route statistics means container-level allocations (bucket-array growth), not ten thousand of them. The values' lifetime follows the container: one `Unit` returns everything. Want to store large objects or objects owned elsewhere (the linked list of Chapter 17 holds a lesson here too)? Make the value type a pointer (`sizeof(ptr)`) and pair it with Chapter 17's slot_map handles — the "map locates + handle fetches" combination reappears in full in Chapter 66's connection table.

### Iteration's three-part contract

```diagram flow
- IterBegin: takes the order snapshot and acquires a reference (no container edits during iteration)
- IterNext: steps once, yielding the key view and value pointer; NULL at the end
- IterEnd: releases the snapshot reference - all three parts are mandatory
```

Iteration yields the key as a **borrowed view** (borrowing) (`xbytesview`) and the value as an **in-slot pointer** — both are valid only for the duration of the iteration (no guarantee after the next `Next` or `End`). "No container edits during iteration" is the snapshot discipline: inserts and deletes invalidate the snapshot; if you must traverse and edit, first collect the pending items into an array (Chapter 14) and edit after the iteration ends.

### Duplicate keys and failure semantics

`xmap`'s insertion family returns the **existing slot** when the key already exists (no overwrite, no error) — "insert-as-take-slot" behaves like `GetOrAdd` minus the created flag. Deletion proceeds by key; a missing key fails. All failure paths leave the container untouched (the second and third of the container's three conventions from Chapter 14 apply here as usual).

## Examples

### Complete program: route statistics and insertion-order iteration

From the repository example `examples/containers/map/main.c`:

```embed path="examples/containers/map/main.c" title="examples/containers/map/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/map/main.c -lws2_32 -liphlpapi
/health requests=1 status=200
/metrics requests=8 status=204
```

**What just happened.** (1) `xrtMapInit(&tRoutes, sizeof(routestat))` passes only the value size — the whole `routestat` struct sits inline in the slot, not a pointer. (2) `GetOrAdd("/health")` on first access: inserts a zero-initialized slot (`bNew=true`), and after the slot pointer returns you `Requests++` and write the status **directly** — the new-slot-already-zeroed convention removes the three initialization steps. The second route likewise settles in two lines. (3) The iteration outputs in **insertion order** `/health`, `/metrics` — not hash-value chaos. This property comes from the internally maintained order chain, and its value is stability: config exports, report generation, and test assertions all reproduce exactly. (4) `IterNext` yields the key view and value pointer together; printing the key with `%.*s` is Chapter 3's posture. (5) `IterEnd` releases the snapshot reference and `Unit` returns the container — the three-part iteration plus container cleanup, none missing.

### Complete program: integer keys and key-order iteration

From `examples/containers/int_map/main.c`, with session IDs from negative to the millions:

```embed path="examples/containers/int_map/main.c" title="examples/containers/int_map/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/int_map/main.c -lws2_32 -liphlpapi
session=-9 requests=3 authenticated=no
session=1000001 requests=1 authenticated=yes
```

**What just happened.** (1) Keys `-9` and `1000001` are a million apart — the integer-key map is completely insensitive to sparsity; this is the "sparse array" usage: indices need not be contiguous, memory scales with actual entry count. (2) The output is in **ascending key order**: `-9` before `1000001`, the reverse of insertion order — this is its essential division of labor from `xmap`'s insertion order; when selecting, first ask "is the traversal order I want the business order (insertion) or the key order (sorted)". (3) `GetOrAdd`'s rhythm is identical to the byte-key version — learn one rhythm and you can drive both map families.

### Performance shape and capacity

The hash map's complexity portrait is tidy: single-key operations amortized O(1) (hash computation + bucket location), iteration O(n) with stable order. The capacity strategy matches Chapter 14's three-piece set: `Reserve` pre-grows to avoid rehashing during loading, `Trim` tightens after turning read-only. Two practical details: first, key hashing happens inside the container (byte keys go through the SipHash family, the key held by the container itself) — callers need not precompute hashes; what you pass is the key itself. Second, `xintmap`'s key-order iteration means "exporting a session list sorted by ID" needs no sorting step — at log-scale data volumes this saving is often more considerable than the lookups themselves. Put the two families into Chapter 23's selection table: for by-key location either works; want insertion order choose `xmap`, want key order and the last word in performance choose `xintmap`.

### From examples to engineering: the map's three host forms

Where the map handle lives decides its engineering role. **In-function ad-hoc statistics**: a stack handle + `Unit` at the end — one-shot tasks like word counting get a two-line lifecycle. **Long-lived tables**: hung on a service object — `Init` at service start, `Unit` at shutdown; routing tables and session tables are this form — note it lives across requests, and the iteration discipline (three parts, no inserts/deletes) must be understood under the external synchronous coordination of the concurrency chapters (Volume 6). **Slots holding pointers**: declare the value type as `sizeof(ptr)` and put an object pointer or a Chapter 17 slot_map handle value in the slot — in the "map locates, handle fetches" composite, the map answers only the "key to handle" hop while the objects themselves live in a pool or slot_map. The three forms cover the vast majority of the map's engineering uses; Chapter 66's connection table is the third form's concurrent, scaled-up edition.

## Contracts

- **GetOrAdd semantics**: if present, returns the existing slot; if absent, inserts a **zero-initialized** slot and returns it; the out-parameter `bNew` distinguishes the two cases.
- **Values inlined**: values live in the slots, one `Unit` returns all; when the value type is `ptr`, object ownership stays with the caller.
- **Three-part iteration**: `Begin`/`Next`/`End`, none optional; no inserts/deletes during iteration; key views and value pointers expire with the iteration.
- **Order promises**: `xmap` insertion order, `xintmap` ascending keys — outputs reproduce exactly and can go into test assertions.
- **Binary-safe keys**: byte keys may contain embedded zeros; length is per the view, not the zero terminator.
- **Host forms**: ad-hoc statistics (stack handle), long-lived tables (on a service object), key-to-handle (values hold `ptr`) — each form has its own cleanup discipline.

### A frequently asked question: why is there no "thread-safe map"

None of Volume 3's containers are thread-safe — not an omission, a layering decision. Locking inside the container means every `GetOrAdd` passes a lock; single-threaded programs pay synchronization for nothing. And the correct granularity of a multithreaded program is usually not the single container operation — "lookup + decide + write back" needs outer consistency, which a container-level lock cannot guarantee. XRT therefore leaves synchronization to Volume 6's primitives: reader-heavy tables get a rwlock around the whole table, cross-thread handoff goes through a channel, and true high concurrency uses sharding into a fragment per thread (one map per thread, periodic reassembly merges). Chapter 23's selection table will flag this again in its "threads" column — pick containers without concurrency in mind, write concurrency without expecting it from containers.

## Pitfalls

### Pitfall 1: saving the pointers iteration hands out

Symptom: using a saved key or value pointer after iteration reads stale or dangling content; occasionally fine at small data volumes.

Cause: iteration yields in-slot addresses and borrowed views; the next `Next`/`End` or any container edit can invalidate them.

```c bad
routestat* pSaved = NULL;
xbytesview KeySaved;
xrtMapIterBegin(&tRoutes, &tIterator);
while ( (pStat = xrtMapIterNext(&tIterator, &Path)) != NULL ) {
	pSaved = pStat;      /* saving an iteration-period pointer */
	KeySaved = Path;
}
xrtMapIterEnd(&tIterator);
pSaved->Requests++;      /* iteration has ended - the pointer's validity has too */
```

```c good
xrtMapIterBegin(&tRoutes, &tIterator);
while ( (pStat = xrtMapIterNext(&tIterator, &Path)) != NULL ) {
	Process(&Path, pStat);   /* use and discard within the iteration */
}
xrtMapIterEnd(&tIterator);
/* need long-term storage: copy keys/values into an array during iteration, process the array after */
```

### Pitfall 2: inserting or deleting during iteration

Symptom: crashes, skipped or duplicated entries; reproduction depends on entry counts and operation timing — nearly impossible to reproduce stably.

Cause: iteration stands on the order snapshot; inserts and deletes reorganize the internals, and the snapshot decouples from the real structure.

```c bad
xrtMapIterBegin(&tRoutes, &tIterator);
while ( (pStat = xrtMapIterNext(&tIterator, &Path)) != NULL ) {
	if ( pStat->Requests == 0 ) {
		xrtMapRemove(&tRoutes, Path);   /* deleting from the map mid-iteration */
	}
}
```

```c good
xarray tDead;
xrtArrayInit(&tDead, sizeof(xbytesview));
xrtMapIterBegin(&tRoutes, &tIterator);
while ( (pStat = xrtMapIterNext(&tIterator, &Path)) != NULL ) {
	if ( pStat->Requests == 0 ) {
		xrtArrayPush(&tDead, &Path);    /* collect keys first (note: key storage must outlive the deletions) */
	}
}
xrtMapIterEnd(&tIterator);
for ( size_t i = 0; i < tDead.Count; i++ ) {
	xrtMapRemove(&tRoutes, *(xbytesview*)xrtArrayGet(&tDead, i));
}
xrtArrayUnit(&tDead);
```

Note: the good version collects key views — the view's source memory must live until the deletions complete; literal or external-buffer keys satisfy this naturally, while container-held keys should be collected as copies (e.g. `xrtTempDup` into an arena).

## Exercises

### Basic: word-frequency counting

Read a passage of English (a hardcoded string is fine), count word frequencies with words as keys, and output all entries in insertion order. Hint: split on spaces, count with `GetOrAdd`; write the expected entry order on paper (insertion order) before comparing with the program.

### Advanced: the two map families side by side

Load the same session data (IDs from -5 to 100) into `xmap` (keys as 8-byte binary views) and `xintmap`, and observe the iteration-order difference; then use Chapter 12's `Hash64` to fingerprint the "key byte strings" for an intuition of the cost versus direct integer keys.

### Challenge: the complete LRU (map + list + capacity eviction)

Implement a capacity-capped LRU: `xmap` holds "key → entry", each entry embeds an `xlistnode` (Chapter 17) hung on the recency chain; a get hit does `MoveFront`, a put past capacity evicts the tail and removes it from the map. Acceptance criteria: with capacity 3 and the access sequence 1/2/3/1/4, the evicted one is 2; after eviction the map's entry count matches the chain's node count; zero leaks.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two families | `xmap` byte keys, insertion order / `xintmap` integer keys, ascending keys |
| Taking a slot | `GetOrAdd` finds-or-creates in one step; new slots zero-initialized; `bNew` out-parameter |
| Values inlined | values live in the slots, `Unit` returns all; store objects as `ptr` values + handles |
| Three-part iteration | `Begin`/`Next`/`End`; iteration-period pointers stay in the iteration; no edits mid-iteration |
| Binary keys | `xbytesview` with zero bytes is legal; length is authoritative |
| Sparse array | with small contiguous keys `xintmap` is the fastest sparse array |
| Capacity | `Reserve`/`Trim` same names and meanings as the three-piece set; hashing done inside the container |
| Selection rule | export in insertion order → `xmap`; export in key order or integer keys → `xintmap` |
