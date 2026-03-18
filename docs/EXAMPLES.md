# XRT 示例说明

> 说明当前主线推荐的示例阅读方式、构建方式和学习顺序

[English Version](EXAMPLES.en.md) | [返回索引](README.md)

---

## 目录

- [示例的定位](#示例的定位)
- [当前推荐的构建方式](#当前推荐的构建方式)
- [建议优先阅读的示例主题](#建议优先阅读的示例主题)
- [推荐学习顺序](#推荐学习顺序)
- [一个最小主线示例](#一个最小主线示例)
- [一个结构化数据示例](#一个结构化数据示例)
- [一个线程与运行时示例](#一个线程与运行时示例)
- [一个 HTTP 客户端示例](#一个-http-客户端示例)
- [注意事项](#注意事项)

---

## 示例的定位

XRT 当前已经不是“只有一个 `#define XRT_IMPLEMENTATION` 的单头小工具库”，而是一个包含源码树、单头文件分发、测试、bench 与多条子系统主线的基础设施库。

因此，当前示例文档的目标不是继续维护一批过时的“单文件复制示例”，而是明确告诉你：

- 当前主线推荐如何引入 XRT
- 不同能力分别应该看哪些 API 文档
- 示例代码应如何按当前 runtime / 并发 / 网络主线书写

如果你要看更完整的 API 说明，请优先阅读：

- [API 索引](api/README.md)
- [Base 基础函数库](api/api-base.md)
- [Thread 线程管理库](api/api-thread.md)
- [Coroutine 协程运行时](api/api-coroutine.md)
- [Future / Task / Promise](api/api-future-task-promise.md)
- [XNet v2 网络主线](api/api-xnet-v2.md)

---

## 当前推荐的构建方式

### 1. 源码树主线

当前主线开发、测试和集成，推荐直接使用源码树：

```c
#include "xrt.h"
```

再编译：

```bash
gcc main.c xrt.c -o app
```

或：

```bash
tcc main.c xrt.c -o app.exe
```

这是当前最稳定、最贴近主线实现的方式。

### 2. 单头文件分发

单头文件分发仍然支持，但它是“分发形态”，不是当前文档和示例的主解释模型。  
如果你确实要使用单头文件，请优先确认生成版本与源码树已同步。

---

## 建议优先阅读的示例主题

当前更推荐按“能力主题”学习，而不是按旧文档那种固定 8 个文件去记。

### 1. 基础运行时

适合先理解：

- `xrtInit() / xrtUnit()`
- `xrtGetError()`
- `xrtTempMemory()`
- 时间、文件、字符串、格式化函数

对应文档：

- [api/api-base.md](api/api-base.md)
- [api/api-string.md](api/api-string.md)
- [api/api-file.md](api/api-file.md)

### 2. 结构化数据

适合继续理解：

- `xvalue`
- `json`
- array / list / dict / coll / table

对应文档：

- [api/api-value.md](api/api-value.md)
- [api/api-json.md](api/api-json.md)

### 3. 并发与异步

适合理解当前主线的“线程 + 协程 + future / task / promise”：

- `xrtThreadCreate()`
- `xrtThreadAttachCurrent()`
- `xrtCoSchedCreate()`
- `xFuture* / xTask* / xWaitSource*`

对应文档：

- [api/api-thread.md](api/api-thread.md)
- [api/api-coroutine.md](api/api-coroutine.md)
- [api/api-future-task-promise.md](api/api-future-task-promise.md)

### 4. 网络主线

适合理解当前 `xnet-v2`：

- `stream`
- `dgram`
- `listener`
- `future / wait-source`
- `TLS session`
- `HTTP / HTTPD / WebSocket`

对应文档：

- [api/api-xnet-v2.md](api/api-xnet-v2.md)
- [api/api-network-tls.md](api/api-network-tls.md)
- [api/api-xhttp.md](api/api-xhttp.md)
- [api/api-xhttpd.md](api/api-xhttpd.md)
- [api/api-xws.md](api/api-xws.md)

---

## 推荐学习顺序

建议按下面顺序阅读和试写代码：

1. `xrtInit()` / `xrtUnit()` / `xrtGetError()`
2. 字符串、文件、时间
3. `xvalue + json`
4. 线程与线程附加模型
5. 协程与调度器
6. `future / task / promise / wait-source`
7. `xnet-v2`
8. `TLS session`
9. `HTTP / HTTPD / WebSocket`

这样能保证你看到的始终是当前主线，而不是历史残留表面。

如果你希望直接看更完整的入门或组合案例，也建议继续阅读：

- [教学文档](guide/README.md)
- [范例解析](case/README.md)

---

## 一个最小主线示例

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	str sGreeting;
	xvalue pNum;

	xrtInit();

	sGreeting = xrtCopyStr("Hello, XRT!", 0);
	pNum = xvoCreateInt(42);

	printf("Greeting: %s\n", (char*)sGreeting);
	printf("Number: %lld\n", (long long)xvoGetInt(pNum));

	xvoUnref(pNum);
	xrtFree(sGreeting);

	xrtUnit();
	return 0;
}
```

编译：

```bash
gcc main.c xrt.c -o app
```

或：

```bash
tcc main.c xrt.c -o app.exe
```

---

## 一个结构化数据示例

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xvalue pUser;
	xvalue pSkills;

	xrtInit();

	pUser = xvoCreateTable();
	pSkills = xvoCreateArray();

	xvoArrayAppendText(pSkills, "C", 0, FALSE);
	xvoArrayAppendText(pSkills, "Python", 0, FALSE);

	xvoTableSetText(pUser, "name", 0, "Alice", 0, FALSE);
	xvoTableSetInt(pUser, "age", 0, 25);
	xvoTableSetValue(pUser, "skills", 0, pSkills, TRUE);

	printf("name = %s\n", (char*)xvoTableGetText(pUser, "name", 0));
	printf("age = %lld\n", (long long)xvoTableGetInt(pUser, "age", 0));
	printf("skill[0] = %s\n", (char*)xvoArrayGetText(pSkills, 0));

	xvoUnref(pUser);
	xrtUnit();
	return 0;
}
```

---

## 一个线程与运行时示例

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	xthread pThread;
} ThreadCtx;

static uint32 Worker(ptr pArg)
{
	ThreadCtx* pCtx = (ThreadCtx*)pArg;

	while ( pCtx->pThread == NULL ) {
		xrtSleep(1);
	}

	printf("thread attached = %d\n", xrtThreadIsAttached());
	printf("rand32 = %u\n", xrtRand32());

	if ( xrtThreadShouldStop(pCtx->pThread) ) {
		return 1;
	}

	return 0;
}

int main(void)
{
	ThreadCtx tCtx = { 0 };

	xrtInit();

	tCtx.pThread = xrtThreadCreate(Worker, &tCtx, 0);
	xrtThreadWait(tCtx.pThread);
	xrtThreadDestroy(tCtx.pThread);

	xrtUnit();
	return 0;
}
```

这个示例体现了当前主线运行时合同：

- 主线程在 `xrtInit()` 后自动附加
- `xrtThreadCreate()` 创建的线程自动附加
- 在线程函数里可以直接使用 `xrtRand32()`、`xrtGetError()`、`xrtTempMemory()` 等线程绑定运行时能力

---

## 一个 HTTP 客户端示例

当前网络主线建议优先按 `xnet-v2 + TLS session + xhttp` 理解，不再继续使用已经退役的旧网络表面。

如果你要写 HTTP 客户端，建议先阅读：

- [api/api-xhttp.md](api/api-xhttp.md)
- [api/api-xnet-v2.md](api/api-xnet-v2.md)
- [api/api-network-tls.md](api/api-network-tls.md)

如果你要写 HTTP 服务端，建议先阅读：

- [api/api-xhttpd.md](api/api-xhttpd.md)
- [api/api-xws.md](api/api-xws.md)

---

## 注意事项

### 1. 不再推荐把旧单头文件示例当作当前主线参考

旧写法里常见的问题包括：

- 把源码树主线继续当成单头文件分发模式来讲解
- `gcc example.c xrt.h -o app`
- 直接访问 `xCore`
- 继续使用旧 TLS 或旧网络表面

这些都不再代表当前主线。

### 2. 运行时相关示例应始终成对出现

只要示例使用了以下任意能力：

- `xrtGetError()`
- `xrtTempMemory()`
- `xrtRand32()` / `xrtRand64()`
- 协程
- future / task / wait-source

都应显式写出：

```c
xrtInit();
/* ... */
xrtUnit();
```

### 3. 跨线程共享示例必须显式 shared

对 array / list / coll / table 这类容器，如果示例里要跨线程共享，应该明确写：

```c
xvalue pTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
```

而不是继续使用默认 local root。

---

## 总结

这份示例说明的重点不是维护一批孤立示例文件，而是明确：

- 当前主线怎么构建
- 当前主线该看哪些示例主题
- 哪些旧示例写法已经不再推荐

后续如果要补“零基础教学文档”和“范例解析文档”，建议放在：

- `docs/guide/`
- `docs/case/`

这样 `docs/api/` 保持 API 合同说明，`guide/` 和 `case/` 再承载更偏教学和拆解式内容。
