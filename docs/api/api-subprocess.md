# Subprocess 子进程模块

> 当前 `subprocess` 主线面向“可靠启动外部程序、结构化读写 stdio、拿到可判定的退出结果、支持 agent/tool 链路”这几个核心目标。

[返回索引](README.md)

---

## 1. 模块定位

这组 API 适合：

- 编译、测试、打包、脚本执行
- `git`、`grep`、`python`、`node` 之类的工具编排
- 需要把 stdout / stderr 流式接入日志或 UI
- 需要给 agent host 做本地命令执行底座

它解决的是“子进程运行时”问题，不是“打开系统默认程序”问题。  
如果你只是想让系统启动一个文件或 URL，优先看 [OS](api-os.md)。


## 2. 核心模型

当前 `subprocess` 有两种目标类型：

```c
#define XPROC_TARGET_EXEC		1
#define XPROC_TARGET_SHELL		2
```

- `XPROC_TARGET_EXEC`
	- 直接按 `program + argv` 启动
	- 默认优先使用这条路径
- `XPROC_TARGET_SHELL`
	- 走 shell 解释器
	- 默认 shell 为 Windows `cmd.exe`，POSIX `/bin/sh`
	- `sProgram` 也可以显式指定 shell 程序，例如 `pwsh`


### 2.1 标准流配置

```c
#define XPROC_STDIO_INHERIT		0
#define XPROC_STDIO_PIPE		1
#define XPROC_STDIO_NULL		2
```

- `INHERIT`
	- 继承当前进程对应的标准流
- `PIPE`
	- 为该流建立管道
	- 可以配合回调、快照、增量读取、stdin 写入
- `NULL`
	- 丢弃该流

`PIPE` 模式下会保留安全读取所需的捕获缓冲。  
`xrtProcessGetStdout()` / `xrtProcessGetStderr()` 返回的是新分配的快照，调用方负责 `xrtFree()`。


### 2.2 环境与工作目录

`xprocessconfig` 支持：

- `sWorkDir`
	- 严格工作目录
	- 无法进入目录时会直接判定为 `SPAWN_FAILED`
- `arrEnv + iEnvCount`
	- 环境变量覆盖数组，元素格式为 `KEY=VALUE`
- `bInheritEnv`
	- `true` 表示继承当前环境后再叠加覆盖
	- `false` 表示只使用 `arrEnv`

这让上层不再需要靠 shell 拼 `cd && export/set && command` 来模拟环境。


## 3. 关键类型

### 3.1 `xprocessstdio`

```c
typedef struct {
	int iMode;
	bool bCapture;
} xprocessstdio;
```

- `iMode`
	- `INHERIT / PIPE / NULL`
- `bCapture`
	- 显式要求保留捕获缓冲
	- `PIPE` 本身已经适合安全读取；这里主要用于更明确的调用意图


### 3.2 `xprocessexitinfo`

```c
typedef struct {
	int iKind;
	int iExitCode;
	int iSignal;
	int iStage;
	int iOsError;
	int iStopReason;
	bool bTimedOut;
	bool bCancelled;
} xprocessexitinfo;
```

退出类型：

```c
#define XPROC_EXIT_NONE			0
#define XPROC_EXIT_NORMAL		1
#define XPROC_EXIT_SIGNAL		2
#define XPROC_EXIT_SPAWN_FAILED	3
#define XPROC_EXIT_WAIT_FAILED	4
```

阶段信息：

```c
#define XPROC_STAGE_NONE		0
#define XPROC_STAGE_SPAWN		1
#define XPROC_STAGE_WORKDIR		2
#define XPROC_STAGE_ENV			3
#define XPROC_STAGE_STDIN		4
#define XPROC_STAGE_STDOUT		5
#define XPROC_STAGE_STDERR		6
#define XPROC_STAGE_EXEC		7
#define XPROC_STAGE_WAIT		8
```

停止原因：

