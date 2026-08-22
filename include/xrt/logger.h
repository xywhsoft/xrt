#ifndef XRT_LOGGER_H
#define XRT_LOGGER_H

#include <xrt/atomic.h>
#include <xrt/error.h>
#include <xrt/sync.h>
#include <xrt/time.h>

#if defined(XRT_FEATURE_LOGGER_RING)
	#include <xrt/queue.h>
#endif

#if defined(XRT_FEATURE_LOGGER_PRINTF)
	#include <xrt/string.h>
#endif

#if defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
	defined(XRT_FEATURE_LOGGER_FORMAT_JSON)
	#include <xrt/number.h>
#endif

#if defined(XRT_FEATURE_LOGGER_FORMAT_JSON)
	#include <xrt/json.h>
#endif

#if defined(XRT_FEATURE_LOGGER_FORMAT_TEXT_BUFFER) || \
	defined(XRT_FEATURE_LOGGER_FORMAT_JSON_BUFFER) || \
	defined(XRT_FEATURE_LOGGER_FILE)
	#include <xrt/buffer.h>
#endif

#if defined(XRT_FEATURE_LOGGER_FILE)
	#include <xrt/file.h>
#endif



#if defined(XRT_FEATURE_LOGGER_CORE) && \
	(!defined(XRT_FEATURE_ATOMIC) || \
	 !defined(XRT_FEATURE_MUTEX) || \
	 !defined(XRT_FEATURE_TIME))
	#error "XRT_FEATURE_LOGGER_CORE requires atomic, mutex and time"
#endif

#if defined(XRT_FEATURE_LOGGER_PRINTF) && \
	(!defined(XRT_FEATURE_LOGGER_CORE) || \
	 !defined(XRT_FEATURE_STRING_FORMAT))
	#error "XRT_FEATURE_LOGGER_PRINTF requires logger_core and string_format"
#endif

#if defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) && \
	(!defined(XRT_FEATURE_LOGGER_CORE) || \
	 !defined(XRT_FEATURE_NUMBER_INTEGER) || \
	 !defined(XRT_FEATURE_NUMBER_FLOAT))
	#error "XRT_FEATURE_LOGGER_FORMAT_TEXT requires logger_core and number formatting"
#endif

#if defined(XRT_FEATURE_LOGGER_FORMAT_TEXT_BUFFER) && \
	(!defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
	 !defined(XRT_FEATURE_BUFFER))
	#error "XRT_FEATURE_LOGGER_FORMAT_TEXT_BUFFER requires logger_format_text and buffer"
#endif

#if defined(XRT_FEATURE_LOGGER_FORMAT_JSON) && \
	(!defined(XRT_FEATURE_LOGGER_CORE) || \
	 !defined(XRT_FEATURE_JSON_ESCAPE) || \
	 !defined(XRT_FEATURE_NUMBER_INTEGER) || \
	 !defined(XRT_FEATURE_NUMBER_FLOAT))
	#error "XRT_FEATURE_LOGGER_FORMAT_JSON requires logger_core, JSON escape and number formatting"
#endif

#if defined(XRT_FEATURE_LOGGER_FORMAT_JSON_BUFFER) && \
	(!defined(XRT_FEATURE_LOGGER_FORMAT_JSON) || \
	 !defined(XRT_FEATURE_BUFFER))
	#error "XRT_FEATURE_LOGGER_FORMAT_JSON_BUFFER requires logger_format_json and buffer"
#endif

#if defined(XRT_FEATURE_LOGGER_CONSOLE) && \
	(!defined(XRT_FEATURE_LOGGER_CORE) || \
	 !defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
	 !defined(XRT_FEATURE_CONSOLE))
	#error "XRT_FEATURE_LOGGER_CONSOLE requires logger_core, logger_format_text and console"
#endif

#if defined(XRT_FEATURE_LOGGER_FILE) && \
	(!defined(XRT_FEATURE_LOGGER_CORE) || \
	 !defined(XRT_FEATURE_BUFFER) || \
	 !defined(XRT_FEATURE_FILE))
	#error "XRT_FEATURE_LOGGER_FILE requires logger_core, buffer and file"
#endif

