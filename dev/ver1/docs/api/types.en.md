# XRT Basic Type Definitions

> All basic types, macro definitions and global data structures of the XRT library

[English](types.en.md) | [中文](types.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Standard Library Dependencies](#standard-library-dependencies)
- [Header Guard Macros](#header-guard-macros)
- [Basic Type Definitions](#basic-type-definitions)
- [String Types](#string-types)
- [Integer Types](#integer-types)
- [Floating Point Types](#floating-point-types)
- [Pointer Types](#pointer-types)
- [Special Types](#special-types)
- [Constant Definitions](#constant-definitions)
- [Runtime Data Model](#runtime-data-model)

---

## Standard Library Dependencies

The XRT library depends on the following C standard library headers:

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>
```

| Header | Purpose |
|--------|---------|
| `stdio.h` | Standard input/output |
| `stdint.h` | Fixed-width integer types (`intptr_t`, `uintptr_t`, etc.) |
| `stdarg.h` | Variable argument support (`va_list`, etc.) |
| `stdlib.h` | Memory allocation (`malloc`, `free`, etc.) |
| `unistd.h` | POSIX system calls |
| `string.h` | String operations (`memcpy`, `strlen`, etc.) |
| `ctype.h` | Character classification (`isalpha`, `isdigit`, etc.) |
| `wctype.h` | Wide character classification |
| `math.h` | Math functions |
| `time.h` | Time processing |
| `inttypes.h` | Integer format macros (`PRId64`, etc.) |
| `stdbool.h` | Boolean type (`bool`, `true`, `false`) |

**Notes:**
- The `_GNU_SOURCE` macro enables GNU extensions (such as `memmem` function on Linux)
- XRT has zero external dependencies, only depends on the C standard library

---

## Header Guard Macros

### XXRTL_CORE

Header file protection macro to prevent multiple inclusions:

```c
#ifndef XXRTL_CORE
    #define XXRTL_CORE
    // ... all type definitions and function declarations ...
#endif
```

---

## Basic Type Definitions

### String Types

| Type | Definition | Description |
|------|------------|-------------|
| `u8str` | `typedef unsigned char* u8str` | UTF-8 string pointer |
| `u16str` | `typedef unsigned short* u16str` | UTF-16 string pointer |
| `u32str` | `typedef unsigned int* u32str` | UTF-32 string pointer |
| `binary` | `typedef unsigned char* binary` | Binary data pointer |
| `str` | `typedef u8str str` | Default string type (UTF-8) |

**Usage Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // UTF-8 string
    str text = (str)"Hello World";
    printf("UTF-8: %s\n", text);
    
    // Convert to UTF-16
    u16str wtext = xrtUTF8to16(text, 0);
    printf("UTF-16 length: %zu\n", u16len(wtext));
    xrtFree(wtext);
    
    // Binary data
    binary data = (binary)"\x00\x01\x02\x03";
    printf("Binary first byte: 0x%02X\n", data[0]);
    
    xrtUnit();
    return 0;
}
```

---

### Integer Types

#### 8-bit Integers

| Type | Definition | Range | Description |
|------|------------|-------|-------------|
| `i8` | `typedef char i8` | -128 ~ 127 | Signed 8-bit integer |
| `int8` | `typedef char int8` | -128 ~ 127 | Signed 8-bit integer (alias) |
| `u8` | `typedef unsigned char u8` | 0 ~ 255 | Unsigned 8-bit integer |
| `uint8` | `typedef unsigned char uint8` | 0 ~ 255 | Unsigned 8-bit integer (alias) |

#### 16-bit Integers

| Type | Definition | Range | Description |
|------|------------|-------|-------------|
| `i16` | `typedef short i16` | -32,768 ~ 32,767 | Signed 16-bit integer |
| `int16` | `typedef short int16` | -32,768 ~ 32,767 | Signed 16-bit integer (alias) |
| `u16` | `typedef unsigned short u16` | 0 ~ 65,535 | Unsigned 16-bit integer |
| `uint16` | `typedef unsigned short uint16` | 0 ~ 65,535 | Unsigned 16-bit integer (alias) |

#### 32-bit Integers

| Type | Definition | Range | Description |
|------|------------|-------|-------------|
| `i32` | `typedef int i32` | -2^31 ~ 2^31-1 | Signed 32-bit integer |
| `int32` | `typedef int int32` | -2^31 ~ 2^31-1 | Signed 32-bit integer (alias) |
| `u32` | `typedef unsigned int u32` | 0 ~ 2^32-1 | Unsigned 32-bit integer |
| `uint32` | `typedef unsigned int uint32` | 0 ~ 2^32-1 | Unsigned 32-bit integer (alias) |
| `uint` | `typedef unsigned int uint` | 0 ~ 2^32-1 | Unsigned integer (alias) |

#### 64-bit Integers

| Type | Definition | Range | Description |
|------|------------|-------|-------------|
| `i64` | `typedef long long i64` | -2^63 ~ 2^63-1 | Signed 64-bit integer |
| `int64` | `typedef long long int64` | -2^63 ~ 2^63-1 | Signed 64-bit integer (alias) |
| `u64` | `typedef unsigned long long u64` | 0 ~ 2^64-1 | Unsigned 64-bit integer |
| `uint64` | `typedef unsigned long long uint64` | 0 ~ 2^64-1 | Unsigned 64-bit integer (alias) |

#### Platform-dependent Integers

| Type | Definition | Description |
|------|------------|-------------|
| `ulong` | `typedef unsigned long ulong` | Unsigned long integer that adapts to 32/64-bit |

---

### Floating Point Types

| Type | Definition | Precision | Description |
|------|------------|-----------|-------------|
| `f32` | `typedef float f32` | Single | 32-bit floating point |
| `float32` | `typedef float float32` | Single | 32-bit floating point (alias) |
| `f64` | `typedef double f64` | Double | 64-bit floating point |
| `float64` | `typedef double float64` | Double | 64-bit floating point (alias) |

**Usage Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Single precision floating point
    f32 pi = 3.14159f;
    float32 radius = 5.0f;
    printf("Circumference: %.4f\n", 2 * pi * radius);
    
    // Double precision floating point
    f64 e = 2.718281828459045;
    float64 x = 10.0;
    printf("e^10 ≈ %.6f\n", pow(e, x));
    
    xrtUnit();
    return 0;
}
```

---

### Pointer Types

| Type | Definition | Description |
|------|------------|-------------|
| `ptr` | `typedef void* ptr` | Generic pointer type |
| `intptr` | `typedef intptr_t intptr` | Pointer-sized signed integer |
| `uintptr` | `typedef uintptr_t uintptr` | Pointer-sized unsigned integer |

**Usage Scenarios:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generic pointer
    ptr data = xrtMalloc(1024);
    printf("Allocated memory address: %p\n", data);
    
    // Pointer to integer
    intptr addr = (intptr)data;
    printf("Pointer integer value: %" PRIdPTR "\n", addr);
    
    // Unsigned pointer integer
    uintptr uaddr = (uintptr)data;
    printf("Unsigned pointer value: %" PRIuPTR "\n", uaddr);
    
    xrtFree(data);
    xrtUnit();
    return 0;
}
```

---

### Special Types

| Type | Definition | Description |
|------|------------|-------------|
| `curr` | `typedef long long curr` | Currency type (64-bit integer) |
| `xtime` | `typedef int64 xtime` | Time type (64-bit integer, in seconds) |

---

## Constant Definitions

### Boolean Constants

```c
#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#ifndef null
    #define null 0
#endif
```

**Notes:**
- XRT uses the standard C `bool` type (requires `<stdbool.h>`)
- `TRUE/FALSE` are for compatibility
- `null` is used as a null pointer constant

### API Export Marker

```c
#ifdef BUILD_DLL
    #define XXAPI __declspec(dllexport)
#else
    #define XXAPI
#endif
```

**Notes:**
- Define `BUILD_DLL` macro when compiling a DLL
- `XXAPI` marks all exported functions

---

## Runtime Data Model

XRT no longer documents the runtime as a single public `xCore` layout that callers should depend on field-by-field.

The formal current model is:

- process-level runtime state
- thread-level runtime state

### Process-Level Runtime

The process-level runtime keeps stable process-wide information such as:

- `sNull`
- application path and file path
- allocator entry points
- global error callback
- high-resolution timer environment

This layer exists for:

- one-time initialization
- process-wide configuration
- platform environment information

### Thread-Level Runtime

The thread-level runtime keeps per-thread state such as:

- last error
- temporary memory
- default random state
- coroutine runtime state
- current-thread deferred continuation queue
- later thread-bound memory context

This is why current code should use runtime APIs instead of depending on old global layout assumptions.

### Recommended Access Pattern

Use runtime APIs such as:

- `xrtInit()`
- `xrtUnit()`
- `xrtThreadAttachCurrent()`
- `xrtThreadDetachCurrent()`
- `xrtGetError()`
- `xrtClearError()`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
	xrtInit();
	
	printf("App Path: %s\n", (char*)xCore.AppPath);
	printf("App File: %s\n", (char*)xCore.AppFile);
	
	xrtClearError();
	printf("Error: %s\n", (char*)xrtGetError());
	
	xrtUnit();
	return 0;
}
```

### Compatibility Note

`xCore` still exists as the process-level runtime object, but modern XRT design no longer treats arbitrary direct field access as the preferred public contract.

In particular:

- thread-local error state should be accessed through `xrtGetError()`
- temporary memory should be treated as thread-local runtime temporary memory
- host-created threads that use runtime-bound APIs should attach with `xrtThreadAttachCurrent()`

---

## Related Documents

- [Base Runtime](api-base.en.md)
- [Thread Runtime](api-thread.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
- [Main Index](README.en.md)
### Runtime Return Conventions

The current mainline no longer recommends relying on hidden return slots such as `xCore.sRet / xCore.iRet / xCore.nRet`.

The preferred contracts are:

- use explicit output parameters when an API exposes them, such as `size_t* iRetSize`
- use `strlen((char*)...)` when the returned value is a UTF-8 string and you need its byte length
- use `xrtGetError()` for thread-local error retrieval

**Recommended Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main()
{
	xrtInit();
	
	str dir = xrtPathGetDir((str)"C:\\test\\file.txt", 0);
	printf("Directory: %s (length: %zu)\n", dir, strlen((char*)dir));
	xrtFree(dir);
	
	xrtUnit();
	return 0;
}
```

### Error Handling

| Field | Type | Description |
|-------|------|-------------|
| `OnError` | Function pointer | Error callback function |

**Usage Example:**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT ERROR] %s\n", sError);
}

int main() {
    xrtInit();
    
    // Set error callback
    xCore.OnError = MyErrorHandler;
    
    // Simulate an error
    xrtSetError((str)"Test error message", FALSE);
    
    // Check for errors
    if ( xrtGetError()[0] != 0 ) {
        printf("Current error: %s\n", (char*)xrtGetError());
        xrtClearError();
    }
    
    xrtUnit();
    return 0;
}
```

### Application Info

| Field | Type | Description |
|-------|------|-------------|
| `LocalAddr` | `uint` | Local IP address (for generating XID) |
| `AppFile` | `str` | Current program full path |
| `AppPath` | `str` | Current program directory |

### Thread-Local Temporary Memory

Current `xrtTempMemory()` should be understood as thread-local runtime temporary memory.

Use it with these rules:

1. It is intended for short-lived temporary results
2. Later temporary allocations in the same thread may invalidate earlier temporary results
3. It is released when the thread runtime is detached, or explicitly by `xrtFreeTempMemory()`
4. Different threads do not share the same temporary slots

**Usage Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // These temporary memories are auto-managed, no xrtFree needed
    str temp1 = xrtTempMemory(100);
    strcpy((char*)temp1, "Temporary data 1");
    printf("temp1: %s\n", temp1);
    
	str temp2 = xrtTempMemory(200);
	strcpy((char*)temp2, "Temporary data 2");
	printf("temp2: %s\n", temp2);

	xrtUnit();
	return 0;
}
```

### Custom Memory Functions

You can replace the default memory allocation functions:

| Field | Type | Description |
|-------|------|-------------|
| `malloc` | Function pointer | Memory allocation function |
| `calloc` | Function pointer | Zero-fill allocation function |
| `realloc` | Function pointer | Reallocation function |
| `free` | Function pointer | Free function |

**Customization Example:**
```c
#include "xrt.h"
#include <stdio.h>

static size_t g_allocCount = 0;
static size_t g_totalBytes = 0;

ptr MyMalloc(size_t size) {
    g_allocCount++;
    g_totalBytes += size;
    printf("[Alloc #%zu] %zu bytes\n", g_allocCount, size);
    return malloc(size);
}

void MyFree(ptr pMem) {
    if (pMem) {
        printf("[Free] Address %p\n", pMem);
        free(pMem);
    }
}

int main() {
    xrtInit();
    
    // Use custom allocator
    xCore.malloc = MyMalloc;
    xCore.free = MyFree;
    
    // Now all xrtMalloc/xrtFree will use custom functions
    ptr data = xrtMalloc(256);
    xrtFree(data);
    
    printf("\nStats: %zu allocations, %zu bytes total\n", g_allocCount, g_totalBytes);
    
    xrtUnit();
    return 0;
}
```

---

## Platform Detection Macros

XRT uses the following macros for platform detection:

### Operating System Detection

```c
#if defined(_WIN32) || defined(_WIN64)
    // Windows platform code
#elif defined(__unix__) || defined(__unix) || defined(__linux__)
    // Linux/Unix platform code
#elif defined(__APPLE__) || defined(__MACH__)
    // macOS platform code
#elif defined(__ANDROID__)
    // Android platform code
#endif
```

### Compiler Detection

```c
#if defined(__TINYC__)
    // TCC compiler
#elif defined(__GNUC__)
    // GCC compiler
#elif defined(__clang__)
    // Clang compiler
#elif defined(_MSC_VER)
    // MSVC compiler
#endif
```

### Architecture Detection

```c
#if defined(__x86_64__) || defined(_M_X64)
    // 64-bit architecture
#elif defined(__i386__) || defined(_M_IX86)
    // 32-bit architecture
#endif
```

---

## Best Practices

### 1. Use Type Aliases

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ Recommended: Use XRT type aliases
    i64 value = 1234567890123LL;
    str text = (str)"Hello XRT";
    u32 count = 100;
    f64 ratio = 3.14159265359;
    
    printf("Integer: %" PRId64 "\n", value);
    printf("String: %s\n", text);
    printf("Count: %u\n", count);
    printf("Ratio: %.10f\n", ratio);
    
    // ❓ Not recommended (though possible)
    // long long value = 1234567890123LL;
    // unsigned char* text = "Hello";
    
    xrtUnit();
    return 0;
}
```

### 2. Check Initialization State

```c
#include "xrt.h"
#include <stdio.h>

void EnsureInit()
{
	// In the current mainline, calling xrtInit() repeatedly is acceptable
	// Callers do not need to read internal init flags
	xrtInit();
}

int main()
{
	// First call performs initialization
	EnsureInit();
	printf("XRT is initialized\n");
	
	// Repeated calls remain safe
	EnsureInit();
	printf("Repeated init calls are also safe\n");
	
	xrtUnit();
	return 0;
}
```

### 3. Use Temporary Memory

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // For small temporary memory, prefer xrtTempMemory
    str temp = xrtTempMemory(256);  // Auto-managed
    sprintf((char*)temp, "Temporary data: %d", 12345);
    printf("%s\n", temp);
    // No xrtFree needed
    
    // For memory that needs to be held long-term, use xrtMalloc
    str permanent = xrtMalloc(1024);  // Requires xrtFree
    strcpy((char*)permanent, "Permanent data");
    printf("%s\n", permanent);
    xrtFree(permanent);  // Must free
    
    xrtUnit();
    return 0;
}
```

### 4. Error Handling

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
    // Optional: log, send notification, etc.
}

bool DoSomething(int value) {
    if (value < 0) {
        xrtSetError((str)"Parameter cannot be negative", FALSE);
        return false;
    }
    printf("Processing value: %d\n", value);
    return true;
}

int main() {
    xrtInit();
    xCore.OnError = MyErrorHandler;
    
    // Test normal case
    if (DoSomething(10)) {
        printf("Success\n");
    }
    
    // Test error case
    if (!DoSomething(-5)) {
        printf("Failed: %s\n", (char*)xrtGetError());
        xrtClearError();
    }
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Base Functions Library](api-base.en.md) - Memory management functions
- [Charset Conversion](api-charset.en.md) - String type conversion
- [Main Index](README.en.md) - Back to documentation home

---

<div align="center">

[⬆️ Back to Top](#xrt-basic-type-definitions)

</div>
