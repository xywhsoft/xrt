#ifndef XRT_PROCESS_H
#define XRT_PROCESS_H

#include <xrt/core.h>
#include <xrt/error.h>
#include <xrt/wait.h>

#if defined(XRT_FEATURE_PROCESS_RUN)
	#include <xrt/cancel.h>
#endif

#if defined(XRT_FEATURE_PROCESS_FUTURE)
	#include <xrt/future.h>
#endif

#if defined(XRT_FEATURE_PROCESS_FILE)
	#include <xrt/file.h>
#endif



#if defined(XRT_FEATURE_PROCESS) && \
	(!defined(XRT_FEATURE_THREAD) || \
	 !defined(XRT_FEATURE_MUTEX) || \
	 !defined(XRT_FEATURE_COND) || \
	 !defined(XRT_FEATURE_UNICODE))
	#error "XRT_FEATURE_PROCESS requires thread, mutex, cond and unicode"
#endif

#if defined(XRT_FEATURE_PROCESS_RUN) && \
	(!defined(XRT_FEATURE_PROCESS) || \
	 !defined(XRT_FEATURE_BUFFER) || \
	 !defined(XRT_FEATURE_CANCEL))
	#error "XRT_FEATURE_PROCESS_RUN requires process, buffer and cancel"
#endif

#if defined(XRT_FEATURE_PROCESS_OPEN) && \
	!defined(XRT_FEATURE_PROCESS)
	#error "XRT_FEATURE_PROCESS_OPEN requires process"
#endif

#if defined(XRT_FEATURE_PROCESS_PIPELINE) && \
	!defined(XRT_FEATURE_PROCESS_RUN)
	#error "XRT_FEATURE_PROCESS_PIPELINE requires process_run"
#endif

#if defined(XRT_FEATURE_PROCESS_FUTURE) && \
	(!defined(XRT_FEATURE_PROCESS) || !defined(XRT_FEATURE_FUTURE))
	#error "XRT_FEATURE_PROCESS_FUTURE requires process and future"
#endif

#if defined(XRT_FEATURE_PROCESS_TERMINAL) && \
	!defined(XRT_FEATURE_PROCESS)
	#error "XRT_FEATURE_PROCESS_TERMINAL requires process"
#endif

#if defined(XRT_FEATURE_PROCESS_FILE) && \
	(!defined(XRT_FEATURE_PROCESS) || !defined(XRT_FEATURE_FILE))
	#error "XRT_FEATURE_PROCESS_FILE requires process and file"
#endif



#if defined(XRT_FEATURE_PROCESS)

/* xrt.process 域错误码稳定区分配置、平台操作和资源边界。 */
typedef enum xprocesserror {
	XPROCESS_ERROR_ARGUMENT = 1,
	XPROCESS_ERROR_CONFIG,
	XPROCESS_ERROR_COMMAND,
	XPROCESS_ERROR_ENVIRONMENT,
	XPROCESS_ERROR_PIPE,
	XPROCESS_ERROR_SPAWN,
	XPROCESS_ERROR_OPEN,
	XPROCESS_ERROR_WAIT,
	XPROCESS_ERROR_READ,
	XPROCESS_ERROR_WRITE,
	XPROCESS_ERROR_CLOSE,
	XPROCESS_ERROR_SIGNAL,
	XPROCESS_ERROR_CALLBACK,
	XPROCESS_ERROR_TERMINAL,
	XPROCESS_ERROR_THREAD,
	XPROCESS_ERROR_LIMIT
} xprocesserror;



/* 直接执行不经过命令解释器；Shell 模式只用于明确需要解释语法的命令。 */
typedef enum xprocesstarget {
	XPROCESS_EXEC = 0,
	XPROCESS_SHELL = 1
} xprocesstarget;



/* 标准流可以继承、建立父子管道、连接空设备或复制调用方原生句柄。 */
typedef enum xprocessiomode {
	XPROCESS_IO_INHERIT = 0,
	XPROCESS_IO_PIPE,
	XPROCESS_IO_NULL,
	XPROCESS_IO_HANDLE,
	XPROCESS_IO_MERGE
} xprocessiomode;



/* 标准流标识同时用于读写、关闭、原生句柄和输出回调。 */
typedef enum xprocessstream {
	XPROCESS_STDIN = 0,
	XPROCESS_STDOUT,
	XPROCESS_STDERR
} xprocessstream;



