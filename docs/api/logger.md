# Logger

Logger 体系以 `logger_core` 为最小层。核心只负责记录、过滤、结构化字段、Sink 组合、生命周期、错误和统计，不隐式打开控制台或文件。

## 类型与常量

### `xloglevel`

日志级别从最详细到最严重排列，OFF 只用于过滤阈值。

```c
typedef enum xloglevel {
	XLOG_TRACE = 0,
	XLOG_DEBUG,
	XLOG_INFO,
	XLOG_WARN,
	XLOG_ERROR,
	XLOG_FATAL,
	XLOG_OFF
} xloglevel;
```

| 值 | 语义 |
|---|---|
| `XLOG_TRACE` | 最详细级别 |
| `XLOG_DEBUG` | 调试级别 |
| `XLOG_INFO` | 信息级别 |
| `XLOG_WARN` | 警告级别 |
| `XLOG_ERROR` | 失败 |
| `XLOG_FATAL` | 致命级别 |
| `XLOG_OFF` | 关闭过滤 |

### `xlogresult`

日志结果把正常过滤、成功写入、主动丢弃和真实错误分开表达。

```c
typedef enum xlogresult {
	XLOG_RESULT_ERROR = -1,
	XLOG_RESULT_SKIPPED = 0,
	XLOG_RESULT_WRITTEN = 1,
	XLOG_RESULT_DROPPED = 2
} xlogresult;
```

| 值 | 语义 |
|---|---|
| `XLOG_RESULT_ERROR` | 失败 |
| `XLOG_RESULT_SKIPPED` | SKIPPED |
| `XLOG_RESULT_WRITTEN` | WRITTEN |
| `XLOG_RESULT_DROPPED` | 未写入但至少一个 Sink 主动丢弃 |

### `xlogerror`

xrt.log 域错误码在各个日志分层之间保持稳定。

```c
typedef enum xlogerror {
	XLOG_ERROR_CALLBACK = 1,
	XLOG_ERROR_TEXT_OUTPUT,
	XLOG_ERROR_JSON_OUTPUT,
	XLOG_ERROR_JSON_CONFIG,
	XLOG_ERROR_JSON_VALUE,
	XLOG_ERROR_JSON_DEPTH,
	XLOG_ERROR_CONSOLE_CONFIG,
	XLOG_ERROR_CONSOLE_WRITE,
	XLOG_ERROR_CONSOLE_FLUSH,
	XLOG_ERROR_FILE_CONFIG,
	XLOG_ERROR_FILE_OPEN,
	XLOG_ERROR_FILE_FORMAT,
	XLOG_ERROR_FILE_LIMIT,
	XLOG_ERROR_FILE_WRITE,
	XLOG_ERROR_FILE_SYNC,
	XLOG_ERROR_FILE_ROTATE,
	XLOG_ERROR_FILE_CLOSE,
	XLOG_ERROR_ASYNC_CONFIG,
	XLOG_ERROR_ASYNC_RECORD,
	XLOG_ERROR_ASYNC_QUEUE,
	XLOG_ERROR_ASYNC_CLOSED,
	XLOG_ERROR_ASYNC_TARGET,
	XLOG_ERROR_ASYNC_FLUSH,
	XLOG_ERROR_ASYNC_THREAD,
	XLOG_ERROR_RING_CONFIG,
	XLOG_ERROR_RING_QUEUE,
	XLOG_ERROR_RING_CLOSED,
	XLOG_ERROR_RING_TARGET,
	XLOG_ERROR_RING_FLUSH,
	XLOG_ERROR_RING_THREAD
} xlogerror;
```

| 值 | 语义 |
|---|---|
| `XLOG_ERROR_CALLBACK` | 回调失败 |
| `XLOG_ERROR_TEXT_OUTPUT` | 文本输出失败 |
| `XLOG_ERROR_JSON_OUTPUT` | JSON输出失败 |
| `XLOG_ERROR_JSON_CONFIG` | JSON配置非法 |
| `XLOG_ERROR_JSON_VALUE` | JSON值非法 |
| `XLOG_ERROR_JSON_DEPTH` | 深度超限 |
| `XLOG_ERROR_CONSOLE_CONFIG` | CONSOLE配置非法 |
| `XLOG_ERROR_CONSOLE_WRITE` | CONSOLE写方向 |
| `XLOG_ERROR_CONSOLE_FLUSH` | CONSOLE刷新 |
| `XLOG_ERROR_FILE_CONFIG` | FILE配置非法 |
| `XLOG_ERROR_FILE_OPEN` | 失败 |
| `XLOG_ERROR_FILE_FORMAT` | 格式非法 |
| `XLOG_ERROR_FILE_LIMIT` | FILE超限 |
| `XLOG_ERROR_FILE_WRITE` | FILE写方向 |
| `XLOG_ERROR_FILE_SYNC` | 失败 |
| `XLOG_ERROR_FILE_ROTATE` | 失败 |
| `XLOG_ERROR_FILE_CLOSE` | 失败 |
| `XLOG_ERROR_ASYNC_CONFIG` | ASYNC配置非法 |
| `XLOG_ERROR_ASYNC_RECORD` | 失败 |
| `XLOG_ERROR_ASYNC_QUEUE` | 失败 |
| `XLOG_ERROR_ASYNC_CLOSED` | ASYNC已关闭 |
| `XLOG_ERROR_ASYNC_TARGET` | 失败 |
| `XLOG_ERROR_ASYNC_FLUSH` | ASYNC刷新 |
| `XLOG_ERROR_ASYNC_THREAD` | ASYNC线程标识 |
| `XLOG_ERROR_RING_CONFIG` | RING配置非法 |
| `XLOG_ERROR_RING_QUEUE` | 失败 |
| `XLOG_ERROR_RING_CLOSED` | RING已关闭 |
| `XLOG_ERROR_RING_TARGET` | 失败 |
| `XLOG_ERROR_RING_FLUSH` | RING刷新 |
| `XLOG_ERROR_RING_THREAD` | Ring 线程失败 |

### `xlogfieldtype`

结构化字段类型不依赖 Value 容器，保持 Logger 核心轻量。

```c
typedef enum xlogfieldtype {
	XLOG_FIELD_NULL = 0,
	XLOG_FIELD_BOOL,
	XLOG_FIELD_INT,
	XLOG_FIELD_UINT,
	XLOG_FIELD_FLOAT,
	XLOG_FIELD_STRING,
	XLOG_FIELD_TIME,
	XLOG_FIELD_ERROR
} xlogfieldtype;
```

| 值 | 语义 |
|---|---|
| `XLOG_FIELD_NULL` | 空值 |
| `XLOG_FIELD_BOOL` | 布尔 |
| `XLOG_FIELD_INT` | 有符号整数 |
| `XLOG_FIELD_UINT` | 无符号整数 |
| `XLOG_FIELD_FLOAT` | 浮点 |
| `XLOG_FIELD_STRING` | 字符串 |
| `XLOG_FIELD_TIME` | 时间 |
| `XLOG_FIELD_ERROR` | 失败 |

### `xlogrecord`

一条记录的所有视图和字段只在提交调用期间有效。

```c
typedef struct xlogrecord {
	xtime Time;
	xloglevel Level;
	xstrview Logger;
	xstrview Message;
	const xlogfield* Fields;
	size_t FieldCount;
	xstrview File;
	xstrview Function;
	uint32 Line;
	uint64 ThreadId;
} xlogrecord;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Time` | `xtime` | 时间戳（Unix 微秒） |
| `Level` | `xloglevel` | 级别 |
| `Logger` | `xstrview` | Logger |
| `Message` | `xstrview` | 消息文本 |
| `Fields` | `const xlogfield*` | Fields |
| `FieldCount` | `size_t` | FieldCount |
| `File` | `xstrview` | 文件名 |
| `Function` | `xstrview` | 函数名 |
| `Line` | `uint32` | 行号 |
| `ThreadId` | `uint64` | 线程标识 |

### `xlogstats`

统计按记录聚合；一个记录写到多个 Sink 仍只计一次 Logger 结果。

```c
typedef struct xlogstats {
	uint64 Submitted;
	uint64 Written;
	uint64 Skipped;
	uint64 Dropped;
	uint64 Failed;
} xlogstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Submitted` | `uint64` | Submitted |
| `Written` | `uint64` | Written |
| `Skipped` | `uint64` | Skipped |
| `Dropped` | `uint64` | Dropped |
| `Failed` | `uint64` | Failed |

### `xlogsinkconfig`

自定义 Sink 配置在创建成功后把 UserData 生命周期交给 Sink。

