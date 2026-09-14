---
num: 14
slug: array
title: The Dynamic Array: xarray
volume: 卷三 容器与数据结构
type: practice
lead: Value-semantics contiguous storage — batch append, in-place sorting, the public Count field, and the polymorphic pointer-array variant.
api: array
---

## Orientation

Volume 3 begins with the most used container. `xarray` stores elements' **bytes contiguously and directly** — cache-friendly, zero per-element allocation, paired with the public `Count` field and a qsort-compatible sort interface, covering the highest-frequency path of "hold a batch of records, sort, iterate". The chapter also covers its twin `xptrarray` (storing pointers, holding polymorphic objects), pinning down in the very first chapter the "value semantics versus pointer semantics" selection question that runs through Volume 3. The container family's shared conventions (Init/Unit lifecycle, no side effects on failure, the borrowing-versus-owning boundary) are also established starting here.

## Introduction

A backend service maintains the "currently online sessions" list: thousands at peak, tens of inserts and removals per second, plus periodic priority-ordered output. Translated into container language: append must amortize O(1), sort must be in-place with zero allocation, iteration must be cache-friendly. A bare array makes you manage growth and copying yourself; a linked list loses contiguity and degrades sorting; rolling your own faces the failure paths Chapter 6 documents. `xarray`'s answer is plain: **contiguous storage + doubling growth + batch operations** — appending three elements is one `memcpy`, sorting is in-place qsort semantics, iteration is the most cache-friendly sequential access. Plain schemes win on constant factors, and container performance debates are mostly debates of constant factors.

When is the plain scheme not enough? With large elements and frequent mid-sequence inserts/deletes, the shifting cost of contiguous storage grows linearly — that is the territory of Chapter 20's AVL tree and Chapter 18's map; when elements need polymorphism (mixed concrete types), value semantics cannot hold them — that is the territory of `xptrarray` and the pointer containers. One less obvious boundary: when "by index" itself is not enough and the outside needs stable object identity, array indices drift on removal — that is the territory of Chapter 17's slot_map generation handles. The container world has no panacea, only an ever more precise selection map; this chapter is its first puzzle piece.

## Concepts

### Value semantics: an element is just bytes

`xrtArrayInit(&arr, sizeof(元素类型))` (element type verbatim below) takes only the element size — the array is fully generic over the element type. Elements are **embedded directly** in the array's own memory: no per-element allocation (appending 100 elements may allocate not even once), no pointer hopping (the prefetcher works at full strength during iteration), and element addresses stay stable until a growth, after which everything relocates wholesale. The direct corollary of value semantics: **element pointers taken while holding the array become invalid after growth** — the protagonist of this chapter's Pitfall 1.

### Lifecycle and public fields

```diagram flow
- Init: binds the element size; stack handle; the buffer managed by the array
- Append/Insert/Remove: batch editing; no side effects on failure
- Get/Set: indexed access; out-of-range returns NULL or failure
- Unit: returns the buffer — the exact inverse of Init
```

`tRecords.Count` is a **public field** — iteration and capacity checks need no function call. XRT containers' universal convention: read-only state goes through public fields; fallible operations go through functions (returning `bool` with Chapter 4's error slot). This gives hot-path reads zero overhead while failure paths keep full diagnostics.

### Growth strategy and Reserve

The dynamic array's capacity grows by doubling: on insufficiency it expands by a factor and relocates wholesale. This guarantees the amortized bound "appending N elements costs O(N) total relocation" — but a single Append may still trigger one big allocation. When the magnitude is known, `Reserve(N)` arrives in one step, turning amortization into certainty: before loading a hundred thousand records, `Reserve(100000)` makes the whole run a single allocation. The companion `Resize` explicitly sets the element count (newly grown elements zero-valued) and `Trim` shrinks capacity to the current count — after loading completes and the read-only phase begins, one Trim cut and the long-resident memory is immediately tidy. This capacity trio (Reserve/Resize/Trim) is standard across all of Volume 3's dynamic containers, with semantics identical on `xarray`, `xbuffer`, and `xmap`.

### Sorting and searching

`xrtArraySort` accepts a qsort-compatible comparison function — any comparator written as three-state `<`/`>` works. The example's `(a>b)-(a<b)` form is worth learning: it avoids the `a-b` subtraction overflow (two large negatives subtracted flip the sign, silently scrambling the sort). A sorted array can be binary-searched by index with the find family; the find functions accept both "search by element" and "search by key" (comparator callback) forms.

