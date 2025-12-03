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

**xrt**（X Runtime Library）是一个功能完备的 C 语言运行时库，提供了包括内存管理、字符集转换、文件处理、数据结构、动态类型系统、JSON 处理、模板引擎等在内的 **32 个功能模块**。该库采用单头文件设计，零外部依赖，支持跨平台编译，旨在为 C 语言开发者提供现代化、高效的基础设施支持。

### ✨ 核心特性

- 🚀 **零依赖**：除标准 C 库外无任何外部依赖
- 📦 **单头文件**：核心 API 统一定义在 `xrt.h` 中，2300+ 行完整声明
- 🔧 **模块化设计**：32 个独立子库，按需使用
- 🌍 **跨平台**：支持 Windows、Linux、macOS（x86/x64/ARM64）
- 🔨 **多编译器**：兼容 TCC、GCC、Clang、MSVC
- ⚡ **高性能**：内置多级内存池、引用计数、内联函数优化
- 🎯 **动态类型**：16 种数据类型的 Value 系统，支持引用计数自动释放
- 💾 **智能内存**：32 槽位环形临时内存、GC 标记回收支持
- 📝 **模板引擎**：支持变量替换、条件判断、循环迭代的模板系统

---

## 🎯 主要功能模块

### 🔹 基础设施层

| 模块 | 头文件 | 功能描述 | 主要 API |
|------|--------|---------|----------|
| **base** | `base.h` | 内存管理、错误处理 | `xrtMalloc`, `xrtFree`, `xrtTempMemory` |
| **charset** | `charset.h` | 字符集转换 | `xrtUTF8to16`, `xrtUTF16to8`, `xrtConvCharset` |
| **hash** | `hash.h` | 哈希计算 (nmhash32x/rapidhash) | `xrtHash32`, `xrtHash64`, `xrtHash64_Nano` |
| **math** | `math.h` | 随机数生成 (PCG) | `xrtRand32`, `xrtRand64`, `xrtRandRange` |
| **time** | `time.h` | 时间处理 | `xrtNow`, `xrtDateSerial`, `xrtDateAdd`, `xrtDateDiff` |

### 🔹 系统交互层

| 模块 | 头文件 | 功能描述 | 主要 API |
|------|--------|---------|----------|
| **os** | `os.h` | 系统操作 | `xrtRun`, `xrtStart`, `xrtChain` |
| **file** | `file.h` | 文件操作 | `xrtOpen`, `xrtFileReadAll`, `xrtDirScan` |
| **path** | `path.h` | 路径处理 | `xrtPathGetName`, `xrtPathGetDir`, `xrtPathJoin` |
| **network** | `network.h` | 网络功能 | `xrtGetLocalIP`, `xrtGetLocalMAC`, `xrtGetLocalName` |
| **thread** | `thread.h` | 线程管理 | `xrtThreadCreate` |

### 🔹 字符串处理层

| 模块 | 头文件 | 功能描述 | 主要 API |
|------|--------|---------|----------|
| **string** | `string.h` | 字符串操作 | `xrtFindStr`, `xrtReplace`, `xrtSplit`, `xrtFormat` |
| **jnum** | `jnum.h` | 数字转换 | `xrtI64ToStr`, `xrtParseNum`, `xrtStrToNum` |
| **template** | `template.h` | 模板引擎 | `xteParse`, `xteMake`, `xteLexer` |

### 🔹 数据结构层

| 模块 | 头文件 | 功能描述 | 数据结构类型 |
|------|--------|---------|-------------|
| **buffer** | `buffer.h` | 动态缓冲区 | `xbuffer` |
| **array** | `array.h` | 结构体数组 | `xarray` |
| **array_point** | `array_point.h` | 指针数组 | `xparray` |
| **stack** | `stack.h` | 静态栈 | `xstack` |
| **stack_dyn** | `stack_dyn.h` | 动态栈 (256步进扩容) | `xdynstack` |
| **llist** | `llist.h` | 双向链表 | `xllist` |
| **avltree** | `avltree.h` | AVL 平衡树 | `xavltree` |
| **dict** | `dict.h` | 字典（字符串键值对） | `xdict` |
| **list** | `list.h` | 列表（整数索引） | `xlist` |

### 🔹 内存管理层

| 模块 | 头文件 | 功能描述 | 适用场景 |
|------|--------|---------|----------|
| **bsmm** | `bsmm.h` | 块结构内存管理 | 固定大小结构体分配 |
| **memunit** | `memunit.h` | 内存单元管理 | 256 字节页管理，支持 GC 标记 |
| **mempool_fs** | `mempool_fs.h` | 固定大小内存池 | 高频小对象分配 |
| **mempool** | `mempool.h` | 通用内存池 | 多级内存块管理，支持 GC |

### 🔹 高级功能层

