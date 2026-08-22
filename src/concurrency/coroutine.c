#include "../internal/xrt_coroutine.h"



#if defined(XRT_FEATURE_COROUTINE)

/* 原子发布协程状态，使结果和错误先于 DONE 对其他线程可见。 */
static void __xrtCoStateStore(xcoro* pCo, xcorostate State)
{
	#if defined(_MSC_VER)
		(void)_InterlockedExchange((volatile long*)&pCo->State, (long)State);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)InterlockedExchange((volatile LONG*)&pCo->State, (LONG)State);
	#elif defined(__TINYC__)
		pCo->State = (int32)State;
	#else
		(void)__sync_lock_test_and_set(&pCo->State, (int32)State);
	#endif
}



/* 原子读取协程状态。 */
static xcorostate __xrtCoStateLoad(const xcoro* pCo)
{
	return (xcorostate)__xrtAtomicRefLoad(&pCo->State);
}



/* 原子发布协程终态原因。 */
static void __xrtCoTermStore(xcoro* pCo, xcoroterm Term)
{
	#if defined(_MSC_VER)
		(void)_InterlockedExchange((volatile long*)&pCo->Term, (long)Term);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)InterlockedExchange((volatile LONG*)&pCo->Term, (LONG)Term);
	#elif defined(__TINYC__)
		pCo->Term = (int32)Term;
	#else
		(void)__sync_lock_test_and_set(&pCo->Term, (int32)Term);
	#endif
}



