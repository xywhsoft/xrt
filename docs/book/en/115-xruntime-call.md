---
num: 115
slug: xruntime-call
title: xruntime (Part 3): Dynamic Calls
volume: 卷十一 其他扩展库
type: practice
lead: Signatures and call frames, the positional/keyword dual track for arguments, multiple returns with the first four inlined, immutable callables and an atomic call sequence — the standard model of host-function calls.
api: xruntime-runtime_call, xruntime-runtime_type
---

## Orientation

After types (113) and objects (114), the dynamic call takes the stage: scripts must call C functions, C must call back into script functions via a callback, and binding layers must pass arguments between them — `runtime_call` provides the unified call model. The trio: **signature** (`xrtfunctionsig` — borrowing declarations of parameter names/types/return types; named parameters participate in signature identity); **call frame** (`xrtcallframe` — borrowed assembly of Self/positional arguments/keyword arguments/context; `FrameValidate` fully validates: duplicate keywords/required parameters/unknown keywords/varargs flags); **callable** (`xrtCallableCreate(签名, 入口, 环境, 环境析构)` (signature, entry, environment, environment destructor) — immutable after creation, reference counting atomic; the environment is held by the callable). **Result** (`xrtcallresult` — the first four inlined with zero allocation, Set/Push reference semantics, Take transfers). `Invoke`'s six-step atomic sequence (copy frame → validate → private temporary result → failure cleanup → count check → one-shot replacement) guarantees **a failed call never damages the caller's original result**. Key boundary: only type-explicit dynamic entries are provided — no universal calling of arbitrary C ABIs (no libffi dependency).

## Introduction

The naive approach to "dynamic calls" is `void* fn(void** args)` — all type information lost, every binding hand-writing casts, errors exploding deep in the run. The second wrong path is libffi-style universal calling — the abyss of platform ABIs (cdecl/stdcall/fastcall, struct-return conventions… the contract docs say plainly "C cannot safely call arbitrary functions universally without an exact prototype or libffi"). xruntime's third path: **type-explicit dynamic entries** `xrtcallproc` — the compiler, a binding generator, or an FFI module generates one adapter per original function (the adapter has the exact prototype and calls the original function with zero UB), and the dynamic layer sees only adapters. **Signatures circulate in the dynamic layer**: parameter names/types/return types are all declared — the entry receives "a validated frame + a valid signature", and casts happen statically inside the adapter.

**The named-parameter design** deserves note: `KeywordNames`/`KeywordValues` are **borrowed name+value arrays** — not a dictionary! "The caller need not construct a dictionary just to pass kwargs" — zero allocation on the hot path once again (contrast the usual scripting-language practice: build a Map first, then pass it as kwargs).

## Concepts

### Signature and call frame

```diagram flow
- Signature: xrtfunctionsig - arrays of parameters (name/type/optionality) and return types (borrowed from the declarer, lifecycle covering the callable)
  - non-empty parameter names participate in signature identity and must be unique; empty-named parameters pass positionally only
- Frame: xrtcallframe - Self (optional receiver) / Arguments (positional; beyond the formals is varargs)
  / KeywordNames+KeywordValues (one-to-one) / Context (self-describing call chain)
- Validation: FrameValidate - array integrity/nulls/duplicate keywords/required parameters/double-passing/unknown keywords/flag combinations
```

**Three entrances for reading parameters**: `FrameArgument`/`FrameKeyword` (the raw passed-argument views), `FrameParameter` (selects positional/keyword by the effective signature's formal index — **the preferred ordinary entrance**; an unprovided optional parameter returns NULL without setting an error).

### Result: inlining and reference semantics

`xrtcallresult` owns the xvalue references within: **the first four results are inlined** (zero result-array allocation); from the fifth on, a lightweight pointer array (no dynamic Value container built). The operation family: `Set`/`Push` (increments the reference), `SetTake`/`PushTake` (transfers and empties the source slot), `Clear` (releases values, keeps capacity — hot-path reuse), `Unit` (releases everything), `Move` (transfers wholesale). **Only replacing an existing index or appending at the end is allowed** — no sparse writes (the lesson of the old version constructing multiple nulls to fill holes — the return count stays explicit always).

### callable and Invoke's six steps

```diagram state
not-created -> usable: Create(signature, entry, environment, destructor) - immutable; the environment held by the callable
in-call: copy frame + attach signature -> validate -> call the entry with a private temporary result
in-call -> failed: release all partial results + wrap the entry error in an xrt.call cause chain (the caller's original result unharmed)
in-call -> success: with a signature, check the exact return count -> replace the caller's result in one shot
```

**The entry may run concurrently and re-entrantly** — whether the environment is concurrency-safe is the entry implementation's responsibility (the contract draws the boundary). The callable type (`xrtTypeCallable()`): a stable C-ABI slot holding one strong reference — Copy/Clone increments and replaces, Move transfers and empties, Drop releases — callables circulate as **values** in containers/fields (the same pattern as Chapter 114's object values).

