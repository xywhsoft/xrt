# XRT Guide: Runtime and Thread Attach

> A practical introduction to `xrtInit()`, thread-local runtime state, and when `xrtThreadAttachCurrent()` is required.

[中文](runtime-thread-attach.md) | [Back to Guides](README.en.md)

---

## What This Page Explains

The current XRT mainline no longer treats all runtime state as process-global.

The most important shift is:

- process-level state
- thread-level state

This page explains what that means in practice and how to keep your code aligned with the current runtime model.

---

## The Basic Rule

If a capability depends on current-thread runtime state, that thread must already be attached to XRT.

Common examples:

- `xrtGetError()`
- `xrtTempMemory()`
- `xrtRand32()` / `xrtRand64()`
- coroutine / future / wait-source related paths

---

## Which Threads Are Attached Automatically

### Automatically attached

- the bootstrap thread after `xrtInit()`
- threads created by `xrtThreadCreate()`

### Not automatically attached

- threads created by the host application itself
- third-party callback threads
- external thread-pool workers

Those threads must use:

```c
xrtThreadAttachCurrent();
/* ... runtime-bound APIs ... */
xrtThreadDetachCurrent();
```

---

## Minimal Main-Thread Pattern

```c
#include "xrt.h"

int main(void)
{
	xrtInit();

	/* use XRT APIs directly on the main thread */

	xrtUnit();
	return 0;
}
```

This is the default mainline entry for source-tree usage.

---

## Minimal Host-Thread Pattern

```c
static void procHostWorker(void)
{
	xrtThreadAttachCurrent();

	/* use runtime-bound APIs here */

	xrtThreadDetachCurrent();
}
```

This is the correct pattern when the thread is not created by `xrtThreadCreate()`.

---

## Related Reading

- [Base Runtime](../api/api-base.en.md)
- [Thread](../api/api-thread.en.md)
- [FAQ](../FAQ.en.md)
- [Migration Guide](../MIGRATION.en.md)
