# Logger

Logger 体系以 `logger_core` 为最小层。核心只负责记录、过滤、结构化字段、Sink 组合、生命周期、错误和统计，不隐式打开控制台或文件。

## 最小用法

```c
xlogger* pLogger = xrtLogCreate(XRT_STR_LITERAL("service"), XLOG_INFO);
xlogsink* pSink = xrtLogSinkCreate(&Config);

xrtLogAttach(pLogger, pSink);
xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("started"));
xrtLogSinkFree(pSink);
xrtLogFree(pLogger);
```

`xrtLogAttach` 增加 Sink 引用，因此调用方可以在附加后立即释放自己的引用。一个 Sink 可以同时附加到多个 Logger。

## 结果

- `XLOG_RESULT_SKIPPED`：被 Logger 或 Sink 阈值过滤，或者 Logger 没有 Sink。
- `XLOG_RESULT_WRITTEN`：至少一个 Sink 完成写入。
- `XLOG_RESULT_DROPPED`：没有写入，但至少一个 Sink 按自身策略主动丢弃。
- `XLOG_RESULT_ERROR`：至少一个 Sink 发生真实错误；其余 Sink 仍会收到记录。

成功、过滤和主动丢弃不会覆盖调用者已有的线程错误。回调返回错误但没有设置具体错误时，核心建立 `xrt.log` 错误。

## 记录和字段

`xlogrecord`、`xlogfield` 中的视图和错误对象全部是借用值，只保证在提交调用期间有效。同步 Sink 必须在回调返回前消费数据；异步 Sink 必须深复制记录。

核心不强制验证 UTF-8。文本和 JSON 格式化层分别定义自己的编码契约，因此自定义二进制或转发 Sink 不会被核心限制。

## printf Helper

启用 `logger_printf` 后，可以使用 `xrtLogPrintf`、`xrtLogFieldsPrintf` 和 `xrtLogSourcePrintf`。这些函数复用字符串模块的安全格式化器，拒绝 `%n`，并以显式长度提交结果。已经得到消息视图时应继续使用无分配的 `xrtLog`，避免不必要的格式化和临时内存。

## 文本格式

`xrtLogTextConfigInit` 提供完整、简单和纯消息三种预设。`xrtLogTextWrite` 直接向同步字节 Writer 分段输出，不构造中间整行；默认把消息控制字符转义为单行文本，`XLOG_TEXT_RAW_MESSAGE` 可显式保留原始字节。

完整格式可包含固定 UTC 偏移时间、级别、Logger、源码、线程和全部结构化字段。字符串字段始终加引号并转义，错误字段保留 kind、domain、code 和 message。

启用独立的 `logger_format_text_buffer` 后，`xrtLogText` 返回由 `xrtFree` 释放的完整文本。控制台和文件 Sink 使用流式格式入口，不依赖该分配型 Helper；文件 Sink 会把分段结果写进自身有界复用缓冲，以便精确滚动并合并文件系统调用。

`ThreadId` 是可选元数据。轻量核心不会依赖完整线程模块自动读取线程标识；需要该字段时由调用方或线程集成 Helper 填入。

## JSON Lines 格式

`logger_format_json` 提供 `xrtLogJsonConfigInit` 和 `xrtLogJsonWrite`。基础入口直接向同步 Writer 分段写入，不创建 `xvalue`、JSON DOM、完整 JSON Writer 或中间整行缓冲；它只依赖共享 JSON 转义、整数和浮点格式化底座。

默认输出顺序稳定：`time`、`level`、`logger`、`message`、`source`、`thread`、`fields`。时间使用 Unix 微秒整数，避免格式化损耗和精度丢失；不存在的源码和线程元数据不会写出。默认以换行结束，可直接组成 JSON Lines 文件或流。

`FieldStyle` 有两种契约：

- `XLOG_JSON_FIELDS_OBJECT`：默认形式，字段名称直接作为对象成员，便于常规查询；重名字段按输入顺序写出，但下游对重复 JSON 名称的解释可能不同。
- `XLOG_JSON_FIELDS_ARRAY`：每项固定包含 `name`、`type` 和 `value`，无损保留重名字段以及 `int`、`uint`、`float`、`time` 等类型差异。

