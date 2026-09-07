# Process API

`<xrt/process.h>` 提供直接执行、真实标准流管道、等待、退出状态和进程树控制。详细设计与平台约束见 [Process 设计](../design/process.md)。

## 快速开始

```c
xprocessconfig config;
xprocessstatus status;
xprocess* process;
const cstr args[] = { "--version" };

xrtProcessConfigInit(&config);
config.Program = "git";
config.Args = args;
config.ArgCount = 1;

process = xrtProcessSpawn(&config);
if ( process != NULL ) {
	xrtProcessWait(process);
	xrtProcessStatus(process, &status);
	xrtProcessDestroy(process);
}
```

直接执行不会经过 Shell。只有命令明确需要管道、重定向、变量展开或 Shell 内建语法时，才使用 `xrtProcessShellConfigInit()`。



## 使用默认程序打开

启用 `process_open` 后，`xrtProcessOpen()` 请求操作系统使用默认关联程序打开文件路径或 URI：

```c
if ( !xrtProcessOpen("https://example.com/") ) {
	const xerror* error = xrtGetError();

	fprintf(stderr, "%s\n", xrtErrorMessage(error));
}
```

Windows 使用 Shell 关联处理器，macOS 使用 `/usr/bin/open`，其他 POSIX 平台直接执行 `xdg-open`。目标始终作为独立参数传递，不经过命令解释器，因此不需要也不允许调用方自行拼接 shell 转义。

返回 `true` 只表示系统已经接受打开请求。关联程序可能复用已有进程，POSIX 桌面启动器也可能稍后失败，所以该入口不会伪造可等待的 `xprocess` 或“界面已经显示”的结果。空目标、非法 UTF-8、缺少桌面启动器和系统关联失败都进入 `xrt.process` 域的 `XPROCESS_ERROR_OPEN`。



## 配置

`xrtProcessConfigInit()` 设置以下默认值：

- `Target = XPROCESS_EXEC`
- 继承当前环境
- 创建独立进程组
- 三个标准流都继承
- Terminal 默认尺寸为 120 x 30

`Args` 不包含 `argv[0]`，每项都是独立零结尾 UTF-8 字符串。`Arg0` 为空时使用 `Program`。全部配置数据只需存活到 `xrtProcessSpawn()` 返回。

环境使用 `xprocessenv` 的 Name/Value 表达。Value 为空指针表示删除变量，指向空字符串表示保留变量但设置空值；重复名称采用最后一项。



## 标准流

| 模式 | 含义 |
|---|---|
| `XPROCESS_IO_INHERIT` | 继承父进程对应标准流 |
| `XPROCESS_IO_PIPE` | 创建由父进程持有的真实管道 |
| `XPROCESS_IO_NULL` | 连接空设备 |
| `XPROCESS_IO_HANDLE` | Spawn 复制借用的原生句柄 |
| `XPROCESS_IO_MERGE` | 仅用于 stderr，连接到最终 stdout |

PIPE stdin 使用 `xrtProcessWrite()`，写完后必须用 `xrtProcessClose(..., XPROCESS_STDIN)` 发送 EOF。stdout 和 stderr 使用 `xrtProcessRead()`；返回零表示 EOF，负数表示结构化错误。

启用 `process_file` 后，`xrtProcessFile()` 把借用的 `xfile` 映射为 HANDLE 配置。Spawn 在返回前复制底层句柄，不接管原文件；因此 Spawn 成功后调用方可以立即关闭文件，子进程仍持有自己的副本。

Process 核心不缓存输出。需要并发排空和有界结果时选择 `process_run`，不要在子进程可能同时大量写 stdout/stderr 时顺序读两个流。



## Terminal

启用 `process_terminal` 后，把 `Terminal` 设为 true 即可使用 Windows ConPTY 或 POSIX PTY。`Columns` 与 `Rows` 的有效范围都是 1 到 32767；运行中使用 `xrtProcessResize()` 修改尺寸。启动前可用 `xrtProcessTerminalSupported()` 探测当前系统运行时能力。

Terminal 会取代三项标准流配置：父端通过 stdin 写入，通过 stdout 读取终端的统一字节流；子进程 stderr 也进入该字节流，因此 `xrtProcessStreamNative(..., XPROCESS_STDERR)` 返回 `-1`。终端输出可能包含输入回显、平台换行和程序产生的控制序列，调用方应按终端流而不是普通 stdout 文本处理。

