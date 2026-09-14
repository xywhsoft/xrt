---
num: 117
slug: xruntime-typed
title: xruntime (Part 4): Typed Containers
volume: 卷十一 其他扩展库
type: practice
lead: Six families — array/stack/list/tree/set/dict — driven by type descriptions, unified ownership and failure atomicity, description-pointer equality and reuse of XRT fast paths — the container face for host values; Volume 11 concludes.
api: xruntime-typed_array, xruntime-typed_dict, xruntime-runtime_type
---

## Orientation

xruntime's four chapters conclude. The typed container family (`xtypedarray`/`xtypedstack`/`xtypedlist`/`xtypedtree`/`xtypedset`/`xtypeddict`) shares **one runtime value contract** atop six underlying data structures: borrowed immutable type descriptions, description-driven value operation families managing value lifecycles (copy/move/compare/hash/destroy all through the description table — Chapter 114's type facts honored by containers), unified ownership verbs (Set/Add/Push copy, Take/Pop move, SetTake atomically transfers on failure, Clone deep-copies), unified failure atomicity (multi-step operations work in staging storage before committing; failure never breaks the visible state). **Engineering essentials**: the extension **reuses XRT containers' internal fast paths directly** (no duplicated underlying implementation — the typed layer is a "type-driven shell"; an int64 array performs exactly like Chapter 14's array); operations touching two containers require **identical description pointers** (same ID/name is not enough — lifecycle ABI and private context are not interchangeable). Volume 11's eighteen chapters close here.

## Introduction

Why typed containers? The C containers of Chapters 14–23 (`xarray(int64)` and friends) are **compile-time typed** — fast, static, but the type is frozen at compile time. Host-language scenarios (script arrays, JSON loaded back into memory, plugins exchanging data) need containers with **runtime types**: "this array's element type is that description named `app.Counter` in the registry" — one container codebase dispatching value operations by description at runtime. The naive approach is a `void*` array plus scattered callbacks — casts everywhere in every operation; the typed layer concentrates dispatch in the description table: **one copy of container code, all type behavior in xrttype**.

The relation to Chapter 31's `xvalue` dynamic values: xvalue is "a single dynamic value" (a self-describing tagged union); typed containers are "collections of same-typed elements" (container-level description, zero per-element tag overhead — an int64 array is contiguous int64s, not an array of Values). They complement each other: Value suits heterogeneous trees (JSON), typed suits homogeneous collections (script arrays/object field tables).

## Concepts

### The shared contract: six families, one face

| Verb | Semantics |
| --- | --- |
| `Set`/`Add`/`Push`/`Insert` | copies the source value — the container owns the new value, the source stays with the caller |
| `SetTake` | atomic-on-failure move — the source is restored to empty on success |
| `Take`/`Pop` | moves into an initialized output — the container no longer owns it |
| `Remove`/`Clear`/`Unit` | runs Drop once per owned value |
| `Clone`/`Merge`/set algebra | deep copy — no hidden shared ownership |

**Address validity**: APIs accepting the address of a complete typed value allow "an external value or one of this container's exact live slots" — addresses pointing into the container structure/metadata/spare capacity/padding/mid-slot are **rejected before read or write**; a move's output and source must lie outside container memory (the container edition of defensive validation).

### Each family's distinctive face

- **array** (contiguous storage): fixed/variable length, indexed access, the Concat/Equals family.
- **stack** (LIFO): Push/Pop/Peek.
- **list** (sparse integer keys): insert/delete, iterate in order — the shape of a script list.
- **tree** (ordered generic keys): comparison-driven ordered map — the key type needs compare operations.
- **set** (unique values): hash-ordered or comparison-ordered dedup — the key type needs hash/compare.
- **dict** (text keys): string-keyed dictionary — the payload of Chapter 115's dynamic field node (`xrtdynamicfields`) is exactly this.

### Description-pointer equality: why the same ID is not enough

Contract-literal: "operations involving two typed containers require the element type descriptions to be the **same pointer**. The same ID, name, size, or operation table is **not sufficient** to prove the two descriptions' lifecycle ABI and private context are interchangeable." Two modules each statically declared a "same" type? Different pointers, no interop: **XRT built-in types and host-registered types should use process-canonical descriptions** (the same pointer `xrtTypeInt64()` returns). This rule eliminates the hidden danger of "same content, different declaration site" (implicit divergence of private context/lifecycle).

### Reuse, not duplicate