#if defined(XRT_FEATURE_LOGGER_FILE_TEXT) && \
	(!defined(XRT_FEATURE_LOGGER_FILE) || \
	 !defined(XRT_FEATURE_LOGGER_FORMAT_TEXT))
	#error "XRT_FEATURE_LOGGER_FILE_TEXT requires logger_file and logger_format_text"
#endif

#if defined(XRT_FEATURE_LOGGER_FILE_JSON) && \
	(!defined(XRT_FEATURE_LOGGER_FILE) || \
	 !defined(XRT_FEATURE_LOGGER_FORMAT_JSON))
	#error "XRT_FEATURE_LOGGER_FILE_JSON requires logger_file and logger_format_json"
#endif

#if defined(XRT_FEATURE_LOGGER_ASYNC) && \
	(!defined(XRT_FEATURE_LOGGER_CORE) || \
	 !defined(XRT_FEATURE_COND) || \
	 !defined(XRT_FEATURE_EVENT) || \
	 !defined(XRT_FEATURE_THREAD))
	#error "XRT_FEATURE_LOGGER_ASYNC requires logger_core, cond, event and thread"
#endif

#if defined(XRT_FEATURE_LOGGER_RING) && \
	(!defined(XRT_FEATURE_LOGGER_CORE) || \
	 !defined(XRT_FEATURE_QUEUE_MPSC) || \
	 !defined(XRT_FEATURE_QUEUE_MPMC) || \
	 !defined(XRT_FEATURE_THREAD))
	#error "XRT_FEATURE_LOGGER_RING requires logger_core, queue_mpsc, queue_mpmc and thread"
#endif



#if defined(XRT_FEATURE_LOGGER_CORE)

/* 日志级别从最详细到最严重排列，OFF 只用于过滤阈值。 */
typedef enum xloglevel {
	XLOG_TRACE = 0,
	XLOG_DEBUG,
	XLOG_INFO,
	XLOG_WARN,
	XLOG_ERROR,
	XLOG_FATAL,
	XLOG_OFF
} xloglevel;



/* 日志结果把正常过滤、成功写入、主动丢弃和真实错误分开表达。 */
typedef enum xlogresult {
	XLOG_RESULT_ERROR = -1,
	XLOG_RESULT_SKIPPED = 0,
	XLOG_RESULT_WRITTEN = 1,
	XLOG_RESULT_DROPPED = 2
} xlogresult;



/* xrt.log 域错误码在各个日志分层之间保持稳定。 */
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



/* 结构化字段类型不依赖 Value 容器，保持 Logger 核心轻量。 */
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



/* 一个字段借用名称和值；Sink 必须在回调返回前完成消费或复制。 */
typedef struct xlogfield {
	xstrview Name;
	xlogfieldtype Type;
	union {
		bool Boolean;
		int64 Integer;
		uint64 Unsigned;
		double Float;
		xstrview String;
		xtime Time;
		const xerror* Error;
	} Value;
} xlogfield;



/* 一条记录的所有视图和字段只在提交调用期间有效。 */
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



/* 统计按记录聚合；一个记录写到多个 Sink 仍只计一次 Logger 结果。 */
typedef struct xlogstats {
	uint64 Submitted;
	uint64 Written;
	uint64 Skipped;
	uint64 Dropped;
	uint64 Failed;
} xlogstats;



/* 通用字节 Writer 必须在返回前消费借用数据。 */
typedef bool (*xlogwriteproc)(xbytesview Data, ptr pUserData);



/* 通用格式器同步地把一条借用记录写给 Writer。 */
typedef bool (*xlogformatproc)(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
);



/* 格式器数据释放回调只在成功创建的拥有者销毁时执行。 */
typedef void (*xlogformatdropproc)(ptr pUserData);



/* Logger 和 Sink 都是线程安全的引用对象。 */
typedef struct xlogger xlogger;
typedef struct xlogsink xlogsink;



/* Sink 回调同步消费借用记录，并返回稳定的流控结果。 */
typedef xlogresult (*xlogsinkwriteproc)(
	const xlogrecord* pRecord,
	ptr pUserData
);



