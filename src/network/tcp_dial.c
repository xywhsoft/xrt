#include "../internal/xrt_tcp.h"
#include "../internal/xrt_net_resolver.h"



#if defined(XRT_FEATURE_NET_TCP_DIAL)

#define XRT_NET_DIAL_ATTEMPTS_DEFAULT 8u
#define XRT_NET_DIAL_ATTEMPTS_MAX 64u
#define XRT_NET_DIAL_TIMEOUT_DEFAULT 30000000u
#define XRT_NET_DIAL_FALLBACK_DEFAULT 250000u

#define XRT_NET_DIAL_GATE_OPEN 0u
#define XRT_NET_DIAL_GATE_CANCEL 1u
#define XRT_NET_DIAL_GATE_WINNER 2u
#define XRT_NET_DIAL_GATE_TERMINAL 3u



typedef struct xrt_net_dial_attempt xrt_net_dial_attempt;



/* 每个候选只在连接发布前持有 Dial 和 Stream 的调用方引用。 */
struct xrt_net_dial_attempt {
	xrt_net_dial_attempt* Next;
	xnetdial* Dial;
	xnetstream* Stream;
	size_t Index;
	bool Active;
};



/* Dial 的策略状态只在目标 Worker 上修改，公开快照使用原子字段。 */
struct xnetdial {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CancelGate;
	xatomic32 AddressCount;
	xatomic32 AttemptsStarted;
	xatomic32 AttemptsFailed;
	xatomic32 ActiveAttempts;
	xatomic32 PeakAttempts;
	xatomic32 Resources;
	xnetengine* Engine;
	xnetworker* Worker;
	xnetresolveop* Resolve;
	xnetaddrlist* ResolveAddresses;
	xerror* ResolveError;
	xnetdialconfig Config;
	xnetstreamevents StreamEvents;
	xnetdialproc Done;
	ptr StreamData;
	ptr DoneData;
	uint16 Port;
	xdeadline Deadline;
	xnetaddr* Addresses;
	size_t Count;
	size_t NextAddress;
	size_t WinnerIndex;
	xrt_net_dial_attempt* Attempts;
	xerror* Error;
	xerror* LastError;
	xnetstream* TerminalStream;
	xnetresult TerminalResult;
	xnetdialstate TerminalState;
	uint64 DeadlineTimer;
	uint64 FallbackTimer;
	__xrt_net_engine_internal StartCommand;
	__xrt_net_engine_internal ResolveCommand;
	__xrt_net_engine_internal CancelCommand;
	bool Started;
	bool ResolveReady;
	bool ResolveProcessed;
	bool WinnerPending;
	bool Stopping;
	bool Terminal;
	bool Published;
	bool EngineHeld;
	char Host[];
};



static void __xrtNetDialDrive(xnetdial* pDial);
static void __xrtNetDialProcessResolve(xnetdial* pDial);
static void __xrtNetDialResolveTask(xnetworker* pWorker, ptr pData);



/* 判断目标 Worker 是否仍可启动或补充连接候选。 */
static bool __xrtNetDialCanDrive(const xnetdial* pDial)
{
	return !pDial->Stopping && !pDial->Terminal &&
		(xrtAtomic32Load(
			&pDial->CancelGate,
			XMEMORY_ACQUIRE
		) == XRT_NET_DIAL_GATE_OPEN);
}



/* 创建带主机上下文和原因链的 Dial 错误。 */
static xerror* __xrtNetDialBuildError(
	const xnetdial* pDial,
	const xerror* pCause,
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	cstr sData
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.net";
	Desc.Code = Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Data = sData != NULL ? sData : pDial->Host;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	if ( pError == NULL ) {
		pError = xrtErrorRef(pCause);
	}
	return pError;
}



