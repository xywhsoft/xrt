#include "../internal/xrt_process.h"



#if defined(XRT_FEATURE_PROCESS)

/* 生成一个带平台错误码的 xrt.process 错误。 */
void __xrtProcessErrorSet(
	xerrkind Kind,
	xprocesserror Code,
	cstr sOperation,
	cstr sMessage,
	int iSystemCode
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.SystemCode = (int32)iSystemCode;
	Desc.Domain = "xrt.process";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 校验环境变量名称只包含平台都能无歧义表达的非空名称。 */
static bool __xrtProcessEnvNameValid(cstr sName)
{
	if ( (sName == NULL) || (sName[0] == 0) || (sName[0] == '=') ) {
		return false;
	}
	return strchr(sName, '=') == NULL;
}



/* 校验标准流模式及 HANDLE 值。 */
static bool __xrtProcessIoValid(
	xprocessstream Stream,
	xprocessio Io
)
{
	if ( (Io.Mode < XPROCESS_IO_INHERIT) ||
		(Io.Mode > XPROCESS_IO_MERGE) ) {
		return false;
	}
	if ( (Io.Mode == XPROCESS_IO_MERGE) &&
		(Stream != XPROCESS_STDERR) ) {
		return false;
	}
	if ( (Io.Mode == XPROCESS_IO_HANDLE) &&
		(Io.Handle == (intptr_t)-1) ) {
		return false;
	}
	return true;
}



/* 在调用任何平台 API 前完整校验借用配置。 */
static bool __xrtProcessConfigValid(const xprocessconfig* pConfig)
{
	size_t i;

	if ( pConfig == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"spawn",
			"process config is null",
			0
		);
		return false;
	}
	if ( (pConfig->Target != XPROCESS_EXEC) &&
		(pConfig->Target != XPROCESS_SHELL) ) {
		__xrtProcessErrorSet(
			XERR_VALUE,
			XPROCESS_ERROR_CONFIG,
			"spawn.target",
			"process target is invalid",
			0
		);
		return false;
	}
	if ( (pConfig->Target == XPROCESS_EXEC) &&
		((pConfig->Program == NULL) || (pConfig->Program[0] == 0)) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_COMMAND,
			"spawn.program",
			"direct process requires a program",
			0
		);
		return false;
	}
	if ( (pConfig->Target == XPROCESS_SHELL) &&
		((pConfig->Command == NULL) || (pConfig->Command[0] == 0)) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_COMMAND,
			"spawn.command",
			"shell process requires a command",
			0
		);
		return false;
	}
	if ( (pConfig->ArgCount != 0u) && (pConfig->Args == NULL) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_COMMAND,
			"spawn.args",
			"process argument array is null",
			0
		);
		return false;
	}
	for ( i = 0u; i < pConfig->ArgCount; i++ ) {
		if ( pConfig->Args[i] == NULL ) {
			__xrtProcessErrorSet(
				XERR_ARGUMENT,
				XPROCESS_ERROR_COMMAND,
				"spawn.args",
				"process argument is null",
				0
			);
			return false;
		}
	}
	if ( (pConfig->EnvCount != 0u) && (pConfig->Env == NULL) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ENVIRONMENT,
			"spawn.environment",
			"process environment array is null",
			0
		);
		return false;
	}
	for ( i = 0u; i < pConfig->EnvCount; i++ ) {
		if ( !__xrtProcessEnvNameValid(pConfig->Env[i].Name) ) {
			__xrtProcessErrorSet(
				XERR_VALUE,
				XPROCESS_ERROR_ENVIRONMENT,
				"spawn.environment",
				"process environment name is invalid",
				0
			);
			return false;
		}
	}
	if ( !__xrtProcessIoValid(XPROCESS_STDIN, pConfig->Stdin) ||
		!__xrtProcessIoValid(XPROCESS_STDOUT, pConfig->Stdout) ||
		!__xrtProcessIoValid(XPROCESS_STDERR, pConfig->Stderr) ) {
		__xrtProcessErrorSet(
			XERR_VALUE,
			XPROCESS_ERROR_CONFIG,
			"spawn.stdio",
			"process standard stream config is invalid",
			0
		);
		return false;
	}
	if ( pConfig->NewConsole && pConfig->HideWindow ) {
		__xrtProcessErrorSet(
			XERR_VALUE,
			XPROCESS_ERROR_CONFIG,
			"spawn.console",
			"new console and hidden window are mutually exclusive",
			0
		);
		return false;
	}
	if ( pConfig->Terminal &&
		(pConfig->NewConsole || pConfig->HideWindow) ) {
		__xrtProcessErrorSet(
			XERR_VALUE,
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal",
			"terminal cannot use new-console or hidden-window mode",
			0
		);
		return false;
	}
	if ( pConfig->Terminal &&
		((pConfig->Columns == 0u) || (pConfig->Columns > 32767u) ||
		 (pConfig->Rows == 0u) || (pConfig->Rows > 32767u)) ) {
		__xrtProcessErrorSet(
			XERR_RANGE,
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal",
			"terminal dimensions must be between 1 and 32767",
			0
		);
		return false;
	}
	#if !defined(XRT_FEATURE_PROCESS_TERMINAL)
		if ( pConfig->Terminal ) {
			__xrtProcessErrorSet(
				XERR_UNSUPPORTED,
				XPROCESS_ERROR_TERMINAL,
				"spawn.terminal",
				"process terminal support is not selected",
				0
			);
			return false;
		}
	#endif
	return true;
}