POSIX PTY 的读端和写端共享同一个 master。普通 `xrtProcessClose()` 只关闭父端写描述符，
不等于向规范模式终端发送 EOF；手动交互程序应写入 EOT、发送退出命令或明确停止进程。
`xrtProcessRun()` 是有限输入的一次性路径，会在输入末尾补充平台规范终端 EOF 序列；
Windows 的 Ctrl-Z 必须位于新行起点，因此未以换行结束的输入会先补一个终端换行。
原始/非 processed 输入模式可能把控制字符作为普通输入，程序此时应使用退出命令或
普通管道。使用 Terminal 时 stderr 合并进 `Result.Stdout`，`Result.Stderr` 为空。



## 一次性运行

`xrtProcessRun()` 同时写入可选 stdin、并发排空 stdout/stderr、等待退出并返回有界结果。默认每个输出流最多保留 16 MiB；达到上限时可选择失败、保留开头或保留结尾。

```c
xprocessresult result;
const cstr args[] = { "--version" };

if ( xrtProcessCapture("git", args, 1u, &result) ) {
	fwrite(result.Stdout, 1u, result.StdoutSize, stdout);
	xrtProcessResultUnit(&result);
}
```

`xprocessresult.InputWritten` 表示 stdin 在子进程提前关闭前实际写入的字节数。基础设施成功与退出码为零是两个概念：`xrtProcessRun()` 的 true 只表示运行与收口成功，使用 `xrtProcessResultSuccess()` 判断正常零退出。

`xrtProcessWaitUntilCancel()` 在进程退出、绝对 Deadline 或取消令牌中等待第一个事件；进程退出与取消同时可见时，退出优先。超时与取消不会由该等待函数隐式停止进程。



## Pipeline

`xrtProcessPipeline()` 先创建 `N - 1` 条真实 OS pipe，再启动全部阶段。中间数据不经过父进程完整缓存，天然保留流式传输和内核背压；只捕获末段 stdout。

每个 `xprocessstageresult` 独立拥有对应阶段的 stderr 与退出状态，不会把多个工具的诊断文本混成无法定位的一块。`xprocesspipelineoutputproc` 同时收到阶段索引和流标识；回调可能由多个输出线程并发调用。

`xprocesspipelineoptions` 的 `Input` 只写入首段，Deadline 与 Cancel 对全部阶段共享。捕获上限分别应用于末段 stdout 和每一段 stderr。调用结束后使用 `xrtProcessPipelineResultUnit()` 释放全部结果。



## 等待与状态

- `xrtProcessWait()`：无限等待。
- `xrtProcessWaitFor()`：相对微秒数。
- `xrtProcessWaitUntil()`：绝对单调 Deadline。
- `xrtProcessStatus()`：成功等待后复制不可变状态。

超时不会自动杀死进程。调用方可以继续等待，或依次调用 Interrupt、Terminate、KillTree。发送请求和观察退出是两个独立步骤。

`xprocessstatus` 的 `Kind` 区分退出码、POSIX 信号和平台等待失败。`Stop` 保存 XRT 成功发出的最高停止强度。

`xrtProcessWaitAsync()` 返回每个 Process 唯一的共享 Future。Future 成功值是由 Future 自身拥有的只读 `xprocessstatus` 快照，因此调用方可以先释放 Process，再等待或读取结果。平台等待失败时 Future 进入失败终态并保留同一结构化错误。

对该共享 Future 请求取消不会隐式停止进程，也不会替其他观察者伪造取消终态；需要停止子进程时应明确调用 Process 停止 API。启用 `future_coroutine` 后，同一个 Future 可以直接由 `xrtFutureAwait()` 等待。



## 生命周期

Process 使用引用计数。`xrtProcessDestroy()` 可以在进程运行时调用；最后一个调用方引用会关闭父端管道，内部等待引用继续回收进程。此操作不会隐式杀死子进程。

多个线程可以等待同一对象。每个输出流只允许一个并发读取者，stdin 只允许一个并发写入者；不要在 I/O 进行时从其他线程关闭同一管道。



## 错误

API 失败时使用 `xrtGetError()` 读取 `xrt.process` 域错误。错误包含通用类别、Process 错误码、操作名和平台错误码。后台等待错误还可以通过 `xrtProcessError()` 取得新的错误引用。



### `xrtProcessConfigInit`

初始化直接执行配置，并启用继承环境与独立进程组。