非有限浮点默认由 `XLOG_JSON_NONFINITE_REJECT` 拒绝，调用方也可显式选择 `NULL` 或 `STRING`。字符串始终严格校验 UTF-8，`EscapeFlags` 复用 `XJSON_WRITE_ESCAPE_SLASH`、`XJSON_WRITE_ESCAPE_HTML` 和 `XJSON_WRITE_ESCAPE_NON_ASCII`。

错误字段保留 `kind`、`domain`、`code`、`message`，并在存在时写出 `system_code`、`operation`、`data` 和递归 `cause`。`MaxErrorDepth` 默认是 `XLOG_JSON_ERROR_DEPTH_DEFAULT`；超限或循环原因链在任何字节写出前失败，不会静默截断。

流式 Writer 失败时，已经成功提交的字节不能回滚，`pWritten` 返回精确数量。Writer 应设置具体错误；未设置时格式器建立 `xrt.log` / `XLOG_ERROR_JSON_OUTPUT`。UTF-8 错误使用 `xrt.json` 域并携带字节位置。

启用独立的 `logger_format_json_buffer` 后，`xrtLogJson` 返回由 `xrtFree` 释放的零结尾文本。文件、控制台和网络 Sink 应优先使用流式入口。

## Console Sink

`xrtLogConsoleConfigInit` 建立适合交互程序的默认配置：最低 `INFO`、完整文本、`ERROR` 起写入 `stderr`、其余写入 `stdout`、逐条刷新并自动检测颜色。`xrtLogConsole(NULL)` 直接创建默认 Sink；`xrtLogAddConsole(pLogger, NULL)` 是创建并附加的一行常用路径。

`Target` 可选择 `STDOUT`、`STDERR` 或 `SPLIT`，分流阈值由 `ErrorLevel` 控制。`Color` 有三种明确语义：

- `AUTO`：只对交互 TTY 启用；Windows 还会协商虚拟终端序列，并尊重 `NO_COLOR` 环境变量。
- `NEVER`：不写 ANSI 序列，适合测试、文件重定向和由外部程序着色的输出。
- `ALWAYS`：始终写级别颜色和复位序列，适合明确支持 ANSI 的管道。

Windows 下会从 CRT 标准流取得当前实际句柄，而不是缓存进程启动时的控制台句柄。真实控制台使用固定栈缓冲把严格 UTF-8 分块转换为 UTF-16，并通过 `WriteConsoleW` 写出；多字节标量可以跨内部块边界，转换过程不分配内存。管道、文件和测试重定向继续按原样写出 UTF-8 字节，不受控制台代码页影响。真实控制台遇到非法 UTF-8 时返回 `XLOG_RESULT_ERROR` 和 `XLOG_ERROR_CONSOLE_WRITE`。

Console Sink 完整复制配置且自身线程安全；同一 Sink 的多线程记录不会在文本格式器分段边界交错。它不关闭 `stdout` 或 `stderr`。错误处理器在 Console 写入失败后递归记录到同一 Sink 时，该嵌套记录返回 `XLOG_RESULT_DROPPED`，避免错误路径自锁；原始写入仍返回 `XLOG_RESULT_ERROR` 和保留 `errno` 的 `xrt.log` 错误。

`Flush = true` 保留旧版逐条刷新手感，适合交互和崩溃诊断；高吞吐命令行程序可以关闭它，并在阶段边界调用 `xrtLogSinkFlush` 或 `xrtLogFlush`。

## File Sink

文件输出分为三个可独立裁剪的层：

- `logger_file`：通用格式回调、文件句柄、动态记录缓冲、精确滚动、持久化、reopen 和统计；不依赖文本或 JSON 格式器。
- `logger_file_text`：`xrtLogTextFile`、`xrtLogAddTextFile`，复制 `xlogtextconfig` 后复用文本流式格式器。
- `logger_file_json`：`xrtLogJsonFile`、`xrtLogAddJsonFile`，复制 `xlogjsonconfig` 后复用 JSON 流式格式器。

