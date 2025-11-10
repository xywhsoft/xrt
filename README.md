# XRT - X Runtime Library

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](https://github.com)
[![Language](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

**轻量级、高性能的 C 语言运行时库**

[English](README.en.md) | 简体中文

</div>

---

## 📖 项目简介

**xrt**（X Runtime Library）是一个功能完备的 C 语言运行时库，提供了包括内存管理、字符集转换、文件处理、数据结构、动态类型系统、JSON 处理等在内的 30+ 个功能模块。该库采用单头文件设计，零外部依赖，支持跨平台编译，旨在为 C 语言开发者提供现代化、高效的基础设施支持。

### ✨ 核心特性

- 🚀 **零依赖**：除标准 C 库外无任何外部依赖
- 📦 **单头文件**：核心 API 统一定义在 `xrt.h` 中
- 🔧 **模块化设计**：30+ 个独立子库，按需使用
- 🌍 **跨平台**：支持 Windows、Linux、macOS
- 🔨 **多编译器**：兼容 TCC、GCC、Clang、MSVC
- ⚡ **高性能**：内置多级内存池、引用计数、内联优化
- 🎯 **动态类型**：类似脚本语言的 Value 类型系统
- 💾 **智能内存**：环形临时内存、自动 GC 支持

---

## 🎯 主要功能模块

### 🔹 基础设施层

| 模块 | 功能描述 | 主要 API |
|------|---------|----------|
| **base** | 内存管理 | `xrtMalloc`, `xrtFree`, `xrtTempMemory` |
| **charset** | 字符集转换 | `xrtUTF8to16`, `xrtUTF16to8`, `xrtConvCharset` |
| **hash** | 哈希计算 | `xrtHash32`, `xrtHash64` |
| **math** | 数学运算 | `xrtRand32`, `xrtRand64`, `xrtRandRange` |
| **time** | 时间处理 | `xrtNow`, `xrtDateSerial`, `xrtDateAdd` |

### 🔹 系统交互层

| 模块 | 功能描述 | 主要 API |
|------|---------|----------|
| **os** | 系统操作 | `xrtRun`, `xrtStart`, `xrtChain` |
| **file** | 文件操作 | `xrtOpen`, `xrtFileReadAll`, `xrtDirScan` |
| **path** | 路径处理 | `xrtPathGetName`, `xrtPathJoin` |
| **network** | 网络功能 | `xrtGetLocalIP`, `xrtGetLocalMAC` |
| **thread** | 线程管理 | `xrtThreadCreate` |

### 🔹 字符串处理层

| 模块 | 功能描述 | 主要 API |
|------|---------|----------|
| **string** | 字符串操作 | `xrtFindStr`, `xrtReplace`, `xrtSplit`, `xrtFormat` |
| **jnum** | 数字转换 | `xrtI64ToStr`, `xrtParseNum` |
| **template** | 模板引擎 | 字符串模板处理 |

### 🔹 数据结构层

| 模块 | 功能描述 | 数据结构类型 |
|------|---------|-------------|
| **buffer** | 动态缓冲区 | `xbuffer` |
| **array** | 结构体数组 | `xarray` |
| **array_point** | 指针数组 | `xparray` |
| **stack** | 静态栈 | `xstack` |
| **stack_dyn** | 动态栈 | `xdynstack` |
| **llist** | 双向链表 | `xllist` |
| **avltree** | AVL 平衡树 | `xavltree` |
| **dict** | 字典（键值对） | `xdict` |
| **list** | 列表（整数索引） | `xlist` |

### 🔹 内存管理层

| 模块 | 功能描述 | 适用场景 |
|------|---------|----------|
| **bsmm** | 块结构内存管理 | 固定大小结构体分配 |
| **memunit** | 内存单元管理 | 256 字节页管理 |
| **mempool_fs** | 固定大小内存池 | 高频小对象分配 |
| **mempool** | 通用内存池 | 多级内存块管理 |

### 🔹 高级功能层

| 模块 | 功能描述 | 特性 |
|------|---------|------|
| **value** | 动态类型系统 | 15 种数据类型，引用计数，自动释放 |
| **json** | JSON 处理 | 解析、生成、序列化 |
| **xid** | 分布式 ID | 192 位唯一 ID 生成器 |

---

## 🚀 快速开始

### 📥 安装

```bash
# 克隆仓库
git clone https://github.com/yourusername/xrt.git
cd xrt
```

### 🔨 编译

#### Windows 平台

```batch
# 使用 TCC 编译 64 位测试程序
build_TCC_TEST_x64.bat

# 使用 GCC 编译 64 位 DLL
build_GCC_DLL_x64.bat
```

#### Linux/macOS 平台

```bash
# 编译测试程序
bash build_test.sh
```

### 📝 使用示例

#### 基础使用

```c
#include "xrt.h"

int main() {
    // 初始化库
    xrtGlobalData* xCore = xrtInit();
    
    // 使用字符串功能
    str text = xrtReplace("Hello World", 0, "World", 0, "XRT", 0);
    printf("%s\n", text);  // 输出: Hello XRT
    xrtFree(text);
    
    // 时间处理
    xtime now = xrtNow();
    str timeStr = xrtTimeToStr(now, 0);
    printf("当前时间: %s\n", timeStr);
    xrtFree(timeStr);
    
    // 清理资源
    xrtUnit();
    return 0;
}
```

#### 动态类型系统

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // 创建数组
    xvalue arr = xvoCreateArray();
    xvoArrayAppendInt(arr, 123);
    xvoArrayAppendText(arr, "Hello", 0, FALSE);
    xvoArrayAppendFloat(arr, 3.14);
    
    // 创建表（字典）
    xvalue table = xvoCreateTable();
    xvoTableSetText(table, "name", 4, "XRT", 0, FALSE);
    xvoTableSetInt(table, "version", 7, 1);
    
    // 获取值
    str name = xvoTableGetText(table, "name", 4);
    printf("Name: %s\n", name);
    
    // 释放资源
    xvoUnref(arr);
    xvoUnref(table);
    xrtUnit();
    return 0;
}
```

#### JSON 处理

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // 解析 JSON
    str jsonText = "{\"name\":\"xrt\",\"version\":1.0}";
    xvalue json = xrtJSONParse(jsonText, 0);
    
    // 读取值
    str name = xvoTableGetText(json, "name", 4);
    printf("Project: %s\n", name);
    
    // 生成 JSON
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

## 📁 项目结构

```
xrt/
├── lib/                    # 30+ 个功能子库
│   ├── base.h             # 基础内存管理
│   ├── charset.h          # 字符集转换
│   ├── string.h           # 字符串处理
│   ├── value.h            # 动态类型系统
│   ├── json.h             # JSON 处理
│   └── ...                # 其他模块
├── test/                   # 测试文件
│   ├── test_base.h
│   ├── test_value.h
│   └── ...
├── release/                # 编译输出目录
│   ├── x64/
│   └── x86/
├── xrt.h                  # 主头文件（API 声明）
├── xrt.c                  # 主实现文件
├── test.c                 # 测试入口
├── build_*.bat            # Windows 构建脚本
├── build_test.sh          # Linux/macOS 构建脚本
├── README.md              # 中文文档
└── README.en.md           # 英文文档
```

---

## 🔧 构建选项

### 支持的编译器

- **TCC**（Tiny C Compiler）：超快编译速度
- **GCC**：GNU 编译器集合
- **Clang**：LLVM 编译器
- **MSVC**：Microsoft Visual C++

### 支持的平台

- **Windows**（x86/x64）
- **Linux**（x86/x64）
- **macOS**（x64/ARM64）

### 构建目标

- **DLL**：动态链接库
- **OBJ**：静态目标文件
- **TEST**：可执行测试程序

---

## 📚 API 文档

### 内存管理

```c
ptr xrtMalloc(size_t iSize);                    // 分配内存
ptr xrtCalloc(size_t iNum, size_t iSize);      // 分配并清零
ptr xrtRealloc(ptr pMem, size_t iSize);        // 重新分配
void xrtFree(ptr pmem);                         // 释放内存
ptr xrtTempMemory(size_t iSize);               // 临时内存（自动释放）
```

### 字符集转换

```c
u16str xrtUTF8to16(u8str sText, size_t iSize);  // UTF-8 转 UTF-16
u8str xrtUTF16to8(u16str sText, size_t iSize);  // UTF-16 转 UTF-8
ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);
bool xrtIsUTF8(str sText, size_t iSize);        // 判断是否 UTF-8
```

### 字符串操作

```c
str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
str xrtFormat(str sFormat, ...);                // 格式化字符串
```

### 文件操作

```c
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);
str xrtFileReadAll(str sPath, int iCharset);    // 读取全部内容
bool xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
bool xrtFileExists(str sPath);                  // 判断文件是否存在
int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
```

### 时间处理

```c
xtime xrtNow();                                  // 当前日期时间
xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
str xrtTimeToStr(xtime iTime, int iFormat);     // 时间转字符串
xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
```

---

## 🎓 设计亮点

### 1. 环形临时内存

32 个临时内存槽位循环使用，自动释放，避免内存泄漏：

```c
str temp1 = xrtTempMemory(1024);  // 使用槽位 0
str temp2 = xrtTempMemory(2048);  // 使用槽位 1
// ... 循环使用到槽位 31 后，自动释放槽位 0
```

### 2. 引用计数

Value 类型系统采用引用计数自动管理内存：

```c
xvalue v = xvoCreateInt(123);  // RefCount = 1
xvoAddRef(v);                   // RefCount = 2
xvoUnref(v);                    // RefCount = 1
xvoUnref(v);                    // RefCount = 0，自动释放
```

### 3. 多级内存池

- **MemUnit**：256 字节页管理
- **FSMemPool**：固定大小内存池
- **MemPool**：通用内存池（支持多种大小）

### 4. 分布式 ID 生成

192 位唯一 ID，包含时间戳、IP 地址、CPU 时钟、随机数：

```c
str xid = xrtMakeXIDS();  // 生成全局唯一 ID
```

---

## 🧪 测试

项目包含 30+ 个测试模块，覆盖所有功能：

```c
// 在 test.c 中启用需要测试的模块
Test_Base(xCore);        // 基础功能测试
Test_String(xCore);      // 字符串测试
Test_Value_Basic(xCore); // Value 类型测试
Test_JSON(xCore);        // JSON 测试
// ...
```

---

## 📄 许可证

本项目采用 [MIT License](LICENSE) 开源协议。

```
Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
```

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 提交 Pull Request

---

## 📧 联系方式

- **作者**：xLeaves [xywhsoft]
- **邮箱**：xywhsoft@qq.com
- **项目主页**：[GitHub](https://github.com/yourusername/xrt)

---

## 🌟 Star History

如果这个项目对你有帮助，请给一个 ⭐ Star 支持一下！

---

<div align="center">

**Made with ❤️ by xLeaves**

</div>