/* Flush 回调提交已经接受的内容；空回调等价于成功。 */
typedef bool (*xlogsinkflushproc)(ptr pUserData);



/* Drop 回调在最后一个 Sink 引用释放后接收用户数据。 */
typedef void (*xlogsinkdropproc)(ptr pUserData);



/* 自定义 Sink 配置在创建成功后把 UserData 生命周期交给 Sink。 */
typedef struct xlogsinkconfig {
	xstrview Name;
	xloglevel Level;
	xlogsinkwriteproc Write;
	xlogsinkflushproc Flush;
	xlogsinkdropproc Drop;
	ptr UserData;
} xlogsinkconfig;



XRT_EXTERN_C_BEGIN



/* 返回稳定的英文日志级别名称。 */
XRT_API cstr xrtLogLevelName(xloglevel Level);



/* 校验记录级别、全部视图、字段范围和字段类型。 */
XRT_API bool xrtLogRecordValidate(const xlogrecord* pRecord);



/* 构造不拥有数据的结构化字段。 */
XRT_API xlogfield xrtLogFieldNull(xstrview Name);
XRT_API xlogfield xrtLogFieldBool(xstrview Name, bool bValue);
XRT_API xlogfield xrtLogFieldInt(xstrview Name, int64 iValue);
XRT_API xlogfield xrtLogFieldUInt(xstrview Name, uint64 iValue);
XRT_API xlogfield xrtLogFieldFloat(xstrview Name, double fValue);
XRT_API xlogfield xrtLogFieldString(xstrview Name, xstrview Value);
XRT_API xlogfield xrtLogFieldTime(xstrview Name, xtime iValue);
XRT_API xlogfield xrtLogFieldError(xstrview Name, const xerror* pError);



/* 创建同步 Logger；名称被复制，初始没有 Sink。 */
XRT_API xlogger* xrtLogCreate(xstrview Name, xloglevel Level);



/* 增加 Logger 引用并返回原指针。 */
XRT_API xlogger* xrtLogRef(xlogger* pLogger);



/* 释放 Logger 引用；空指针不执行操作。 */
XRT_API void xrtLogFree(xlogger* pLogger);



/* 返回 Logger 生命周期内稳定的借用名称。 */
XRT_API xstrview xrtLogName(const xlogger* pLogger);



/* 读取或修改 Logger 的并发过滤阈值。 */
XRT_API xloglevel xrtLogLevel(const xlogger* pLogger);
XRT_API bool xrtLogSetLevel(xlogger* pLogger, xloglevel Level);



/* 创建可被多个 Logger 和包装 Sink 共享的自定义 Sink。 */
XRT_API xlogsink* xrtLogSinkCreate(const xlogsinkconfig* pConfig);



/* 增加或释放 Sink 引用。 */
XRT_API xlogsink* xrtLogSinkRef(xlogsink* pSink);
XRT_API void xrtLogSinkFree(xlogsink* pSink);



/* 返回 Sink 生命周期内稳定的借用名称。 */
XRT_API xstrview xrtLogSinkName(const xlogsink* pSink);



/* 读取或修改 Sink 的并发过滤阈值。 */
XRT_API xloglevel xrtLogSinkLevel(const xlogsink* pSink);
XRT_API bool xrtLogSinkSetLevel(xlogsink* pSink, xloglevel Level);



/* 直接向一个 Sink 提交完整记录，供包装器和高级用户组合处理链。 */
XRT_API xlogresult xrtLogSinkSubmit(
	xlogsink* pSink,
	const xlogrecord* pRecord
);



/* 提交 Sink 已接受的内容。 */
XRT_API bool xrtLogSinkFlush(xlogsink* pSink);



/* 读取 Sink 的并发统计快照。 */
XRT_API bool xrtLogSinkStats(const xlogsink* pSink, xlogstats* pStats);



/* 把可共享 Sink 附加到 Logger；重复附加返回 exists 错误。 */
XRT_API bool xrtLogAttach(xlogger* pLogger, xlogsink* pSink);



/* 从 Logger 移除 Sink；未附加时返回 false 且不设置错误。 */
XRT_API bool xrtLogDetach(xlogger* pLogger, xlogsink* pSink);



