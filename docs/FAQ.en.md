# XRT Frequently Asked Questions (FAQ)

> Common problems and solutions when using XRT

[English Version](FAQ.en.md) | [Return to Documentation Index](docs/README.md)

---

## 📑 Table of Contents

- [Compilation Issues](#compilation-issues)
- [Memory Management Issues](#memory-management-issues)
- [API Usage Issues](#api-usage-issues)
- [Performance Optimization Issues](#performance-optimization-issues)
- [Cross-Platform Issues](#cross-platform-issues)
- [JSON Processing Issues](#json-processing-issues)
- [Value Type Issues](#value-type-issues)
- [Network Communication Issues](#network-communication-issues)

---

## Compilation Issues

### Question 1: "undefined symbol" error

**Error message**:
```
tcc: error: undefined symbol 'xrtMalloc'
```

**Cause**: Forgot to define `XRT_IMPLEMENTATION`

**Solution**:
```c
// ❌ Incorrect usage
#include "xrt.h"

// ✅ Correct usage
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

### Question 2: TCC Compilation Error

**Error message**:
```
tcc: error: could not write 'test.exe': Permission denied
```

**Cause**: Program is running or in use

**Solution**:
```batch
REM Method 1: Close the process
taskkill /F /IM test.exe

REM Method 2: Use /force flag
tcc -force xrt_log.c test.c -o test.exe
```

### Question 3: GCC Link Error

**Error message**:
```
gcc: error: undefined reference to 'xrtMalloc'
```

**Cause**: Forgot to link xrt.c

**Solution**:
```bash
# Correct way
gcc xrt_log.c test.c -o test.exe

# Incorrect way
gcc test.c -o test.exe  # xrt_log.c not linked
```

### Question 4: MSVC Compilation Error

**Error message**:
```
error LNK2019: unresolved external symbol xrtMalloc
```

**Cause**: xrt.c not added to the project

**Solution**:
```
# Add in Visual Studio project
xrt_log.c
test.c
```

---

## Memory Management Issues

### Question 5: Memory Leak

**Cause**: Allocated using `xrtMalloc` but not freed using `xrtFree`

**Solution**:
```c
// ❌ Incorrect usage
str data = xrtMalloc(1024);
// Use data...
// Forgot to free!

// ✅ Correct usage
str data = xrtMalloc(1024);
// Use data...
xrtFree(data);  // Must free
```

### Question 6: Program crashes after using temporary memory

**Cause**: Used more than 32 temporary memory slots

**Solution**:
```c
// ❌ Incorrect usage
for (int i = 0; i < 100; i++) {
    str temp = xrtTempMemory(1024);
    // If i >= 32, slot 0 will be released, causing previously allocated memory to become invalid
}

// ✅ Correct usage
for (int i = 0; i < 32; i++) {
    str temp = xrtTempMemory(1024);
    // Use temporary memory...
    // Automatically released when function returns
}
```

### Question 7: Frequent malloc causes memory fragmentation

**Cause**: Frequent small allocations/deallocations

**Solution**:
```c
// ❌ Bad practice
for (int i = 0; i < 1000; i++) {
    void* p = xrtMalloc(128);  // 1000 allocations of 128 bytes
    // Use p ...
    xrtFree(p);
}

// ✅ Good practice: Use memory pool
xvalue pool = xvoMemPoolCreate(128);
for (int i = 0; i < 1000; i++) {
    void* p = xrtMemPoolAlloc(pool, 128);
    // Use p ...
    xrtMemPoolFree(pool, p);  // No fragmentation
}
```

---

## API Usage Issues

### Question 8: How to properly free Value type

**Incorrect example**:
```c
// ❌ Error: Direct free
xvalue val = xvoCreateInt(123);
xrtFree(val);  // Error! Value must use xvoUnref
```

**Correct example**:
```c
// ✅ Correct: Use reference counting
xvalue val = xvoCreateInt(123);
// ... use val ...
xvoUnref(val);  // Correct: Decrease reference count
```

### Question 9: Value type conversion failed

**Incorrect example**:
```c
// ❌ Error: Direct conversion
int64 ival = (int64)xvoInt(value);  // Error: Not an integer type
```

**Correct example**:
```c
// ✅ Correct: Use type checking
if (xvoIsInt(value)) {
    int64 ival = xvoGetInt(value);
}
```

### Question 10: How to determine Value type

**Incorrect example**:
```c
// ❌ Error: Direct access to internal fields
if (value->Type == 2) {  // Error: Direct access to internal fields
    printf("Int\n");
}
```

**Correct example**:
```c
// ✅ Correct: Use type checking functions
if (xvoIsInt(value)) {
    int64 ival = xvoGetInt(value);
    printf("Int: %lld\n", ival);
} else if (xvoIsText(value)) {
    str s = xvoGetText(value);
    printf("Text: %s\n", s);
}
```

### Question 11: Array operation out of bounds

**Incorrect example**:
```c
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 10);      // Allocate 10 elements
xvoArrayAppendInt(arr, 1);
xvoArrayAppendInt(arr, 2);
xvoArrayGetValue(arr, 10);    // Error: Out of bounds!
```

**Correct example**:
```c
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 10);      // Allocate 10 elements, indices 0-9
for (int i = 0; i < 10; i++) {
    xvoArrayAppendInt(arr, i + 1);
    xvoArraySetValue(arr, i, xvoCreateInt(i + 1));
}
for (int i = 0; i < 10; i++) {
    int64 val = xvoGetInt(xvoArrayGetValue(arr, i));
    printf("arr[%d] = %lld\n", i, val);
}
```

### Question 12: Dict/Table key does not exist

**Incorrect example**:
```c
xvalue dict = xvoCreateTable();
xvoTableSetText(dict, "name", 0, "XRT", 0, FALSE);
str name = xvoTableGetText(dict, "version", 0);  // Error: Key does not exist!
printf("Version: %s\n", name);  // Undefined behavior
```

**Correct example**:
```c
xvalue dict = xvoCreateTable();
xvoTableSetText(dict, "name", 0, "XRT", 0, FALSE);
str name = xvoTableGetText(dict, "name", 0);  // Correct: Key exists
str version = xvoTableGetText(dict, "version", 0);  // Add this field
if (version != xCore->sNull) {
    printf("Version: %s\n", version);
}
```

---

## Performance Optimization Issues

### Question 13: Poor string concatenation performance

**Problem scenario**:
```c
// ❌ Bad: Frequent memory allocation
str result = xrtConcat("Hello ", "World ", "!");  // 3 allocations
xrtFree(result);
```

**Solution**:
```c
// ✅ Good way: Use formatting
str result = xrtFormat("Hello %s!", "World");
xrtFree(result);  // Only 1 allocation
```

**Performance improvement**: 70-80%

### Question 14: Slow array operations

**Problem scenario**:
```c
// ❌ Bad: Individual assignment
xvalue arr = xvoCreateArray();
for (int i = 0; i < 1000; i++) {
    xvoArraySetValue(arr, i, xvoCreateInt(i));
}
```

**Solution**:
```c
// ✅ Good way: Pre-allocate + set
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 1000);
for (int i = 0; i < 1000; i++) {
    xvoArraySetValue(arr, i, xvoCreateInt(i));
}
```

**Performance improvement**: 80-90%

### Question 15: High memory usage for JSON parsing

**Problem scenario**:
```c
str json = "{\"data\":[";
for (int i = 0; i < 10000; i++) {
    json = xrtFormat("%s{\"id\":%d},", json, i);  // String concatenation will be very large
}
json = xrtFormat("%s]}", json);
xvalue data = xrtParseJSON(json, 0);  // DOM method, high memory usage
xvoUnref(data);
```

**Solution**:
```c
// ✅ Good way: Use SAX parsing
json_sax_cb_t cb = {
    .on_number = on_json_number,
    .on_string = on_json_string,
    .on_start_object = on_json_start,
    .on_end_object = on_json_end,
    // ... other callbacks
};

xrtJsonParseSAX(json_text, strlen(json_text), &cb);
// SAX method: Parse and release on the fly, low memory usage
```

**Memory savings**: 50-70%

---

## Cross-Platform Issues

### Question 16: Path error on Windows

**Problem scenario**:
```c
// ❌ Error: Wrong path separator
char* path = "data\\files\\config.json";
FILE* fp = fopen(path, "r");  // Will fail on Linux
```

**Solution**:
```c
// ✅ Good way: Use XRT path API
char* path = xrtPathJoin("data", "files", "config.json");
FILE* fp = fopen(path, "r");  // Cross-platform compatible
```

### Question 17: Newline character incompatibility

**Problem scenario**:
```c
// ❌ Error: Hardcoded newline character
char* log = "line1\nline2\n";
FILE* fp = fopen("log.txt", "w");
fprintf(fp, "%s\n", log);  // Incorrect newline on Windows
```

**Solution**:
```c
// ✅ Good way: Use XRT newline constant
char* log = "line1" LINE_END "line2" LINE_END;
FILE* fp = fopen("log.txt", "w");
fprintf(fp, "%s", log);  // Automatically use platform newline
fclose(fp);
```

**LINE_END definition**:
```c
#ifdef _WIN32 || defined(_WIN64)
#define LINE_END "\r\n"
#else
#define LINE_END "\n"
#endif
```

### Question 18: Chinese path garbled

**Problem scenario**:
```c
// ❌ Error: Character set not specified
FILE* fp = fopen("数据\\配置.json", "r");
char buf[1024];
fread(buf, 1, 1024, fp);
// Output garbled text
```

**Solution**:
```c
// ✅ Good way: Specify character set
#include "xrt.h"

FILE* fp = fopen("数据\\配置.json", "r", XRT_CP_UTF8);
char buf[1024];
fread(buf, 1, 1024, fp);
fclose(fp);
```

**Character set constants**:
```c
XRT_CP_UTF8     // UTF-8
XRT_CP_UTF16    // UTF-16
XRT_CP_UTF32    // UTF-32
XRT_CP_AUTO      // Auto detect
```

---

## JSON Processing Issues

### Question 19: JSON parsing failed

**Problem scenario**:
```c
str json = "{\"name\":\"xrt\",\"features\":[\"fast\",\"simple\"}";
xvalue data = xrtParseJSON(json, 0);
if (!data || xvoIsNull(data)) {
    printf("JSON parse failed!\n");
    return;
}
// No error message, don't know why it failed
```

**Solution: Add error handling and detailed reporting**
```c
// ✅ Good way: Use error callback
void on_error(str sError) {
    printf("JSON Error: %s\n", sError);
}

xrtGlobalData* xCore = xrtInit();
xCore->OnError = on_error;  // Set error callback

str json = "{\"name\":\"xrt\",\"features\":[\"fast\",\"simple\"]}";
xvalue data = xrtParseJSON(json, 0);
if (!data || xvoIsNull(data)) {
    printf("JSON parse failed! Last Error: %s\n", xCore->LastError);
    return;
}
```

### Question 20: JSON formatting output not美观

**Problem scenario**:
```c
xvalue data = xvoCreateTable();
xvoTableSetText(data, "title", 0, "XRT", 0, FALSE);
xvoTableSetInt(data, "count", 0, 42);
xvoTableSetText(data, "features", 0, "fast, simple", 0, TRUE);

size_t len = 0;
str json = xrtStringifyJSON(data, FALSE, &len);  // FALSE = Compressed output
printf("%s\n", json);
xrtFree(json);
```

**Output result (not美观)**:
```json
{"title":"XRT","count":42,"features":"fast,simple"}
```

**Solution**:
```c
// ✅ Good way: Use formatted output
size_t len = 0;
str json = xrtStringifyJSON(data, TRUE, &len);  // TRUE = Formatted
printf("%s\n", json);
xrtFree(json);
```

**Output result (美观)**:
```json
{
    "title": "XRT",
    "count": 42,
    "features": "fast, simple",
    "features": [
        "fast",
        "simple"
    ]
}
```

---

## Value Type Issues

### Question 21: Value comparison failed

**Problem scenario**:
```c
xvalue v1 = xvoCreateInt(100);
xvalue v2 = xvoCreateInt(100);

// ❌ Error: Direct comparison
if (v1 == v2) {
    printf("Equal!\n");
}
```

**Error reason**: Directly comparing object pointers, only comparing pointer addresses

**Solution**: Use type-safe comparison functions
```c
// ✅ Good way: Use comparison function
if (xvoEqual(v1, v2)) {
    printf("Equal!\n");
}

// ✅ Also possible: Use type checking
if (xvoIsInt(v1) && xvoIsInt(v2)) {
    if (xvoGetInt(v1) == xvoGetInt(v2)) {
        printf("Values are equal!\n");
    }
}
```

### Question 22: Traversing Array/List is slow

**Problem scenario**:
```c
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 10000);  // 10000 elements

// ❌ Bad: Each call to xvoArrayGetValue triggers reference check
for (int i = 0; i < 10000; i++) {
    int64 val = xvoGetInt(xvoArrayGetValue(arr, i));
    printf("arr[%d] = %lld\n", i, val);
}
```

**Solution: Use cached access**
```c
// ✅ Good way: Use pointer array (array_point.h)
xparray ptr_arr = xrtPtrArrayCreate(10, 10);
for (int i = 0; i < 10000; i++) {
    xvoArraySetValue(arr, i, xvoCreateInt(i));
}

// Get pointer array
xparray ptr_array = xvoGetArray(arr);
for (int i = 0; i < 10000; i++) {
    xvalue val = xvoIntArrGetValue(ptr_arr, i);
    int64 ival = xvoGetInt(val);
    printf("arr[%d] = %lld\n", i, ival);
}
```

**Performance improvement**: 200-300%

### Question 23: Deep copy is slow

**Problem scenario**:
```c
xvalue data = xvoCreateTable();
xvoTableSetText(data, "name", 0, "XRT", 0, FALSE);
xvoTableSetInt(data, "version", 0, 1);
xvoTableSetInt(data, "features", 0, "xrt", 0, TRUE);
xvoTableSetInt(data, "items", 0, NULL, 0);  // Array

xvalue copy = xvoDeepCopy(data);  // Deep copy entire tree structure
xvoUnref(data);
xvoUnref(copy);
```

**Cause**:
- Dict/List uses AVL tree, deep copy traverses the entire tree
- Each node needs to allocate new memory
- Frequent reference counting operations

**Solution**:
```c
// ✅ Good way: Shallow copy + set parent
xvalue parent_table = xvoCreateTable();
xvoTableSetParent(data, parent_table);

for (int i = 0; i < 10; i++) {
    str name = xvoTableGetText(data, "items", 0);
    xvalue item = xvoCreateInt(i + 1);
    xvoTableSetInt(parent_table, name, 0, item);
}
xvoUnref(parent_table);
xvoUnref(data);
```

**Performance improvement**: 80-90%

---

## Network Communication Issues

### Question 24: HTTP request timeout

**Problem scenario**:
```c
xvalue response = xrtHttpGet("https://api.example.com/data", NULL, NULL);
// No timeout, will wait indefinitely
```

**Solution**: Set timeout time**

```c
xvalue options = xvoCreateTable();
xrtTableSetText(options, "timeout", 0, "30", 0, FALSE);
xrtTableSetInt(options, "connect", 0, 30);  // 30 second timeout
xrtTableSetInt(options, "read", 0, 30);  // 30 second read timeout

xvalue response = xrtHttpGet("https://api.example.com/data", NULL, options);
```

### Question 25: TLS handshake failed

**Problem scenario**:
```c
xvalue tls_options = xvoCreateTable();
xrtTableSetText(tls_options, "host", 0, "example.com", 0, FALSE);
xrtTableSetInt(tls_options, "port", 0, 443, 0, 1);  // HTTPS port
xrtTableSetInt(tls_options, "verify", 0, TRUE);  // Verify certificate

xvalue response = xrtHttpsGet("https://example.com/api", tls_options, NULL, NULL);
// TLS handshake failed, no detailed error information
```

**Solution: Get detailed error information**
```c
void tls_error_callback(str msg) {
    printf("TLS Error: %s\n", msg);
}

xrtCore->OnError = tls_error_callback;
```

---

## Other Issues

### Question 26: Program crashes

**Symptom**: Crashes when reaching a certain line, no error message

**Possible causes**:
1. Null pointer dereference
2. Stack overflow
3. Memory out of bounds
4. Type mismatch
5. Reference counting error

**Solution: Use Valgrind to detect**

```bash
# Add debug symbols during compilation
gcc -g -O0 -g xrt_log.c test.c -o test.exe

# Use Valgrind to detect memory leaks
valgrind --leak-check=full ./test.exe
```

### Question 27: Performance not up to standard

**Symptom**: Theoretical performance should be fast, but actual performance is slow

**Optimization checklist**:
```c
// 1. Are you using the correct container type
// 2. Avoided unnecessary copies
// 3. Are you using pre-allocation
// 4. Are you using efficient algorithms
// 5. Is optimized compilation enabled
```

### Question 28: Program becomes larger after compilation

**Symptom**: exe file is very large after adding debug symbols

**Solution**: Release version removes debug symbols

```batch
# Debug version (with debug symbols)
gcc -g -O3 xrt_log.c test.c -o test_d.exe

# Release version (optimized + stripped)
gcc -O3 -s -DNDEBUG xrt_log.c test.c -o test.exe

# Remove debug symbols
strip test.exe
```

---

## Debugging Tips

### Using breakpoint debugging

**GDB usage**:
```bash
gcc -g -o test.exe xrt_log.c test.c
gdb ./test.exe
(gdb) b main
(gdb) run
(gdb) cont
```

**VS Code usage**:
```c
// Click to the left of the line number to set breakpoints
F5 - Start debugging
```

### Print debugging

```c
// Add print statements to track execution flow
printf("Step 1\n");
printf("Step 2\n");
// ...
```

### Memory usage analysis

```bash
# View process memory usage
tasklist | grep <program name>

# Use Valgrind to detect memory leaks
valgrind --leak-check=full ./program
```

---

## Related Links

- [README.md](README.md) - Project description
- [api-value.md](docs/api-value.md) - Value type detailed documentation
- [api-json.md](docs/api-json.md) - JSON processing documentation
- [docs/api-string.md](docs/api-string.md) - String operations documentation
- [docs/api-file.md](docs/api-file.md) - File operations documentation
- [docs/api-base.md](docs/api-base.md) - Base functions documentation

---

**XRT FAQ Document Version: 1.0** | **Last Updated: 2025-01**
