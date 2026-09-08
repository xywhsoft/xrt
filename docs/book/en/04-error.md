---
num: 4
slug: error
title: The Error Model: xerror in Full
volume: 卷一 起步与核心
type: concept
lead: Structured error objects, cause chains, and the thread error slot — the unified failure-reporting model across the whole XRT library.
api: error, error_format, core
---

## Orientation

This chapter is one of the book's foundations. Nearly every fallible function in XRT reports failure the same way: return `false` or `NULL`, and place a structured `xerror` object into the current execution context's error slot. You already saw `xrtGetError()` called in Chapter 3; this chapter lays the model out completely — which fields make up an error, how the cause chain expresses "what actually went wrong below", who owns the error object, what the thread error slot's lifecycle is, and the engineering discipline of cross-thread transfer and wrapping timing. Every later chapter (networking, TLS, tasks) reuses these rules: you will see the combination "failure by return value, details in the error slot, layering by cause chain" over and over, so this chapter deserves a thorough read.

## Introduction

Start with a scenario every C programmer has met: you wrote a configuration-loading function.

```c
str load_config(const char* sPath);
```

It returns `NULL` when the file doesn't exist. The caller gets `NULL` — and then? "File missing", "file exists but the JSON is invalid", and "out of memory" look identical in the return value. A common patch is printing a log line — but logs are for humans; upper layers cannot branch on them. Or define a pile of negative error codes — but each layer's code space collides with the others, and context like "line N, column M of the file is wrong" has nowhere to live.

Layering makes it worse. The config loader internally reads the file and parses JSON: when parsing fails, what the upper layer really needs to know is "config load failed, because line 3 of the JSON has a syntax error" — information from both layers must survive, not collapse into a bare `parse failed`.

Traditional solutions are each broken in their own way. Returning `int` error codes makes every module maintain a code table; crossing layers either loses context or collides code spaces. Printing straight to `printf` / stderr welds the information into the presentation layer — callers get neither category nor branchability. An `errno`-style global needs platform special-casing under threads and carries a single integer at a time. Fundamentally, these answers only say "it failed" — never "which layer, why, and what next".

These three problems — **classification, context, layering** — are what the XRT error model solves. It uses no error-code integer; instead an immutable object carries structured information and a human message together, and each execution context offers a "current error" slot, so a `false` return and the error details travel as a pair automatically.

## Concepts

### What's inside an error object

`xerror` is immutable and safely holdable across threads. Once created, nobody can mutate its fields, which makes "passing it around" forever safe. Each error consists of:

- **Generic kind** (`xerrkind`): a coarse classification stable across modules, e.g. `XERR_TIMEOUT`, `XERR_NOT_FOUND`. It is for control flow — decisions like "retry or not" and "degrade or not" look only at the kind.
- **Domain**: a module-namespace string like `"xrt.json"`, `"example.net"`. The same numeric code means different things in different domains; the domain keeps codes from colliding.
- **In-module error code**: each domain numbers its own from 1, for precise module-side decisions.
- **System code, operation name, UTF-8 message**: bridging platform errno semantics, naming the failed operation, and a human-readable description respectively.
- **Optional data and the cause chain**: machine-readable extras (say, the JSON error position) plus a link to the lower-layer error.

One discipline runs throughout: **kind and domain are for machine decisions, messages are for display only**. Never write code that parses error-message strings — branch on the kind when that's what you need, and read the structured fields for details.

The kind set is deliberately small and stable; the commonly used ones:

| Group | Kind | Meaning |
| --- | --- | --- |
| Input violates contract | `XERR_ARGUMENT` `XERR_TYPE` `XERR_VALUE` `XERR_RANGE` | Null parameter, type mismatch, illegal value, out of range |
| State disallows | `XERR_STATE` | Object or runtime state doesn't support this operation |
| Resources | `XERR_MEMORY` `XERR_IO` `XERR_NOT_FOUND` `XERR_EXISTS` `XERR_PERMISSION` | Out of memory and system-resource errors |
| Retry later | `XERR_AGAIN` | Object still valid; retry after waiting |
| Wait terminated | `XERR_TIMEOUT` `XERR_CANCELLED` `XERR_CLOSED` | Timeout, cancellation, resource closed |
| Protocol & capability | `XERR_PROTOCOL` `XERR_UNSUPPORTED` `XERR_INTERNAL` | Protocol error, capability missing, internal invariant broken |