```c
#define XPROC_STOP_NONE			0
#define XPROC_STOP_INTERRUPT	1
#define XPROC_STOP_TERMINATE	2
#define XPROC_STOP_KILL			3
#define XPROC_STOP_KILL_TREE	4
```

常见判断方式：

- `iKind == XPROC_EXIT_SPAWN_FAILED`
	- 子进程根本没有成功进入运行态
	- 这时重点看 `iStage / iOsError`
- `bTimedOut == true`
	- `xrtExecCapture()` 的等待超时并触发了内部收口
- `iKind == XPROC_EXIT_SIGNAL`
	- POSIX 下被信号结束
- `bCancelled == true`
	- 调用方主动发起过停止请求


### 3.3 `xprocessreadinfo`

```c
typedef struct {
	uint64 iBaseOffset;
	uint64 iNextOffset;
	uint64 iStreamEndOffset;
	bool bDone;
} xprocessreadinfo;
```

它用于描述一次 `ReadSince` 调用后的偏移状态：

- `iBaseOffset`
	- 当前缓冲还保留的数据起始偏移
- `iNextOffset`
	- 下一次继续读取时应使用的偏移
- `iStreamEndOffset`
	- 调用当下流尾偏移
- `bDone`
	- 对应流是否已经结束

如果你传入的 offset 早于 `iBaseOffset`，说明较早的数据已经不在当前保留窗口里了。


### 3.4 `xprocessevent`

```c
typedef struct {
	uint64 iSeq;
	int iKind;
	int iStream;
	uint64 iOffset;
	uint64 iSize;
	uint64 tTimeMs;
	xprocessexitinfo ExitInfo;
} xprocessevent;
```

它表示统一事件时间线里的单条记录：

- `iSeq`
	- 全局递增序号
- `iKind`
	- `START / OUTPUT / EXIT`
- `iStream`
	- `STDOUT / STDERR / TERMINAL`
- `iOffset / iSize`
	- 对应输出块在流内的偏移和长度
- `ExitInfo`
	- 只有 `EXIT` 事件有意义


### 3.5 `xprocesseventreadinfo`

```c
typedef struct {
	uint64 iBaseSeq;
	uint64 iNextSeq;
	uint64 iEventEndSeq;
	bool bDone;
} xprocesseventreadinfo;
```

它用于描述一次 `ReadEventsSince` 调用后的事件窗口状态。


### 3.6 `xprocessconfig`

```c
typedef struct {
	int iTargetKind;
	str sProgram;
	str* arrArgs;
	uint32 iArgCount;
	str sCommand;
	str sWorkDir;
	str* arrEnv;
	uint32 iEnvCount;
	bool bInheritEnv;
	bool bUseTerminal;
	bool bMergeStderr;
	bool bCreateProcessGroup;
	bool bHideWindow;
	uint32 iTerminalCols;
	uint32 iTerminalRows;
	uint32 iReadChunkSize;
	size_t iMaxCaptureBytes;
	uint32 iMaxEventCount;
	xprocessstdio Stdin;
	xprocessstdio Stdout;
	xprocessstdio Stderr;
	const xprocessevents* pEvents;
	ptr pUserData;
} xprocessconfig;
```

字段重点：

- `iTargetKind`
	- `EXEC` 或 `SHELL`
- `sProgram`
	- `EXEC` 时是目标程序
	- `SHELL` 时是可选 shell 程序
- `arrArgs / iArgCount`
	- 追加参数
- `sCommand`
	- shell 模式下的命令文本
- `bUseTerminal`
	- 请求 PTY / ConPTY 终端模式
	- 打开后 stdout 会变成 terminal 原始字节流
- `bMergeStderr`
	- 把 stderr 并入 stdout
- `iTerminalCols / iTerminalRows`
	- terminal 初始尺寸
- `bCreateProcessGroup`
	- 让 `Interrupt / KillTree` 更稳定作用于整组子进程
- `iMaxCaptureBytes`
	- 0 表示不限制
- `iMaxEventCount`
	- 统一事件时间线保留条数

