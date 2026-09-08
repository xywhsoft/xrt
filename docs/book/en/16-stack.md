---
num: 16
slug: stack
title: The Stack Family: stack / fixed / block and the Pointer Variant
volume: 卷三 容器与数据结构
type: practice
lead: Four stacks for four constraints — dynamic growth, zero-allocation fixed capacity, stable-address block chaining, and the pointer-element variant.
api: stack
---

## Orientation

The stack is computer science's smallest container, yet XRT gives it four variants — because the "stack" need looks different under different constraints. Conclusion first: the dynamic stack `xstack` for the vast majority of scenarios; switch to the fixed stack when "depth known + hot path"; the block stack when element addresses must stay stable long-term; the pointer variant for holding object references. This chapter's task is making that selection a derivable judgment. The **dynamic stack** `xstack` pushes as much as you like; the **fixed stack** `xfixedstack` binds to a caller buffer with zero allocation throughout; the **block stack** `xblockstack` keeps **existing element addresses unchanged** while growing; the pointer variant holds `ptr`. Two complete examples explain the two engineering constraints "zero allocation" and "stable address", a decision table settles the four-way choice, and the mechanical recipe of "recursion into an explicit stack" is tabulated — it is the foundation move of safe parsers. Recursive-descent parsing and backtracking traversal run through the examples as the application scenario — they are also the main data structures of Chapter 31's parser and the latter half of Volume 3. Read this chapter carrying one concrete question: "if the input nests ten thousand deep, what happens to my stack?" — every variant answers differently, and that is the whole of selection.

## Introduction

A recursive-descent parser must save the "current scan position" at every recursion level — this scenario reappears in Chapter 31's JSON parser, where you will see how the real frame structure corresponds to the simulation here. Local variables on the call stack? Recursion depth is decided by the input (the nesting depth of Chapter 3's resource limits), and stack overflow is then an attack surface. A hand-written loop instead of recursion? Then you need an explicit frame stack. How to choose the frame stack's constraints: a parser frame is a few dozen bytes, the maximum depth bounded by resource limits (say 128) — **depth known, hot path**: the fixed stack bound to a stack array, zero heap allocation throughout. And when traversing a tree the user mutates at will, must the node addresses held mid-traversal not go stale on stack growth? — no: stack growth invalidates the addresses of the **stack's own elements**, not the tree nodes'; but if the stack holds "pointers into an in-stack work area", the block stack's stable addresses become a hard requirement.

Two scenarios, two answers — exactly why the family exists: **constraints drive selection**, not one universal stack conquering all. Unfolding "recursion into an explicit stack" one step further: any recursive algorithm can be mechanically rewritten as "loop + explicit stack" — the "function-call level" becomes "push", "return" becomes "pop". After the rewrite, the depth cap moves from the system call stack (uncontrolled) to your stack capacity (explicit and controllable) — precisely why Chapter 3's resource limits can land: the parser declares "max depth 128", the frame stack is provisioned for 128, and over-deep input is refused at level 128 instead of eating through the process stack.

## Concepts

### The four variants in one table

| Variant | Storage | Growth | Element addresses | Fits |
| --- | --- | --- | --- | --- |
| `xstack` dynamic | self-managed heap buffer | auto-grows | invalid after growth | the general default |
| `xfixedstack` fixed | caller buffer | full means failure | stable while bound | known depth, zero-allocation hot paths |
| `xblockstack` block | block chain | adds blocks, no relocation | **never invalid** | element addresses held long-term |
| pointer stack | the three containers above + `ptr` elements | same as host | the pointer itself stable | holding object references |

### The life of a frame stack

Using a parser's frame stack, put the four variants into the same workflow:

```diagram flow
- Start: Init binds storage — the fixed stack binds a local array, the dynamic starts empty
- Push a frame: Push writes an element copy — the fixed stack allocates nothing, the dynamic grows on demand
- Backtrack: Pop removes the newest frame — LIFO preserves the recursion shape
- Boundaries: full-stack Push fails (fixed) / empty-stack Pop fails (all)
- Finish: Unit returns — the fixed stack leaves the caller buffer alone, the dynamic frees its own
```

### The zero-allocation contract