`XERR_NONE` only means "no error" and cannot be used to create an error object.

### The cause chain: a layered story of failure

`xrtErrorWrap` "wraps" a lower-layer error into a new upper-layer error, forming a cause chain. Back to the opening config example, the chain looks like this:

```diagram flow
- Lower layer: JSON parse failed: the xrt.json domain records the line/column position
- Wrapping: config load failed: the example.config domain wraps the parse error in XERR_IO
- Decision: xrtErrorIs sees XERR_IO at the top; the chain can be followed down to the syntax error
- Display: xrtErrorMessage reads only the top message; the logging layer may traverse the whole chain
```

Deciding on the chain takes no manual traversal: `xrtErrorIs(错误, XERR_TIMEOUT)` searches the chain for the given kind and returns a borrowed pointer to the hit (or `NULL`). "Was this failure ultimately caused by a timeout" is one line of code. `xrtErrorFind` searches precisely by domain and code.

### When to wrap: errors gain semantics at boundaries

A cause chain does not mean wrapping at every level. A practical discipline: **wrap at module boundaries, pass through inside modules**. An `xrt.json`-domain error raised inside the JSON module crosses the "config loading" boundary and becomes an `example.config`-domain "config load failed" — from then on the upper layer's retry and degradation strategy has a clear decision point: is it `XERR_IO` (retrying makes sense) or `XERR_ARGUMENT` (retrying is pointless — report a configuration error)? Low-level calls between boundaries need no extra "call failed" wrapper; otherwise the chain bloats with information-free duplicate nodes and the real semantics dilute.

The touchstone for "should I wrap": if the new layer can **add decision information** (a new domain, a new semantic kind, a new recovery strategy), wrap; if it merely restates "it failed below", don't — let the original error keep rising.

### Division of labor between errors and logging

Error objects "carry failure structurally"; logging "leaves a trail of execution" — do not substitute one for the other. A common anti-pattern is printing the error message at every layer — the same event appears three times in the log, and troubleshooting loses the structure. A better division: errors pass quietly along the call chain and get handled at **decision points** (retry, degrade, final failure exit); logging records only at decision points and key boundaries, carrying the full kind and cause chain. Chapter 37's Logger can write `xerror`'s structured fields into log records; for now, remember the division of labor.

### The thread error slot: the current error

Every execution context owns one current-error slot. Plain threads default to the thread context; coroutines and task schedulers switch to their own slots, so coroutine migration never pollutes the hosting thread and tasks never cross-talk. The slot's semantics:

```diagram state
unset -> set: SetError (the slot adds an object reference)
set -> unset: ClearError / SetError(NULL)
set -> unset: TakeError (takes ownership away)
```

`xrtGetError` only borrows and changes nothing. Three key rules:

- `xrtSetError` **adds** a reference to the passed object and replaces the current error; passing `NULL` is equivalent to clearing. On replace or clear, the slot releases the reference it held.
- `xrtTakeError` takes the current reference and empties the slot — use it to keep an error long-term (log queues, cross-thread handoff) and remember `xrtErrorFree` when done.
- Objects still sitting in the default slot when a native thread exits are freed automatically; extension threads need not force-clear errors to avoid leaks.

One default that's easy to trip on: **successful operations do not implicitly clear an old error**. The slot may still hold the previous failure — only after a function declares failure via its return value are you entitled to read the slot.

### Cross-thread and long-term storage

`xerror` is immutable, so the object itself is safe to hold across threads; the only things needing care are "who frees it" and "which context owns the slot". To carry a failure from a worker thread back to the main thread (say, an error attached to a task result), the correct posture is `xrtTakeError` to take ownership, pass it along with the result, and `xrtErrorFree` at the consumer; storing the borrowed pointer from `xrtGetError()` is a race — the slot may be replaced at any moment. Coroutine and task error slots are managed by the scheduler and migrate with the execution context — you never carry them; only native threads need explicit handoff. Reference counting goes through `xrtErrorRef`/`xrtErrorFree` with the same rules as the general reference-counted objects of Chapter 5.

### The one-step form for common failures

