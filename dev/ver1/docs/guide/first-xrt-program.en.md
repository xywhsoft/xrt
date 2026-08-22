# Your First XRT Program

> A first-contact guide for developers new to XRT. The goal is not to cover every API, but to understand the current mainline with the shortest possible path.

[Back to Guides](README.en.md) | [Back to API Index](../api/README.en.md)

---

## Contents

- [Goal](#goal)
- [Why Start from the Source Tree](#why-start-from-the-source-tree)
- [Prepare a Minimal Program](#prepare-a-minimal-program)
- [Build and Run](#build-and-run)
- [What This Program Shows About the Mainline](#what-this-program-shows-about-the-mainline)
- [What to Read Next](#what-to-read-next)

---

## Goal

This page only solves four things:

1. write your first working XRT program
2. understand why the current mainline starts from the source tree instead of explaining everything as single-header first
3. understand that `xrtInit()` / `xrtUnit()` define the runtime boundary
4. understand the minimum usage pattern for strings, dynamic values, and error handling

---

## Why Start from the Source Tree

XRT still supports single-header distribution, but current development, testing, and documentation are already centered on the source-tree mainline.

So for first contact, the recommended starting point is:

```c
#include "xrt.h"
```

then build directly:

```bash
gcc main.c xrt.c -o app
```

or:

```bash
tcc main.c xrt.c -o app.exe
```

This way you touch the current formal mainline directly instead of an older example style or a distribution-oriented surface.

---

## Prepare a Minimal Program

Create a `main.c`:

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	str sGreeting;
	xvalue pCount;

	xrtInit();

	sGreeting = xrtCopyStr("Hello, XRT!", 0);
	pCount = xvoCreateInt(42);

	printf("Greeting: %s\n", (char*)sGreeting);
	printf("Count: %lld\n", (long long)xvoGetInt(pCount));

	xvoUnref(pCount);
	xrtFree(sGreeting);

	xrtUnit();
	return 0;
}
```

---

## Build and Run

### Windows / MinGW GCC

```bash
gcc main.c xrt.c -o app.exe
app.exe
```

### Windows / TCC

```bash
tcc main.c xrt.c -o app.exe
app.exe
```

### Linux / GCC

```bash
gcc main.c xrt.c -o app
./app
```

If you see output like this, the current mainline is running correctly:

```text
Greeting: Hello, XRT!
Count: 42
```

---

## What This Program Shows About the Mainline

### 1. `xrtInit()` / `xrtUnit()` define the runtime boundary

The current XRT mainline has an explicit runtime concept:

- `xrtInit()`: initialize the runtime and attach the main thread to XRT
- `xrtUnit()`: release runtime-related resources

If the program uses string allocation, dynamic values, thread-bound error state, temporary memory, coroutines, futures, or the network mainline, keep this boundary explicit.

### 2. `str` is XRT's own string mainline

`str` in XRT is not just a casual alias for `char*`. It is part of a deliberate string/byte mainline.  
When calling standard-library APIs, cast to `(char*)` when necessary.

### 3. `xvalue` is the high-level dynamic data mainline

This line:

```c
xvalue pCount = xvoCreateInt(42);
```

already shows the direction of XRT's unified data model. You will keep using it later in:

- JSON
- templates
- configuration
- HTTP request/response payloads
- structured data in AI-facing scenarios

### 4. Memory release is layered

This small example already shows two release paths:

- `xrtFree()`: release ordinary allocated strings
- `xvoUnref()`: release reference-counted value objects

They are not interchangeable.

---

## What to Read Next

Suggested reading order:

1. [Base Runtime](../api/api-base.en.md)
2. [Value System](../api/api-value.en.md)
3. [JSON](../api/api-json.en.md)
4. [Thread](../api/api-thread.en.md)
5. [Coroutine](../api/api-coroutine.en.md)

If you are already preparing to write network programs, continue with:

1. [XNet V2](../api/api-xnet-v2.en.md)
2. [Network TLS](../api/api-network-tls.en.md)
3. [XHTTP](../api/api-xhttp.en.md)

---

**Status: first batch of formal English guide pages**
