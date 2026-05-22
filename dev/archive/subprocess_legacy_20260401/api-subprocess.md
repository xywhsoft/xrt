# Subprocess 子进程模块

> 当前 XRT 的 `subprocess` 能力面向“启动外部程序、流式消费输出、写入 stdin、同步或异步等待退出”这条工具链主线。

[返回索引](README.md)

---

## 1. 定位

这组 API 主要解决：

- 启动外部程序
- 直接传 `argv` 或切到 shell 模式
- 读取 stdout / stderr
- 向 stdin 写入输入
- 等待退出并拿退出码
- 把“进程完成”接到 `future` 链路

它更适合：

- CLI 工具编排
- 构建脚本
- agent 工作流
- 需要流式消费子工具输出的后台任务

如果你只是：

- 让系统打开一个文件
- 启动默认程序
- 单纯同步执行一个命令

优先先看：

- [OS](api-os.md)


## 2. 可用性与边界

当前源码主线里，先记住下面 8 条：

1. `subprocess` 依赖线程能力；裁掉线程支持时，这组 API 会失败并写入线程相关错误。
2. `xrtProcessWaitFuture()` 依赖 `future` 层；如果相应能力未编译进来，这个入口不会形成完整主线。
3. `xrtExecCapture()` 是“一次性执行 + 自动等待 + 自动搬出 stdout/stderr 缓冲”的便捷入口。
4. `xrtProcessSpawn()` 才是流式、交互式、显式生命周期控制的正式入口。
5. `xrtProcessDestroy()` 不是 kill；它负责释放已经结束或由调用方不再持有的进程对象。
6. `xrtExecCapture(..., iTimeoutMs = 0)` 当前会按“无限等待”处理，不是“立刻超时”。
7. `xrtProcessGetStdout()` / `xrtProcessGetStderr()` 返回的是进程对象内部缓存指针，不是新复制出来的堆对象。
8. `OnStart / OnStdout / OnStderr / OnExit` 回调运行在内部工作线程上，不会自动切回调用线程。


## 3. 状态与 Flags

### 3.1 进程状态

```c
#define XPROC_STATE_FAILED   -1
#define XPROC_STATE_INIT      0
#define XPROC_STATE_RUNNING   1
#define XPROC_STATE_EXITED    2
#define XPROC_STATE_CLOSED    3
```

推荐理解：

- `INIT`
	- 对象已创建，尚未稳定进入运行态
- `RUNNING`
	- 子进程还没退出
- `EXITED`
	- 已退出，可以读取 exit code / captured output
- `CLOSED`
	- 对象已经进入关闭收口
- `FAILED`
	- 启动或内部状态失败


### 3.2 启动 Flags

```c
#define XPROC_F_USE_SHELL     0x0001u
#define XPROC_F_PIPE_STDIN    0x0002u
#define XPROC_F_PIPE_STDOUT   0x0004u
#define XPROC_F_PIPE_STDERR   0x0008u
#define XPROC_F_MERGE_STDERR  0x0010u
#define XPROC_F_HIDE_WINDOW   0x0020u
#define XPROC_F_NO_CAPTURE    0x0040u
#define XPROC_F_KILL_TREE     0x0080u
```

使用建议：

- `XPROC_F_USE_SHELL`
	- 从 direct argv 切到 shell 命令行模式；默认能不用就别用
- `XPROC_F_PIPE_STDIN`
	- 需要交互输入时再开
- `XPROC_F_PIPE_STDOUT` / `XPROC_F_PIPE_STDERR`
	- 需要读输出时打开
- `XPROC_F_MERGE_STDERR`
	- 把 stderr 合并进 stdout
- `XPROC_F_HIDE_WINDOW`
	- Windows 下隐藏子窗口
- `XPROC_F_NO_CAPTURE`
	- 只走流式回调，不保留内部捕获缓冲
- `XPROC_F_KILL_TREE`
	- 结束时尽量连子进程树一起收掉


## 4. 核心类型

### `xprocess`

运行中的子进程句柄。  
这是不透明对象，调用方通过 API 访问它。


### `xprocessevents`

```c
typedef struct {
	void (*OnStart)(xprocess* pProcess, ptr pUserData);
	void (*OnStdout)(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData);
	void (*OnStderr)(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData);
	void (*OnExit)(xprocess* pProcess, int iExitCode, ptr pUserData);
} xprocessevents;
```

适合：

- 流式打印
- 增量解析 stdout/stderr
- 在退出时触发后续逻辑

注意：

- 回调线程不是调用线程
- 回调里不要默认假设自己已经附着到你自己的调度模型


### `xprocessconfig`

```c
typedef struct {
	uint32 iFlags;
	str sProgram;
	str* arrArgs;
	uint32 iArgCount;
	str sCommandLine;
	str sWorkDir;
	uint32 iReadChunkSize;
	size_t iMaxCaptureBytes;
	const xprocessevents* pEvents;
	ptr pUserData;
} xprocessconfig;
```

字段重点：

- `sProgram`
	- direct argv 模式下的程序名
- `arrArgs` / `iArgCount`
	- direct argv 参数数组
- `sCommandLine`
	- shell 模式下的完整命令行
- `sWorkDir`
	- 工作目录
- `iReadChunkSize`
	- 内部管道分块读取大小
- `iMaxCaptureBytes`
	- 最大捕获字节数；`0` 表示不限制
- `pEvents`
	- 回调集合
- `pUserData`
	- 透传给回调


### `xprocessresult`

