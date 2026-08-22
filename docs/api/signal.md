# 进程信号

`<xrt/signal.h>` 提供进程自身信号的订阅、一次性订阅、忽略、恢复、发送和计数能力。模块把原生信号处理上下文限制为无锁计数与唤醒；用户回调统一在一条 XRT 调度线程中串行执行，因此可以使用常规 XRT API，但不应长期阻塞。

启用方式：

```c
#define XRT_MODULE_SIGNAL
#include "xrt.h"
```

依赖闭包为 `atomic`、`once`、`cond`、`thread` 及这些模块的基础依赖。原生处理器要求目标平台提供锁自由 32 位原子操作，不满足时首次订阅返回 `XERR_UNSUPPORTED`。未启用 `signal` 时，不会编译平台处理器、调度线程或唤醒资源。

## 信号代码

`xsignal` 使用跨平台稳定代码：

- `XSIGNAL_INT`：交互中断；Windows 对应 `SIGINT`，POSIX 对应 `SIGINT`。
- `XSIGNAL_TERM`：终止请求；对应 `SIGTERM`。
- `XSIGNAL_HUP`：POSIX 挂断；Windows 不支持。
- `XSIGNAL_BREAK`：Windows 控制台 Break；POSIX 不支持。
- `XSIGNAL_CLOSE`、`XSIGNAL_LOGOFF`、`XSIGNAL_SHUTDOWN`：Windows 控制台生命周期通知；POSIX 不支持。

先用 `xrtSignalSupported` 判断平台能力。`xrtSignalName` 对已知代码返回稳定大写名称，对未知代码返回 `UNKNOWN`。

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

## 原生处理方式

```c
bool xrtSignalIgnore(xsignal Code);
bool xrtSignalRestore(xsignal Code);
bool xrtSignalRestoreAll(void);
bool xrtSignalRaise(xsignal Code);
```

Ignore 和 Restore 都会先注销对应代码的全部 XRT 监听。Restore 恢复 XRT 首次接管前保存的原生处理方式，不强行恢复为系统默认值。Raise 使用当前平台的进程内原生发送能力；如果当前处理方式是默认方式，它可能立即终止进程，因此通常只应在已经监听或明确忽略时调用。

Windows 的 INT 在 XRT 监听或忽略期间使用进程内逻辑投递，避免 `GenerateConsoleCtrlEvent` 把 Ctrl+C 广播到整个控制台进程组；未接管时仍使用 CRT 默认处理。Close、Logoff 和 Shutdown 没有安全的单进程程序化发送入口，`xrtSignalRaise` 对这些代码返回 `XERR_UNSUPPORTED`。这些生命周期事件还受到系统控制台处理超时约束，操作系统可能在长回调完成前终止进程，因此回调应只做快速通知，把收尾工作交给应用主流程。

## 计数与关闭

```c
uint64 xrtSignalCount(xsignal Code);
bool xrtSignalReceived(xsignal Code);
bool xrtSignalClear(xsignal Code);
bool xrtSignalShutdown(void);
```

向 Count 或 Clear 传入 `XSIGNAL_NONE` 表示全部信号。待处理计数使用饱和 32 位原子数，自管道或 Windows Event 只负责唤醒，因此高频通知不会因为唤醒对象暂时已满而静默丢失。

Shutdown 注销全部监听、恢复所有原生处理方式、等待调度线程结束并释放平台唤醒资源。调度线程回调内调用会返回 `XERR_STATE`，避免等待自身。关闭后再次订阅会惰性创建一套新的调度资源。

## 错误

`xsignalerror` 属于 `xrt.signal` 错误域。非法代码、平台不支持、原生 API 失败和生命周期冲突分别使用 `XSIGNAL_ERROR_CODE`、`XSIGNAL_ERROR_UNSUPPORTED`、`XSIGNAL_ERROR_SYSTEM`、`XSIGNAL_ERROR_STATE`。系统失败通过 `xrtErrorSystemCode` 保留 `errno` 或 `GetLastError`。

完整示例见 `examples/process/signal/main.c`。
