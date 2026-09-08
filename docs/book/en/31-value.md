---
num: 31
slug: value
title: The xvalue Dynamic Value System
volume: 卷四 文本与结构化数据
type: practice
lead: One type that holds all data — exact reads with no implicit conversion, reference counting for the lifecycle, value trees composing configs and messages.
api: value
---

## Orientation

From this chapter we enter structured data. `xvalue` is XRT's dynamic type system: one opaque pointer can hold null/bool/int/float/string/bytes/time/array/object/set/map — everything JSON can express, plus XSON's extended types (Chapter 32) living here directly. Three designs decide the usage experience: **exact reads** (GetInt accepts only int; a type mismatch fails — the first gate against type confusion), a **reference-counted lifecycle** (the full flowering of Chapter 3's primitives), and **immutable sharing** (a value tree can be referenced from many places without copying). It is the parse product of Chapters 31/32/34 and the data source of templates — the foundation of this volume's second half.

## Introduction

The classic puzzle of configuration systems: in one config file, the timeout is a number, the switch a boolean, the tags a list, the nested service an object — C's static types cannot hold data whose "shape is known only at runtime". Hand-written `void*` plus a type tag is the wheel everyone keeps reinventing, with an extreme pitfall density: type judgments rely on memory, free responsibilities on convention, copy semantics on prayer.

`xvalue` standardizes that wheel: one type enum (`xvaluetype`) plus a family of construct/read/test functions, with a reference-counted lifecycle. It is conceptually isomorphic to dynamic languages' value objects (Python's object, JS's value), but puts "type strictness" first — **the out-param you get is exactly the type you asked for**, with none of "numbers and strings auto-convert" leniency. This strictness is precisely a hard requirement in config parsing: `"timeout": "30"` (string) and `"timeout": 30` (number) are two different error signals; leniency quietly turns the former into the latter, and the bug is buried deep from then on.

## Concepts

### The type family and construction

| Type | Construct | Exact read |
| --- | --- | --- |
| null / bool | `xrtValueNull` / `Bool` | `GetBool` |
| int64 / double | `xrtValueInt` / `Float` | `GetInt` / `GetFloat` |
| string / bytes | `xrtValueString` / `Bytes` | `GetString` (view) / `GetBytes` |
| time / pointer | `xrtValueTime` / `Pointer` | `GetTime` / `GetPointer` |
| array / object | `ArrayNew` / `ObjectNew` + the Set family | by index / by name |
| set / intmap | `SetNew` / `IntMapNew` | sets and integer-keyed maps |

Null is a **process-level singleton** — the same pointer every time, safe to compare with `==`. String reads return a **view** (borrowing, Chapter 3's convention), not a freshly allocated copy. Numbers get one dedicated lenient channel: `xrtValueScalarEqual` lets int 2 equal float 2.0 (cross-type numeric semantics), and `ValueHash` agrees with it (int 2 and float 2.0 hash identically) — a consistency design necessary for "value trees as cache keys".

### The philosophy of exact reads

`GetInt` handed a float value **fails** rather than silently truncating; if you want numeric leniency, test the type explicitly first, then read. This philosophy shares ancestry with Chapter 26's strict parsing: **type confusion surfaces at the read gate, not as a landmine at the consumer**. Paired with `xrtValueType` (query the type) and the `Is`/`IsNumber`/`IsContainer` test family, read code always has the shape "test first, take second; type mismatch goes down the error path".

### Lifecycle: reference counting in full

```diagram flow
- Construct: Int/String/ObjectNew return owning values with a count of 1
- Share: Retain increments - a value tree stored into many structures is not copied
- Release: Release decrements - whoever reaches zero is destroyed (entire subtree included)
- Container attach: the Set family hangs child values into the container - the container holds a reference; after attaching you may safely Release your own copy
```

This is Chapter 3's `xrtRefRetain`/`xrtRefRelease` primitives applied in full: construction returns count 1; `xrtValueRetain` shares, `xrtValueRelease` returns; when a container takes a child value, **the container acquires its own reference** — you Release your copy right after constructing, and the child's lifecycle is the container's business from then on. Releasing the whole tree returns recursively — the root's Release reaches zero, the entire tree destructs.

### Value-tree operations

The two big pieces of the config scenario: `xrtValueObjectMerge` (object overlay merge — the standard posture of "defaults + user overrides", with REPLACE/keep/report-conflict strategies) and `xrtValueSetUnion/Merge` (set union — the standard posture of permission merging). `xrtValueClone` does a shallow clone (shared elements) — the shallow-vs-deep choice gets a controlled experiment in the ownership sample.

## Examples

### Complete program: a full-type tour and exact reads

From the repository sample `examples/value/basic/main.c`:

```embed path="examples/value/basic/main.c" title="examples/value/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/value/basic/main.c -lws2_32 -liphlpapi
xrt value API v2
time type: time
```

**What just happened.** (1) The construction family builds values one by one: Int, Float, String, Bytes, Time... each constructor returns an owning value with count 1. (2) The exact-read family takes them back one by one: `GetInt` reads an int successfully and a float with **failure** — the "you get exactly the type you ask for" gate verified on each type. (3) The test family pairs with `ValueTypeName` (type enum to lowercase name) — debug output and error messages report stable names like `"time"`. (4) `ScalarEqual` verifies that 2 equals 2.0 across types, and `ValueHash` verifies both hash the same — lenient numeric semantics and hash consistency appear as a pair, the precondition for value trees as keys.

### Complete program: config merging and set union

From `examples/value/collections/main.c`:

```embed path="examples/value/collections/main.c" title="examples/value/collections/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/value/collections/main.c -lws2_32 -liphlpapi
options=1 permissions=2
```

**What just happened.** (1) Object merge `Merge(默认, 用户, REPLACE)` (defaults, user, REPLACE): the user config's timeout 5 overrides the default 30 — after merging, count is still 1; overriding adds no key. (2) Set union takes two roads: `Union` produces a new set and `Merge` merges in place, the products tested equal with `SetEqual` — different roads, same destination. (3) `IsDisjoint` decides two sets are disjoint — permission checking's "any intersection?" answered in one step. (4) These two operations together are the configuration-layer skeleton of "default config + user override + permission merge"; Chapter 37's templates and the value trees read out by Chapter 31's JSON use them directly.

## Contracts

- **Exact reads**: type mismatch fails, no implicit conversion; numeric leniency goes through explicit `ScalarEqual`.
- **Reference counting**: construct = 1, `Ref` shares, `Release` returns, zero destructs the whole tree; a container attaching a value acquires its own reference.
- **Null singleton**: one pointer process-wide, comparable with `==`.
- **String borrowing**: `GetString` returns a view whose validity follows the value tree.
- **Hash consistency**: `ValueHash` pairs with `ScalarEqual` — value trees can be cache keys.
- **Merge strategies**: REPLACE overwrite / keep existing / report conflict — chosen explicitly.

### From examples to engineering: three hosts of value trees

Three typical hosts of value trees in engineering decide how the reference counting balances. **Config trees**: parsed at startup (Chapter 31's JSON), constructed once, read-only throughout, root-Released once at exit — the simplest balancing (construct and root-release only, no Ref/Release in between). **Message passing**: a value tree travels as a message across threads/modules — the sender Retains then hands over, both sides Release; or use SetTake to transfer (the standard posture in Chapter 18's queue scenario — one transfer, zero surplus references). **Cache sharing**: one value tree referenced by many consumers (default config shared with all requests) — the constructor Retains, puts it into a global, consumers Retain/Release individually; COW (copy-on-write) semantics make a shallow-shared Clone truly copy only on first edit. Beyond the three hosts, one master discipline: **write the balancing into the design document, don't pray at runtime** — Chapter 6's statistics are the final judge of balance correctness.

### The division of labor with containers

What is the relation between Chapter 30's value containers (array/object/set/intmap) and Volume 3's same-named containers (Chapters 14/18/19)? **Two parallel implementations serving two worlds.** Volume 3's containers are the statically typed world: element types fixed at compile time, no type tags, maximum performance — hot-path data structures use them. Value containers are the dynamically typed world: every value carries a type tag, heterogeneous mixing works, serialization is supported — config and protocol data use them. Selection mnemonic: **data shape known at compile time → Volume 3 containers; known only at runtime (from JSON/network/scripts) → value trees**. The two can bridge (a value tree's intmap converts with xintmap), but don't mix semantics — using a value tree as a high-performance container, or conversely adding type tags to static containers, both re-invent the other.

## Pitfalls

### Pitfall 1: forgetting to Release your copy after attaching into a container

Symptom: memory leak — after the value tree is released, statistics still show live values; the leak volume is proportional to "the number of child values attached into containers".

Cause: attaching into a container **increments** the reference (the container's copy); the reference you got at construction is still in your hand — un-Released, it stays one too many forever.

```c bad
xvalue* pTimeout = xrtValueInt(30);
xrtValueObjectSet(pConfig, XRT_STR_LITERAL("timeout"), pTimeout);
/* Release missing: pTimeout's reference count sits at 2; after the container frees, 1 remains - a leak */
```

```c good
xvalue* pTimeout = xrtValueInt(30);
xrtValueObjectSet(pConfig, XRT_STR_LITERAL("timeout"), pTimeout);
xrtValueRelease(pTimeout);   /* the container already holds a reference - return your own copy */
```

### Pitfall 2: reading through type leniency

Symptom: a number written as a string in the config (`"30"`) — the program "runs" but its behavior drifts with the upstream format; the landmine detonates during refactoring.

Cause: the discipline of testing with `IsNumber` first was replaced by the "just try GetInt" mindset — leniency masks an upstream data-shape error.

```c bad
int64 iTimeout;
if ( xrtValueGetInt(pValue, &iTimeout) ) {
	/* "30" (string) fails here - but worse, someone will add a fallback that retries as a string */
}
```

```c good
if ( xrtValueType(pValue) == XVALUE_INT ) {
	int64 iTimeout;
	xrtValueGetInt(pValue, &iTimeout);
} else {
	ReportTypeMismatch("timeout", "int");   /* the shape error is reported at the read gate */
}
```

## Exercises

### Basic: a type tour

Construct one of every scalar type, print each's `ValueTypeName` and `ValueType`; call GetFloat on an int value and verify the failure — the feel of strictness.

### Advanced: a config merger

Build two object trees, "default config" and "user config" (3~4 keys each, nested objects included), merge with REPLACE and print all key-values; then switch to the report-conflict strategy and feed a key-colliding input to watch it fail. Hint: think through the nested-object merge semantics too (replace whole or merge recursively — decide by your business and write it in a comment).

### Challenge: a value-tree cache

Store "config name → value tree" in Chapter 18's map, fingerprint the tree with `ValueHash` and test equivalence with `ScalarEqual`; the same config parsed twice must share a fingerprint; changing one field must change it. Acceptance criteria: the fingerprint is stable enough for test assertions; cached-hit and cached-miss trees compare equal via `ScalarEqual`; after releasing everything, Chapter 6's statistics verify zero leaks.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Type family | scalars + string/bytes/time/pointer + array/object/set/intmap |
| Exact reads | type mismatch fails; numeric leniency via `ScalarEqual`; type testing via `ValueType` |
| Lifecycle | construct = 1 / `Ref` shares / `Release` returns; a container attaching a value holds its own reference |
| Null singleton | the same pointer compares with `==`; to distinguish "absent" from "value is null", test the type with `XVALUE_NULL` |
| Hash | `ValueHash` pairs with `ScalarEqual` — value trees can be cache keys |
| Config duo | `ObjectMerge` (REPLACE/keep/conflict) + `SetUnion/Merge` |
| Strings | `GetString` returns a borrowed view; Dup it yourself if you need a copy |