/* 验证协程归属于当前原生线程。 */
static bool __xrtCoCheckOwner(const xcoro* pCo)
{
	if ( pCo == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pCo->OwnerThreadId != xrtThreadCurrentId() ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 将请求栈大小约束到平台后端可接受的稳定范围。 */
static bool __xrtCoStackSize(const xcoroargs* pArgs, size_t* pStackSize)
{
	size_t iStackSize = pArgs != NULL ? pArgs->StackSize : 0;

	if ( iStackSize == 0 ) {
		iStackSize = XRT_CORO_STACK_DEFAULT;
	}
	if ( (iStackSize < XRT_CORO_STACK_MIN) || (iStackSize > XRT_CORO_STACK_MAX) ) {
		__xrtErrorSetRange();
		return false;
	}
	*pStackSize = iStackSize;
	return true;
}



/* 在协程上下文中按后进先出顺序执行全部清理过程。 */
static void __xrtCoRunCleanup(xcoro* pCo)
{
	pCo->InCleanup = true;
	while ( pCo->CleanupTop != NULL ) {
		xcocleanup* pCleanup = pCo->CleanupTop;
		xcocleanupproc pProc = pCleanup->Proc;
		ptr pData = pCleanup->Data;
		bool bManaged = pCleanup->Managed;

		pCo->CleanupTop = pCleanup->Previous;
		pCleanup->Previous = NULL;
		pCleanup->Owner = NULL;
		pCleanup->Active = false;
		pCleanup->Managed = false;
		pProc(pData);
		if ( bManaged ) {
			xrtFree(pCleanup);
		}
	}
	pCo->InCleanup = false;
}



/* 完成协程并在最后一次状态发布前固定结果、错误和清理副作用。 */
void __xrtCoFinish(xcoro* pCo, xcoroterm Term)
{
	xrt_error_context tFinalizeError;
	xcorofinalproc pFinalize;
	ptr pFinalizeData;
	ptr pResult;
	const xerror* pError;

	if ( pCo == NULL ) {
		return;
	}
	__xrtCoRunCleanup(pCo);
	if ( Term == XCORO_TERM_RETURNED ) {
		if ( pCo->CancelConfirmed ) {
			Term = XCORO_TERM_CANCELLED;
		} else if ( pCo->ErrorContext.Error != NULL ) {
			Term = XCORO_TERM_ERROR;
		}
	}
	pFinalize = pCo->Finalize;
	pFinalizeData = pCo->FinalizeData;
	pResult = Term == XCORO_TERM_RETURNED ? pCo->Result : NULL;
	pError = Term == XCORO_TERM_ERROR ? pCo->ErrorContext.Error : NULL;
	pCo->Finalize = NULL;
	pCo->FinalizeData = NULL;
	pCo->Finalized = true;
	if ( pFinalize != NULL ) {
		xrt_error_context* pPreviousError;

		/* 终结器使用独立错误域，不能清除或替换已经确定的终态错误。 */
		memset(&tFinalizeError, 0, sizeof(tFinalizeError));
		pPreviousError = __xrtErrorContextSwap(&tFinalizeError);
		pCo->InFinalize = true;
		pFinalize(Term, pResult, pError, pFinalizeData);
		pCo->InFinalize = false;
		(void)__xrtErrorContextSwap(pPreviousError);
		xrtErrorFree(tFinalizeError.Error);
	}
	if ( (pCo->Runtime != NULL) && (pCo->Runtime->LiveCount != 0) ) {
		pCo->Runtime->LiveCount--;
	}
	__xrtCoTermStore(pCo, Term);
	__xrtCoStateStore(pCo, XCORO_DONE);
}



/* 在所属宿主线程栈上完成一个尚未进入用户过程的协程，并保护宿主上下文。 */
void __xrtCoFinishHost(xcoro* pCo, xcoroterm Term)
{
	xrt_error_context* pPreviousError;
	#if !defined(_WIN32) && !defined(_WIN64)
		xtemparena* pPreviousTemp;
	#endif

	pPreviousError = __xrtErrorContextSwap(&pCo->ErrorContext);
	#if !defined(_WIN32) && !defined(_WIN64)
		pPreviousTemp = __xrtTempContextSwap(&pCo->Temp);
	#endif
	__xrtCoFinish(pCo, Term);
	#if !defined(_WIN32) && !defined(_WIN64)
		(void)__xrtTempContextSwap(pPreviousTemp);
	#endif
	(void)__xrtErrorContextSwap(pPreviousError);
}



/* 后端首次进入协程时执行用户过程并返回宿主。 */
void __xrtCoEntry(xcoro* pCo)
{
	xrt_co_runtime* pRuntime = pCo->Runtime;

	pCo->Result = pCo->Proc(pCo->Data);
	__xrtCoFinish(pCo, XCORO_TERM_RETURNED);
	__xrtCoBackendYield(pRuntime, pCo);
	abort();
}



/* 释放一个不再活跃且已从调度器脱离的协程。 */
void __xrtCoFree(xcoro* pCo)
{
	__xrtCoBackendDestroy(pCo);
	xrtCancelDestroy(pCo->Cancel);
	xrtErrorFree(pCo->ErrorContext.Error);
	xrtTempUnit(&pCo->Temp);
	xrtFree(pCo);
}



/* 创建尚未运行的协程。 */
XRT_API xcoro* xrtCoCreate(xcoroproc pProc, ptr pData, const xcoroargs* pArgs)
{
	xcoro* pCo;
	size_t iStackSize;

	if ( pProc == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtCoStackSize(pArgs, &iStackSize) ) {
		return NULL;
	}
	pCo = (xcoro*)xrtCalloc(1, sizeof(xcoro));
	if ( pCo == NULL ) {
		return NULL;
	}
	pCo->State = XCORO_READY;
	pCo->Term = XCORO_TERM_NONE;
	pCo->OwnerThreadId = xrtThreadCurrentId();
	pCo->Proc = pProc;
	pCo->Data = pData;
	if ( pArgs != NULL ) {
		pCo->Finalize = pArgs->Finalize;
		pCo->FinalizeData = pArgs->FinalizeData;
	}
	pCo->Cancel = xrtCancelChild(pArgs != NULL ? pArgs->Cancel : NULL);
	if ( pCo->Cancel == NULL ) {
		xrtFree(pCo);
		return NULL;
	}
	if ( !xrtTempInit(&pCo->Temp, NULL) ) {
		xrtCancelDestroy(pCo->Cancel);
		xrtFree(pCo);
		return NULL;
	}
	if ( !__xrtCoBackendCreate(pCo, iStackSize) ) {
		xrtTempUnit(&pCo->Temp);
		xrtCancelDestroy(pCo->Cancel);
		xrtFree(pCo);
		return NULL;
	}
	return pCo;
}



/* 销毁未启动或已经完成的协程。 */
XRT_API bool xrtCoDestroy(xcoro* pCo)
{
	xcorostate State;

	if ( pCo == NULL ) {
		return true;
	}
	if ( !__xrtCoCheckOwner(pCo) ) {
		return false;
	}
	State = __xrtCoStateLoad(pCo);
	if ( (State != XCORO_READY) && (State != XCORO_DONE) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	#if defined(XRT_FEATURE_COROUTINE_SCHEDULER)
		if ( pCo->Scheduler != NULL ) {
			return __xrtCoSchedDestroyCoroutine(pCo);
		}
	#endif
	__xrtCoFree(pCo);
	return true;
}



/* 恢复协程并切换它独立的错误和临时内存上下文。 */
XRT_API bool xrtCoResume(xcoro* pCo)
{
	xrt_co_runtime* pRuntime;
	xrt_error_context* pPreviousError;
	xerror* pResumeError = NULL;
	xcorostate PreviousState;
	bool bFirstStart;
	#if !defined(_WIN32) && !defined(_WIN64)
		xtemparena* pPreviousTemp;
	#endif

	if ( !__xrtCoCheckOwner(pCo) ) {
		return false;
	}
	PreviousState = __xrtCoStateLoad(pCo);
	if ( (PreviousState != XCORO_READY) && (PreviousState != XCORO_SUSPENDED) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pRuntime = __xrtCoRuntimeGet(true);
	if ( pRuntime == NULL ) {
		return false;
	}
	if ( pRuntime->Current != NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !pCo->Started && xrtCancelRequested(pCo->Cancel) ) {
		__xrtCoFinishHost(pCo, XCORO_TERM_CANCELLED);
		return true;
	}
	bFirstStart = !pCo->Started;
	pCo->Started = true;
	if ( pCo->Runtime == NULL ) {
		pCo->Runtime = pRuntime;
		pRuntime->LiveCount++;
	} else if ( pCo->Runtime != pRuntime ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pRuntime->Current = pCo;
	__xrtCoStateStore(pCo, XCORO_RUNNING);
	pPreviousError = __xrtErrorContextSwap(&pCo->ErrorContext);
	#if !defined(_WIN32) && !defined(_WIN64)
		pPreviousTemp = __xrtTempContextSwap(&pCo->Temp);
	#endif
	#if !defined(_WIN32) && !defined(_WIN64)
		if ( xrtTempCurrent() != &pCo->Temp ) {
			__xrtErrorSetInvalidState();
		}
	#endif
	if (
		#if !defined(_WIN32) && !defined(_WIN64)
			xrtTempCurrent() != &pCo->Temp ||
		#endif
		!__xrtCoBackendResume(pRuntime, pCo)
	) {
		pResumeError = xrtErrorRef(pCo->ErrorContext.Error);
		#if !defined(_WIN32) && !defined(_WIN64)
			(void)__xrtTempContextSwap(pPreviousTemp);
		#endif
		(void)__xrtErrorContextSwap(pPreviousError);
		if ( pResumeError != NULL ) {
			__xrtErrorSetOwned(pResumeError);
		}
		pRuntime->Current = NULL;
		if ( bFirstStart ) {
			pCo->Started = false;
			pCo->Runtime = NULL;
			pRuntime->LiveCount--;
		}
		__xrtCoStateStore(pCo, PreviousState);
		return false;
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		(void)__xrtTempContextSwap(pPreviousTemp);
	#endif
	(void)__xrtErrorContextSwap(pPreviousError);
	pRuntime->Current = NULL;
	return true;
}



/* 让出当前协程并在恢复后检查取消状态。 */
XRT_API xwaitresult xrtCoYield(void)
{
	xrt_co_runtime* pRuntime = __xrtCoRuntimeGet(false);
	xcoro* pCo = pRuntime != NULL ? pRuntime->Current : NULL;

	if ( pCo == NULL ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	if ( pCo->InCleanup || pCo->InFinalize ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	__xrtCoStateStore(pCo, XCORO_SUSPENDED);
	__xrtCoBackendYield(pRuntime, pCo);
	return xrtCancelRequested(pCo->Cancel) ? XWAIT_CANCELLED : XWAIT_OK;
}



/* 返回当前协程。 */
XRT_API xcoro* xrtCoCurrent(void)
{
	xrt_co_runtime* pRuntime = __xrtCoRuntimeGet(false);

	return pRuntime != NULL ? pRuntime->Current : NULL;
}



/* 返回协程状态快照。 */
XRT_API xcorostate xrtCoState(const xcoro* pCo)
{
	if ( pCo == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XCORO_DONE;
	}
	return __xrtCoStateLoad(pCo);
}



/* 返回协程终态原因。 */
XRT_API xcoroterm xrtCoTerm(const xcoro* pCo)
{
	if ( pCo == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XCORO_TERM_NONE;
	}
	if ( __xrtCoStateLoad(pCo) != XCORO_DONE ) {
		return XCORO_TERM_NONE;
	}
	return (xcoroterm)__xrtAtomicRefLoad(&pCo->Term);
}



/* 返回正常完成协程的借用结果。 */
XRT_API ptr xrtCoResult(const xcoro* pCo)
{
	if ( pCo == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return (xrtCoTerm(pCo) == XCORO_TERM_RETURNED) ? pCo->Result : NULL;
}



/* 返回失败协程的借用错误。 */
XRT_API const xerror* xrtCoError(const xcoro* pCo)
{
	if ( pCo == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return (xrtCoTerm(pCo) == XCORO_TERM_ERROR) ? pCo->ErrorContext.Error : NULL;
}



/* 请求协程协作取消并通知可选调度器。 */
XRT_API bool xrtCoCancel(xcoro* pCo)
{
	if ( pCo == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtCancelRequested(pCo->Cancel) ) {
		(void)xrtCancelRequest(pCo->Cancel);
	}
	#if defined(XRT_FEATURE_COROUTINE_SCHEDULER)
		if ( pCo->Scheduler != NULL ) {
			(void)__xrtCoSchedWake(pCo);
		}
	#endif
	return true;
}



/* 返回增加引用后的协程取消令牌。 */
XRT_API xcancel* xrtCoCancelToken(const xcoro* pCo)
{
	if ( pCo == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtCancelRef(pCo->Cancel);
}



/* 判断当前协程是否收到取消请求。 */
XRT_API bool xrtCoStopping(void)
{
	xcoro* pCo = xrtCoCurrent();

	return pCo != NULL && xrtCancelRequested(pCo->Cancel);
}



/* 显式确认当前过程返回时应发布取消终态。 */
XRT_API bool xrtCoConfirmCancel(void)
{
	xcoro* pCo = xrtCoCurrent();

	if ( (pCo == NULL) || pCo->InCleanup || pCo->InFinalize ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !xrtCancelRequested(pCo->Cancel) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pCo->CancelConfirmed = true;
	return true;
}



/* 释放当前线程的惰性协程运行时。 */
XRT_API bool xrtCoThreadDetach(void)
{
	return __xrtCoRuntimeDetach();
}



/* 压入一个无分配清理节点。 */
XRT_API bool xrtCoCleanupPush(
	xcocleanup* pCleanup,
	xcocleanupproc pProc,
	ptr pData
)
{
	xcoro* pCo = xrtCoCurrent();

	if ( pCo == NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pCleanup == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pCo->InCleanup || pCo->InFinalize || pCleanup->Active ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pCleanup->Previous = pCo->CleanupTop;
	pCleanup->Owner = pCo;
	pCleanup->Proc = pProc;
	pCleanup->Data = pData;
	pCleanup->Active = true;
	pCleanup->Managed = false;
	pCo->CleanupTop = pCleanup;
	return true;
}



/* 分配一个与协程终态绑定的易用清理节点。 */
XRT_API xcocleanup* xrtCoDefer(xcocleanupproc pProc, ptr pData)
{
	xcoro* pCo = xrtCoCurrent();
	xcocleanup* pCleanup;

	if ( (pCo == NULL) || pCo->InCleanup || pCo->InFinalize ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( pProc == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pCleanup = (xcocleanup*)xrtCalloc(1, sizeof(xcocleanup));
	if ( pCleanup == NULL ) {
		return NULL;
	}
	if ( !xrtCoCleanupPush(pCleanup, pProc, pData) ) {
		xrtFree(pCleanup);
		return NULL;
	}
	pCleanup->Managed = true;
	return pCleanup;
}



/* 弹出当前协程严格匹配的栈顶清理节点。 */
XRT_API bool xrtCoCleanupPop(xcocleanup* pCleanup, bool bRun)
{
	xcoro* pCo = xrtCoCurrent();
	xcocleanupproc pProc;
	ptr pData;
	bool bManaged;

	if ( pCo == NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pCleanup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pCo->InCleanup || pCo->InFinalize || !pCleanup->Active ||
		 (pCleanup->Owner != pCo) || (pCo->CleanupTop != pCleanup) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pCo->CleanupTop = pCleanup->Previous;
	pProc = pCleanup->Proc;
	pData = pCleanup->Data;
	bManaged = pCleanup->Managed;
	pCleanup->Previous = NULL;
	pCleanup->Owner = NULL;
	pCleanup->Active = false;
	pCleanup->Managed = false;
	if ( bRun ) {
		pCo->InCleanup = true;
		pProc(pData);
		pCo->InCleanup = false;
	}
	if ( bManaged ) {
		xrtFree(pCleanup);
	}
	return true;
}



/* 返回当前目标的协程后端名称。 */
XRT_API cstr xrtCoBackend(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return "fiber-win";
	#elif defined(__x86_64__) || defined(_M_X64)
		return "asm-x64-sysv";
	#elif defined(__aarch64__)
		return "asm-arm64-aapcs";
	#elif defined(__riscv) && (__riscv_xlen == 64)
		return "asm-rv64-lp64";
	#elif defined(__loongarch64)
		return "asm-la64-lp64";
	#else
		return "unsupported";
	#endif
}

#endif
