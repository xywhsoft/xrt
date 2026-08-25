#include "../internal/xrt_process_run.h"



#if defined(XRT_FEATURE_PROCESS_RUN)

/* 每个输出泵独占自己的动态缓冲，不需要共享数据锁。 */
typedef struct xprocessrunpump {
	struct xprocessrunstate* State;
	xprocessstream Stream;
	xbuffer Buffer;
	xthread* Thread;
	size_t Limit;
	bool Truncated;
} xprocessrunpump;



/* 输入泵只借用 Run 调用期间稳定的输入视图。 */
typedef struct xprocessruninput {
	struct xprocessrunstate* State;
	xbytesview Input;
	xthread* Thread;
	size_t Written;
} xprocessruninput;



/* Run 状态只同步失败发布；三个泵的具体数据保持单写者。 */
typedef struct xprocessrunstate {
	xmutex Lock;
	xprocess* Process;
	xcancel* Control;
	xprocessrunoptions Options;
	xerror* Error;
	bool Failed;
	bool Terminal;
	xprocessrunpump Stdout;
	xprocessrunpump Stderr;
	xprocessruninput Stdin;
} xprocessrunstate;



/* 取消监听只唤醒 Process 条件变量，不在请求线程执行信号操作。 */
static void __xrtProcessWaitCancel(ptr pData)
{
	xprocess* pProcess = (xprocess*)pData;

	(void)xrtMutexLock(&pProcess->Lock);
	(void)xrtCondBroadcast(&pProcess->Changed);
	(void)xrtMutexUnlock(&pProcess->Lock);
}



/* 等待终态、Deadline 或取消，完成终态优先于同时发生的取消。 */
XRT_API xwaitresult xrtProcessWaitUntilCancel(
	xprocess* pProcess,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xcancelwatch* pWatch;
	xwaitresult Result = XWAIT_OK;
	xerror* pError = NULL;

	if ( pCancel == NULL ) {
		return xrtProcessWaitUntil(pProcess, iDeadline);
	}
	if ( pProcess == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"wait.cancel",
			"process is null",
			0
		);
		return XWAIT_ERROR;
	}
	pWatch = xrtCancelWatch(pCancel, __xrtProcessWaitCancel, pProcess);
	if ( pWatch == NULL ) {
		return XWAIT_ERROR;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	while ( pProcess->State != XPROCESS_EXITED ) {
		if ( xrtCancelRequested(pCancel) ) {
			Result = XWAIT_CANCELLED;
			break;
		}
		Result = xrtCondWaitUntil(
			&pProcess->Changed,
			&pProcess->Lock,
			iDeadline
		);
		if ( Result != XWAIT_OK ) {
			break;
		}
	}
	if ( (Result == XWAIT_OK) &&
		(pProcess->Status.Kind == XPROCESS_EXIT_LOST) ) {
		pError = xrtErrorRef(pProcess->Error);
		Result = XWAIT_ERROR;
	}
	(void)xrtMutexUnlock(&pProcess->Lock);
	xrtCancelUnwatch(pWatch);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return Result;
}



/* 初始化空 Result，并把退出码设为尚不可用。 */
static void __xrtProcessResultInit(xprocessresult* pResult)
{
	memset(pResult, 0, sizeof(xprocessresult));
	pResult->Status.Kind = XPROCESS_EXIT_NONE;
	pResult->Status.Code = -1;
	pResult->Wait = XWAIT_ERROR;
}



/* 只发布 Run 的第一个基础设施错误，并请求内部取消。 */
static void __xrtProcessRunFail(
	xprocessrunstate* pState,
	xerror* pError
)
{
	bool bRequest = false;

	if ( pError == NULL ) {
		__xrtProcessErrorSet(
			XERR_INTERNAL,
			XPROCESS_ERROR_WAIT,
			"run",
			"process run failed without an error",
			0
		);
		pError = xrtTakeError();
	}
	(void)xrtMutexLock(&pState->Lock);
	if ( !pState->Failed ) {
		pState->Failed = true;
		pState->Error = pError;
		pError = NULL;
		bRequest = true;
	}
	(void)xrtMutexUnlock(&pState->Lock);
	xrtErrorFree(pError);
	if ( bRequest ) {
		(void)xrtCancelRequest(pState->Control);
	}
}