The extension **reuses XRT containers' internal fast paths directly** — `xtypedarray`'s contiguous storage is Chapter 14's array internals; the typed layer adds entrances that "dispatch value operations through the description table". The payoff is bidirectional: performance (the int64 push fast path doesn't slow down under the typed shell) and maintenance (one copy of container algorithms — Chapter 22's pooling idea in another form, "implementation reuse").

### Failure atomicity (shared)

When allocation/value initialization/copy/move fails: publicly visible elements, keys, order, ownership **stay unchanged** — multi-step operations (Clone/Merge/set algebra) complete in an independent working store first and commit once; failure cleanup destroys only the values this run successfully constructed, keeping the underlying error as the cause. OOM's final category is always `XERR_MEMORY`. This continues the atomicity discipline of Chapter 31's Value operations and Chapter 18's containers — the typed layer doesn't relax just because it's "dynamic".

### Typed containers' place in xruntime's four chapters

```diagram flow
- Type system (Chapter 114): the xrttype description - the fact source of value operations/compare/hash
- Objects and graphs (Chapter 115): reference-counted objects - containers can be payloads and gain object identity
- Dynamic calls (Chapter 116): callable values - circulate as container elements
- Typed containers (this chapter): six description-driven container families - interop only with identical description pointers
```

## Examples

### First complete program: the full typed-array operation set

The program below is from `examples/runtime/typed_array` — the loop of Init/Push/Clone/Concat/Equals/IndexOf:

```embed path="extlibs/xruntime/examples/runtime/typed_array/main.c" title="extlibs/xruntime/examples/runtime/typed_array/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/typed_array/main.c -lws2_32 -liphlpapi
（输出 count/joined/index 的数组操作自检行）
```

**What just happened.** (1) `xrtTypedArrayInit(&Values, xrtTypeInt64())` — driven by **the built-in int64 canonical description**: subsequent Pushes copy int64s (the description's Ops decide), Equals compares by int64 — the container code contains no int64-specific logic. (2) After three Pushes, `Clone` deep-copies, `Concat` splices (Values+Copy = 6 elements), `Equals(Values, Copy)` decides equality — the ownership verbs work per the shared contract (Clone deep-copies, no sharing). (3) Printing count=3, joined=6, index=the found position — **a runtime-typed array behaves isomorphically to a C array**; the only difference is "the type comes from a description". Swap in `xrtTypeString()` or a custom type — the same API set (everything description-driven).

### Second complete program: the object field dictionary

The second program is from `examples/runtime/typed_dict` — the dynamic-fields entity:

```embed path="extlibs/xruntime/examples/runtime/typed_dict/main.c" title="extlibs/xruntime/examples/runtime/typed_dict/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/typed_dict/main.c -lws2_32 -liphlpapi
（输出文本键字典操作的自检行）
```

**What just happened.** (1) A text-keyed dictionary with a description-driven value type — Set/Take/Remove per the shared contract; keys are strings (the built-in STRING description), values arbitrary (the types demonstrated here). (2) This shape is exactly the payload of Chapter 115's `xrtdynamicfields` — **a host object's dynamic field table = a dictionary node in the object graph**: the object graph traces the dictionary node, and the dictionary's values trace runtime-object references via `xrtTypeValue()` — three chapters (114/113/116) converge here. (3) The companion family covers all six families (`typed_stack/list/tree/set`) plus the three queue forms (`typed_queue_spsc/mpsc/mpmc` — the typed edition of Chapter 21's topology-specific queues) and `typed_value_containers` (Value-boxing containers) — **one sample per underlying structure** is the acceptance face of the six-family contract.

## Contracts

- **Description borrowing**: the six families borrow immutable xrttype; stable across registration and container lifetime (Chapter 114's discipline).
- **Pointer equality**: cross-container operations require identical description pointers — same ID/name/size/operation table is insufficient; built-in and host types use process-canonical descriptions.
- **Value-operation dispatch**: everything goes through the type description's value operation family (table-driven) — containers carry zero type-specific code.
- **Ownership verbs**: Set family copies / SetTake atomically transfers / Take family moves / Remove family Drops per value / Clone family deep-copies.
- **Address validity**: value addresses limited to external or this container's exact live slots; structure/metadata/capacity/padding/mid-slot rejected; a move's output and source lie outside the container.
- **Failure atomicity**: visible state unchanged on failure; multi-step operations stage before committing; OOM always XERR_MEMORY with the underlying error kept as cause.
- **Implementation reuse**: XRT container internal fast paths reused directly — zero performance tax from the typed shell, one copy of algorithms.
- **Plain C container boundary**: C containers carry no reference counting or object identity — weak references/cycle reclamation combine via runtime_object with the container as payload.
- **Family specifics**: array contiguous / stack LIFO / list sparse-ordered / tree comparison-ordered / set unique / dict text-keyed — each specializes its structure atop the shared contract.

## Pitfalls

### Pitfall 1: interop with "the same type" described by different pointers

Symptom: Concat/compare of two containers whose element types "are clearly the same" gets rejected — each module statically declared the same-named type.

Cause: pointer equality is the hard rule — identical content doesn't prove lifecycle ABI and private context interchangeable. Uniformly use process-canonical descriptions (the built-in `xrtTypeInt64()` / the registry's process-canonical descriptions).

```c bad
/* Counter descriptions statically declared in modules A and B respectively */
xrtTypedArrayConcat(&A_Array, &B_Array);   /* different description pointers: rejected */
```

```c good
/* one description source process-wide: fetch the process-canonical description from the registry */
const xrttype* T = host_registry_find("app.Counter");
xrtTypedArrayInit(&A, T); xrtTypedArrayInit(&B, T);
xrtTypedArrayConcat(&A, &B);   /* same pointer: interoperable */
```

### Pitfall 2: after Clone, sharing values you thought were independent

Symptom: clone the container, change one, the other follows — the values hide reference/handle-type elements.

Cause: Clone is a **deep copy** — but how "deep" is defined by the element type's Ops: copying a reference-type element (an object value) is a **strong-reference increment** (Chapter 115's ObjectValueOps Copy = Ref) — the object itself is still shared (exactly right for reference-semantics types). Expecting "even the objects are separate" requires an element-level custom Clone.

