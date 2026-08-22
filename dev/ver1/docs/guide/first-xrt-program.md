# 从零开始写第一个 XRT 程序

> 面向第一次接触 XRT 的开发者。目标不是覆盖所有 API，而是用最短路径理解当前主线的运行方式。

[返回教学文档](README.md) | [返回 API 索引](../api/README.md)

---

## 目录

- [目标](#目标)
- [为什么先从源码树开始](#为什么先从源码树开始)
- [准备一个最小程序](#准备一个最小程序)
- [编译与运行](#编译与运行)
- [这个程序体现了什么主线原则](#这个程序体现了什么主线原则)
- [下一步应该看什么](#下一步应该看什么)

---

## 目标

这一页只解决 4 件事：

1. 让你写出第一个能运行的 XRT 程序
2. 理解当前主线推荐的是源码树，而不是把所有东西都解释成单头文件
3. 理解 `xrtInit()` / `xrtUnit()` 是运行时入口
4. 理解字符串、动态值和错误处理的最小使用方式

---

## 为什么先从源码树开始

当前 XRT 仍然支持单头文件分发，但主线开发、测试和文档解释都已经建立在源码树之上。

因此，入门时优先推荐：

```c
#include "xrt.h"
```

再直接编译：

```bash
gcc main.c xrt.c -o app
```

或：

```bash
tcc main.c xrt.c -o app.exe
```

这样你接触到的就是当前正式主线，而不是历史示例或分发表面的变体。

---

## 准备一个最小程序

新建一个 `main.c`：

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	str sGreeting;
	xvalue pCount;

	xrtInit();

	sGreeting = xrtCopyStr("Hello, XRT!", 0);
	pCount = xvoCreateInt(42);

	printf("Greeting: %s\n", (char*)sGreeting);
	printf("Count: %lld\n", (long long)xvoGetInt(pCount));

	xvoUnref(pCount);
	xrtFree(sGreeting);

	xrtUnit();
	return 0;
}
```

---

## 编译与运行

### Windows / MinGW GCC

```bash
gcc main.c xrt.c -o app.exe
app.exe
```

### Windows / TCC

```bash
tcc main.c xrt.c -o app.exe
app.exe
```

### Linux / GCC

```bash
gcc main.c xrt.c -o app
./app
```

如果看到类似输出，就说明当前主线已跑通：

```text
Greeting: Hello, XRT!
Count: 42
```

---

## 这个程序体现了什么主线原则

### 1. `xrtInit()` / `xrtUnit()` 是运行时边界

当前 XRT 已经有明确的运行时概念：

- `xrtInit()`：初始化当前运行时，并把主线程附加到 XRT
- `xrtUnit()`：释放运行时相关资源

只要程序会使用字符串分配、动态值、线程绑定错误状态、临时内存、协程、future 或网络主线能力，都应保持这对边界。

### 2. `str` 是 XRT 自己的字符串主线

XRT 的 `str` 不是普通 `char*` 风格别名，而是有明确设计意图的字符串/字节视图类型。  
与标准库函数交互时，必要时做 `(char*)` 转换即可。

### 3. `xvalue` 是高层动态数据主线

这个例子里的：

```c
xvalue pCount = xvoCreateInt(42);
```

体现的是 XRT 统一的数据处理方向。以后你会继续在：

- JSON
- 模板
- 配置
- 网络请求/响应
- AI 场景结构化数据

里反复用到它。

### 4. 内存释放方式是分层的

这个例子里有两类释放：

- `xrtFree()`：释放普通分配的字符串
- `xvoUnref()`：释放引用计数值对象

它们不能混用。

---

## 下一步应该看什么

建议按这个顺序继续：

1. [Base 基础运行时](../api/api-base.md)
2. [Value 动态类型系统](../api/api-value.md)
3. [JSON 处理](../api/api-json.md)
4. [Thread 线程管理](../api/api-thread.md)
5. [Coroutine 协程运行时](../api/api-coroutine.md)

如果你已经准备写网络程序，下一步看：

1. [XNet V2](../api/api-xnet-v2.md)
2. [Network TLS](../api/api-network-tls.md)
3. [XHTTP](../api/api-xhttp.md)

---

**状态：第一批正式教学页**