Most of the time you don't need the manual "create, set, free" trio. `xrtSetErrorInfo` creates and hands over in one step:

```c
xrtSetErrorInfo(XERR_ARGUMENT, "app.config", 1, "path is empty");
```

For dynamic messages use `xrtSetErrorFormat` from the `error_format` module (printf rules, rejecting write-side-effect `%n`; on formatting failure it leaves an allocation-free `XERR_MEMORY`). Both helpers create immutable objects and hand them straight to the current context. To carry the source position, use `xrtErrorBuildAt` and read it back through `xrtErrorFile`, `xrtErrorLine`, `xrtErrorColumn`.

## Examples

### Complete program: create, wrap, and query a cause chain

The following program comes from the repository example `examples/core/error/main.c`, demonstrating a full error lifecycle under the ownership model. Read the code first, then see what it does:

```embed path="examples/core/error/main.c" title="examples/core/error/main.c"
```

```term
$ gcc -O1 -I single impl.c examples/core/error/main.c -lws2_32 -liphlpapi
error: request failed
timeout cause: yes
```

**What just happened.** The program takes four steps. First, `xrtErrorCreate` builds the lower-layer error (network timeout) from "kind + domain + code + message", returning an **owning** pointer — whoever creates it frees it. Second, `xrtErrorWrap` wraps it into an upper-layer `XERR_IO`; Wrap internally **adds a reference** to the cause, so the creator immediately `xrtErrorFree`s its own copy — whether the count reaches zero is the chain's business. Third, `xrtSetError` places it in the thread slot — note SetError also adds a reference, so the slot has its own copy; the creator releases its copy again, and at this moment the object has two referents (the chain and the slot). Fourth, `xrtGetError` borrows for reading: `xrtErrorMessage` never returns `NULL` (it returns `(no error)` when unset), and `xrtErrorIs` walks the chain and hits `XERR_TIMEOUT`. Finally `xrtClearError` empties the slot and releases the slot's reference.

### Complete program: printf-style one-step construction

The second program comes from `examples/core/error_format/main.c`, showing convenient construction of dynamic messages:

```embed path="examples/core/error_format/main.c" title="examples/core/error_format/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/core/error_format/main.c -lws2_32 -liphlpapi
file does not exist: app.json
```

The difference from `xrtErrorCreate` is the message source: Create takes a complete string, Format takes a printf format string plus variadic arguments and internally reuses the string-formatting module before creating — error messages can carry runtime context (here, the file name) without the caller pre-assembling a buffer.

Note the second program never shows a single `Free`: `xrtSetErrorFormat`, like `xrtSetErrorInfo`, **hands over** to the current context right after creating — the creator no longer holds a reference, and the final `xrtClearError` releases the slot's copy. Compared with the first program's three steps of "create → set → free your copy", the one-step form saves more than lines — it eliminates the entire error class of "forgetting to free your copy" (Pitfall 2 of this chapter). In everyday code, prefer the helpers unless you are storing the error in a structure or carrying it across threads.

## Contracts

- **Ownership**: `Create`/`Wrap` return owning pointers that must see exactly one `Free`; `Wrap` adds a reference to the cause; `SetError` adds a reference to the passed object; `GetError` only borrows.
- **Threading**: `xerror` is immutable and holdable across threads; each execution context has its own error slot; to cross threads, `TakeError` first.
- **Error convention**: `false`/`NULL` return values declare failure and the slot provides details; success does not clear the slot; read the slot only after seeing a failure return.
- **Display/judgment separation**: control flow reads only the kind (domain+code when needed); messages and positions are for display and logs only.
- **Extension data**: modules needing structured extras should define a stable format for `Data` instead of asking callers to parse display messages; `xrtErrorBuildAt` carries the source position, read back via `xrtErrorFile`/`xrtErrorLine`/`xrtErrorColumn`.
- **Minimal dependencies**: the core error API depends on no containers, strings, or printf runtime; dynamic formatting is the separate `error_format` convenience module.

## Pitfalls

### Pitfall 1: freeing a borrowed error

Symptoms: occasional crashes or heap corruption, often near code that "reads the error then cleans up".

Cause: `xrtGetError` returns a borrowed pointer; the slot may replace or clear it (releasing that reference) at any time; calling `xrtErrorFree` on a borrowed pointer is a double free.

