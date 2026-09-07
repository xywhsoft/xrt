# 进程信号

`<xrt/signal.h>` 提供进程自身信号的订阅、一次性订阅、忽略、恢复、发送和计数能力。模块把原生信号处理上下文限制为无锁计数与唤醒；用户回调统一在一条 XRT 调度线程中串行执行，因此可以使用常规 XRT API，但不应长期阻塞。

启用方式：

```c
#define XRT_MODULE_SIGNAL
#include "xrt.h"
```

依赖闭包为 `atomic`、`once`、`cond`、`thread` 及这些模块的基础依赖。原生处理器要求目标平台提供锁自由 32 位原子操作，不满足时首次订阅返回 `XERR_UNSUPPORTED`。未启用 `signal` 时，不会编译平台处理器、调度线程或唤醒资源。

## 类型与常量

### `xsignal`

XRT 信号代码跨平台稳定；并非每个平台都支持全部代码。

```c
typedef enum xsignal {
	XSIGNAL_NONE = 0,
	XSIGNAL_HUP = 1,
	XSIGNAL_INT = 2,
	XSIGNAL_TERM = 15,
	XSIGNAL_BREAK = 1001,
	XSIGNAL_CLOSE = 1002,
	XSIGNAL_LOGOFF = 1003,
	XSIGNAL_SHUTDOWN = 1004
} xsignal;
```

| 值 | 语义 |
|---|---|
| `XSIGNAL_NONE` | 无 |
| `XSIGNAL_HUP` | HUP |
| `XSIGNAL_INT` | 有符号整数 |
| `XSIGNAL_TERM` | TERM |
| `XSIGNAL_BREAK` | BREAK |
| `XSIGNAL_CLOSE` | CLOSE |
| `XSIGNAL_LOGOFF` | LOGOFF |

### `xsignalerror`

信号错误代码在 xrt.signal 错误域内稳定。

```c
typedef enum xsignalerror {
	XSIGNAL_ERROR_CODE = 1,
	XSIGNAL_ERROR_UNSUPPORTED,
	XSIGNAL_ERROR_SYSTEM,
	XSIGNAL_ERROR_STATE
} xsignalerror;
```

| 值 | 语义 |
|---|---|
| `XSIGNAL_ERROR_CODE` | 失败 |
| `XSIGNAL_ERROR_UNSUPPORTED` | 不支持 |
| `XSIGNAL_ERROR_SYSTEM` | 失败 |

### `xsignalevent`

一次调度可以合并多个同类原生通知，Count 是本批数量，Total 是清零后的累计数量。

```c
typedef struct xsignalevent {
	xsignal Code;
	int32 SystemCode;
	uint32 Count;
	uint64 Total;
	xtime Time;
	cstr Name;
} xsignalevent;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Code` | `xsignal` | 错误码 |
| `SystemCode` | `int32` | 平台错误码 |
| `Count` | `uint32` | 数量 |
| `Total` | `uint64` | 总量 |
| `Time` | `xtime` | 时间戳（Unix 微秒） |
| `Name` | `cstr` | 名称 |

### `xsignalwatch`

信号监听句柄由 XRT 引用计数管理，对外保持不透明。

```c
typedef struct xsignalwatch xsignalwatch;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xsignalproc`

用户回调始终在 XRT 信号调度线程执行，不在原生信号处理上下文执行。

```c
typedef void (*xsignalproc)(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xsignalfreeproc`

Owned 监听句柄最终释放时执行数据析构器。

```c
typedef void (*xsignalfreeproc)(ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 信号代码

`xsignal` 使用跨平台稳定代码：

- `XSIGNAL_INT`：交互中断；Windows 对应 `SIGINT`，POSIX 对应 `SIGINT`。
- `XSIGNAL_TERM`：终止请求；对应 `SIGTERM`。
- `XSIGNAL_HUP`：POSIX 挂断；Windows 不支持。
- `XSIGNAL_BREAK`：Windows 控制台 Break；POSIX 不支持。
- `XSIGNAL_CLOSE`、`XSIGNAL_LOGOFF`、`XSIGNAL_SHUTDOWN`：Windows 控制台生命周期通知；POSIX 不支持。

先用 `xrtSignalSupported` 判断平台能力。`xrtSignalName` 对已知代码返回稳定大写名称，对未知代码返回 `UNKNOWN`。

### `xrtSignalSupported`

判断当前平台是否支持指定信号代码。

```c
bool xrtSignalSupported(xsignal Code)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | — | 信号代码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否支持 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 平台支持查询

