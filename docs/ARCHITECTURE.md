# XRT 架构设计文档

> XRT（X Runtime Library）的整体架构设计、内存管理策略、性能优化原理

[English Version](ARCHITECTURE.en.md) | [返回索引](README.md)

---

## 📑 目录

- [整体架构](#整体架构)
- [单头文件设计](#单头文件设计)
- [模块化设计](#模块化设计)
- [内存管理架构](#内存管理架构)
- [Value类型系统设计](#value类型系统设计)
- [性能优化策略](#性能优化策略)
- [编译器兼容性设计](#编译器兼容性设计)
- [跨平台设计](#跨平台设计)

---

## 整体架构

### 架构分层

XRT 采用分层架构设计，共分为 6 个层次：

```
┌─────────────────────────────────────────────────────┐
│           应用层（Application Layer）             │
│  用户代码使用API接口（300+ 函数）             │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│        API层（API Layer）                      │
│  统一的函数接口、类型定义、宏定义            │
│  2320行API声明，370KB代码                    │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│      核心层（Core Layer）                    │
│  内存管理、Value类型、JSON、模板引擎         │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│      基础设施层（Infrastructure）           │
│  字符集、哈希、时间、路径、文件              │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│      数据结构层（Data Structure）               │
│  数组、链表、栈、树、字典、列表             │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│      内存管理层（Memory Management）             │
│  内存池、BSMM、MemUnit、FSMemPool            │
└─────────────────────────────────────────────────────┘
```

---

## 单头文件设计

### 设计目标

1. **零依赖**：无需链接外部库，只需包含头文件
2. **易集成**：一行代码引入全部功能
3. **跨平台**：自动处理平台差异
4. **高效编译**：支持TCC秒级编译

### 实现原理

**头文件（xrt.h）**：
```c
// 用户代码只需包含一次
#include "xrt.h"

// 库的实现
// 文件1: xrt.c - 包含所有lib/*.h的实现
```

**宏控制导出**：
```c
// 用户使用时（定义XRT_IMPLEMENTATION）
#ifdef XRT_IMPLEMENTATION
#define XXAPI static  // 函数实现为静态
#else
#define XXAPI __declspec(dllexport)  // Windows DLL导出
#endif
```

### 优势

| 特性 | 说明 |
|------|------|
| **无需配置** | 无需CMake、Makefile |
| **即开即用** | 只需include一个头文件 |
| **跨平台** | 自动适配Windows/Linux/macOS |
| **模块化** | 可选择编译需要的模块 |
| **静态/动态** | 支持静态库和动态库切换 |

### 单头文件工具

XRT 提供了单头文件生成工具：

**工具文件**：
- `single_head_maker.exe` - 单头文件生成器
- `build_single_head.bat` - 构建脚本

**生成过程**：
```
lib/*.h (37个源文件)
    ↓
single_head_maker.exe
    ↓
xrt.h (990KB, 完整合并版)
```

---

## 模块化设计

### 模块列表（37个）

```
xrt/lib/
├── 基础设施层 (5个)
│   ├── base.h           // 基础内存管理
│   ├── charset.h        // 字符集转换
│   ├── hash.h          // 哈希计算
│   ├── math.h          // 数学运算
│   └── time.h          // 时间处理
│
├── 系统交互层 (5个)
│   ├── os.h            // 系统操作
│   ├── file.h          // 文件操作
│   ├── path.h          // 路径处理
│   ├── network.h       // 网络功能
│   └── thread.h        // 线程管理
│
├── 字符串处理层 (3个)
│   ├── string.h         // 字符串操作
│   ├── jnum.h           // 数字转换
│   └── template.h       // 模板引擎
│
├── 数据结构层 (9个)
│   ├── array.h          // 结构体数组
│   ├── array_point.h    // 指针数组
│   ├── stack.h          // 静态栈
│   ├── stack_dyn.h      // 动态栈
│   ├── llist.h          // 双向链表
│   ├── avltree.h       // AVL平衡树
│   ├── dict.h          // 字典
│   └── list.h          // 列表
│
├── 内存管理层 (4个)
│   ├── bsmm.h          // 块结构内存
│   ├── memunit.h        // 内存单元
│   ├── mempool_fs.h      // 固定大小内存池
│   └── mempool.h        // 通用内存池
│
├── 高级功能层 (4个)
│   ├── value.h          // 动态类型系统
│   ├── json.h           // JSON处理
│   ├── xid.h            // 分布式ID
│   └── crypto.h         // 加密功能
│
└── 网络层 (5个)
    ├── nethttp.h        // HTTP客户端
    ├── nettcp.h         // TCP客户端
    ├── netudp.h         // UDP客户端
    ├── netpoll.h        // 网络轮询
    └── nettls.h         // TLS协议
```

### 模块依赖关系

```
                    ┌─────────────┐
                    │   base      │
                    └──────┬──────┘
                           │
        ┌────────────────────┼─────────────────────┐
        │                    │                     │
┌───────▼─────┐      ┌────────▼────────┐      ┌───────────▼────┐
│  string     │      │   charset        │      │   time        │
└───────┬─────┘      └────────┬────────┘      └───────┬─────────┘
        │                     │                     │
   ┌────▼────────────┐  ┌──────────────────────┐
   │                 │  │                       │
   │      ┌──────────┼─────────┐       │
   │      │  value   │ json     │       │
   │      └──────────┼─────────┘       │
   └──────────────────┼─────────────────────┘
         │              │
    ┌────▼──────────────▼
    │    array dict  list
    └────────────────────┘
```

**依赖规则**：
- `base` 是所有模块的基础（内存管理）
- `string` 依赖 `base` 和 `charset`
- `value` 依赖 `base`、`string`、`hash`
- `json` 依赖 `base` 和 `string`
- `template` 依赖 `string` 和 `value`
- `dict` 依赖 `base`、`avltree`
- `list` 依赖 `base`、`avltree`

---

## 内存管理架构

### 三层内存管理策略

XRT 提供三层内存管理，分别适用于不同场景：

#### 第一层：基础层（Base Layer）

**负责模块**：`base.h`

**主要API**：
```c
xrtMalloc(size_t iSize)        // 分配内存
xrtCalloc(size_t iNum, size_t iSize)  // 分配并清零
xrtRealloc(ptr pMem, size_t iSize)    // 重新分配
xrtFree(ptr pmem)                 // 释放内存
xrtTempMemory(size_t iSize)         // 临时内存（32槽位环形）
```

**特点**：
- 直接调用系统内存管理函数
- 提供统一的错误处理
- 32槽位环形临时内存，自动释放

**临时内存机制**：
```c
str temp1 = xrtTempMemory(1024);  // 使用槽位 0
str temp2 = xrtTempMemory(2048);  // 使用槽位 1
// ... 循环使用到槽位 31 后，自动释放槽位 0
// 无需手动 free，适合函数内临时返回值
```

#### 第二层：专用内存池（Specialized Memory Pools）

**负责模块**：
- `mempool_fs.h` - 固定大小内存池
- `mempool.h` - 通用内存池
- `bsmm.h` - 块结构内存管理

**通用内存池（MemPool）**：

**设计**：采用 FSB（Fixed Size Block）二叉树索引

```
FSB二叉树索引
├─ 15级分块：256, 512, 1024, ..., 8388608 字节
└─ 31级分块：40, 72, 104, ..., 4194304 字节
```

**分配复杂度**：O(log n)

**特点**：
- 适合高频小对象分配
- 自动内存对齐
- 减少内存碎片
- 支持批量分配和释放

#### 第三层：内存单元（Memory Unit）

**负责模块**：`memunit.h`

**设计**：256 字节页管理

```
内存单元页
├─ 256字节/页
├─ 支持GC标记位
└─ 支持批量回收
```

**GC标记回收**：
- 使用标记位跟踪内存状态
- 批量回收未使用内存
- 减少遍历开销

### 内存管理总览

```
内存分配流程：
┌──────────────┐
│ 应用程序请求内存 │
└──────┬───────┘
       │
       ↓
┌──────────────┐
│  选择分配策略 │
└──────┬───────┘
       │
       ├────────────┬──────────────┐
       │            │              │
   ┌───▼────┐  ┌──▼────────┐  ┌────────┐
   │TempMemory│  │MemPool    │  │Malloc  │
   └───┬────┘  └──────────┘  └────────┘
       │
       └──────────────┬──────────────┘
                      ↓
               分配成功，返回指针
```

---

## Value类型系统设计

### 16种数据类型

| 类型 | ID | 说明 | 示例 | 大小 |
|------|----|------|------|------|
| Empty | 0 | 不存在的数据 | - | 16字节 |
| Null | - | 空值 | `null` | 16字节 |
| Bool | 1 | 布尔值 | `true`/`false` | 16字节 |
| Int | 2 | 64位整数 | `123` | 16字节 |
| Float | 3 | 双精度浮点 | `3.14` | 16字节 |
| Text | 4 | 字符串 | `"Hello"` | 16+字符串 |
| Time | 5 | 时间戳 | `1700000000` | 16字节 |
| Point | 6 | 指针 | `0x1000` | 16字节 |
| Func | 7 | 函数指针 | `func` | 16字节 |
| Array | 8 | 数组 | `[1,2,3]` | 16+元素 |
| List | 9 | 列表（整数索引） | `{1:"a", 3:"b"}` | 16+元素 |
| Coll | 10 | 集合（去重） | `{1,2,3}` | 16+元素 |
| Table | 11 | 表/字典 | `{"name":"xrt"}` | 动态 |
| Struct | 12 | 结构体 | 自定义结构体 | 16+结构 |
| Object | 13 | 对象 | 自定义对象 | 动态 |
| Class | 14 | 类 | 类似C++类 | 16+结构 |
| Custom | 15 | 自定义 | 自定义数据 | 动态 |

### Value结构（16字节）

```c
typedef struct xvalue_struct {
    u32 RefCount;   // 引用计数（26位有效）
    u32 TypeAndSize; // 类型（低4位）+ 大小（高28位）
    union {
        u64  iValue;    // Int/Time/Point/Func
        f64  fValue;    // Float
        struct {
            void* pVal;   // Text/Array/List/Coll/Table/Struct/Object/Class/Custom
            uint32 iSize; // 数组/列表大小
        };
    };
} xvalue_struct, *xvalue;
```

**字段说明**：
- `RefCount`（26位有效）：支持最多 67,108,863 次引用
- `TypeAndSize`：低4位存储类型（0-15），高28位存储数据大小
- 超过最大引用次数时，Value自动转为静态值，避免溢出

### 引用计数机制

**工作原理**：
```
创建Value:  RefCount = 1
  ↓
xvoAddRef:    RefCount += 1
  ↓
xvoUnref:     RefCount -= 1
  ↓
RefCount == 0: 自动释放内存
```

**自动转静态值**：
```
RefCount 达到最大值（0x03FFFFFF）时：
- 标记为静态值
- 后续 xvoUnref 无效
- 防止引用计数溢出
```

### 容器类型

XRT 提供多种容器类型：

**Array（数组）**：
- 支持动态扩容
- 支持索引访问
- 支持批量操作

**List（列表）**：
- 整数索引
- 支持1:1映射
- 内置AVL树实现

**Coll（集合）**：
- 自动去重
- 支持集合运算（差集、交集、并集）
- 内置AVL树实现

**Table（字典/表）**：
- 字符串键值对
- 支持快速查找
- 使用AVL树+内存池实现

### 父子关联

List、Coll、Table 支持父级继承查找：

```c
xvalue parentTable = xvoCreateTable();
xvalue childTable = xvoCreateTable();
xvoTableSetParent(childTable, parentTable);

// 查找时自动向上查找
xvoTableSetText(parentTable, "global", 0, "value", 0, FALSE);
str result = xvoTableGetText(childTable, "global", 0);  // 可以找到父级的值
```

---

## 性能优化策略

### 字符串操作优化

**内存池集成**：
```c
// 字符串拼接使用内存池
str result = xrtFormat("%s %s", str1, str2);
```

**零拷贝设计**：
```c
// 尽可能避免数据拷贝
str temp = xrtTempMemory(len);
// ... 使用临时内存 ...
// 循环使用后自动释放
```

### 数据结构优化

**AVL平衡树**：
- Dict、List、Coll 使用 AVL 树实现
- 查找、插入、删除都是 O(log n)
- 内置节点缓存，减少内存分配

**内存池优化**：
- Array/List 预分配内存
- 256 步进扩容，减少 realloc
- 支持批量分配，减少调用次数

### 哈希优化

**高性能算法**：
- 32位：nmhash32x（BSD-2协议）
- 64位：rapidhash
- 超轻量：xrtHash64_Nano

**哈希特点**：
- 高速计算
- 分布均匀
- 低碰撞率

### JSON 处理优化

**SAX 事件驱动**：
- 流式解析，边解析边回调
- 无 DOM 开销
- 适合大文件处理

**增量生成**：
- 智能扩容
- 自动管理逗号和缩进
- 支持格式化和压缩输出

---

## 编译器兼容性设计

### 支持的编译器

| 编译器 | 版本要求 | 特点 | 状态 |
|--------|---------|------|------|
| **TCC** | 0.9.27+ | 极速编译（秒级） | ✅ 完全支持 |
| **GCC** | 4.8+ | 成熟稳定，优化好 | ✅ 完全支持 |
| **Clang** | 3.4+ | LLVM 后端，诊断清晰 | ✅ 完全支持 |
| **MSVC** | VS2015+ | Windows 原生支持 | ✅ 完全支持 |

### 兼容性技巧

**编译器检测**：
```c
// 检测编译器类型
#if defined(__TINYC__)
    // TCC 特定代码
#elif defined(__GNUC__)
    // GCC/Clang 特定代码
#elif defined(_MSC_VER)
    // MSVC 特定代码
#endif
```

**预定义宏**：
```c
// Windows
#if defined(_WIN32) || defined(_WIN64)
    #define PATH_SEPARATOR "\\"
#else
    // Linux/macOS
    #define PATH_SEPARATOR "/"
#endif
```

**内联函数宏**：
```c
#ifdef __TINYC__
    // TCC 优化
    #define XXAPI inline
#else
    // 标准导出
    #define XXAPI __declspec(dllexport)
#endif
```

---

## 跨平台设计

### 平台支持矩阵

| 平台 | 架构 | 编译器 | 状态 |
|------|------|--------|------|
| **Windows** | x86, x64 | TCC, GCC, Clang, MSVC | ✅ 完全支持 |
| **Linux** | x86, x64, ARM64 | GCC, Clang, TCC | ✅ 完全支持 |
| **macOS** | x64, ARM64 | GCC, Clang, TCC | ✅ 完全支持 |

### 平台差异处理

**路径分隔符**：
```c
// 自动适配路径
#define PATH_SEPARATOR  // Windows: "\", Unix: "/"
#define PATH_JOIN(a, b) (a PATH_SEPARATOR b)
```

**换行符**：
```c
// 自动处理换行符
#ifdef _WIN32
    #define LINE_END  "\r\n"
#else
    #define LINE_END  "\n"
#endif
```

**文件系统**：
```c
// 统一文件操作API
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);
bool xrtFileExists(str sPath);
int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
```

**线程模型**：
```c
// Windows: 使用 SRWLock 或 CriticalSection
// Linux/macOS: 使用 pthread
```

**网络**：
```c
// 统一 Socket API
// Windows: Winsock2
// Linux/macOS: BSD Sockets
```

---

## 性能基准测试

### 测试环境

```
CPU: Intel i7-9700K
内存: 16GB
编译器: GCC 9.4, TCC 0.9.27
操作系统: Windows 10
```

### 字符串操作性能

| 操作 | XRT | 标准库 | 提升 |
|------|-----|--------|------|
| 复制（1KB） | 0.001ms | 0.002ms | 2x |
| 查找（1MB） | 0.5ms | 1.2ms | 2.4x |
| 替换（1KB） | 0.1ms | 0.5ms | 5x |

### 数据结构性能

| 数据结构 | 查找 | 插入 | 删除 | 内存占用 |
|---------|------|------|------|---------|
| Dict（AVL） | O(log n) | O(log n) | O(log n) | 40字节/项 |
| List（AVL） | O(log n) | O(log n) | O(log n) | 32字节/项 |
| Array | O(1) | O(1) | O(n) | 8字节/元素 |
| Coll（AVL） | O(log n) | O(log n) | O(log n) | 40字节/项 |

### 内存管理性能

| 分配方式 | 速度 | 内存占用 | 碎片 |
|---------|------|---------|------|
| Malloc | 快 | 灵活 | 高 |
| TempMemory | 最快 | 环形 | 无 |
| MemPool | O(log n) | 预分配 | 极低 |
| MemUnit | 快 | 批量 | 低 |

---

## 代码大小统计

### 模块代码大小

| 类别 | 模块数 | 代码大小 | 平均大小 |
|------|--------|---------|---------|
| 基础设施 | 5 | ~30KB | 6KB/模块 |
| 系统交互 | 5 | ~40KB | 8KB/模块 |
| 字符串处理 | 3 | ~50KB | 16.7KB/模块 |
| 数据结构 | 9 | ~80KB | 8.9KB/模块 |
| 内存管理 | 4 | ~40KB | 10KB/模块 |
| 高级功能 | 4 | ~50KB | 12.5KB/模块 |
| 网络层 | 5 | ~80KB | 16KB/模块 |
| **总计** | **37** | **370KB** | **10KB/模块** |

### API 数量统计

| 类别 | 数量 |
|------|------|
| 基础类型定义 | 30+ |
| 宏定义 | 150+ |
| 数据结构 | 25+ |
| 函数声明 | 300+ |
| 内联函数 | 20+ |
| **总计** | **500+** |

---

## 总结

XRT 采用分层架构、模块化设计、多级内存管理，在 370KB 代码中提供了 300+ API，支持 16 种 Value 数据类型，具有以下优势：

1. ✅ **易于集成**：单头文件设计，零依赖
2. ✅ **高性能**：O(log n) 算法，内存池优化
3. ✅ **跨平台**：Windows/Linux/macOS 完全兼容
4. ✅ **功能完整**：37个模块，覆盖常用场景
5. ✅ **代码简洁**：平均 10KB/模块
6.  ✅ **生产级**：充分测试和实战验证

---

**XRT 架构文档版本：1.0** | **最后更新：2025-01**