The fixed stack's `Init` binds the handle to a caller-provided buffer (a local array, static storage, or an arena block all work), after which Push/Pop operate entirely on that memory — **never allocating**; the repository's `_noalloc` contract tests guarantee it (a sibling of Chapter 6's fault injection: one proves "failure paths correct", the other "the zero-allocation path truly exists"). Full-stack Push fails and empty-stack Pop fails — both boundaries carry clear errors, no overrun, no crash.

### Stable addresses: the block stack

The dynamic stack relocates wholesale on growth, invalidating all element pointers. When does that become a problem? When "stack elements are referenced from outside the stack" — the most typical form being "the stack is the work area": each pushed work frame is back-referenced by deeper frames (a tree's parent pointers, a parser's upstream context), and relocation dangles all those back-references at once. The block stack splits storage into a block chain; growth hangs a new block while old blocks stay put, cancellation of relocation at the root. The block stack `xblockstack` splits storage into a block chain — growth hangs a new block and **old blocks stay put** — element addresses taken from the stack remain valid for the stack's whole lifetime. "The stack also holds pointers to stack elements" sounds rare, but in the "stack as work area" pattern (each recursion level's work frame referenced by deeper levels) it is a real requirement. The price: traversal crosses blocks, constants slightly higher.

### LIFO semantics and error hygiene

Popping an empty stack returns `false` and leaves a record in the thread error slot — the example's `xrtClearError` after the loop clears this "expected failure". It is Chapter 4's "success doesn't clear the slot" discipline in daily use: **control the loop with return values, diagnose with the error slot, and actively clear the residual error once the boundary is reached** — never let it leak into the next stretch of logic.

## Examples

### Complete program: a zero-allocation work stack

From the repository example `examples/containers/fixed_stack/main.c`, simulating a parser's frame stack:

```embed path="examples/containers/fixed_stack/main.c" title="examples/containers/fixed_stack/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/fixed_stack/main.c -lws2_32 -liphlpapi
function=3 position=30
function=2 position=20
function=1 position=10
```

**What just happened.** (1) `pStorage[8]` is the stack's physical storage — an ordinary local array; `xrtFixedStackInit`'s parameters are "handle, buffer, capacity in bytes, element size". (2) After pushing three frames and popping them, the output is the `3/2/1` LIFO order — last-in-first-out is the stack's entire semantics and the easiest to "casually" break (if you want queue order while using a stack, you want Chapter 21's queue). (3) When emptied, Pop returns `false` ending the loop, and `xrtClearError` clears the empty-stack record — see "error hygiene" above. (4) Capacity 8, three pushed — pushing a ninth would fail rather than overrun: **the cap is the capacity's defense line**; with Chapter 3's resource limits, specs like "parse depth ≤ 128" gain a physical carrier. Quick accounting: 128 frames × 16 bytes = 2 KB — one stack array holds the whole working state, without even an arena (Chapter 7); that is what "zero allocation" looks like on the memory ledger.

### Complete program: the block stack's stable addresses

From `examples/containers/block_stack/main.c`, verifying that element addresses don't change during growth — the machine's testimony for the "never invalid" promise:

```embed path="examples/containers/block_stack/main.c" title="examples/containers/block_stack/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/block_stack/main.c -lws2_32 -liphlpapi
count=21 blocks=6 root=100 stable=yes
```

**What just happened.** (1) Twenty-one elements pushed (including a root value of 100), storage automatically splitting into 6 blocks — zero relocation across growth. (2) The program noted an element's address mid-push and read it back after all pushes to verify the content remains reachable: `stable=yes` is the empirical proof of "element addresses never invalid". (3) The control experiment deserves a hands-on run: the same sequence on the dynamic stack and the addresses changed worlds long ago. When is this property worth money? When stack elements are referenced from structures outside — say tree algorithms whose traversal work frames are referenced by parent frames.

### The stack-versus-recursion correspondence table

Rewriting a recursive function into an explicit stack has a fixed recipe: the function's "per-level local state" becomes a pushed frame structure (the element type); "the recursive call" becomes "push + continue"; "return to the parent" becomes "pop"; "the entry parameters" become "the initial frame". Side by side: local variable table → frame structure fields, call-stack depth → the stack's Count, stack-overflow risk → the capacity cap (explicit, configurable, refusable). This table is used directly in Chapter 31's parser and Volume 3's AVL traversals — recursion into an explicit stack is not an exercise but the safety baseline for parsers handling untrusted input.

## Contracts