/* 释放已经没有调用方和等待线程引用的 Process 对象。 */
static void __xrtProcessFree(xprocess* pProcess)
{
	xthread* pWaiter = pProcess->Waiter;
	xerror* pError = pProcess->Error;
	#if defined(XRT_FEATURE_PROCESS_FUTURE)
		xfuture* pFuture = pProcess->WaitFuture;
		xpromise* pPromise = pProcess->WaitPromise;
		xprocessstatus* pStatus = pProcess->WaitStatus;
	#endif

	__xrtProcessPlatformUnit(pProcess);
	(void)xrtCondUnit(&pProcess->Changed);
	(void)xrtMutexUnit(&pProcess->Lock);
	xrtFree(pProcess);
	xrtThreadDestroy(pWaiter);
	xrtErrorFree(pError);
	#if defined(XRT_FEATURE_PROCESS_FUTURE)
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pFuture);
		xrtFree(pStatus);
	#endif
}



/* 释放一个内部或外部共享引用。 */
static void __xrtProcessRelease(xprocess* pProcess)
{
	if ( (pProcess != NULL) &&
		(xrtRefRelease(&pProcess->RefCount) == 0) ) {
		__xrtProcessFree(pProcess);
	}
}



/* 后台等待者只负责一次平台等待和不可变终态发布。 */
static int32 __xrtProcessWaiter(ptr pData)
{
	xprocess* pProcess = (xprocess*)pData;
	xprocessstatus Status;
	xerror* pError = NULL;
	bool bOk;

	memset(&Status, 0, sizeof(Status));
	bOk = __xrtProcessPlatformWait(pProcess, &Status);
	if ( !bOk ) {
		pError = xrtTakeError();
		Status.Kind = XPROCESS_EXIT_LOST;
		Status.Code = -1;
	}
	(void)__xrtProcessPlatformClose(pProcess, XPROCESS_STDIN);

	(void)xrtMutexLock(&pProcess->Lock);
	Status.Stop = pProcess->RequestedStop;
	pProcess->Status = Status;
	pProcess->State = XPROCESS_EXITED;
	pProcess->Error = pError;
	(void)xrtCondBroadcast(&pProcess->Changed);
	(void)xrtMutexUnlock(&pProcess->Lock);
	#if defined(XRT_FEATURE_PROCESS_FUTURE)
		__xrtProcessFutureComplete(pProcess);
	#endif

	__xrtProcessRelease(pProcess);
	return bOk ? 0 : -1;
}



/* 初始化直接执行配置，并采用资源收容友好的默认进程组。 */
XRT_API bool xrtProcessConfigInit(xprocessconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"config.init",
			"process config output is null",
			0
		);
		return false;
	}
	memset(pConfig, 0, sizeof(xprocessconfig));
	pConfig->Target = XPROCESS_EXEC;
	pConfig->InheritEnv = true;
	pConfig->NewGroup = true;
	pConfig->Columns = 120u;
	pConfig->Rows = 30u;
	pConfig->Stdin.Mode = XPROCESS_IO_INHERIT;
	pConfig->Stdout.Mode = XPROCESS_IO_INHERIT;
	pConfig->Stderr.Mode = XPROCESS_IO_INHERIT;
	return true;
}