/* 把底层配置错误包装为稳定的 Dial 入口错误。 */
static void __xrtNetDialSetConfigError(const xerror* pCause, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_ARGUMENT;
	Desc.Domain = "xrt.net";
	Desc.Code = XNET_ERROR_DIAL_CONFIG;
	Desc.Operation = "configure-dial";
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 设置没有底层原因的 Dial 创建阶段错误。 */
static void __xrtNetDialSetCreateError(xerrkind Kind, cstr sMessage)
{
	__xrtNetSetError(
		Kind,
		XNET_ERROR_DIAL_CREATE,
		"create-dial",
		sMessage,
		0
	);
}



/* 把 Engine 占用失败包装为 Dial 创建错误并保留原因。 */
static void __xrtNetDialWrapCreateError(cstr sMessage)
{
	__xrtErrorWrapDetail(
		XERR_CLOSED,
		"xrt.net",
		XNET_ERROR_DIAL_CREATE,
		"create-dial",
		sMessage
	);
}



/* 内部资源归零后先释放 Engine 占用，再发布稳定终态和唯一回调。 */
static void __xrtNetDialTryPublish(xnetdial* pDial)
{
	xnetstream* pStream;

	if ( !pDial->Terminal || pDial->Published ||
		(xrtAtomic32Load(
		&pDial->Resources,
		XMEMORY_ACQUIRE
	) != 0) ) {
		return;
	}
	pStream = pDial->TerminalStream;
	pDial->TerminalStream = NULL;
	pDial->Published = true;
	if ( pDial->EngineHeld ) {
		pDial->EngineHeld = false;
		__xrtNetEngineObjectRelease(pDial->Engine);
	}
	xrtAtomic32Store(
		&pDial->State,
		(uint32)pDial->TerminalState,
		XMEMORY_RELEASE
	);
	pDial->Done(
		pDial,
		pDial->TerminalResult,
		pStream,
		pDial->Error,
		pDial->DoneData
	);
}



/* 为一个异步内部资源增加 Dial 引用和 Engine 活动计数。 */
static bool __xrtNetDialResourceAdd(xnetdial* pDial)
{
	if ( xrtNetDialRef(pDial) == NULL ) {
		return false;
	}
	(void)xrtAtomic32FetchAdd(
		&pDial->Resources,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 一个异步内部资源终结后释放其 Dial 引用。 */
static void __xrtNetDialResourceDrop(xnetdial* pDial)
{
	(void)xrtAtomic32FetchSub(
		&pDial->Resources,
		1,
		XMEMORY_ACQ_REL
	);
	__xrtNetDialTryPublish(pDial);
	xrtNetDialDestroy(pDial);
}



/* 取消仍在等待回调的两个 Timer；资源由 Timer 终态回调释放。 */
static void __xrtNetDialCancelTimers(xnetdial* pDial)
{
	if ( pDial->FallbackTimer != 0 ) {
		if ( !__xrtNetEngineTimerCancelLifecycle(
			pDial->Engine,
			pDial->FallbackTimer
		) ) {
			xrtClearError();
		}
	}
	if ( pDial->DeadlineTimer != 0 ) {
		if ( !__xrtNetEngineTimerCancelLifecycle(
			pDial->Engine,
			pDial->DeadlineTimer
		) ) {
			xrtClearError();
		}
	}
}



/* 取消旧候选时间基准；迟到终态仍由原 Timer 回调释放资源。 */
static void __xrtNetDialResetFallback(xnetdial* pDial)
{
	uint64 Id = pDial->FallbackTimer;

	if ( Id == 0 ) {
		return;
	}
	pDial->FallbackTimer = 0;
	if ( !__xrtNetEngineTimerCancelLifecycle(pDial->Engine, Id) ) {
		xrtClearError();
	}
}



/* 分离尚未派发的解析回调，并把初始资源回收交还目标 Worker。 */
static void __xrtNetDialDetachResolve(xnetdial* pDial)
{
	if ( pDial->ResolveProcessed || (pDial->Resolve == NULL) ) {
		return;
	}
	(void)xrtNetResolveOpCancel(pDial->Resolve);
	if ( !__xrtNetResolveOpClaimCallback(pDial->Resolve) ) {
		return;
	}
	xrtNetResolveOpDestroy(pDial->Resolve);
	pDial->Resolve = NULL;
	__xrtNetEnginePostInternal(
		pDial->Worker,
		&pDial->ResolveCommand,
		__xrtNetDialResolveTask,
		pDial
	);
}



/* 发布唯一终态；成功 Stream 的调用方引用在回调入口转移。 */
static void __xrtNetDialFinish(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	xerror* pError
)
{
	xnetdialstate State;
	uint32 iExpected;

	if ( pDial->Terminal ) {
		if ( pStream != NULL ) {
			__xrtNetStreamReject(pStream);
			xrtNetStreamDestroy(pStream);
		}
		return;
	}
	if ( pDial->WinnerPending ) {
		iExpected = XRT_NET_DIAL_GATE_WINNER;
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		iExpected = XRT_NET_DIAL_GATE_CANCEL;
	} else {
		iExpected = XRT_NET_DIAL_GATE_OPEN;
	}
	if ( !xrtAtomic32CompareExchange(
		&pDial->CancelGate,
		&iExpected,
		XRT_NET_DIAL_GATE_TERMINAL,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		if ( pStream != NULL ) {
			__xrtNetStreamReject(pStream);
			xrtNetStreamDestroy(pStream);
		}
		return;
	}
	__xrtNetDialDetachResolve(pDial);
	if ( Result == XNET_RESULT_OK ) {
		State = XNET_DIAL_CONNECTED;
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		State = XNET_DIAL_CANCELLED;
	} else {
		State = XNET_DIAL_FAILED;
	}
	pDial->Stopping = true;
	pDial->Error = xrtErrorRef(pError);
	pDial->TerminalStream = pStream;
	pDial->TerminalResult = Result;
	pDial->TerminalState = State;
	pDial->Terminal = true;
	__xrtNetDialCancelTimers(pDial);
	__xrtNetDialTryPublish(pDial);
}



/* 从活动链表摘除一个候选；调用方位于唯一目标 Worker。 */
static void __xrtNetDialAttemptRemove(
	xnetdial* pDial,
	xrt_net_dial_attempt* pAttempt
)
{
	xrt_net_dial_attempt** ppCurrent = &pDial->Attempts;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pAttempt) ) {
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == pAttempt ) {
		*ppCurrent = pAttempt->Next;
	}
	pAttempt->Next = NULL;
}



/* 控制器脱钩或候选关闭后释放候选及其全部持有。 */
static void __xrtNetDialAttemptRelease(ptr pData)
{
	xrt_net_dial_attempt* pAttempt =
		(xrt_net_dial_attempt*)pData;
	xnetdial* pDial = pAttempt->Dial;
	xnetstream* pStream = pAttempt->Stream;

	__xrtNetDialAttemptRemove(pDial, pAttempt);
	pAttempt->Stream = NULL;
	__xrtNetWorkerNodeRecycle(
		pDial->Worker,
		pAttempt,
		sizeof(*pAttempt)
	);
	xrtNetStreamDestroy(pStream);
	__xrtNetDialResourceDrop(pDial);
}



/* 记录候选端点及其原始失败原因，供最终错误链使用。 */
static void __xrtNetDialRememberFailure(
	xnetdial* pDial,
	size_t iIndex,
	const xerror* pCause,
	xnetresult Result
)
{
	str sEndpoint = NULL;
	xerror* pError;
	xerrkind Kind = Result == XNET_RESULT_TIMEOUT ?
		XERR_TIMEOUT : xrtErrorKind(pCause);

	if ( Kind == XERR_NONE ) {
		Kind = XERR_IO;
	}

	if ( iIndex < pDial->Count ) {
		sEndpoint = xrtNetAddrEndpointString(&pDial->Addresses[iIndex]);
	}
	pError = __xrtNetDialBuildError(
		pDial,
		pCause,
		Kind,
		XNET_ERROR_DIAL_CONNECT,
		"connect-candidate",
		"TCP candidate connection failed",
		sEndpoint
	);
	xrtFree(sEndpoint);
	if ( pError == NULL ) {
		pError = xrtErrorRef(pCause);
	}
	xrtErrorFree(pDial->LastError);
	pDial->LastError = pError;
}



/* 全部地址耗尽后用最后一个候选错误作为根因。 */
static void __xrtNetDialFinishExhausted(xnetdial* pDial)
{
	xerrkind Kind;
	xerror* pError;

	Kind = xrtErrorKind(pDial->LastError);
	if ( Kind == XERR_NONE ) {
		Kind = XERR_IO;
	}
	pError = __xrtNetDialBuildError(
		pDial,
		pDial->LastError,
		Kind,
		XNET_ERROR_DIAL_CONNECT,
		"connect-host",
		"all TCP connection attempts failed",
		NULL
	);

	if ( pError == NULL ) {
		pError = xrtErrorRef(pDial->LastError);
	}
	__xrtNetDialFinish(pDial, XNET_RESULT_ERROR, NULL, pError);
	xrtErrorFree(pError);
}



/* 无分配地拒绝除获胜者以外的全部活动候选。 */
static void __xrtNetDialRejectOthers(
	xnetdial* pDial,
	const xrt_net_dial_attempt* pWinner
)
{
	xrt_net_dial_attempt* pAttempt = pDial->Attempts;

	while ( pAttempt != NULL ) {
		xrt_net_dial_attempt* pNext = pAttempt->Next;

		if ( (pAttempt != pWinner) && pAttempt->Active ) {
			pAttempt->Active = false;
			(void)xrtAtomic32FetchSub(
				&pDial->ActiveAttempts,
				1,
				XMEMORY_ACQ_REL
			);
			__xrtNetStreamReject(pAttempt->Stream);
		}
		pAttempt = pNext;
	}
}



/* 第一个成功候选安装用户事件，并在公开 Open 前拒绝其他候选。 */
static bool __xrtNetDialAttemptOpen(xnetstream* pStream, ptr pData)
{
	xrt_net_dial_attempt* pAttempt =
		(xrt_net_dial_attempt*)pData;
	xnetdial* pDial = pAttempt->Dial;
	uint32 iExpected = XRT_NET_DIAL_GATE_OPEN;

	if ( !__xrtNetDialCanDrive(pDial) ||
		 !xrtAtomic32CompareExchange(
			&pDial->CancelGate,
			&iExpected,
			XRT_NET_DIAL_GATE_WINNER,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
		return false;
	}
	pDial->WinnerPending = true;
	if ( !__xrtNetStreamAdopt(
		pStream,
		&pDial->StreamEvents,
		pDial->StreamData
	) ) {
		__xrtNetDialRememberFailure(
			pDial,
			pAttempt->Index,
			xrtGetError(),
			XNET_RESULT_ERROR
		);
		pDial->Stopping = true;
		__xrtNetDialFinish(
			pDial,
			XNET_RESULT_ERROR,
			NULL,
			pDial->LastError
		);
		return false;
	}
	pAttempt->Active = false;
	(void)xrtAtomic32FetchSub(
		&pDial->ActiveAttempts,
		1,
		XMEMORY_ACQ_REL
	);
	pDial->WinnerIndex = pAttempt->Index;
	__xrtNetDialRejectOthers(pDial, pAttempt);
	return true;
}



/* 公开 Open 已经完成后转移获胜 Stream 引用并完成 Dial。 */
static void __xrtNetDialAttemptPublished(xnetstream* pStream, ptr pData)
{
	xrt_net_dial_attempt* pAttempt =
		(xrt_net_dial_attempt*)pData;
	xnetdial* pDial = pAttempt->Dial;

	pAttempt->Stream = NULL;
	__xrtNetDialFinish(pDial, XNET_RESULT_OK, pStream, NULL);
}



/* 候选失败时保留根因并立即补充下一个地址。 */
static void __xrtNetDialAttemptClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xrt_net_dial_attempt* pAttempt =
		(xrt_net_dial_attempt*)pData;
	xnetdial* pDial = pAttempt->Dial;
	uint32 iActive = xrtAtomic32Load(
		&pDial->ActiveAttempts,
		XMEMORY_ACQUIRE
	);

	(void)pStream;
	if ( pAttempt->Active ) {
		pAttempt->Active = false;
		iActive = xrtAtomic32FetchSub(
			&pDial->ActiveAttempts,
			1,
			XMEMORY_ACQ_REL
		) - 1u;
	}
	if ( __xrtNetDialCanDrive(pDial) && !pDial->WinnerPending ) {
		(void)xrtAtomic32FetchAdd(
			&pDial->AttemptsFailed,
			1,
			XMEMORY_RELAXED
		);
		__xrtNetDialRememberFailure(
			pDial,
			pAttempt->Index,
			pError,
			Result
		);
		if ( iActive == 0 ) {
			__xrtNetDialResetFallback(pDial);
		}
		__xrtNetDialDrive(pDial);
	}
}



/* 更新候选并发峰值。 */
static void __xrtNetDialPeak(xnetdial* pDial, uint32 iActive)
{
	uint32 iPeak = xrtAtomic32Load(
		&pDial->PeakAttempts,
		XMEMORY_RELAXED
	);

	if ( iActive > iPeak ) {
		xrtAtomic32Store(
			&pDial->PeakAttempts,
			iActive,
			XMEMORY_RELAXED
		);
	}
}



/* 启动下一个候选；同步创建失败不会阻断后续地址。 */
static bool __xrtNetDialLaunchOne(xnetdial* pDial)
{
	xrt_net_dial_attempt* pAttempt;
	__xrt_net_stream_control Control;
	size_t iIndex = pDial->NextAddress++;
	uint32 iActive;

	pAttempt = (xrt_net_dial_attempt*)__xrtNetWorkerNodeAlloc(
		pDial->Worker,
		sizeof(*pAttempt)
	);
	(void)xrtAtomic32FetchAdd(
		&pDial->AttemptsStarted,
		1,
		XMEMORY_RELAXED
	);
	if ( (pAttempt == NULL) || !__xrtNetDialResourceAdd(pDial) ) {
		__xrtNetWorkerNodeRecycle(
			pDial->Worker,
			pAttempt,
			sizeof(*pAttempt)
		);
		(void)xrtAtomic32FetchAdd(
			&pDial->AttemptsFailed,
			1,
			XMEMORY_RELAXED
		);
		__xrtNetDialRememberFailure(
			pDial,
			iIndex,
			xrtGetError(),
			XNET_RESULT_ERROR
		);
		return false;
	}
	pAttempt->Dial = pDial;
	pAttempt->Index = iIndex;
	pAttempt->Next = pDial->Attempts;
	pDial->Attempts = pAttempt;
	memset(&Control, 0, sizeof(Control));
	Control.Open = __xrtNetDialAttemptOpen;
	Control.Published = __xrtNetDialAttemptPublished;
	Control.Close = __xrtNetDialAttemptClose;
	Control.Release = __xrtNetDialAttemptRelease;
	Control.Data = pAttempt;
	pAttempt->Stream = __xrtNetStreamCreateControlled(
		pDial->Engine,
		&pDial->Addresses[iIndex],
		pDial->Config.Affinity,
		&pDial->Config.Stream,
		&Control
	);
	if ( pAttempt->Stream == NULL ) {
		(void)xrtAtomic32FetchAdd(
			&pDial->AttemptsFailed,
			1,
			XMEMORY_RELAXED
		);
		__xrtNetDialRememberFailure(
			pDial,
			iIndex,
			xrtGetError(),
			XNET_RESULT_ERROR
		);
		__xrtNetDialAttemptRelease(pAttempt);
		return false;
	}
	pAttempt->Active = true;
	iActive = xrtAtomic32FetchAdd(
		&pDial->ActiveAttempts,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	__xrtNetDialPeak(pDial, iActive);
	__xrtNetStreamStartControlled(pAttempt->Stream);
	return true;
}



/* Fallback Timer 每次只开放一个新候选，并保证自身只存在一份。 */
static void __xrtNetDialFallbackTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xnetdial* pDial = (xnetdial*)pData;
	bool bCurrent = pDial->FallbackTimer == Id;

	(void)pWorker;
	if ( bCurrent ) {
		pDial->FallbackTimer = 0;
	}
	if ( bCurrent && (Result == XNET_RESULT_OK) &&
		 __xrtNetDialCanDrive(pDial) ) {
		__xrtNetDialDrive(pDial);
	} else if ( bCurrent && (Result == XNET_RESULT_ERROR) &&
		 __xrtNetDialCanDrive(pDial) ) {
		while ( (pDial->NextAddress < pDial->Count) &&
			 __xrtNetDialCanDrive(pDial) ) {
			(void)__xrtNetDialLaunchOne(pDial);
		}
		if ( xrtAtomic32Load(
			&pDial->ActiveAttempts,
			XMEMORY_ACQUIRE
		) == 0 ) {
			__xrtNetDialFinishExhausted(pDial);
		}
	}
	__xrtNetDialResourceDrop(pDial);
}



/* 在仍有备用地址时建立唯一的延迟竞争 Timer。 */
static bool __xrtNetDialScheduleFallback(xnetdial* pDial)
{
	if ( !__xrtNetDialCanDrive(pDial) ||
		 (pDial->NextAddress >= pDial->Count) ||
		 (pDial->FallbackTimer != 0) ) {
		return true;
	}
	if ( !__xrtNetDialResourceAdd(pDial) ) {
		return false;
	}
	pDial->FallbackTimer = xrtNetEngineAfter(
		pDial->Engine,
		pDial->Config.Affinity,
		pDial->Config.FallbackDelay,
		__xrtNetDialFallbackTimer,
		pDial
	);
	if ( pDial->FallbackTimer == 0 ) {
		__xrtNetDialResourceDrop(pDial);
		return false;
	}
	return true;
}



/* 保持至少一个活动候选，并按延迟逐步扩展并发。 */
static void __xrtNetDialDrive(xnetdial* pDial)
{
	uint32 iActive;

	if ( !__xrtNetDialCanDrive(pDial) || pDial->WinnerPending ) {
		return;
	}
	iActive = xrtAtomic32Load(
		&pDial->ActiveAttempts,
		XMEMORY_ACQUIRE
	);
	while ( (pDial->NextAddress < pDial->Count) &&
		 __xrtNetDialCanDrive(pDial) &&
		 ((iActive == 0) || (pDial->Config.FallbackDelay == 0)) ) {
		(void)__xrtNetDialLaunchOne(pDial);
		if ( !__xrtNetDialCanDrive(pDial) ) {
			return;
		}
		iActive = xrtAtomic32Load(
			&pDial->ActiveAttempts,
			XMEMORY_ACQUIRE
		);
	}
	if ( (pDial->NextAddress >= pDial->Count) && (iActive == 0) ) {
		__xrtNetDialFinishExhausted(pDial);
		return;
	}
	if ( (pDial->NextAddress < pDial->Count) &&
		 !__xrtNetDialScheduleFallback(pDial) ) {
		while ( (pDial->NextAddress < pDial->Count) &&
			 __xrtNetDialCanDrive(pDial) ) {
			(void)__xrtNetDialLaunchOne(pDial);
		}
		if ( xrtAtomic32Load(
			&pDial->ActiveAttempts,
			XMEMORY_ACQUIRE
		) == 0 ) {
			__xrtNetDialFinishExhausted(pDial);
		}
	}
}



/* Deadline 到期时取消解析和候选，并发布统一超时终态。 */
static void __xrtNetDialDeadlineTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xnetdial* pDial = (xnetdial*)pData;
	xerror* pError = NULL;

	(void)pWorker;
	if ( pDial->DeadlineTimer == Id ) {
		pDial->DeadlineTimer = 0;
	}
	if ( (Result == XNET_RESULT_OK) && __xrtNetDialCanDrive(pDial) ) {
		pDial->Stopping = true;
		pError = __xrtNetDialBuildError(
			pDial,
			pDial->LastError,
			XERR_TIMEOUT,
			XNET_ERROR_DIAL_CONNECT,
			"connect-host",
			"TCP dial timed out",
			NULL
		);
		__xrtNetDialRejectOthers(pDial, NULL);
		__xrtNetDialFinish(pDial, XNET_RESULT_TIMEOUT, NULL, pError);
		xrtErrorFree(pError);
	} else if ( (Result == XNET_RESULT_ERROR) &&
		 __xrtNetDialCanDrive(pDial) ) {
		pDial->Stopping = true;
		pError = __xrtNetDialBuildError(
			pDial,
			xrtGetError(),
			XERR_INTERNAL,
			XNET_ERROR_DIAL_CREATE,
			"schedule-dial",
			"TCP dial deadline timer failed",
			NULL
		);
		__xrtNetDialRejectOthers(pDial, NULL);
		__xrtNetDialFinish(pDial, XNET_RESULT_ERROR, NULL, pError);
		xrtErrorFree(pError);
	}
	__xrtNetDialResourceDrop(pDial);
}



/* 从列表按指定地址族取下一个地址。 */
static const xnetaddr* __xrtNetDialNextFamily(
	const xnetaddrlist* pList,
	xnetfamily Family,
	size_t* pCursor
)
{
	size_t iCount = xrtNetAddrListCount(pList);

	while ( *pCursor < iCount ) {
		const xnetaddr* pAddress = xrtNetAddrListGet(
			pList,
			(*pCursor)++
		);

		if ( (pAddress != NULL) && (pAddress->Family == Family) ) {
			return pAddress;
		}
	}
	return NULL;
}



/* 保持各地址族内部顺序，并在双栈结果中交错 IPv6 与 IPv4。 */
static bool __xrtNetDialPrepareAddresses(
	xnetdial* pDial,
	xnetaddrlist* pResolved,
	xerror** ppError
)
{
	size_t iAvailable;
	size_t iLimit;
	size_t iPreferred = 0;
	size_t iAlternate = 0;
	size_t iWritten = 0;
	const xnetaddr* pFirst;
	xnetfamily Preferred;
	xnetfamily Alternate;
	bool bPreferred = true;

	*ppError = NULL;
	iAvailable = xrtNetAddrListCount(pResolved);
	iLimit = iAvailable < pDial->Config.MaxAttempts ?
		iAvailable : pDial->Config.MaxAttempts;
	if ( iLimit == 0 ) {
		return false;
	}
	pFirst = xrtNetAddrListGet(pResolved, 0);
	if ( pFirst == NULL ) {
		return false;
	}
	Preferred = (xnetfamily)pFirst->Family;
	Alternate = Preferred == XNET_FAMILY_IPV6 ?
		XNET_FAMILY_IPV4 : XNET_FAMILY_IPV6;
	pDial->Addresses = (xnetaddr*)xrtMalloc(
		iLimit * sizeof(*pDial->Addresses)
	);
	if ( pDial->Addresses == NULL ) {
		*ppError = xrtTakeError();
		return false;
	}
	while ( iWritten < iLimit ) {
		const xnetaddr* pAddress = bPreferred ?
			__xrtNetDialNextFamily(pResolved, Preferred, &iPreferred) :
			__xrtNetDialNextFamily(pResolved, Alternate, &iAlternate);

		if ( pAddress == NULL ) {
			pAddress = bPreferred ?
				__xrtNetDialNextFamily(
					pResolved,
					Alternate,
					&iAlternate
				) :
				__xrtNetDialNextFamily(
					pResolved,
					Preferred,
					&iPreferred
				);
		}
		if ( pAddress == NULL ) {
			break;
		}
		pDial->Addresses[iWritten++] = *pAddress;
		pDial->Addresses[iWritten - 1u].Port = pDial->Port;
		bPreferred = !bPreferred;
	}
	pDial->Count = iWritten;
	xrtAtomic32Store(
		&pDial->AddressCount,
		(uint32)iWritten,
		XMEMORY_RELEASE
	);
	return iWritten != 0;
}



/* 解析完成后建立候选顺序，或把 DNS 终态翻译为 Dial 错误。 */
static void __xrtNetDialProcessResolve(xnetdial* pDial)
{
	xerror* pError = NULL;
	xerror* pCause = NULL;
	xerrkind Kind;

	if ( pDial->ResolveProcessed ) {
		return;
	}
	pDial->ResolveProcessed = true;
	if ( pDial->Resolve != NULL ) {
		xrtNetResolveOpDestroy(pDial->Resolve);
		pDial->Resolve = NULL;
	}
	if ( __xrtNetDialCanDrive(pDial) ) {
		if ( pDial->ResolveAddresses == NULL ) {
			Kind = xrtErrorKind(pDial->ResolveError);
			if ( Kind == XERR_NONE ) {
				Kind = XERR_NOT_FOUND;
			}
			pError = __xrtNetDialBuildError(
				pDial,
				pDial->ResolveError,
				Kind,
				XNET_ERROR_DIAL_RESOLVE,
				"resolve-host",
				"TCP host resolution failed",
				NULL
			);
			__xrtNetDialFinish(
				pDial,
				XNET_RESULT_ERROR,
				NULL,
				pError
			);
			xrtErrorFree(pError);
		} else if ( !__xrtNetDialPrepareAddresses(
			pDial,
			pDial->ResolveAddresses,
			&pCause
		) ) {
			Kind = xrtErrorKind(pCause);
			if ( Kind == XERR_NONE ) {
				Kind = XERR_NOT_FOUND;
			}
			pError = __xrtNetDialBuildError(
				pDial,
				pCause,
				Kind,
				XNET_ERROR_DIAL_RESOLVE,
				"resolve-host",
				"TCP host resolution returned no usable addresses",
				NULL
			);
			__xrtNetDialFinish(
				pDial,
				XNET_RESULT_ERROR,
				NULL,
				pError
			);
			xrtErrorFree(pError);
			xrtErrorFree(pCause);
		} else {
			xrtAtomic32Store(
				&pDial->State,
				XNET_DIAL_CONNECTING,
				XMEMORY_RELEASE
			);
			__xrtNetDialDrive(pDial);
		}
	}
	xrtNetAddrListDestroy(pDial->ResolveAddresses);
	pDial->ResolveAddresses = NULL;
	xrtErrorFree(pDial->ResolveError);
	pDial->ResolveError = NULL;
	__xrtNetDialResourceDrop(pDial);
}



/* 解析命令可能先于启动命令到达；启动完成后统一处理一次。 */
static void __xrtNetDialResolveTask(xnetworker* pWorker, ptr pData)
{
	xnetdial* pDial = (xnetdial*)pData;

	(void)pWorker;
	pDial->ResolveReady = true;
	if ( pDial->Started ) {
		__xrtNetDialProcessResolve(pDial);
	}
}



/* 建立总截止 Timer，并释放被提前送达的解析结果。 */
static void __xrtNetDialStartTask(xnetworker* pWorker, ptr pData)
{
	xnetdial* pDial = (xnetdial*)pData;
	xerror* pError;

	(void)pWorker;
	pDial->Started = true;
	if ( __xrtNetDialCanDrive(pDial) &&
		 (pDial->Config.Timeout != 0) ) {
		if ( __xrtNetDialResourceAdd(pDial) ) {
			pDial->DeadlineTimer = xrtNetEngineSchedule(
				pDial->Engine,
				pDial->Config.Affinity,
				pDial->Deadline,
				__xrtNetDialDeadlineTimer,
				pDial
			);
			if ( pDial->DeadlineTimer == 0 ) {
				__xrtNetDialResourceDrop(pDial);
				pError = __xrtNetDialBuildError(
					pDial,
					xrtGetError(),
					XERR_AGAIN,
					XNET_ERROR_DIAL_CREATE,
					"schedule-dial",
					"TCP dial deadline could not be scheduled",
					NULL
				);
				__xrtNetDialFinish(
					pDial,
					XNET_RESULT_ERROR,
					NULL,
					pError
				);
				xrtErrorFree(pError);
			}
		} else {
			pError = __xrtNetDialBuildError(
				pDial,
				xrtGetError(),
				XERR_INTERNAL,
				XNET_ERROR_DIAL_CREATE,
				"schedule-dial",
				"TCP dial could not retain its deadline",
				NULL
			);
			__xrtNetDialFinish(
				pDial,
				XNET_RESULT_ERROR,
				NULL,
				pError
			);
			xrtErrorFree(pError);
		}
	}
	if ( pDial->ResolveReady ) {
		__xrtNetDialProcessResolve(pDial);
	}
}



/* 目标 Worker 串行执行取消，避免与候选和 Timer 状态竞争。 */
static void __xrtNetDialCancelTask(xnetworker* pWorker, ptr pData)
{
	xnetdial* pDial = (xnetdial*)pData;
	xerror* pError;

	(void)pWorker;
	if ( pDial->Terminal ) {
		__xrtNetDialResourceDrop(pDial);
		return;
	}
	pDial->Stopping = true;
	__xrtNetDialRejectOthers(pDial, NULL);
	pError = __xrtNetDialBuildError(
		pDial,
		NULL,
		XERR_CANCELLED,
		XNET_ERROR_DIAL_CONNECT,
		"cancel-dial",
		"TCP dial was cancelled",
		NULL
	);
	__xrtNetDialFinish(
		pDial,
		XNET_RESULT_CANCELLED,
		NULL,
		pError
	);
	xrtErrorFree(pError);
	__xrtNetDialResourceDrop(pDial);
}



/* Resolver 回调使用的真实投递入口，单独定义以保持回调签名可检查。 */
static void __xrtNetDialResolvedPost(xnetresolveop* pOperation, ptr pData)
{
	xnetdial* pDial = (xnetdial*)pData;
	xnetresolveopstate State = xrtNetResolveOpState(pOperation);

	if ( State == XNET_RESOLVE_RESOLVED ) {
		pDial->ResolveAddresses = xrtNetResolveOpResult(pOperation);
		if ( pDial->ResolveAddresses == NULL ) {
			pDial->ResolveError = xrtTakeError();
		}
	} else {
		pDial->ResolveError = xrtErrorRef(
			xrtNetResolveOpError(pOperation)
		);
	}
	__xrtNetEnginePostInternal(
		pDial->Worker,
		&pDial->ResolveCommand,
		__xrtNetDialResolveTask,
		pDial
	);
}



/* 初始化托管连接的默认策略。 */
XRT_API void xrtNetDialConfigInit(xnetdialconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtNetStreamConfigInit(&pConfig->Stream);
	pConfig->Family = XNET_FAMILY_UNSPEC;
	pConfig->Timeout = XRT_NET_DIAL_TIMEOUT_DEFAULT;
	pConfig->FallbackDelay = XRT_NET_DIAL_FALLBACK_DEFAULT;
	pConfig->MaxAttempts = XRT_NET_DIAL_ATTEMPTS_DEFAULT;
}



/* 统一验证 Dial 自身策略和其复用的 Stream 配置。 */
XRT_API bool xrtNetDialConfigValid(const xnetdialconfig* pConfig)
{
	xerror* pCause;

	if ( (pConfig == NULL) ||
		 ((pConfig->Family != XNET_FAMILY_UNSPEC) &&
		  (pConfig->Family != XNET_FAMILY_IPV4) &&
		  (pConfig->Family != XNET_FAMILY_IPV6)) ||
		 (pConfig->MaxAttempts == 0) ||
		 (pConfig->MaxAttempts > XRT_NET_DIAL_ATTEMPTS_MAX) ) {
		__xrtNetDialSetConfigError(NULL, "invalid TCP dial policy");
		return false;
	}
	if ( !__xrtNetStreamConfigValid(&pConfig->Stream) ) {
		pCause = xrtTakeError();
		__xrtNetDialSetConfigError(
			pCause,
			"invalid TCP dial Stream configuration"
		);
		xrtErrorFree(pCause);
		return false;
	}
	return true;
}



/* 创建一个由 Resolver 和目标网络 Worker 共同持有的 Dial。 */
XRT_API xnetdial* xrtNetDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xnetdialproc pDone,
	ptr pDoneData
)
{
	xnetdialconfig Config;
	xnetdial* pDial;
	size_t iHostSize;
	uint32 iWorkerCount;

	if ( (pEngine == NULL) || (pResolver == NULL) ||
		 (sHost == NULL) || (sHost[0] == 0) ||
		 (iPort == 0) || (pDone == NULL) ) {
		__xrtNetDialSetConfigError(NULL, "invalid TCP dial arguments");
		return NULL;
	}
	xrtNetDialConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	if ( !xrtNetDialConfigValid(&Config) ) {
		return NULL;
	}
	if ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
		__xrtNetDialSetCreateError(
			XERR_CLOSED,
			"TCP dial requires a running engine"
		);
		return NULL;
	}
	iHostSize = strlen(sHost);
	if ( iHostSize > (SIZE_MAX - sizeof(*pDial) - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iWorkerCount = xrtNetEngineWorkerCount(pEngine);
	if ( iWorkerCount == 0 ) {
		__xrtNetDialSetCreateError(
			XERR_STATE,
			"TCP dial requires an active worker"
		);
		return NULL;
	}
	if ( !__xrtNetEngineObjectHold(pEngine) ) {
		__xrtNetDialWrapCreateError("TCP dial could not hold the engine");
		return NULL;
	}
	pDial = (xnetdial*)xrtCalloc(
		1,
		sizeof(*pDial) + iHostSize + 1u
	);
	if ( pDial == NULL ) {
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	memcpy(pDial->Host, sHost, iHostSize + 1u);
	pDial->References = 2;
	xrtAtomic32Init(&pDial->State, XNET_DIAL_RESOLVING);
	xrtAtomic32Init(&pDial->CancelGate, XRT_NET_DIAL_GATE_OPEN);
	xrtAtomic32Init(&pDial->AddressCount, 0);
	xrtAtomic32Init(&pDial->AttemptsStarted, 0);
	xrtAtomic32Init(&pDial->AttemptsFailed, 0);
	xrtAtomic32Init(&pDial->ActiveAttempts, 0);
	xrtAtomic32Init(&pDial->PeakAttempts, 0);
	xrtAtomic32Init(&pDial->Resources, 1);
	pDial->Engine = pEngine;
	pDial->Worker = xrtNetEngineWorker(
		pEngine,
		(uint32)(Config.Affinity % iWorkerCount)
	);
	pDial->Config = Config;
	pDial->Done = pDone;
	pDial->StreamData = pStreamData;
	pDial->DoneData = pDoneData;
	pDial->Port = iPort;
	pDial->WinnerIndex = SIZE_MAX;
	pDial->EngineHeld = true;
	if ( pStreamEvents != NULL ) {
		pDial->StreamEvents = *pStreamEvents;
	}
	if ( Config.Timeout != 0 ) {
		pDial->Deadline = xrtDeadlineAfter(Config.Timeout);
	}
	pDial->Resolve = xrtNetResolverResolve(
		pResolver,
		pDial->Host,
		Config.Family,
		__xrtNetDialResolvedPost,
		pDial
	);
	if ( pDial->Resolve == NULL ) {
		xrtAtomic32Store(&pDial->Resources, 0, XMEMORY_RELEASE);
		pDial->EngineHeld = false;
		__xrtNetEngineObjectRelease(pEngine);
		xrtNetDialDestroy(pDial);
		xrtNetDialDestroy(pDial);
		return NULL;
	}
	__xrtNetEnginePostInternal(
		pDial->Worker,
		&pDial->StartCommand,
		__xrtNetDialStartTask,
		pDial
	);
	return pDial;
}



/* 增加 Dial 引用。 */
XRT_API xnetdial* xrtNetDialRef(xnetdial* pDial)
{
	if ( (pDial == NULL) ||
		 (xrtRefRetain(&pDial->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pDial;
}



/* 释放 Dial 引用，并在全部异步资源终结后回收对象。 */
XRT_API void xrtNetDialDestroy(xnetdial* pDial)
{
	if ( (pDial == NULL) ||
		 (xrtRefRelease(&pDial->References) != 0) ) {
		return;
	}
	xrtNetAddrListDestroy(pDial->ResolveAddresses);
	xrtErrorFree(pDial->ResolveError);
	xrtErrorFree(pDial->Error);
	xrtErrorFree(pDial->LastError);
	if ( pDial->TerminalStream != NULL ) {
		__xrtNetStreamReject(pDial->TerminalStream);
		xrtNetStreamDestroy(pDial->TerminalStream);
	}
	xrtFree(pDial->Addresses);
	xrtFree(pDial);
}



/* 原子受理一次取消请求，并无分配地交给目标 Worker。 */
XRT_API bool xrtNetDialCancel(xnetdial* pDial)
{
	uint32 iExpected = XRT_NET_DIAL_GATE_OPEN;

	if ( pDial == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtAtomic32CompareExchange(
		&pDial->CancelGate,
		&iExpected,
		XRT_NET_DIAL_GATE_CANCEL,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	if ( !__xrtNetDialResourceAdd(pDial) ) {
		xrtAtomic32Store(
			&pDial->CancelGate,
			XRT_NET_DIAL_GATE_OPEN,
			XMEMORY_RELEASE
		);
		return false;
	}
	__xrtNetEnginePostInternal(
		pDial->Worker,
		&pDial->CancelCommand,
		__xrtNetDialCancelTask,
		pDial
	);
	return true;
}



/* 原子读取 Dial 状态。 */
XRT_API xnetdialstate xrtNetDialState(const xnetdial* pDial)
{
	if ( pDial == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XNET_DIAL_FAILED;
	}
	return (xnetdialstate)xrtAtomic32Load(
		&pDial->State,
		XMEMORY_ACQUIRE
	);
}



/* 终态 acquire 读取保证错误对象已经完整发布。 */
XRT_API const xerror* xrtNetDialError(const xnetdial* pDial)
{
	xnetdialstate State;

	if ( pDial == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	State = xrtNetDialState(pDial);
	return (State == XNET_DIAL_FAILED) ||
		(State == XNET_DIAL_CANCELLED) ? pDial->Error : NULL;
}



/* 复制全部可并发观察的 Dial 统计。 */
XRT_API bool xrtNetDialStats(
	const xnetdial* pDial,
	xnetdialstats* pStats
)
{
	xnetdialstate State;

	if ( (pDial == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	State = xrtNetDialState(pDial);
	memset(pStats, 0, sizeof(*pStats));
	pStats->State = State;
	pStats->Addresses = xrtAtomic32Load(
		&pDial->AddressCount,
		XMEMORY_ACQUIRE
	);
	pStats->AttemptsStarted = xrtAtomic32Load(
		&pDial->AttemptsStarted,
		XMEMORY_ACQUIRE
	);
	pStats->AttemptsFailed = xrtAtomic32Load(
		&pDial->AttemptsFailed,
		XMEMORY_ACQUIRE
	);
	pStats->ActiveAttempts = xrtAtomic32Load(
		&pDial->ActiveAttempts,
		XMEMORY_ACQUIRE
	);
	pStats->PeakAttempts = xrtAtomic32Load(
		&pDial->PeakAttempts,
		XMEMORY_ACQUIRE
	);
	if ( State == XNET_DIAL_CONNECTED ) {
		pStats->WinnerIndex = pDial->WinnerIndex;
		pStats->HasWinner = true;
	}
	return true;
}

#endif
