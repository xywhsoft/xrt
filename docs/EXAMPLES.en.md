# XRT Examples Guide

> How to read, build, and understand examples on the current XRT mainline

[中文](EXAMPLES.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [What Examples Mean in the Current Mainline](#what-examples-mean-in-the-current-mainline)
- [Recommended Build Style](#recommended-build-style)
- [Recommended Example Topics](#recommended-example-topics)
- [Suggested Reading Order](#suggested-reading-order)
- [Minimal Runtime Example](#minimal-runtime-example)
- [Structured Data Example](#structured-data-example)
- [Thread and Runtime Example](#thread-and-runtime-example)
- [HTTP Client Reading Path](#http-client-reading-path)
- [Notes](#notes)

---

## What Examples Mean in the Current Mainline

XRT is no longer just a tiny single-header utility. The project now has:

- source-tree mainline development
- single-header distribution
- runtime and concurrency subsystems
- networking mainline
- application-layer networking
- async mainline

Examples should show:

- how the current mainline should be built
- which API documents map to which capability
- how runtime, structured data, async, and networking fit together

If you want detailed API contracts, start with:

- [API Index](api/README.en.md)
- [Base Library](api/api-base.en.md)
- [Thread Library](api/api-thread.en.md)
- [Coroutine Runtime](api/api-coroutine.en.md)
- [Future / Task / Promise](api/api-future-task-promise.en.md)
- [XNet v2 Mainline](api/api-xnet-v2.en.md)

---

## Recommended Build Style

### 1. Source-tree mainline

For current development, testing, and integration, prefer the source tree:

```c
#include "xrt.h"
```

Then compile:

```bash
gcc main.c xrt.c -o app
```

or:

```bash
tcc main.c xrt.c -o app.exe
```

### 2. Single-header distribution

Single-header distribution is still supported, but it is a distribution form, not the conceptual center of the documentation.

---

## Recommended Example Topics

### 1. Runtime basics

- `xrtInit()` / `xrtUnit()`
- `xrtGetError()`
- `xrtTempMemory()`
- time, file, string, and formatting helpers

### 2. Structured data

- `xvalue`
- `json`
- table / array / list / coll

### 3. Concurrency and async

- thread
- coroutine
- future / task / promise
- wait-source

### 4. Networking mainline

- `xnet-v2`
- stream / dgram / listener
- TLS session
- HTTP / HTTPD / WebSocket

---

## Suggested Reading Order

1. `xrtInit()` / `xrtUnit()` / `xrtGetError()`
2. string, file, and time helpers
3. `xvalue + json`
4. thread runtime and attach rules
5. coroutine runtime
6. `future / task / promise / wait-source`
7. `xnet-v2`
8. `xtlssession`
9. `xhttp / xhttpd / xws`

Continue with:

- [Guide Index](guide/README.en.md)
- [Case Index](case/README.en.md)

---

## Minimal Runtime Example

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	str sGreeting;
	xvalue pNum;

	xrtInit();

	sGreeting = xrtCopyStr("Hello, XRT!", 0);
	pNum = xvoCreateInt(42);

	printf("Greeting: %s\n", (char*)sGreeting);
	printf("Number: %lld\n", (long long)xvoGetInt(pNum));

	xvoUnref(pNum);
	xrtFree(sGreeting);

	xrtUnit();
	return 0;
}
```

---

## Structured Data Example

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xvalue pUser;
	xvalue pSkills;

	xrtInit();

	pUser = xvoCreateTable();
	pSkills = xvoCreateArray();

	xvoArrayAppendText(pSkills, "C", 0, FALSE);
	xvoArrayAppendText(pSkills, "Python", 0, FALSE);

	xvoTableSetText(pUser, "name", 0, "Alice", 0, FALSE);
	xvoTableSetInt(pUser, "age", 0, 25);
	xvoTableSetValue(pUser, "skills", 0, pSkills, TRUE);

	printf("name = %s\n", (char*)xvoTableGetText(pUser, "name", 0));
	printf("age = %lld\n", (long long)xvoTableGetInt(pUser, "age", 0));
	printf("skill[0] = %s\n", (char*)xvoArrayGetText(pSkills, 0));

	xvoUnref(pUser);
	xrtUnit();
	return 0;
}
```

---

## Thread and Runtime Example

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	xthread pThread;
} ThreadCtx;

static uint32 Worker(ptr pArg)
{
	ThreadCtx* pCtx = (ThreadCtx*)pArg;

	while ( pCtx->pThread == NULL ) {
		xrtSleep(1);
	}

	printf("thread attached = %d\n", xrtThreadIsAttached());
	printf("rand32 = %u\n", xrtRand32());
	return 0;
}
```

This reflects the current runtime contract:

- the main thread is attached by `xrtInit()`
- threads created by `xrtThreadCreate()` are attached automatically

---

## HTTP Client Reading Path

For HTTP client development, prefer:

- [XHTTP](api/api-xhttp.en.md)
- [XNet v2](api/api-xnet-v2.en.md)
- [TLS Session Mainline](api/api-network-tls.en.md)

For HTTP server development:

- [XHTTPD](api/api-xhttpd.en.md)
- [XWS](api/api-xws.en.md)

---

## Notes

### 1. Do not treat retired example styles as the current mainline

Examples should not be centered on retired public surfaces or runtime internals.

### 2. Runtime-sensitive examples should always show initialization explicitly

If an example uses:

- `xrtGetError()`
- `xrtTempMemory()`
- coroutine
- future / task / wait-source

then it should explicitly show:

```c
xrtInit();
/* ... */
xrtUnit();
```

### 3. Cross-thread shared examples should use explicit shared roots

For cross-thread container sharing:

```c
xvalue pTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
```

---

## Summary

The goal of this examples guide is to show:

- how the current mainline is built
- which example topics map to which subsystems
- which old example styles should no longer guide new code