```c
typedef struct {
	int iExitCode;
	ptr pStdout;
	size_t iStdoutSize;
	ptr pStderr;
	size_t iStderrSize;
	bool bStdoutTruncated;
	bool bStderrTruncated;
} xprocessresult;
```

这是 `xrtExecCapture()` 的结果对象。  
内部 `stdout / stderr` 缓冲会被搬到这里，使用完后统一：

- `xrtProcessResultUnit()`


## 5. 公开 API

### 5.1 配置与启动

```c
XXAPI void xrtProcessConfigInit(xprocessconfig* pConfig);
XXAPI xprocess* xrtProcessSpawn(const xprocessconfig* pConfig);
XXAPI bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32 iTimeoutMs);
XXAPI void xrtProcessResultUnit(xprocessresult* pResult);
```

补充说明：

- `xrtProcessConfigInit()` 应作为配置对象起点。
- `xrtProcessSpawn()` 返回 `NULL` 表示启动失败。
- `xrtExecCapture()` 内部会强制打开 stdout capture，并在未设置 `MERGE_STDERR` 时自动打开 stderr capture。
- `xrtExecCapture()` 成功时会把内部缓冲移动到 `xprocessresult`，调用方需要 `xrtProcessResultUnit()` 收口。
- `xrtExecCapture()` 的 `iTimeoutMs == 0` 当前按无限等待处理。


### 5.2 运行态访问

```c
XXAPI int xrtProcessState(xprocess* pProcess);
XXAPI bool xrtProcessIsRunning(xprocess* pProcess);
XXAPI int xrtProcessExitCode(xprocess* pProcess);

XXAPI int64 xrtProcessWrite(xprocess* pProcess, const void* pData, size_t iSize);
XXAPI int64 xrtProcessWriteText(xprocess* pProcess, str sText, size_t iSize);
XXAPI bool xrtProcessCloseStdin(xprocess* pProcess);

XXAPI ptr xrtProcessGetStdout(xprocess* pProcess, size_t* piSize);
XXAPI ptr xrtProcessGetStderr(xprocess* pProcess, size_t* piSize);
```

补充说明：

- `xrtProcessWriteText()` 里 `iSize == 0` 时，按当前字符串长度处理是更自然的调用方式。
- 很多命令行程序会在收到 EOF 前一直等输入，因此写完 stdin 后通常要记得 `xrtProcessCloseStdin()`。
- `xrtProcessGetStdout()` / `xrtProcessGetStderr()` 返回的是内部缓存地址；生命周期受 `xprocess` 对象约束，不要自行释放。
- 如果启用了 `XPROC_F_NO_CAPTURE`，就不要指望这些 getter 还能给你完整输出。


### 5.3 等待与终止

```c
XXAPI bool xrtProcessWait(xprocess* pProcess);
XXAPI int xrtProcessWaitTimeout(xprocess* pProcess, uint32 iTimeoutMs);
XXAPI bool xrtProcessTerminate(xprocess* pProcess);
XXAPI bool xrtProcessKillTree(xprocess* pProcess);
XXAPI void xrtProcessDestroy(xprocess* pProcess);
```

补充说明：

- `xrtProcessWait()` 等价于无限等待直到退出。
- `xrtProcessWaitTimeout()` 返回等待状态码，当前主线按 `XRT_WAIT_OK / XRT_WAIT_TIMEOUT / XRT_WAIT_ERROR` 理解。
- `xrtProcessTerminate()` 是结束当前进程。
- `xrtProcessKillTree()` 更适合带子 worker 的工具链。
- `xrtProcessDestroy()` 不负责替你强杀进程；推荐顺序通常是：
	1. `Wait/WaitTimeout`
	2. 必要时 `KillTree/Terminate`
	3. 最后 `Destroy`


### 5.4 Future 入口

```c
XXAPI xfuture* xrtProcessWaitFuture(xprocess* pProcess);
```

补充说明：

- 这个 future 在完成时，value 当前会 resolve 成 `pProcess` 本身。
- 它适合接到 `xFutureWait*()`、`xFutureWhenAll()`、continuation 或 task group 链路。
- 如果进程已经退出，再取 wait future，也会得到可解析的完成态 future。


## 6. 推荐用法

### 6.1 短命令、整包抓取

优先：

- `xrtExecCapture()`

适合：

- `git status`
- `ffprobe`
- 轻量工具调用


### 6.2 长命令、流式输出、交互 stdin

优先：

- `xrtProcessSpawn()`

适合：

- 编译器
- LLM CLI
- 长时间运行的子 worker


### 6.3 需要和异步主线组合

优先：

- `xrtProcessWaitFuture()`

适合：

- `subprocess + future`
- `subprocess + task group`
- `subprocess + file_async`


## 7. 常见错误

1. 把 `xrtProcessDestroy()` 当成 kill。
2. 写完 stdin 忘记 `xrtProcessCloseStdin()`。
3. 明明只需要 direct argv，却先切到 shell 模式。
4. 在回调里直接操作线程不安全共享状态。
5. 用 `XPROC_F_NO_CAPTURE` 以后还去读 `GetStdout()/GetStderr()`。
6. 超时后只 `Destroy()`，却没先做 `Terminate()` / `KillTree()` 收口。


## 8. 相关阅读

- [XRT 子进程与工具执行入门](../guide/subprocess-intro.md)
- [OS](api-os.md)
- [Thread](api-thread.md)
- [Future / Task / Promise](api-future-task-promise.md)
- [用 Subprocess + File Async 写一个工具链流水线](../case/subprocess-file-async-pipeline.md)