```c bad
xerror* pError = xrtGetError();
printf("%s\n", xrtErrorMessage(pError));
xrtErrorFree(pError);   /* freed a borrow — double free when the slot clears */
```

```c good
/* Use a borrow immediately; to hold it long-term, take ownership */
printf("%s\n", xrtErrorMessage(xrtGetError()));
/* or: xerror* pKeep = xrtTakeError(); ... xrtErrorFree(pKeep); */
```

### Pitfall 2: forgetting to release your own cause reference after Wrap

Symptoms: slow memory growth; leak detectors report `xerror` objects that only increase.

Cause: after `xrtErrorWrap` **adds** a reference to the cause, the creator's original reference still exists and must be released by the creator — otherwise the cause object never reaches zero.

```c bad
xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "net", 1, "timeout");
xerror* pError = xrtErrorWrap(pCause, XERR_IO, "client", 2, "request failed");
/* missing xrtErrorFree(pCause): the cause object leaks */
```

```c good
xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "net", 1, "timeout");
xerror* pError = xrtErrorWrap(pCause, XERR_IO, "client", 2, "request failed");
xrtErrorFree(pCause);
```

### Pitfall 3: treating "the slot has an error" as the failure signal

Symptoms: the function clearly succeeded, yet the upper layer reports a stale error; error-handling logic works intermittently.

Cause: successful operations don't implicitly clear old errors, and the slot may still hold the previous failure's object. Using "slot non-empty" as the failure criterion is deciding on stale state.

```c bad
call_something();              /* returned true — success */
if ( xrtGetError() != NULL ) { /* misjudged: this is left over from a previous failure */
	recover();
}
```

```c good
if ( !call_something() ) {     /* see the failure return first */
	report(xrtErrorMessage(xrtGetError()));
}
```

## Exercises

### Basic: create and read

Write a program: set an `XERR_ARGUMENT` error with `xrtSetErrorInfo`, print the message for its kind, then `xrtClearError` and confirm `xrtErrorMessage(xrtGetError())` returns `(no error)`.

### Advanced: a three-layer cause chain

Build the three-layer chain "`服务启动失败 → 配置加载失败 → JSON 第 N 行语法错误`": the bottom layer via `xrtErrorCreate` (not Wrap), the middle and top via `xrtErrorWrap`. Query the bottom layer's kind with one `xrtErrorIs` call and print whether it hit. Hint: release your own cause reference immediately after each Wrap to avoid leaks.

### Challenge: position-carrying failure logging

Implement a `log_failure(const char* sFile, int iLine)` macro: the failure path calls `xrtErrorBuildAt` (carrying the call-site source position automatically), then prints "file:line + message + every chain layer's message". Acceptance: build a two-layer cause chain and see both layers' messages plus the call-site file and line in the output; the position read back via `xrtErrorFile`/`xrtErrorLine` matches where the macro expanded. When done, return to the chapter's opening config-loading scenario and rewrite it to return structured errors — you will have a failure-handling skeleton ready for a real project.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Creation | `xrtErrorCreate` (owning) / `xrtSetErrorInfo` (one-step handover) / `xrtSetErrorFormat` (printf message) |
| Layering | `xrtErrorWrap` wraps a cause and adds a reference; `xrtErrorIs` searches the chain by kind; `xrtErrorFind` by domain and code |
| Slot semantics | `SetError` adds a reference and replaces; `TakeError` takes ownership; `ClearError` empties and releases; `GetError` only borrows |
| Decision timing | Read the slot only after a failure return; success doesn't clear |
| One-step construction | `xrtSetErrorInfo` / `xrtSetErrorFormat` create and hand over — no leak window |
| Cross-thread | Objects are immutable and cross-thread safe; carry via `TakeError`, `Free` at the consumer |
| Wrapping discipline | Wrap at module boundaries, pass through inside; no new semantics, no wrap |
| Source position | Written by `xrtErrorBuildAt`, read back by `xrtErrorFile`/`xrtErrorLine`/`xrtErrorColumn` |
| Ownership | The creator must `Free` exactly once; borrows are never freed; `TakeError` before crossing threads |
| Display | `xrtErrorMessage` never `NULL`; kinds for control flow, messages for humans |