/* 移除 Logger 的全部 Sink，并返回实际移除数量。 */
XRT_API size_t xrtLogDetachAll(xlogger* pLogger);



/* 返回并发快照中的 Sink 数量。 */
XRT_API size_t xrtLogSinkCount(xlogger* pLogger);



/* 提交完整记录；Logger 字段始终由目标 Logger 名称覆盖。 */
XRT_API xlogresult xrtLogSubmit(
	xlogger* pLogger,
	const xlogrecord* pRecord
);



/* 使用当前时间提交一条常用文本记录。 */
XRT_API xlogresult xrtLog(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message
);



/* 使用当前时间提交带结构化字段的记录。 */
XRT_API xlogresult xrtLogFields(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message,
	const xlogfield* pFields,
	size_t iFieldCount
);



/* 使用当前时间提交带源码位置和结构化字段的记录。 */
XRT_API xlogresult xrtLogSource(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message,
	const xlogfield* pFields,
	size_t iFieldCount,
	xstrview File,
	xstrview Function,
	uint32 iLine,
	uint64 iThreadId
);



/* 提交 Logger 全部 Sink 已经接受的内容。 */
XRT_API bool xrtLogFlush(xlogger* pLogger);



/* 读取 Logger 的并发统计快照。 */
XRT_API bool xrtLogStats(const xlogger* pLogger, xlogstats* pStats);



/* 返回进程默认 Logger 的新引用；未设置时返回空且不设置错误。 */
XRT_API xlogger* xrtLogDefault(void);



/* 原子替换进程默认 Logger；空指针用于清除。 */
XRT_API bool xrtLogSetDefault(xlogger* pLogger);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_FORMAT_TEXT)

/* 三种预设只初始化配置，调用方仍可逐位调整输出组成。 */
typedef enum xlogtextstyle {
	XLOG_TEXT_FULL = 0,
	XLOG_TEXT_SIMPLE,
	XLOG_TEXT_MESSAGE
} xlogtextstyle;



/* 文本标志分别控制前缀、元数据、字段、换行和消息原样输出。 */
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



/* 文本配置使用固定 UTC 偏移，默认完整格式为 UTC 单行文本。 */
typedef struct xlogtextconfig {
	uint32 Flags;
	int UtcOffset;
} xlogtextconfig;



XRT_EXTERN_C_BEGIN



/* 按完整、简单或纯消息预设初始化文本配置。 */
XRT_API bool xrtLogTextConfigInit(
	xlogtextconfig* pConfig,
	xlogtextstyle Style
);



/* 校验文本标志和固定 UTC 偏移，并在失败时设置参数错误。 */
XRT_API bool xrtLogTextConfigValidate(const xlogtextconfig* pConfig);



/* 无中间整行分配地把记录格式化到同步 Writer。 */
XRT_API bool xrtLogTextWrite(
	const xlogrecord* pRecord,
	const xlogtextconfig* pConfig,
	xlogwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_FORMAT_TEXT_BUFFER)

XRT_EXTERN_C_BEGIN



/* 创建由 xrtFree 释放的文本记录，并返回不含末尾零字节的长度。 */
XRT_API str xrtLogText(
	const xlogrecord* pRecord,
	const xlogtextconfig* pConfig,
	size_t* pSize
);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_FORMAT_JSON)

#define XLOG_JSON_ERROR_DEPTH_DEFAULT 16u



/* JSON 字段默认写成对象，数组形式可无损保留重名和字段类型。 */
typedef enum xlogjsonfieldstyle {
	XLOG_JSON_FIELDS_OBJECT = 0,
	XLOG_JSON_FIELDS_ARRAY
} xlogjsonfieldstyle;



/* 非有限浮点值必须由调用方明确选择拒绝、null 或字符串表示。 */
typedef enum xlogjsonnonfinite {
	XLOG_JSON_NONFINITE_REJECT = 0,
	XLOG_JSON_NONFINITE_NULL,
	XLOG_JSON_NONFINITE_STRING
} xlogjsonnonfinite;



/* JSON 标志分别控制顶层元数据、字段和 JSON Lines 换行。 */
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