```c
bool xrtProcessConfigInit(xprocessconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream](../../examples/process/stream/main.c) · 直接执行配置

```c
	if ( !xrtProcessConfigInit(&Config) ) {
```

### `xrtProcessShellConfigInit`

初始化系统 Shell 配置；`Command` 只借用到 Spawn 返回。

```c
bool xrtProcessShellConfigInit(
	xprocessconfig* pConfig,
	cstr sCommand
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |
| `sCommand` | 输入 | 非空、零结尾 | Shell 命令行 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file](../../examples/process/file/main.c) · Shell 配置

```c
		if ( !xrtProcessShellConfigInit(&Config, "echo redirected output") ) {
```

### `xrtProcessSpawn`

启动子进程；失败不返回半初始化对象，详情写入当前结构化错误。

```c
xprocess* xrtProcessSpawn(const xprocessconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空且通过校验 | 进程配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 进程对象（引用 1） | — |
| `NULL` | 启动失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` 域错误 — 启动失败：`ARGUMENT`（参数）、`CONFIG`（`XERR_VALUE`，配置组合）、`COMMAND`（程序路径）、`ENVIRONMENT`（`XERR_VALUE`，环境块）、`PIPE`（管道创建）、`SPAWN`（平台创建）

#### 范例

[stream](../../examples/process/stream/main.c) · 启动进程

```c
	pProcess = xrtProcessSpawn(&Config);
```

### `xrtProcessRef`

增加进程对象引用并返回原指针。

```c
xprocess* xrtProcessRef(xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/process/tour/main.c) · 共享引用

```c
	pRef = xrtProcessRef(pProcess);
```

### `xrtProcessDestroy`

释放进程对象引用；最后一个调用方引用可在进程运行时释放，此时关闭父端标准流并由内部等待者回收子进程，不隐式杀死子进程。

```c
void xrtProcessDestroy(xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[stream](../../examples/process/stream/main.c) · 释放引用

```c
		xrtProcessDestroy(pProcess);
```

### `xrtProcessState`

返回进程状态快照。

```c
xprocessstate xrtProcessState(const xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XPROCESS_RUNNING` | 尚未退出 | — |
| `XPROCESS_EXITED` | 已退出 | — |

#### 错误

- 无 — 快照读取不设置错误；空句柄返回 `XPROCESS_RUNNING`

#### 范例

[tour](../../examples/process/tour/main.c) · 状态快照

```c
		(xrtProcessState(pProcess) != XPROCESS_RUNNING) ||
```

### `xrtProcessId`

返回平台进程标识，失败返回零。

```c
uint64 xrtProcessId(const xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | 平台进程标识 | — |
| `0` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/process/tour/main.c) · 进程标识

```c
		(xrtProcessId(pProcess) == 0u) ||
```

### `xrtProcessNative`

返回借用的原生进程句柄；POSIX 返回 pid，Windows 返回 HANDLE。

```c
intptr_t xrtProcessNative(const xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 句柄 | 原生进程句柄借用 | — |
| `-1` 等异常值 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/process/tour/main.c) · 原生句柄

```c
		(xrtProcessNative(pProcess) == 0) ||
```

### `xrtProcessStreamNative`

返回借用的父端标准流句柄；未配置 PIPE 或已关闭时返回 -1。

```c
intptr_t xrtProcessStreamNative(
	const xprocess* pProcess,
	xprocessstream Stream
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |
| `Stream` | 输入 | — | 标准流选择 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 句柄 | 原生流句柄借用 | — |
| `-1` | 未配置 PIPE 或已关闭 | 不设错 |

#### 错误

- 无句柄时返回 `-1` 且不设置错误；空句柄 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/process/tour/main.c) · 标准流原生句柄

```c
		(xrtProcessStreamNative(pProcess,
			XPROCESS_STDOUT) == 0) ) {
```

### `xrtProcessStatus`

复制退出状态；进程尚未退出时返回 `false` 并设置状态错误。

```c
bool xrtProcessStatus(
	const xprocess* pProcess,
	xprocessstatus* pStatus
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |
| `pStatus` | 输出 | 非空 | 接收状态快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 状态已复制 | — |
| `false` | 尚未退出 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 进程尚未退出；句柄或输出为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/process/tour/main.c) · 退出状态

```c
		!xrtProcessStatus(pProcess, &Status) ||
```

### `xrtProcessError`

返回进程后台等待失败的新错误引用，没有后台错误时返回空；用后 `xrtErrorFree` 释放。

```c
xerror* xrtProcessError(const xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 错误新引用 | — |
| `NULL` | 无后台错误 | 不设错 |

#### 错误

- 无错误 — 尚无后台失败时返回 `NULL` 且不设置错误

#### 范例

[tour](../../examples/process/tour/main.c) · 后台错误

```c
		(xrtProcessError(pProcess) != NULL) ) {
```

### `xrtProcessRead`

从 stdout 或 stderr 管道同步读取；零表示 EOF，负数表示错误。同一标准流同一时刻只允许一个读取者。

```c
int64 xrtProcessRead(
	xprocess* pProcess,
	xprocessstream Stream,
	void* pData,
	size_t iSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |
| `Stream` | 输入 | STDOUT/ERR | 读取管道 |
| `pData` | 输出 | 非空 | 接收缓冲 |
| `iSize` | 输入 | > 0 | 缓冲容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 实际读取字节数 | — |
| `0` | EOF | — |
| `< 0` | 失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` / `XPROCESS_ERROR_READ` — 读取失败（含句柄或流配置非法 `XERR_ARGUMENT`）

#### 范例

[stream](../../examples/process/stream/main.c) · 同步读取

```c
	while ( (iRead = xrtProcessRead(
		pProcess,
		XPROCESS_STDOUT,
		pOutput,
		sizeof(pOutput)
	)) > 0 ) {
```

### `xrtProcessWrite`

向 stdin 管道同步写入，返回实际写入字节数，负数表示错误。函数可能部分写入；同一时刻只允许一个写入者。

```c
int64 xrtProcessWrite(
	xprocess* pProcess,
	const void* pData,
	size_t iSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |
| `pData` | 输入 | 非空 | 待写入数据 |
| `iSize` | 输入 | > 0 | 写入字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际写入字节数（可部分写入） | — |
| `< 0` | 失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` / `XPROCESS_ERROR_WRITE` — 写入失败（含参数非法 `XERR_ARGUMENT`）

#### 范例

[stream](../../examples/process/stream/main.c) · 同步写入

```c
	if ( xrtProcessWrite(pProcess, sInput, sizeof(sInput) - 1u) <= 0 ) {
```

### `xrtProcessClose`

关闭父进程持有的指定管道端；重复关闭成功。

```c
bool xrtProcessClose(
	xprocess* pProcess,
	xprocessstream Stream
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |
| `Stream` | 输入 | — | 要关闭的流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭（或本就关闭） | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.process` / `XPROCESS_ERROR_CLOSE` — 平台关闭失败

#### 范例

[stream](../../examples/process/stream/main.c) · 关闭管道端

```c
	(void)xrtProcessClose(pProcess, XPROCESS_STDIN);
```

### `xrtProcessWait`

等待进程退出。

```c
xwaitresult xrtProcessWait(xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 进程已退出，状态可读 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或等待失败 | `XERR_ARGUMENT` / `xrt.process` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.process` / `XPROCESS_ERROR_WAIT` — 平台等待失败

#### 范例

[file](../../examples/process/file/main.c) · 无限等待

```c
	if ( xrtProcessWait(pProcess) != XWAIT_OK ) {
```

### `xrtProcessWaitFor`

在相对微秒数内等待进程退出。

```c
xwaitresult xrtProcessWaitFor(
	xprocess* pProcess,
	uint64 iTimeout
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |
| `iTimeout` | 输入 | — | 相对等待微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 进程已退出，状态可读 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或等待失败 | `XERR_ARGUMENT` / `xrt.process` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.process` / `XPROCESS_ERROR_WAIT` — 平台等待失败

#### 范例

[tour](../../examples/process/tour/main.c) · 限时等待

```c
	if ( xrtProcessWaitFor(pProcess, 100000u) != XWAIT_TIMEOUT ) {
```

### `xrtProcessWaitUntil`

等待进程退出到指定单调时钟截止时间。

```c
xwaitresult xrtProcessWaitUntil(
	xprocess* pProcess,
	xdeadline iDeadline
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |
| `iDeadline` | 输入 | — | 截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 进程已退出，状态可读 | — |
| `XWAIT_TIMEOUT` | 期限或截止时间先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或等待失败 | `XERR_ARGUMENT` / `xrt.process` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.process` / `XPROCESS_ERROR_WAIT` — 平台等待失败

#### 范例

[tour](../../examples/process/tour/main.c) · 限期等待

```c
		(xrtProcessWaitUntil(pProcess,
			xrtDeadlineAfter(UINT64_C(2000000))) !=
			XWAIT_OK) ) {
```

### `xrtProcessWaitUntilCancel`

等待进程、Deadline 或取消令牌中的首个事件。

```c
xwaitresult xrtProcessWaitUntilCancel(
	xprocess* pProcess,
	xdeadline iDeadline,
	xcancel* pCancel
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |
| `iDeadline` | 输入 | — | 截止时间 |
| `pCancel` | 输入 | 允许空 | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWAIT_OK` | 进程已退出，状态可读 | — |
| `XWAIT_TIMEOUT` | 期限先到达 | 不设错误 |
| `XWAIT_ERROR` | 参数或等待失败 | `XERR_ARGUMENT` / `xrt.process` 域错误 |
| `XWAIT_CANCELLED` | 取消令牌触发 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.process` / `XPROCESS_ERROR_WAIT` — 平台等待失败

#### 范例

[tour](../../examples/process/tour/main.c) · 可取消等待

```c
		(xrtProcessWaitUntilCancel(pProcess,
			xrtDeadlineAfter(UINT64_C(3000000)),
			pCancel) != XWAIT_CANCELLED) ) {
```

### `xrtProcessInterrupt`

请求控制台中断或 POSIX SIGINT，不等待进程退出。

```c
bool xrtProcessInterrupt(xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 请求已发出 | — |
| `false` | 失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` / `XPROCESS_ERROR_SIGNAL` — 平台信号发送失败；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/process/tour/main.c) · 请求中断

```c
	if ( !xrtProcessInterrupt(pProcess) ) {
```

### `xrtProcessTerminate`

请求温和终止并关闭 stdin，不等待进程退出。

```c
bool xrtProcessTerminate(xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 请求已发出 | — |
| `false` | 失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` / `XPROCESS_ERROR_SIGNAL` — 平台终止失败；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/process/tour/main.c) · 温和终止

```c
	if ( !xrtProcessTerminate(pProcess) ||
		!xrtProcessKill(pProcess) ||
		(xrtProcessWaitUntil(pProcess,
			xrtDeadlineAfter(UINT64_C(2000000))) !=
			XWAIT_OK) ) {
```

### `xrtProcessKill`

强制结束根进程，不等待进程退出。

```c
bool xrtProcessKill(xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 请求已发出 | — |
| `false` | 失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` / `XPROCESS_ERROR_SIGNAL` — 平台结束失败；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/process/tour/main.c) · 强制结束

```c
		!xrtProcessKill(pProcess) ||
```

### `xrtProcessKillTree`

强制结束创建时进程组中的完整进程树，不等待进程退出。

```c
bool xrtProcessKillTree(xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 请求已发出 | — |
| `false` | 失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` / `XPROCESS_ERROR_SIGNAL` — 平台结束失败；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/process/tour/main.c) · 结束进程树

```c
			xrtProcessKillTree(pVictim) &&
```

### `xrtProcessOpen`

请求系统使用默认关联程序打开 UTF-8 文件路径或 URI；返回 `true` 只表示系统接受请求，不表示目标应用已经完成展示。

```c
bool xrtProcessOpen(cstr sTarget)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sTarget` | 输入 | 非空、零结尾 | 文件路径或 URI |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 系统已接受请求 | — |
| `false` | 失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` / `XPROCESS_ERROR_OPEN` — 系统打开请求失败；参数非法 `XERR_ARGUMENT`

#### 范例

[open](../../examples/process/open/main.c) · 默认程序打开

```c
	if ( !xrtProcessOpen(argv[1]) ) {
```

### `xrtProcessFile`

从借用的 XRT 文件构造 HANDLE 标准流配置。

```c
xprocessio xrtProcessFile(xfile File)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 打开的文件句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| IO 配置 | 可赋给 `xprocessconfig` 的流配置 | — |
| 无效配置 | 文件为空 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file](../../examples/process/file/main.c) · 文件标准流

```c
	Config.Stdout = xrtProcessFile(File);
```

### `xrtProcessTerminalSupported`

判断当前系统是否具备 ConPTY 或 POSIX PTY 支持。

```c
bool xrtProcessTerminalSupported(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否支持终端 | — |

#### 错误

- 无 — 纯能力查询，不设置错误

#### 范例

[terminal](../../examples/process/terminal/main.c) · 终端能力

```c
	if ( !xrtProcessTerminalSupported() ) {
```

### `xrtProcessResize`

调整 Terminal 进程窗口并通知子进程。

```c
bool xrtProcessResize(
	xprocess* pProcess,
	uint32 iColumns,
	uint32 iRows
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空、Terminal 进程 | 目标进程 |
| `iColumns` | 输入 | > 0 | 列数 |
| `iRows` | 输入 | > 0 | 行数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已调整并通知 | — |
| `false` | 失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` / `XPROCESS_ERROR_TERMINAL` — 非终端进程或平台调整失败；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/process/tour/main.c) · 调整终端窗口

```c
				(void)xrtProcessResize(pTerm, 120u, 30u);
```

### `xrtProcessRunOptionsInit`

初始化有界捕获、无限等待和 250 ms 分级停止宽限。

```c
bool xrtProcessRunOptionsInit(xprocessrunoptions* pOptions)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOptions` | 输出 | 非空 | 接收选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/process/tour/main.c) · 运行选项

```c
	if ( !xrtProcessRunOptionsInit(&RunOptions) ||
		!xrtProcessRun(&Config, &RunOptions, &Result) ||
		!xrtProcessResultSuccess(&Result) ||
		(Result.StdoutSize < 4u) ||  /* "ping\r\n" 至少 4 */
		(Result.Stdout == NULL) ) {
```

### `xrtProcessResultUnit`

释放结果持有的输出并恢复为空结果。

```c
void xrtProcessResultUnit(xprocessresult* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResult` | 输入/输出 | 非空 | 目标结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放并清空 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[capture](../../examples/process/capture/main.c) · 释放结果

```c
	xrtProcessResultUnit(&Result);
```

### `xrtProcessResultSuccess`

判断进程是否在未超时、未取消条件下以退出码零正常结束。

```c
bool xrtProcessResultSuccess(const xprocessresult* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResult` | 输入 | 非空 | 目标结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否正常零退出 | — |

#### 错误

- 无 — 纯判断，不设置错误

#### 范例

[capture](../../examples/process/capture/main.c) · 成功判断

```c
	bOk = xrtProcessResultSuccess(&Result);
```

### `xrtProcessRun`

启动、并发排空 stdout/stderr、写入输入、等待并收口结果；返回 `false` 只表示基础设施失败，非零退出码仍返回 `true`。

```c
bool xrtProcessRun(
	const xprocessconfig* pConfig,
	const xprocessrunoptions* pOptions,
	xprocessresult* pResult
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空且通过校验 | 进程配置 |
| `pOptions` | 输入 | 允许空 | 空 = 默认选项 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已收口结果（含非零退出码） | — |
| `false` | 基础设施失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` 域错误 — 启动失败：`ARGUMENT`（参数）、`CONFIG`（`XERR_VALUE`，配置组合）、`COMMAND`（程序路径）、`ENVIRONMENT`（`XERR_VALUE`，环境块）、`PIPE`（管道创建）、`SPAWN`（平台创建）
- `xrt.process` / `XPROCESS_ERROR_WAIT`、`READ`、`WRITE` — 收口或排空失败

#### 范例

[tour](../../examples/process/tour/main.c) · 一次性运行

```c
		!xrtProcessRun(&Config, &RunOptions, &Result) ||
```

### `xrtProcessCapture`

直接执行程序并使用默认有界捕获策略。

```c
bool xrtProcessCapture(
	cstr sProgram,
	const cstr* pArgs,
	size_t iArgCount,
	xprocessresult* pResult
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sProgram` | 输入 | 非空、零结尾 | 程序路径 |
| `pArgs` | 输入 | 允许空 | 参数数组 |
| `iArgCount` | 输入 | — | 参数数量 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已收口结果（含非零退出码） | — |
| `false` | 基础设施失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` 域错误 — 启动失败：`ARGUMENT`（参数）、`CONFIG`（`XERR_VALUE`，配置组合）、`COMMAND`（程序路径）、`ENVIRONMENT`（`XERR_VALUE`，环境块）、`PIPE`（管道创建）、`SPAWN`（平台创建）
- `xrt.process` / `XPROCESS_ERROR_WAIT`、`READ` — 收口失败

#### 范例

[tour](../../examples/process/tour/main.c) · 直接执行捕获

```c
	if ( !xrtProcessCapture("cmd", arrArgs, 1u, &Result) ||
		!xrtProcessResultSuccess(&Result) ||
		(Result.StderrSize != 0u) ) {
```

### `xrtProcessShell`

通过系统 Shell 执行命令并使用默认有界捕获策略。

```c
bool xrtProcessShell(
	cstr sCommand,
	xprocessresult* pResult
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sCommand` | 输入 | 非空、零结尾 | Shell 命令行 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已收口结果（含非零退出码） | — |
| `false` | 基础设施失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` 域错误 — 启动失败：`ARGUMENT`（参数）、`CONFIG`（`XERR_VALUE`，配置组合）、`COMMAND`（程序路径）、`ENVIRONMENT`（`XERR_VALUE`，环境块）、`PIPE`（管道创建）、`SPAWN`（平台创建）
- `xrt.process` / `XPROCESS_ERROR_WAIT`、`READ` — 收口失败

#### 范例

[capture](../../examples/process/capture/main.c) · Shell 捕获

```c
		bOk = xrtProcessShell("echo captured output", &Result);
```

### `xrtProcessPipelineOptionsInit`

初始化 Pipeline 的有界捕获、无限等待和 250 ms 停止宽限。

```c
bool xrtProcessPipelineOptionsInit(
	xprocesspipelineoptions* pOptions
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOptions` | 输出 | 非空 | 接收选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/process/tour/main.c) · Pipeline 选项

```c
	if ( !xrtProcessPipelineOptionsInit(&PipeOptions) ) {
```

### `xrtProcessPipelineResultUnit`

释放 Pipeline 结果持有的状态数组和输出。

```c
void xrtProcessPipelineResultUnit(xprocesspipelineresult* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResult` | 输入/输出 | 非空 | 目标结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放并清空 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[pipeline](../../examples/process/pipeline/main.c) · 释放 Pipeline 结果

```c
	xrtProcessPipelineResultUnit(&Result);
```

### `xrtProcessPipelineSuccess`

判断全部阶段是否都以退出码零正常结束。

```c
bool xrtProcessPipelineSuccess(
	const xprocesspipelineresult* pResult
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResult` | 输入 | 非空 | 目标结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否全部零退出 | — |

#### 错误

- 无 — 纯判断，不设置错误

#### 范例

[pipeline](../../examples/process/pipeline/main.c) · Pipeline 成功判断

```c
	bOk = xrtProcessPipelineSuccess(&Result);
```

### `xrtProcessPipeline`

并发启动真实 OS 管道连接的全部阶段并按一个 Deadline 收口。

```c
bool xrtProcessPipeline(
	const xprocessconfig* pStages,
	size_t iStageCount,
	const xprocesspipelineoptions* pOptions,
	xprocesspipelineresult* pResult
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStages` | 输入 | 非空数组 | 各阶段配置 |
| `iStageCount` | 输入 | > 1 | 阶段数量 |
| `pOptions` | 输入 | 允许空 | 空 = 默认选项 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已收口结果（含非零退出码） | — |
| `false` | 基础设施失败 | `xrt.process` 域错误 |

#### 错误

- `xrt.process` 域错误 — 启动失败：`ARGUMENT`（参数）、`CONFIG`（`XERR_VALUE`，配置组合）、`COMMAND`（程序路径）、`ENVIRONMENT`（`XERR_VALUE`，环境块）、`PIPE`（管道创建）、`SPAWN`（平台创建）
- `xrt.process` / `XPROCESS_ERROR_PIPE` — 阶段间管道连接失败
- `xrt.process` / `XPROCESS_ERROR_WAIT`、`READ` — 收口失败

#### 范例

[pipeline](../../examples/process/pipeline/main.c) · 执行 Pipeline

```c
	bOk = xrtProcessPipeline(Stages, 2u, NULL, &Result);
```

### `xrtProcessWaitAsync`

返回以 Future 形式等待进程退出的对象；成功值是由 Future 自身拥有的只读 `xprocessstatus` 快照。

```c
xfuture* xrtProcessWaitAsync(xprocess* pProcess)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pProcess` | 输入 | 非空 | 目标进程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Future，失败时完成并携带错误 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — Future 分配失败

#### 范例

[future](../../examples/process/future/main.c) · Future 等待

```c
	pFuture = xrtProcessWaitAsync(pProcess);
```

## 公共类型索引

| 类型 | 用途 |
|---|---|
| `xprocesserror` | `xrt.process` 域内的稳定错误代码。 |
| `xprocesstarget` | 区分直接执行与显式 Shell 命令。 |
| `xprocessiomode` | 选择标准流继承、管道、空设备、借用句柄或合并。 |
| `xprocessstream` | 标识 stdin、stdout 或 stderr。 |
| `xprocessstate` | 表示进程仍在运行或已经进入终态。 |
| `xprocessexitkind` | 区分正常退出码、信号退出和等待状态丢失。 |
| `xprocessstop` | 记录 XRT 成功发出的最高停止强度。 |
| `xprocessio` | 保存一个标准流模式及可选借用原生句柄。 |
| `xprocessenv` | 描述一项环境覆盖或删除操作。 |
| `xprocessstatus` | 保存不可变退出种类、代码、信号和停止来源。 |
| `xprocessconfig` | 保存启动目标、参数、环境、目录、终端和标准流配置。 |
| `xprocess` | 引用计数管理的不透明进程对象。 |
| `xprocessoverflow` | 选择捕获超限时失败、保留开头或保留结尾。 |
| `xprocessoutputproc` | 接收 Run 产生的临时输出块。 |
| `xprocessrunoptions` | 保存输入、Deadline、取消、停止宽限和捕获策略。 |
| `xprocessresult` | 拥有单次运行的退出状态和两项有界输出。 |
| `xprocesspipelineoutputproc` | 接收带阶段索引的 Pipeline 输出块。 |
| `xprocesspipelineoptions` | 保存整条 Pipeline 的输入和收口策略。 |
| `xprocessstageresult` | 拥有一个 Pipeline 阶段的状态和 stderr。 |
| `xprocesspipelineresult` | 拥有全部阶段结果和末段 stdout。 |



## 状态与策略常量

`xprocessstate`：

| 常量 | 含义 |
|---|---|
| `XPROCESS_RUNNING` | 进程尚未发布退出状态。 |
| `XPROCESS_EXITED` | 退出状态已经发布并保持不变。 |

`xprocessexitkind`：

| 常量 | 含义 |
|---|---|
| `XPROCESS_EXIT_NONE` | 尚无退出结果。 |
| `XPROCESS_EXIT_CODE` | 正常退出，读取 `Code`。 |
| `XPROCESS_EXIT_SIGNAL` | POSIX 信号退出，读取 `Signal`。 |
| `XPROCESS_EXIT_LOST` | 平台等待失败，读取 Process 错误。 |

`xprocessstop`：

| 常量 | 含义 |
|---|---|
| `XPROCESS_STOP_NONE` | XRT 没有发出停止请求。 |
| `XPROCESS_STOP_INTERRUPT` | 已发出交互中断。 |
| `XPROCESS_STOP_TERMINATE` | 已发出温和终止。 |
| `XPROCESS_STOP_KILL` | 已强制结束根进程。 |
| `XPROCESS_STOP_KILL_TREE` | 已强制结束 XRT 创建的进程组。 |

`xprocessoverflow`：

| 常量 | 含义 |
|---|---|
| `XPROCESS_OVERFLOW_ERROR` | 捕获达到上限后以限制错误收口。 |
| `XPROCESS_OVERFLOW_KEEP_FIRST` | 继续排空，只保留输出开头。 |
| `XPROCESS_OVERFLOW_KEEP_LAST` | 继续排空，使用滑动窗口保留结尾。 |

`XPROCESS_CAPTURE_LIMIT_DEFAULT` 为 stdout 和 stderr 分别设置 16 MiB 默认捕获上限。`XPROCESS_STDOUT` 是 stdout 流标识；另外两项流标识 `XPROCESS_STDIN` 与 `XPROCESS_STDERR` 的读写限制见“标准流”。



## 错误代码索引

| 常量 | 失败范围 |
|---|---|
| `XPROCESS_ERROR_ARGUMENT` | 公共指针、枚举或长度参数无效。 |
| `XPROCESS_ERROR_CONFIG` | 启动选项组合或工作目录无效。 |
| `XPROCESS_ERROR_COMMAND` | 程序、参数或 Shell 命令无法表达。 |
| `XPROCESS_ERROR_ENVIRONMENT` | 环境读取、转换或覆盖失败。 |
| `XPROCESS_ERROR_PIPE` | 标准流、错误通道或 Pipeline 管道失败。 |
| `XPROCESS_ERROR_SPAWN` | 平台创建或子进程启动阶段失败。 |
| `XPROCESS_ERROR_OPEN` | 默认关联程序拒绝文件路径或 URI。 |
| `XPROCESS_ERROR_WAIT` | 等待或退出状态采集失败。 |
| `XPROCESS_ERROR_READ` | stdout、stderr 或终端读取失败。 |
| `XPROCESS_ERROR_WRITE` | stdin 或 Pipeline 输入写入失败。 |
| `XPROCESS_ERROR_CLOSE` | 父端管道关闭失败。 |
| `XPROCESS_ERROR_SIGNAL` | 中断、终止或强杀请求失败。 |
| `XPROCESS_ERROR_CALLBACK` | 输出观察回调拒绝继续执行。 |
| `XPROCESS_ERROR_TERMINAL` | ConPTY/PTY 能力、启动或尺寸操作失败。 |
| `XPROCESS_ERROR_THREAD` | 内部等待或并发排空线程失败。 |
| `XPROCESS_ERROR_LIMIT` | 命令、捕获或平台资源达到显式上限。 |



## 辅助函数索引

| 函数 | 契约 |
|---|---|
| `xrtProcessRef()` | 增加一个调用方引用；失败返回空。 |
| `xrtProcessState()` | 返回锁保护的运行/退出状态快照。 |
| `xrtProcessId()` | 返回跨平台无符号进程标识，失败返回零。 |
| `xrtProcessNative()` | 返回借用的 HANDLE 或 pid，不转移所有权。 |
| `xrtProcessInterrupt()` | 请求交互中断，不等待退出。 |
| `xrtProcessTerminate()` | 请求温和终止并关闭 stdin，不等待退出。 |
| `xrtProcessKill()` | 强制结束根进程，不等待退出。 |
| `xrtProcessKillTree()` | 强制结束创建时的 Job 或进程组，不扫描系统进程表。 |
| `xrtProcessRunOptionsInit()` | 初始化无限 Deadline、默认捕获上限和 250 ms 停止宽限。 |
| `xrtProcessShell()` | 使用默认有界策略执行明确的 Shell 命令。 |
| `xrtProcessPipelineOptionsInit()` | 初始化 Pipeline 的输入、等待和捕获策略。 |
| `xrtProcessPipelineSuccess()` | 仅在全部阶段均以退出码零正常结束时返回 true。 |
