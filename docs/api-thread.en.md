# Thread Management Library

> Multi-threading creation and management functions

[English](api-thread.en.md) | [中文](api-thread.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Data Types](#data-types)
- [Thread Operations](#thread-operations)
- [Platform-Specific Operations](#platform-specific-operations)
- [Use Cases](#use-cases)
- [Notes](#notes)

---

## Data Types

### xthread_struct

Thread data structure.

**Definition:**
```c
typedef struct {
    ptr Handle;                    // Thread handle (Windows: HANDLE, Linux: pthread_t)
    uint32 TID;                    // Thread ID
    uint32 (*Proc)(ptr param);     // Thread function pointer
    ptr Param;                     // Thread parameter
} xthread_struct, *xthread;
```

**Member Description:**
- `Handle` - Platform-specific thread handle
- `TID` - Thread ID
- `Proc` - Thread execution function
- `Param` - Parameter passed to the thread

---

## Thread Operations

### xrtThreadCreate

Create and start a new thread.

**Function Prototype:**
```c
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
```

**Parameters:**
- `pProc` - Thread function pointer
- `pParam` - Parameter to pass to the thread
- `iStackSize` - Stack size (bytes, 0 uses system default)

**Return Value:**
- Success: Thread object pointer `xthread`
- Failure: `NULL`

**Thread Function Prototype:**
```c
// Windows
DWORD WINAPI ThreadFunc(LPVOID param);

// Or generic form
uint32 ThreadFunc(ptr param);
```

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    DWORD WINAPI WorkerThread(LPVOID param) {
        int* value = (int*)param;
        printf("Thread received parameter: %d\n", *value);
        xrtSleep(1000);  // Simulate work
        printf("Thread completed\n");
        return 0;
    }
#else
    #include <pthread.h>
    void* WorkerThread(void* param) {
        int* value = (int*)param;
        printf("Thread received parameter: %d\n", *value);
        xrtSleep(1000);  // Simulate work
        printf("Thread completed\n");
        return NULL;
    }
#endif

int main() {
    xrtInit();
    
    int data = 42;
    xthread t = xrtThreadCreate(WorkerThread, &data, 0);
    if (t) {
        printf("Thread created successfully, TID: %u\n", t->TID);
        
        // Wait for thread completion (platform-specific)
        #if defined(_WIN32) || defined(_WIN64)
            WaitForSingleObject(t->Handle, INFINITE);
            CloseHandle(t->Handle);
        #else
            pthread_join((pthread_t)t->Handle, NULL);
        #endif
        
        xrtFree(t);
    } else {
        printf("Thread creation failed\n");
    }
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- Windows uses `CreateThread` implementation
- Linux uses `pthread_create` implementation (requires linking `-lpthread`)
- Returned `xthread` must be manually released (`xrtFree`)
- Thread handle requires platform API for waiting and closing

---

## Platform-Specific Operations

### Thread Waiting

Since thread waiting requires platform-specific APIs, here's a cross-platform approach:

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <pthread.h>
#endif

void WaitThread(xthread t) {
    if (!t) return;
    
    #if defined(_WIN32) || defined(_WIN64)
        WaitForSingleObject(t->Handle, INFINITE);
        CloseHandle(t->Handle);
    #else
        pthread_join((pthread_t)t->Handle, NULL);
    #endif
    
    xrtFree(t);
}
```

### Thread Sleep

Use `xrtSleep` function from the Time library:

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("Starting sleep...\n");
    xrtSleep(2000);  // Sleep for 2000 milliseconds (2 seconds)
    printf("Sleep ended\n");
    
    xrtUnit();
    return 0;
}
```

**Note:** `xrtSleep` is defined in [Time Processing Library](api-time.en.md).

---

## Use Cases

### 1. Parallel Task Processing

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    DWORD WINAPI TaskWorker(LPVOID param) {
        int id = *(int*)param;
        printf("Task %d started\n", id);
        xrtSleep(500 + xrtRandRange(0, 1000));  // Simulate varying task duration
        printf("Task %d completed\n", id);
        return 0;
    }
#else
    #include <pthread.h>
    void* TaskWorker(void* param) {
        int id = *(int*)param;
        printf("Task %d started\n", id);
        xrtSleep(500 + xrtRandRange(0, 1000));
        printf("Task %d completed\n", id);
        return NULL;
    }
#endif

int main() {
    xrtInit();
    
    #define THREAD_COUNT 4
    xthread threads[THREAD_COUNT];
    int ids[THREAD_COUNT];
    
    // Create multiple threads
    for (int i = 0; i < THREAD_COUNT; i++) {
        ids[i] = i + 1;
        threads[i] = xrtThreadCreate(TaskWorker, &ids[i], 0);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (threads[i]) {
            #if defined(_WIN32) || defined(_WIN64)
                WaitForSingleObject(threads[i]->Handle, INFINITE);
                CloseHandle(threads[i]->Handle);
            #else
                pthread_join((pthread_t)threads[i]->Handle, NULL);
            #endif
            xrtFree(threads[i]);
        }
    }
    
    printf("All tasks completed\n");
    
    xrtUnit();
    return 0;
}
```

---

### 2. Background Task

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <pthread.h>
#endif

typedef struct {
    str filename;
    int running;
} BackgroundTask;

#if defined(_WIN32) || defined(_WIN64)
DWORD WINAPI BackgroundWorker(LPVOID param) {
#else
void* BackgroundWorker(void* param) {
#endif
    BackgroundTask* task = (BackgroundTask*)param;
    
    while (task->running) {
        printf("Background task running: %s\n", task->filename);
        xrtSleep(1000);
    }
    
    printf("Background task stopped\n");
    #if defined(_WIN32) || defined(_WIN64)
        return 0;
    #else
        return NULL;
    #endif
}

int main() {
    xrtInit();
    
    BackgroundTask task = { (str)"data.log", TRUE };
    xthread t = xrtThreadCreate(BackgroundWorker, &task, 0);
    
    if (t) {
        printf("Background task started\n");
        
        // Main thread works for 3 seconds
        xrtSleep(3000);
        
        // Stop background task
        task.running = FALSE;
        
        // Wait for thread to end
        #if defined(_WIN32) || defined(_WIN64)
            WaitForSingleObject(t->Handle, INFINITE);
            CloseHandle(t->Handle);
        #else
            pthread_join((pthread_t)t->Handle, NULL);
        #endif
        xrtFree(t);
    }
    
    xrtUnit();
    return 0;
}
```

---

## Notes

### 1. Thread Safety

Most XRT library functions are **not thread-safe**, especially:

- `xCore` global variable (`iRet`, `LastError`, etc.)
- Temporary memory (`xrtTempMemory`)
- JSON parser

```c
// ❌ Unsafe: Multiple threads sharing xCore.iRet
void ThreadFunc1() {
    str s = xrtCopyStr((str)"hello", 0);
    size_t len = xCore.iRet;  // May be overwritten by other threads
}

// ✅ Safe: Save return value immediately
void ThreadFunc2() {
    str s = xrtCopyStr((str)"hello", 0);
    size_t len = xCore.iRet;  // Save immediately
    // Subsequent operations...
}
```

### 2. Memory Management

Memory allocated in a thread should be released in the same thread, or use appropriate synchronization mechanisms.

### 3. Resource Release

```c
// ✅ Correct resource release order
xthread t = xrtThreadCreate(func, param, 0);
if (t) {
    // 1. Wait for thread completion
    #if defined(_WIN32) || defined(_WIN64)
        WaitForSingleObject(t->Handle, INFINITE);
        // 2. Close handle
        CloseHandle(t->Handle);
    #else
        pthread_join((pthread_t)t->Handle, NULL);
    #endif
    // 3. Release struct
    xrtFree(t);
}
```

### 4. Compilation and Linking

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
