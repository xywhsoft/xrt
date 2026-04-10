# Base Library

> XRT core foundation: library initialization, memory management, error handling

[English](api-base.en.md) | [中文](api-base.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constants](#constants)
- [Data Types](#data-types)
- [Global Variables](#global-variables)
- [Library Initialization](#library-initialization)
- [Memory Management](#memory-management)
- [Temporary Memory](#temporary-memory)
- [Error Handling](#error-handling)
- [Supplementary Functions](#supplementary-functions)

---

## Constants

### Boolean Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `TRUE` | `1` | Logical true |
| `FALSE` | `0` | Logical false |
| `null` | `0` | Null pointer/value |

**Notes:**
- These constants are used to distinguish from C standard library's `true`/`false`
- `TRUE`/`FALSE` return values indicate function success or failure
- `null` is equivalent to `NULL`, used for null pointer checking

### API Export Macro

| Macro | Description |
|-------|-------------|
| `XXAPI` | API function export identifier. Defined as `__declspec(dllexport)` when compiling as DLL, empty otherwise |

**Definition:**
```c
#ifdef BUILD_DLL
    #define XXAPI   __declspec(dllexport)
#else
    #define XXAPI
#endif
```

---

## Data Types

### xrtGlobalData

XRT process-global runtime structure. Stores global state and configuration.

**Definition:**
```c
typedef struct {
    int bInit;                          // Whether initialized
    uint32 iInitRef;                    // Global init refcount
    str sNull;                          // Global empty string constant (read-only)
    void (*OnError)(str sError);        // Global error callback
    #if defined(_WIN32) || defined(_WIN64)
        uint64 Frequency;               // Clock frequency
    #endif
    uint LocalAddr;                     // Local IP address (for XID generation)
    str AppFile;                        // Application full path
    str AppPath;                        // Application directory
    ptr (*malloc)(size_t iSize);        // Allocate memory
    ptr (*calloc)(size_t iNum, size_t iSize);
    ptr (*realloc)(ptr pMem, size_t iSize);
    void (*free)(ptr pMem);             // Free memory
} xrtGlobalData;
```

**Member Description:**

| Member | Type | Description |
|--------|------|-------------|
| `bInit` | `int` | Initialization flag, `TRUE` means initialized |
| `iInitRef` | `uint32` | Global init reference count |
| `sNull` | `str` | Global empty string, used for safe returns |
| `OnError` | Function pointer | Callback function when error occurs |
| `AppFile` | `str` | Full path of current program |
| `AppPath` | `str` | Directory of current program |
| `malloc/calloc/realloc/free` | Function pointers | Customizable memory allocation functions |

### xrtThreadData

XRT thread-local runtime structure. Stores the current thread's error state, temporary memory, default RNG state, and cleanup stack.

After phase-1, the following data has moved to thread-local runtime state:

- `LastError`
- temporary memory used by `xrtTempMemory()`
- default RNG state used by `xrtRand32()` / `xrtRand64()` / `xrtRandRange()`

In most cases you do not operate on `xrtThreadData` directly. Use these APIs instead:

- `xrtThreadGetCurrent()`
- `xrtThreadAttachCurrent()`
- `xrtThreadDetachCurrent()`
- `xrtGetError()`

---

## Global Variables

### xCore

Global data instance, available after `xrtInit()`.

**Declaration:**
```c
XXAPI extern xrtGlobalData xCore;
```

**Notes:**
- `xCore` stores process-global state only
- After phase-1, `LastError` and temporary memory are no longer stored in `xCore`
- Use `xrtGetError()` to read the current-thread error message

**Usage Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main()
{
    xrtInit();
    
    // Access application path
    printf("App Path: %s\n", xCore.AppPath);
    
    // Set error callback
    xCore.OnError = MyErrorHandler;
    
    // Check error
    if (xrtGetError()[0] != 0) {
        printf("Error: %s\n", xrtGetError());
    }
    
    xrtUnit();
    return 0;
}
```

---

## Library Initialization

### xrtInit

Initialize XRT library

**Function Prototype:**
```c
XXAPI xrtGlobalData* xrtInit();
```

**Return Value:**
- Returns global data structure pointer `&xCore`

**Notes:**
- **Must** be called before using any XRT functionality
- Multiple calls are safe (checks initialization state)
- Automatically attaches the current thread to the XRT runtime
- Initialization includes:
  - Setting global constants
  - Initializing current-thread runtime state
  - Getting application path
  - Initializing network (Windows)
  - Getting local IP address

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
}

int main() {
    // Initialize library
    xrtGlobalData* pCore = xrtInit();
    
    // Set error callback (optional)
    pCore->OnError = MyErrorHandler;
    
    // Print application info
    printf("App File: %s\n", pCore->AppFile);
    printf("App Path: %s\n", pCore->AppPath);
    
    // Use library features
    str text = xrtFormat("Hello, %s!", "World");
    printf("%s\n", text);
    xrtFree(text);
    
    // Cleanup resources
    xrtUnit();
    return 0;
}
```

---

### xrtUnit

Release XRT library resources

**Function Prototype:**
```c
XXAPI void xrtUnit();
```

**Notes:**
- Detaches the current thread from the XRT runtime
- Releases process-global resources only when the global init refcount reaches zero
- Cleans up network resources (Windows) when the last runtime user exits
- Should be called before program exit

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Use XRT features
    str text = xrtCopyStr("Hello XRT", 0);
    printf("%s\n", text);
    xrtFree(text);
    
    // Cleanup before program ends
    xrtUnit();
    return 0;
}
```

---

## Memory Management

### xrtMalloc

Allocate memory

**Function Prototype:**
```c
XXAPI ptr xrtMalloc(size_t iSize);
```

**Parameters:**
- `iSize` - Number of bytes to allocate

**Return Value:**
- Success: Returns allocated memory pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // Allocate 1024 bytes
    ptr buffer = xrtMalloc(1024);
    if (buffer) {
        // Use memory
        memset(buffer, 0, 1024);
        strcpy((char*)buffer, "Hello Memory!");
        printf("%s\n", (char*)buffer);
        
        // Free memory
        xrtFree(buffer);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtCalloc

Allocate and zero memory

**Function Prototype:**
```c
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize);
```

**Parameters:**
- `iNum` - Number of elements
- `iSize` - Size of each element in bytes

**Return Value:**
- Success: Returns allocated and zeroed memory pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Notes:**
- Allocates `iNum * iSize` bytes of memory
- All bytes initialized to 0

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Allocate array of 100 int32, initialized to 0
    i32* numbers = (i32*)xrtCalloc(100, sizeof(i32));
    if (numbers) {
        // numbers[0] ~ numbers[99] are all 0
        printf("Initial value: %d\n", numbers[50]);  // Output: 0
        
        numbers[0] = 42;
        numbers[99] = 100;
        printf("numbers[0] = %d, numbers[99] = %d\n", numbers[0], numbers[99]);
        
        xrtFree(numbers);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtRealloc

Reallocate memory

**Function Prototype:**
```c
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize);
```

**Parameters:**
- `pMem` - Original memory pointer (can be `NULL`)
- `iSize` - New size in bytes

**Return Value:**
- Success: Returns new memory pointer (may differ from original)
- Failure: Returns `NULL` (original memory unchanged)

**Memory Release:** ✅ Requires `xrtFree` to release

**Notes:**
- If `pMem` is `NULL`, equivalent to `xrtMalloc(iSize)`
- If `iSize` is 0, equivalent to `xrtFree(pMem)`
- New memory may be at different address, original content is copied

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // Initial allocation
    str text = xrtMalloc(100);
    strcpy((char*)text, "Hello");
    printf("Before: %s\n", text);
    
    // Extend to 200 bytes
    text = xrtRealloc(text, 200);
    strcat((char*)text, " World!");
    printf("After: %s\n", text);
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

**Safe Mode:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str old_text = xrtMalloc(100);
    str new_text = xrtRealloc(old_text, 200);
    if (new_text) {
        old_text = new_text;  // Update pointer
        printf("Realloc succeeded\n");
    } else {
        // Reallocation failed, old_text still valid
        printf("Realloc failed\n");
    }
    xrtFree(old_text);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFree

Free memory

**Function Prototype:**
```c
XXAPI void xrtFree(ptr pmem);
```

**Parameters:**
- `pmem` - Memory pointer to free

**Notes:**
- Checks if pointer is `NULL` first
- Passing `NULL` is safe (won't crash)
- Can only free memory allocated by `xrtMalloc/xrtCalloc/xrtRealloc`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = xrtMalloc(256);
    xrtFree(text);       // Normal release
    
    xrtFree(NULL);       // Safe, no error
    
    // Recommended practice to avoid double free
    str data = xrtMalloc(100);
    if (data) {
        // Use data
        xrtFree(data);
        data = NULL;     // Recommended: set to NULL after free
    }
    
    xrtUnit();
    return 0;
}
```

---

## Temporary Memory

### xrtTempMemory

Allocate current-thread temporary memory (auto-managed)

**Function Prototype:**
```c
XXAPI ptr xrtTempMemory(size_t iSize);
```

**Parameters:**
- `iSize` - Number of bytes to allocate

**Return Value:**
- Success: Returns temporary memory pointer
- Failure: Returns `NULL`

**Memory Release:** ⭕ **No manual release needed** (auto-managed)

**Notes:**
- Uses 32 temporary memory slots owned by the current thread
- The 33rd call on the same thread automatically frees that thread's 1st slot
- Different threads do not overwrite each other's temporary memory
- Suitable for temporary, short-term small memory
- **Not suitable for** data that needs long-term holding
- Depends on thread runtime state; the bootstrap thread is attached after `xrtInit()`, and `xrtThreadCreate()` threads are attached automatically

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Temporary formatted string
    str temp1 = xrtTempMemory(100);
    sprintf((char*)temp1, "Number: %d", 123);
    printf("%s\n", temp1);  // Use
    
    str temp2 = xrtTempMemory(200);
    sprintf((char*)temp2, "Value: %.2f", 3.14);
    printf("%s\n", temp2);
    
    // No need to call xrtFree
    
    xrtUnit();
    return 0;
}
```

**Cautions:**
```c
// ❌ Wrong usage: returning temporary memory
str GetTempString() {
    str temp = xrtTempMemory(100);
    strcpy((char*)temp, "data");
    return temp;  // Dangerous! Later temporary allocations on the same thread may overwrite it
}

// ✅ Correct usage: use immediately
void PrintFormat(int value) {
    str temp = xrtTempMemory(50);
    sprintf((char*)temp, "Value: %d", value);
    printf("%s\n", temp);  // Used immediately
}
```

---

### xrtFreeTempMemory

Free all temporary memory of the current thread

**Function Prototype:**
```c
XXAPI void xrtFreeTempMemory();
```

**Notes:**
- Frees all 32 temporary memory slots of the current thread
- Resets the current thread's temporary memory index
- Usually called automatically during `xrtUnit()` or `xrtThreadDetachCurrent()`
- Can be called manually when immediate release is needed

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    for (int i = 0; i < 100; i++) {
        str temp = xrtTempMemory(1024);
        sprintf((char*)temp, "Item %d", i);
        // Use temp
        
        if (i % 32 == 31) {
            xrtFreeTempMemory();  // Clean every 32 times to avoid overwrite
        }
    }
    
    xrtUnit();
    return 0;
}
```

---

## Error Handling

### xrtSetError

Set error message (UTF-8 string)

**Function Prototype:**
```c
XXAPI void xrtSetError(str sError, bool bFree);
```

**Parameters:**
- `sError` - Error message string
- `bFree` - Whether the string needs to be freed
  - `TRUE` - XRT will free when setting new error or cleanup
  - `FALSE` - XRT won't free (constant string)

**Notes:**
- Sets the current-thread error state
- If `xCore.OnError` callback is set, triggers the callback
- Does not affect other threads

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT ERROR] %s\n", sError);
}

int main() {
    xrtInit();
    xCore.OnError = MyErrorHandler;
    
    // Use constant string (no free)
    xrtSetError("File not found", FALSE);
    
    // Use dynamic string (XRT will free)
    int param = -1;
    str error = xrtFormat("Invalid parameter: %d", param);
    xrtSetError(error, TRUE);  // XRT will handle freeing

    fprintf(stderr, "Current Error: %s\n", xrtGetError());
    
    xrtUnit();
    return 0;
}
```

---

### xrtSetErrorU16

Set error message (UTF-16 string)

**Function Prototype:**
```c
XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree);
```

**Parameters:**
- `sError` - UTF-16 error message
- `iSize` - String length (character count, 0 for auto-calculate)
- `bFree` - Whether to free

**Notes:**
- Automatically converts to UTF-8 before setting

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Set UTF-16 error message
    u16str error = (u16str)L"File not found";
    xrtSetErrorU16(error, 0, FALSE);
    
    // Error message converted to UTF-8
    printf("Error: %s\n", xrtGetError());
    
    xrtUnit();
    return 0;
}
```

---

### xrtSetErrorU32

Set error message (UTF-32 string)

**Function Prototype:**
```c
XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree);
```

**Parameters:**
- `sError` - UTF-32 error message
- `iSize` - String length (character count, 0 for auto-calculate)
- `bFree` - Whether to free

**Notes:**
- Automatically converts to UTF-8 before setting

---

### xrtGetError

Get the error message of the current thread.

**Function Prototype:**
```c
XXAPI str xrtGetError();
```

**Return Value:**
- Returns the current-thread error string
- Returns the empty-string sentinel when there is no error

**Notes:**
- This is the recommended way to read error state after phase-1
- Different threads observe independent error state

---

### xrtClearError

Clear error message

**Function Prototype:**
```c
XXAPI void xrtClearError();
```

**Notes:**
- Resets the current-thread error state to the empty-string sentinel
- Frees the previous error first if it owns the string

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main()
{
    xrtInit();
    
    // Simulate an error
    xrtSetError("Something went wrong", FALSE);
    
    // Check and handle error
    if (xrtGetError()[0] != 0) {
        printf("Error: %s\n", xrtGetError());
        xrtClearError();  // Clear error
    }
    
    // Confirm error cleared
    if (xrtGetError()[0] == 0) {
        printf("Error cleared.\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

## Supplementary Functions

### memmem

Find sub-memory in memory (Windows only)

**Function Prototype:**
```c
#if defined(_WIN32) || defined(_WIN64)
    XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize);
#endif
```

**Parameters:**
- `pMem` - Memory to search in
- `iMemSize` - Size of memory to search
- `pSub` - Sub-memory to find
- `iSubSize` - Size of sub-memory

**Return Value:**
- Success: Returns pointer to found position
- Failure: Returns `NULL`

**Notes:**
- Linux/Unix systems have this function natively
- Windows systems use XRT implementation

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    #if defined(_WIN32) || defined(_WIN64)
    char data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    char pattern[] = {0x03, 0x04};
    
    ptr found = memmem(data, sizeof(data), pattern, sizeof(pattern));
    if (found) {
        size_t offset = (char*)found - data;
        printf("Found at offset: %zu\n", offset);  // Output: 2
    } else {
        printf("Pattern not found\n");
    }
    #else
    printf("memmem is a native function on Linux/Unix\n");
    #endif
    
    xrtUnit();
    return 0;
}
```

---

### u16len

Get UTF-16 string length

**Function Prototype:**
```c
XXAPI size_t u16len(u16str sText);
```

**Parameters:**
- `sText` - UTF-16 string

**Return Value:**
- String length (character count, excluding terminating `\0`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    u16str text = (u16str)L"Hello";
    size_t len = u16len(text);  // Returns 5
    printf("UTF-16 length: %zu\n", len);
    
    u16str chinese = (u16str)L"Hello World";
    size_t len2 = u16len(chinese);  // Returns 11
    printf("UTF-16 length: %zu\n", len2);
    
    xrtUnit();
    return 0;
}
```

---

### u32len

Get UTF-32 string length

**Function Prototype:**
```c
XXAPI size_t u32len(u32str sText);
```

**Parameters:**
- `sText` - UTF-32 string

**Return Value:**
- String length (character count, excluding terminating `\0`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    u32str text = (u32str)U"Hello";
    size_t len = u32len(text);  // Returns 5
    printf("UTF-32 length: %zu\n", len);
    
    u32str emoji = (u32str)U"Hello🌍";
    size_t len2 = u32len(emoji);  // Returns 6 (including emoji)
    printf("With emoji UTF-32 length: %zu\n", len2);
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Initialization and Cleanup

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
}

int main() {
    // 1. Initialize
    xrtGlobalData* pCore = xrtInit();
    
    // 2. Set error handling
    pCore->OnError = MyErrorHandler;
    
    // 3. Use features
    printf("XRT initialized, app path: %s\n", pCore->AppPath);
    
    // 4. Cleanup
    xrtUnit();
    return 0;
}
```

### 2. Memory Management Strategy

```c
#include "xrt.h"
#include <stdio.h>

void UseIt(str s) { printf("%s\n", s); }
void LoadData(str s) { strcpy((char*)s, "Loaded data"); }

int main() {
    xrtInit();
    int value = 42;
    
    // Short-term temporary use → xrtTempMemory
    str temp = xrtTempMemory(256);
    sprintf((char*)temp, "Temp: %d", value);
    UseIt(temp);
    // No need to free
    
    // Long-term use → xrtMalloc + xrtFree
    str permanent = xrtMalloc(1024);
    LoadData(permanent);
    printf("%s\n", permanent);
    xrtFree(permanent);
    
    // Array allocation → xrtCalloc (auto-zero)
    i32* arr = (i32*)xrtCalloc(100, sizeof(i32));
    arr[0] = 1;
    arr[99] = 100;
    printf("arr[0]=%d, arr[99]=%d\n", arr[0], arr[99]);
    xrtFree(arr);
    
    xrtUnit();
    return 0;
}
```

### 3. Error Handling

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT Error] %s\n", sError);
    // Optional: write to log file
}

bool MyFunction(int param) {
    if (param < 0) {
        xrtSetError("Parameter must be positive", FALSE);
        return false;
    }
    return true;
}

int main() {
    xrtInit();
    xCore.OnError = MyErrorHandler;
    
    // Call function
    if (!MyFunction(-1)) {
        printf("Check error: %s\n", xrtGetError());
        xrtClearError();
    }
    
    if (MyFunction(10)) {
        printf("Function succeeded\n");
    }
    
    xrtUnit();
    return 0;
}
```

### 4. Safe Memory Operations

```c
#include "xrt.h"
#include <stdio.h>

int SafeMemoryExample() {
    xrtInit();
    size_t size = 1024;
    size_t new_size = 2048;
    
    // ✅ Safe: check return value
    ptr data = xrtMalloc(size);
    if (data == NULL) {
        xrtSetError("Memory allocation failed", FALSE);
        xrtUnit();
        return -1;
    }
    printf("Allocated %zu bytes\n", size);
    
    // ✅ Safe: reallocation
    ptr new_data = xrtRealloc(data, new_size);
    if (new_data) {
        data = new_data;
        printf("Reallocated to %zu bytes\n", new_size);
    } else {
        // Reallocation failed, keep original data
        printf("Realloc failed, keeping original\n");
    }
    
    // ✅ Safe: set to NULL after free
    xrtFree(data);
    data = NULL;
    printf("Memory freed\n");
    
    xrtUnit();
    return 0;
}

int main() {
    return SafeMemoryExample();
}
```

---

## Performance Tips

### Temporary Memory Performance

```c
#include "xrt.h"
#include <stdio.h>

void Process(str s) { /* Process string */ }

int main() {
    xrtInit();
    
    // ❌ Inefficient: frequent alloc/free
    for (int i = 0; i < 100; i++) {
        str temp = xrtMalloc(256);
        sprintf((char*)temp, "Item %d", i);
        Process(temp);
        xrtFree(temp);
    }
    
    // ✅ Efficient: use current-thread temporary memory for short-lived buffers
    for (int i = 0; i < 30; i++) {
        str temp = xrtTempMemory(256);
        sprintf((char*)temp, "Item %d", i);
        Process(temp);
    }
    
    // ✅ Optimal: pre-allocate
    str buffer = xrtMalloc(256);
    for (int i = 0; i < 1000; i++) {
        sprintf((char*)buffer, "Item %d", i);
        Process(buffer);
    }
    xrtFree(buffer);
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Type Definitions](types.en.md) - Basic types and global structures
- [Charset Conversion](api-charset.en.md) - String encoding conversion
- [Main Index](README.en.md) - Return to documentation home

---

<div align="center">

[⬆️ Back to Top](#base-library)

</div>
