---
num: 114
slug: xruntime
title: xruntime (Part 1): The Type System
volume: 卷十一 其他扩展库
type: practice
lead: Stable type IDs derived from ABI names, 22 runtime kinds, value operation tables and single inheritance, registries and conversion — the type-facts layer shared by C, XRT, and host languages.
api: xruntime-runtime_type, xruntime-runtime_convert
---

## Orientation

The last family of Volume 11: **xruntime** — a runtime-model extension built on XRT (type descriptions/objects/calls/typed containers) that gives host languages (script engines, plugin systems, serialization frameworks) a **type-facts layer** shared between C and XRT. This chapter covers the foundation: **type description** (`xrttype` — the complete description struct of Id/Kind/ABI name/size-alignment/operation table/inheritance/field-method metadata); **type identity** (`xrtTypeId(ABI 名)` (ABI name) derives a stable nonzero ID — never a casually assigned integer; `TypeSame` compares ID + ABI name, `TypeIsA` walks the single-inheritance chain); **22 runtime kinds** (stable storage kinds from INVALID to CLASS/ENUM — describing storage, not syntax); **registry and conversion** (the type registry as the process-level set of facts; `runtime_convert`'s convertibility checks and value conversion). Core discipline: **modules own no descriptors** — every description stays address-stable and unmodified for the duration of registration and queries (borrowing of metadata, process-lifetime life).

## Introduction

Why does a host language need a "type-facts layer"? Picture embedding a script engine in C: the script's `Counter` class must be printed by a C-side debugger (needs the type name and field table), traversed by a serialization framework (needs value operations: copy/compare/hash/destruct), tracked by a garbage collector (needs strong-reference enumeration), inherited by another script module (needs the base-class chain) — **every capability needs structured knowledge about the type**, and `switch(type)` scattered everywhere is maintenance hell. `xrttype` standardizes that knowledge into one description table: the script engine registers it, XRT containers consume it, debuggers read it — three parties sharing one set of facts.

Two key design decisions deserve attention. **First: IDs derive from ABI names** — `xrtTypeId("app.Counter")` produces a stable ID, and a custom type's Id must equal that result — cross-module, cross-process type equivalence has something to stand on (casually assigned integers collide the moment a second module loads). **Second: descriptions borrow, they don't own** — modules hold no descriptor memory; the caller guarantees address stability and immutability during registration and queries — process-lifetime static descriptions (C's natural shape) cost nothing, and dynamically generated descriptions get their life managed by the host.

## Concepts

### Type description: the full field table

| Field | Contract |
| --- | --- |
| `Id`/`AbiName` | stable ID (must equal `xrtTypeId(AbiName)`) and a non-empty, cross-module unique canonical name |
| `Kind` | one of the 22 stable storage kinds |
| `Name` | non-empty display name (usable for localized display) |
| `Size`/`Align` | size and power-of-two alignment of the C-ABI value |
| `InstanceSize`/`InstanceAlign` | size and alignment of the heap instance payload (declared separately for reference types) |
| `Ops` | value-lifecycle operation table (copy/compare/hash/format/strong-reference tracing — optional) |
| `InstanceOps` | instance payload Init/Drop/Trace (optional — used by Chapter 115's objects) |
| `Base` | class base (only CLASS may inherit; FINAL cannot be inherited; depth capped at 255) |
| `Arguments`/`Fields`/`Methods`/`Metadata` | borrowed generic arguments/field table/method table/custom metadata |

**The hard rule for reference types**: `Size == sizeof(ptr)` and `Align` equals the pointer alignment — a reference is exactly one pointer in the C ABI, and the object payload is described separately by `InstanceSize` (the two-layer split of value versus instance — the foundation of Chapter 115's object model). `xrtTypeValidate` fully validates a description — an invalid description never enters the system (yet another instance of startup-time interception).

### The 22 kinds: storage kinds, not syntax kinds

`xrttypekind` expresses **stable runtime storage kinds** — "what shape is this value in memory" — not language visibility or syntactic aliases (public/private and typedef are language-layer concerns): INVALID/NULL/BOOL/SIGNED_INT/UNSIGNED_INT/FLOAT/STRING/BYTES/TIME/POINTER/CALLABLE/ARRAY/LIST/SET/DICT/RECORD/HANDLE/TYPE/FUTURE/CLASS/ENUM — the full spectrum from primitives to containers to class references. **XRT's built-in types** (`xrtTypeInt64()`/`xrtTypeString()`/`xrtTypeCallable()` and friends) return process-canonical descriptions — hosts registering types should likewise use process-canonical descriptions (the "pointer equality" requirement of Chapter 117's container contract).

### The value operation table: Ops' six jobs

`Ops` describes the behavior of the **value** (the thing stored in the C ABI): copy (atomic on failure), compare (ordering), hash (consistent with compare), format (debug printing), strong-reference tracing (GC cooperation — if the value holds object references they must be enumerable). `InstanceOps` describes the **instance payload** (Init/Drop/Trace) — expanded in Chapter 115. The two operation tables mirror the value/instance two-layer split.

### The registry: the process-level set of type facts

The `xrtTypeRegistryCreate` family (Add/At/FindId/FindName) — registering type descriptions and finding them by name; the sample's `RegistryFindName(AbiName)` is exactly the registry consumption of "find the description by canonical name". The registry does not copy descriptions (it borrows) — the description's lifetime still belongs to its declarer.

### Conversion: runtime_convert

`runtime_convert` answers "can this value become that type" on top of the type facts: `xrtTypeCanConvert` (convertibility — exact match/widening), `xrtTypeCanWiden` (numeric widening such as int32→int64), `xrtTypeConvert` (performs the conversion), `xrtTypeToString`/`TypeFormat` (value → text). The conversion layer's meaning: the host language's dynamic type system ("use this arbitrary value as an int") gains a **declarative foundation** — checking and execution separated, error paths explicit.

### xruntime's place in the extension-library family

```diagram flow
- Type system (this chapter): the xrttype facts layer - 22 kinds/operation tables/inheritance/registry/conversion
- Objects and object graphs (Chapter 115): reference-counted objects/weak references/safepoint cycle collection
- Dynamic calls (Chapter 116): signatures/call frames/multiple returns - the callable environment
- Typed containers (Chapter 117): the container family driven by type descriptions - the container face for host values
```

## Examples

### First complete program: type registration and value operations

The program below is from `examples/runtime/type` — the full loop of description, registration, and value operations:

```embed path="extlibs/xruntime/examples/runtime/type/main.c" title="extlibs/xruntime/examples/runtime/type/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/type/main.c -lws2_32 -liphlpapi
（输出类型名、稳定 ID、大小、比较与散列结果的自检行）
```

**What just happened.** (1) The type description registers into the registry, retrieved both ways via `RegistryAt(0)` and `FindName(AbiName)` — **type lookup by canonical name** is the standard posture for host-module interop (script module A registers Counter; module B fetches it by name). (2) `xrtTypeCompareValue(&Type, &左, &右, &结果)` (left, right, result) — **generic value comparison** driven by the type description: you need not know what type the value is; the Ops in the description table decide (hashing likewise — the foundation for dynamic values in hash containers). (3) The printed Id/Size come from the description — `TypeValidate` guarantees their self-consistency (reference types' Size=sizeof(ptr), etc.). **This output line is the executable proof of the "type-facts layer"**: C code operated on a value it doesn't know, holding the description table.

### Second complete program: the conversion layer

The second program is from `examples/runtime/convert` — the consumption shape of conversion checks:

```embed path="extlibs/xruntime/examples/runtime/convert/main.c" title="extlibs/xruntime/examples/runtime/convert/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/convert/main.c -lws2_32 -liphlpapi
（输出转换判定与执行结果的自检行）
```

**What just happened.** (1) The check shapes of `CanConvert`/`CanWiden` — the host's dynamic-type "may this value be used that way" check; widen (lossless widening) and plain convert (possibly lossy) are two separate checks — precise semantics never conflated. (2) `Convert` executes — check first, then execute, with an explicit failure path (not a "just try it" implicit conversion). (3) The `convert_string` sample is the text-direction companion (the string↔type bridge — the concrete instance behind the `runtime_type_string`/`type_future` contract cards). This check/execute separation is exactly the runtime-types edition of Chapter 26's "validate, then convert" number parsing.

## Contracts

- **Borrowing of descriptions**: modules own no descriptors; addresses stay stable and unmodified during registration and queries; process-lifetime static descriptions cost nothing.
- **ID derivation**: a custom type's Id must equal `xrtTypeId(AbiName)`; TypeSame compares the two factors Id + ABI name.
- **Inheritance rules**: only CLASS may inherit; FINAL cannot be inherited; derived size/alignment never smaller than any base; depth capped at 255.
- **Reference hard rule**: reference types have Size=sizeof(ptr), Align=pointer alignment; the payload is described separately by InstanceSize.
- **Kind semantics**: the 22 are stable storage kinds — they express neither language visibility nor syntactic aliases.
- **Two operation layers**: Ops (value: copy/compare/hash/format/trace) and InstanceOps (instance: Init/Drop/Trace) are separate.
- **Validation first**: TypeValidate validates fully — invalid descriptions never enter the system.
- **Built-in descriptions**: `xrtTypeInt64()` and friends return process-canonical descriptions — container operations require description-pointer equality (Chapter 117).
- **Registry**: Add/Remove/At/FindName; descriptions are not copied.
- **Conversion**: CanConvert/CanWiden checks separated from Convert execution; ToString/Format for the text direction.
- **Trimming**: `XRUNTIME_FEATURE_RUNTIME_TYPE` depends only on core — the minimal foundation.

## Pitfalls

### Pitfall 1: casually assigning Ids to custom types

Symptom: two modules each with a type `Id = 42` meet — TypeSame collides, containers and the call layer behave erratically.

Cause: Id stability comes from **ABI-name derivation** — the same canonical name yields the same Id (cross-module, cross-process); casual integers carry no such guarantee.

```c bad
xrttype Type = { .Id = 42u, ... };   /* casually assigned: a collision waiting to happen */
```

```c good
xrttype Type = {
	.Id = xrtTypeId(XRT_STR_LITERAL("app.Counter")),
	.AbiName = XRT_STR_INIT("app.Counter"),   /* name and Id paired */
	...
};
```

### Pitfall 2: registering a description table that lives on the stack

Symptom: descriptions fetched from the registry are left dangling — the stack frame dies and the description memory goes with it.

Cause: registration borrows without copying — descriptions must be **process-lifetime stable** (static storage or host-managed long-lived heap).

```c bad
void register_types(void) {
	xrttype T = { ... };            /* on the stack */
	xrtTypeRegistryAdd(&Reg, &T);   /* dangling on return */
}
```

```c good
static const 类型方法表 kCounterMethods[] = { ... };
static xrttype Type = { ... };        /* static storage */
/* or the host's type object holds the description (lifetime tied to the type) */
```

### Pitfall 3: declaring a non-pointer Size for a reference type

Symptom: Validate rejects it, or later object/container layers access out of bounds.

Cause: a reference type's C-ABI storage is always exactly one pointer — Size/Align must be declared truthfully; the payload size goes in InstanceSize/InstanceAlign (the two-layer split must not be mixed).

```c bad
.Id = ..., .Kind = XRT_TYPE_CLASS,
.Size = sizeof(counter), .Align = _Alignof(counter),  /* payload size leaked into value fields */
```

```c good
.Kind = XRT_TYPE_CLASS,
.Size = sizeof(ptr), .Align = _Alignof(ptr),           /* value = pointer */
.InstanceSize = sizeof(counter), .InstanceAlign = _Alignof(counter),  /* payload */
```

## Exercises

### Basic: a built-in type tour

Print the Kind/Size/Align of built-in descriptions like `xrtTypeInt64()`/`xrtTypeString()`/`xrtTypeCallable()` — against the kind table. Acceptance criteria: each kind's storage shape matches the table; TypeValidate passes for all.

### Advanced: a three-type inheritance chain

Base(FINAL=false)→Middle→Leaf, three CLASS layers: verify TypeIsA's chain decisions (Leaf is-a Base), FINAL-inheritance rejection, and depth limits. Acceptance criteria: the three is-a decisions correct; illegal inheritance rejected at Validate/registration time.

### Challenge: a script enum type

Define an ENUM type with payload (Ok/Error/WithValue, three cases) + Metadata carrying the case-name table; implement `print_value(Type, Value)` generic printing (switch Kind → dispatch to format). Acceptance criteria: enum values print their case name; the printer handles a mixed workload of five built-in types (zero type-specific code).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Positioning | the type-facts layer shared by C/XRT/host languages — the single source of type knowledge |
| ID derivation | `xrtTypeId(AbiName)` stable and nonzero; TypeSame = Id + ABI name, two factors |
| 22 kinds | stable storage kinds (value shapes) — not language syntax |
| Two-layer sizes | value (Size/Align) and instance (InstanceSize/Align) separate; a reference type's value = pointer |
| Two operation layers | Ops (value) / InstanceOps (instance) — mirroring the two-layer sizes |
| Inheritance | CLASS only; FINAL not inheritable; depth 255; is-a walks the single chain |
| Description borrowing | registration copies nothing; process-lifetime stable; Validate first |
| Built-in descriptions | the `xrtTypeInt64()` family process-canonical — containers require pointer equality |
| Registry | Add/Remove/At/FindName — interop by canonical name |
| Conversion | CanConvert/CanWiden checks, Convert execution, ToString for text |