| 模块 | 头文件 | 功能描述 | 特性 |
|------|--------|---------|------|
| **value** | `value.h` | 动态类型系统 | 16 种数据类型，26位引用计数，自动释放 |
| **json** | `json.h` | JSON 处理 | SAX 解析/生成，支持格式化输出 |
| **xid** | `xid.h` | 分布式 ID | 192 位唯一 ID（时间戳+IP+时钟+随机数） |

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
    str timeStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("当前时间: %s\n", timeStr);
    xrtFree(timeStr);
    
    // 清理资源
    xrtUnit();
    return 0;
}
```

#### 动态类型系统（Value）

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
    xvoTableSetText(table, "name", 0, "XRT", 0, FALSE);
    xvoTableSetInt(table, "version", 0, 1);
    xvoTableSetBool(table, "active", 0, TRUE);
    
    // 获取值
    str name = xvoTableGetText(table, "name", 0);
    printf("Name: %s\n", name);
    
    // 释放资源（引用计数自动管理）
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
    str jsonText = "{\"name\":\"xrt\",\"version\":1.0,\"features\":[\"fast\",\"simple\"]}";
    xvalue json = xrtParseJSON(jsonText, 0);
    
    // 读取值
    str name = xvoTableGetText(json, "name", 0);
    printf("Project: %s\n", name);
    
    // 生成 JSON（格式化输出）
    xvalue newJson = xvoCreateTable();
    xvoTableSetText(newJson, "status", 0, "ok", 0, FALSE);
    xvoTableSetInt(newJson, "code", 0, 200);
    
    size_t len = 0;
    str output = xrtStringifyJSON(newJson, TRUE, &len);  // TRUE = 格式化
    printf("%s\n", output);
    xrtFree(output);
    
    xvoUnref(json);
    xvoUnref(newJson);
    xrtUnit();
    return 0;
}
```

#### 模板引擎

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // 创建模板变量
    xvalue vars = xvoCreateTable();
    xvoTableSetText(vars, "title", 0, "XRT Library", 0, FALSE);
    xvoTableSetInt(vars, "count", 0, 42);
    
    // 解析模板
    str template = "Welcome to {$title}! Count: {%count}";
    XTE_LiteObject tpl = xteParse(template, 0, NULL);
    
    // 生成输出
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

## 📁 项目结构

```
xrt/
├── lib/                    # 32 个功能子库
│   ├── base.h             # 基础内存管理
│   ├── charset.h          # 字符集转换 (UTF-8/16/32)
│   ├── string.h           # 字符串处理
│   ├── value.h            # 动态类型系统 (16种类型)
│   ├── json.h             # JSON SAX 解析/生成
│   ├── template.h         # 模板引擎
│   ├── dict.h             # 字典 (AVL树实现)
│   ├── mempool.h          # 通用内存池
│   └── ...                # 其他模块
├── docs/                   # API 文档
│   ├── api-base.md
│   ├── api-value.md
│   ├── api-json.md
│   └── ...                # 各模块文档
├── test/                   # 测试文件 (31个模块)
│   ├── test_base.h
│   ├── test_value.h
│   ├── test_json.h
│   └── ...
├── release/                # 编译输出目录
│   ├── x64/
│   └── x86/
├── xrt.h                  # 主头文件（API 声明，2300+ 行）
├── xrt.c                  # 主实现文件（包含所有 lib/*.h）
├── test.c                 # 测试入口
├── build_*.bat            # Windows 构建脚本
├── build*.sh              # Linux/macOS 构建脚本
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
ptr xrtCalloc(size_t iNum, size_t iSize);       // 分配并清零
ptr xrtRealloc(ptr pMem, size_t iSize);         // 重新分配
void xrtFree(ptr pmem);                          // 释放内存
ptr xrtTempMemory(size_t iSize);                // 临时内存（32槽位环形自动释放）
```

### 字符集转换

```c
u16str xrtUTF8to16(u8str sText, size_t iSize);   // UTF-8 转 UTF-16
u8str xrtUTF16to8(u16str sText, size_t iSize);   // UTF-16 转 UTF-8
u32str xrtUTF8to32(u8str sText, size_t iSize);   // UTF-8 转 UTF-32
ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);  // 任意编码转换
bool xrtIsUTF8(str sText, size_t iSize);         // 判断是否 UTF-8
int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);  // 自动检测编码
```

### 字符串操作

```c
str xrtCopyStr(str sText, size_t iSize);         // 复制字符串
str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
str xrtFormat(str sFormat, ...);                 // 格式化字符串
str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);  // Base64 编码
ptr xrtBase64Decode(str sText, size_t iSize, str sTable);  // Base64 解码
```

### 文件操作

```c
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);  // 打开文件
void xrtClose(xfile objFile);                    // 关闭文件
str xrtFileReadAll(str sPath, int iCharset);     // 读取全部内容
int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
bool xrtFileExists(str sPath);                   // 判断文件是否存在
bool xrtDirExists(str sPath);                    // 判断目录是否存在
int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);  // 遍历目录
bool xrtDirCreateAll(str sPath);                 // 创建多级目录
```

