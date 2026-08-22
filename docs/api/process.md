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

POSIX PTY 的读端和写端共享同一个 master。关闭父端 stdin 写描述符不等于向规范模式终端发送 EOF；交互程序应写入 EOT、发送退出命令或明确停止进程。`xrtProcessRun()` 使用 Terminal 时把 stderr 合并进 Result.Stdout，Result.Stderr 为空。



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
