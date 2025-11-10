# XRT - X Runtime Library

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](https://github.com)
[![Language](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

**Lightweight, High-Performance C Runtime Library**

[English](README.en.md) | [简体中文](README.md)

</div>

---

## 📖 Project Overview

**xrt** (X Runtime Library) is a full-featured C runtime library providing 30+ functional modules including memory management, character set conversion, file handling, data structures, dynamic type system, JSON processing, and more. The library features a single-header design, zero external dependencies, and cross-platform compilation support, aiming to provide modern, efficient infrastructure support for C language developers.

### ✨ Core Features

- 🚀 **Zero Dependencies**: No external dependencies except the standard C library
- 📦 **Single Header**: All core APIs defined in `xrt.h`
- 🔧 **Modular Design**: 30+ independent sub-libraries, use as needed
- 🌍 **Cross-Platform**: Supports Windows, Linux, macOS
- 🔨 **Multi-Compiler**: Compatible with TCC, GCC, Clang, MSVC
- ⚡ **High Performance**: Built-in multi-level memory pools, reference counting, inline optimization
- 🎯 **Dynamic Types**: Script-like Value type system
- 💾 **Smart Memory**: Ring buffer temporary memory, automatic GC support

---

## 🎯 Main Functional Modules

### 🔹 Infrastructure Layer

| Module | Description | Main API |
|--------|-------------|----------|
| **base** | Memory management | `xrtMalloc`, `xrtFree`, `xrtTempMemory` |
| **charset** | Character set conversion | `xrtUTF8to16`, `xrtUTF16to8`, `xrtConvCharset` |
| **hash** | Hash computation | `xrtHash32`, `xrtHash64` |
| **math** | Mathematical operations | `xrtRand32`, `xrtRand64`, `xrtRandRange` |
| **time** | Time handling | `xrtNow`, `xrtDateSerial`, `xrtDateAdd` |

### 🔹 System Interaction Layer

| Module | Description | Main API |
|--------|-------------|----------|
| **os** | System operations | `xrtRun`, `xrtStart`, `xrtChain` |
| **file** | File operations | `xrtOpen`, `xrtFileReadAll`, `xrtDirScan` |
| **path** | Path handling | `xrtPathGetName`, `xrtPathJoin` |
| **network** | Network functions | `xrtGetLocalIP`, `xrtGetLocalMAC` |
| **thread** | Thread management | `xrtThreadCreate` |

### 🔹 String Processing Layer

| Module | Description | Main API |
|--------|-------------|----------|
| **string** | String operations | `xrtFindStr`, `xrtReplace`, `xrtSplit`, `xrtFormat` |
| **jnum** | Number conversion | `xrtI64ToStr`, `xrtParseNum` |
| **template** | Template engine | String template processing |

### 🔹 Data Structure Layer

| Module | Description | Data Structure Type |
|--------|-------------|-----------------|
| **buffer** | Dynamic buffer | `xbuffer` |
| **array** | Struct array | `xarray` |
| **array_point** | Pointer array | `xparray` |
| **stack** | Static stack | `xstack` |
| **stack_dyn** | Dynamic stack | `xdynstack` |
| **llist** | Doubly linked list | `xllist` |
| **avltree** | AVL balanced tree | `xavltree` |
| **dict** | Dictionary (key-value) | `xdict` |
| **list** | List (integer index) | `xlist` |

### 🔹 Memory Management Layer

| Module | Description | Use Case |
|--------|-------------|----------|
| **bsmm** | Block struct memory mgmt | Fixed-size struct allocation |
| **memunit** | Memory unit management | 256-byte page management |
| **mempool_fs** | Fixed-size memory pool | High-frequency small object allocation |
| **mempool** | General memory pool | Multi-level memory block management |

### 🔹 Advanced Features Layer

| Module | Description | Features |
|--------|-------------|----------|
| **value** | Dynamic type system | 15 data types, reference counting, auto-release |
| **json** | JSON processing | Parse, generate, serialize |
| **xid** | Distributed ID | 192-bit unique ID generator |

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
    str timeStr = xrtTimeToStr(now, 0);
    printf("Current time: %s\n", timeStr);
    xrtFree(timeStr);
    
    // Cleanup
    xrtUnit();
    return 0;
}
```

#### Dynamic Type System

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
    xvoTableSetText(table, "name", 4, "XRT", 0, FALSE);
    xvoTableSetInt(table, "version", 7, 1);
    
    // Get values
    str name = xvoTableGetText(table, "name", 4);
    printf("Name: %s\n", name);
    
    // Release resources
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
    str jsonText = "{\"name\":\"xrt\",\"version\":1.0}";
    xvalue json = xrtJSONParse(jsonText, 0);
    
    // Read values
    str name = xvoTableGetText(json, "name", 4);
    printf("Project: %s\n", name);
    
    // Generate JSON
    xvalue newJson = xvoCreateTable();
    xvoTableSetText(newJson, "status", 6, "ok", 0, FALSE);
    str output = xrtJSONStringify(newJson);
    printf("%s\n", output);
    
    xrtFree(output);
    xvoUnref(json);
    xvoUnref(newJson);
    xrtUnit();
    return 0;
}
```

---

## 📁 Project Structure

```
xrt/
├── lib/                    # 30+ functional sub-libraries
│   ├── base.h             # Basic memory management
│   ├── charset.h          # Character set conversion
│   ├── string.h           # String processing
│   ├── value.h            # Dynamic type system
│   ├── json.h             # JSON processing
│   └── ...                # Other modules
├── test/                   # Test files
│   ├── test_base.h
│   ├── test_value.h
│   └── ...
├── release/                # Build output directory
│   ├── x64/
│   └── x86/
├── xrt.h                  # Main header file (API declarations)
├── xrt.c                  # Main implementation file
├── test.c                 # Test entry point
├── build_*.bat            # Windows build scripts
├── build_test.sh          # Linux/macOS build script
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
ptr xrtCalloc(size_t iNum, size_t iSize);      // Allocate and zero
ptr xrtRealloc(ptr pMem, size_t iSize);        // Reallocate
void xrtFree(ptr pmem);                         // Free memory
ptr xrtTempMemory(size_t iSize);               // Temporary memory (auto-free)
```

### Character Set Conversion

```c
u16str xrtUTF8to16(u8str sText, size_t iSize);  // UTF-8 to UTF-16
u8str xrtUTF16to8(u16str sText, size_t iSize);  // UTF-16 to UTF-8
ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);
bool xrtIsUTF8(str sText, size_t iSize);        // Check if UTF-8
```

### String Operations

```c
str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
str xrtFormat(str sFormat, ...);                // Format string
```

### File Operations

```c
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);
str xrtFileReadAll(str sPath, int iCharset);    // Read all content
bool xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
bool xrtFileExists(str sPath);                  // Check file existence
int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
```

### Time Handling

```c
xtime xrtNow();                                  // Current date and time
xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
str xrtTimeToStr(xtime iTime, int iFormat);     // Time to string
xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
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

### 2. Reference Counting

Value type system uses reference counting for automatic memory management:

```c
xvalue v = xvoCreateInt(123);  // RefCount = 1
xvoAddRef(v);                   // RefCount = 2
xvoUnref(v);                    // RefCount = 1
xvoUnref(v);                    // RefCount = 0, auto-release
```

### 3. Multi-Level Memory Pools

- **MemUnit**: 256-byte page management
- **FSMemPool**: Fixed-size memory pool
- **MemPool**: General memory pool (supports multiple sizes)

### 4. Distributed ID Generation

192-bit unique ID containing timestamp, IP address, CPU tick, and random number:

```c
str xid = xrtMakeXIDS();  // Generate globally unique ID
```

---

## 🧪 Testing

The project includes 30+ test modules covering all functionality:

```c
// Enable modules to test in test.c
Test_Base(xCore);        // Basic functionality test
Test_String(xCore);      // String test
Test_Value_Basic(xCore); // Value type test
Test_JSON(xCore);        // JSON test
// ...
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