/* JSON 配置独立约束转义、字段表示、非有限数和错误原因链。 */
typedef struct xlogjsonconfig {
	uint32 Flags;
	uint32 EscapeFlags;
	xlogjsonfieldstyle FieldStyle;
	xlogjsonnonfinite NonFinite;
	size_t MaxErrorDepth;
} xlogjsonconfig;



XRT_EXTERN_C_BEGIN



/* 初始化完整、紧凑且以换行结束的 JSON Lines 配置。 */
XRT_API bool xrtLogJsonConfigInit(xlogjsonconfig* pConfig);



/* 校验 JSON 标志、策略和错误原因深度，并在失败时设置配置错误。 */
XRT_API bool xrtLogJsonConfigValidate(const xlogjsonconfig* pConfig);



/* 无中间对象和整行分配地把记录格式化到同步 Writer。 */
XRT_API bool xrtLogJsonWrite(
	const xlogrecord* pRecord,
	const xlogjsonconfig* pConfig,
	xlogwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_FORMAT_JSON_BUFFER)

XRT_EXTERN_C_BEGIN



/* 创建由 xrtFree 释放的 JSON Lines 记录，并返回不含末尾零字节的长度。 */
XRT_API str xrtLogJson(
	const xlogrecord* pRecord,
	const xlogjsonconfig* pConfig,
	size_t* pSize
);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_CONSOLE)

/* 控制台目标可固定到一个流，或按级别分流。 */
typedef enum xlogconsoletarget {
	XLOG_CONSOLE_STDOUT = 0,
	XLOG_CONSOLE_STDERR,
	XLOG_CONSOLE_SPLIT
} xlogconsoletarget;



/* 自动配色只对支持 ANSI 的交互终端生效，并尊重 NO_COLOR。 */
typedef enum xlogconsolecolor {
	XLOG_CONSOLE_COLOR_AUTO = 0,
	XLOG_CONSOLE_COLOR_NEVER,
	XLOG_CONSOLE_COLOR_ALWAYS
} xlogconsolecolor;



/* Console Sink 配置在创建时完整复制，后续修改不影响已创建对象。 */
typedef struct xlogconsoleconfig {
	xloglevel Level;
	xlogconsoletarget Target;
	xlogconsolecolor Color;
	xloglevel ErrorLevel;
	bool Flush;
	xlogtextconfig Text;
} xlogconsoleconfig;



XRT_EXTERN_C_BEGIN



/* 初始化 INFO、自动配色、ERROR 分流、逐条刷新和完整文本格式。 */
XRT_API bool xrtLogConsoleConfigInit(xlogconsoleconfig* pConfig);



/* 创建调用方拥有的线程安全 Console Sink。 */
XRT_API xlogsink* xrtLogConsole(const xlogconsoleconfig* pConfig);



/* 创建并附加 Console Sink，成功后由 Logger 独占该引用。 */
XRT_API bool xrtLogAddConsole(
	xlogger* pLogger,
	const xlogconsoleconfig* pConfig
);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_FILE)

#define XLOG_FILE_RECORD_LIMIT_DEFAULT (16u * 1024u * 1024u)
#define XLOG_FILE_BUFFER_LIMIT_DEFAULT (64u * 1024u)



/* 启动模式只影响首次打开；reopen 和滚动后的文件始终按追加语义打开。 */
typedef enum xlogfilemode {
	XLOG_FILE_APPEND = 0,
	XLOG_FILE_TRUNCATE
} xlogfilemode;



/* 文件持久化可以完全手动、逐条执行，或在写入时按单调时钟间隔执行。 */
typedef enum xlogfilesync {
	XLOG_FILE_SYNC_MANUAL = 0,
	XLOG_FILE_SYNC_RECORD,
	XLOG_FILE_SYNC_INTERVAL
} xlogfilesync;



/* 文件选项在创建时完整复制，Path 文本也会被独立复制。 */
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



/* 通用文件配置在创建成功后把格式器数据生命周期交给 Sink。 */
typedef struct xlogfileconfig {
	xlogfileoptions Options;
	xlogformatproc Format;
	xlogformatdropproc Drop;
	ptr UserData;
} xlogfileconfig;



