# Thread Management Library

> Thread runtime, runtime attachment, cleanup stack, and synchronization primitives

[English](api-thread.en.md) | [中文](api-thread.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constants](#constants)
- [Data Types](#data-types)
- [Thread Management](#thread-management)
- [Runtime Attachment](#runtime-attachment)
- [Cleanup and Thread-Exit Hooks](#cleanup-and-thread-exit-hooks)
- [Mutex](#mutex)
- [Semaphore](#semaphore)
- [Condition Variable](#condition-variable)
- [Use Cases](#use-cases)
- [Notes](#notes)

---

## Constants

### Thread States

```c
#define XRT_THREAD_STOPPED      0    // Stopped
#define XRT_THREAD_RUNNING      1    // Running
#define XRT_THREAD_SUSPENDED    2    // Suspended
```

### Wait Return Values

```c
#define XRT_WAIT_OK            0    // Wait successful
#define XRT_WAIT_TIMEOUT       1    // Wait timeout
#define XRT_WAIT_ERROR        -1    // Wait error
```

---

## Data Types

### xthread_struct

Thread management object.

```c
typedef struct xthread_struct* xthread;
```

Important notes:

- treat `xthread` as an opaque runtime-managed handle
- do not depend on internal field layout
- current mainline keeps thread lifecycle, stop state, join state, exit code, and runtime attachment state behind the API
- this keeps the public contract stable while allowing internal lifecycle hardening to continue

### xmutex_struct

Mutex data structure.

```c
typedef struct {
    ptr Handle;                    // Mutex handle
} xmutex_struct, *xmutex;
```

### xsem_struct

Semaphore data structure.

```c
typedef struct {
    ptr Handle;                    // Semaphore handle
} xsem_struct, *xsem;
```

### xcond_struct

Condition variable data structure.

```c
typedef struct {
    ptr Handle;                    // Condition variable handle
} xcond_struct, *xcond;
```

---

## Thread Management

### xrtThreadCreate

Create and start a new thread.

```c
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
```

**Parameters:**
- `pProc` - Thread function pointer
- `pParam` - Parameter to pass to thread
- `iStackSize` - Stack size (bytes, 0 uses system default)

**Returns:** Thread object pointer on success, `NULL` on failure

**Notes:**
- Threads created by `xrtThreadCreate()` are automatically attached to the XRT runtime
- Runtime-bound APIs such as `xrtTempMemory()`, `xrtSetError()`, and `xrtRand32()` can be called directly inside the worker procedure

---

### xrtThreadDestroy

Destroy the thread object (does not terminate the thread, only releases the management structure).

```c
XXAPI void xrtThreadDestroy(xthread pThread);
```

**Parameters:**
- `pThread` - Thread object

**Notes:**
- Only destroys the thread management object; it never force-terminates the thread
- If the thread is still running, `xrtThreadDestroy()` sets the current-thread error message and returns immediately
- Recommended order: `xrtThreadWait()` -> `xrtThreadDestroy()`

---

### xrtThreadWait

Wait for thread to finish (blocking).

```c
XXAPI void xrtThreadWait(xthread pThread);
```

---

### xrtThreadWaitTimeout

Wait for thread to finish (with timeout).

```c
XXAPI int xrtThreadWaitTimeout(xthread pThread, uint32 iTimeout);
```

**Parameters:**
- `pThread` - Thread object
- `iTimeout` - Timeout in milliseconds

**Returns:** `XRT_WAIT_OK`/`XRT_WAIT_TIMEOUT`/`XRT_WAIT_ERROR`

**Notes:**
- Windows uses native wait objects
- The current Linux/macOS implementation keeps the thread joinable for a later `xrtThreadWait()` call instead of consuming the join state during timeout polling

---

### xrtThreadStop

Send stop signal. Thread must check `xrtThreadShouldStop`.

```c
XXAPI void xrtThreadStop(xthread pThread);
```

---

### xrtThreadShouldStop

Check if thread should stop (called from within thread).

```c
XXAPI bool xrtThreadShouldStop(xthread pThread);
```

**Returns:** `TRUE` if should stop

---

### xrtThreadKill

Forcefully terminate thread.

```c
XXAPI bool xrtThreadKill(xthread pThread);
```

**Warning:** Dangerous operation, may cause resource leaks. Not recommended.

---

### xrtThreadSuspend

Suspend thread.

```c
XXAPI bool xrtThreadSuspend(xthread pThread);
```

**Note:** Only supported on Windows, returns `FALSE` on Linux/macOS

---

### xrtThreadResume

Resume thread.

```c
XXAPI bool xrtThreadResume(xthread pThread);
```

**Note:** Only supported on Windows, returns `FALSE` on Linux/macOS

---

### xrtThreadGetState

Get thread state.

```c
XXAPI int xrtThreadGetState(xthread pThread);
```

**Returns:** `XRT_THREAD_STOPPED`/`XRT_THREAD_RUNNING`/`XRT_THREAD_SUSPENDED`

**Notes:**
- This is a best-effort snapshot
- Observable granularity between running and stopped states is not identical across platforms

---

### xrtThreadGetExitCode

Get thread exit code.

```c
XXAPI uint32 xrtThreadGetExitCode(xthread pThread);
```

---

### xrtThreadGetCurrentId

Get current thread ID.

```c
XXAPI uint64 xrtThreadGetCurrentId();
```

---

## Runtime Attachment

### xrtThreadIsAttached

Check whether the current thread is attached to the XRT runtime.

```c
XXAPI bool xrtThreadIsAttached();
```

### xrtThreadGetCurrent

Get the runtime state object of the current thread.

```c
XXAPI xrtThreadData* xrtThreadGetCurrent();
```

### xrtThreadAttachCurrent

Attach the current thread to the XRT runtime.

```c
XXAPI xrtThreadData* xrtThreadAttachCurrent();
```

### xrtThreadDetachCurrent

Detach the current thread from the XRT runtime.

```c
XXAPI void xrtThreadDetachCurrent();
```

### xrtThreadPushCleanup / xrtThreadPopCleanup

Register or pop a thread-exit cleanup callback for the current thread.

```c
XXAPI bool xrtThreadPushCleanup(xrtThreadCleanupProc proc, ptr pArg);
XXAPI bool xrtThreadPopCleanup(xrtThreadCleanupProc proc, ptr pArg);
```

**Notes:**
- The bootstrap thread is attached automatically after `xrtInit()`
- Threads created by `xrtThreadCreate()` are attached automatically and clean up attached runtime resources on exit
- Host-created threads that need runtime-bound APIs should call `xrtThreadAttachCurrent()` before use and `xrtThreadDetachCurrent()` before exit

---

## Cleanup and Thread-Exit Hooks

The thread runtime now supports explicit thread-exit cleanup callbacks.

```c
XXAPI bool xrtThreadPushCleanup(xrtThreadCleanupProc proc, ptr pArg);
XXAPI bool xrtThreadPopCleanup(xrtThreadCleanupProc proc, ptr pArg);
```

Recommended uses:

- releasing thread-bound caches
- draining thread-owned runtime helpers
- cleaning up resources tied to a host-attached thread

Important rules:

- cleanup callbacks run on the current thread during runtime detach or managed thread exit
- use them for thread-owned resource release, not as a replacement for normal structured shutdown
- do not assume callback order except the documented stack semantics

---

### xrtThreadYield

Yield current thread's time slice.

```c
XXAPI void xrtThreadYield();
```

---

## Mutex

### xrtMutexCreate

Create mutex.

```c
XXAPI xmutex xrtMutexCreate();
```

**Returns:** Mutex object on success, `NULL` on failure

---

### xrtMutexDestroy

Destroy mutex.

```c
XXAPI void xrtMutexDestroy(xmutex pMutex);
```

---

### xrtMutexInit / xrtMutexUnit

Initialize/release mutex (for self-maintained struct pointers).

```c
XXAPI void xrtMutexInit(xmutex pMutex);
XXAPI void xrtMutexUnit(xmutex pMutex);
```

---

### xrtMutexLock

Lock mutex (blocking).

```c
XXAPI void xrtMutexLock(xmutex pMutex);
```

---

### xrtMutexTryLock

Try to lock mutex (non-blocking).

```c
XXAPI bool xrtMutexTryLock(xmutex pMutex);
```

**Returns:** `TRUE` if lock successful

---

### xrtMutexUnlock

Unlock mutex.

```c
XXAPI void xrtMutexUnlock(xmutex pMutex);
```

---

## Semaphore

### xrtSemCreate

Create semaphore.

```c
XXAPI xsem xrtSemCreate(uint32 iInitValue, uint32 iMaxValue);
```

**Parameters:**
- `iInitValue` - Initial value
- `iMaxValue` - Maximum value

**Returns:** Semaphore object on success, `NULL` on failure

---

### xrtSemDestroy

Destroy semaphore.

```c
XXAPI void xrtSemDestroy(xsem pSem);
```

---

### xrtSemInit / xrtSemUnit

Initialize/release semaphore (for self-maintained struct pointers).

```c
XXAPI void xrtSemInit(xsem pSem, uint32 iInitValue, uint32 iMaxValue);
XXAPI void xrtSemUnit(xsem pSem);
```

---

### xrtSemWait

Wait for semaphore (blocking, count decreases by 1).

```c
XXAPI void xrtSemWait(xsem pSem);
```

---

### xrtSemTryWait

Try to wait for semaphore (non-blocking).

```c
XXAPI bool xrtSemTryWait(xsem pSem);
```

---

### xrtSemWaitTimeout

Wait for semaphore (with timeout).

```c
XXAPI int xrtSemWaitTimeout(xsem pSem, uint32 iTimeout);
```

**Returns:** `XRT_WAIT_OK`/`XRT_WAIT_TIMEOUT`/`XRT_WAIT_ERROR`

---

### xrtSemPost

Release semaphore (count increases by 1).

```c
XXAPI bool xrtSemPost(xsem pSem);
```

---

### xrtSemPostMultiple

Release semaphore (count increases by N).

```c
XXAPI bool xrtSemPostMultiple(xsem pSem, uint32 iCount);
```

---

## Condition Variable

### xrtCondCreate

Create condition variable.

```c
XXAPI xcond xrtCondCreate();
```

---

### xrtCondDestroy

Destroy condition variable.

```c
XXAPI void xrtCondDestroy(xcond pCond);
```

---

### xrtCondInit / xrtCondUnit

Initialize/release condition variable.

```c
XXAPI void xrtCondInit(xcond pCond);
XXAPI void xrtCondUnit(xcond pCond);
```

---

### xrtCondWait

Wait for condition variable (must lock mutex first).

```c
XXAPI void xrtCondWait(xcond pCond, xmutex pMutex);
```

---

### xrtCondWaitTimeout

Wait for condition variable (with timeout).

```c
XXAPI int xrtCondWaitTimeout(xcond pCond, xmutex pMutex, uint32 iTimeout);
```

---

### xrtCondSignal

Wake one waiting thread.

```c
XXAPI void xrtCondSignal(xcond pCond);
```

---

### xrtCondBroadcast

Wake all waiting threads.

```c
XXAPI void xrtCondBroadcast(xcond pCond);
```

---

## Use Cases

### 1. Basic Thread Usage

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    xthread Thread;
} ThreadCtx;

static uint32 WorkerThread(ptr param)
{
    ThreadCtx* pCtx = (ThreadCtx*)param;

    while (pCtx->Thread == NULL) {
        xrtSleep(1);
    }

    for (int i = 0; i < 10; i++) {
        if (xrtThreadShouldStop(pCtx->Thread)) {
            printf("Thread received stop signal\n");
            return 1;
        }
        printf("Working: %d\n", i);
        xrtSleep(100);
    }
    return 0;
}

int main() {
    ThreadCtx tCtx = { 0 };
    xrtInit();

    tCtx.Thread = xrtThreadCreate(WorkerThread, &tCtx, 0);

    xrtThreadWait(tCtx.Thread);
    xrtThreadDestroy(tCtx.Thread);
    
    xrtUnit();
    return 0;
}
```

### 2. Mutex Protecting Shared Data

```c
#include "xrt.h"
#include <stdio.h>

static int g_Counter = 0;
static xmutex g_Mutex = NULL;

static uint32 CounterThread(ptr param)
{
    for (int i = 0; i < 1000; i++) {
        xrtMutexLock(g_Mutex);
        g_Counter++;
        xrtMutexUnlock(g_Mutex);
    }
    return 0;
}

int main() {
    xrtInit();
    
    g_Mutex = xrtMutexCreate();
    
    xthread threads[4];
    for (int i = 0; i < 4; i++) {
        threads[i] = xrtThreadCreate(CounterThread, NULL, 0);
    }
    
    for (int i = 0; i < 4; i++) {
        xrtThreadWait(threads[i]);
        xrtThreadDestroy(threads[i]);
    }
    
    printf("Final count: %d (expected: 4000)\n", g_Counter);
    
    xrtMutexDestroy(g_Mutex);
    xrtUnit();
    return 0;
}
```

### 3. Producer-Consumer Pattern

```c
#include "xrt.h"
#include <stdio.h>

#define BUFFER_SIZE 5

static int g_Buffer[BUFFER_SIZE];
static int g_Index = 0;
static xsem g_SemEmpty = NULL;
static xsem g_SemFull = NULL;
static xmutex g_Mutex = NULL;

static uint32 ProducerThread(ptr param)
{
    for (int i = 1; i <= 10; i++) {
        xrtSemWait(g_SemEmpty);
        
        xrtMutexLock(g_Mutex);
        g_Buffer[g_Index++] = i;
        printf("Produced: %d\n", i);
        xrtMutexUnlock(g_Mutex);
        
        xrtSemPost(g_SemFull);
    }
    return 0;
}

static uint32 ConsumerThread(ptr param)
{
    for (int i = 0; i < 10; i++) {
        xrtSemWait(g_SemFull);
        
        xrtMutexLock(g_Mutex);
        int value = g_Buffer[--g_Index];
        printf("Consumed: %d\n", value);
        xrtMutexUnlock(g_Mutex);
        
        xrtSemPost(g_SemEmpty);
    }
    return 0;
}

int main() {
    xrtInit();
    
    g_SemEmpty = xrtSemCreate(BUFFER_SIZE, BUFFER_SIZE);
    g_SemFull = xrtSemCreate(0, BUFFER_SIZE);
    g_Mutex = xrtMutexCreate();
    
    xthread producer = xrtThreadCreate(ProducerThread, NULL, 0);
    xthread consumer = xrtThreadCreate(ConsumerThread, NULL, 0);
    
    xrtThreadWait(producer);
    xrtThreadWait(consumer);
    
    xrtThreadDestroy(producer);
    xrtThreadDestroy(consumer);
    xrtSemDestroy(g_SemEmpty);
    xrtSemDestroy(g_SemFull);
    xrtMutexDestroy(g_Mutex);
    
    xrtUnit();
    return 0;
}
```

### 4. Condition Variable Wait/Notify

```c
#include "xrt.h"
#include <stdio.h>

static xcond g_Cond = NULL;
static xmutex g_Mutex = NULL;
static volatile int g_Ready = 0;

static uint32 WaiterThread(ptr param)
{
    int id = (int)(intptr)param;
    
    xrtMutexLock(g_Mutex);
    while (!g_Ready) {
        printf("Thread %d waiting...\n", id);
        xrtCondWait(g_Cond, g_Mutex);
    }
    printf("Thread %d awakened\n", id);
    xrtMutexUnlock(g_Mutex);
    
    return 0;
}

int main() {
    xrtInit();
    
    g_Cond = xrtCondCreate();
    g_Mutex = xrtMutexCreate();
    
    xthread waiters[3];
    for (int i = 0; i < 3; i++) {
        waiters[i] = xrtThreadCreate(WaiterThread, (ptr)(intptr)(i + 1), 0);
    }
    
    xrtSleep(500);
    
    xrtMutexLock(g_Mutex);
    g_Ready = 1;
    xrtCondBroadcast(g_Cond);
    xrtMutexUnlock(g_Mutex);
    
    for (int i = 0; i < 3; i++) {
        xrtThreadWait(waiters[i]);
        xrtThreadDestroy(waiters[i]);
    }
    
    xrtCondDestroy(g_Cond);
    xrtMutexDestroy(g_Mutex);
    xrtUnit();
    return 0;
}
```

---

## Notes

### 1. Thread Safety

After phase-1, the XRT runtime model is:

- `xCore` only stores process-global state such as `AppPath`, `AppFile`, `sNull`, and `OnError`
- `LastError`, `xrtTempMemory()`, and the default RNG belong to the current thread
- Threads created by `xrtThreadCreate()` can call runtime-bound APIs directly
- Host threads should not call runtime-bound APIs before they are attached
- thread-exit cleanup should be registered through `xrtThreadPushCleanup()` instead of being hidden in unrelated code paths

Use mutex to protect shared data:

```c
xmutex mutex = xrtMutexCreate();

// Thread-safe access
xrtMutexLock(mutex);
// Access shared data...
xrtMutexUnlock(mutex);

xrtMutexDestroy(mutex);
```

### 2. Correct Way to Stop Threads

Recommend using stop signal instead of forceful termination:

```c
// ✅ Recommended: Use stop signal
static uint32 SafeThread(ptr param) {
    xthread self = (xthread)param;
    while (!xrtThreadShouldStop(self)) {
        // Work...
    }
    return 0;
}

// Send stop signal
xrtThreadStop(thread);
xrtThreadWait(thread);

// ❌ Not recommended: Force terminate
xrtThreadKill(thread);  // May cause resource leaks
```

### 3. Resource Release Order

```c
xthread t = xrtThreadCreate(func, param, 0);
if (t) {
    // 1. Wait for thread to complete
    xrtThreadWait(t);
    // 2. Destroy thread object
    xrtThreadDestroy(t);
}
```

For host-attached threads, the equivalent rule is:

```c
xrtThreadAttachCurrent();
/* ... runtime-bound work ... */
xrtThreadDetachCurrent();
```

### 4. Platform Differences

| Feature | Windows | Linux/macOS |
|---------|---------|-------------|
| Thread suspend/resume | Supported | Not supported |
| Thread wait with timeout | Supported | Partially supported |
| Condition variable | Fully supported | Fully supported |

### 5. Compilation and Linking

On Linux/macOS, link the pthread library:

```bash
gcc -o program program.c -lpthread
```

---

## Related Documentation

- [Time Processing Library](api-time.en.md) - Contains `xrtSleep` function
- [Base Functions Library](api-base.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#thread-management-library)

</div>