/* 返回 Run 是否已由任意工作线程发布失败。 */
static bool __xrtProcessRunFailed(xprocessrunstate* pState)
{
	bool bFailed;

	(void)xrtMutexLock(&pState->Lock);
	bFailed = pState->Failed;
	(void)xrtMutexUnlock(&pState->Lock);
	return bFailed;
}



/* 按策略把一块输出追加到有界动态窗口。 */
bool __xrtProcessCaptureAppend(
	xbuffer* pBuffer,
	size_t iLimit,
	xprocessoverflow Overflow,
	bool* pTruncated,
	xbytesview Data
)
{
	size_t iAvailable;
	size_t iKeep;
	size_t iRemove;

	iAvailable = (pBuffer->Size < iLimit) ?
		(iLimit - pBuffer->Size) : 0u;
	if ( Data.Size <= iAvailable ) {
		return xrtBufferAppend(pBuffer, Data);
	}
	*pTruncated = true;
	if ( Overflow == XPROCESS_OVERFLOW_ERROR ) {
		__xrtProcessErrorSet(
			XERR_RANGE,
			XPROCESS_ERROR_LIMIT,
			"run.capture",
			"process output exceeded its capture limit",
			0
		);
		return false;
	}
	if ( Overflow == XPROCESS_OVERFLOW_KEEP_FIRST ) {
		iKeep = iAvailable;
		return (iKeep == 0u) || xrtBufferAppend(
			pBuffer,
			(xbytesview){ Data.Data, iKeep }
		);
	}
	if ( iLimit == 0u ) {
		return true;
	}
	if ( Data.Size >= iLimit ) {
		return xrtBufferAssign(
			pBuffer,
			(xbytesview){
				Data.Data + (Data.Size - iLimit),
				iLimit
			}
		);
	}
	iRemove = (pBuffer->Size + Data.Size) - iLimit;
	if ( (iRemove != 0u) &&
		!xrtBufferRemove(pBuffer, 0u, iRemove) ) {
		return false;
	}
	return xrtBufferAppend(pBuffer, Data);
}



/* 持续排空一个输出流，并独立执行回调与有界捕获。 */
static int32 __xrtProcessRunOutput(ptr pData)
{
	xprocessrunpump* pPump = (xprocessrunpump*)pData;
	unsigned char pDataBuffer[16384];

	while ( true ) {
		int64 iRead = xrtProcessRead(
			pPump->State->Process,
			pPump->Stream,
			pDataBuffer,
			sizeof(pDataBuffer)
		);
		xbytesview Data;

		if ( iRead == 0 ) {
			break;
		}
		if ( iRead < 0 ) {
			__xrtProcessRunFail(pPump->State, xrtTakeError());
			break;
		}
		Data.Data = pDataBuffer;
		Data.Size = (size_t)iRead;
		if ( (pPump->State->Options.Output != NULL) &&
			!pPump->State->Options.Output(
				pPump->Stream,
				Data,
				pPump->State->Options.UserData
			) ) {
			__xrtProcessErrorSet(
				XERR_IO,
				XPROCESS_ERROR_CALLBACK,
				"run.output",
				"process output callback rejected a chunk",
				0
			);
			__xrtProcessRunFail(pPump->State, xrtTakeError());
			break;
		}
		if ( !__xrtProcessCaptureAppend(
			&pPump->Buffer,
			pPump->Limit,
			pPump->State->Options.Overflow,
			&pPump->Truncated,
			Data
		) ) {
			__xrtProcessRunFail(pPump->State, xrtTakeError());
			break;
		}
	}
	(void)xrtProcessClose(pPump->State->Process, pPump->Stream);
	return __xrtProcessRunFailed(pPump->State) ? -1 : 0;
}