### 时间处理

```c
xtime xrtNow();                                   // 当前日期时间
xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);  // 构建日期
xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
str xrtTimeToStr(xtime iTime, int iFormat);      // 时间转字符串
xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);  // 时间加减
int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);  // 时间差
int64 xrtYear(xtime iTime);                      // 获取年份
int xrtMonth(xtime iTime);                       // 获取月份
int xrtDay(xtime iTime);                         // 获取日期
```

### Value 动态类型

```c
// 创建值
xvalue xvoCreateNull();                          // 空值
xvalue xvoCreateBool(bool bVal);                 // 布尔值
xvalue xvoCreateInt(int64 iVal);                 // 整数
xvalue xvoCreateFloat(double fVal);              // 浮点数
xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);  // 字符串
xvalue xvoCreateTime(xtime tVal);                // 时间
xvalue xvoCreateArray();                         // 数组
xvalue xvoCreateList();                          // 列表
xvalue xvoCreateColl();                          // 集合
xvalue xvoCreateTable();                         // 表（字典）

// 引用计数
void xvoAddRef(xvalue pVal);                     // 增加引用
void xvoUnref(xvalue pVal);                      // 减少引用（归零自动释放）

// 数组操作
xvoArrayAppendInt(arr, value);                   // 追加整数
xvoArrayAppendText(arr, str, len, colloc);       // 追加字符串
xvalue xvoArrayGetValue(xvalue pArr, uint32 index);  // 获取值
uint32 xvoArrayItemCount(xvalue pArr);           // 获取数量

// 表操作
xvoTableSetInt(table, key, keylen, value);       // 设置整数
xvoTableSetText(table, key, keylen, str, len, colloc);  // 设置字符串
str xvoTableGetText(xvalue pTbl, str key, uint32 kl);  // 获取字符串
```

### JSON 处理

```c
xvalue xrtParseJSON(str sText, size_t iSize);    // 解析 JSON 字符串
xvalue xrtParseJSON_File(str sFile);             // 解析 JSON 文件
str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);  // 生成 JSON
int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);    // 保存到文件
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

### 2. 引用计数 GC

Value 类型系统采用 26 位引用计数自动管理内存：

```c
xvalue v = xvoCreateInt(123);  // RefCount = 1
xvoAddRef(v);                   // RefCount = 2
xvoUnref(v);                    // RefCount = 1
xvoUnref(v);                    // RefCount = 0，自动释放
```

### 3. 多级内存池

- **BSMM**：块结构内存管理，256 元素一页
- **MemUnit**：256 字节页管理，支持 GC 标记回收
- **FSMemPool**：固定大小内存池
- **MemPool**：通用内存池，支持多种大小、GC 回收

### 4. 分布式 ID 生成

192 位唯一 ID，包含时间戳、IP 地址、CPU 时钟、随机数：

```c
str xid = xrtMakeXIDS();  // 生成全局唯一 ID
xrtFree(xid);
```

### 5. 16 种 Value 数据类型

| 类型 | 说明 | 类型 | 说明 |
|------|------|------|------|
| Empty | 不存在的数据 | Null | 空值 |
| Bool | 布尔值 | Int | 64位整数 |
| Float | 双精度浮点 | Text | 字符串 |
| Time | 时间戳 | Point | 指针 |
| Func | 函数指针 | Array | 数组 |
| List | 列表 | Coll | 集合 |
| Table | 表/字典 | Struct | 结构体 |
| Object | 对象 | Custom | 自定义 |

### 6. 高性能哈希

- **32位**：nmhash32x（BSD-2 协议）
- **64位**：rapidhash（BSD-2 协议），提供 Micro/Nano 变体

### 7. JSON SAX 模式

采用事件驱动的 SAX 模式解析/生成 JSON，内存效率高：

```c
// 解析：流式读取，边解析边回调
xrtJsonParseSAX(jsonText, len, callback);

// 生成：增量式写入，智能扩容
json_sax_print_hd hd = xrtJsonPrintStart(&choice);
xrtJsonPrintObject(hd, NULL, JSON_SAX_START);
xrtJsonPrintString(hd, &key, &value);
xrtJsonPrintObject(hd, NULL, JSON_SAX_FINISH);
str result = xrtJsonPrintFinish(hd, &len, NULL);
```

---

## 🧪 测试

项目包含 31 个测试模块，覆盖所有功能：

```c
// 在 test.c 中启用需要测试的模块
Test_Base(xCore);        // 基础功能测试
Test_Charset(xCore);     // 字符集测试
Test_String(xCore);      // 字符串测试
Test_File(xCore);        // 文件测试
Test_Value_Basic(xCore); // Value 类型测试
Test_JSON(xCore);        // JSON 测试
Test_Template(xCore);    // 模板引擎测试
Test_Dict(xCore);        // 字典测试
Test_MemPool(xCore);     // 内存池测试
// ...
```

运行测试：

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
