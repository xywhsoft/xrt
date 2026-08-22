# XRT 基础类型定义

> XRT 库的所有基础类型、宏定义和全局数据结构

[English](types.en.md) | [中文](types.md) | [返回索引](README.md)

---

## 📑 目录

- [标准库依赖](#标准库依赖)
- [保护宏定义](#保护宏定义)
- [基础类型定义](#基础类型定义)
- [字符串类型](#字符串类型)
- [整数类型](#整数类型)
- [浮点数类型](#浮点数类型)
- [指针类型](#指针类型)
- [特殊类型](#特殊类型)
- [常量定义](#常量定义)
- [运行时数据模型](#运行时数据模型)

---

## 标准库依赖

XRT 库依赖以下 C 标准库头文件：

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

| 头文件 | 用途 |
|--------|------|
| `stdio.h` | 标准输入输出 |
| `stdint.h` | 固定宽度整数类型 (`intptr_t`, `uintptr_t` 等) |
| `stdarg.h` | 可变参数支持 (`va_list` 等) |
| `stdlib.h` | 内存分配 (`malloc`, `free` 等) |
| `unistd.h` | POSIX 系统调用 |
| `string.h` | 字符串操作 (`memcpy`, `strlen` 等) |
| `ctype.h` | 字符分类 (`isalpha`, `isdigit` 等) |
| `wctype.h` | 宽字符分类 |
| `math.h` | 数学函数 |
| `time.h` | 时间处理 |
| `inttypes.h` | 整数格式化宏 (`PRId64` 等) |
| `stdbool.h` | 布尔类型 (`bool`, `true`, `false`) |

**说明：**
- `_GNU_SOURCE` 宏启用 GNU 扩展功能（如 `memmem` 函数在 Linux 上）
- XRT 零外部依赖，仅依赖 C 标准库

---

## 保护宏定义

### XXRTL_CORE

头文件保护宏，防止重复包含：

```c
#ifndef XXRTL_CORE
    #define XXRTL_CORE
    // ... 所有类型定义和函数声明 ...
#endif
```

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // UTF-8 字符串
    str text = (str)"Hello World";
    printf("UTF-8: %s\n", text);
    
    // 转换为 UTF-16
    u16str wtext = xrtUTF8to16(text, 0);
    printf("UTF-16 length: %zu\n", u16len(wtext));
    xrtFree(wtext);
    
    // 二进制数据
    binary data = (binary)"\x00\x01\x02\x03";
    printf("Binary first byte: 0x%02X\n", data[0]);
    
    xrtUnit();
    return 0;
}
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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 单精度浮点数
    f32 pi = 3.14159f;
    float32 radius = 5.0f;
    printf("圆周长: %.4f\n", 2 * pi * radius);
    
    // 双精度浮点数
    f64 e = 2.718281828459045;
    float64 x = 10.0;
    printf("e^10 ≈ %.6f\n", pow(e, x));
    
    xrtUnit();
    return 0;
}
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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 通用指针
    ptr data = xrtMalloc(1024);
    printf("分配内存地址: %p\n", data);
    
    // 指针转整数
    intptr addr = (intptr)data;
    printf("指针整数值: %" PRIdPTR "\n", addr);
    
    // 无符号指针整数
    uintptr uaddr = (uintptr)data;
    printf("无符号指针值: %" PRIuPTR "\n", uaddr);
    
    xrtFree(data);
    xrtUnit();
    return 0;
}
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

## 运行时数据模型

当前主线不再把运行时文档写成“外部应直接依赖的完整 `xCore` 结构布局”。

更准确的理解方式是：

- 进程级运行时状态
- 线程级运行时状态

### 进程级运行时

进程级运行时保存的是整个进程共享的稳定信息，例如：

- `sNull`
- 程序路径与文件路径
- 分配器入口
- 全局错误回调
- 高精度时钟环境

这一层主要用于：

- 一次性初始化
- 进程级配置
- 平台环境信息

### 线程级运行时

线程级运行时保存的是当前线程独有的状态，例如：

- 最后错误
- 临时内存
- 默认随机数状态
- 协程运行时
- 当前线程 deferred continuation 队列
- 后续线程绑定内存上下文

这也是为什么当前主线更推荐：

- 使用运行时 API
- 而不是继续依赖旧 `xCore` 布局假设

### 推荐访问方式

优先通过运行时 API 访问状态：

- `xrtInit()`
- `xrtUnit()`
- `xrtThreadAttachCurrent()`
- `xrtThreadDetachCurrent()`
- `xrtGetError()`
- `xrtClearError()`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main()
{
	xrtInit();
	
	printf("App Path: %s\n", (char*)xCore.AppPath);
	printf("App File: %s\n", (char*)xCore.AppFile);
	
	xrtClearError();
	printf("Error: %s\n", (char*)xrtGetError());
	
	xrtUnit();
	return 0;
}
```

### 兼容性说明

`xCore` 仍然存在，并继续承载进程级运行时对象；但当前主线已经不再把“任意直接字段访问”视为推荐的 public 契约。

尤其是：

- 线程级错误状态应通过 `xrtGetError()` 读取
- 临时内存应理解为线程级运行时临时内存
- 宿主线程如果要调用 runtime-bound API，应先 `xrtThreadAttachCurrent()`

---

## 相关文档

- [Base 基础模块](api-base.md)
- [Thread 线程库](api-thread.md)
- [Future / Task / Promise](api-future-task-promise.md)
- [主索引](README.md)

---

## 字段详解

### 全局常量

| 字段 | 类型 | 说明 |
|------|------|------|
| `sNull` | `str` | 空字符串常量，用于比较和返回 |

### 运行时返回值约定

当前主线不再推荐依赖 `xCore.sRet / xCore.iRet / xCore.nRet` 这种隐藏返回槽。

更推荐的方式是：

- 需要长度时，优先使用显式输出参数（例如 `size_t* iRetSize`）
- 需要字符串长度时，直接对返回字符串做 `strlen((char*)...)`
- 需要错误信息时，使用 `xrtGetError()`

**推荐示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main()
{
	xrtInit();
	
	str dir = xrtPathGetDir((str)"C:\\test\\file.txt", 0);
	printf("目录: %s (长度: %zu)\n", dir, strlen((char*)dir));
	xrtFree(dir);
	
	xrtUnit();
	return 0;
}
```

### 错误处理

| 字段 | 类型 | 说明 |
|------|------|------|
| `OnError` | 函数指针 | 全局错误回调函数 |

**使用示例：**
```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[XRT ERROR] %s\n", sError);
}

int main() {
    xrtInit();
    
    // 设置错误回调
    xCore.OnError = MyErrorHandler;
    
    // 模拟错误
    xrtSetError((str)"Test error message", FALSE);
    
    // 检查当前线程错误
    if ( xrtGetError()[0] != 0 ) {
        printf("当前错误: %s\n", xrtGetError());
        xrtClearError();
    }
    
    xrtUnit();
    return 0;
}
```

### 应用信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `LocalAddr` | `uint` | 本机IP地址（用于生成XID） |
| `AppFile` | `str` | 当前程序完整路径 |
| `AppPath` | `str` | 当前程序所在目录 |

### 线程临时内存

当前 `xrtTempMemory()` 使用的是当前线程私有的临时内存槽。

应这样理解它：

1. 只适合短期临时结果
2. 同一线程内的后续临时分配可能覆盖更早结果
3. 线程退出或显式 `xrtFreeTempMemory()` 时会统一释放
4. 不同线程之间互不干扰

**使用示例：**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // 这些临时内存会自动管理，无需 xrtFree
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
#include "xrt.h"
#include <stdio.h>

static size_t g_allocCount = 0;
static size_t g_totalBytes = 0;

ptr MyMalloc(size_t size) {
    g_allocCount++;
    g_totalBytes += size;
    printf("[分配 #%zu] %zu 字节\n", g_allocCount, size);
    return malloc(size);
}

void MyFree(ptr pMem) {
    if (pMem) {
        printf("[释放] 地址 %p\n", pMem);
        free(pMem);
    }
}

int main() {
    xrtInit();
    
    // 使用自定义分配器
    xCore.malloc = MyMalloc;
    xCore.free = MyFree;
    
    // 现在所有 xrtMalloc/xrtFree 都会使用自定义函数
    ptr data = xrtMalloc(256);
    xrtFree(data);
    
    printf("\n统计: 分配 %zu 次, 共 %zu 字节\n", g_allocCount, g_totalBytes);
    
    xrtUnit();
    return 0;
}
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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ 推荐：使用 XRT 类型别名
    i64 value = 1234567890123LL;
    str text = (str)"Hello XRT";
    u32 count = 100;
    f64 ratio = 3.14159265359;
    
    printf("整数: %" PRId64 "\n", value);
    printf("字符串: %s\n", text);
    printf("计数: %u\n", count);
    printf("比例: %.10f\n", ratio);
    
    // ❓ 不推荐（虽然可以）
    // long long value = 1234567890123LL;
    // unsigned char* text = "Hello";
    
    xrtUnit();
    return 0;
}
```

### 2. 检查初始化状态

```c
#include "xrt.h"
#include <stdio.h>

void EnsureInit()
{
	// 当前主线下，重复调用 xrtInit() 是可接受的
	// 调用方无需自行读取内部初始化标志
	xrtInit();
}

int main()
{
	// 第一次调用会完成初始化
	EnsureInit();
	printf("XRT 已初始化\n");
	
	// 第二次调用仍然安全
	EnsureInit();
	printf("重复初始化调用也是安全的\n");
	
	xrtUnit();
	return 0;
}
```

### 3. 使用临时内存

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // 对于临时使用的小内存，优先使用 xrtTempMemory
    str temp = xrtTempMemory(256);  // 自动管理
    sprintf((char*)temp, "临时数据: %d", 12345);
    printf("%s\n", temp);
    // 无需 xrtFree
    
    // 对于需要长期持有的内存，使用 xrtMalloc
    str permanent = xrtMalloc(1024);  // 需要 xrtFree
    strcpy((char*)permanent, "持久数据");
    printf("%s\n", permanent);
    xrtFree(permanent);  // 必须释放
    
    xrtUnit();
    return 0;
}
```

### 4. 错误处理

```c
#include "xrt.h"
#include <stdio.h>

void MyErrorHandler(str sError) {
    fprintf(stderr, "[ERROR] %s\n", sError);
    // 可选：记录日志、发送通知等
}

bool DoSomething(int value) {
    if (value < 0) {
        xrtSetError((str)"参数不能为负数", FALSE);
        return false;
    }
    printf("处理值: %d\n", value);
    return true;
}

int main() {
    xrtInit();
    xCore.OnError = MyErrorHandler;
    
    // 测试正常情况
    if (DoSomething(10)) {
        printf("成功\n");
    }
    
    // 测试错误情况
    if (!DoSomething(-5)) {
        printf("失败: %s\n", xrtGetError());
        xrtClearError();
    }
    
    xrtUnit();
    return 0;
}
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