```c
typedef struct xlogsinkconfig {
	xstrview Name;
	xloglevel Level;
	xlogsinkwriteproc Write;
	xlogsinkflushproc Flush;
	xlogsinkdropproc Drop;
	ptr UserData;
} xlogsinkconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | 名称 |
| `Level` | `xloglevel` | 级别 |
| `Write` | `xlogsinkwriteproc` | Write |
| `Flush` | `xlogsinkflushproc` | Flush |
| `Drop` | `xlogsinkdropproc` | Drop |
| `UserData` | `ptr` | 用户数据 |

### `xlogtextstyle`

三种预设只初始化配置，调用方仍可逐位调整输出组成。

```c
typedef enum xlogtextstyle {
	XLOG_TEXT_FULL = 0,
	XLOG_TEXT_SIMPLE,
	XLOG_TEXT_MESSAGE
} xlogtextstyle;
```

| 值 | 语义 |
|---|---|
| `XLOG_TEXT_FULL` | 已满 |
| `XLOG_TEXT_SIMPLE` | 简单格式 |
| `XLOG_TEXT_MESSAGE` | 仅输出消息本身 |

### `xlogtextflag`

文本标志分别控制前缀、元数据、字段、换行和消息原样输出。

```c
typedef enum xlogtextflag {
	XLOG_TEXT_TIME = UINT32_C(0x00000001),
	XLOG_TEXT_LEVEL = UINT32_C(0x00000002),
	XLOG_TEXT_LOGGER = UINT32_C(0x00000004),
	XLOG_TEXT_SOURCE = UINT32_C(0x00000008),
	XLOG_TEXT_THREAD = UINT32_C(0x00000010),
	XLOG_TEXT_FIELDS = UINT32_C(0x00000020),
	XLOG_TEXT_NEWLINE = UINT32_C(0x00000040),
	XLOG_TEXT_RAW_MESSAGE = UINT32_C(0x00000080)
} xlogtextflag;
```

| 值 | 语义 |
|---|---|
| `XLOG_TEXT_TIME` | 时间 |
| `XLOG_TEXT_LEVEL` | 级别 |
| `XLOG_TEXT_LOGGER` | Logger 名称 |
| `XLOG_TEXT_SOURCE` | 源码位置 |
| `XLOG_TEXT_THREAD` | 线程标识 |
| `XLOG_TEXT_FIELDS` | 字段 |
| `XLOG_TEXT_NEWLINE` | 换行 |
| `XLOG_TEXT_RAW_MESSAGE` | 消息控制字符原样输出 |

### `xlogtextconfig`

文本配置使用固定 UTC 偏移，默认完整格式为 UTC 单行文本。

```c
typedef struct xlogtextconfig {
	uint32 Flags;
	int UtcOffset;
} xlogtextconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `UtcOffset` | `int` | UTC 偏移（分钟） |

### `xlogjsonfieldstyle`

JSON 字段默认写成对象，数组形式可无损保留重名和字段类型。

```c
typedef enum xlogjsonfieldstyle {
	XLOG_JSON_FIELDS_OBJECT = 0,
	XLOG_JSON_FIELDS_ARRAY
} xlogjsonfieldstyle;
```

| 值 | 语义 |
|---|---|
| `XLOG_JSON_FIELDS_OBJECT` | XLOGJSON字段对象形态 |
| `XLOG_JSON_FIELDS_ARRAY` | 数组形式字段 |

### `xlogjsonnonfinite`

非有限浮点值必须由调用方明确选择拒绝、null 或字符串表示。

```c
typedef enum xlogjsonnonfinite {
	XLOG_JSON_NONFINITE_REJECT = 0,
	XLOG_JSON_NONFINITE_NULL,
	XLOG_JSON_NONFINITE_STRING
} xlogjsonnonfinite;
```

| 值 | 语义 |
|---|---|
| `XLOG_JSON_NONFINITE_REJECT` | REJECT |
| `XLOG_JSON_NONFINITE_NULL` | 空值 |
| `XLOG_JSON_NONFINITE_STRING` | 字符串表示 |

### `xlogjsonflag`

JSON 标志分别控制顶层元数据、字段和 JSON Lines 换行。

```c
typedef enum xlogjsonflag {
	XLOG_JSON_TIME = UINT32_C(0x00000001),
	XLOG_JSON_LEVEL = UINT32_C(0x00000002),
	XLOG_JSON_LOGGER = UINT32_C(0x00000004),
	XLOG_JSON_MESSAGE = UINT32_C(0x00000008),
	XLOG_JSON_SOURCE = UINT32_C(0x00000010),
	XLOG_JSON_THREAD = UINT32_C(0x00000020),
	XLOG_JSON_FIELDS = UINT32_C(0x00000040),
	XLOG_JSON_NEWLINE = UINT32_C(0x00000080)
} xlogjsonflag;
```

| 值 | 语义 |
|---|---|
| `XLOG_JSON_TIME` | 时间 |
| `XLOG_JSON_LEVEL` | 级别 |
| `XLOG_JSON_LOGGER` | Logger 名称 |
| `XLOG_JSON_MESSAGE` | 消息 |
| `XLOG_JSON_SOURCE` | 源码位置 |
| `XLOG_JSON_THREAD` | 线程标识 |
| `XLOG_JSON_FIELDS` | 字段 |
| `XLOG_JSON_NEWLINE` | 换行结束 |

### `xlogjsonconfig`

JSON 配置独立约束转义、字段表示、非有限数和错误原因链。

```c
typedef struct xlogjsonconfig {
	uint32 Flags;
	uint32 EscapeFlags;
	xlogjsonfieldstyle FieldStyle;
	xlogjsonnonfinite NonFinite;
	size_t MaxErrorDepth;
} xlogjsonconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `EscapeFlags` | `uint32` | EscapeFlags |
| `FieldStyle` | `xlogjsonfieldstyle` | FieldStyle |
| `NonFinite` | `xlogjsonnonfinite` | NonFinite |
| `MaxErrorDepth` | `size_t` | MaxErrorDepth |

### `xlogconsoletarget`

控制台目标可固定到一个流，或按级别分流。

```c
typedef enum xlogconsoletarget {
	XLOG_CONSOLE_STDOUT = 0,
	XLOG_CONSOLE_STDERR,
	XLOG_CONSOLE_SPLIT
} xlogconsoletarget;
```

| 值 | 语义 |
|---|---|
| `XLOG_CONSOLE_STDOUT` | 标准输出 |
| `XLOG_CONSOLE_STDERR` | 标准错误 |
| `XLOG_CONSOLE_SPLIT` | 按级别分流 |

### `xlogconsolecolor`

自动配色只对支持 ANSI 的交互终端生效，并尊重 NO_COLOR。

```c
typedef enum xlogconsolecolor {
	XLOG_CONSOLE_COLOR_AUTO = 0,
	XLOG_CONSOLE_COLOR_NEVER,
	XLOG_CONSOLE_COLOR_ALWAYS
} xlogconsolecolor;
```

| 值 | 语义 |
|---|---|
| `XLOG_CONSOLE_COLOR_AUTO` | 自动 |
| `XLOG_CONSOLE_COLOR_NEVER` | 永不 |
| `XLOG_CONSOLE_COLOR_ALWAYS` | 始终着色 |

### `xlogconsoleconfig`

Console Sink 配置在创建时完整复制，后续修改不影响已创建对象。

```c
typedef struct xlogconsoleconfig {
	xloglevel Level;
	xlogconsoletarget Target;
	xlogconsolecolor Color;
	xloglevel ErrorLevel;
	bool Flush;
	xlogtextconfig Text;
} xlogconsoleconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Level` | `xloglevel` | 级别 |
| `Target` | `xlogconsoletarget` | 目标视图 |
| `Color` | `xlogconsolecolor` | Color |
| `ErrorLevel` | `xloglevel` | ErrorLevel |
| `Flush` | `bool` | Flush |
| `Text` | `xlogtextconfig` | 文本视图 |

### `xlogfilemode`

启动模式只影响首次打开；reopen 和滚动后的文件始终按追加语义打开。

```c
typedef enum xlogfilemode {
	XLOG_FILE_APPEND = 0,
	XLOG_FILE_TRUNCATE
} xlogfilemode;
```

| 值 | 语义 |
|---|---|
| `XLOG_FILE_APPEND` | XLOGFILE追加 |
| `XLOG_FILE_TRUNCATE` | 创建时截断 |

### `xlogfilesync`

文件持久化可以完全手动、逐条执行，或在写入时按单调时钟间隔执行。

```c
typedef enum xlogfilesync {
	XLOG_FILE_SYNC_MANUAL = 0,
	XLOG_FILE_SYNC_RECORD,
	XLOG_FILE_SYNC_INTERVAL
} xlogfilesync;
```

| 值 | 语义 |
|---|---|
| `XLOG_FILE_SYNC_MANUAL` | 手动 |
| `XLOG_FILE_SYNC_RECORD` | RECORD |
| `XLOG_FILE_SYNC_INTERVAL` | 按间隔落盘 |

### `xlogfileoptions`

文件选项在创建时完整复制，Path 文本也会被独立复制。

```c
typedef struct xlogfileoptions {
	cstr Path;
	xloglevel Level;
	xlogfilemode Mode;
	xlogfilesync Sync;
	uint64 MaxBytes;
	uint32 BackupCount;
	size_t RecordLimit;
	size_t BufferLimit;
	uint64 SyncInterval;
} xlogfileoptions;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Path` | `cstr` | 路径 |
| `Level` | `xloglevel` | 级别 |
| `Mode` | `xlogfilemode` | 模式 |
| `Sync` | `xlogfilesync` | Sync |
| `MaxBytes` | `uint64` | 滚动字节阈值 |
| `BackupCount` | `uint32` | 保留备份数 |
| `RecordLimit` | `size_t` | 单条上限 |
| `BufferLimit` | `size_t` | 缓冲保留上限 |
| `SyncInterval` | `uint64` | 同步间隔 |

### `xlogfileconfig`

通用文件配置在创建成功后把格式器数据生命周期交给 Sink。

```c
typedef struct xlogfileconfig {
	xlogfileoptions Options;
	xlogformatproc Format;
	xlogformatdropproc Drop;
	ptr UserData;
} xlogfileconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Options` | `xlogfileoptions` | 选项 |
| `Format` | `xlogformatproc` | 格式 |
| `Drop` | `xlogformatdropproc` | Drop |
| `UserData` | `ptr` | 用户数据 |

### `xlogfilestats`

文件统计区分当前文件大小和进程内累计写入量。

```c
typedef struct xlogfilestats {
	uint64 CurrentBytes;
	uint64 WrittenBytes;
	uint64 Records;
	uint64 Rotations;
	uint64 Reopens;
	uint64 Syncs;
} xlogfilestats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `CurrentBytes` | `uint64` | CurrentBytes |
| `WrittenBytes` | `uint64` | WrittenBytes |
| `Records` | `uint64` | Records |
| `Rotations` | `uint64` | Rotations |
| `Reopens` | `uint64` | Reopens |
| `Syncs` | `uint64` | Syncs |

