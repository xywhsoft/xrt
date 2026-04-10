# XRT Best Practices Guide

> Current-mainline practices for runtime, structured data, concurrency, networking, and cross-platform development

[English Version](BEST_PRACTICES.en.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Runtime and Error Handling](#runtime-and-error-handling)
- [Temporary Memory and Ownership](#temporary-memory-and-ownership)
- [Memory Debugging and Dangerous Operation Detection](#memory-debugging-and-dangerous-operation-detection)
- [Structured Data Mainline](#structured-data-mainline)
- [Shared Root and Cross-Thread Rules](#shared-root-and-cross-thread-rules)
- [Thread, Coroutine, and Async Mainline](#thread-coroutine-and-async-mainline)
- [Network and TLS Mainline](#network-and-tls-mainline)
- [Template and JSON Mainline](#template-and-json-mainline)
- [Performance Practices](#performance-practices)
- [Cross-Platform Practices](#cross-platform-practices)

---

## Runtime and Error Handling

### 1. Always treat the runtime as an explicit execution context

If code uses runtime-bound features such as:

- `xrtGetError()`
- `xrtTempMemory()`
- `xrtRand32()` / `xrtRand64()`
- coroutine runtime
- future/task/wait-source helpers

then the thread must be inside the XRT runtime.

### 2. Pair `xrtInit()` and `xrtUnit()`

```c
int main(void)
{
	xrtInit();

	/* ... program logic ... */

	xrtUnit();
	return 0;
}
```

### 3. Use `xrtGetError()` instead of touching runtime internals

```c
if ( pValue == NULL ) {
	printf("error: %s\n", (char*)xrtGetError());
}
```

### 4. For host-created threads, attach explicitly

Threads created by `xrtThreadCreate()` are attached automatically.  
External threads should use:

```c
xrtThreadAttachCurrent();
/* ... */
xrtThreadDetachCurrent();
```

---

## Temporary Memory and Ownership

### 1. Use `xrtTempMemory()` only for short-lived, same-thread temporary work

Good fit:

- formatting intermediate text
- temporary buffers
- local conversion output

Bad fit:

- storing long-lived data
- sharing across threads
- returning as if it were owned heap memory

### 2. Prefer clear ownership rules

When an API returns data, know whether it is:

- owned and must be released
- borrowed
- temporary

### 3. Distinguish `Get*` and `Peek*`

Current mainline convention:

- `Get*` usually returns a retained or owned reference
- `Peek*` usually returns a borrowed reference

Do not treat them as interchangeable.

---

## Memory Debugging and Dangerous Operation Detection

XRT memory facilities are not only about allocation speed.  
They are also designed to help expose mistakes early.

Current memory-related debugging capability can help with:

- locating memory leaks back to source file and line
- detecting double free
- detecting invalid free
- detecting block corruption caused by out-of-bounds writes
- exposing use-after-free style lifecycle errors
- diagnosing wrong-thread access to thread-local or local-root objects
- surfacing container misuse that would otherwise silently corrupt internal state

### Recommended practice

- keep debug-oriented builds available during development
- prefer XRT-managed allocation paths when you want diagnostics
- do not bypass runtime ownership rules just to “save a call”

---

## Structured Data Mainline

### 1. Use `xvalue` as the internal structured data model

For modern XRT application code, the preferred internal model is:

- `xvalue`
- `table`
- `array`
- `list`
- `coll`

### 2. Treat JSON as an exchange format

Recommended:

- parse JSON into `xvalue`
- process data in `xvalue`
- stringify back to JSON at the boundary

### 3. Choose the right container

- `table / dict`: keyed lookup
- `array`: contiguous indexed storage
- `list`: ordered structure with different update characteristics
- `coll`: deduplicated or set-like semantics

---

## Shared Root and Cross-Thread Rules

### 1. Local is the default

The default assumption is:

- newly created root objects are local
- cross-thread mutation is not allowed unless explicitly shared

### 2. Use explicit shared roots when crossing threads

```c
xvalue pShared = xvoCreateTableEx(XRT_OBJMODE_SHARED);
```

### 3. Do not expect hidden lock-free magic

Shared mode does not mean every view operation is universally safe without rules.

### 4. Let wrong-thread misuse fail loudly

The current mainline prefers to reject invalid cross-thread mutation rather than silently corrupt internal state.

---

## Thread, Coroutine, and Async Mainline

### 1. Prefer one unified async model

Current mainline is built around:

- thread runtime
- coroutine runtime
- future / task / promise
- wait-source

### 2. Use task/future for structured async workflows

Prefer:

- `xTaskRunThread`
- `xTaskRunCo`
- `xTaskRunEngine`
- `xFutureWait*`
- `xTaskGroup*`

### 3. Use task groups for scope control

If a set of child async operations belongs to one logical scope, prefer:

- `xTaskGroupCreate`
- `xTaskGroupRun*`
- `xTaskGroupJoin*`
- `xTaskGroupClose`
- `xTaskGroupDestroy`

### 4. Prefer current-mainline continuation targets

Continuation targets already have an explicit model:

- inline
- current-thread deferred
- engine
- coroutine scheduler

---

## Network and TLS Mainline

### 1. Use `xnet-v2` as the active network mainline

For new code, prefer:

- `xnet-v2`
- stream / dgram / listener
- future / wait-source integration

### 2. Use TLS session mainline

Current public TLS mainline is:

- `xtlssession`
- `xrtNetTlsSession*`

### 3. Prefer wait-source based waiting

For network waiting logic, the recommended shape is:

- network object
- wait-source or future
- thread/co wait through unified wait APIs

---

## Template and JSON Mainline

### 1. Keep one data model through the pipeline

Recommended chain:

- HTTP input
- JSON parse
- `xvalue`
- business logic
- template render or JSON stringify

### 2. Use template for rendering, not for business logic

Templates can do selection, loops, and formatting, but core business logic should remain in C and in runtime data structures.

### 3. In AI flows, keep transport and structure separate

Recommended pattern:

- `xhttp` / `xtlssession` for transport
- `json` for exchange
- `xvalue` for internal structure
- `template` for prompt or human-facing output assembly

---

## Performance Practices

### 1. Avoid optimizing against the wrong layer

Examples:

- do not replace `xvalue` with ad-hoc string maps just because JSON exists
- do not replace a structured future/task chain with callback spaghetti for imagined micro-speed

### 2. Reuse compiled/renderable artifacts

- parse templates once, render many times
- reuse compiled task/future composition patterns
- prefer stable data pipelines over repeated ad-hoc rebuilding

### 3. Pay attention to the real async hotspots

Important hotspots include:

- continuation fan-out
- `WhenAny / Race` concurrent completion
- task-group join/reap/cancel behavior
- stream wait-source registration and teardown

---

## Cross-Platform Practices

### 1. Keep platform branching inside the runtime when possible

Prefer XRT abstractions over scattering platform-specific code through application layers.

### 2. Treat compiler and backend differences explicitly

This matters especially in:

- thread local runtime behavior
- coroutine backend selection
- networking backends
- single-header distribution

### 3. Keep source-tree mainline and single-header distribution distinct

For development and verification:

- prefer the source tree mainline

Use single-header mode as a distribution form, not as the conceptual center of all examples and documentation.

---

## Summary

The current XRT mainline is strongest when you keep these rules together:

- explicit runtime context
- explicit ownership
- `xvalue` as the structured data center
- explicit shared roots for cross-thread work
- unified async model through future/task/promise/wait-source
- `xnet-v2 + xtlssession` as the networking mainline
- JSON and template as boundary tools, not competing internal models