/* 成功启动后的进程只有运行与退出两种公共状态。 */
typedef enum xprocessstate {
	XPROCESS_RUNNING = 0,
	XPROCESS_EXITED = 1
} xprocessstate;



/* 正常退出、信号退出和平台等待失败保持互斥。 */
typedef enum xprocessexitkind {
	XPROCESS_EXIT_NONE = 0,
	XPROCESS_EXIT_CODE,
	XPROCESS_EXIT_SIGNAL,
	XPROCESS_EXIT_LOST
} xprocessexitkind;



/* 停止强度逐级增加，KILL_TREE 以创建时的进程组为边界。 */
typedef enum xprocessstop {
	XPROCESS_STOP_NONE = 0,
	XPROCESS_STOP_INTERRUPT,
	XPROCESS_STOP_TERMINATE,
	XPROCESS_STOP_KILL,
	XPROCESS_STOP_KILL_TREE
} xprocessstop;



/* HANDLE 模式借用原生句柄，Spawn 在返回前完成复制，不接管调用方句柄。 */
typedef struct xprocessio {
	xprocessiomode Mode;
	intptr_t Handle;
} xprocessio;



/* Value 为空表示从子进程环境删除变量，非空值允许为空字符串。 */
typedef struct xprocessenv {
	cstr Name;
	cstr Value;
} xprocessenv;



/* 退出状态由进程对象保存，成功等待后可重复读取。 */
typedef struct xprocessstatus {
	xprocessexitkind Kind;
	int32 Code;
	int32 Signal;
	xprocessstop Stop;
	bool CoreDumped;
} xprocessstatus;



/*
	配置中的全部字符串、数组和原生句柄只借用到 Spawn 返回。
	Args 不包含 argv[0]；Arg0 为空时直接使用 Program。
*/
typedef struct xprocessconfig {
	xprocesstarget Target;
	cstr Program;
	cstr Arg0;
	const cstr* Args;
	size_t ArgCount;
	cstr Command;
	cstr WorkDir;
	const xprocessenv* Env;
	size_t EnvCount;
	bool InheritEnv;
	bool NewGroup;
	bool NewConsole;
	bool HideWindow;
	bool Terminal;
	uint32 Columns;
	uint32 Rows;
	xprocessio Stdin;
	xprocessio Stdout;
	xprocessio Stderr;
} xprocessconfig;



/* 进程对象使用引用计数；内部等待引用保证提前释放调用方引用仍可安全回收。 */
typedef struct xprocess xprocess;



XRT_EXTERN_C_BEGIN



/* 初始化直接执行配置，并启用继承环境与独立进程组。 */
XRT_API bool xrtProcessConfigInit(xprocessconfig* pConfig);



/* 初始化系统 Shell 配置；Command 只借用到 Spawn 返回。 */
XRT_API bool xrtProcessShellConfigInit(
	xprocessconfig* pConfig,
	cstr sCommand
);



/* 启动子进程；失败不返回半初始化对象，详情写入当前结构化错误。 */
XRT_API xprocess* xrtProcessSpawn(const xprocessconfig* pConfig);



/* 增加进程对象引用并返回原指针。 */
XRT_API xprocess* xrtProcessRef(xprocess* pProcess);



/*
	释放进程对象引用；最后一个调用方引用可在进程运行时释放。
	此时关闭父端标准流并由内部等待者回收子进程，不隐式杀死子进程。
*/
XRT_API void xrtProcessDestroy(xprocess* pProcess);



/* 返回进程状态快照。 */
XRT_API xprocessstate xrtProcessState(const xprocess* pProcess);



/* 返回平台进程标识，失败返回零。 */
XRT_API uint64 xrtProcessId(const xprocess* pProcess);



/* 返回借用的原生进程句柄；POSIX 返回 pid，Windows 返回 HANDLE。 */
XRT_API intptr_t xrtProcessNative(const xprocess* pProcess);



/* 返回借用的父端标准流句柄；未配置 PIPE 或已关闭时返回 -1。 */
XRT_API intptr_t xrtProcessStreamNative(
	const xprocess* pProcess,
	xprocessstream Stream
);



/* 复制退出状态；进程尚未退出时返回 false 并设置状态错误。 */
XRT_API bool xrtProcessStatus(
	const xprocess* pProcess,
	xprocessstatus* pStatus
);