/* 写完借用输入后无条件关闭 stdin，短写和提前关闭由结果字节数表达。 */
static int32 __xrtProcessRunInput(ptr pData)
{
	xprocessruninput* pInput = (xprocessruninput*)pData;

	while ( pInput->Written < pInput->Input.Size ) {
		int64 iWrite = xrtProcessWrite(
			pInput->State->Process,
			pInput->Input.Data + pInput->Written,
			pInput->Input.Size - pInput->Written
		);

		if ( iWrite <= 0 ) {
			xrtClearError();
			break;
		}
		pInput->Written += (size_t)iWrite;
	}
	/* 普通管道关闭 stdin 通知 EOF；终端模式下关闭 ConPTY 输入句柄会
	   直接拆除伪控制台会话，子进程收到控制事件退出并丢失未冲刷输出，
	   因此终端模式的 stdin 交给进程销毁路径统一收口。 */
	if ( !pInput->State->Terminal ) {
		(void)xrtProcessClose(pInput->State->Process, XPROCESS_STDIN);
	}
	return 0;
}



/* 丢弃一次预期失败的停止错误。 */
static void __xrtProcessRunDiscardError(void)
{
	xerror* pError = xrtTakeError();

	xrtErrorFree(pError);
}



/* 在控制流超时、取消或内部失败后执行固定分级收口。 */
bool __xrtProcessRunStop(
	xprocess* pProcess,
	uint64 iGrace
)
{
	if ( xrtProcessState(pProcess) == XPROCESS_EXITED ) {
		return true;
	}
	if ( !xrtProcessInterrupt(pProcess) ) {
		__xrtProcessRunDiscardError();
	}
	if ( xrtProcessWaitFor(pProcess, iGrace) == XWAIT_OK ) {
		return true;
	}
	if ( !xrtProcessTerminate(pProcess) ) {
		__xrtProcessRunDiscardError();
	}
	if ( xrtProcessWaitFor(pProcess, iGrace) == XWAIT_OK ) {
		return true;
	}
	if ( !xrtProcessKillTree(pProcess) ) {
		__xrtProcessRunDiscardError();
		if ( !xrtProcessKill(pProcess) ) {
			return false;
		}
	}
	return xrtProcessWait(pProcess) == XWAIT_OK;
}



/* 依次等待并释放 Run 创建的全部工作线程。 */
static bool __xrtProcessRunThreadsJoin(xprocessrunstate* pState)
{
	bool bOk = true;
	xthread* pThreads[] = {
		pState->Stdin.Thread,
		pState->Stdout.Thread,
		pState->Stderr.Thread
	};

	for ( size_t i = 0u; i < 3u; i++ ) {
		if ( pThreads[i] == NULL ) {
			continue;
		}
		if ( xrtThreadWait(pThreads[i]) != XWAIT_OK ) {
			bOk = false;
		}
		xrtThreadDestroy(pThreads[i]);
	}
	pState->Stdin.Thread = NULL;
	pState->Stdout.Thread = NULL;
	pState->Stderr.Thread = NULL;
	return bOk;
}



/* 初始化默认 Run 选项。 */
XRT_API bool xrtProcessRunOptionsInit(xprocessrunoptions* pOptions)
{
	if ( pOptions == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"run.config",
			"process run options output is null",
			0
		);
		return false;
	}
	memset(pOptions, 0, sizeof(xprocessrunoptions));
	pOptions->Deadline = XRT_DEADLINE_NEVER;
	pOptions->StopGrace = UINT64_C(250000);
	pOptions->StdoutLimit = XPROCESS_CAPTURE_LIMIT_DEFAULT;
	pOptions->StderrLimit = XPROCESS_CAPTURE_LIMIT_DEFAULT;
	pOptions->Overflow = XPROCESS_OVERFLOW_ERROR;
	return true;
}