/* 初始化系统 Shell 配置。 */
XRT_API bool xrtProcessShellConfigInit(
	xprocessconfig* pConfig,
	cstr sCommand
)
{
	if ( !xrtProcessConfigInit(pConfig) ) {
		return false;
	}
	pConfig->Target = XPROCESS_SHELL;
	pConfig->Command = sCommand;
	return true;
}



/* 分配对象、启动平台进程并建立唯一后台等待者。 */
XRT_API xprocess* xrtProcessSpawn(const xprocessconfig* pConfig)
{
	xprocess* pProcess;
	xprocessstatus Status;

	if ( !__xrtProcessConfigValid(pConfig) ) {
		return NULL;
	}
	pProcess = (xprocess*)xrtCalloc(1u, sizeof(xprocess));
	if ( pProcess == NULL ) {
		return NULL;
	}
	pProcess->RefCount = 1;
	pProcess->UserRefs = 1;
	pProcess->State = XPROCESS_RUNNING;
	pProcess->Status.Kind = XPROCESS_EXIT_NONE;
	pProcess->Status.Code = -1;
	pProcess->NewGroup = pConfig->NewGroup;
	pProcess->Terminal = pConfig->Terminal;
	#if defined(_WIN32) || defined(_WIN64)
		#if defined(XRT_FEATURE_PROCESS_TERMINAL)
			pProcess->TerminalHandle = NULL;
		#endif
		pProcess->Stdin = INVALID_HANDLE_VALUE;
		pProcess->Stdout = INVALID_HANDLE_VALUE;
		pProcess->Stderr = INVALID_HANDLE_VALUE;
	#else
		pProcess->Id = -1;
		pProcess->Stdin = -1;
		pProcess->Stdout = -1;
		pProcess->Stderr = -1;
	#endif
	if ( !xrtMutexInit(&pProcess->Lock) ) {
		xrtFree(pProcess);
		return NULL;
	}
	if ( !xrtCondInit(&pProcess->Changed) ) {
		(void)xrtMutexUnit(&pProcess->Lock);
		xrtFree(pProcess);
		return NULL;
	}
	if ( !__xrtProcessPlatformSpawn(pProcess, pConfig) ) {
		__xrtProcessPlatformUnit(pProcess);
		(void)xrtCondUnit(&pProcess->Changed);
		(void)xrtMutexUnit(&pProcess->Lock);
		xrtFree(pProcess);
		return NULL;
	}

	if ( xrtRefRetain(&pProcess->RefCount) < 0 ) {
		__xrtProcessErrorSet(
			XERR_INTERNAL,
			XPROCESS_ERROR_THREAD,
			"spawn.waiter",
			"process internal reference failed",
			0
		);
		goto fail_running;
	}
	pProcess->Waiter = xrtThreadCreate(__xrtProcessWaiter, pProcess, 0u);
	if ( pProcess->Waiter == NULL ) {
		(void)xrtRefRelease(&pProcess->RefCount);
		__xrtProcessErrorSet(
			XERR_INTERNAL,
			XPROCESS_ERROR_THREAD,
			"spawn.waiter",
			"process wait thread could not start",
			0
		);
		goto fail_running;
	}
	return pProcess;

fail_running:
	(void)__xrtProcessPlatformKillTree(pProcess);
	(void)__xrtProcessPlatformKill(pProcess);
	memset(&Status, 0, sizeof(Status));
	(void)__xrtProcessPlatformWait(pProcess, &Status);
	__xrtProcessPlatformUnit(pProcess);
	(void)xrtCondUnit(&pProcess->Changed);
	(void)xrtMutexUnit(&pProcess->Lock);
	xrtFree(pProcess);
	return NULL;
}



/* 同时增加外部引用计数和对象总引用计数。 */
XRT_API xprocess* xrtProcessRef(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return NULL;
	}
	if ( xrtRefRetain(&pProcess->RefCount) < 0 ) {
		return NULL;
	}
	if ( xrtRefRetain(&pProcess->UserRefs) < 0 ) {
		(void)xrtRefRelease(&pProcess->RefCount);
		return NULL;
	}
	return pProcess;
}