/* 返回进程后台等待失败的新错误引用，没有后台错误时返回空。 */
XRT_API xerror* xrtProcessError(const xprocess* pProcess);



/*
	从 stdout 或 stderr 管道同步读取；零表示 EOF，负数表示错误。
	同一标准流同一时刻只允许一个读取者。
*/
XRT_API int64 xrtProcessRead(
	xprocess* pProcess,
	xprocessstream Stream,
	void* pData,
	size_t iSize
);



/*
	向 stdin 管道同步写入，返回实际写入字节数，负数表示错误。
	函数可能部分写入；同一时刻只允许一个写入者。
*/
XRT_API int64 xrtProcessWrite(
	xprocess* pProcess,
	const void* pData,
	size_t iSize
);



/* 关闭父进程持有的指定管道端；重复关闭成功。 */
XRT_API bool xrtProcessClose(
	xprocess* pProcess,
	xprocessstream Stream
);



/* 等待进程退出。 */
XRT_API xwaitresult xrtProcessWait(xprocess* pProcess);



/* 在相对微秒数内等待进程退出。 */
XRT_API xwaitresult xrtProcessWaitFor(
	xprocess* pProcess,
	uint64 iTimeout
);



/* 等待进程退出到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtProcessWaitUntil(
	xprocess* pProcess,
	xdeadline iDeadline
);



/* 请求控制台中断或 POSIX SIGINT，不等待进程退出。 */
XRT_API bool xrtProcessInterrupt(xprocess* pProcess);



/* 请求温和终止并关闭 stdin，不等待进程退出。 */
XRT_API bool xrtProcessTerminate(xprocess* pProcess);



/* 强制结束根进程，不等待进程退出。 */
XRT_API bool xrtProcessKill(xprocess* pProcess);



/* 强制结束创建时进程组中的完整进程树，不等待进程退出。 */
XRT_API bool xrtProcessKillTree(xprocess* pProcess);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PROCESS_OPEN)

XRT_EXTERN_C_BEGIN



/*
	请求系统使用默认关联程序打开 UTF-8 文件路径或 URI。
	返回 true 只表示系统接受请求，不表示目标应用已经完成展示。
*/
XRT_API bool xrtProcessOpen(cstr sTarget);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PROCESS_RUN)

/* 默认分别为 stdout 和 stderr 保留最多 16 MiB。 */
#define XPROCESS_CAPTURE_LIMIT_DEFAULT (16u * 1024u * 1024u)



/* 捕获达到上限时可以失败、保留开头或保留结尾。 */
typedef enum xprocessoverflow {
	XPROCESS_OVERFLOW_ERROR = 0,
	XPROCESS_OVERFLOW_KEEP_FIRST,
	XPROCESS_OVERFLOW_KEEP_LAST
} xprocessoverflow;



/* 输出回调借用当前读取块；stdout 与 stderr 回调可能并发执行。 */
typedef bool (*xprocessoutputproc)(
	xprocessstream Stream,
	xbytesview Data,
	ptr pUserData
);



/*
	Run 选项把等待控制、输入、捕获边界和流式观察集中在一个稳定结构中。
	Deadline 为 NEVER 时不超时；Cancel 只借用到 Run 返回。
*/
typedef struct xprocessrunoptions {
	xbytesview Input;
	xdeadline Deadline;
	xcancel* Cancel;
	uint64 StopGrace;
	size_t StdoutLimit;
	size_t StderrLimit;
	xprocessoverflow Overflow;
	xprocessoutputproc Output;
	ptr UserData;
} xprocessrunoptions;



/* Run 结果拥有两个动态输出；基础设施成功不等价于子进程退出码为零。 */
typedef struct xprocessresult {
	xprocessstatus Status;
	xwaitresult Wait;
	size_t InputWritten;
	bytes Stdout;
	size_t StdoutSize;
	bytes Stderr;
	size_t StderrSize;
	bool StdoutTruncated;
	bool StderrTruncated;
	uint64 Duration;
} xprocessresult;



XRT_EXTERN_C_BEGIN



/* 初始化有界捕获、无限等待和 250 ms 分级停止宽限。 */
XRT_API bool xrtProcessRunOptionsInit(xprocessrunoptions* pOptions);