### The raw ABI boundary

The old callable stored a mix of cdecl/stdcall/fastcall pointers and a dynamic entry — this module provides **only one type-explicit dynamic entry**. Binding generators emit `xrtcallproc` adapters that call the original function: this boundary **avoids undefined behavior** and **does not impose platform-ABI costs on the basic call layer** (a win for both trimming and portability).

### The call layer's position

```diagram flow
- Signature layer: xrtfunctionsig - borrowed declarations of parameters/returns (names participate in identity)
- Frame layer: xrtcallframe - borrowed assembly of Self/positional/keyword/context + Validate in full
- callable layer: immutable object + atomic references; the environment held by it
- Result layer: xrtcallresult - first four inlined, reference/transfer semantics, replace-or-append only
- Boundary layer: a single type-explicit dynamic entry - original functions attach via adapters
```

## Examples

### First complete program: creating and calling a callable

The program below is from `examples/runtime/call` — the full loop of signature, frame, and call:

```embed path="extlibs/xruntime/examples/runtime/call/main.c" title="extlibs/xruntime/examples/runtime/call/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/call/main.c -lws2_32 -liphlpapi
type=callable
7 + 5 = 12
```

**What just happened.** (1) The creation side: a signature (two int64 parameters, int64 return — borrowed declaration) + an addition entry + an environment compose the callable — `xrtTypeCallable()->Name` prints `callable`: **the callable itself is a typed value** (the CALLABLE kind of Chapter 113's type spectrum). (2) The call side: the frame assembles two positional arguments → `xrtCallableInvoke(pCallable, &Frame, &Result)` → `xrtCallResultGet(&Result, 0)` fetches the first result → `xrtValueGetInt` dereferences — four steps from a dynamic result back to a static int64. (3) Behind `7 + 5 = 12` is the full six-step sequence: the frame validated, the temporary result atomically replaced — **if any step fails, your Result stays untouched** (the next call can safely reuse it). `Unit` to finish + `Unref` the callable — reference discipline identical to Chapter 114's objects.

### Second complete program: boxing callables on the value side

The second program is from `examples/runtime/value_callable` — callables entering the Value world:

```embed path="extlibs/xruntime/examples/runtime/value_callable/main.c" title="extlibs/xruntime/examples/runtime/value_callable/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/value_callable/main.c -lws2_32 -liphlpapi
（输出 callable 装入 Value 后的调用自检结果）
```

**What just happened.** (1) `runtime_value_callable` is an independent trimming layer: a callable's strong reference is boxed into an `xvalue` (Chapter 31's dynamic values, runtime extension) — the script-layer "functions are first-class values" lands here: a callable inside a Value can be stored in a dict, passed to functions, and returned from them. (2) The base `runtime_call` does **not force** Value boxing on — scenarios that don't need first-class functions pay zero boxing cost (Chapter 113's trimming philosophy applied layer by layer). (3) Companion samples: `value_future` (Future boxing), `value_object` (object boxing), `value_type` (type-value boxing) — the complete bridge face between **runtime objects and dynamic values**.

## Contracts

- **Signature borrowing**: signature/type/parameter/return arrays are held by the declarer, lifecycle covering the callable; named parameters participate in signature identity and are unique; empty names positional only.
- **Frames borrow**: Self/argument/keyword arrays/context are all borrowed — valid for the duration of the call.
- **Validation in full**: integrity/nulls/duplicate keywords/required/double-passing/unknown keywords/flag combinations — only a validated frame reaches the entry.
- **Parameter reading**: FrameParameter selects positional/keyword by signature index (preferred); an unprovided optional = NULL without an error; a nonexistent keyword = NULL without an error.
- **Result semantics**: first four inlined; Set/Push increment references, the Take family transfers and empties the source; Clear keeps capacity; replace-or-append only — no sparse writes.
- **Callables immutable**: signature/entry/environment unchanged after creation; reference count atomic; the environment destructed exactly once.
- **Invoke atomic**: the six-step sequence; on failure, partial results released + the cause chain wrapped, the caller's result unharmed; on success, one-shot replacement.
- **Entry discipline**: concurrent and re-entrant; the environment's concurrency safety belongs to the entry implementation.
- **ABI boundary**: exactly one type-explicit entry; original functions attach via adapters — no universal calling, no UB.
- **Boxing independent**: Value callable is an independent trim — zero boxing cost in the base layer.