- **Lifecycle pairing**: `Init`/`Unit` paired; the fixed stack's `Unit` does not free the caller buffer (it never owned it).
- **Zero allocation**: once bound, the fixed stack's Push/Pop never allocate; full-stack and empty-stack failures both carry clear errors.
- **Address validity**: the dynamic stack invalidates on growth; the fixed is stable while bound; the block never invalidates — at selection time first ask "will stack elements be referenced from outside".
- **Error hygiene**: control flow by return values; clear residual boundary errors when done (`xrtClearError`).
- **Pointer variant**: the pointer stack manages only pointer slots; element ownership and freeing lie entirely with the caller.

## Pitfalls

This chapter's two pitfalls: one about address validity (same root as Chapter 14's array Pitfall 1 — the fate of contiguous storage), one about capacity detached from spec. The former is a technical pit; the latter an engineering pit — engineering pits are usually more expensive.

### Pitfall 1: using element pointers after dynamic-stack growth

Symptoms: the same dangling-pointer family as the array's, appearing in code that "casually notes an in-stack element's address while pushing".

Cause: the dynamic stack's elements live in its own buffer; growth relocates wholesale.

```c bad
exampleframe* pTop = (exampleframe*)xrtStackPeek(&tFrames, 0);
xrtStackPush(&tFrames, &tNext);      /* growth relocates */
pTop->Position = 42;                  /* dangles */
```

```c good
xrtStackPush(&tFrames, &tNext);      /* push first */
exampleframe* pTop = (exampleframe*)xrtStackPeek(&tFrames, 0);
/* need long-term stable addresses: switch to xblockstack or xfixedstack */
```

### Pitfall 2: sizing the fixed stack by "enough for now"

Symptoms: a deeper-nesting input arrives, and Push fails mid-parse; the error path treats half-parsed state as complete state.

Cause: the fixed stack's cap is a physical fact set at creation; "enough for now" is not a spec — Chapter 3's resource limits (`iMaxDepth`) are.

```c bad
exampleframe pStorage[4];            /* test cases nest 3 deep — explodes on day one */
xrtFixedStackInit(&tFrames, pStorage, sizeof(pStorage), sizeof(exampleframe));
```

```c good
exampleframe pStorage[128];          /* aligned with the resource limit's iMaxDepth */
xrtFixedStackInit(&tFrames, pStorage, sizeof(pStorage), sizeof(exampleframe));
/* before parsing, check input depth <= 128 (one of the three resource gates); over-limit refuses whole */
```

## Exercises

### Basic: three LIFO frames

Reproduce the main example: push three frames, pop them all, print in pop order (wrong order means you want a queue, not a stack — Chapter 21 awaits); then verify the fourth Pop returns `false` with `Count` zeroed.

### Advanced: a bracket matcher

Write a bracket matcher with the fixed stack: scan `((a)(b))`, push positions on left brackets, pop a pair on right; a right bracket with an empty stack, or a non-empty stack at the end, both report the error position. Hint: the stack element stores "position + bracket kind"; size by the maximum nesting depth.

### Challenge: a relocation comparison of two stacks (quantifying address stability)

For the same input (push 1000 elements, recording the top element's address every 100) run the dynamic and block stacks separately, outputting address-change counts. Acceptance: the dynamic changes addresses several times, the block always zero; then measure both versions' total push time and write your quantified conclusion on "the price of stable addresses".

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four variants | dynamic `xstack` (general) / fixed `xfixedstack` (zero allocation) / block `xblockstack` (stable addresses) / pointer stack (holding references) |
| Fixed stack | `Init(句柄, 缓冲, 字节数, 元素大小)`; capacity sourced from the resource limit's depth cap |
| Boundaries | full-stack Push fails, empty-stack Pop fails; control flow by return values, both boundaries with clear error categories |
| Address validity | invalid on growth (dynamic) / stable while bound (fixed) / never invalid (block) |
| Error hygiene | residual boundary errors cleared with `xrtClearError` when done |
| Selection order | zero allocation prefers fixed; address stability uses block; unconstrained uses dynamic |
| Capacity alignment | fixed-stack capacity = the resource limit's depth cap; the two numbers must share one source (one constant, two references) |
| The rewrite formula | recursion into explicit stack: call level = push, return = pop; depth moves from the system stack to explicit capacity |
| Error hygiene | empty-stack Pop returns false to control the loop; residual errors actively cleared, never leaked to the next stretch of logic |