### `xlogasyncfull`

队列满载策略明确区分业务背压、新记录丢弃和旧记录覆盖。

```c
typedef enum xlogasyncfull {
	XLOG_ASYNC_BLOCK = 0,
	XLOG_ASYNC_DROP_NEWEST,
	XLOG_ASYNC_DROP_OLDEST
} xlogasyncfull;
```

| 值 | 语义 |
|---|---|
| `XLOG_ASYNC_BLOCK` | 阻塞策略 |
| `XLOG_ASYNC_DROP_NEWEST` | 丢弃策略NEWEST |
| `XLOG_ASYNC_DROP_OLDEST` | 覆盖最旧记录 |

### `xlogasyncshutdown`

最后一个 Async Sink 引用释放时可以排空队列，也可以显式放弃未处理记录。

```c
typedef enum xlogasyncshutdown {
	XLOG_ASYNC_DRAIN = 0,
	XLOG_ASYNC_DISCARD
} xlogasyncshutdown;
```

| 值 | 语义 |
|---|---|
| `XLOG_ASYNC_DRAIN` | XLOGASYNC排空策略 |
| `XLOG_ASYNC_DISCARD` | 放弃未处理记录 |

### `xlogasyncconfig`

Async Sink 保持单工作线程顺序，并同时限制排队记录数和真实记录字节数。

```c
typedef struct xlogasyncconfig {
	xstrview Name;
	xloglevel Level;
	xlogasyncfull Full;
	xlogasyncshutdown Shutdown;
	size_t Capacity;
	size_t RecordLimit;
	size_t ByteLimit;
	size_t StackSize;
} xlogasyncconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | 名称 |
| `Level` | `xloglevel` | 级别 |
| `Full` | `xlogasyncfull` | Full |
| `Shutdown` | `xlogasyncshutdown` | Shutdown |
| `Capacity` | `size_t` | 容量 |
| `RecordLimit` | `size_t` | 单条上限 |
| `ByteLimit` | `size_t` | ByteLimit |
| `StackSize` | `size_t` | 栈大小 |

### `xlogasyncstats`

异步统计区分入队、目标结果、各类丢弃和当前队列高水位。

```c
typedef struct xlogasyncstats {
	uint64 Enqueued;
	uint64 Processed;
	uint64 Written;
	uint64 Skipped;
	uint64 DroppedNewest;
	uint64 DroppedOldest;
	uint64 DroppedTarget;
	uint64 ReentrantDrops;
	uint64 Discarded;
	uint64 Failed;
	uint64 Flushes;
	size_t Queued;
	size_t QueueBytes;
	size_t PeakQueued;
	size_t PeakBytes;
} xlogasyncstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Enqueued` | `uint64` | Enqueued |
| `Processed` | `uint64` | Processed |
| `Written` | `uint64` | Written |
| `Skipped` | `uint64` | Skipped |
| `DroppedNewest` | `uint64` | DroppedNewest |
| `DroppedOldest` | `uint64` | DroppedOldest |
| `DroppedTarget` | `uint64` | DroppedTarget |
| `ReentrantDrops` | `uint64` | ReentrantDrops |
| `Discarded` | `uint64` | Discarded |
| `Failed` | `uint64` | Failed |
| `Flushes` | `uint64` | Flushes |
| `Queued` | `size_t` | Queued |
| `QueueBytes` | `size_t` | QueueBytes |
| `PeakQueued` | `size_t` | PeakQueued |
| `PeakBytes` | `size_t` | PeakBytes |

### `xlogringconfig`

Ring 预分配固定记录槽；满载和超长记录均立即丢弃，不反向阻塞业务线程。

```c
typedef struct xlogringconfig {
	xstrview Name;
	xloglevel Level;
	size_t Capacity;
	size_t RecordLimit;
	size_t Batch;
	size_t StackSize;
	uint64 IdleWait;
} xlogringconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | 名称 |
| `Level` | `xloglevel` | 级别 |
| `Capacity` | `size_t` | 容量 |
| `RecordLimit` | `size_t` | 单条上限 |
| `Batch` | `size_t` | 批量大小 |
| `StackSize` | `size_t` | 栈大小 |
| `IdleWait` | `uint64` | IdleWait |

### `xlogringstats`

Ring 统计区分容量丢弃、记录超限、递归写入和目标 Sink 结果。

```c
typedef struct xlogringstats {
	uint64 Enqueued;
	uint64 Processed;
	uint64 Written;
	uint64 Skipped;
	uint64 TargetDropped;
	uint64 Dropped;
	uint64 Oversized;
	uint64 ReentrantDrops;
	uint64 Failed;
	uint64 Flushes;
	size_t Queued;
	size_t QueueBytes;
	size_t PeakQueued;
	size_t PeakBytes;
} xlogringstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Enqueued` | `uint64` | Enqueued |
| `Processed` | `uint64` | Processed |
| `Written` | `uint64` | Written |
| `Skipped` | `uint64` | Skipped |
| `TargetDropped` | `uint64` | TargetDropped |
| `Dropped` | `uint64` | Dropped |
| `Oversized` | `uint64` | Oversized |
| `ReentrantDrops` | `uint64` | ReentrantDrops |
| `Failed` | `uint64` | Failed |
| `Flushes` | `uint64` | Flushes |
| `Queued` | `size_t` | Queued |
| `QueueBytes` | `size_t` | QueueBytes |
| `PeakQueued` | `size_t` | PeakQueued |
| `PeakBytes` | `size_t` | PeakBytes |

### `xlogger`

Logger 和 Sink 都是线程安全的引用对象。

```c
typedef struct xlogger xlogger;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xlogsink`

Sink 对象（不透明）：线程安全的引用对象，可被多个 Logger 和包装 Sink 共享。


```c
typedef struct xlogsink xlogsink;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xlogwriteproc`

通用字节 Writer 必须在返回前消费借用数据。

```c
typedef bool (*xlogwriteproc)(xbytesview Data, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xlogformatproc`

通用格式器同步地把一条借用记录写给 Writer。

```c
typedef bool (*xlogformatproc)(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xlogformatdropproc`

格式器数据释放回调只在成功创建的拥有者销毁时执行。

```c
typedef void (*xlogformatdropproc)(ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xlogsinkwriteproc`

Sink 回调同步消费借用记录，并返回稳定的流控结果。

```c
typedef xlogresult (*xlogsinkwriteproc)(
	const xlogrecord* pRecord,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xlogsinkflushproc`

Flush 回调提交已经接受的内容；空回调等价于成功。

```c
typedef bool (*xlogsinkflushproc)(ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xlogsinkdropproc`

Drop 回调在最后一个 Sink 引用释放后接收用户数据。

```c
typedef void (*xlogsinkdropproc)(ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XLOG_ASYNC_CAPACITY_DEFAULT` | `1024u` | ASYNCCAPACITY默认值 |
| `XLOG_RING_CAPACITY_DEFAULT` | `1024u` | RINGCAPACITY默认值 |
| `XLOG_RING_RECORD_LIMIT_DEFAULT` | `4096u` | RINGRECORD超限默认值 |
| `XLOG_RING_BATCH_DEFAULT` | `64u` | RINGBATCH默认值 |
| `XLOG_RING_BATCH_MAX` | `256u` | RINGBATCH上限 |
| `XLOG_RING_IDLE_WAIT_DEFAULT` | `100u` | RINGIDLEWAIT默认值 |

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


## 级别、记录与字段

### `xrtLogLevelName`

返回日志级别的稳定英文名称，供格式化和诊断输出使用。

```c
cstr xrtLogLevelName(xloglevel Level)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Level` | 输入 | — | 待命名的日志级别 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 静态英文名称（`"TRACE"` … `"OFF"`） | — |
| `""` | 级别非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 级别不在 `XLOG_TRACE` … `XLOG_OFF` 枚举范围内

#### 范例

[core](../../examples/logging/core/main.c) · 级别名称

```c
		xrtLogLevelName(pRecord->Level),
```


### `xrtLogRecordValidate`