/* 文件统计区分当前文件大小和进程内累计写入量。 */
typedef struct xlogfilestats {
	uint64 CurrentBytes;
	uint64 WrittenBytes;
	uint64 Records;
	uint64 Rotations;
	uint64 Reopens;
	uint64 Syncs;
} xlogfilestats;



XRT_EXTERN_C_BEGIN



/* 初始化追加、INFO、16 MiB 单条上限、64 KiB 缓存保留和手动持久化选项。 */
XRT_API bool xrtLogFileOptionsInit(
	xlogfileoptions* pOptions,
	cstr sPath
);



/* 创建调用方拥有的线程安全文件 Sink；成功后接管格式器数据。 */
XRT_API xlogsink* xrtLogFile(const xlogfileconfig* pConfig);



/* 返回文件 Sink 生命周期内稳定的借用 UTF-8 路径。 */
XRT_API cstr xrtLogFilePath(const xlogsink* pSink);



/* 读取文件 Sink 的并发统计快照。 */
XRT_API bool xrtLogFileStats(
	const xlogsink* pSink,
	xlogfilestats* pStats
);



/* 立即滚动当前文件；零备份配置会截断当前路径。 */
XRT_API bool xrtLogFileRotate(xlogsink* pSink);



/* 重新打开当前路径，供外部 logrotate 或路径替换后切换句柄。 */
XRT_API bool xrtLogFileReopen(xlogsink* pSink);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_FILE_TEXT)

XRT_EXTERN_C_BEGIN



/* 使用复制的文本配置创建文件 Sink；空文本配置使用完整格式。 */
XRT_API xlogsink* xrtLogTextFile(
	const xlogfileoptions* pOptions,
	const xlogtextconfig* pText
);



/* 创建并附加文本文件 Sink，成功后由 Logger 独占该引用。 */
XRT_API bool xrtLogAddTextFile(
	xlogger* pLogger,
	const xlogfileoptions* pOptions,
	const xlogtextconfig* pText
);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_FILE_JSON)

XRT_EXTERN_C_BEGIN



/* 使用复制的 JSON 配置创建文件 Sink；空 JSON 配置使用完整 JSON Lines。 */
XRT_API xlogsink* xrtLogJsonFile(
	const xlogfileoptions* pOptions,
	const xlogjsonconfig* pJson
);



/* 创建并附加 JSON 文件 Sink，成功后由 Logger 独占该引用。 */
XRT_API bool xrtLogAddJsonFile(
	xlogger* pLogger,
	const xlogfileoptions* pOptions,
	const xlogjsonconfig* pJson
);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_ASYNC)

#define XLOG_ASYNC_CAPACITY_DEFAULT 1024u
#define XLOG_ASYNC_RECORD_LIMIT_DEFAULT (1024u * 1024u)
#define XLOG_ASYNC_BYTE_LIMIT_DEFAULT (8u * 1024u * 1024u)



/* 队列满载策略明确区分业务背压、新记录丢弃和旧记录覆盖。 */
typedef enum xlogasyncfull {
	XLOG_ASYNC_BLOCK = 0,
	XLOG_ASYNC_DROP_NEWEST,
	XLOG_ASYNC_DROP_OLDEST
} xlogasyncfull;



/* 最后一个 Async Sink 引用释放时可以排空队列，也可以显式放弃未处理记录。 */
typedef enum xlogasyncshutdown {
	XLOG_ASYNC_DRAIN = 0,
	XLOG_ASYNC_DISCARD
} xlogasyncshutdown;



/* Async Sink 保持单工作线程顺序，并同时限制排队记录数和真实记录字节数。 */
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



/* 异步统计区分入队、目标结果、各类丢弃和当前队列高水位。 */
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



XRT_EXTERN_C_BEGIN



/* 初始化无额外线程栈、TRACE 透传、丢弃最新记录和优雅排空的默认配置。 */
XRT_API bool xrtLogAsyncConfigInit(xlogasyncconfig* pConfig);



/* 创建调用方拥有的异步包装 Sink；目标 Sink 只增加引用，不转移调用方引用。 */
XRT_API xlogsink* xrtLogAsync(
	xlogsink* pTarget,
	const xlogasyncconfig* pConfig
);