推荐起点一定是：

```c
xrtProcessConfigInit(&tConfig);
```

默认值包括：

- `iTargetKind = XPROC_TARGET_EXEC`
- `bInheritEnv = true`
- `bCreateProcessGroup = true`
- `iTerminalCols = 120`
- `iTerminalRows = 30`
- `Stdin/Stdout/Stderr = INHERIT`


### 3.7 `xprocessresult`

```c
typedef struct {
	xprocessexitinfo ExitInfo;
	int iExitCode;
	ptr pStdout;
	size_t iStdoutSize;
	uint64 iStdoutBaseOffset;
	ptr pStderr;
	size_t iStderrSize;
	uint64 iStderrBaseOffset;
	bool bStdoutTruncated;
	bool bStderrTruncated;
} xprocessresult;
```

这是 `xrtExecCapture()` 的结果对象。  
使用完后统一调用：

```c
xrtProcessResultUnit(&tResult);
```


## 4. 事件回调

```c
typedef struct {
	void (*OnStart)(xprocess* pProcess, ptr pUserData);
	void (*OnStdout)(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData);
	void (*OnStderr)(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData);
	void (*OnExit)(xprocess* pProcess, const xprocessexitinfo* pExitInfo, ptr pUserData);
} xprocessevents;
```

注意：

- 回调运行在内部线程
- `OnExit` 拿到的是结构化退出信息，不再只是一个 `exit code`
- 如果同时需要流式消费和轮询读取，可以结合 `PIPE + 回调 + ReadSince`
- 如果需要统一时间线，优先用 `xrtProcessReadEventsSince()`


## 5. 公开 API

### 5.1 配置与启动

```c
XXAPI void xrtProcessConfigInit(xprocessconfig* pConfig);
XXAPI xprocess* xrtProcessSpawn(const xprocessconfig* pConfig);
XXAPI bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32 iTimeoutMs);
XXAPI void xrtProcessResultUnit(xprocessresult* pResult);
```

语义说明：

- `xrtProcessSpawn()`
	- 启动成功返回 `xprocess*`
	- 启动失败返回 `NULL`
- `xrtExecCapture()`
	- 成功启动并完成收口时返回 `true`
	- 启动失败或参数无效时返回 `false`
	- 即使发生超时，只要最终形成了有效结果，也会通过 `pResult->ExitInfo` 返回


### 5.2 运行态访问

```c
XXAPI int xrtProcessState(xprocess* pProcess);
XXAPI bool xrtProcessIsRunning(xprocess* pProcess);
XXAPI int xrtProcessExitCode(xprocess* pProcess);
XXAPI bool xrtProcessGetExitInfo(xprocess* pProcess, xprocessexitinfo* pInfo);
XXAPI bool xrtProcessTerminalSupported(void);

XXAPI int64 xrtProcessWrite(xprocess* pProcess, const void* pData, size_t iSize);
XXAPI int64 xrtProcessWriteText(xprocess* pProcess, str sText, size_t iSize);
XXAPI bool xrtProcessCloseStdin(xprocess* pProcess);

XXAPI bool xrtProcessResizeTerminal(xprocess* pProcess, uint32 iCols, uint32 iRows);

XXAPI ptr xrtProcessGetStdout(xprocess* pProcess, size_t* piSize);
XXAPI ptr xrtProcessGetStderr(xprocess* pProcess, size_t* piSize);
XXAPI ptr xrtProcessReadStdoutSince(xprocess* pProcess, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo);
XXAPI ptr xrtProcessReadStderrSince(xprocess* pProcess, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo);
XXAPI xprocessevent* xrtProcessReadEventsSince(xprocess* pProcess, uint64 iSeq, uint32 iMaxCount, uint32* piCount, xprocesseventreadinfo* pInfo);
```

要点：

