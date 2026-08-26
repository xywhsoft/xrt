#include "../internal/xrt_process_pipeline.h"



#if defined(XRT_FEATURE_PROCESS_PIPELINE)

/* 一个输出泵只拥有一个阶段的一条输出流与捕获窗口。 */
typedef struct xprocesspipelinepump {
	struct xprocesspipelinestate* State;
	xprocess* Process;
	size_t Stage;
	xprocessstream Stream;
	xbuffer Buffer;
	xthread* Thread;
	bool Truncated;
} xprocesspipelinepump;



/* 首段输入线程借用调用期间稳定的输入视图。 */
typedef struct xprocesspipelineinput {
	struct xprocesspipelinestate* State;
	xprocess* Process;
	xbytesview Input;
	xthread* Thread;
	size_t Written;
} xprocesspipelineinput;



/* Pipeline 状态只串行化首次基础设施失败。 */
typedef struct xprocesspipelinestate {
	xmutex Lock;
	xcancel* Control;
	xprocesspipelineoptions Options;
	xerror* Error;
	bool Failed;
} xprocesspipelinestate;



/* 初始化空 Pipeline 结果。 */
static void __xrtProcessPipelineResultInit(
	xprocesspipelineresult* pResult
)
{
	memset(pResult, 0, sizeof(xprocesspipelineresult));
	pResult->Wait = XWAIT_ERROR;
}