/* 最后一个调用方引用关闭父端管道，后台等待引用继续负责回收。 */
XRT_API void xrtProcessDestroy(xprocess* pProcess)
{
	int32 iUsers;

	if ( pProcess == NULL ) {
		return;
	}
	iUsers = xrtRefRelease(&pProcess->UserRefs);
	if ( iUsers < 0 ) {
		return;
	}
	if ( iUsers == 0 ) {
		(void)__xrtProcessPlatformClose(pProcess, XPROCESS_STDIN);
		(void)__xrtProcessPlatformClose(pProcess, XPROCESS_STDOUT);
		(void)__xrtProcessPlatformClose(pProcess, XPROCESS_STDERR);
	}
	__xrtProcessRelease(pProcess);
}



/* 返回锁保护的状态快照。 */
XRT_API xprocessstate xrtProcessState(const xprocess* pProcess)
{
	xprocessstate State;

	if ( pProcess == NULL ) {
		return XPROCESS_EXITED;
	}
	(void)xrtMutexLock((xmutex*)&pProcess->Lock);
	State = pProcess->State;
	(void)xrtMutexUnlock((xmutex*)&pProcess->Lock);
	return State;
}



/* 返回平台进程标识。 */
XRT_API uint64 xrtProcessId(const xprocess* pProcess)
{
	return pProcess != NULL ? __xrtProcessPlatformId(pProcess) : 0u;
}



/* 返回借用的原生进程句柄。 */
XRT_API intptr_t xrtProcessNative(const xprocess* pProcess)
{
	return pProcess != NULL ? __xrtProcessPlatformNative(pProcess) : -1;
}



/* 返回借用的原生父端标准流句柄。 */
XRT_API intptr_t xrtProcessStreamNative(
	const xprocess* pProcess,
	xprocessstream Stream
)
{
	if ( pProcess == NULL ) {
		return -1;
	}
	return __xrtProcessPlatformStream(pProcess, Stream);
}



/* 只在终态发布后复制不可变退出状态。 */
XRT_API bool xrtProcessStatus(
	const xprocess* pProcess,
	xprocessstatus* pStatus
)
{
	if ( (pProcess == NULL) || (pStatus == NULL) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"status",
			"process status argument is null",
			0
		);
		return false;
	}
	(void)xrtMutexLock((xmutex*)&pProcess->Lock);
	if ( pProcess->State != XPROCESS_EXITED ) {
		(void)xrtMutexUnlock((xmutex*)&pProcess->Lock);
		__xrtProcessErrorSet(
			XERR_STATE,
			XPROCESS_ERROR_WAIT,
			"status",
			"process is still running",
			0
		);
		return false;
	}
	*pStatus = pProcess->Status;
	(void)xrtMutexUnlock((xmutex*)&pProcess->Lock);
	return true;
}



/* 增加后台错误引用后在锁外返回。 */
XRT_API xerror* xrtProcessError(const xprocess* pProcess)
{
	xerror* pError;

	if ( pProcess == NULL ) {
		return NULL;
	}
	(void)xrtMutexLock((xmutex*)&pProcess->Lock);
	pError = xrtErrorRef(pProcess->Error);
	(void)xrtMutexUnlock((xmutex*)&pProcess->Lock);
	return pError;
}



/* 把标准流读取交给平台实现。 */
XRT_API int64 xrtProcessRead(
	xprocess* pProcess,
	xprocessstream Stream,
	void* pData,
	size_t iSize
)
{
	if ( (pProcess == NULL) || (pData == NULL) ||
		((Stream != XPROCESS_STDOUT) &&
		 (Stream != XPROCESS_STDERR)) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"read",
			"process read argument is invalid",
			0
		);
		return -1;
	}
	if ( iSize == 0u ) {
		return 0;
	}
	return __xrtProcessPlatformRead(pProcess, Stream, pData, iSize);
}