```c bad
pCopy = xrtTypedArrayClone(&Objs);   /* object-array clone: references +1, not new objects */
mutate(pCopy);                        /* the object changed: the original container "changed" too */
```

```c good
/* reference semantics is a feature: multiple containers share one object (graph tracing handles reclamation) */
/* for truly independent copies: the element type provides deep-Clone Ops */
```

### Pitfall 3: using a typed container as an object-graph root (missed tracing)

Symptom: object references held by the container are never traced by the object graph — a cycle leaks.

Cause: plain typed containers **carry** no reference counting or object identity — what the graph traces is runtime_object (Chapter 115). The right answers: make the container an object's payload (the container itself is an object), or use dynamic field nodes (the dict payload traced via the type's value operations).

```c bad
/* a bare typed array holding object references, hoping the graph discovers it */
xrtTypedArrayPush(&Arr, &ObjRef);   /* the graph doesn't know Arr exists */
```

```c good
/* make the container an object (payload holds the container) -> the object joins the graph -> InstanceOps.Trace enumerates container elements
   or use xrtdynamicfields (field dictionary nodes trace natively) */
```

## Exercises

### Basic: a six-family tour

Run each of the six containers through its minimal loop (Init → two operations → Destroy) — with the same element type (int64). Acceptance criteria: ownership verbs behave consistently across families; failure atomicity (OOM simulation) leaves each family's visible state unchanged.

### Advanced: a polymorphic list

A list with element type `xrtTypeValue()` (Chapter 31's dynamic-value boxing) — store int/string/object values and iterate printing (Kind dispatch). Acceptance criteria: three value kinds coexist; deep clones are independent (value-semantics elements); sorting by Value comparison.

### Challenge: an object field engine

A field table in the `xrtdynamicfields` style: an object (Chapter 115) + typed dict payload + set/get/enumerate fields + object-graph tracing (cycle collection of self-referencing fields). Acceptance criteria: the fields' strong references enumerated by Trace; self-reference cycles reclaimed at a safepoint; field types mixed (int/string/object).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Positioning | six underlying structures sharing one runtime value contract — type-driven, reusing XRT fast paths |
| Description discipline | borrow immutable descriptions; cross-container operations need pointer equality (same content insufficient) |
| Value dispatch | all through the type description's value operations — zero type-specific container code |
| Ownership | Set copies / SetTake atomic transfer / Take moves / Remove Drops per value / Clone deep-copies |
| Failure atomicity | visible state unchanged; stage before committing; OOM always MEMORY with the cause kept |
| Six families | array contiguous / stack LIFO / list sparse / tree comparison-ordered / set unique / dict text-keyed |
| Versus xvalue | Value = single dynamic value (heterogeneous trees); typed = homogeneous collections (zero per-element tag overhead) |
| Object identity | C containers have no reference counting — object semantics compose via runtime_object |
| Tracing closure | dynamic fields = dict payload + Value tracing — 114/113/116 converge |
| Three queue forms | spsc/mpsc/mpmc typed editions — Chapter 21's topology choice continued |