## Pitfalls

### Pitfall 1: the signature array is a stack temporary

Symptom: the callable reads garbage signatures at call time — the parameter/return arrays from creation time died with the stack frame.

Cause: the signature is **borrowed** — its lifecycle must cover the callable's entire life (the callable does not copy it). Put signatures in static storage or in heap memory that lives as long as the callable.

```c bad
xrtcallable* make(void) {
	xrtfunctionsig Sig = { ...栈上... };
	return xrtCallableCreate(&Sig, entry, NULL, NULL);  /* dangling on return */
}
```

```c good
static const xrttype* const kArgs[] = { ... };       /* static */
static const xrtfunctionsig kSig = { ... };
xrtcallable* C = xrtCallableCreate(&kSig, entry, NULL, NULL);
```

### Pitfall 2: using the result right after a failed call

Symptom: the failure path reads Result — what it gets is **the previous successful call's old value** (atomicity protected it, but it is not this call's result).

Cause: the six-step sequence guarantees "failure does not damage the original result" — the original result may be empty (first call) or an old value; after failure, Result's semantics are "unchanged" — you must take the error branch, not read the result.

```c bad
if ( !xrtCallableInvoke(C, &Frame, &Result) ) {
	use(xrtCallResultGet(&Result, 0));   /* old value/empty - not this call's */
}
```

```c good
if ( !xrtCallableInvoke(C, &Frame, &Result) ) {
	report(xrtErrorMessage(xrtGetError()));   /* error branch: read the error only */
} else {
	use(xrtCallResultGet(&Result, 0));         /* only success carries this call's result */
}
```

### Pitfall 3: sparsely writing results (Set index 2 when only 0 exists)

Symptom: rejected — results allow only replacing an existing index or appending at the end.

Cause: the semantic hole of sparse writes (what is the middle index? null?) was the old design's pit — the new contract keeps "the return count explicit always"; to placehold, explicitly Push empty.

```c bad
xrtCallResultSet(&Result, 2u, Value);   /* out-of-range sparse: rejected */
```

```c good
/* append in order - count and order both explicit */
xrtCallResultPush(&Result, V0);
xrtCallResultPush(&Result, V1);
xrtCallResultPush(&Result, V2);
```

## Exercises

### Basic: an addition callable on both argument tracks

Same entry: call with positional arguments + call with keyword arguments (name-array passing) — FrameParameter reads both tracks. Acceptance criteria: both tracks resolve the same parameters; unknown keywords rejected by Validate.

### Advanced: optional parameters and defaults

A three-parameter signature (one required, two optional): the entry takes defaults for unprovided parameters — FrameParameter's NULL semantics. Acceptance criteria: all three call shapes (all given/one given/required only) behave correctly; double-passing (positional + keyword of the same name) rejected.

### Challenge: a script function table

A dict (Chapter 32) storing a name→callable registry; implement a mini-interpreter for `call(name, 帧字符串)` (frame string): table lookup → frame → Invoke → print results. Add one C primitive function (e.g., `max(int64,int64)`) registered via an adapter. Acceptance criteria: three functions registered and callable; unknown names report cleanly; concurrent calls of the same callable with zero errors (the entry is stateless).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| The trio | signature (borrowing declaration) / frame (borrowed assembly) / callable (immutable + atomic references) |
| Named parameters | name+value borrowed arrays — not a dictionary; names participate in signature identity, unique; empty names positional only |
| Validation | Validate in full: duplicate keywords/required/unknown/varargs combinations |
| Parameter reading | FrameParameter by signature index (preferred); an unprovided optional = NULL without an error |
| Result | first four inlined; Set/Push reference, Take transfer, Clear keeps capacity; replace/append only |
| Invoke's six steps | copy frame → validate → temporary result → failure cleanup → count check → one-shot replacement |
| Atomicity | failure never damages the caller's original result; errors wrapped in an xrt.call cause chain |
| Entry | concurrent and re-entrant; the environment's concurrency belongs to the implementation |
| ABI boundary | exactly one type-explicit entry; original functions via adapters — no UB, no platform cost |
| Boxing | Value callable is an independent trim — first-class functions optional |