/* 把 stdin 写入交给平台实现。 */
XRT_API int64 xrtProcessWrite(
	xprocess* pProcess,
	const void* pData,
	size_t iSize
)
{
	if ( (pProcess == NULL) || (pData == NULL) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"write",
			"process write argument is invalid",
			0
		);
		return -1;
	}
	if ( iSize == 0u ) {
		return 0;
	}
	return __xrtProcessPlatformWrite(pProcess, pData, iSize);
}



/* 校验流标识后幂等关闭父端句柄。 */
XRT_API bool xrtProcessClose(
	xprocess* pProcess,
	xprocessstream Stream
)
{
	if ( (pProcess == NULL) || (Stream < XPROCESS_STDIN) ||
		(Stream > XPROCESS_STDERR) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"close",
			"process close argument is invalid",
			0
		);
		return false;
	}
	return __xrtProcessPlatformClose(pProcess, Stream);
}



/* 等待终态并把后台等待错误复制到当前调用上下文。 */
XRT_API xwaitresult xrtProcessWaitUntil(
	xprocess* pProcess,
	xdeadline iDeadline
)
{
	xwaitresult Result = XWAIT_OK;
	xerror* pError = NULL;

	if ( pProcess == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"wait",
			"process is null",
			0
		);
		return XWAIT_ERROR;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	while ( pProcess->State != XPROCESS_EXITED ) {
		Result = xrtCondWaitUntil(
			&pProcess->Changed,
			&pProcess->Lock,
			iDeadline
		);
		if ( Result != XWAIT_OK ) {
			(void)xrtMutexUnlock(&pProcess->Lock);
			return Result;
		}
	}
	if ( pProcess->Status.Kind == XPROCESS_EXIT_LOST ) {
		pError = xrtErrorRef(pProcess->Error);
		Result = XWAIT_ERROR;
	}
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return Result;
}



/* 使用无限 Deadline 等待。 */
XRT_API xwaitresult xrtProcessWait(xprocess* pProcess)
{
	return xrtProcessWaitUntil(pProcess, XRT_DEADLINE_NEVER);
}



/* 从相对微秒数构造单调 Deadline。 */
XRT_API xwaitresult xrtProcessWaitFor(
	xprocess* pProcess,
	uint64 iTimeout
)
{
	return xrtProcessWaitUntil(pProcess, xrtDeadlineAfter(iTimeout));
}



/* 在成功发出停止请求后记录最终可观测停止原因。 */
static bool __xrtProcessStop(
	xprocess* pProcess,
	xprocessstop Stop,
	bool (*pProc)(xprocess*)
)
{
	bool bOk;

	if ( pProcess == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"stop",
			"process is null",
			0
		);
		return false;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	if ( pProcess->State == XPROCESS_EXITED ) {
		(void)xrtMutexUnlock(&pProcess->Lock);
		return true;
	}
	(void)xrtMutexUnlock(&pProcess->Lock);
	bOk = pProc(pProcess);
	if ( !bOk ) {
		return false;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	if ( pProcess->RequestedStop < Stop ) {
		pProcess->RequestedStop = Stop;
	}
	(void)xrtMutexUnlock(&pProcess->Lock);
	return true;
}



/* 请求中断。 */
XRT_API bool xrtProcessInterrupt(xprocess* pProcess)
{
	return __xrtProcessStop(
		pProcess,
		XPROCESS_STOP_INTERRUPT,
		__xrtProcessPlatformInterrupt
	);
}



/* 关闭 stdin 后请求温和终止。 */
XRT_API bool xrtProcessTerminate(xprocess* pProcess)
{
	if ( pProcess != NULL ) {
		(void)__xrtProcessPlatformClose(pProcess, XPROCESS_STDIN);
	}
	return __xrtProcessStop(
		pProcess,
		XPROCESS_STOP_TERMINATE,
		__xrtProcessPlatformTerminate
	);
}



/* 强制结束根进程。 */
XRT_API bool xrtProcessKill(xprocess* pProcess)
{
	return __xrtProcessStop(
		pProcess,
		XPROCESS_STOP_KILL,
		__xrtProcessPlatformKill
	);
}



/* 强制结束进程组。 */
XRT_API bool xrtProcessKillTree(xprocess* pProcess)
{
	return __xrtProcessStop(
		pProcess,
		XPROCESS_STOP_KILL_TREE,
		__xrtProcessPlatformKillTree
	);
}

#endif
