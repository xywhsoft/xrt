# XRT 基础类型定义

> XRT 库的所有基础类型、宏定义和全局数据结构

[返回索引](README.md)

---

## 📑 目录

- [基础类型定义](#基础类型定义)
- [字符串类型](#字符串类型)
- [整数类型](#整数类型)
- [浮点数类型](#浮点数类型)
- [指针类型](#指针类型)
- [特殊类型](#特殊类型)
- [常量定义](#常量定义)
- [全局数据结构](#全局数据结构)

---

## 基础类型定义

### 字符串类型

| 类型 | 定义 | 说明 |
|------|------|------|
| `u8str` | `typedef unsigned char* u8str` | UTF-8 字符串指针 |
| `u16str` | `typedef unsigned short* u16str` | UTF-16 字符串指针 |
| `u32str` | `typedef unsigned int* u32str` | UTF-32 字符串指针 |
| `binary` | `typedef unsigned char* binary` | 二进制数据指针 |
| `str` | `typedef u8str str` | 默认字符串类型（UTF-8） |

**使用示例：**
```c
str text = "Hello World";
u16str wtext = xrtUTF8to16(text, 0);
xrtFree(wtext);
```

---

### 整数类型

#### 8位整数

| 类型 | 定义 | 范围 | 说明 |
|------|------|------|------|
| `i8` | `typedef char i8` | -128 ~ 127 | 有符号8位整数 |
| `int8` | `typedef char int8` | -128 ~ 127 | 有符号8位整数（别名） |
| `u8` | `typedef unsigned char u8` | 0 ~ 255 | 无符号8位整数 |
| `uint8` | `typedef unsigned char uint8` | 0 ~ 255 | 无符号8位整数（别名） |

#### 16位整数

| 类型 | 定义 | 范围 | 说明 |
|------|------|------|------|
| `i16` | `typedef short i16` | -32,768 ~ 32,767 | 有符号16位整数 |
| `int16` | `typedef short int16` | -32,768 ~ 32,767 | 有符号16位整数（别名） |
| `u16` | `typedef unsigned short u16` | 0 ~ 65,535 | 无符号16位整数 |
| `uint16` | `typedef unsigned short uint16` | 0 ~ 65,535 | 无符号16位整数（别名） |

#### 32位整数

| 类型 | 定义 | 范围 | 说明 |
|------|------|------|------|
| `i32` | `typedef int i32` | -2^31 ~ 2^31-1 | 有符号32位整数 |
| `int32` | `typedef int int32` | -2^31 ~ 2^31-1 | 有符号32位整数（别名） |
| `u32` | `typedef unsigned int u32` | 0 ~ 2^32-1 | 无符号32位整数 |
| `uint32` | `typedef unsigned int uint32` | 0 ~ 2^32-1 | 无符号32位整数（别名） |
| `uint` | `typedef unsigned int uint` | 0 ~ 2^32-1 | 无符号整数（别名） |

#### 64位整数

| 类型 | 定义 | 范围 | 说明 |
|------|------|------|------|
| `i64` | `typedef long long i64` | -2^63 ~ 2^63-1 | 有符号64位整数 |
| `int64` | `typedef long long int64` | -2^63 ~ 2^63-1 | 有符号64位整数（别名） |
| `u64` | `typedef unsigned long long u64` | 0 ~ 2^64-1 | 无符号64位整数 |
| `uint64` | `typedef unsigned long long uint64` | 0 ~ 2^64-1 | 无符号64位整数（别名） |

#### 平台相关整数

| 类型 | 定义 | 说明 |
|------|------|------|
| `ulong` | `typedef unsigned long ulong` | 自动适配32/64位的无符号长整数 |

---

### 浮点数类型

| 类型 | 定义 | 精度 | 说明 |
|------|------|------|------|
| `f32` | `typedef float f32` | 单精度 | 32位浮点数 |
| `float32` | `typedef float float32` | 单精度 | 32位浮点数（别名） |
| `f64` | `typedef double f64` | 双精度 | 64位浮点数 |
| `float64` | `typedef double float64` | 双精度 | 64位浮点数（别名） |

**使用示例：**
```c
f32 pi = 3.14159f;
f64 e = 2.718281828459045;
```

---

### 指针类型

| 类型 | 定义 | 说明 |
|------|------|------|
| `ptr` | `typedef void* ptr` | 通用指针类型 |
| `intptr` | `typedef intptr_t intptr` | 指针大小的有符号整数 |
| `uintptr` | `typedef uintptr_t uintptr` | 指针大小的无符号整数 |

**使用场景：**
```c
ptr data = xrtMalloc(1024);
intptr addr = (intptr)data;  // 指针转整数
xrtFree(data);
```

---

### 特殊类型

| 类型 | 定义 | 说明 |
|------|------|------|
| `curr` | `typedef long long curr` | 货币类型（64位整数） |
| `xtime` | `typedef int64 xtime` | 时间类型（64位整数，秒为单位） |

---

## 常量定义

### 布尔常量

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

**说明：**
- XRT 使用标准 C 的 `bool` 类型（需要 `<stdbool.h>`）
- `TRUE/FALSE` 用于兼容性
- `null` 作为空指针常量

### API 导出标记

```c
#ifdef BUILD_DLL
    #define XXAPI __declspec(dllexport)
#else
    #define XXAPI
#endif
```

**说明：**
- 编译 DLL 时定义 `BUILD_DLL` 宏
- `XXAPI` 标记所有导出函数

---

## 全局数据结构

### xrtGlobalData - 全局数据管理器

```c
typedef struct {
    // 初始化状态
    int bInit;                          // 是否已初始化
    
    // 全局常量数据
    str sNull;                          // 空字符串常量
    
    // 临时返回值存储
    str sRet;                           // 字符串返回值
    int64 iRet;                         // 整数返回值
    double nRet;                        // 浮点数返回值
    
    // 错误处理
    str LastError;                      // 最后错误信息
    int __pri_FreeError;                // 私有：错误信息是否需要释放
    void (*OnError)(str sError);        // 错误回调函数
    
    // 高精度时钟（仅Windows）
    #if defined(_WIN32) || defined(_WIN64)
        uint64 Frequency;               // 时钟频率
    #endif
    
    // 应用信息
    uint LocalAddr;                     // 本机IP地址（用于XID）
    str AppFile;                        // 应用程序完整路径
    str AppPath;                        // 应用程序目录
    
    // 环形临时内存（32个槽位循环使用）
    ptr TempMem[32];                    // 临时内存数组
    uint32 TempMemIdx;                  // 当前索引
    
    // 可自定义的内存函数
    ptr (*malloc)(size_t iSize);        // 内存分配函数
    ptr (*calloc)(size_t iNum, size_t iSize);  // 清零分配函数
    ptr (*realloc)(ptr pMem, size_t iSize);    // 重新分配函数
    void (*free)(ptr pMem);             // 释放函数
    
} xrtGlobalData;
```

### 全局实例

```c
XXAPI extern xrtGlobalData xCore;
```

**访问方式：**
```c
xrtGlobalData* core = xrtInit();  // 初始化并获取指针
// 或直接访问
xCore.LastError = xCore.sNull;
```

---

## 字段详解

### 初始化状态

| 字段 | 类型 | 说明 |
|------|------|------|
| `bInit` | `int` | 是否已初始化，TRUE表示已初始化 |

### 全局常量

| 字段 | 类型 | 说明 |
|------|------|------|
| `sNull` | `str` | 空字符串常量，用于比较和返回 |

### 临时返回值

某些函数使用全局返回值存储多个返回值：

| 字段 | 类型 | 说明 |
|------|------|------|
| `sRet` | `str` | 临时字符串返回值 |
| `iRet` | `int64` | 临时整数返回值 |
| `nRet` | `double` | 临时浮点数返回值 |

**使用示例：**
```c
str dir = xrtPathGetDir("C:\\test\\file.txt", 0);
size_t len = xCore.iRet;  // 获取路径长度
xrtFree(dir);
```

### 错误处理

| 字段 | 类型 | 说明 |
|------|------|------|
| `LastError` | `str` | 最后一次错误信息 |
| `OnError` | 函数指针 | 错误回调函数 |

**使用示例：**
```c
void MyErrorHandler(str sError) {
    fprintf(stderr, "Error: %s\n", sError);
}

xCore.OnError = MyErrorHandler;
```

### 应用信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `LocalAddr` | `uint` | 本机IP地址（用于生成XID） |
| `AppFile` | `str` | 当前程序完整路径 |
| `AppPath` | `str` | 当前程序所在目录 |

### 环形临时内存

XRT 提供32个临时内存槽位，循环使用：

| 字段 | 类型 | 说明 |
|------|------|------|
| `TempMem[32]` | `ptr` 数组 | 32个临时内存槽位 |
| `TempMemIdx` | `uint32` | 当前使用的槽位索引 |

**工作原理：**
1. 每次调用 `xrtTempMemory()` 使用下一个槽位
2. 如果槽位已有内存，先释放再分配
3. 循环到第32个后回到第1个
4. 无需手动释放，会自动管理

**使用示例：**
```c
// 这些临时内存会自动管理，无需 xrtFree
str temp1 = xrtTempMemory(100);
str temp2 = xrtTempMemory(200);
// ... 使用 temp1 和 temp2
// 无需释放，会在循环32次后自动释放
```

### 自定义内存函数

可以替换默认的内存分配函数：

| 字段 | 类型 | 说明 |
|------|------|------|
| `malloc` | 函数指针 | 内存分配函数 |
| `calloc` | 函数指针 | 清零分配函数 |
| `realloc` | 函数指针 | 重新分配函数 |
| `free` | 函数指针 | 释放函数 |

**自定义示例：**
```c
ptr MyMalloc(size_t size) {
    printf("Allocating %zu bytes\n", size);
    return malloc(size);
}

xCore.malloc = MyMalloc;  // 使用自定义分配器
```

---

## 平台检测宏

XRT 使用以下宏进行平台检测：

### 操作系统检测

```c
#if defined(_WIN32) || defined(_WIN64)
    // Windows 平台代码
#elif defined(__unix__) || defined(__unix) || defined(__linux__)
    // Linux/Unix 平台代码
#elif defined(__APPLE__) || defined(__MACH__)
    // macOS 平台代码
#elif defined(__ANDROID__)
    // Android 平台代码
#endif
```

### 编译器检测

```c
#if defined(__TINYC__)
    // TCC 编译器
#elif defined(__GNUC__)
    // GCC 编译器
#elif defined(__clang__)
    // Clang 编译器
#elif defined(_MSC_VER)
    // MSVC 编译器
#endif
```

### 架构检测

```c
#if defined(__x86_64__) || defined(_M_X64)
    // 64位架构
#elif defined(__i386__) || defined(_M_IX86)
    // 32位架构
#endif
```

---

## 最佳实践

### 1. 使用类型别名

```c
// ✅ 推荐
i64 value = 1234567890;
str text = "Hello";

// ❌ 不推荐（虽然可以）
long long value = 1234567890;
unsigned char* text = "Hello";
```

### 2. 检查初始化状态

```c
if (!xCore.bInit) {
    xrtInit();
}
```

### 3. 使用临时内存

```c
// 对于临时使用的小内存，优先使用 xrtTempMemory
str temp = xrtTempMemory(256);  // 自动管理
strcpy(temp, "temporary data");

// 对于需要长期持有的内存，使用 xrtMalloc
str permanent = xrtMalloc(1024);  // 需要 xrtFree
```

### 4. 错误处理

```c
void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
    // 可选：记录日志、发送通知等
}

xrtInit();
xCore.OnError = MyErrorHandler;
```

---

## 相关文档

- [Base 基础函数库](api-base.md) - 内存管理函数
- [Charset 字符集转换](api-charset.md) - 字符串类型转换
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#xrt-基础类型定义)

</div>