校验记录的级别、全部视图、字段范围和字段类型；所有提交入口内部先执行同一校验。

```c
bool xrtLogRecordValidate(const xlogrecord* pRecord)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRecord` | 输入 | 非空 | 待校验的记录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 记录合法，可提交 | — |
| `false` | 非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 提交前校验

```c
			xrtLogRecordValidate(&(xlogrecord){ 0 }) ? 1 : 0);
```


### `xrtLogFieldNull`

构造显式 NULL 语义的字段；字段借用名称，不拥有任何数据。

```c
xlogfield xrtLogFieldNull(xstrview Name)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字段值 | 按值返回的借用字段 | — |

#### 错误

- 无 — 本函数不设置错误

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 空值字段

```c
		Fields[i++] = xrtLogFieldNull(SV("n"));
```


### `xrtLogFieldBool`

构造布尔字段。

```c
xlogfield xrtLogFieldBool(xstrview Name, bool bValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名视图 |
| `bValue` | 输入 | — | 布尔值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字段值 | 按值返回的借用字段 | — |

#### 错误

- 无 — 本函数不设置错误

#### 范例

[json](../../examples/logging/json/main.c) · 布尔字段

```c
	Fields[1] = xrtLogFieldBool(XRT_STR_LITERAL("cached"), false);
```


### `xrtLogFieldInt`

构造 int64 字段。

```c
xlogfield xrtLogFieldInt(xstrview Name, int64 iValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名视图 |
| `iValue` | 输入 | — | 整数值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字段值 | 按值返回的借用字段 | — |

#### 错误

- 无 — 本函数不设置错误

#### 范例

[core](../../examples/logging/core/main.c) · 整数字段

```c
	Field = xrtLogFieldInt(XRT_STR_LITERAL("request_id"), 42);
```


### `xrtLogFieldUInt`

构造 uint64 字段。

```c
xlogfield xrtLogFieldUInt(xstrview Name, uint64 iValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名视图 |
| `iValue` | 输入 | — | 无符号值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字段值 | 按值返回的借用字段 | — |

#### 错误

- 无 — 本函数不设置错误

#### 范例

[file_json](../../examples/logging/file_json/main.c) · 无符号字段

```c
	Field = xrtLogFieldUInt(XRT_STR_LITERAL("request_id"), 42u);
```


### `xrtLogFieldFloat`

构造 double 字段。

```c
xlogfield xrtLogFieldFloat(xstrview Name, double fValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名视图 |
| `fValue` | 输入 | — | 浮点值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字段值 | 按值返回的借用字段 | — |

#### 错误

- 无 — 本函数不设置错误

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 浮点字段

```c
		Fields[i++] = xrtLogFieldFloat(SV("f"), 1.5);
```


### `xrtLogFieldString`

构造字符串字段；值视图与字段名一样只在提交调用期间有效。

```c
xlogfield xrtLogFieldString(xstrview Name, xstrview Value)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名视图 |
| `Value` | 输入 | 借用 | 字符串值视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字段值 | 按值返回的借用字段 | — |

#### 错误

- 无 — 本函数不设置错误

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 字符串字段

```c
		Fields[i++] = xrtLogFieldString(SV("s"), SV("val"));
```


### `xrtLogFieldTime`

构造 Unix Epoch 微秒时间字段。

```c
xlogfield xrtLogFieldTime(xstrview Name, xtime iValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名视图 |
| `iValue` | 输入 | — | Unix Epoch 微秒 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字段值 | 按值返回的借用字段 | — |

#### 错误

- 无 — 本函数不设置错误

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 时间字段

```c
		Fields[i++] = xrtLogFieldTime(SV("t"), xrtNow());
```


### `xrtLogFieldError`

构造借用结构化错误的字段；异步层复制时会增加错误引用。

```c
xlogfield xrtLogFieldError(xstrview Name, const xerror* pError)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 字段名视图 |
| `pError` | 输入 | 借用 | 不可变错误对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 字段值 | 按值返回的借用字段 | — |

#### 错误

- 无 — 本函数不设置错误

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 错误字段

```c
		Fields[i++] = xrtLogFieldError(SV("e"), NULL);
```


## Logger 生命周期

### `xrtLogCreate`

创建同步 Logger；名称被复制到对象内部，初始没有 Sink。

```c
xlogger* xrtLogCreate(xstrview Name, xloglevel Level)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 非空视图 | Logger 名称 |
| `Level` | 输入 | 合法级别 | 初始过滤阈值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Logger（引用 1） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_OVERFLOW` — 名称长度引起尺寸溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[core](../../examples/logging/core/main.c) · 创建与最小用法

```c
	pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_DEBUG);
```


### `xrtLogRef`

增加 Logger 引用并返回原指针。

```c
xlogger* xrtLogRef(xlogger* pLogger)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法或引用已失效 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 引用计数已失效

#### 范例

[logger_tour](../../examples/logging/logger_tour/main.c) · 共享引用

```c
		xlogger* pRef = xrtLogRef(pLogger);
```


### `xrtLogFree`

释放 Logger 引用；空指针不执行操作，归零时释放最终 Sink 快照。

```c
void xrtLogFree(xlogger* pLogger)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[core](../../examples/logging/core/main.c) · 释放

```c
		xrtLogFree(pLogger);
```


### `xrtLogName`

返回 Logger 生命周期内稳定的借用名称。

```c
xstrview xrtLogName(const xlogger* pLogger)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空视图 | 借用名称 | — |
| 空视图 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[logger_tour](../../examples/logging/logger_tour/main.c) · 名称查询

```c
			(int)xrtLogName(pDefault).Size, xrtLogName(pDefault).Data);
```


### `xrtLogLevel`

并发读取 Logger 当前过滤阈值。

```c
xloglevel xrtLogLevel(const xlogger* pLogger)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 当前阈值 | `XLOG_TRACE` … `XLOG_OFF` | — |
| `XLOG_OFF` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[logger_tour](../../examples/logging/logger_tour/main.c) · 阈值读取

```c
		printf(" level=%d", (int)xrtLogLevel(pDefault));
```


### `xrtLogSetLevel`

原子调整 Logger 过滤阈值，立即对后续提交生效。

```c
bool xrtLogSetLevel(xlogger* pLogger, xloglevel Level)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 新阈值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已更新 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[logger_tour](../../examples/logging/logger_tour/main.c) · 阈值调整

```c
	printf("set-level=%d", xrtLogSetLevel(pLogger, XLOG_WARN) ? 1 : 0);
```


### `xrtLogDefault`

返回进程默认 Logger 的新引用；核心不会自动创建默认 Logger。

```c
xlogger* xrtLogDefault(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 默认 Logger 新引用，用后 `xrtLogFree` | — |
| `NULL` | 尚未设置默认 | 不设错 |

#### 错误

- 无错误 — 未设置默认 Logger 时返回 `NULL` 且不设置错误

#### 范例

[logger_tour](../../examples/logging/logger_tour/main.c) · 进程默认 Logger

```c
		xlogger* pDefault = xrtLogDefault();
```


### `xrtLogSetDefault`

原子替换进程默认 Logger；保存新引用并释放旧引用，空指针用于清除。

```c
bool xrtLogSetDefault(xlogger* pLogger)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 允许空 | 新默认；空 = 清除 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已替换或已清除 | — |
| `false` | 引用失败 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 对目标 Logger 增加引用失败；空指针清除不失败

#### 范例

[logger_tour](../../examples/logging/logger_tour/main.c) · 替换默认 Logger

```c
		xrtLogSetDefault(pLogger) ? "ok" : "fail");
```


## Sink 生命周期与直连

### `xrtLogSinkCreate`

创建可被多个 Logger 和包装 Sink 共享的自定义 Sink；成功后 `UserData` 生命周期交给 Sink。

```c
xlogsink* xrtLogSinkCreate(const xlogsinkconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空且 `Write` 非空 | 操作表与上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Sink（引用 1） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 内部结构分配失败

#### 范例

[async](../../examples/logging/async/main.c) · 自定义 Sink

```c
	pTarget = xrtLogSinkCreate(&TargetConfig);
```


### `xrtLogSinkRef`

增加 Sink 引用并返回原指针。

```c
xlogsink* xrtLogSinkRef(xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 非空 | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法或引用已失效 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 引用计数已失效

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 共享引用

```c
	(void)xrtLogSinkRef(pConsole);
```


### `xrtLogSinkFree`

释放 Sink 引用；空指针不执行操作，归零时调用 `Drop` 回调。

```c
void xrtLogSinkFree(xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[async](../../examples/logging/async/main.c) · 释放

```c
		xrtLogSinkFree(pTarget);
```


### `xrtLogSinkName`

返回 Sink 生命周期内稳定的借用名称。

```c
xstrview xrtLogSinkName(const xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 非空 | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空视图 | 借用名称 | — |
| 空视图 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 名称查询

```c
		(int)xrtLogSinkName(pConsole).Size, xrtLogSinkName(pConsole).Data);
```


### `xrtLogSinkLevel`

并发读取 Sink 当前过滤阈值。

```c
xloglevel xrtLogSinkLevel(const xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 非空 | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 当前阈值 | `XLOG_TRACE` … `XLOG_OFF` | — |
| `XLOG_OFF` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 阈值读取

```c
	printf(" level=%d", (int)xrtLogSinkLevel(pConsole));
```


### `xrtLogSinkSetLevel`

原子调整 Sink 过滤阈值。

```c
bool xrtLogSinkSetLevel(xlogsink* pSink, xloglevel Level)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 非空 | 目标 Sink |
| `Level` | 输入 | 合法级别 | 新阈值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已更新 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 阈值调整

```c
	(void)xrtLogSinkSetLevel(pConsole, XLOG_WARN);
```


### `xrtLogSinkSubmit`

绕过 Logger 直接向一个 Sink 提交完整记录，供包装器和高级用户组合处理链。

```c
xlogresult xrtLogSinkSubmit(
	xlogsink* pSink,
	const xlogrecord* pRecord
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 非空 | 目标 Sink |
| `pRecord` | 输入 | 非空且通过校验 | 完整记录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 本 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被本 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 本 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或本 Sink 回调真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空或记录未通过 `xrtLogRecordValidate`
- `xrt.log` / `XLOG_ERROR_CALLBACK` — Sink 回调返回错误但未设置具体错误
- `XERR_INTERNAL` — Sink 回调返回越界的结果值

#### 范例

[async](../../examples/logging/async/main.c) · 直连提交

```c
	if ( xrtLogSinkSubmit(pAsync, &Record) != XLOG_RESULT_WRITTEN ) {
```


### `xrtLogSinkFlush`

提交 Sink 已经接受的内容；文件 Sink 落盘、异步 Sink 插入栅栏等待。

```c
bool xrtLogSinkFlush(xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 非空 | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已提交 | — |
| `false` | 参数非法或 Flush 失败 | `XERR_ARGUMENT` / `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` 域错误 — Flush 回调失败（如 `XLOG_ERROR_ASYNC_FLUSH`、文件同步错误）

#### 范例

[async](../../examples/logging/async/main.c) · 冲刷

```c
	if ( !xrtLogSinkFlush(pAsync) ) {
```


### `xrtLogSinkStats`

并发读取 Sink 的聚合统计快照。

```c
bool xrtLogSinkStats(const xlogsink* pSink, xlogstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 非空 | 目标 Sink |
| `pStats` | 输出 | 非空 | 接收快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 统计快照

```c
	if ( xrtLogSinkStats(pConsole, &Stats) ) {
```


## 附加与移除

### `xrtLogAttach`

把可共享 Sink 附加到 Logger；双方各增加引用，同一 Sink 可附加到多个 Logger。

```c
bool xrtLogAttach(xlogger* pLogger, xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入/输出 | 非空 | 目标 Logger |
| `pSink` | 输入 | 非空 | 要附加的 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已附加 | — |
| `false` | 失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_EXISTS` — 该 Sink 已经附加到本 Logger
- `XERR_STATE` — Sink 引用失败
- `XERR_MEMORY` — 快照扩容分配失败

#### 范例

[core](../../examples/logging/core/main.c) · 挂载 Sink

```c
		!xrtLogAttach(pLogger, pSink)
```


### `xrtLogDetach`

从 Logger 移除指定 Sink；未附加时返回失败且不设置错误。

```c
bool xrtLogDetach(xlogger* pLogger, xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入/输出 | 非空 | 目标 Logger |
| `pSink` | 输入 | 非空 | 要移除的 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已移除并释放对应引用 | — |
| `false` | 未附加 | 不设错 |

#### 错误

- 不设错误 — 未附加时返回 `false` 且不设置错误；句柄为空设置 `XERR_ARGUMENT`

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 卸载

```c
	printf("detach-single=%d", xrtLogDetach(pLogger, pConsole) ? 1 : 0);
```


### `xrtLogDetachAll`

移除 Logger 的全部 Sink，并返回实际移除数量。

```c
size_t xrtLogDetachAll(xlogger* pLogger)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入/输出 | 非空 | 目标 Logger |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际移除数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 全部卸载

```c
	printf(" detach-all=%zu\n", xrtLogDetachAll(pLogger));
```


### `xrtLogSinkCount`

返回并发快照中的 Sink 数量。

```c
size_t xrtLogSinkCount(xlogger* pLogger)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 当前附加数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 数量查询

```c
	printf(" count=%zu", xrtLogSinkCount(pLogger));
```


## 提交与统计

### `xrtLogSubmit`

提交完整记录；记录的 `Logger` 视图始终由目标 Logger 名称覆盖。

```c
xlogresult xrtLogSubmit(
	xlogger* pLogger,
	const xlogrecord* pRecord
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `pRecord` | 输入 | 非空且通过校验 | 完整记录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空或记录未通过 `xrtLogRecordValidate`
- `xrt.log` / `XLOG_ERROR_CALLBACK` — Sink 回调返回错误但未设置具体错误
- `XERR_INTERNAL` — Sink 回调返回越界的结果值

#### 范例

[logger_tour](../../examples/logging/logger_tour/main.c) · 完整记录提交

```c
		xrtLogSubmit(xrtLogDefault(), &Record) == XLOG_RESULT_WRITTEN ? 1 : 0);
```


### `xrtLog`

使用当前时间提交一条无字段文本记录。

```c
xlogresult xrtLog(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `Message` | 输入 | 借用 | 消息视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空或记录未通过 `xrtLogRecordValidate`
- `xrt.log` / `XLOG_ERROR_CALLBACK` — Sink 回调返回错误但未设置具体错误
- `XERR_INTERNAL` — Sink 回调返回越界的结果值

#### 范例

[console](../../examples/logging/console/main.c) · 文本提交

```c
	(void)xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("service started"));
```


### `xrtLogFields`

使用当前时间提交带结构化字段的记录。

```c
xlogresult xrtLogFields(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message,
	const xlogfield* pFields,
	size_t iFieldCount
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `Message` | 输入 | 借用 | 消息视图 |
| `pFields` | 输入 | 借用数组；计数为 0 时可空 | 字段数组 |
| `iFieldCount` | 输入 | — | 字段数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空或记录未通过 `xrtLogRecordValidate`
- `xrt.log` / `XLOG_ERROR_CALLBACK` — Sink 回调返回错误但未设置具体错误
- `XERR_INTERNAL` — Sink 回调返回越界的结果值

#### 范例

[core](../../examples/logging/core/main.c) · 字段提交

```c
		xrtLogFields(
			pLogger,
			XLOG_INFO,
			XRT_STR_LITERAL("request complete"),
			&Field,
			1u
		) != XLOG_RESULT_WRITTEN
```


### `xrtLogSource`

使用当前时间提交带完整源码位置和结构化字段的记录。

```c
xlogresult xrtLogSource(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message,
	const xlogfield* pFields,
	size_t iFieldCount,
	xstrview File,
	xstrview Function,
	uint32 iLine,
	uint64 iThreadId
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `Message` | 输入 | 借用 | 消息视图 |
| `pFields` | 输入 | 借用数组；计数为 0 时可空 | 字段数组 |
| `iFieldCount` | 输入 | — | 字段数量 |
| `File` | 输入 | 借用；可空视图 | 源文件名 |
| `Function` | 输入 | 借用；可空视图 | 函数名 |
| `iLine` | 输入 | — | 行号 |
| `iThreadId` | 输入 | — | 线程标识，0 = 不记录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空或记录未通过 `xrtLogRecordValidate`
- `xrt.log` / `XLOG_ERROR_CALLBACK` — Sink 回调返回错误但未设置具体错误
- `XERR_INTERNAL` — Sink 回调返回越界的结果值

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 源码元数据提交

```c
	(void)xrtLogSource(pLogger, XLOG_WARN, SV("direct source"), NULL, 0u,
		SV("demo.c"), SV("main"), 1u, 0u);
```


### `xrtLogFlush`

提交 Logger 全部 Sink 已经接受的内容。

```c
bool xrtLogFlush(xlogger* pLogger)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 全部 Sink 冲刷成功 | — |
| `false` | 任一 Sink 失败 | `XERR_ARGUMENT` / `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` 域错误 — 任一 Sink 的 Flush 失败

#### 范例

[console](../../examples/logging/console/main.c) · 全部冲刷

```c
	(void)xrtLogFlush(pLogger);
```


### `xrtLogStats`

并发读取 Logger 的聚合统计快照；写到多个 Sink 的一条记录只计一次 Logger 结果。

```c
bool xrtLogStats(const xlogger* pLogger, xlogstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `pStats` | 输出 | 非空 | 接收快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 统计快照

```c
		printf("log-stats=%d\n", xrtLogStats(pLogger, &LogStats) ? 1 : 0);
```


## 文本格式化

### `xrtLogTextConfigInit`

按完整、简单或纯消息预设初始化文本配置，调用方仍可逐位调整输出组成。

```c
bool xrtLogTextConfigInit(
	xlogtextconfig* pConfig,
	xlogtextstyle Style
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |
| `Style` | 输入 | 合法预设 | `XLOG_TEXT_FULL` / `SIMPLE` / `MESSAGE` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[format_text_buffer](../../examples/logging/format_text_buffer/main.c) · 配置预设

```c
	if ( !xrtLogTextConfigInit(&Config, XLOG_TEXT_LEVEL |
		XLOG_TEXT_MESSAGE) ) {
```


### `xrtLogTextConfigValidate`

校验文本标志组合和固定 UTC 偏移。

```c
bool xrtLogTextConfigValidate(const xlogtextconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 待校验配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 配置自洽 | — |
| `false` | 不自洽 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 配置校验

```c
			xrtLogTextConfigValidate(&TextConfig) ? 1 : 0);
```


### `xrtLogTextWrite`

无中间整行分配地把记录按配置分段写给同步 Writer；Writer 失败时 `pWritten` 返回已提交的精确字节数。

```c
bool xrtLogTextWrite(
	const xlogrecord* pRecord,
	const xlogtextconfig* pConfig,
	xlogwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRecord` | 输入 | 非空且通过校验 | 完整记录 |
| `pConfig` | 输入 | 非空且通过校验 | 文本配置 |
| `pWrite` | 输入 | 非空 | 字节 Writer 回调 |
| `pUserData` | 输入 | 任意值 | Writer 数据 |
| `pWritten` | 输出 | 允许空 | 接收写出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完整写出 | — |
| `false` | 记录、配置非法或 Writer 失败 | `XERR_ARGUMENT` / `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_TEXT_OUTPUT` — Writer 返回失败但未设置具体错误

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 流式写出

```c
		if ( xrtLogTextWrite(&(xlogrecord){ 0 }, &TextConfig,
			writeSink, stdout, &iSize) ) {
```


### `xrtLogText`

创建由 `xrtFree` 释放的完整文本记录，并返回不含末尾零字节的长度。

```c
str xrtLogText(
	const xlogrecord* pRecord,
	const xlogtextconfig* pConfig,
	size_t* pSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRecord` | 输入 | 非空且通过校验 | 完整记录 |
| `pConfig` | 输入 | 非空且通过校验 | 文本配置 |
| `pSize` | 输出 | 允许空 | 接收文本长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾文本，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_TEXT_OUTPUT` — Writer 返回失败但未设置具体错误
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[format_text_buffer](../../examples/logging/format_text_buffer/main.c) · 分配整行

```c
	sText = xrtLogText(&Record, &Config, &iSize);
```


## JSON Lines 格式化

### `xrtLogJsonConfigInit`

初始化完整、紧凑且以换行结束的 JSON Lines 配置。

```c
bool xrtLogJsonConfigInit(xlogjsonconfig* pConfig)
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

[json](../../examples/logging/json/main.c) · 默认配置

```c
		!xrtLogJsonConfigInit(&Config) ||
```


### `xrtLogJsonConfigValidate`

校验 JSON 标志、转义、字段表示、非有限数策略和错误原因深度。

```c
bool xrtLogJsonConfigValidate(const xlogjsonconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 待校验配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 配置自洽 | — |
| `false` | 不自洽 | `xrt.log` / `XLOG_ERROR_JSON_CONFIG` |

#### 错误

- `xrt.log` / `XLOG_ERROR_JSON_CONFIG` — 标志、策略组合或 `MaxErrorDepth` 非法

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 配置校验

```c
			xrtLogJsonConfigValidate(&JsonConfig) ? 1 : 0);
```


### `xrtLogJsonWrite`

无中间对象和整行分配地把记录按配置分段写给同步 Writer；默认输出顺序 `time`、`level`、`logger`、`message`、`source`、`thread`、`fields`。

```c
bool xrtLogJsonWrite(
	const xlogrecord* pRecord,
	const xlogjsonconfig* pConfig,
	xlogwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRecord` | 输入 | 非空且通过校验 | 完整记录 |
| `pConfig` | 输入 | 非空且通过校验 | JSON 配置 |
| `pWrite` | 输入 | 非空 | 字节 Writer 回调 |
| `pUserData` | 输入 | 任意值 | Writer 数据 |
| `pWritten` | 输出 | 允许空 | 接收写出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完整写出 | — |
| `false` | 记录、配置、UTF-8 非法或 Writer 失败 | `XERR_ARGUMENT` / `xrt.log` / `xrt.json` 错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_JSON_CONFIG` — 标志、策略组合或错误深度非法
- `xrt.log` / `XLOG_ERROR_JSON_OUTPUT` — Writer 失败但未设置具体错误
- `xrt.json` 域错误 — 字符串字段不是合法 UTF-8

#### 范例

[json](../../examples/logging/json/main.c) · 流式写出

```c
		!xrtLogJsonWrite(
			&Record,
			&Config,
			exampleLogWrite,
			stdout,
			NULL
		)
```


### `xrtLogJson`

创建由 `xrtFree` 释放的零结尾 JSON Lines 记录，并返回不含末尾零字节的长度。

```c
str xrtLogJson(
	const xlogrecord* pRecord,
	const xlogjsonconfig* pConfig,
	size_t* pSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRecord` | 输入 | 非空且通过校验 | 完整记录 |
| `pConfig` | 输入 | 非空且通过校验 | JSON 配置 |
| `pSize` | 输出 | 允许空 | 接收文本长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾 JSON 行，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_JSON_CONFIG` — 标志、策略组合或错误深度非法
- `xrt.log` / `XLOG_ERROR_JSON_OUTPUT` — Writer 失败但未设置具体错误
- `xrt.json` 域错误 — 字符串字段不是合法 UTF-8
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[format_json_buffer](../../examples/logging/format_json_buffer/main.c) · 分配整行

```c
	sJson = xrtLogJson(&Record, &Config, &iSize);
```


## Console Sink

### `xrtLogConsoleConfigInit`

初始化适合交互程序的默认配置：`INFO` 阈值、完整文本、`ERROR` 起写 `stderr`、逐条刷新和自动配色。

```c
bool xrtLogConsoleConfigInit(xlogconsoleconfig* pConfig)
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

[logger_tour](../../examples/logging/logger_tour/main.c) · 默认配置

```c
	(void)xrtLogConsoleConfigInit(&ConsoleConfig);
```


### `xrtLogConsole`

创建调用方拥有的线程安全 Console Sink；配置在创建时完整复制。

```c
xlogsink* xrtLogConsole(const xlogconsoleconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 空 = `xrtLogConsoleConfigInit` 默认值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Console Sink | — |
| `NULL` | 配置非法或分配失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 内部结构分配失败
- `xrt.log` / `XLOG_ERROR_CONSOLE_CONFIG` — 配置组合非法

#### 范例

[logger_tour](../../examples/logging/logger_tour/main.c) · 创建 Sink

```c
		xlogsink* pConsoleSink = xrtLogConsole(&ConsoleConfig);
```


### `xrtLogAddConsole`

创建 Console Sink 并附加到 Logger；成功后由 Logger 独占该引用。

```c
bool xrtLogAddConsole(
	xlogger* pLogger,
	const xlogconsoleconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `pConfig` | 输入 | 允许空 | 空 = 默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已创建并附加 | — |
| `false` | 创建或附加失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 内部结构分配失败
- `xrt.log` / `XLOG_ERROR_CONSOLE_CONFIG` — 配置组合非法

#### 范例

[console](../../examples/logging/console/main.c) · 创建并附加

```c
	if ( (pLogger == NULL) || !xrtLogAddConsole(pLogger, NULL) ) {
```


## File Sink

### `xrtLogFileOptionsInit`

初始化追加模式、`INFO` 阈值、16 MiB 单条上限、64 KiB 缓存保留和手动持久化选项，并复制路径文本。

```c
bool xrtLogFileOptionsInit(
	xlogfileoptions* pOptions,
	cstr sPath
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOptions` | 输出 | 非空 | 接收选项 |
| `sPath` | 输入 | 非空且非空串 | UTF-8 文件路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化并复制路径 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file_text](../../examples/logging/file_text/main.c) · 默认选项

```c
		!xrtLogFileOptionsInit(&Options, "example_logger_text.log") ||
```


### `xrtLogFile`

创建通用文件 Sink；成功后接管格式器 `UserData`，失败时数据仍归调用方。

```c
xlogsink* xrtLogFile(const xlogfileconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空且 `Format` 非空 | 选项与格式回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 文件 Sink | — |
| `NULL` | 配置非法或打开失败 | `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_FILE_CONFIG` — 选项组合非法
- `xrt.log` / `XLOG_ERROR_FILE_OPEN` — 打开失败，`xrt.file` 原因保留在原因链
- `XERR_OVERFLOW` — 路径长度引起尺寸溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[file](../../examples/logging/file/main.c) · 自定义格式器

```c
	pSink = xrtLogFile(&Config);
```


### `xrtLogFilePath`

返回文件 Sink 生命周期内稳定的借用 UTF-8 路径。

```c
cstr xrtLogFilePath(const xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 文件 Sink | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾路径借用 | — |
| `NULL` | 非文件 Sink 或空句柄 | 不设错 |

#### 错误

- 无错误 — 非文件 Sink 或空句柄返回 `NULL` 且不设置错误

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 路径查询

```c
				cstr sPath = xrtLogFilePath(pTextFileSink);
```


### `xrtLogFileStats`

并发读取文件 Sink 的统计快照：当前大小、累计写入、记录、滚动、reopen 和持久化次数。

```c
bool xrtLogFileStats(
	const xlogsink* pSink,
	xlogfilestats* pStats
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 文件 Sink | 目标 Sink |
| `pStats` | 输出 | 非空 | 接收统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 统计已写出 | — |
| `false` | 非文件 Sink 或参数非法 | 仅 `pStats` 为空时 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pStats` 为空；非文件 Sink 返回 `false` 且不设置错误

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 统计快照

```c
					(void)xrtLogFileStats(pTextFileSink, &FileStats);
```


### `xrtLogFileRotate`

立即执行滚动流程；零备份配置到达阈值后直接截断当前路径。

```c
bool xrtLogFileRotate(xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 文件 Sink | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已滚动 | — |
| `false` | 滚动失败 | `xrt.log` 错误 |

#### 错误

- `xrt.log` / `XLOG_ERROR_FILE_ROTATE` — 滚动失败，底层错误保留在原因链

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 立即滚动

```c
				(void)xrtLogFileRotate(pTextFileSink);
```


### `xrtLogFileReopen`

重新打开当前路径，供外部 `logrotate` 或路径替换后切换句柄；先开新句柄再关旧句柄。

```c
bool xrtLogFileReopen(xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 文件 Sink | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已切换到新句柄 | — |
| `false` | 重开失败 | `xrt.log` 错误 |

#### 错误

- `xrt.log` / `XLOG_ERROR_FILE_OPEN` — 新句柄打开失败，原句柄保持有效

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 重开句柄

```c
				(void)xrtLogFileReopen(pTextFileSink);
```


## 文件组合层

### `xrtLogTextFile`

使用复制的文本配置创建文件 Sink；空文本配置使用完整格式。

```c
xlogsink* xrtLogTextFile(
	const xlogfileoptions* pOptions,
	const xlogtextconfig* pText
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOptions` | 输入 | 非空 | 文件选项 |
| `pText` | 输入 | 允许空 | 空 = 完整文本格式 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 文本文件 Sink | — |
| `NULL` | 创建失败 | 同 `xrtLogFile` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_FILE_CONFIG` — 选项组合非法
- `xrt.log` / `XLOG_ERROR_FILE_OPEN` — 打开失败，`xrt.file` 原因保留在原因链
- `XERR_OVERFLOW` — 路径长度引起尺寸溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 文本文件 Sink

```c
			xlogsink* pTextFileSink = xrtLogTextFile(&Options, NULL);
```


### `xrtLogAddTextFile`

创建文本文件 Sink 并附加到 Logger；成功后由 Logger 独占该引用。

```c
bool xrtLogAddTextFile(
	xlogger* pLogger,
	const xlogfileoptions* pOptions,
	const xlogtextconfig* pText
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `pOptions` | 输入 | 非空 | 文件选项 |
| `pText` | 输入 | 允许空 | 空 = 完整文本格式 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已创建并附加 | — |
| `false` | 创建或附加失败 | 同 `xrtLogFile` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_FILE_CONFIG` — 选项组合非法
- `xrt.log` / `XLOG_ERROR_FILE_OPEN` — 打开失败，`xrt.file` 原因保留在原因链
- `XERR_OVERFLOW` — 路径长度引起尺寸溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[file_text](../../examples/logging/file_text/main.c) · 创建并附加

```c
		!xrtLogAddTextFile(pLogger, &Options, &Text) ||
```


### `xrtLogJsonFile`

使用复制的 JSON 配置创建文件 Sink；空 JSON 配置使用完整 JSON Lines。

```c
xlogsink* xrtLogJsonFile(
	const xlogfileoptions* pOptions,
	const xlogjsonconfig* pJson
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOptions` | 输入 | 非空 | 文件选项 |
| `pJson` | 输入 | 允许空 | 空 = 完整 JSON Lines |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | JSON 文件 Sink | — |
| `NULL` | 创建失败 | 同 `xrtLogFile` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_FILE_CONFIG` — 选项组合非法
- `xrt.log` / `XLOG_ERROR_FILE_OPEN` — 打开失败，`xrt.file` 原因保留在原因链
- `XERR_OVERFLOW` — 路径长度引起尺寸溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · JSON 文件 Sink

```c
			xlogsink* pJsonSink = xrtLogJsonFile(&Options, NULL);
```


### `xrtLogAddJsonFile`

创建 JSON 文件 Sink 并附加到 Logger；成功后由 Logger 独占该引用。

```c
bool xrtLogAddJsonFile(
	xlogger* pLogger,
	const xlogfileoptions* pOptions,
	const xlogjsonconfig* pJson
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `pOptions` | 输入 | 非空 | 文件选项 |
| `pJson` | 输入 | 允许空 | 空 = 完整 JSON Lines |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已创建并附加 | — |
| `false` | 创建或附加失败 | 同 `xrtLogFile` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.log` / `XLOG_ERROR_FILE_CONFIG` — 选项组合非法
- `xrt.log` / `XLOG_ERROR_FILE_OPEN` — 打开失败，`xrt.file` 原因保留在原因链
- `XERR_OVERFLOW` — 路径长度引起尺寸溢出
- `XERR_MEMORY` — 分配失败

#### 范例

[file_json](../../examples/logging/file_json/main.c) · 创建并附加

```c
		!xrtLogAddJsonFile(pLogger, &Options, NULL) ||   /* 一行挂载 */
```


## Async Sink

### `xrtLogAsyncConfigInit`

初始化无额外线程栈、`TRACE` 透传、丢弃最新记录和优雅排空的默认配置。

```c
bool xrtLogAsyncConfigInit(xlogasyncconfig* pConfig)
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

[async](../../examples/logging/async/main.c) · 默认配置

```c
	if ( !xrtLogAsyncConfigInit(&AsyncConfig) ) {
```


### `xrtLogAsync`

创建有界异步包装 Sink 并启动唯一顺序工作线程；目标只增加引用，不接管调用方引用。

```c
xlogsink* xrtLogAsync(
	xlogsink* pTarget,
	const xlogasyncconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入 | 非空 | 被包装的目标 Sink |
| `pConfig` | 输入 | 允许空 | 空 = 默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 异步包装 Sink | — |
| `NULL` | 创建或线程启动失败 | `xrt.log` 错误 |

#### 错误

- `xrt.log` / `XLOG_ERROR_ASYNC_CONFIG` — 目标为空或配置非法，kind 为 `XERR_ARGUMENT`
- `xrt.log` / `XLOG_ERROR_ASYNC_CONFIG` — 状态分配失败（`XERR_MEMORY`）或目标引用失败（`XERR_STATE`）
- `xrt.log` / `XLOG_ERROR_ASYNC_THREAD` — 工作线程启动失败

#### 范例

[async](../../examples/logging/async/main.c) · 创建包装器

```c
	pAsync = xrtLogAsync(pTarget, &AsyncConfig);
```


### `xrtLogAddAsync`

创建异步包装并附加到 Logger；成功后 Logger 独占新包装器的引用。

```c
bool xrtLogAddAsync(
	xlogger* pLogger,
	xlogsink* pTarget,
	const xlogasyncconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `pTarget` | 输入 | 非空 | 被包装的目标 Sink |
| `pConfig` | 输入 | 允许空 | 空 = 默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已创建并附加 | — |
| `false` | 创建或附加失败 | 同 `xrtLogAsync` |

#### 错误

- `xrt.log` / `XLOG_ERROR_ASYNC_CONFIG` — 目标为空或配置非法，kind 为 `XERR_ARGUMENT`
- `xrt.log` / `XLOG_ERROR_ASYNC_CONFIG` — 状态分配失败（`XERR_MEMORY`）或目标引用失败（`XERR_STATE`）
- `xrt.log` / `XLOG_ERROR_ASYNC_THREAD` — 工作线程启动失败

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 创建并附加

```c
		if ( !xrtLogAddAsync(pAsyncLogger, pTargetSink, &AsyncConfig) ) {
```


### `xrtLogAsyncTarget`

返回 Async Sink 生命周期内稳定的借用目标；错误类型的 Sink 返回空指针。

```c
xlogsink* xrtLogAsyncTarget(const xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 异步 Sink | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 目标 Sink 借用 | — |
| `NULL` | 非异步 Sink 或空句柄 | 不设错 |

#### 错误

- 无错误 — 非异步 Sink 或空句柄返回 `NULL` 且不设置错误

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 目标查询

```c
				xlogsink* pInner = xrtLogAsyncTarget(pAsyncWrapper);
```


### `xrtLogAsyncStats`

并发读取异步统计：入队、处理、各类丢弃、失败、当前队列和高水位。

```c
bool xrtLogAsyncStats(
	const xlogsink* pSink,
	xlogasyncstats* pStats
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 异步 Sink | 目标 Sink |
| `pStats` | 输出 | 非空 | 接收统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 统计已写出 | — |
| `false` | 非异步 Sink 或参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 统计快照

```c
			(void)xrtLogAsyncStats(NULL, &AsyncStats);
```


### `xrtLogAsyncLastError`

返回后台最近一次目标或 Flush 错误的新引用；用后 `xrtErrorFree` 释放。

```c
xerror* xrtLogAsyncLastError(const xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | 异步 Sink | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 错误新引用 | — |
| `NULL` | 尚无错误或非异步 Sink | 不设错 |

#### 错误

- 无错误 — 尚无后台错误时返回 `NULL` 且不设置错误

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 后台错误

```c
		pLastError = xrtLogAsyncLastError(NULL);
```


## Ring Sink

### `xrtLogRingConfigInit`

初始化无生产者分配、无生产者互斥等待的有界 Ring 默认配置。

```c
bool xrtLogRingConfigInit(xlogringconfig* pConfig)
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

[ring_async](../../examples/logging/ring_async/main.c) · 默认配置

```c
		(void)xrtLogRingConfigInit(&RingConfig);
```


### `xrtLogRing`

创建高吞吐 Ring 包装 Sink；目标只增加引用，不接管调用方引用。

```c
xlogsink* xrtLogRing(
	xlogsink* pTarget,
	const xlogringconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入 | 非空 | 被包装的目标 Sink |
| `pConfig` | 输入 | 允许空 | 空 = 默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Ring 包装 Sink | — |
| `NULL` | 创建或线程启动失败 | `xrt.log` 错误 |

#### 错误

- `xrt.log` / `XLOG_ERROR_RING_CONFIG` — 目标为空或配置非法，kind 为 `XERR_ARGUMENT`
- `xrt.log` / `XLOG_ERROR_RING_CONFIG` — 状态分配失败（`XERR_MEMORY`）或目标引用失败（`XERR_STATE`）
- `xrt.log` / `XLOG_ERROR_RING_THREAD` — 工作线程启动失败

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 创建包装器

```c
				pWrapper = xrtLogRing(pTarget2, &RingConfig2);
```


### `xrtLogAddRing`

创建 Ring 包装并附加到 Logger；成功后 Logger 独占新包装器引用。

```c
bool xrtLogAddRing(
	xlogger* pLogger,
	xlogsink* pTarget,
	const xlogringconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `pTarget` | 输入 | 非空 | 被包装的目标 Sink |
| `pConfig` | 输入 | 允许空 | 空 = 默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已创建并附加 | — |
| `false` | 创建或附加失败 | 同 `xrtLogRing` |

#### 错误

- `xrt.log` / `XLOG_ERROR_RING_CONFIG` — 目标为空或配置非法，kind 为 `XERR_ARGUMENT`
- `xrt.log` / `XLOG_ERROR_RING_CONFIG` — 状态分配失败（`XERR_MEMORY`）或目标引用失败（`XERR_STATE`）
- `xrt.log` / `XLOG_ERROR_RING_THREAD` — 工作线程启动失败

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 创建并附加

```c
		if ( !xrtLogAddRing(pLogger, pTargetSink, &RingConfig) ) {
```


### `xrtLogRingTarget`

返回 Ring 生命周期内稳定的借用目标；错误类型 Sink 返回空。

```c
xlogsink* xrtLogRingTarget(const xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | Ring Sink | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 目标 Sink 借用 | — |
| `NULL` | 非 Ring Sink 或空句柄 | 不设错 |

#### 错误

- 无错误 — 非 Ring Sink 或空句柄返回 `NULL` 且不设置错误

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 目标查询

```c
					xlogsink* pInner = xrtLogRingTarget(pWrapper);
```


### `xrtLogRingStats`

读取无锁统计快照：容量丢弃、记录超限、递归写入和目标结果；并发字段之间不承诺同一时刻一致。

```c
bool xrtLogRingStats(
	const xlogsink* pSink,
	xlogringstats* pStats
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | Ring Sink | 目标 Sink |
| `pStats` | 输出 | 非空 | 接收统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 统计已写出 | — |
| `false` | 非 Ring Sink 或参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 统计快照

```c
				(void)xrtLogRingStats(NULL, &RingStats);
```


### `xrtLogRingLastError`

返回后台最近一次错误的新引用；用后 `xrtErrorFree` 释放。

```c
xerror* xrtLogRingLastError(const xlogsink* pSink)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSink` | 输入 | Ring Sink | 目标 Sink |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 错误新引用 | — |
| `NULL` | 尚无错误或非 Ring Sink | 不设错 |

#### 错误

- 无错误 — 尚无后台错误时返回 `NULL` 且不设置错误

#### 范例

[ring_async](../../examples/logging/ring_async/main.c) · 后台错误

```c
			pLastError = xrtLogRingLastError(NULL);
```


## printf 提交

### `xrtLogPrintfV`

使用 printf 规则和已有参数列表提交常用日志；复用字符串模块的安全格式化器，拒绝 `%n`。

```c
xlogresult xrtLogPrintfV(
	xlogger* pLogger,
	xloglevel Level,
	cstr sFormat,
	va_list Args
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `Args` | 输入 | 已由 `va_start` 初始化 | 变参列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · va_list 提交

```c
	Result = xrtLogPrintfV(pLogger, XLOG_INFO, sFmt, Args);
```


### `xrtLogPrintf`

使用 printf 规则提交常用日志；已得到消息视图时应改用无分配的 `xrtLog`。

```c
xlogresult xrtLogPrintf(
	xlogger* pLogger,
	xloglevel Level,
	cstr sFormat,
	...
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `...` | 输入 | 与格式串匹配 | 变参 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配

#### 范例

[printf](../../examples/logging/printf/main.c) · 格式化提交

```c
		Result = xrtLogPrintf(
			pLogger,
			XLOG_INFO,
			"request=%u status=%u",
			42u,
			200u
		);
```


### `xrtLogFieldsPrintfV`

使用 printf 规则和已有参数列表提交带结构化字段的记录。

```c
xlogresult xrtLogFieldsPrintfV(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	cstr sFormat,
	va_list Args
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `pFields` | 输入 | 借用数组；计数为 0 时可空 | 字段数组 |
| `iFieldCount` | 输入 | — | 字段数量 |
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `Args` | 输入 | 已由 `va_start` 初始化 | 变参列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 字段 + va_list

```c
	Result = xrtLogFieldsPrintfV(pLogger, XLOG_INFO, NULL, 0u, sFmt, Args);
```


### `xrtLogFieldsPrintf`

使用 printf 规则提交带结构化字段的记录。

```c
xlogresult xrtLogFieldsPrintf(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	cstr sFormat,
	...
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `pFields` | 输入 | 借用数组；计数为 0 时可空 | 字段数组 |
| `iFieldCount` | 输入 | — | 字段数量 |
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `...` | 输入 | 与格式串匹配 | 变参 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 字段 + 格式化

```c
		(void)xrtLogFieldsPrintf(pLogger, XLOG_ERROR, Fields, 1u,
			"count=%d", 1);
```


### `xrtLogSourcePrintfV`

使用 printf 规则和已有参数列表提交带完整源码元数据的记录。

```c
xlogresult xrtLogSourcePrintfV(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	xstrview File,
	xstrview Function,
	uint32 iLine,
	uint64 iThreadId,
	cstr sFormat,
	va_list Args
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `pFields` | 输入 | 借用数组；计数为 0 时可空 | 字段数组 |
| `iFieldCount` | 输入 | — | 字段数量 |
| `File` | 输入 | 借用；可空视图 | 源文件名 |
| `Function` | 输入 | 借用；可空视图 | 函数名 |
| `iLine` | 输入 | — | 行号 |
| `iThreadId` | 输入 | — | 线程标识，0 = 不记录 |
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `Args` | 输入 | 已由 `va_start` 初始化 | 变参列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 完整元数据 + va_list

```c
	Result = xrtLogSourcePrintfV(pLogger, Level, NULL, 0u, SV("demo.c"), SV("main"), 1u, 0u, sFmt, Args);
```


### `xrtLogSourcePrintf`

使用 printf 规则提交带完整源码元数据的记录。

```c
xlogresult xrtLogSourcePrintf(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	xstrview File,
	xstrview Function,
	uint32 iLine,
	uint64 iThreadId,
	cstr sFormat,
	...
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLogger` | 输入 | 非空 | 目标 Logger |
| `Level` | 输入 | 合法级别 | 本条级别 |
| `pFields` | 输入 | 借用数组；计数为 0 时可空 | 字段数组 |
| `iFieldCount` | 输入 | — | 字段数量 |
| `File` | 输入 | 借用；可空视图 | 源文件名 |
| `Function` | 输入 | 借用；可空视图 | 函数名 |
| `iLine` | 输入 | — | 行号 |
| `iThreadId` | 输入 | — | 线程标识，0 = 不记录 |
| `sFormat` | 输入 | 非空 | printf 格式串 |
| `...` | 输入 | 与格式串匹配 | 变参 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLOG_RESULT_WRITTEN` | 至少一个 Sink 完成写入 | — |
| `XLOG_RESULT_SKIPPED` | 被 Logger 或 Sink 阈值过滤 | 不设错 |
| `XLOG_RESULT_DROPPED` | 未写入，但至少一个 Sink 按自身策略丢弃 | 不设错 |
| `XLOG_RESULT_ERROR` | 记录非法或至少一个 Sink 真实错误 | `XERR_ARGUMENT` 或 `xrt.log` 错误 |

#### 错误

- `XERR_ARGUMENT` — 句柄为空
- `xrt.str` 域错误 — 格式串非法（含 `%n`）或与实参不匹配

#### 范例

[sink_tour](../../examples/logging/sink_tour/main.c) · 完整元数据 + 格式化

```c
	(void)xrtLogSourcePrintf(pLogger, XLOG_WARN, NULL, 0u,
		SV("demo.c"), SV("main"), 1u, 0u, "src=%s", "printf");
```