### The pointer array: the gateway to polymorphism

The value array handles "dense storage of same-typed elements", but two requirement classes it naturally cannot: elements of different concrete types (a renderer's shape family, an interpreter's node family), or element ownership belonging elsewhere (a cache does not own its objects, only references them). `xptrarray` stores pointers — elements may be of different concrete types (polymorphic through a common header), may be owned by another container, may be stack objects. The price: one extra indirection per access, and per-element lifecycle management is yours (the array's Unit does not free what the pointers point to). The selection mnemonic: **same-typed elements with heavy whole-array traversal → value array; polymorphic elements or ownership elsewhere → pointer array**. There is also a hidden but common third option: mix the two — a value array storing "keys and handles" as the dense index, with object bodies scattered in a pool or slot_map (Chapter 17). This chapter's session-table exercise rehearses exactly this "array + handle" architecture, and Chapter 67's network connection table is its full bloom.

Besides `Init`, two other starting postures are worth knowing: `Create` directly returns a heap-allocated array object (the handle itself on the heap, suited to long-term holding inside other structures), and `InitAligned` specifies element alignment (SIMD or atomic-field scenarios). `Clear` zeroes the element count but retains capacity — when reusing a batch of arrays in a loop, Clear is far faster than a paired Unit/Init and never touches the allocator.

## Examples

### Complete program: batch append and in-place sort

From the repository example `examples/containers/array/main.c`:

```embed path="examples/containers/array/main.c" title="examples/containers/array/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/array/main.c -lws2_32 -liphlpapi
id=102 priority=10
id=103 priority=20
id=101 priority=30
```

**What just happened.** (1) `xrtArrayInit` takes only the element size — genericity without macros or templates, by "moving bytes". (2) `xrtArrayAppend(&tRecords, pInput, 3)` appends three records at once (a single `memcpy` plus possible growth); the batch interface saves two function calls and two capacity checks over per-item Append. (3) `xrtArraySort` in-place sorts the shuffled input into ascending priority — the output order `10/20/30` verifies. (4) Iteration uses the public field `Count` and `Get`; `Get` returns a **direct pointer into the array's memory** (readable and writable), `NULL` when out of range. (5) `Unit` returns the buffer — the array handle itself is on the stack; only the heap buffer is returned.

### Complete program: pointer-array traversal

From `examples/containers/ptr_array/main.c`, demonstrating pointer-semantics storage and traversal:

```embed path="examples/containers/ptr_array/main.c" title="examples/containers/ptr_array/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/ptr_array/main.c -lws2_32 -liphlpapi
[0] 10
[1] 30
[2] 20
```

**What just happened.** The array stores `ptr` — references to objects living elsewhere. The element taken out during traversal is "the pointer's value"; the object bodies are not in the array's memory. All three typical sources are legal: stack objects (this example), objects owned by other containers, objects you allocated one by one and will free one by one. Note that the pointer array's `Unit` returns only the pointer-slot storage — **not one pointed-to object is freed**; the ownership discipline lies entirely with the caller, which is exactly Pitfall 2's theme.

Read the two examples side by side and the "value versus pointer" trade-off becomes three-dimensional: the value array loads three records in one memcpy, sorts by moving whole blocks, iterates with zero indirection; the pointer array adds one dereference per operation, sorts by moving 8-byte pointers (the element bodies stay put — an advantage for large objects!), and supports polymorphic mixing. Rule of thumb: elements smaller than a cache line and same-typed — the value array almost always wins; elements huge, from mixed allocation sources, or needing polymorphism — the pointer array.

### The container family's shared conventions (effective from this chapter)

The three conventions established here run through all of Volume 3; set them up now, later chapters reference directly. **Convention one: handle on the stack, memory on the heap** — `Init` opens with a stack handle, the container manages its buffers, `Unit` returns exactly; when the container object itself must live on the heap, use the `Create`/`Destroy` pair. **Convention two: read via fields, write via functions** — read-only state (`Count`, `Size`, `Data`) is public fields, zero-overhead direct reads; fallible operations all go through function return values with Chapter 4's error slot. **Convention three: no side effects on failure** — any edit failing leaves the container in its pre-call state, so Cleanup-section code can be written with zero anxiety. The three conventions compound in every later chapter: reading any new container, you already know its lifecycle shape, field style, and failure semantics — only "what is this container good at" remains new.

## Contracts

- **Lifecycle**: `Init`/`Unit` paired exactly; failure paths also `Unit` (no-side-effect-on-failure makes this cleanup line always safe).
- **Public fields**: read-only state like `Count` directly as fields; fallible operations via function return values.
- **Pointer validity**: growth invalidates existing element pointers — need stable addresses? Reserve capacity first, or switch to the pointer array/linked list.
- **Out-of-range behavior**: `Get` returns `NULL` when out of range; edit operations fail and set an error on out-of-range — never an out-of-bounds write.
- **Pointer-array ownership**: `Unit` does not free the pointed-to elements; whoever allocated frees.
- **Comparators**: three-state `(a>b)-(a<b)` style; `a-b` forbidden (overflow).
- **The capacity trio**: `Reserve` pre-grows, `Resize` sets length, `Trim` tightens; `Clear` zeroes retaining capacity; semantics identical across Volume 3's dynamic containers.

## Pitfalls

### Pitfall 1: using old element pointers after growth

Symptoms: sporadic data corruption or crashes, typically in "Append during iteration" code; everything fine with small data (no growth triggered).

Cause: value-semantics elements live in the array's buffer; growth = allocate new buffer + relocate wholesale + free old buffer — every old pointer dangles.

```c bad
examplerecord* pRecord = (examplerecord*)xrtArrayGet(&tRecords, 0);
xrtArrayAppend(&tRecords, pMore, 100);   /* may trigger growth + relocation */
pRecord->ID = 42;                         /* pRecord points into the freed old buffer */
```

```c good
examplerecord* pRecord;
xrtArrayAppend(&tRecords, pMore, 100);   /* do the possibly-growing operation first */
pRecord = (examplerecord*)xrtArrayGet(&tRecords, 0);   /* then take the pointer */
pRecord->ID = 42;
```

### Pitfall 2: expecting the pointer array to free elements

Symptoms: memory keeps growing; Chapter 6's statistics show live bytes only rising — everything leaking is the element bodies.

Cause: `xptrarray` manages only the "pointer slot" storage; ownership of element objects was never in the array's hands.

```c bad
xptrarray tObjects;
/* ... load a batch of xrtMalloc'ed objects ... */
xrtPtrArrayUnit(&tObjects);   /* returns only the pointer slots — every element leaks */
```

```c good
for ( size_t i = 0; i < tObjects.Count; i++ ) {
	xrtFree(xrtPtrArrayGet(&tObjects, i));   /* return the elements one by one first */
}
xrtPtrArrayUnit(&tObjects);                  /* then return the array itself */
```

## Exercises

### Basic: reverse output

Load the integers 1 through 10 into an `xarray`, print in reverse by index; then `Append` a new value in forward order and print again, verifying `Count` semantics stay consistent; finally `Clear` and confirm `Count` zeroed and that appending again needs no fresh Init.

### Advanced: stable-sort verification

Construct 20 records with "equal priority, distinct sequence numbers" and sort, checking the relative order of equal-priority records; then sort extreme values (`INT_MAX` and `INT_MIN`) with both the `(a>b)-(a<b)` and `a-b` comparators and see the overflow with your own eyes. Hint: qsort does not guarantee stability; draw "relative order of equal keys" conclusions from actual output.

### Challenge: a session-table prototype

Implement the "online session table": a value array storing `会话{ID, 优先级, 时间戳}` (session{ID, priority, timestamp}); supporting batch login (Append), logout (find by ID and Remove), priority-ordered output of the top N. Acceptance: after mixed operations, `Count` matches the actual record count; logging out a nonexistent ID fails without modifying the array; Chapter 6's statistics verify zero leaks throughout.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Initialization | `Init(&arr, sizeof(元素))`; handle on the stack, buffer owned by the array |
| Batch editing | `Append(数组, 源, 个数)` one memcpy; no side effects on failure |
| Iteration | `Count` public field + `Get` direct pointer; out-of-range `NULL` |
| Sorting | `Sort` qsort-compatible; three-state comparator style against overflow |
| Pointer validity | old pointers invalid after growth; grow first, take pointers after |
| Pointer array | `xptrarray` stores `ptr`; `Unit` frees no elements — ownership with the caller |
| Selection | same-typed heavy traversal → value array; polymorphic/elsewhere-owned → pointer array; key lookup → Chapter 18's map |
| Hybrid architecture | value array of keys and handles + objects scattered in pool/slot_map — the rehearsal of Chapter 67's connection table |
