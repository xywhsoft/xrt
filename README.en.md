# XRT - X Runtime Library

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](https://github.com)
[![Language](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

**Lightweight, High-Performance C Runtime Library**

English | [简体中文](README.md)

</div>

---

## 📖 Project Overview

**xrt** (X Runtime Library) is a full-featured C runtime library providing **32 functional modules** including memory management, character set conversion, file handling, data structures, dynamic type system, JSON processing, template engine and more. The library features a single-header design, zero external dependencies, and cross-platform compilation support, aiming to provide modern, efficient infrastructure support for C language developers.

### ✨ Core Features

- 🚀 **Zero Dependencies**: No external dependencies except the standard C library
- 📦 **Single Header**: All core APIs defined in `xrt.h`, 2300+ lines of complete declarations
- 🔧 **Modular Design**: 32 independent sub-libraries, use as needed
- 🌍 **Cross-Platform**: Supports Windows, Linux, macOS (x86/x64/ARM64)
- 🔨 **Multi-Compiler**: Compatible with TCC, GCC, Clang, MSVC
- ⚡ **High Performance**: Built-in multi-level memory pools, reference counting, inline optimization
- 🎯 **Dynamic Types**: 16 data types Value system with 26-bit reference counting and auto-release
- 💾 **Smart Memory**: 32-slot ring buffer temporary memory, GC mark-sweep support
- 📝 **Template Engine**: Template system with variable substitution, conditionals, and loop iteration

---

## 🎯 Main Functional Modules

### 🔹 Infrastructure Layer

| Module | Header | Description | Main API |
|--------|--------|-------------|----------|
| **base** | `base.h` | Memory management, error handling | `xrtMalloc`, `xrtFree`, `xrtTempMemory` |
| **charset** | `charset.h` | Character set conversion | `xrtUTF8to16`, `xrtUTF16to8`, `xrtConvCharset` |
| **hash** | `hash.h` | Hash computation (nmhash32x/rapidhash) | `xrtHash32`, `xrtHash64`, `xrtHash64_Nano` |
| **math** | `math.h` | Random number generation (PCG) | `xrtRand32`, `xrtRand64`, `xrtRandRange` |
| **time** | `time.h` | Time handling | `xrtNow`, `xrtDateSerial`, `xrtDateAdd`, `xrtDateDiff` |

### 🔹 System Interaction Layer

| Module | Header | Description | Main API |
|--------|--------|-------------|----------|
| **os** | `os.h` | System operations | `xrtRun`, `xrtStart`, `xrtChain` |
| **file** | `file.h` | File operations | `xrtOpen`, `xrtFileReadAll`, `xrtDirScan` |
| **path** | `path.h` | Path handling | `xrtPathGetName`, `xrtPathGetDir`, `xrtPathJoin` |
| **network** | `network.h` | Network functions | `xrtGetLocalIP`, `xrtGetLocalMAC`, `xrtGetLocalName` |
| **thread** | `thread.h` | Thread management | `xrtThreadCreate` |

### 🔹 String Processing Layer

| Module | Header | Description | Main API |
|--------|--------|-------------|----------|
| **string** | `string.h` | String operations | `xrtFindStr`, `xrtReplace`, `xrtSplit`, `xrtFormat` |
| **jnum** | `jnum.h` | Number conversion | `xrtI64ToStr`, `xrtParseNum`, `xrtStrToNum` |
| **template** | `template.h` | Template engine | `xteParse`, `xteMake`, `xteLexer` |

### 🔹 Data Structure Layer

| Module | Header | Description | Data Structure Type |
|--------|--------|-------------|---------------------|
| **buffer** | `buffer.h` | Dynamic buffer | `xbuffer` |
| **array** | `array.h` | Struct array | `xarray` |
| **array_point** | `array_point.h` | Pointer array | `xparray` |
| **stack** | `stack.h` | Static stack | `xstack` |
| **stack_dyn** | `stack_dyn.h` | Dynamic stack (256-step expansion) | `xdynstack` |
| **llist** | `llist.h` | Doubly linked list | `xllist` |
| **avltree** | `avltree.h` | AVL balanced tree | `xavltree` |
| **dict** | `dict.h` | Dictionary (string key-value) | `xdict` |
| **list** | `list.h` | List (integer index) | `xlist` |

### 🔹 Memory Management Layer

| Module | Header | Description | Use Case |
|--------|--------|-------------|----------|
| **bsmm** | `bsmm.h` | Block struct memory management | Fixed-size struct allocation |
| **memunit** | `memunit.h` | Memory unit management | 256-byte page management, GC mark support |
| **mempool_fs** | `mempool_fs.h` | Fixed-size memory pool | High-frequency small object allocation |
| **mempool** | `mempool.h` | General memory pool | Multi-level memory block management, GC support |

### 🔹 Advanced Features Layer

| Module | Header | Description | Features |
|--------|--------|-------------|----------|
| **value** | `value.h` | Dynamic type system | 16 data types, 26-bit ref counting, auto-release |
| **json** | `json.h` | JSON processing | SAX parsing/generation, formatted output |
| **xid** | `xid.h` | Distributed ID | 192-bit unique ID (timestamp+IP+clock+random) |

---

## 🚀 Quick Start

### 📥 Installation

```bash
# Clone repository
git clone https://github.com/yourusername/xrt.git
cd xrt
```

### 🔨 Build

#### Windows Platform

```batch
# Build 64-bit test program using TCC
build_TCC_TEST_x64.bat

# Build 64-bit DLL using GCC
build_GCC_DLL_x64.bat
```

#### Linux/macOS Platform

```bash
# Build test program
bash build_test.sh
```

### 📝 Usage Examples

#### Basic Usage

```c
#include "xrt.h"

int main() {
    // Initialize library
    xrtGlobalData* xCore = xrtInit();
    
    // Use string functions
    str text = xrtReplace("Hello World", 0, "World", 0, "XRT", 0);
    printf("%s\n", text);  // Output: Hello XRT
    xrtFree(text);
    
    // Time handling
    xtime now = xrtNow();
    str timeStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("Current time: %s\n", timeStr);
    xrtFree(timeStr);
    
    // Cleanup
    xrtUnit();
    return 0;
}
```

#### Dynamic Type System (Value)

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // Create array
    xvalue arr = xvoCreateArray();
    xvoArrayAppendInt(arr, 123);
    xvoArrayAppendText(arr, "Hello", 0, FALSE);
    xvoArrayAppendFloat(arr, 3.14);
    
    // Create table (dictionary)
    xvalue table = xvoCreateTable();
    xvoTableSetText(table, "name", 0, "XRT", 0, FALSE);
    xvoTableSetInt(table, "version", 0, 1);
    xvoTableSetBool(table, "active", 0, TRUE);
    
    // Get values
    str name = xvoTableGetText(table, "name", 0);
    printf("Name: %s\n", name);
    
    // Release resources (reference counting auto-managed)
    xvoUnref(arr);
    xvoUnref(table);
    xrtUnit();
    return 0;
}
```

#### JSON Processing

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // Parse JSON
    str jsonText = "{\"name\":\"xrt\",\"version\":1.0,\"features\":[\"fast\",\"simple\"]}";
    xvalue json = xrtParseJSON(jsonText, 0);
    
    // Read values
    str name = xvoTableGetText(json, "name", 0);
    printf("Project: %s\n", name);
    
    // Generate JSON (formatted output)
    xvalue newJson = xvoCreateTable();
    xvoTableSetText(newJson, "status", 0, "ok", 0, FALSE);
    xvoTableSetInt(newJson, "code", 0, 200);
    
    size_t len = 0;
    str output = xrtStringifyJSON(newJson, TRUE, &len);  // TRUE = formatted
    printf("%s\n", output);
    xrtFree(output);
    
    xvoUnref(json);
    xvoUnref(newJson);
    xrtUnit();
    return 0;
}
```

#### Template Engine

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // Create template variables
    xvalue vars = xvoCreateTable();
    xvoTableSetText(vars, "title", 0, "XRT Library", 0, FALSE);
    xvoTableSetInt(vars, "count", 0, 42);
    
    // Parse template
    str template = "Welcome to {$title}! Count: {%count}";
    XTE_LiteObject tpl = xteParse(template, 0, NULL);
    
    // Generate output
    size_t len = 0;
    str result = xteMake(tpl, vars, NULL, NULL, &len);
    printf("%s\n", result);  // Welcome to XRT Library! Count: 42
    xrtFree(result);
    
    xteParseFree(tpl);
    xvoUnref(vars);
    xrtUnit();
    return 0;
}
```

---

## 📁 Project Structure

```
xrt/
├── lib/                    # 32 functional sub-libraries
│   ├── base.h             # Basic memory management
│   ├── charset.h          # Character set conversion (UTF-8/16/32)
│   ├── string.h           # String processing
│   ├── value.h            # Dynamic type system (16 types)
│   ├── json.h             # JSON SAX parsing/generation
│   ├── template.h         # Template engine
│   ├── dict.h             # Dictionary (AVL tree implementation)
│   ├── mempool.h          # General memory pool
│   └── ...                # Other modules
├── docs/                   # API documentation
│   ├── api-base.md
│   ├── api-value.md
│   ├── api-json.md
│   └── ...                # Module documentation
├── test/                   # Test files (31 modules)
│   ├── test_base.h
│   ├── test_value.h
│   ├── test_json.h
│   └── ...
├── release/                # Build output directory
│   ├── x64/
│   └── x86/
├── xrt.h                  # Main header file (API declarations, 2300+ lines)
├── xrt.c                  # Main implementation file (includes all lib/*.h)
├── test.c                 # Test entry point
├── build_*.bat            # Windows build scripts
├── build*.sh              # Linux/macOS build scripts
├── README.md              # Chinese documentation
└── README.en.md           # English documentation
```

---

## 🔧 Build Options

### Supported Compilers

- **TCC** (Tiny C Compiler): Ultra-fast compilation speed
- **GCC**: GNU Compiler Collection
- **Clang**: LLVM Compiler
- **MSVC**: Microsoft Visual C++

### Supported Platforms

- **Windows** (x86/x64)
- **Linux** (x86/x64)
- **macOS** (x64/ARM64)

### Build Targets

- **DLL**: Dynamic Link Library
- **OBJ**: Static Object Files
- **TEST**: Executable Test Programs

---

## 📚 API Documentation

### Memory Management

```c
ptr xrtMalloc(size_t iSize);                    // Allocate memory
ptr xrtCalloc(size_t iNum, size_t iSize);       // Allocate and zero
ptr xrtRealloc(ptr pMem, size_t iSize);         // Reallocate
void xrtFree(ptr pmem);                          // Free memory
ptr xrtTempMemory(size_t iSize);                // Temporary memory (32-slot ring auto-free)
```

### Character Set Conversion

```c
u16str xrtUTF8to16(u8str sText, size_t iSize);   // UTF-8 to UTF-16
u8str xrtUTF16to8(u16str sText, size_t iSize);   // UTF-16 to UTF-8
u32str xrtUTF8to32(u8str sText, size_t iSize);   // UTF-8 to UTF-32
ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);  // Any encoding conversion
bool xrtIsUTF8(str sText, size_t iSize);         // Check if UTF-8
int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);  // Auto-detect encoding
```

### String Operations

```c
str xrtCopyStr(str sText, size_t iSize);         // Copy string
str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
str xrtFormat(str sFormat, ...);                 // Format string
str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);  // Base64 encode
ptr xrtBase64Decode(str sText, size_t iSize, str sTable);  // Base64 decode
```

### File Operations

```c
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);  // Open file
void xrtClose(xfile objFile);                    // Close file
str xrtFileReadAll(str sPath, int iCharset);     // Read all content
int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
bool xrtFileExists(str sPath);                   // Check file existence
bool xrtDirExists(str sPath);                    // Check directory existence
int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);  // Scan directory
bool xrtDirCreateAll(str sPath);                 // Create multi-level directory
```

### Time Handling

```c
xtime xrtNow();                                   // Current date and time
xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);  // Build date
xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
str xrtTimeToStr(xtime iTime, int iFormat);      // Time to string
xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);  // Add/subtract time
int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);  // Time difference
int64 xrtYear(xtime iTime);                      // Get year
int xrtMonth(xtime iTime);                       // Get month
int xrtDay(xtime iTime);                         // Get day
```

### Value Dynamic Types

```c
// Create values
xvalue xvoCreateNull();                          // Null
xvalue xvoCreateBool(bool bVal);                 // Boolean
xvalue xvoCreateInt(int64 iVal);                 // Integer
xvalue xvoCreateFloat(double fVal);              // Float
xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);  // String
xvalue xvoCreateTime(xtime tVal);                // Time
xvalue xvoCreateArray();                         // Array
xvalue xvoCreateList();                          // List
xvalue xvoCreateColl();                          // Collection
xvalue xvoCreateTable();                         // Table (dictionary)