/* 释放 Result 持有的两个输出。 */
XRT_API void xrtProcessResultUnit(xprocessresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtFree(pResult->Stdout);
	xrtFree(pResult->Stderr);
	__xrtProcessResultInit(pResult);
}



/* 成功结果必须是正常零退出且未经过超时或取消控制流。 */
XRT_API bool xrtProcessResultSuccess(const xprocessresult* pResult)
{
	return (pResult != NULL) && (pResult->Wait == XWAIT_OK) &&
		(pResult->Status.Kind == XPROCESS_EXIT_CODE) &&
		(pResult->Status.Code == 0);
}



/* 运行进程并收口有界输出。 */
XRT_API bool xrtProcessRun(
	const xprocessconfig* pConfig,
	const xprocessrunoptions* pOptions,
	xprocessresult* pResult
)
{
	xprocessrunoptions Options;
	xprocessrunstate State;
	xprocessconfig Config;
	xprocess* pProcess = NULL;
	xwaitresult Wait;
	uint64 iStart;
	bool bNeedInput;
	bool bOk = false;
	bool bLockReady = false;
	bool bThreadsReady = false;

	if ( pResult != NULL ) {
		__xrtProcessResultInit(pResult);
	}
	if ( (pConfig == NULL) || (pResult == NULL) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"run",
			"process run argument is null",
			0
		);
		return false;
	}
	if ( pOptions == NULL ) {
		(void)xrtProcessRunOptionsInit(&Options);
	} else {
		Options = *pOptions;
	}
	if ( ((Options.Input.Data == NULL) && (Options.Input.Size != 0u)) ||
		(Options.Overflow < XPROCESS_OVERFLOW_ERROR) ||
		(Options.Overflow > XPROCESS_OVERFLOW_KEEP_LAST) ) {
		__xrtProcessErrorSet(
			XERR_VALUE,
			XPROCESS_ERROR_CONFIG,
			"run.config",
			"process run options are invalid",
			0
		);
		return false;
	}
	if ( (Options.Cancel != NULL) && xrtCancelRequested(Options.Cancel) ) {
		pResult->Wait = XWAIT_CANCELLED;
		return true;
	}
	memset(&State, 0, sizeof(State));
	State.Options = Options;
	State.Stdout.State = &State;
	State.Stdout.Stream = XPROCESS_STDOUT;
	State.Stdout.Limit = Options.StdoutLimit;
	State.Stderr.State = &State;
	State.Stderr.Stream = XPROCESS_STDERR;
	State.Stderr.Limit = Options.StderrLimit;
	State.Stdin.State = &State;
	State.Stdin.Input = Options.Input;
	if ( !xrtMutexInit(&State.Lock) ) {
		goto cleanup;
	}
	bLockReady = true;
	(void)xrtBufferInit(&State.Stdout.Buffer);
	(void)xrtBufferInit(&State.Stderr.Buffer);
	State.Control = xrtCancelChild(Options.Cancel);
	if ( State.Control == NULL ) {
		goto cleanup;
	}
	Config = *pConfig;
	Config.Stdout.Mode = XPROCESS_IO_PIPE;
	if ( Config.Terminal ) {
		Config.Stderr.Mode = XPROCESS_IO_MERGE;
	} else if ( Config.Stderr.Mode != XPROCESS_IO_MERGE ) {
		Config.Stderr.Mode = XPROCESS_IO_PIPE;
	}
	bNeedInput = (Options.Input.Data != NULL) ||
		(Options.Input.Size != 0u) ||
		(Config.Stdin.Mode == XPROCESS_IO_PIPE);
	if ( bNeedInput ) {
		Config.Stdin.Mode = XPROCESS_IO_PIPE;
	}
	iStart = xrtClock();
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		goto cleanup;
	}
	State.Process = pProcess;
	State.Terminal = Config.Terminal;
	State.Stdout.Thread = xrtThreadCreate(
		__xrtProcessRunOutput,
		&State.Stdout,
		0u
	);
	if ( State.Stdout.Thread == NULL ) {
		goto stop_failed;
	}
	if ( Config.Stderr.Mode != XPROCESS_IO_MERGE ) {
		State.Stderr.Thread = xrtThreadCreate(
			__xrtProcessRunOutput,
			&State.Stderr,
			0u
		);
		if ( State.Stderr.Thread == NULL ) {
			goto stop_failed;
		}
	}
	if ( bNeedInput ) {
		State.Stdin.Thread = xrtThreadCreate(
			__xrtProcessRunInput,
			&State.Stdin,
			0u
		);
		if ( State.Stdin.Thread == NULL ) {
			goto stop_failed;
		}
	}
	bThreadsReady = true;
	Wait = xrtProcessWaitUntilCancel(
		pProcess,
		Options.Deadline,
		State.Control
	);
	if ( Wait == XWAIT_ERROR ) {
		__xrtProcessRunFail(&State, xrtTakeError());
	}
	if ( Wait != XWAIT_OK ) {
		if ( !__xrtProcessRunStop(pProcess, Options.StopGrace) ) {
			__xrtProcessRunFail(&State, xrtTakeError());
		}
	}
	if ( !__xrtProcessRunThreadsJoin(&State) ) {
		__xrtProcessRunFail(&State, xrtTakeError());
	}
	if ( !xrtProcessStatus(pProcess, &pResult->Status) ) {
		__xrtProcessRunFail(&State, xrtTakeError());
	}
	pResult->Wait = __xrtProcessRunFailed(&State) ? XWAIT_ERROR : Wait;
	pResult->InputWritten = State.Stdin.Written;
	pResult->Stdout = xrtBufferTake(
		&State.Stdout.Buffer,
		&pResult->StdoutSize,
		NULL
	);
	pResult->Stderr = xrtBufferTake(
		&State.Stderr.Buffer,
		&pResult->StderrSize,
		NULL
	);
	pResult->StdoutTruncated = State.Stdout.Truncated;
	pResult->StderrTruncated = State.Stderr.Truncated;
	pResult->Duration = xrtClock() - iStart;
	bOk = !__xrtProcessRunFailed(&State);
	if ( !bOk && (State.Error != NULL) ) {
		xrtSetError(State.Error);
	}
	goto cleanup;