常用文本文件只需要：

```c
xlogfileoptions File;
xlogtextconfig Text;

xrtLogFileOptionsInit(&File, "service.log");
xrtLogTextConfigInit(&Text, XLOG_TEXT_SIMPLE);
File.MaxBytes = 64u * 1024u * 1024u;
File.BackupCount = 5u;
xrtLogAddTextFile(pLogger, &File, &Text);
```

JSON Lines 路径可以直接使用 `xrtLogAddJsonFile(pLogger, &File, NULL)`；空 JSON 配置采用完整默认格式。高级用户可向 `xrtLogFile` 提供 `xlogformatproc`，写入自定义文本、二进制封包或已有协议格式。格式器必须检查每次 Writer 返回值；即使第三方格式器错误地忽略失败，文件核心也会拒绝整条记录，不会写出已截断缓冲。`xlogfileconfig.UserData` 只在 `xrtLogFile` 成功后转移给 Sink，最后一个引用释放时调用 `Drop`；创建失败时仍归调用方。

### 内存和写入

文件 Sink 不分配固定 8K 缓冲。每个 Sink 从空缓冲开始，按实际编码后记录增长：

- `RecordLimit` 是单条编码后记录的硬上限，默认 `XLOG_FILE_RECORD_LIMIT_DEFAULT`，即 16 MiB。超限记录在任何文件字节写出前失败。
- `BufferLimit` 是记录结束后允许保留的复用容量上限，默认 `XLOG_FILE_BUFFER_LIMIT_DEFAULT`，即 64 KiB。偶发大记录不会永久抬高 Sink 常驻内存；设为零可在每条记录后释放缓冲。
- 一条记录完成编码后由一次 `xrtWriteFull` 提交，文本/JSON 格式器的多个小片段不会变成多个文件系统调用，并发记录也不会交错。

格式化或内存失败不会修改文件。底层文件写入发生短写后可能已经留下部分记录，错误通过 `XLOG_ERROR_FILE_WRITE` 包裹原始 `xrt.file` 原因并保留实际统计；文件系统无法对普通追加写提供通用回滚。

### 启动和滚动

`Mode` 只控制首次打开：`XLOG_FILE_APPEND` 保留已有内容，`XLOG_FILE_TRUNCATE` 截断已有文件。滚动和 `xrtLogFileReopen` 始终以追加语义打开新路径，避免意外清空外部创建的新文件。

`MaxBytes = 0` 禁用自动滚动。启用后，Sink 先完成格式化，再按当前精确文件大小和完整记录大小判断；记录恰好到达边界不会提前滚动。当前文件非空且加入下一条会越界时，先把 `path.N-1` 移到 `path.N`，再把当前文件移到 `path.1`。`BackupCount = 0` 表示到达阈值后直接截断当前路径。

单条记录允许大于 `MaxBytes`：空文件会完整容纳它，下一条记录到来前再滚动，因此记录不会被拆到两个文件。最大超出量由 `RecordLimit` 明确约束。`xrtLogFileRotate` 可立即执行同一滚动流程。

### 持久化和外部轮转

- `XLOG_FILE_SYNC_MANUAL`：默认高吞吐模式；只有 `xrtLogSinkFlush` / `xrtLogFlush` 显式调用 `xrtFlush`。
- `XLOG_FILE_SYNC_RECORD`：每条成功写入后提交到稳定存储，延迟最高但崩溃窗口最小。
- `XLOG_FILE_SYNC_INTERVAL`：写入记录时按 `xrtClock` 单调时间检查 `SyncInterval`。它没有后台线程，空闲期间不会为了计时单独唤醒。

Unix `logrotate` 或 Windows 外部路径替换完成后调用 `xrtLogFileReopen`，Sink 会先打开当前路径的新追加句柄，成功切换后再关闭旧句柄。`xrtLogFilePath` 返回稳定借用路径，`xrtLogFileStats` 返回当前大小、累计写入字节、记录、滚动、reopen 和持久化次数。