/* 等待进程、Deadline 或取消令牌中的首个事件。 */
XRT_API xwaitresult xrtProcessWaitUntilCancel(
	xprocess* pProcess,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 释放结果持有的输出并恢复为空结果。 */
XRT_API void xrtProcessResultUnit(xprocessresult* pResult);



/* 判断进程是否在未超时、未取消条件下以退出码零正常结束。 */
XRT_API bool xrtProcessResultSuccess(const xprocessresult* pResult);



/*
	启动、并发排空 stdout/stderr、写入输入、等待并收口结果。
	返回 false 只表示基础设施失败；非零退出码仍返回 true。
*/
XRT_API bool xrtProcessRun(
	const xprocessconfig* pConfig,
	const xprocessrunoptions* pOptions,
	xprocessresult* pResult
);



/* 直接执行程序并使用默认有界捕获策略。 */
XRT_API bool xrtProcessCapture(
	cstr sProgram,
	const cstr* pArgs,
	size_t iArgCount,
	xprocessresult* pResult
);



/* 通过系统 Shell 执行命令并使用默认有界捕获策略。 */
XRT_API bool xrtProcessShell(
	cstr sCommand,
	xprocessresult* pResult
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PROCESS_PIPELINE)

/* Pipeline 流式回调携带阶段索引；不同阶段与流可能并发调用。 */
typedef bool (*xprocesspipelineoutputproc)(
	size_t iStage,
	xprocessstream Stream,
	xbytesview Data,
	ptr pUserData
);



/* Pipeline 选项独立表达首段输入、共享等待控制和逐流捕获边界。 */
typedef struct xprocesspipelineoptions {
	xbytesview Input;
	xdeadline Deadline;
	xcancel* Cancel;
	uint64 StopGrace;
	size_t StdoutLimit;
	size_t StderrLimit;
	xprocessoverflow Overflow;
	xprocesspipelineoutputproc Output;
	ptr UserData;
} xprocesspipelineoptions;



/* 每段结果独立拥有 stderr，避免跨阶段拼接后丢失错误归属。 */
typedef struct xprocessstageresult {
	xprocessstatus Status;
	bytes Stderr;
	size_t StderrSize;
	bool StderrTruncated;
} xprocessstageresult;



/* Pipeline 结果拥有全部阶段结果与末段 stdout。 */
typedef struct xprocesspipelineresult {
	xprocessstageresult* Stages;
	size_t StageCount;
	size_t InputWritten;
	bytes Stdout;
	size_t StdoutSize;
	bool StdoutTruncated;
	xwaitresult Wait;
	uint64 Duration;
} xprocesspipelineresult;



XRT_EXTERN_C_BEGIN



/* 初始化 Pipeline 的有界捕获、无限等待和 250 ms 停止宽限。 */
XRT_API bool xrtProcessPipelineOptionsInit(
	xprocesspipelineoptions* pOptions
);



/* 释放 Pipeline 结果持有的状态数组和输出。 */
XRT_API void xrtProcessPipelineResultUnit(xprocesspipelineresult* pResult);



/* 判断全部阶段是否都以退出码零正常结束。 */
XRT_API bool xrtProcessPipelineSuccess(
	const xprocesspipelineresult* pResult
);



/* 并发启动真实 OS 管道连接的全部阶段并按一个 Deadline 收口。 */
XRT_API bool xrtProcessPipeline(
	const xprocessconfig* pStages,
	size_t iStageCount,
	const xprocesspipelineoptions* pOptions,
	xprocesspipelineresult* pResult
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PROCESS_FUTURE)

XRT_EXTERN_C_BEGIN



/* Future 成功值是由 Future 自身拥有的只读 xprocessstatus 快照。 */
XRT_API xfuture* xrtProcessWaitAsync(xprocess* pProcess);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PROCESS_TERMINAL)

XRT_EXTERN_C_BEGIN



/* 判断当前系统是否具备 ConPTY 或 POSIX PTY 支持。 */
XRT_API bool xrtProcessTerminalSupported(void);



/* 调整 Terminal 进程窗口并通知子进程。 */
XRT_API bool xrtProcessResize(
	xprocess* pProcess,
	uint32 iColumns,
	uint32 iRows
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_PROCESS_FILE)

XRT_EXTERN_C_BEGIN



/* 从借用的 XRT 文件构造 HANDLE 标准流配置。 */
XRT_API xprocessio xrtProcessFile(xfile File);



XRT_EXTERN_C_END

#endif

#endif