```c
	if ( !xrtSignalSupported(XSIGNAL_INT) ||
		!xrtSignalSupported(XSIGNAL_TERM) ||  /* Windows 无 TERM */
		(xrtSignalName(XSIGNAL_INT) == NULL) ||
		(xrtSignalName(XSIGNAL_NONE) == NULL) ||
		!xrtSignalHealthy() ) {
```

### `xrtSignalName`

返回稳定信号名称；未知代码返回 `"UNKNOWN"`。

```c
cstr xrtSignalName(xsignal Code)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | — | 信号代码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 稳定名称（`"INT"`、`"TERM"` 等）；未知为 `"UNKNOWN"` | — |

#### 错误

- 无 — 未知代码返回 `"UNKNOWN"` 且不设置错误

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 信号名称

```c
		(xrtSignalName(XSIGNAL_INT) == NULL) ||
```

## 订阅

```c
xsignalwatch* xrtSignalOn(xsignal Code, xsignalproc pProc, ptr pData);
xsignalwatch* xrtSignalOnOwned(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree
);
xsignalwatch* xrtSignalOnce(xsignal Code, xsignalproc pProc, ptr pData);
xsignalwatch* xrtSignalOnceOwned(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree
);
```

`On` 重复调度，`Once` 第一次入选事件快照时先变为不活动，再执行一次回调。Owned 入口只在创建成功后接管 `pData`，句柄最终释放时恰好调用一次析构器。监听器没有固定数量上限，调度过程不分配临时监听数组。

回调签名：

```c
void callback(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
);
```

`xsignalevent.Count` 是本次合并批次的通知数，`Total` 是最近一次 `xrtSignalClear` 或 `xrtSignalShutdown` 之后的累计数，`Time` 是调度线程构造事件时的 XRT 时间。`SystemCode` 保留本批最后一个原生通知代码。

同一进程的信号回调由唯一调度线程串行执行。一个事件开始调度后新增的监听不会回看该事件。回调可以注销自身，也可以注册其他监听。

### `xrtSignalOn`

订阅信号；成功后调用方拥有返回句柄，回调可重复执行。

```c
xsignalwatch* xrtSignalOn(
	xsignal Code,
	xsignalproc pProc,
	ptr pData
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效且平台支持 | 信号代码 |
| `pProc` | 输入 | 非空 | 信号回调 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 监听句柄（引用 1） | — |
| `NULL` | 订阅失败 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` / `XSIGNAL_ERROR_CODE`（`XERR_ARGUMENT`） — 信号代码无效
- `xrt.signal` / `XSIGNAL_ERROR_UNSUPPORTED`（`XERR_UNSUPPORTED`） — 当前平台不支持该信号
- `xrt.signal` / `XSIGNAL_ERROR_STATE`（`XERR_STATE`） — 调度线程创建失败
- `xrt.signal` 域错误（`XERR_RANGE`） — 监听句柄标识空间耗尽
- `XERR_MEMORY` — 句柄分配失败

#### 范例

[signal](../../examples/process/signal/main.c) · 订阅

```c
	xsignalwatch* pWatch = xrtSignalOn(
		XSIGNAL_INT,
		exampleSignal,
		&Received
	);
```

### `xrtSignalOnOwned`

订阅信号并在句柄最终释放时析构用户数据；失败时数据所有权不转移。

```c
xsignalwatch* xrtSignalOnOwned(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效且平台支持 | 信号代码 |
| `pProc` | 输入 | 非空 | 信号回调 |
| `pData` | 输入 | 任意值 | 回调数据，成功后所有权转移 |
| `pFree` | 输入 | 非空 | 数据析构回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 监听句柄（引用 1） | — |
| `NULL` | 订阅失败，数据仍归调用方 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` / `XSIGNAL_ERROR_CODE`（`XERR_ARGUMENT`） — 信号代码无效
- `xrt.signal` / `XSIGNAL_ERROR_UNSUPPORTED`（`XERR_UNSUPPORTED`） — 当前平台不支持该信号
- `xrt.signal` / `XSIGNAL_ERROR_STATE`（`XERR_STATE`） — 调度线程创建失败
- `xrt.signal` 域错误（`XERR_RANGE`） — 监听句柄标识空间耗尽
- `XERR_MEMORY` — 句柄分配失败

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 订阅（接管数据）

```c
	pOwned = xrtSignalOnOwned(XSIGNAL_INT, exampleOnCallback,
		NULL, exampleFree);
```

### `xrtSignalOnce`

订阅一次信号；第一次入选调度后先注销，再执行用户回调。

```c
xsignalwatch* xrtSignalOnce(
	xsignal Code,
	xsignalproc pProc,
	ptr pData
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效且平台支持 | 信号代码 |
| `pProc` | 输入 | 非空 | 信号回调 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 一次性监听句柄（引用 1） | — |
| `NULL` | 订阅失败 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` / `XSIGNAL_ERROR_CODE`（`XERR_ARGUMENT`） — 信号代码无效
- `xrt.signal` / `XSIGNAL_ERROR_UNSUPPORTED`（`XERR_UNSUPPORTED`） — 当前平台不支持该信号
- `xrt.signal` / `XSIGNAL_ERROR_STATE`（`XERR_STATE`） — 调度线程创建失败
- `xrt.signal` 域错误（`XERR_RANGE`） — 监听句柄标识空间耗尽
- `XERR_MEMORY` — 句柄分配失败

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 订阅一次

```c
	pOnce = xrtSignalOnce(XSIGNAL_INT, exampleOnceCallback, NULL);
```

### `xrtSignalOnceOwned`

订阅一次信号并接管用户数据；失败时数据所有权不转移。

```c
xsignalwatch* xrtSignalOnceOwned(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效且平台支持 | 信号代码 |
| `pProc` | 输入 | 非空 | 信号回调 |
| `pData` | 输入 | 任意值 | 回调数据，成功后所有权转移 |
| `pFree` | 输入 | 非空 | 数据析构回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 一次性监听句柄（引用 1） | — |
| `NULL` | 订阅失败，数据仍归调用方 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` / `XSIGNAL_ERROR_CODE`（`XERR_ARGUMENT`） — 信号代码无效
- `xrt.signal` / `XSIGNAL_ERROR_UNSUPPORTED`（`XERR_UNSUPPORTED`） — 当前平台不支持该信号
- `xrt.signal` / `XSIGNAL_ERROR_STATE`（`XERR_STATE`） — 调度线程创建失败
- `xrt.signal` 域错误（`XERR_RANGE`） — 监听句柄标识空间耗尽
- `XERR_MEMORY` — 句柄分配失败

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 订阅一次（接管数据）

```c
	pOnceOwned = xrtSignalOnceOwned(XSIGNAL_INT,
		exampleOnceCallback, NULL, exampleFree);
```

## 句柄生命周期

```c
xsignalwatch* xrtSignalRef(xsignalwatch* pWatch);
bool xrtSignalOff(xsignalwatch* pWatch);
void xrtSignalFree(xsignalwatch* pWatch);
bool xrtSignalActive(const xsignalwatch* pWatch);
bool xrtSignalHealthy(void);
xsignal xrtSignalCode(const xsignalwatch* pWatch);
```

`xrtSignalOff` 幂等注销，但不释放调用方引用。从非调度线程调用时，它会等待该句柄已经开始的回调结束，因此返回后不会再执行该监听的用户代码。从监听自己的回调内注销不会自锁。`xrtSignalFree` 先注销，再释放一个引用；每次 `xrtSignalRef` 都需要对应一次 `xrtSignalFree`。

平台等待后端发生不可恢复错误时，XRT 会立即停用监听、尝试恢复原生处理方式，并把结构化错误交给进程级错误处理器。`xrtSignalHealthy` 返回 `false` 并在当前执行上下文重建同一系统错误；调用 `xrtSignalShutdown` 完成清理后可以重新惰性启动。

### `xrtSignalRef`

增加监听句柄引用并返回原指针。

```c
xsignalwatch* xrtSignalRef(xsignalwatch* pWatch)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWatch` | 输入 | 非空 | 目标句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或信号代码非法

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 共享引用

```c
	pRef = xrtSignalRef(pOwned);
```

### `xrtSignalOff`

幂等注销监听；从其他线程调用时，返回前保证该句柄回调已经结束。

```c
bool xrtSignalOff(xsignalwatch* pWatch)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWatch` | 输入 | 非空 | 目标句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已注销（或此前已注销） | — |
| `false` | 参数非法或引用失败 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或信号代码非法

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 幂等注销

```c
	if ( !xrtSignalOff(pOwned) ||
		xrtSignalActive(pOwned) ) {
```

### `xrtSignalFree`

注销监听并释放一个调用方引用；空指针可安全传入。

```c
void xrtSignalFree(xsignalwatch* pWatch)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWatch` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1，归零时释放并析构数据 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[signal](../../examples/process/signal/main.c) · 释放引用

```c
		xrtSignalFree(pWatch);
```

### `xrtSignalActive`

判断监听是否仍会进入新的回调。

```c
bool xrtSignalActive(const xsignalwatch* pWatch)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWatch` | 输入 | 非空 | 目标句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否仍活跃 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 活跃查询

```c
		!xrtSignalActive(pOwned) ||
```

### `xrtSignalCode`

返回监听对应的信号代码；空指针返回 `XSIGNAL_NONE`。

```c
xsignal xrtSignalCode(const xsignalwatch* pWatch)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWatch` | 输入 | 非空 | 目标句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 信号代码 | 订阅的代码 | — |
| `XSIGNAL_NONE` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或信号代码非法

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 信号代码查询

```c
		(xrtSignalCode(pOwned) != XSIGNAL_INT) ) {
```

## 原生处理方式

```c
bool xrtSignalIgnore(xsignal Code);
bool xrtSignalRestore(xsignal Code);
bool xrtSignalRestoreAll(void);
bool xrtSignalRaise(xsignal Code);
```

Ignore 和 Restore 都会先注销对应代码的全部 XRT 监听。Restore 恢复 XRT 首次接管前保存的原生处理方式，不强行恢复为系统默认值。Raise 使用当前平台的进程内原生发送能力；如果当前处理方式是默认方式，它可能立即终止进程，因此通常只应在已经监听或明确忽略时调用。

Windows 的 INT 在 XRT 监听或忽略期间使用进程内逻辑投递，避免 `GenerateConsoleCtrlEvent` 把 Ctrl+C 广播到整个控制台进程组；未接管时仍使用 CRT 默认处理。Close、Logoff 和 Shutdown 没有安全的单进程程序化发送入口，`xrtSignalRaise` 对这些代码返回 `XERR_UNSUPPORTED`。这些生命周期事件还受到系统控制台处理超时约束，操作系统可能在长回调完成前终止进程，因此回调应只做快速通知，把收尾工作交给应用主流程。

### `xrtSignalIgnore`

忽略指定信号并注销该代码的全部 XRT 监听。

```c
bool xrtSignalIgnore(xsignal Code)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效且平台支持 | 信号代码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已忽略并注销 | — |
| `false` | 失败 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` / `XSIGNAL_ERROR_CODE`（`XERR_ARGUMENT`） — 信号代码无效
- `xrt.signal` / `XSIGNAL_ERROR_UNSUPPORTED`（`XERR_UNSUPPORTED`） — 当前平台不支持该信号

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 忽略信号

```c
	if ( !xrtSignalIgnore(XSIGNAL_INT) ||
		!xrtSignalRestore(XSIGNAL_INT) ||
		!xrtSignalIgnore(XSIGNAL_INT) ||
		!xrtSignalRestoreAll() ) {
```

### `xrtSignalRestore`

注销指定代码的全部监听，并恢复 XRT 接管前的原生处理方式。

```c
bool xrtSignalRestore(xsignal Code)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效 | 信号代码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已恢复 | — |
| `false` | 失败 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` / `XSIGNAL_ERROR_CODE`（`XERR_ARGUMENT`） — 信号代码无效

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 恢复原生处理

```c
		!xrtSignalRestore(XSIGNAL_INT) ||
```

### `xrtSignalRestoreAll`

注销全部监听并恢复全部由 XRT 接管的原生处理方式。

```c
bool xrtSignalRestoreAll(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已全部恢复 | — |
| `false` | 失败 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` 域错误 — 恢复过程中平台调用失败

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 恢复全部

```c
		!xrtSignalRestoreAll() ) {
```

### `xrtSignalRaise`

向当前进程发送原生信号；默认处理方式可能终止进程。

```c
bool xrtSignalRaise(xsignal Code)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效且平台可发送 | 信号代码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已发送 | — |
| `false` | 发送失败 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` / `XSIGNAL_ERROR_CODE`（`XERR_ARGUMENT`） — 信号代码无效
- `xrt.signal` / `XSIGNAL_ERROR_UNSUPPORTED`（`XERR_UNSUPPORTED`） — 该信号无法在当前平台发送

#### 范例

[signal](../../examples/process/signal/main.c) · 发送信号

```c
	if ( (pWatch == NULL) || !xrtSignalRaise(XSIGNAL_INT) ) {
```

## 计数与关闭

```c
uint64 xrtSignalCount(xsignal Code);
bool xrtSignalReceived(xsignal Code);
bool xrtSignalClear(xsignal Code);
bool xrtSignalShutdown(void);
```

向 Count 或 Clear 传入 `XSIGNAL_NONE` 表示全部信号。待处理计数使用饱和 32 位原子数，自管道或 Windows Event 只负责唤醒，因此高频通知不会因为唤醒对象暂时已满而静默丢失。

Shutdown 注销全部监听、恢复所有原生处理方式、等待调度线程结束并释放平台唤醒资源。调度线程回调内调用会返回 `XERR_STATE`，避免等待自身。关闭后再次订阅会惰性创建一套新的调度资源。

### `xrtSignalCount`

返回指定信号的累计接收数；`XSIGNAL_NONE` 返回全部信号之和。

```c
uint64 xrtSignalCount(xsignal Code)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效或 `XSIGNAL_NONE` | 信号代码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 累计接收数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或信号代码非法

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 累计接收数

```c
		uint64 iBefore = xrtSignalCount(XSIGNAL_INT);
```

### `xrtSignalReceived`

判断指定信号自上次清零后是否至少接收过一次。

```c
bool xrtSignalReceived(xsignal Code)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效 | 信号代码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否接收过 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或信号代码非法

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 接收判断

```c
		if ( !xrtSignalReceived(XSIGNAL_INT) ||
			(xrtSignalCount(XSIGNAL_INT) < 2u) ||
			(iBefore < 2u) ) {
```

### `xrtSignalClear`

清零指定信号的累计数与尚未调度数量；`XSIGNAL_NONE` 清零全部。

```c
bool xrtSignalClear(xsignal Code)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Code` | 输入 | 有效或 `XSIGNAL_NONE` | 信号代码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已清零 | — |
| `false` | 代码无效 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或信号代码非法

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 清零计数

```c
		if ( !xrtSignalClear(XSIGNAL_INT) ||
			xrtSignalReceived(XSIGNAL_INT) ||
			(xrtSignalCount(XSIGNAL_INT) != 0u) ) {
```

### `xrtSignalHealthy`

判断调度后端是否健康；故障时重建前必须先调用 `xrtSignalShutdown`。

```c
bool xrtSignalHealthy(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 后端是否健康 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[signal_tour](../../examples/process/signal_tour/main.c) · 后端健康

```c
		!xrtSignalHealthy() ) {
```

### `xrtSignalShutdown`

停止调度线程、注销全部监听并恢复原生处理方式；回调线程内不可调用。

```c
bool xrtSignalShutdown(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已停止并恢复 | — |
| `false` | 失败 | `xrt.signal` 域错误 |

#### 错误

- `xrt.signal` / `XSIGNAL_ERROR_STATE`（`XERR_STATE`） — 在信号回调线程内调用（自关闭）

#### 范例

[signal](../../examples/process/signal/main.c) · 关闭调度

```c
	return xrtSignalShutdown() ? 0 : 3;
```

## 错误

`xsignalerror` 属于 `xrt.signal` 错误域。非法代码、平台不支持、原生 API 失败和生命周期冲突分别使用 `XSIGNAL_ERROR_CODE`、`XSIGNAL_ERROR_UNSUPPORTED`、`XSIGNAL_ERROR_SYSTEM`、`XSIGNAL_ERROR_STATE`。系统失败通过 `xrtErrorSystemCode` 保留 `errno` 或 `GetLastError`。

完整示例见 `examples/process/signal/main.c`。
