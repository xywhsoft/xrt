---
num: 19
slug: set
title: Sets: xset
volume: 卷三 容器与数据结构
type: practice
lead: Value deduplication, insertion-order iteration, and set algebra — the standard tool for whitelists, tag groups, and permission differences.
api: set
---

## Orientation

`xset` is the container that "only asks whether it's in there": adds deduplicate, membership checks are O(1), and union/intersection/difference/symmetric difference compute in one step. It shares the hash foundation and insertion-order iteration with `xmap`, but the semantic center of gravity differs — the map cares about "what the key maps to", the set cares about "whether the key belongs". Firewall port whitelists, user tag groups, permission differences, deduplicated counting of encodings — all set problems. This chapter nails the difference between two API shapes — **`Unit` for stack handles** versus **`Destroy` for operation results** — the discipline most worth establishing first in a set chapter.

## Introduction

A gateway must decide which ports to allow: the administrator enabled `{80, 443, 8080}`, the application requests `{3000, 443, 8080}` — the allowed set is the **intersection** `{443, 8080}`, and this chapter's main example implements exactly this scene. With arrays: two nested loops deduplicating, a dozen-plus lines, quadratic complexity, and "deduplication" itself a bug magnet. With sets: `Add` to load, `Intersection` in one step, iterate and print — three segments of two or three lines each. More importantly the semantics are explicit: what appears in the code is the word "intersection" itself; a reviewer need not reverse-engineer intent out of nested loops.

The return shape of set operations deserves particular attention: `Intersection` / `Union` / `Difference` / `SymmetricDifference` return a **heap-allocated new set** (an `xset*` pointer) — the result lives independently of its operands; you can keep operating on it or iterating it; the price is that it must be `xrtSetDestroy`-ed when done. Sets used "for loading" are stack handles finished with `xrtSetUnit`. Two object kinds, two finishes — mixing them up is a leak or a dangling reference; pitfall 1 polices exactly this, and the example's cleanup segment — both finishes side by side — is the canonical form worth copying wholesale.

## Concepts

First a framing: all of the set's concepts unfold around three questions — how elements enter (Add and value semantics), how membership is judged (Has and byte equality), how they combine (the algebra and the two finishes). The Concepts section unfolds along these three questions, the Examples section gives one complete answer each, and the Pitfalls section enforces the two most crash-prone disciplines. Looking back after reading you will find: half of this chapter's knowledge actually came from Chapter 18 — precisely the compound interest of the container family's shared conventions.

### Isomorphism with the map

The set's foundation is the same stock as `xmap`: hash location, insertion-order chain, three-part iteration (`Begin`/`Next`/`End`) all identical. Only two differences: there is no value (the element is the key), and there is a family of set-algebra operations. Coming from Chapter 18, you only need the incremental "algebra" block — the two chapters share most of their pitfalls and contracts (iteration-pointer validity, no edits mid-iteration).

### Set algebra

| Operation | Result | Typical question |
| --- | --- | --- |
| `Intersection` | elements in both | requested ∩ enabled = actually allowed |
| `Union` | the union of both | merging two tag groups |
| `Difference` | left has, right lacks | configured − done = to-do |
| `SymmetricDifference` | in exactly one | diffing two configurations |

The shared contract of all four: they **return newly allocated heap sets**, and the iteration order follows the **left operand**'s insertion order. Results can chain into the next operation — a "union first, then intersect" multi-step policy writes naturally as one operation per line.

### The data flow of set operations

One policy computation is usually several algebras in series; drawing the data flow is more intuitive than reading code:

```diagram flow
- Loading: two groups of raw IDs each Add into sets (deduplication comes free)
- Intersection: requested ∩ enabled - only what both sides nod to gets through
- Difference: candidates - blacklist - excluded further from the intersection
- Output: iterate the final set along the left operand's insertion order, reproducible line by line
```

Note that every step's product is an independent heap set: intermediate results get `Destroy`-ed as soon as they're used, the final result after iteration gets its `Destroy`. Cleanup for chained operations is best written alongside the operations (evaluate-and-clean), avoiding the "compute everything first, clean up in one batch" long list — the latter leaks more easily on error paths.

### Value semantics and comparison