/* 只发布第一项基础设施错误，并唤醒共享等待。 */
static void __xrtProcessPipelineFail(
	xprocesspipelinestate* pState,
	xerror* pError
)
{
	bool bRequest = false;

	if ( pError == NULL ) {
		__xrtProcessErrorSet(
			XERR_INTERNAL,
			XPROCESS_ERROR_WAIT,
			"pipeline",
			"pipeline failed without an error",
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



/* 查询任一 Pipeline 工作线程是否已经发布失败。 */
static bool __xrtProcessPipelineFailed(xprocesspipelinestate* pState)
{
	bool bFailed;

	(void)xrtMutexLock(&pState->Lock);
	bFailed = pState->Failed;
	(void)xrtMutexUnlock(&pState->Lock);
	return bFailed;
}



/* 并发排空一个阶段输出，并保留该阶段的归属。 */
static int32 __xrtProcessPipelineOutput(ptr pData)
{
	xprocesspipelinepump* pPump = (xprocesspipelinepump*)pData;
	unsigned char pDataBuffer[16384];
	size_t iLimit = pPump->Stream == XPROCESS_STDOUT ?
		pPump->State->Options.StdoutLimit :
		pPump->State->Options.StderrLimit;

	while ( true ) {
		int64 iRead = xrtProcessRead(
			pPump->Process,
			pPump->Stream,
			pDataBuffer,
			sizeof(pDataBuffer)
		);
		xbytesview Data;

		if ( iRead == 0 ) {
			break;
		}
		if ( iRead < 0 ) {
			__xrtProcessPipelineFail(pPump->State, xrtTakeError());
			break;
		}
		Data.Data = pDataBuffer;
		Data.Size = (size_t)iRead;
		if ( (pPump->State->Options.Output != NULL) &&
			!pPump->State->Options.Output(
				pPump->Stage,
				pPump->Stream,
				Data,
				pPump->State->Options.UserData
			) ) {
			__xrtProcessErrorSet(
				XERR_IO,
				XPROCESS_ERROR_CALLBACK,
				"pipeline.output",
				"pipeline output callback rejected a chunk",
				0
			);
			__xrtProcessPipelineFail(pPump->State, xrtTakeError());
			break;
		}
		if ( !__xrtProcessCaptureAppend(
			&pPump->Buffer,
			iLimit,
			pPump->State->Options.Overflow,
			&pPump->Truncated,
			Data
		) ) {
			__xrtProcessPipelineFail(pPump->State, xrtTakeError());
			break;
		}
	}
	(void)xrtProcessClose(pPump->Process, pPump->Stream);
	return __xrtProcessPipelineFailed(pPump->State) ? -1 : 0;
}



/* 写完首段输入后关闭 stdin 以发送 EOF。 */
static int32 __xrtProcessPipelineInput(ptr pData)
{
	xprocesspipelineinput* pInput = (xprocesspipelineinput*)pData;

	while ( pInput->Written < pInput->Input.Size ) {
		int64 iWrite = xrtProcessWrite(
			pInput->Process,
			pInput->Input.Data + pInput->Written,
			pInput->Input.Size - pInput->Written
		);

		if ( iWrite <= 0 ) {
			xrtClearError();
			break;
		}
		pInput->Written += (size_t)iWrite;
	}
	(void)xrtProcessClose(pInput->Process, XPROCESS_STDIN);
	return 0;
}



/* 丢弃停止阶段中允许发生的一项竞争错误。 */
static void __xrtProcessPipelineDiscardError(void)
{
	xerror* pError = xrtTakeError();

	xrtErrorFree(pError);
}



/* 向全部仍在运行的阶段发出同一强度的停止请求。 */
static void __xrtProcessPipelineSignal(
	xprocess** pProcesses,
	size_t iCount,
	xprocessstop Stop
)
{
	for ( size_t i = 0u; i < iCount; i++ ) {
		bool bOk;

		if ( (pProcesses[i] == NULL) ||
			(xrtProcessState(pProcesses[i]) == XPROCESS_EXITED) ) {
			continue;
		}
		if ( Stop == XPROCESS_STOP_INTERRUPT ) {
			bOk = xrtProcessInterrupt(pProcesses[i]);
		} else if ( Stop == XPROCESS_STOP_TERMINATE ) {
			bOk = xrtProcessTerminate(pProcesses[i]);
		} else {
			bOk = xrtProcessKillTree(pProcesses[i]);
			if ( !bOk ) {
				__xrtProcessPipelineDiscardError();
				bOk = xrtProcessKill(pProcesses[i]);
			}
		}
		if ( !bOk ) {
			__xrtProcessPipelineDiscardError();
		}
	}
}



/* 使用一个共享 Deadline 等待全部阶段，返回是否已全部结束。 */
static bool __xrtProcessPipelineWaitAllUntil(
	xprocess** pProcesses,
	size_t iCount,
	xdeadline iDeadline
)
{
	bool bExited = true;

	for ( size_t i = 0u; i < iCount; i++ ) {
		xwaitresult Result;

		if ( (pProcesses[i] == NULL) ||
			(xrtProcessState(pProcesses[i]) == XPROCESS_EXITED) ) {
			continue;
		}
		Result = xrtProcessWaitUntil(pProcesses[i], iDeadline);
		if ( Result != XWAIT_OK ) {
			bExited = false;
			if ( Result == XWAIT_ERROR ) {
				__xrtProcessPipelineDiscardError();
			}
		}
	}
	return bExited;
}



/* 以共享宽限窗口分级停止全部阶段，避免每段重复消耗完整宽限。 */
static bool __xrtProcessPipelineStopAll(
	xprocess** pProcesses,
	size_t iCount,
	uint64 iGrace
)
{
	__xrtProcessPipelineSignal(
		pProcesses,
		iCount,
		XPROCESS_STOP_INTERRUPT
	);
	if ( __xrtProcessPipelineWaitAllUntil(
		pProcesses,
		iCount,
		xrtDeadlineAfter(iGrace)
	) ) {
		return true;
	}
	__xrtProcessPipelineSignal(
		pProcesses,
		iCount,
		XPROCESS_STOP_TERMINATE
	);
	if ( __xrtProcessPipelineWaitAllUntil(
		pProcesses,
		iCount,
		xrtDeadlineAfter(iGrace)
	) ) {
		return true;
	}
	__xrtProcessPipelineSignal(
		pProcesses,
		iCount,
		XPROCESS_STOP_KILL_TREE
	);
	for ( size_t i = 0u; i < iCount; i++ ) {
		if ( (pProcesses[i] != NULL) &&
			(xrtProcessWait(pProcesses[i]) != XWAIT_OK) ) {
			return false;
		}
	}
	return true;
}



/* 等待并释放全部输入输出工作线程。 */
static bool __xrtProcessPipelineThreadsJoin(
	xprocesspipelinepump* pPumps,
	size_t iPumpCount,
	xprocesspipelineinput* pInput
)
{
	bool bOk = true;

	if ( pInput->Thread != NULL ) {
		if ( xrtThreadWait(pInput->Thread) != XWAIT_OK ) {
			bOk = false;
		}
		xrtThreadDestroy(pInput->Thread);
		pInput->Thread = NULL;
	}
	for ( size_t i = 0u; i < iPumpCount; i++ ) {
		if ( pPumps[i].Thread == NULL ) {
			continue;
		}
		if ( xrtThreadWait(pPumps[i].Thread) != XWAIT_OK ) {
			bOk = false;
		}
		xrtThreadDestroy(pPumps[i].Thread);
		pPumps[i].Thread = NULL;
	}
	return bOk;
}



/* 初始化 Pipeline 默认选项。 */
XRT_API bool xrtProcessPipelineOptionsInit(
	xprocesspipelineoptions* pOptions
)
{
	if ( pOptions == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"pipeline.config",
			"pipeline options output is null",
			0
		);
		return false;
	}
	memset(pOptions, 0, sizeof(xprocesspipelineoptions));
	pOptions->Deadline = XRT_DEADLINE_NEVER;
	pOptions->StopGrace = UINT64_C(250000);
	pOptions->StdoutLimit = XPROCESS_CAPTURE_LIMIT_DEFAULT;
	pOptions->StderrLimit = XPROCESS_CAPTURE_LIMIT_DEFAULT;
	pOptions->Overflow = XPROCESS_OVERFLOW_ERROR;
	return true;
}



/* 释放 Pipeline 结果拥有的逐段 stderr 与末段 stdout。 */
XRT_API void xrtProcessPipelineResultUnit(
	xprocesspipelineresult* pResult
)
{
	if ( pResult == NULL ) {
		return;
	}
	for ( size_t i = 0u; i < pResult->StageCount; i++ ) {
		xrtFree(pResult->Stages[i].Stderr);
	}
	xrtFree(pResult->Stages);
	xrtFree(pResult->Stdout);
	__xrtProcessPipelineResultInit(pResult);
}



/* 全部阶段都正常零退出才是成功 Pipeline。 */
XRT_API bool xrtProcessPipelineSuccess(
	const xprocesspipelineresult* pResult
)
{
	if ( (pResult == NULL) || (pResult->Wait != XWAIT_OK) ||
		(pResult->StageCount == 0u) ) {
		return false;
	}
	for ( size_t i = 0u; i < pResult->StageCount; i++ ) {
		if ( (pResult->Stages[i].Status.Kind != XPROCESS_EXIT_CODE) ||
			(pResult->Stages[i].Status.Code != 0) ) {
			return false;
		}
	}
	return true;
}



/* 并发运行由真实 OS pipe 连接的全部阶段。 */
XRT_API bool xrtProcessPipeline(
	const xprocessconfig* pStages,
	size_t iStageCount,
	const xprocesspipelineoptions* pOptions,
	xprocesspipelineresult* pResult
)
{
	xprocesspipelineoptions Options;
	xprocesspipelinestate State;
	xprocesspipelineinput Input;
	xprocesspipelinepump* pPumps = NULL;
	xprocessstageresult* pStageResults = NULL;
	xprocess** pProcesses = NULL;
	xprocesspipe* pPipes = NULL;
	xwaitresult Wait = XWAIT_OK;
	uint64 iStart = 0u;
	size_t iPumpCount = 0;
	bool bNeedInput = false;
	bool bLockReady = false;
	bool bOk = false;

	if ( pResult != NULL ) {
		__xrtProcessPipelineResultInit(pResult);
	}
	if ( (pStages == NULL) || (iStageCount == 0u) ||
		(iStageCount == SIZE_MAX) || (pResult == NULL) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"pipeline",
			"pipeline stages or result are invalid",
			0
		);
		return false;
	}
	if ( pOptions == NULL ) {
		(void)xrtProcessPipelineOptionsInit(&Options);
	} else {
		Options = *pOptions;
	}
	if ( ((Options.Input.Data == NULL) && (Options.Input.Size != 0u)) ||
		(Options.Overflow < XPROCESS_OVERFLOW_ERROR) ||
		(Options.Overflow > XPROCESS_OVERFLOW_KEEP_LAST) ) {
		__xrtProcessErrorSet(
			XERR_VALUE,
			XPROCESS_ERROR_CONFIG,
			"pipeline.config",
			"pipeline options are invalid",
			0
		);
		return false;
	}
	for ( size_t i = 0u; i < iStageCount; i++ ) {
		if ( pStages[i].Terminal ) {
			__xrtProcessErrorSet(
				XERR_VALUE,
				XPROCESS_ERROR_CONFIG,
				"pipeline.config",
				"terminal processes cannot be pipeline stages",
				0
			);
			return false;
		}
	}
	if ( (Options.Cancel != NULL) && xrtCancelRequested(Options.Cancel) ) {
		pResult->Wait = XWAIT_CANCELLED;
		return true;
	}

	memset(&State, 0, sizeof(State));
	memset(&Input, 0, sizeof(Input));
	State.Options = Options;
	if ( !xrtMutexInit(&State.Lock) ) {
		goto cleanup;
	}
	bLockReady = true;
	State.Control = xrtCancelChild(Options.Cancel);
	if ( State.Control == NULL ) {
		goto cleanup;
	}
	iPumpCount = iStageCount + 1u;
	pPumps = (xprocesspipelinepump*)xrtCalloc(
		iPumpCount,
		sizeof(xprocesspipelinepump)
	);
	pStageResults = (xprocessstageresult*)xrtCalloc(
		iStageCount,
		sizeof(xprocessstageresult)
	);
	pProcesses = (xprocess**)xrtCalloc(iStageCount, sizeof(xprocess*));
	if ( iStageCount > 1u ) {
		pPipes = (xprocesspipe*)xrtCalloc(
			iStageCount - 1u,
			sizeof(xprocesspipe)
		);
	}
	if ( (pPumps == NULL) || (pStageResults == NULL) ||
		(pProcesses == NULL) || ((iStageCount > 1u) && (pPipes == NULL)) ) {
		goto cleanup;
	}
	for ( size_t i = 0u; i < iPumpCount; i++ ) {
		(void)xrtBufferInit(&pPumps[i].Buffer);
		pPumps[i].State = &State;
	}
	for ( size_t i = 0u; i < (iStageCount - 1u); i++ ) {
		pPipes[i].Read = -1;
		pPipes[i].Write = -1;
	}
	for ( size_t i = 0u; i < (iStageCount - 1u); i++ ) {
		if ( !__xrtProcessPipeCreate(&pPipes[i]) ) {
			goto cleanup;
		}
	}

	iStart = xrtClock();
	for ( size_t i = 0u; i < iStageCount; i++ ) {
		xprocessconfig Config = pStages[i];

		if ( i != 0u ) {
			Config.Stdin.Mode = XPROCESS_IO_HANDLE;
			Config.Stdin.Handle = pPipes[i - 1u].Read;
		} else {
			bNeedInput = (Options.Input.Data != NULL) ||
				(Options.Input.Size != 0u) ||
				(Config.Stdin.Mode == XPROCESS_IO_PIPE);
			if ( bNeedInput ) {
				Config.Stdin.Mode = XPROCESS_IO_PIPE;
			}
		}
		if ( i + 1u < iStageCount ) {
			Config.Stdout.Mode = XPROCESS_IO_HANDLE;
			Config.Stdout.Handle = pPipes[i].Write;
		} else {
			Config.Stdout.Mode = XPROCESS_IO_PIPE;
		}
		if ( Config.Stderr.Mode != XPROCESS_IO_MERGE ) {
			Config.Stderr.Mode = XPROCESS_IO_PIPE;
		}
		pProcesses[i] = xrtProcessSpawn(&Config);
		if ( pProcesses[i] == NULL ) {
			__xrtProcessPipelineFail(&State, xrtTakeError());
			goto stop_failed;
		}
	}
	for ( size_t i = 0u; i < (iStageCount - 1u); i++ ) {
		__xrtProcessPipeClose(&pPipes[i]);
	}

	pPumps[0].Process = pProcesses[iStageCount - 1u];
	pPumps[0].Stage = iStageCount - 1u;
	pPumps[0].Stream = XPROCESS_STDOUT;
	pPumps[0].Thread = xrtThreadCreate(
		__xrtProcessPipelineOutput,
		&pPumps[0],
		0u
	);
	if ( pPumps[0].Thread == NULL ) {
		__xrtProcessPipelineFail(&State, xrtTakeError());
		goto stop_failed;
	}
	for ( size_t i = 0u; i < iStageCount; i++ ) {
		xprocesspipelinepump* pPump = &pPumps[i + 1u];

		pPump->Process = pProcesses[i];
		pPump->Stage = i;
		pPump->Stream = XPROCESS_STDERR;
		if ( xrtProcessStreamNative(
			pProcesses[i],
			XPROCESS_STDERR
		) == -1 ) {
			continue;
		}
		pPump->Thread = xrtThreadCreate(
			__xrtProcessPipelineOutput,
			pPump,
			0u
		);
		if ( pPump->Thread == NULL ) {
			__xrtProcessPipelineFail(&State, xrtTakeError());
			goto stop_failed;
		}
	}
	if ( bNeedInput ) {
		Input.State = &State;
		Input.Process = pProcesses[0];
		Input.Input = Options.Input;
		Input.Thread = xrtThreadCreate(
			__xrtProcessPipelineInput,
			&Input,
			0u
		);
		if ( Input.Thread == NULL ) {
			__xrtProcessPipelineFail(&State, xrtTakeError());
			goto stop_failed;
		}
	}

	for ( size_t i = 0u; i < iStageCount; i++ ) {
		Wait = xrtProcessWaitUntilCancel(
			pProcesses[i],
			Options.Deadline,
			State.Control
		);
		if ( Wait != XWAIT_OK ) {
			if ( Wait == XWAIT_ERROR ) {
				__xrtProcessPipelineFail(&State, xrtTakeError());
			}
			break;
		}
	}
	if ( Wait != XWAIT_OK ) {
		if ( !__xrtProcessPipelineStopAll(
			pProcesses,
			iStageCount,
			Options.StopGrace
		) ) {
			__xrtProcessPipelineFail(&State, xrtTakeError());
		}
	}
	if ( !__xrtProcessPipelineThreadsJoin(
		pPumps,
		iPumpCount,
		&Input
	) ) {
		__xrtProcessPipelineFail(&State, xrtTakeError());
	}
	for ( size_t i = 0u; i < iStageCount; i++ ) {
		if ( !xrtProcessStatus(
			pProcesses[i],
			&pStageResults[i].Status
		) ) {
			__xrtProcessPipelineFail(&State, xrtTakeError());
		}
		pStageResults[i].Stderr = xrtBufferTake(
			&pPumps[i + 1u].Buffer,
			&pStageResults[i].StderrSize,
			NULL
		);
		pStageResults[i].StderrTruncated = pPumps[i + 1u].Truncated;
	}
	pResult->Stages = pStageResults;
	pResult->StageCount = iStageCount;
	pResult->InputWritten = Input.Written;
	pResult->Stdout = xrtBufferTake(
		&pPumps[0].Buffer,
		&pResult->StdoutSize,
		NULL
	);
	pResult->StdoutTruncated = pPumps[0].Truncated;
	pResult->Wait = __xrtProcessPipelineFailed(&State) ? XWAIT_ERROR : Wait;
	pResult->Duration = xrtClock() - iStart;
	pStageResults = NULL;
	bOk = !__xrtProcessPipelineFailed(&State);
	goto cleanup;

stop_failed:
	(void)__xrtProcessPipelineStopAll(
		pProcesses,
		iStageCount,
		Options.StopGrace
	);
	(void)__xrtProcessPipelineThreadsJoin(pPumps, iPumpCount, &Input);

cleanup:
	if ( pPipes != NULL ) {
		for ( size_t i = 0u; i < (iStageCount - 1u); i++ ) {
			__xrtProcessPipeClose(&pPipes[i]);
		}
	}
	if ( pPumps != NULL ) {
		(void)__xrtProcessPipelineThreadsJoin(pPumps, iPumpCount, &Input);
		for ( size_t i = 0u; i < iPumpCount; i++ ) {
			xrtBufferUnit(&pPumps[i].Buffer);
		}
	}
	if ( pProcesses != NULL ) {
		for ( size_t i = 0u; i < iStageCount; i++ ) {
			xrtProcessDestroy(pProcesses[i]);
		}
	}
	if ( pStageResults != NULL ) {
		for ( size_t i = 0u; i < iStageCount; i++ ) {
			xrtFree(pStageResults[i].Stderr);
		}
	}
	if ( !bOk && (State.Error != NULL) ) {
		xrtSetError(State.Error);
	}
	xrtFree(pStageResults);
	xrtFree(pPumps);
	xrtFree(pProcesses);
	xrtFree(pPipes);
	xrtCancelDestroy(State.Control);
	xrtErrorFree(State.Error);
	if ( bLockReady ) {
		(void)xrtMutexUnit(&State.Lock);
	}
	return bOk;
}

#endif