// Reference counting
void xvoAddRef(xvalue pVal);                     // Add reference
void xvoUnref(xvalue pVal);                      // Remove reference (auto-release at zero)

// Array operations
xvoArrayAppendInt(arr, value);                   // Append integer
xvoArrayAppendText(arr, str, len, colloc);       // Append string
xvalue xvoArrayGetValue(xvalue pArr, uint32 index);  // Get value
uint32 xvoArrayItemCount(xvalue pArr);           // Get count

// Table operations
xvoTableSetInt(table, key, keylen, value);       // Set integer
xvoTableSetText(table, key, keylen, str, len, colloc);  // Set string
str xvoTableGetText(xvalue pTbl, str key, uint32 kl);  // Get string
```

### JSON Processing

```c
xvalue xrtParseJSON(str sText, size_t iSize);    // Parse JSON string
xvalue xrtParseJSON_File(str sFile);             // Parse JSON file
str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);  // Generate JSON
int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);    // Save to file
```

---

## 🎓 Design Highlights

### 1. Ring Buffer Temporary Memory

32 temporary memory slots are used cyclically with automatic release to avoid memory leaks:

```c
str temp1 = xrtTempMemory(1024);  // Use slot 0
str temp2 = xrtTempMemory(2048);  // Use slot 1
// ... After cycling to slot 31, slot 0 is automatically freed
```

### 2. Reference Counting GC

Value type system uses 26-bit reference counting for automatic memory management:

```c
xvalue v = xvoCreateInt(123);  // RefCount = 1
xvoAddRef(v);                   // RefCount = 2
xvoUnref(v);                    // RefCount = 1
xvoUnref(v);                    // RefCount = 0, auto-release
```

### 3. Multi-Level Memory Pools

- **BSMM**: Block struct memory management, 256 elements per page
- **MemUnit**: 256-byte page management, supports GC mark-sweep
- **FSMemPool**: Fixed-size memory pool
- **MemPool**: General memory pool, supports multiple sizes, GC recycling

### 4. Distributed ID Generation

192-bit unique ID containing timestamp, IP address, CPU clock, and random number:

```c
str xid = xrtMakeXIDS();  // Generate globally unique ID
xrtFree(xid);
```

### 5. 16 Value Data Types

| Type | Description | Type | Description |
|------|-------------|------|-------------|
| Empty | Non-existent data | Null | Null value |
| Bool | Boolean | Int | 64-bit integer |
| Float | Double precision float | Text | String |
| Time | Timestamp | Point | Pointer |
| Func | Function pointer | Array | Array |
| List | List | Coll | Collection |
| Table | Table/Dictionary | Struct | Struct |
| Object | Object | Custom | Custom |

### 6. High-Performance Hashing

- **32-bit**: nmhash32x (BSD-2 license)
- **64-bit**: rapidhash (BSD-2 license), provides Micro/Nano variants

### 7. JSON SAX Mode

Event-driven SAX mode for parsing/generating JSON with high memory efficiency:

```c
// Parse: stream reading, parse with callbacks
xrtJsonParseSAX(jsonText, len, callback);