同一 Sink 内的提交、Flush、滚动、reopen 和统计是线程安全的。追加记录被合并为单次文件提交，但多个进程或多个独立 Sink 对同一路径执行备份滚动仍需要应用级排他协调；核心不会为了少数跨进程场景强制引入文件锁依赖。

文件错误使用 `xrt.log` 域的 `XLOG_ERROR_FILE_CONFIG`、`OPEN`、`FORMAT`、`LIMIT`、`WRITE`、`SYNC`、`ROTATE` 和 `CLOSE`，并把 `xrt.file`、内存或格式器错误保留为原因链。错误处理器递归提交到同一文件 Sink 时嵌套记录返回 `XLOG_RESULT_DROPPED`，原始错误不会因序列化锁而死锁。

## Ring Sink

`logger_ring` 是日志不能反向阻塞业务线程时的高吞吐路径。创建时一次性分配固定记录槽、
空闲槽 MPMC 队列和就绪 MPSC 队列；预热完成后，生产者只做容量检查、完整记录复制和
无锁发布，不取得互斥锁，也不执行动态分配。单个工作线程按 `Batch` 批量消费，因此同一
Ring 内的已接受记录保持 FIFO。

```c
xlogringconfig Ring;
xlogsink* pFast;

xrtLogRingConfigInit(&Ring);
Ring.Capacity = 4096u;
Ring.RecordLimit = 1024u;
Ring.Batch = 128u;
pFast = xrtLogRing(pFile, &Ring);
```

`Capacity` 会规范为队列支持的 2 次幂容量，`RecordLimit` 是每槽可容纳的完整拥有型
记录上限。消息、Logger 名称、源码位置、字段名称、字符串字段和错误引用都在提交返回前
转为槽内拥有数据；工作线程不会读取调用方已经失效的视图。满载、超长记录和工作线程
递归写回当前 Ring 都立即返回 `XLOG_RESULT_DROPPED`，不会退化为分配或阻塞。

`xrtLogSinkFlush(pFast)` 使用创建时预分配的唯一栅栏，等待此前所有记录提交目标并调用
目标 Flush；并发 Flush 只在管理冷路径串行，不影响普通生产者。`xrtLogRingStats` 返回
接受、处理、各种丢弃、失败、当前队列、字节和高水位快照；`xrtLogRingLastError` 返回
后台最近错误的新引用。释放最后一个 Ring 引用会关闭生产入口、排空已接受记录、执行
最终目标 Flush 并等待工作线程退出。

## Async Sink

`logger_async` 不复制 Console、File 或格式化逻辑，而是把任意同步 `xlogsink` 包装成有界异步 Sink。每个包装器只有一个工作线程，因此同一包装器内已经完成入队的记录、Flush 栅栏和目标调用保持 FIFO；需要并行处理不同目标时，应创建多个包装器，而不是让一个日志流在多个工作线程间失序。

```c
xlogasyncconfig Async;
xlogsink* pFile = xrtLogTextFile(&File, &Text);

xrtLogAsyncConfigInit(&Async);
Async.Capacity = 4096u;
Async.ByteLimit = 32u * 1024u * 1024u;
Async.Full = XLOG_ASYNC_DROP_NEWEST;
xrtLogAddAsync(pLogger, pFile, &Async);
xrtLogSinkFree(pFile);
```

`xrtLogAsync` 只增加目标引用，不接管调用方已有引用；返回的新包装器由调用方拥有。`xrtLogAddAsync` 是创建并附加的一行 Helper，成功后 Logger 持有包装器。空 `Name` 继承目标名称，包装层默认阈值为 `TRACE`，目标 Sink 仍会执行自己的阈值过滤。

### 记录所有权和内存

异步层在提交调用返回前接管完整记录：Logger、消息、源文件、函数、每个字段名和字符串字段都被复制；不可变 `xerror` 字段增加引用。所有字段和文本放在一块精确大小的动态分配中，没有每对象固定 8K 缓冲，也没有固定容量的指针环。