Elements are **values**: `Add` copies the element bytes into the slot, `sizeof(元素类型)` (element type) declared at `Init`. Element equality is byte equality — structs with padding must be zeroed before entering the set (otherwise the same value with different padding is judged unequal). Storing pointers works, but the set compares pointer values, not the objects pointed to — an "object-identity set" and an "object-value set" are different things; when you need the latter, use the business key (say an ID) as the element.

### Iteration and queries

`Has` judges membership, `Remove` deletes an element, `Count` is a public field — the container's three conventions (Chapter 14) apply verbatim. Iteration outputs in insertion order, exactly reproducible; the `set_tour` sample also demonstrates reverse iteration and visitor-style traversal (callback form), both of the same stock as the forward three-parter.

## Examples

### Complete program: port-whitelist intersection

From the repository example `examples/containers/set/main.c`:

```embed path="examples/containers/set/main.c" title="examples/containers/set/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/set/main.c -lws2_32 -liphlpapi
allowed port: 443
allowed port: 8080
```

**What just happened.** (1) Two stack-handle sets each `Init(sizeof(int))`, loaded by `Add` loops — 3000 is in the requested set but not in the enabled set; its fate was sealed at the intersection. (2) `Intersection(&tRequested, &tEnabled)` returns the heap set `pAllowed` — note it is a pointer, a different object kind from the stack handle `tRequested`. (3) The iteration outputs 443, 8080, ordered along the **left operand**'s (requested set's) insertion order — the result order reproduces and can go into test assertions. (4) The cleanup is this chapter's discipline scene: `pAllowed` is an operation result, `Destroy`; `tRequested`/`tEnabled` are stack handles, `Unit` — neither of the two finishes may be misplaced, and the failure-path `goto cleanup` carries both.

### Complete program: a full-interface tour of the set

From `examples/containers/set_tour/main.c`, covering lifecycle, queries, algebra, and two traversal styles:

```embed path="examples/containers/set_tour/main.c" title="examples/containers/set_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/containers/set_tour/main.c -lws2_32 -liphlpapi
set: lifecycle count=2 cap>=2 trim=1 clear=0
set: queries get-or-add new=0/1 has=1/0 remove=1
set: algebra union=4 diff=2 sym=2 subset/super/disjoint
set: rbegin order=3..1 visit=stopped
```