// Generate: incremental writing, smart expansion
json_sax_print_hd hd = xrtJsonPrintStart(&choice);
xrtJsonPrintObject(hd, NULL, JSON_SAX_START);
xrtJsonPrintString(hd, &key, &value);
xrtJsonPrintObject(hd, NULL, JSON_SAX_FINISH);
str result = xrtJsonPrintFinish(hd, &len, NULL);
```

---

## 🧪 Testing

The project includes 31 test modules covering all functionality:

```c
// Enable modules to test in test.c
Test_Base(xCore);        // Basic functionality test
Test_Charset(xCore);     // Character set test
Test_String(xCore);      // String test
Test_File(xCore);        // File test
Test_Value_Basic(xCore); // Value type test
Test_JSON(xCore);        // JSON test
Test_Template(xCore);    // Template engine test
Test_Dict(xCore);        // Dictionary test
Test_MemPool(xCore);     // Memory pool test
// ...
```

Run tests:

```batch
# Windows
build_TCC_TEST_x64.bat
cd release\x64
test.exe

# Linux/macOS
bash build_test.sh
./test
```

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).

```
Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
```

---

## 🤝 Contributing

Issues and Pull Requests are welcome!

1. Fork this repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 📧 Contact

- **Author**: xLeaves [xywhsoft]
- **Email**: xywhsoft@qq.com
- **Project**: [GitHub](https://github.com/yourusername/xrt)

---

## 🌟 Star History

If this project helps you, please give it a ⭐ Star to show your support!

---

<div align="center">

**Made with ❤️ by xLeaves**

</div>