stop_failed:
	__xrtProcessRunFail(&State, xrtTakeError());
	(void)__xrtProcessRunStop(pProcess, Options.StopGrace);
	(void)__xrtProcessRunThreadsJoin(&State);
	goto cleanup;

cleanup:
	if ( pProcess != NULL ) {
		xrtProcessDestroy(pProcess);
	}
	if ( !bThreadsReady ) {
		(void)__xrtProcessRunThreadsJoin(&State);
	}
	xrtCancelDestroy(State.Control);
	xrtBufferUnit(&State.Stdout.Buffer);
	xrtBufferUnit(&State.Stderr.Buffer);
	xrtErrorFree(State.Error);
	if ( bLockReady ) {
		(void)xrtMutexUnit(&State.Lock);
	}
	return bOk;
}



/* 直接执行常用 Helper 使用 NULL stdin 和默认有界捕获。 */
XRT_API bool xrtProcessCapture(
	cstr sProgram,
	const cstr* pArgs,
	size_t iArgCount,
	xprocessresult* pResult
)
{
	xprocessconfig Config;

	if ( !xrtProcessConfigInit(&Config) ) {
		return false;
	}
	Config.Program = sProgram;
	Config.Args = pArgs;
	Config.ArgCount = iArgCount;
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	return xrtProcessRun(&Config, NULL, pResult);
}



/* Shell 常用 Helper 使用 NULL stdin 和默认有界捕获。 */
XRT_API bool xrtProcessShell(
	cstr sCommand,
	xprocessresult* pResult
)
{
	xprocessconfig Config;

	if ( !xrtProcessShellConfigInit(&Config, sCommand) ) {
		return false;
	}
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	return xrtProcessRun(&Config, NULL, pResult);
}

#endif