**What just happened.** (1) The lifecycle line: the public `Count` field, capacity `Reserve`/`Trim` (Chapter 14's capacity three-piece set, same names and meanings on sets), `Clear` back to zero. (2) The queries line: sets also have a `GetOrAdd` shape (confirm existence and distinguish old from new), with `Has` and `Remove` verified as success/failure pairs. (3) The algebra line: result counts of union, difference, and symmetric difference asserted one by one, plus subset/superset/disjoint judgments — relation checks allocate no new set and return booleans directly. (4) The traversal line: reverse iteration (`3..1`) and callback-style visiting (stoppable midway — `visit=stopped` proves the abort semantics of the callback returning false) — two variants of the three-parter, take as the scene demands.

### From examples to engineering: the set's three landing points

The port whitelist is the set's "security" landing point; the other two are equally frequent. **Tags and groupings**: a group of users carries several tags, one tag covers several users — "users with both tag A and tag B" is an intersection, "A without B" is a difference; a tag system is at heart a combination punch of set operations. **State-machine legal-transition tables**: make "current state × input → legal" pairs into a set, and the pre-transition legality check is one `Has` — illegal transitions are tabulated at design time instead of scattered through if chains. The three landing points share this chapter's API; they differ only in element type and operation combinations — the set is Volume 3's most "business-flavored" container, and choosing it is often not for performance but so that the business vocabulary itself — intersection, union, difference — appears in the code.

## Contracts

- **Two finishes**: stack handles `Unit`; operation results and `Create` products `Destroy` — which finish a set takes depends on how it was born.
- **Result independence**: intersection and friends do not depend on their operands' survival; the result stays valid after the operands are destroyed.
- **Result order**: algebra results follow the left operand's insertion order; all iteration outputs reproduce.
- **Value equality**: byte equality is element equality; zero-pad structs first; pointer elements compare pointer values.
- **Iteration discipline**: same as `xmap` — three parts, iteration-period pointers stay in the iteration, no edits mid-iteration.
- **Engineering landing points**: whitelists/tag groupings/transition tables — set code contains the business vocabulary itself.

### A habit for reading set code

Set operations shorten code but make "intermediate products" invisible — three operations chained on one line mean three temporary sets alive on the heap at once. Build the habit of reading set code: on seeing `Intersection` silently say "one heap set"; on seeing a chain, count the intermediates. Pair it with Chapter 6's stats for a checkup: run set-dense logic and watch whether the live-bytes curve is a "sawtooth" (allocation-release pairs) rather than a "staircase" (up only) — sawtooth is healthy, staircase leaks; this reading is far faster than line-by-line review.

## Pitfalls

### Pitfall 1: finishing an operation result with Unit

Symptom: memory leaks (Chapter 6's stats show the leak proportional to the operation count), or conversely `Destroy`-ing a stack handle and crashing.

Cause: `Unit` only returns the storage associated with a stack handle and does not free heap objects; `Intersection` and friends return independent heap-allocated sets — the two finish functions are not interchangeable.

```c bad
xset* pAllowed = xrtSetIntersection(&tRequested, &tEnabled);
/* ... use pAllowed ... */
xrtSetUnit(pAllowed);            /* Unit does not free a heap set - leak */
```

```c good
xset* pAllowed = xrtSetIntersection(&tRequested, &tEnabled);
/* ... use pAllowed ... */
xrtSetDestroy(pAllowed);          /* operation result: Destroy */
xrtSetUnit(&tRequested);          /* stack handle: Unit */
xrtSetUnit(&tEnabled);
```

### Pitfall 2: struct elements with unzeroed padding (a ghost bug that drifts with the compiler)

Symptom: "obviously equal elements" sometimes compare equal and sometimes not; "duplicate" elements appear in the set; behavior drifts with compiler and optimization level.

Cause: struct bytes include compiler-inserted padding; two logically identical structs with different padding bytes compare unequal byte-wise.

```c bad
typedef struct portspec { short Port; int Enabled; } portspec;   /* has padding */
portspec A; A.Port = 443; A.Enabled = 1;                          /* padding untouched */
xrtSetAdd(&tSet, &A);                                             /* enters the set with random padding */
```

```c good
portspec A;
memset(&A, 0, sizeof(A));          /* zero first, then assign - padding is deterministic */
A.Port = 443; A.Enabled = 1;
xrtSetAdd(&tSet, &A);
```

## Exercises

### Basic: deduplicated counting

Load a group of IDs with duplicates into a set, output `Count` and the deduplicated element list; then compare code volume and output order against Chapter 14's "sort and dedup" array approach (the array way outputs sorted, the set way outputs insertion order — think of one serving scenario for each order).

### Advanced: three-layer permission filtering (chained algebra)

Three groups of IDs (user group, permission table, blacklist); compute the final set of "has permission and not blacklisted" with intersection and difference; assert correctness at the scale of 1000 random IDs per layer. Hint: the intersection's result continues into the difference.

### Challenge: a configuration diff tool (symmetric difference + ownership annotation)

Read two port configurations (hardcoded is fine), use `SymmetricDifference` to output the differing ports annotated "only in A / only in B" — hint: annotating ownership requires looking the differing elements up in both source sets. Acceptance criteria: with three configurations of known differences, the output matches expectations line by line; all operation results `Destroy`-ed, stats verify zero leaks.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Loading | `Init(sizeof(元素))` + `Add` (deduplication for free); `Has`/`Remove`/`Count` public field reads |
| Algebra | intersection/union/difference/symmetric return **heap sets** (finish with `Destroy`); order follows the left operand |
| Relation checks | subset/superset/disjoint return booleans directly — no allocation, no intermediate sets |
| Value equality | byte equality; zero struct padding first; pointer elements compare pointer values |
| Iteration | three-parter + reverse + callback abort — three traversal postures, all outputs reproducible |
| Relation to map | same foundation and discipline; set = a map without values + the algebra family |
| Algebra data flow | every step's product lives independently; evaluate-and-clean, error paths miss no Destroy; intermediate sets die when used up |
| Relation rules | intersection = in both; difference = left-only; symmetric = exactly one; result order is always the left operand's insertion order |