- `xrtProcessWriteText(..., iSize = 0)` 会自动取字符串长度
- 很多程序要等到 EOF 才退出，所以写完 stdin 后通常还要 `xrtProcessCloseStdin()`
- `GetStdout/GetStderr` 返回完整快照副本
- `ReadStdoutSince/ReadStderrSince` 适合 agent 轮询式 streaming
- `ReadEventsSince` 适合还原统一输出时间线和生命周期事件
- terminal 模式下 stdout 保存的是原始 terminal 字节流，可能包含 VT 控制序列
- 这些读取 API 返回的内存都需要 `xrtFree()`


### 5.3 等待与停止

```c
XXAPI bool xrtProcessWait(xprocess* pProcess);
XXAPI int xrtProcessWaitTimeout(xprocess* pProcess, uint32 iTimeoutMs);

XXAPI bool xrtProcessInterrupt(xprocess* pProcess);
XXAPI bool xrtProcessTerminate(xprocess* pProcess);
XXAPI bool xrtProcessKill(xprocess* pProcess);
XXAPI bool xrtProcessKillTree(xprocess* pProcess);

XXAPI void xrtProcessDestroy(xprocess* pProcess);
```

建议理解：

- `Interrupt`
	- 尽量先通知对方中断
	- Unix 走 `SIGINT`
	- Windows 走 `CTRL_BREAK_EVENT`
- `Terminate`
	- 尽量温和退出
- `Kill`
	- 直接强杀当前进程
- `KillTree`
	- 尽量强杀整棵进程树
- `Destroy`
	- 只负责释放对象
	- 不是 stop API

推荐收口顺序：

1. `WaitTimeout`
2. 必要时 `Interrupt`
3. 再不行 `Terminate`
4. 仍未退出时 `KillTree` 或 `Kill`
5. 最后 `Destroy`


### 5.4 Future

如果编译包含 future 层：

```c
XXAPI xfuture* xrtProcessWaitFuture(xprocess* pProcess);
```

它会在进程退出时 resolve 为对应的 `xprocess*`。


## 6. 推荐用法

### 6.1 一次性工具执行

- 用 `XPROC_TARGET_EXEC`
- 用 `xrtExecCapture()`
- 让 `stdout/stderr` 自动 capture

### 6.2 shell 命令执行

- 用 `XPROC_TARGET_SHELL`
- 把命令放进 `sCommand`
- 不要再自己手写 `cmd /c` 或 `/bin/sh -c`

### 6.3 agent 轮询输出

- `Stdout/Stderr.iMode = XPROC_STDIO_PIPE`
- 用 `xrtProcessReadStdoutSince()` / `xrtProcessReadStderrSince()`
- 保存 `iNextOffset` 作为下一轮 offset

### 6.4 带环境隔离的命令

- `bInheritEnv = false`
- 明确给出 `arrEnv`
- 避免依赖外层 shell 环境状态

### 6.5 terminal / shell 会话

- 先用 `xrtProcessTerminalSupported()` 判平台能力
- 设置 `bUseTerminal = true`
- 需要交互时把 `Stdin` 保持为可写，并用 `xrtProcessWriteText()` 发送命令
- terminal 输出是原始终端字节流，不是“纯文本 stdout”

### 6.6 统一事件时间线

- 用 `xrtProcessReadEventsSince()`
- 保存 `iNextSeq` 作为下一轮游标
- `OUTPUT` 事件的 `iStream` 可区分 `STDOUT / STDERR / TERMINAL`


## 7. 当前边界

- terminal 模式返回的是原始终端字节流，调用方如果要“纯文本”展示，应自行去掉 VT 控制序列
- 回调接口仍然是 `stdout / stderr` 两条通道；如果要统一时间线，使用 `xrtProcessReadEventsSince()`
- Windows terminal 依赖 ConPTY；不支持时 `xrtProcessTerminalSupported()` 会返回 `false`


## 8. 相关文档

- [Subprocess 入门](../guide/subprocess-intro.md)
- [Thread](api-thread.md)
- [Future / Task / Promise](api-future-task-promise.md)
- [OS](api-os.md)