- `RecordLimit` 限制一条深拷贝记录的完整分配大小，默认 `XLOG_ASYNC_RECORD_LIMIT_DEFAULT`，即 1 MiB。
- `Capacity` 限制尚未被工作线程取走的记录和控制栅栏数量，默认 1024。
- `ByteLimit` 限制这些记录的完整分配字节，默认 `XLOG_ASYNC_BYTE_LIMIT_DEFAULT`，即 8 MiB。
- 槽位和字节预算在记录分配前预留；并发的 `BLOCK` 提交者也不能在队列外各自分配一条记录后绕过上限。
- `Queued` / `QueueBytes` 包含已经预留、正在深拷贝或已经入链的记录；`PeakQueued` / `PeakBytes` 是同一口径的高水位。

目标 Sink 正在处理的那一条记录已经离开队列，因此不计入队列预算。错误对象本身由其他模块创建且只增加引用，也不重复计入 Async 新增字节。

### 满载策略

- `XLOG_ASYNC_DROP_NEWEST`：默认策略；不分配新记录，立即返回 `XLOG_RESULT_DROPPED`，适合日志不能反向拖慢业务的服务。
- `XLOG_ASYNC_BLOCK`：等待记录数和字节预算同时可用，形成硬背压；不得用于目标回调可能无限阻塞的关键业务路径。
- `XLOG_ASYNC_DROP_OLDEST`：从队首覆盖尚未开始处理的记录，再接受新记录；Flush 栅栏永远不会被覆盖，栅栏位于队首或槽位仍被并发复制占用时会改为丢弃新记录。为了保持严格字节上限，旧记录先释放、再分配新记录，因此新记录随后发生 OOM 时旧记录仍已按覆盖策略丢弃。

同一工作线程由目标回调递归提交回当前 Async Sink 时直接返回 `XLOG_RESULT_DROPPED`，避免递归日志循环和 `BLOCK` 自锁；次数记录在 `ReentrantDrops`。

### Flush、错误和关闭

`xrtLogSinkFlush(pAsync)` 向 FIFO 插入不可覆盖栅栏并等待工作线程：所有在栅栏前已经被接受的记录先提交给目标，然后在同一工作线程调用目标 Flush。目标回调不能 Flush 自己所在的 Async Sink，该调用返回 `XLOG_ERROR_ASYNC_FLUSH`，不会自锁。

异步提交返回 `XLOG_RESULT_WRITTEN` 表示记录已被包装器接受，不表示目标已经写入。目标后续的 `WRITTEN`、`SKIPPED`、`DROPPED` 和 `ERROR` 分别进入 `xlogasyncstats`；后台最近一次目标或 Flush 错误可由 `xrtLogAsyncLastError` 取得新引用，调用方使用 `xrtErrorFree` 释放。错误域为 `xrt.log`，异步代码覆盖 `CONFIG`、`RECORD`、`QUEUE`、`CLOSED`、`TARGET`、`FLUSH` 和 `THREAD`，底层错误保留在原因链中。

最后一个包装器引用释放时关闭发送侧。`XLOG_ASYNC_DRAIN` 默认排空所有已接受记录并最终 Flush 目标；`XLOG_ASYNC_DISCARD` 释放尚未开始的记录，但不会中断正在执行的目标回调，随后仍执行最终目标 Flush。析构会等待工作线程完成，因此目标永久阻塞时析构同样会等待；库不会用强制终止线程换取表面上的快速退出。

## 并发

Logger、Sink、阈值、统计、附加、移除、提交和 Flush 都可并发调用。Sink 回调不在 Logger 锁内执行，因此允许递归记录，也允许回调移除自身。

移除只阻止新的快照取得 Sink。已经开始的提交可以完成当前回调，最后一个引用释放后才调用 `Drop`。

## 默认 Logger

`xrtLogSetDefault` 保存引用，`xrtLogDefault` 返回新引用。核心不会自动创建默认 Logger，也不会为默认 Logger 隐式附加 Sink。