/* 创建异步包装并附加到 Logger，成功后 Logger 独占新包装器的引用。 */
XRT_API bool xrtLogAddAsync(
	xlogger* pLogger,
	xlogsink* pTarget,
	const xlogasyncconfig* pConfig
);



/* 返回 Async Sink 生命周期内稳定的借用目标；错误类型的 Sink 返回空指针。 */
XRT_API xlogsink* xrtLogAsyncTarget(const xlogsink* pSink);



/* 读取 Async Sink 的并发统计快照。 */
XRT_API bool xrtLogAsyncStats(
	const xlogsink* pSink,
	xlogasyncstats* pStats
);



/* 返回后台最近一次错误的新引用；尚无错误时返回空且不设置错误。 */
XRT_API xerror* xrtLogAsyncLastError(const xlogsink* pSink);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_RING)

#define XLOG_RING_CAPACITY_DEFAULT 1024u
#define XLOG_RING_RECORD_LIMIT_DEFAULT 4096u
#define XLOG_RING_BATCH_DEFAULT 64u
#define XLOG_RING_BATCH_MAX 256u
#define XLOG_RING_IDLE_WAIT_DEFAULT 100u



/* Ring 预分配固定记录槽；满载和超长记录均立即丢弃，不反向阻塞业务线程。 */
typedef struct xlogringconfig {
	xstrview Name;
	xloglevel Level;
	size_t Capacity;
	size_t RecordLimit;
	size_t Batch;
	size_t StackSize;
	uint64 IdleWait;
} xlogringconfig;



/* Ring 统计区分容量丢弃、记录超限、递归写入和目标 Sink 结果。 */
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



XRT_EXTERN_C_BEGIN



/* 初始化无生产者分配、无生产者互斥等待的有界 Ring 默认配置。 */
XRT_API bool xrtLogRingConfigInit(xlogringconfig* pConfig);



/* 创建高吞吐 Ring 包装 Sink；目标只增加引用，不转移调用方引用。 */
XRT_API xlogsink* xrtLogRing(
	xlogsink* pTarget,
	const xlogringconfig* pConfig
);



/* 创建 Ring 包装并附加到 Logger，成功后 Logger 独占新包装器引用。 */
XRT_API bool xrtLogAddRing(
	xlogger* pLogger,
	xlogsink* pTarget,
	const xlogringconfig* pConfig
);



/* 返回 Ring 生命周期内稳定的借用目标；错误类型 Sink 返回空。 */
XRT_API xlogsink* xrtLogRingTarget(const xlogsink* pSink);



/* 读取无锁统计快照；并发字段之间不承诺同一时刻的一致性。 */
XRT_API bool xrtLogRingStats(
	const xlogsink* pSink,
	xlogringstats* pStats
);



/* 返回后台最近一次错误的新引用；尚无错误时返回空且不设置错误。 */
XRT_API xerror* xrtLogRingLastError(const xlogsink* pSink);



XRT_EXTERN_C_END
#endif



#if defined(XRT_FEATURE_LOGGER_PRINTF)

XRT_EXTERN_C_BEGIN



/* 使用 printf 规则和已有参数列表提交常用日志。 */
XRT_API xlogresult xrtLogPrintfV(
	xlogger* pLogger,
	xloglevel Level,
	cstr sFormat,
	va_list Args
);



/* 使用 printf 规则提交常用日志。 */
XRT_API xlogresult xrtLogPrintf(
	xlogger* pLogger,
	xloglevel Level,
	cstr sFormat,
	...
);



/* 使用 printf 规则和已有参数列表提交结构化字段。 */
XRT_API xlogresult xrtLogFieldsPrintfV(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	cstr sFormat,
	va_list Args
);



/* 使用 printf 规则提交结构化字段。 */
XRT_API xlogresult xrtLogFieldsPrintf(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	cstr sFormat,
	...
);



/* 使用 printf 规则和已有参数列表提交完整源码元数据。 */
XRT_API xlogresult xrtLogSourcePrintfV(
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
);



/* 使用 printf 规则提交完整源码元数据。 */
XRT_API xlogresult xrtLogSourcePrintf(
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
);



XRT_EXTERN_C_END
#endif

#endif
