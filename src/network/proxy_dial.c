#include "../internal/xrt_proxy.h"
#include <xrt/detail/wait.h>



#if defined(XRT_FEATURE_NET_PROXY_DIAL)

#define XRT_NET_PROXY_DIAL_TIMEOUT_DEFAULT 30000
#define XRT_PROXY_GATE_OPEN 0u
#define XRT_PROXY_GATE_CANCEL 1u
#define XRT_PROXY_GATE_WINNER 2u
#define XRT_PROXY_GATE_TERMINAL 3u
#define XRT_PROXY_OWNERSHIP_READY 1u
#define XRT_PROXY_OWNERSHIP_CLEARED 2u



/* Proxy Dial 由调用方、运行阶段和可选 Timer 分别持有引用。 */
struct xnetproxydial {
	volatile int32 References;
	xmutex Lock;
	xatomic32 OwnershipActive, OwnershipState, OwnershipStop, Resources;
	xatomicptr OwnershipClaim;
	__xrt_net_engine_runtime RuntimeOwner;
	__xrt_net_engine_internal StartCommand, OwnershipCommand;
	__xrt_net_stream_events_owner* PreparedEvents;
	const xnetproxydialownershipv1* DonePolicy;
	xnetproxydialconfig Config;
	double Deadline;
	xnetresult TerminalResult;
	bool Started, Starting, TransportReady, Published, EngineHeld, LockReady;
	xatomic32 State;
	xatomic32 CancelGate;
	xatomic32 FinishGate;
	xatomic32 TimedOut;
	xatomic32 TimerDone;
	xatomic64 Timer;
	xatomicptr TransportDial;
	xatomicptr Stream;
	xnetengine* Engine;
	xnetworker* Worker;
	__xrt_net_engine_internal CancelCommand;
	xnetproxy* Proxy;
	xnetproxyhandshake* Handshake;
	xnetstreamevents StreamEvents;
	ptr StreamData;
	xnetproxydialproc Done;
	ptr DoneData;
	xerror* Error;
	size_t TargetSize;
	size_t ReceiveLimit;
	size_t WriteLimit;
	uint16 TargetPort;
	bool RuntimeHeld;
	bool Driving;
	bool DriveAgain;
	char TargetHost[];
};



/* 从网络错误域创建一份带原因链的 Proxy Dial 错误。 */
static xerror* __xrtNetProxyDialErrorCreate(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.net";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 设置创建阶段的当前线程错误。 */
static void __xrtNetProxyDialSetError(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtNetSetError(Kind, Code, sOperation, sMessage, 0);
}



static bool __xrtNetProxyDialCount(const void* pData, size_t* pCount);
static bool __xrtNetProxyDialTrace(const void* pData,
	xrtownershipvisitor visit, ptr context);
static const xrtownershipops __xrtNetProxyDialOps = {
	__xrtNetProxyDialCount, __xrtNetProxyDialTrace
};
static void __xrtNetProxyDialTryPublish(xnetproxydial* pDial);

static bool __xrtNetProxyDialEnter(xnetproxydial* pDial,bool bSubmit)
{
	xrtownershipscope Mutation={0};
	if (!pDial || !xrtOwnershipMutationBegin(&Mutation)) {if(!pDial)__xrtErrorSetInvalidArgument();return false;}
	bool bOK=!(xrtAtomic32Load(&pDial->OwnershipState,XMEMORY_ACQUIRE)&XRT_PROXY_OWNERSHIP_CLEARED) &&
		(!bSubmit || !xrtAtomicPtrLoad(&pDial->OwnershipClaim,XMEMORY_ACQUIRE));
	if (bOK) {
		uint32 iActive=xrtAtomic32Load(&pDial->OwnershipActive,XMEMORY_ACQUIRE);
		bOK=false;
		while(iActive<UINT32_MAX) {uint32 iExpected=iActive;
			if(xrtAtomic32CompareExchange(&pDial->OwnershipActive,&iExpected,iActive+1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){bOK=true;break;}
			iActive=iExpected;
		}
	}
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
	if(!bOK)__xrtErrorSetInvalidState();
	return bOK;
}
static void __xrtNetProxyDialLeave(xnetproxydial* pDial)
{
	xrtownershipscope Mutation={0};if(!xrtOwnershipMutationBegin(&Mutation))abort();
	if(!xrtAtomic32FetchSub(&pDial->OwnershipActive,1,XMEMORY_ACQ_REL))abort();
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
}
static void __xrtNetProxyDialLock(xnetproxydial* pDial)
{if(!xrtMutexLock(&pDial->Lock))abort();}
static void __xrtNetProxyDialUnlock(xnetproxydial* pDial)
{if(!xrtMutexUnlock(&pDial->Lock))abort();}
XRT_API xnetproxydial* xrtNetProxyDialRef(xnetproxydial* pDial)
{
	xrtownershipscope Mutation={0};if(!xrtOwnershipMutationBegin(&Mutation))return NULL;
	bool bOK=pDial && !(xrtAtomic32Load(&pDial->OwnershipState,XMEMORY_ACQUIRE)&XRT_PROXY_OWNERSHIP_CLEARED) &&
		xrtRefRetain(&pDial->References)>=0;
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
	if(!bOK){__xrtErrorSetInvalidArgument();return NULL;}return pDial;
}
XRT_API void xrtNetProxyDialDestroy(xnetproxydial* pDial)
{
	if(!pDial)return;
	xrtownershipscope Mutation={0};if(!xrtOwnershipMutationBegin(&Mutation))abort();
	int32 iLeft=xrtRefRelease(&pDial->References);if(iLeft<0)abort();
	if(!iLeft && (pDial->RuntimeOwner.Data || pDial->RuntimeOwner.Previous ||
		pDial->PreparedEvents || xrtAtomic32Load(&pDial->Resources,XMEMORY_ACQUIRE)))abort();
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
	if(iLeft)return;
	xrtNetDialDestroy(xrtAtomicPtrLoad(&pDial->TransportDial,XMEMORY_ACQUIRE));
	xrtNetStreamDestroy(xrtAtomicPtrLoad(&pDial->Stream,XMEMORY_ACQUIRE));
	xrtNetProxyHandshakeDestroy(pDial->Handshake);xrtNetProxyRelease(pDial->Proxy);xrtErrorFree(pDial->Error);
	if(pDial->EngineHeld)__xrtNetEngineObjectRelease(pDial->Engine);
	if(pDial->LockReady && !xrtMutexUnit(&pDial->Lock))abort();
	xrtSecureZero(pDial,sizeof(*pDial)+pDial->TargetSize+1u);xrtFree(pDial);
}
/* Every resource credit is an ACTUAL retained Root reference: setup command,
 * native timer, inner Done data, Stream binding or cancellation command. */
static bool __xrtNetProxyDialResourceAdd(xnetproxydial* pDial)
{
	xrtownershipscope Mutation={0};if(!xrtOwnershipMutationBegin(&Mutation))return false;
	bool bOK=xrtAtomic32Load(&pDial->Resources,XMEMORY_ACQUIRE)<UINT32_MAX &&
		xrtRefRetain(&pDial->References)>=0;
	if(bOK)xrtAtomic32FetchAdd(&pDial->Resources,1,XMEMORY_ACQ_REL);
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
	if(!bOK)__xrtErrorSetSizeOverflow();
	return bOK;
}
static void __xrtNetProxyDialResourceDrop(xnetproxydial* pDial)
{
	xrtownershipscope Mutation={0};if(!xrtOwnershipMutationBegin(&Mutation))abort();
	if(!xrtAtomic32FetchSub(&pDial->Resources,1,XMEMORY_ACQ_REL))abort();
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
	if(pDial->Started && xrtNetWorkerIsCurrent(pDial->Worker))__xrtNetProxyDialTryPublish(pDial);
	xrtNetProxyDialDestroy(pDial); /* this resource remains a real pin through publication */
}
static void __xrtNetProxyDialDataDrop(const void* pData)
{__xrtNetProxyDialResourceDrop((xnetproxydial*)pData);}
static void __xrtNetProxyDialDiscardPrepared(xnetproxydial* pDial)
{
	__xrt_net_stream_events_owner* pOwner=pDial->PreparedEvents;pDial->PreparedEvents=NULL;
	__xrtNetStreamEventsDiscard(pOwner);
}
/* A lifecycle cancellation always delivers the real Timer callback, including
 * cancellation. Only that physical callback consumes its resource credit. */
static void __xrtNetProxyDialCancelTimer(xnetproxydial* pDial)
{
	uint64 Id=xrtAtomic64Exchange(&pDial->Timer,0,XMEMORY_ACQ_REL);
	if(Id && !xrtAtomic32Load(&pDial->TimerDone,XMEMORY_ACQUIRE)) {
		xerror* pPrior=xrtTakeError();
		(void)__xrtNetEngineTimerCancelLifecycle(pDial->Engine,Id);
		xrtClearError();if(pPrior)xrtSetErrorTake(pPrior);
	}
}
static void __xrtNetProxyDialTryPublish(xnetproxydial* pDial)
{
	if(!pDial->Started || pDial->Starting || !xrtAtomic32Load(&pDial->FinishGate,XMEMORY_ACQUIRE))return;
	__xrtNetProxyDialLock(pDial);
	if(pDial->Published || xrtAtomic32Load(&pDial->Resources,XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pDial->OwnershipStop,XMEMORY_ACQUIRE)){
		__xrtNetProxyDialUnlock(pDial);return;
	}
	pDial->Published=true;
	xrtAtomic32Store(&pDial->CancelGate,XRT_PROXY_GATE_TERMINAL,XMEMORY_RELEASE);
	xnetproxydialproc Done=pDial->Done;ptr pData=pDial->DoneData;
	const xnetproxydialownershipv1* pDonePolicy=pDial->DonePolicy;
	pDial->Done=NULL;pDial->DoneData=NULL;pDial->DonePolicy=NULL;
	memset(&pDial->StreamEvents,0,sizeof(pDial->StreamEvents));pDial->StreamData=NULL;
	__xrtNetProxyDialUnlock(pDial);
	xnetstream* pStream=xrtAtomicPtrExchange(&pDial->Stream,NULL,XMEMORY_ACQ_REL);
	xrtNetProxyHandshakeDestroy(pDial->Handshake);pDial->Handshake=NULL;
	const xnetruntimeownershipv1* pRuntimePolicy=NULL;xrtownershipscope Mutation={0};
	if(!xrtOwnershipMutationBegin(&Mutation))abort();
	if(__xrtNetEngineRuntimeTake(pDial->Engine,&pDial->RuntimeOwner,&pRuntimePolicy)!=pDial ||
		pRuntimePolicy!=xrtNetProxyDialRuntimeOwnershipV1Get())abort();
	pDial->RuntimeHeld=false;pDial->EngineHeld=false;__xrtNetEngineObjectRelease(pDial->Engine);
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
	xrtAtomic32Store(&pDial->State,pDial->TerminalResult==XNET_RESULT_OK?XNET_PROXY_DIAL_CONNECTED:
		pDial->TerminalResult==XNET_RESULT_CANCELLED?XNET_PROXY_DIAL_CANCELLED:XNET_PROXY_DIAL_FAILED,XMEMORY_RELEASE);
	if(Done)Done(pDial,pDial->TerminalResult,pDial->TerminalResult==XNET_RESULT_OK?pStream:NULL,pDial->Error,pData);
	if(pDonePolicy)pDonePolicy->Drop(pData);
	if(pDial->TerminalResult!=XNET_RESULT_OK)xrtNetStreamDestroy(pStream);
	if(!xrtOwnershipMutationBegin(&Mutation))abort();
	xrtAtomic32Store(&pDial->OwnershipState,XRT_PROXY_OWNERSHIP_READY,XMEMORY_RELEASE);
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
	pRuntimePolicy->Release(pDial); /* last access: real runtime lease */
}
static void __xrtNetProxyDialFinish(xnetproxydial* pDial,xnetresult Result,const xerror* pError)
{
	__xrtNetProxyDialLock(pDial);
	if(xrtAtomic32Load(&pDial->FinishGate,XMEMORY_ACQUIRE)){__xrtNetProxyDialUnlock(pDial);return;}
	uint32 iGate=xrtAtomic32Load(&pDial->CancelGate,XMEMORY_ACQUIRE);
	if(iGate==XRT_PROXY_GATE_CANCEL)Result=XNET_RESULT_CANCELLED;
	else if(iGate==XRT_PROXY_GATE_OPEN)xrtAtomic32Store(&pDial->CancelGate,XRT_PROXY_GATE_TERMINAL,XMEMORY_RELEASE);
	xrtAtomic32Store(&pDial->FinishGate,1,XMEMORY_RELEASE);
	__xrtNetProxyDialUnlock(pDial);
	xerror* pCause=xrtErrorRef(pError);
	__xrtNetProxyDialCancelTimer(pDial);
	if(Result!=XNET_RESULT_OK){
		if(xrtAtomic32Load(&pDial->TimedOut,XMEMORY_ACQUIRE)){
			xerror* pTimeout=__xrtNetProxyDialErrorCreate(XERR_TIMEOUT,XNET_ERROR_PROXY_CONNECT,
				"dial-proxy","proxy dial timed out before the tunnel opened",pDial->Error?pDial->Error:pCause);
			xrtErrorFree(pDial->Error);pDial->Error=pTimeout;Result=XNET_RESULT_TIMEOUT;
		}else if(!pDial->Error){
			pDial->Error=__xrtNetProxyDialErrorCreate(Result==XNET_RESULT_CANCELLED?XERR_CANCELLED:XERR_IO,
				XNET_ERROR_PROXY_CONNECT,"dial-proxy",Result==XNET_RESULT_CANCELLED?
				"proxy dial was cancelled":"proxy dial failed before the tunnel opened",pCause);
		}
	}
	pDial->TerminalResult=Result;
	xrtErrorFree(pCause);
	__xrtNetProxyDialTryPublish(pDial);
}


/* 记录协议失败根因并让 TCP Close 统一发布组合终态。 */
static void __xrtNetProxyDialFailStream(
	xnetproxydial* pDial,
	xnetstream* pStream,
	const xerror* pError
)
{
	if ( pDial->Error == NULL ) {
		pDial->Error = xrtErrorRef(pError);
		if ( pDial->Error == NULL ) {
			pDial->Error = __xrtNetProxyDialErrorCreate(
				XERR_PROTOCOL,
				XNET_ERROR_PROXY_PROTOCOL,
				"handshake-proxy",
				"proxy handshake failed",
				NULL
			);
		}
	}
	__xrtNetStreamFailCurrent(
		pStream,
		xrtAtomic32Load(
			&pDial->CancelGate,
			XMEMORY_ACQUIRE
		) == XRT_PROXY_GATE_CANCEL ? XNET_RESULT_CANCELLED : XNET_RESULT_ERROR
	);
}



/* 完成事件切换，按 Open、预读 Read、Done 的稳定顺序发布隧道。 */
static void __xrtNetProxyDialReady(
	xnetproxydial* pDial,
	xnetstream* pStream
)
{
	xnetbuf* pInput = &pStream->ReadBuffer;
	uint32 iExpected=XRT_PROXY_GATE_OPEN;
	if(!xrtAtomic32CompareExchange(&pDial->CancelGate,&iExpected,XRT_PROXY_GATE_WINNER,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){
		__xrtNetStreamFailCurrent(pStream,XNET_RESULT_CANCELLED);return;
	}

	if ( !xrtNetStreamSetEvents(
		pStream,
		&pDial->StreamEvents,
		pDial->StreamData
	) ) {
		__xrtNetProxyDialFailStream(
			pDial, pStream, xrtGetError()
		);
		return;
	}
	xrtNetProxyHandshakeDestroy(pDial->Handshake);
	pDial->Handshake = NULL;
	if ( pDial->StreamEvents.Open != NULL ) {
		pDial->StreamEvents.Open(pStream, pDial->StreamData);
	}
	if ( !xrtNetBufEmpty(pInput) &&
		(pDial->StreamEvents.Read != NULL) ) {
		pDial->StreamEvents.Read(
			pStream,
			pInput,
			pDial->StreamData
		);
	}
	__xrtNetProxyDialFinish(pDial, XNET_RESULT_OK, NULL);
}



/* 把握手输出按 TCP 剩余硬预算分段排队，并继续解析已有输入。 */
static void __xrtNetProxyDialDriveUnsafe(
	xnetproxydial* pDial,
	xnetstream* pStream
)
{
	for ( ;; ) {
		xnetproxyhandshakestate State =
			xrtNetProxyHandshakeState(pDial->Handshake);

		if ( State == XNET_PROXY_HANDSHAKE_WRITE ) {
			xnetspan Output;
			size_t iPending = xrtNetStreamPending(pStream);
			size_t iAvailable = iPending < pDial->WriteLimit ?
				pDial->WriteLimit - iPending : 0;
			size_t iChunk;
			bytes pCopy;
			xnetresult Result;

			if ( iAvailable == 0 ) {
				return;
			}
			if ( !xrtNetProxyHandshakeOutput(
				pDial->Handshake, &Output
			) || (Output.Size == 0) ) {
				__xrtNetProxyDialSetError(
					XERR_INTERNAL,
					XNET_ERROR_PROXY_PROTOCOL,
					"send-proxy-handshake",
					"proxy handshake entered WRITE without output"
				);
				__xrtNetProxyDialFailStream(
					pDial, pStream, xrtGetError()
				);
				return;
			}
			iChunk = Output.Size < iAvailable ?
				Output.Size : iAvailable;
			pCopy = (bytes)xrtMemDup(Output.Data, iChunk);
			if ( pCopy == NULL ) {
				__xrtNetProxyDialFailStream(
					pDial, pStream, xrtGetError()
				);
				return;
			}
			Result = __xrtNetStreamSendSecureTake(pStream,pCopy,iChunk);
			if ( Result == XNET_RESULT_AGAIN ) {
				xrtSecureZero(pCopy, iChunk);
				xrtFree(pCopy);
				return;
			}
			if ( Result != XNET_RESULT_OK ) {
				xrtSecureZero(pCopy, iChunk);
				xrtFree(pCopy);
				__xrtNetProxyDialFailStream(
					pDial, pStream, xrtGetError()
				);
				return;
			}
			if ( xrtNetProxyHandshakeSent(
				pDial->Handshake, iChunk
			) != iChunk ) {
				__xrtNetProxyDialSetError(
					XERR_INTERNAL,
					XNET_ERROR_PROXY_PROTOCOL,
					"send-proxy-handshake",
					"proxy handshake rejected acknowledged output"
				);
				__xrtNetProxyDialFailStream(
					pDial, pStream, xrtGetError()
				);
				return;
			}
			continue;
		}
		if ( State == XNET_PROXY_HANDSHAKE_READ ) {
			State = xrtNetProxyHandshakeStep(
				pDial->Handshake,
				&pStream->ReadBuffer
			);
			if ( State == XNET_PROXY_HANDSHAKE_READ ) {
				return;
			}
			continue;
		}
		if ( State == XNET_PROXY_HANDSHAKE_READY ) {
			__xrtNetProxyDialReady(pDial, pStream);
			return;
		}
		__xrtNetProxyDialFailStream(
			pDial,
			pStream,
			xrtNetProxyHandshakeError(pDial->Handshake)
		);
		return;
	}
}



/* 串行推进握手，并把同步水位回调折叠为下一轮驱动。 */
static void __xrtNetProxyDialDrive(
	xnetproxydial* pDial,
	xnetstream* pStream
)
{
	if(xrtAtomic32Load(&pDial->FinishGate,XMEMORY_ACQUIRE) || !pDial->Handshake ||
		xrtAtomic32Load(&pDial->CancelGate,XMEMORY_ACQUIRE)==XRT_PROXY_GATE_CANCEL)return;
	if ( pDial->Driving ) {
		pDial->DriveAgain = true;
		return;
	}
	if ( !xrtNetProxyDialRef(pDial) ) {
		return;
	}
	pDial->Driving = true;
	do {
		pDial->DriveAgain = false;
		__xrtNetProxyDialDriveUnsafe(pDial, pStream);
	} while ( pDial->DriveAgain && !xrtAtomic32Load(
		&pDial->FinishGate,
		XMEMORY_ACQUIRE
	) );
	pDial->Driving = false;
	xrtNetProxyDialDestroy(pDial);
}



/* TCP Open 后才从所属 Worker 缓冲池创建短生命周期握手。 */
static void __xrtNetProxyDialStreamOpen(
	xnetstream* pStream,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;
	xnetproxyhandshakeconfig Config;

	if ( xrtAtomic32Load(&pDial->CancelGate, XMEMORY_ACQUIRE)==XRT_PROXY_GATE_CANCEL ||
		xrtAtomic32Load(&pDial->FinishGate,XMEMORY_ACQUIRE)) {
		__xrtNetStreamFailCurrent(
			pStream,
			XNET_RESULT_CANCELLED
		);
		return;
	}
	xrtNetProxyHandshakeConfigInit(&Config);
	Config.Proxy = pDial->Proxy;
	Config.TargetHost.Data = pDial->TargetHost;
	Config.TargetHost.Size = pDial->TargetSize;
	Config.TargetPort = pDial->TargetPort;
	Config.ReceiveLimit = pDial->ReceiveLimit;
	Config.Pool = xrtNetWorkerBufPool(pStream->Worker);
	pDial->Handshake = xrtNetProxyHandshakeCreate(&Config);
	if ( pDial->Handshake == NULL ) {
		__xrtNetProxyDialFailStream(
			pDial, pStream, xrtGetError()
		);
		return;
	}
	xrtAtomic32Store(
		&pDial->State,
		XNET_PROXY_DIAL_HANDSHAKE,
		XMEMORY_RELEASE
	);
	__xrtNetProxyDialDrive(pDial, pStream);
}



/* TCP Read 只推进协议状态机，成功时剩余输入交给最终用户事件。 */
static void __xrtNetProxyDialStreamRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;

	(void)pBuffer;
	__xrtNetProxyDialDrive(pDial, pStream);
}



/* 代理回复完成前的 EOF 是协议截断。 */
static void __xrtNetProxyDialStreamEnd(
	xnetstream* pStream,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;

	__xrtNetProxyDialSetError(
		XERR_PROTOCOL,
		XNET_ERROR_PROXY_PROTOCOL,
		"handshake-proxy",
		"proxy closed before completing the handshake"
	);
	__xrtNetProxyDialFailStream(pDial, pStream, xrtGetError());
}



/* 写队列回到低水位后继续提交尚未排队的协议输出。 */
static void __xrtNetProxyDialStreamLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)iQueued;
	__xrtNetProxyDialDrive((xnetproxydial*)pData, pStream);
}



/* 写队列完全排空也必须唤醒未跨越高水位的小上限配置。 */
static void __xrtNetProxyDialStreamDrain(
	xnetstream* pStream,
	ptr pData
)
{
	__xrtNetProxyDialDrive((xnetproxydial*)pData, pStream);
}



/* 握手前 TCP 关闭只通过 Proxy Dial 完成回调发布。 */
static void __xrtNetProxyDialStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;

	(void)pStream;
	__xrtNetProxyDialFinish(
		pDial,
		Result,
		pDial->Error != NULL ? pDial->Error : pError
	);
}



XRT_API const xnetstreameventsownershipv1* xrtNetProxyDialStreamOwnershipV1Get(void)
{
	static const xnetstreameventsownershipv1 Policy={sizeof(Policy),
		{__xrtNetProxyDialStreamOpen,__xrtNetProxyDialStreamRead,__xrtNetProxyDialStreamEnd,NULL,
		 __xrtNetProxyDialStreamLowWater,__xrtNetProxyDialStreamDrain,__xrtNetProxyDialStreamClose},
		__xrtNetProxyDialDataDrop,&__xrtNetProxyDialOps};
	return &Policy;
}
/* The inner Dial transfers a REAL creator before protocol callbacks begin.
 * Binding metadata was prepared before submission, so OOM cannot strand an
 * unobserved live Stream here. Closing streams still accept this private
 * lifecycle observer; already-closed streams need no fabricated completion. */
static void __xrtNetProxyDialBeginTunnel(xnetproxydial* pDial)
{
	if(!pDial->Started || pDial->Starting || !pDial->TransportReady || !pDial->PreparedEvents)return;
	xnetstream* pStream=xrtAtomicPtrLoad(&pDial->Stream,XMEMORY_ACQUIRE);
	if(xrtNetStreamState(pStream)==XNET_STREAM_CLOSED){
		__xrtNetProxyDialFinish(pDial,XNET_RESULT_ERROR,xrtNetStreamError(pStream));
		__xrtNetProxyDialDiscardPrepared(pDial);return;
	}
	__xrt_net_stream_events_owner* pOwner=pDial->PreparedEvents;
	if(!__xrtNetStreamEventsAttach(pStream,pOwner)){
		/* No read Future or foreign callback is installed on an inner Dial's
		 * private winner. Any other rejection is a violated native invariant. */
		abort();
	}
	pDial->PreparedEvents=NULL;
	if(xrtNetStreamState(pStream)==XNET_STREAM_OPEN)__xrtNetProxyDialStreamOpen(pStream,pDial);
}
static void __xrtNetProxyDialTransportDone(xnetdial* pTransport,xnetresult Result,
	xnetstream* pStream,const xerror* pError,ptr pData)
{
	xnetproxydial* pDial=pData;
	if(Result!=XNET_RESULT_OK){
		__xrtNetProxyDialFinish(pDial,Result,pError?pError:xrtNetDialError(pTransport));
		__xrtNetProxyDialDiscardPrepared(pDial);return;
	}
	if(!pStream || xrtAtomicPtrLoad(&pDial->Stream,XMEMORY_ACQUIRE))abort();
	xrtAtomicPtrStore(&pDial->Stream,pStream,XMEMORY_RELEASE);pDial->TransportReady=true;
	__xrtNetProxyDialBeginTunnel(pDial);
}
XRT_API const xnetdialownershipv1* xrtNetProxyDialTransportOwnershipV1Get(void)
{
	static const xnetdialownershipv1 Policy={sizeof(Policy),__xrtNetProxyDialTransportDone,
		__xrtNetProxyDialDataDrop,&__xrtNetProxyDialOps};
	return &Policy;
}
static void __xrtNetProxyDialStopWorker(xnetproxydial* pDial,bool bTimeout)
{
	if(xrtAtomic32Load(&pDial->FinishGate,XMEMORY_ACQUIRE))return;
	__xrtNetProxyDialLock(pDial);
	uint32 iGate=xrtAtomic32Load(&pDial->CancelGate,XMEMORY_ACQUIRE);
	if(iGate==XRT_PROXY_GATE_OPEN){
		xrtAtomic32Store(&pDial->CancelGate,XRT_PROXY_GATE_CANCEL,XMEMORY_RELEASE);
		if(bTimeout)xrtAtomic32Store(&pDial->TimedOut,1,XMEMORY_RELEASE);
	}else if(iGate!=XRT_PROXY_GATE_CANCEL || bTimeout){__xrtNetProxyDialUnlock(pDial);return;}
	__xrtNetProxyDialUnlock(pDial);
	xnetstream* pStream=xrtAtomicPtrLoad(&pDial->Stream,XMEMORY_ACQUIRE);
	if(pStream){
		__xrtNetProxyDialBeginTunnel(pDial);
		if(xrtNetStreamState(pStream)!=XNET_STREAM_CLOSED)__xrtNetStreamFailCurrent(pStream,XNET_RESULT_CANCELLED);
		else __xrtNetProxyDialFinish(pDial,XNET_RESULT_CANCELLED,xrtNetStreamError(pStream));
	}else{
		xnetdial* pTransport=xrtAtomicPtrLoad(&pDial->TransportDial,XMEMORY_ACQUIRE);
		if(pTransport)(void)xrtNetDialCancel(pTransport);
	}
}
static void __xrtNetProxyDialCancelTask(xnetworker* pWorker,ptr pData)
{
	(void)pWorker;xnetproxydial* pDial=pData;
	__xrtNetProxyDialStopWorker(pDial,false);
	__xrtNetProxyDialResourceDrop(pDial);
}
static void __xrtNetProxyDialTimer(xnetworker* pWorker,uint64 Id,xnetresult Result,ptr pData)
{
	(void)pWorker;(void)Id;xnetproxydial* pDial=pData;
	xrtAtomic32Store(&pDial->TimerDone,1,XMEMORY_RELEASE);
	xrtAtomic64Store(&pDial->Timer,0,XMEMORY_RELEASE);
	if(Result==XNET_RESULT_OK)__xrtNetProxyDialStopWorker(pDial,true);
	__xrtNetProxyDialResourceDrop(pDial);
}
static void __xrtNetProxyDialStartTask(xnetworker* pWorker,ptr pData)
{
	(void)pWorker;xnetproxydial* pDial=pData;pDial->Started=true;pDial->Starting=true;
	bool bHasDeadline=pDial->Config.Timeout!=0 && pDial->Config.Timeout!=XRT_WAIT_FOREVER;
	if(!xrtAtomic32Load(&pDial->FinishGate,XMEMORY_ACQUIRE) &&
		xrtAtomic32Load(&pDial->CancelGate,XMEMORY_ACQUIRE)==XRT_PROXY_GATE_OPEN && bHasDeadline){
		if(__xrtWaitExpired(pDial->Deadline)){
			__xrtNetProxyDialStopWorker(pDial,true);
		}else if(!__xrtNetProxyDialResourceAdd(pDial)){
			__xrtNetProxyDialFinish(pDial,XNET_RESULT_ERROR,xrtGetError());
		}else{
			uint64 Id=__xrtNetEngineSchedule(pDial->Engine,pDial->Config.Transport.Affinity,
				pDial->Deadline,__xrtNetProxyDialTimer,pDial);
			if(!Id){
				xerror* pCause=xrtTakeError();
				__xrtNetProxyDialResourceDrop(pDial);
				__xrtNetProxyDialFinish(pDial,XNET_RESULT_ERROR,pCause);
				xrtErrorFree(pCause);
			}else{
				xrtAtomic64Store(&pDial->Timer,Id,XMEMORY_RELEASE);
				if(!__xrtNetEngineRuntimeTimer(pDial->Worker,Id,xrtNetProxyDialRuntimeOwnershipV1Get()))abort();
			}
		}
	}
	pDial->Starting=false;
	if(xrtAtomic32Load(&pDial->FinishGate,XMEMORY_ACQUIRE)){
		xnetdial* pTransport=xrtAtomicPtrLoad(&pDial->TransportDial,XMEMORY_ACQUIRE);
		if(pTransport)(void)xrtNetDialCancel(pTransport);
	}
	__xrtNetProxyDialBeginTunnel(pDial);
	if(xrtAtomic32Load(&pDial->CancelGate,XMEMORY_ACQUIRE)==XRT_PROXY_GATE_CANCEL)
		__xrtNetProxyDialStopWorker(pDial,false);
	__xrtNetProxyDialResourceDrop(pDial); /* actual setup command reference */
}


XRT_API void xrtNetProxyDialConfigInit(xnetproxydialconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CONFIG,
			"init-proxy-dial-config",
			"proxy dial config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtNetDialConfigInit(&pConfig->Transport);
	pConfig->Timeout = XRT_NET_PROXY_DIAL_TIMEOUT_DEFAULT;
	pConfig->ReceiveLimit = XRT_NET_PROXY_RECEIVE_LIMIT;
}



/* 验证构建后端、TCP 缓冲边界和代理输入硬上限。 */
static bool __xrtNetProxyDialConfigValid(
	const xnetproxy* pProxy,
	const xnetproxydialconfig* pConfig,
	xstrview Target
)
{
	xnetproxyinfo Info;

	(void)Target;

	if ( !__xrtNetStreamConfigValid(&pConfig->Transport.Stream) ) {
		return false;
	}
	if ( (pConfig->ReceiveLimit < 4u) ||
		(pConfig->ReceiveLimit > pConfig->Transport.Stream.ReadLimit) ) {
		__xrtNetProxyDialSetError(
			XERR_RANGE,
			XNET_ERROR_PROXY_LIMIT,
			"configure-proxy-dial",
			"proxy receive limit must fit the TCP read limit"
		);
		return false;
	}
	if ( !xrtNetProxyInfo(pProxy, &Info) ) {
		return false;
	}
	if ( Info.Type == XNET_PROXY_SOCKS5 ) {
		#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)
			xnetaddr Address;

			if ( !__xrtNetAddrTryParse(
				&Address,
				Target,
				0
			) && (Target.Size > UINT8_MAX) ) {
				__xrtNetProxyDialSetError(
					XERR_RANGE,
					XNET_ERROR_PROXY_LIMIT,
					"configure-proxy-dial",
					"SOCKS5 target host exceeds the protocol limit"
				);
				return false;
			}
			return true;
		#else
			__xrtNetProxyDialSetError(
				XERR_UNSUPPORTED,
				XNET_ERROR_PROXY_UNSUPPORTED,
				"configure-proxy-dial",
				"SOCKS5 support is not compiled"
			);
			return false;
		#endif
	}
	if ( Info.Type == XNET_PROXY_HTTP_CONNECT ) {
		#if defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)
			return true;
		#else
			__xrtNetProxyDialSetError(
				XERR_UNSUPPORTED,
				XNET_ERROR_PROXY_UNSUPPORTED,
				"configure-proxy-dial",
				"HTTP CONNECT support is not compiled"
			);
			return false;
		#endif
	}
	__xrtNetProxyDialSetError(
		XERR_VALUE,
		XNET_ERROR_PROXY_CONFIG,
		"configure-proxy-dial",
		"proxy type is invalid"
	);
	return false;
}



static void __xrtNetProxyDialConstructionDrop(xnetproxydial* pDial)
{
	__xrtNetProxyDialDiscardPrepared(pDial);
	__xrtNetProxyDialResourceDrop(pDial); /* unposted setup credit */
	if(pDial->RuntimeHeld){
		const xnetruntimeownershipv1* pPolicy=NULL;
		if(__xrtNetEngineRuntimeTake(pDial->Engine,&pDial->RuntimeOwner,&pPolicy)!=pDial ||
			pPolicy!=xrtNetProxyDialRuntimeOwnershipV1Get())abort();
		pDial->RuntimeHeld=false;pPolicy->Release(pDial);
	}else xrtNetProxyDialDestroy(pDial); /* unadopted runtime credit */
	xrtNetProxyDialDestroy(pDial); /* failed creator */
}
static xnetproxydial* __xrtNetProxyDialCreate(xnetengine* pEngine,xnetresolver* pResolver,
	const xnetproxy* pProxy,cstr sTargetHost,uint16 iTargetPort,const xnetproxydialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,ptr pStreamData,xnetproxydialproc Done,ptr pDoneData,
	const xnetproxydialownershipv1* pDonePolicy)
{
	if(!pEngine || !pResolver || !pProxy || !sTargetHost || !sTargetHost[0] || !iTargetPort || !Done){
		__xrtErrorSetInvalidArgument();return NULL;
	}
	xnetproxydialconfig Config;xrtNetProxyDialConfigInit(&Config);if(pConfig)Config=*pConfig;
	xstrview Target={sTargetHost,strlen(sTargetHost)};
	if(!__xrtNetProxyHostValid(Target,"dial-proxy","proxy target host is empty") ||
		!__xrtNetProxyDialConfigValid(pProxy,&Config,Target))return NULL;
	if(Config.Timeout<XRT_WAIT_FOREVER){__xrtErrorSetRange();return NULL;}
	xnetproxyinfo Info;if(!xrtNetProxyInfo(pProxy,&Info))return NULL;
	if(!__xrtNetEngineObjectHold(pEngine))return NULL;
	uint32 iWorkers=xrtNetEngineWorkerCount(pEngine);
	if(!iWorkers){__xrtNetEngineObjectRelease(pEngine);__xrtErrorSetInvalidState();return NULL;}
	xnetproxydial* pDial=xrtCalloc(1,sizeof(*pDial)+Target.Size+1u);
	if(!pDial){__xrtNetEngineObjectRelease(pEngine);return NULL;}
	/* Creator, registered-runtime reservation, and an ACTUAL setup credit. */
	pDial->References=3;pDial->Engine=pEngine;pDial->EngineHeld=true;
	pDial->Worker=xrtNetEngineWorker(pEngine,(uint32)(Config.Transport.Affinity%iWorkers));
	pDial->TargetSize=Target.Size;memcpy(pDial->TargetHost,sTargetHost,Target.Size+1u);
	xrtAtomic32Init(&pDial->OwnershipActive,1);xrtAtomic32Init(&pDial->OwnershipState,0);
	xrtAtomic32Init(&pDial->OwnershipStop,0);xrtAtomic32Init(&pDial->Resources,1);
	xrtAtomicPtrInit(&pDial->OwnershipClaim,NULL);xrtAtomic32Init(&pDial->State,XNET_PROXY_DIAL_RESOLVING);
	xrtAtomic32Init(&pDial->CancelGate,XRT_PROXY_GATE_OPEN);xrtAtomic32Init(&pDial->FinishGate,0);
	xrtAtomic32Init(&pDial->TimedOut,0);xrtAtomic32Init(&pDial->TimerDone,0);
	xrtAtomic64Init(&pDial->Timer,0);xrtAtomicPtrInit(&pDial->TransportDial,NULL);xrtAtomicPtrInit(&pDial->Stream,NULL);
	pDial->Config=Config;pDial->TargetPort=iTargetPort;pDial->ReceiveLimit=Config.ReceiveLimit;
	pDial->WriteLimit=Config.Transport.Stream.WriteLimit;
	pDial->Deadline=Config.Timeout!=0?__xrtWaitAfter(Config.Timeout):0;
	pDial->Done=Done;pDial->DoneData=pDoneData;pDial->StreamData=pStreamData;
	if(pStreamEvents)pDial->StreamEvents=*pStreamEvents;
	if(!xrtMutexInit(&pDial->Lock))goto failed;
	pDial->LockReady=true;pDial->Proxy=xrtNetProxyRetain(pProxy);if(!pDial->Proxy)goto failed;
	if(!__xrtNetEngineRuntimeAdopt(pEngine,&pDial->RuntimeOwner,pDial,xrtNetProxyDialRuntimeOwnershipV1Get()))goto failed;
	pDial->RuntimeHeld=true;
	/* Reserve the event-data credit and its UNIQUE binding BEFORE any I/O.
	 * Rejection/OOM after winning a Stream never needs another allocation. */
	if(!__xrtNetProxyDialResourceAdd(pDial))goto failed;
	pDial->PreparedEvents=__xrtNetStreamEventsPrepare(pDial,xrtNetProxyDialStreamOwnershipV1Get());
	if(!pDial->PreparedEvents){__xrtNetProxyDialResourceDrop(pDial);goto failed;}
	if(!__xrtNetProxyDialResourceAdd(pDial))goto failed;
	xnetdial* pTransport=xrtNetDialOwnedV1(pEngine,pResolver,Info.Host.Data,Info.Port,&Config.Transport,
		NULL,NULL,pDial,xrtNetProxyDialTransportOwnershipV1Get());
	if(!pTransport){__xrtNetProxyDialResourceDrop(pDial);goto failed;}
	xrtAtomicPtrStore(&pDial->TransportDial,pTransport,XMEMORY_RELEASE);
	pDial->DonePolicy=pDonePolicy; /* accepted submission consumes ONE DoneData credit */
	/* LiveObjects is still held, so legal Stop/TryStop cannot seal this
	 * allocation-free lifecycle post. No callback runs in the caller stack. */
	if(!__xrtNetEnginePostInternal(pDial->Worker,&pDial->StartCommand,__xrtNetProxyDialStartTask,pDial))abort();
	__xrtNetProxyDialLeave(pDial);
	return pDial;
failed:{
	xerror* pError=xrtTakeError();
	__xrtNetProxyDialConstructionDrop(pDial);
	xrtClearError();if(pError)xrtSetErrorTake(pError);
	return NULL;
	}
}
XRT_API xnetproxydial* xrtNetProxyDial(xnetengine* pEngine,xnetresolver* pResolver,
	const xnetproxy* pProxy,cstr sTargetHost,uint16 iTargetPort,const xnetproxydialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,ptr pStreamData,xnetproxydialproc Done,ptr pDoneData)
{return __xrtNetProxyDialCreate(pEngine,pResolver,pProxy,sTargetHost,iTargetPort,pConfig,pStreamEvents,pStreamData,Done,pDoneData,NULL);}
XRT_API xnetproxydial* xrtNetProxyDialOwnedV1(xnetengine* pEngine,xnetresolver* pResolver,
	const xnetproxy* pProxy,cstr sTargetHost,uint16 iTargetPort,const xnetproxydialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,ptr pStreamData,ptr pDoneData,const xnetproxydialownershipv1* pPolicy)
{
	if(!pDoneData || !pPolicy || pPolicy->size!=sizeof(*pPolicy) || !pPolicy->Done || !pPolicy->Drop ||
		!pPolicy->Ops || !pPolicy->Ops->Count || !pPolicy->Ops->Trace){__xrtErrorSetInvalidArgument();return NULL;}
	return __xrtNetProxyDialCreate(pEngine,pResolver,pProxy,sTargetHost,iTargetPort,pConfig,pStreamEvents,
		pStreamData,pPolicy->Done,pDoneData,pPolicy);
}
XRT_API bool xrtNetProxyDialCancel(xnetproxydial* pDial)
{
	if(!__xrtNetProxyDialEnter(pDial,true))return false;
	__xrtNetProxyDialLock(pDial);
	bool bOK=false;
	if(!xrtAtomic32Load(&pDial->FinishGate,XMEMORY_ACQUIRE) &&
		xrtAtomic32Load(&pDial->CancelGate,XMEMORY_ACQUIRE)==XRT_PROXY_GATE_OPEN &&
		__xrtNetProxyDialResourceAdd(pDial)){
		uint32 iGate=XRT_PROXY_GATE_OPEN;
		if(xrtAtomic32CompareExchange(&pDial->CancelGate,&iGate,XRT_PROXY_GATE_CANCEL,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){
			bOK=__xrtNetEnginePostInternal(pDial->Worker,&pDial->CancelCommand,__xrtNetProxyDialCancelTask,pDial);
			if(!bOK)xrtAtomic32Store(&pDial->CancelGate,XRT_PROXY_GATE_OPEN,XMEMORY_RELEASE);
		}
		if(!bOK){
			/* Publication waits for this mutex; no failed command credit is
			 * leaked and no non-Worker completion is fabricated. */
			xrtownershipscope Mutation={0};if(!xrtOwnershipMutationBegin(&Mutation))abort();
			if(!xrtAtomic32FetchSub(&pDial->Resources,1,XMEMORY_ACQ_REL))abort();
			if(!xrtOwnershipScopeEnd(&Mutation))abort();
			xrtNetProxyDialDestroy(pDial);
		}
	}
	__xrtNetProxyDialUnlock(pDial);
	__xrtNetProxyDialLeave(pDial);return bOK;
}


/* 解析和 TCP 连接阶段直接映射底层 TCP Dial 快照。 */
XRT_API xnetproxydialstate xrtNetProxyDialState(
	const xnetproxydial* pDial
)
{
	xnetproxydialstate State;
	xnetdial* pTransportDial;
	xnetdialstate TransportState;

	if ( pDial == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CREATE,
			"state-proxy-dial",
			"proxy dial is null"
		);
		return XNET_PROXY_DIAL_FAILED;
	}
	State = (xnetproxydialstate)xrtAtomic32Load(
		&pDial->State,
		XMEMORY_ACQUIRE
	);
	if ( (State != XNET_PROXY_DIAL_RESOLVING) &&
		(State != XNET_PROXY_DIAL_CONNECTING) ) {
		return State;
	}
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	if ( pTransportDial == NULL ) {
		return State;
	}
	TransportState = xrtNetDialState(pTransportDial);
	if ( TransportState == XNET_DIAL_CONNECTING ) {
		return XNET_PROXY_DIAL_CONNECTING;
	}
	if ( TransportState == XNET_DIAL_CONNECTED ) {
		return XNET_PROXY_DIAL_HANDSHAKE;
	}
	return State;
}



/* 终态后返回 DNS、TCP、代理协议或全过程超时错误。 */
XRT_API const xerror* xrtNetProxyDialError(
	const xnetproxydial* pDial
)
{
	xnetproxydialstate State;

	if ( pDial == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CREATE,
			"error-proxy-dial",
			"proxy dial is null"
		);
		return NULL;
	}
	State = xrtNetProxyDialState(pDial);
	return ((State == XNET_PROXY_DIAL_FAILED) ||
		(State == XNET_PROXY_DIAL_CANCELLED)) ?
		pDial->Error : NULL;
}



/* 复制组合阶段和底层地址竞速统计。 */
XRT_API bool xrtNetProxyDialStats(
	const xnetproxydial* pDial,
	xnetproxydialstats* pStats
)
{
	xnetdial* pTransportDial;

	if ( (pDial == NULL) || (pStats == NULL) ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CREATE,
			"stats-proxy-dial",
			"proxy dial or statistics output is null"
		);
		return false;
	}
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	if ( pTransportDial == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_STATE,
			XNET_ERROR_PROXY_CONNECT,
			"stats-proxy-dial",
			"proxy TCP dial is not available"
		);
		return false;
	}
	memset(pStats, 0, sizeof(*pStats));
	pStats->State = xrtNetProxyDialState(pDial);
	return xrtNetDialStats(pTransportDial, &pStats->Transport);
}

static bool __xrtNetProxyDialPrepared(const void* pData)
{
	const xnetproxydial* pDial=pData;
	return pDial && !xrtAtomic32Load(&pDial->OwnershipActive,XMEMORY_ACQUIRE) &&
		(xrtAtomic32Load(&pDial->OwnershipState,XMEMORY_ACQUIRE)&XRT_PROXY_OWNERSHIP_READY);
}
static bool __xrtNetProxyDialBusy(const void* pData)
{
	const xnetproxydial* pDial=pData;if(!pDial)return false;
	if(xrtAtomic32Load(&pDial->OwnershipActive,XMEMORY_ACQUIRE))return true;
	if(xrtAtomic32Load(&pDial->OwnershipState,XMEMORY_ACQUIRE)&XRT_PROXY_OWNERSHIP_READY)return false;
	return !pDial->EngineHeld || xrtNetEngineOwnershipQuiescenceV1Get()->Busy(pDial->Engine);
}
static bool __xrtNetProxyDialEmptyEvents(const xnetstreamevents* e)
{return !e->Open&&!e->Read&&!e->End&&!e->HighWater&&!e->LowWater&&!e->Drain&&!e->Close;}
/* Handshake and its buffer records are UNIQUE children, not fabricated RC
 * nodes. Report the actual copied Proxy credit, Error and buffer-pool leases. */
static bool __xrtNetProxyDialHandshakeWalk(const xnetproxydial* pDial,xrtownershipvisitor visit,ptr context)
{
	const xnetproxyhandshake* h=pDial->Handshake;if(!h)return true;
	if(h->Proxy!=pDial->Proxy || !__xrtNetBufOwnershipWalk(&h->Output,NULL,NULL,visit,context))return false;
	if(visit){
		if(!visit(xrtNetProxyOwnership(h->Proxy),context))return false;
		xrtownershipref error=xrtErrorOwnership(h->Error);if(error.Data&&!visit(error,context))return false;
	}
	return true;
}
static bool __xrtNetProxyDialCount(const void* pData,size_t* pCount)
{
	const xnetproxydial* pDial=pData;
	if(!pDial || !pCount || __xrtNetProxyDialBusy(pDial) ||
		(xrtAtomic32Load(&pDial->OwnershipState,XMEMORY_ACQUIRE)&XRT_PROXY_OWNERSHIP_CLEARED) ||
		pDial->StreamData || !__xrtNetProxyDialEmptyEvents(&pDial->StreamEvents) ||
		(pDial->Done&&!pDial->DonePolicy) || !pDial->Proxy)return false;
	if(__xrtNetProxyDialPrepared(pDial)){
		if(!pDial->Published || pDial->EngineHeld || pDial->RuntimeHeld || pDial->RuntimeOwner.Data || pDial->RuntimeOwner.Previous ||
			pDial->Done || pDial->DoneData || pDial->DonePolicy || pDial->PreparedEvents || pDial->Handshake ||
			xrtAtomicPtrLoad(&pDial->Stream,XMEMORY_ACQUIRE) || xrtAtomic64Load(&pDial->Timer,XMEMORY_ACQUIRE) ||
			xrtAtomic32Load(&pDial->Resources,XMEMORY_ACQUIRE))return false;
	}else if(!pDial->EngineHeld || !pDial->RuntimeHeld || pDial->RuntimeOwner.Data!=pDial ||
		!pDial->RuntimeOwner.Previous || pDial->RuntimeOwner.Engine!=pDial->Engine ||
		pDial->RuntimeOwner.Policy!=xrtNetProxyDialRuntimeOwnershipV1Get())return false;
	if(pDial->PreparedEvents && (pDial->PreparedEvents->Next || pDial->PreparedEvents->Data!=pDial ||
		pDial->PreparedEvents->Policy!=xrtNetProxyDialStreamOwnershipV1Get()))return false;
	if(!__xrtNetProxyDialHandshakeWalk(pDial,NULL,NULL))return false;
	int32 refs=__xrtAtomicRefLoad(&pDial->References);if(refs<=0)return false;
	*pCount=(size_t)refs;return true;
}
static bool __xrtNetProxyDialTrace(const void* pData,xrtownershipvisitor visit,ptr context)
{
	const xnetproxydial* pDial=pData;size_t refs;if(!visit||!__xrtNetProxyDialCount(pData,&refs))return false;
	if(pDial->EngineHeld&&!visit(xrtNetEngineOwnership(pDial->Engine),context))return false;
	if(!visit(xrtNetProxyOwnership(pDial->Proxy),context))return false;
	xnetdial* transport=xrtAtomicPtrLoad(&pDial->TransportDial,XMEMORY_ACQUIRE);
	if(transport&&!visit(xrtNetDialOwnership(transport),context))return false;
	xnetstream* stream=xrtAtomicPtrLoad(&pDial->Stream,XMEMORY_ACQUIRE);
	if(stream&&!visit(xrtNetStreamOwnership(stream),context))return false;
	if(pDial->PreparedEvents&&!visit(xrtNetProxyDialOwnership(pDial),context))return false;
	if(pDial->DonePolicy&&!visit((xrtownershipref){pDial->DoneData,pDial->DonePolicy->Ops},context))return false;
	if(!__xrtNetProxyDialHandshakeWalk(pDial,visit,context))return false;
	xrtownershipref error=xrtErrorOwnership(pDial->Error);
	return !error.Data||visit(error,context);
}
XRT_API xrtownershipref xrtNetProxyDialOwnership(const xnetproxydial* pDial)
{return (xrtownershipref){pDial,pDial?&__xrtNetProxyDialOps:NULL};}
XRT_API const xrtownershipquiescencev1* xrtNetProxyDialOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy={sizeof(Policy),&__xrtNetProxyDialOps,__xrtNetProxyDialBusy};return &Policy;
}
static void __xrtNetProxyDialRuntimeRelease(ptr pData)
{xrtNetProxyDialDestroy(pData);}
XRT_API const xnetruntimeownershipv1* xrtNetProxyDialRuntimeOwnershipV1Get(void)
{
	static const xnettimerproc Timers[]={__xrtNetProxyDialTimer};
	static const xnetruntimeownershipv1 Policy={sizeof(Policy),&__xrtNetProxyDialOps,__xrtNetProxyDialRuntimeRelease,Timers,1};return &Policy;
}
static bool __xrtNetProxyDialHold(const void* pData)
{return xrtNetProxyDialRef((xnetproxydial*)pData)!=NULL;}
static void __xrtNetProxyDialDrop(const void* pData)
{xrtNetProxyDialDestroy((xnetproxydial*)pData);}
static bool __xrtNetProxyDialClaim(const void* pData,const void* pToken)
{
	xnetproxydial* pDial=(xnetproxydial*)pData;size_t count;
	ptr old=xrtAtomicPtrLoad(&pDial->OwnershipClaim,XMEMORY_ACQUIRE);
	if(!pToken || (old&&old!=pToken) || !__xrtNetProxyDialCount(pDial,&count))return false;
	xrtAtomicPtrStore(&pDial->OwnershipClaim,(ptr)pToken,XMEMORY_RELEASE);return true;
}
static void __xrtNetProxyDialRestore(const void* pData,const void* pToken)
{
	xnetproxydial* pDial=(xnetproxydial*)pData;
	if(!pToken || xrtAtomicPtrLoad(&pDial->OwnershipClaim,XMEMORY_ACQUIRE)!=pToken ||
		(xrtAtomic32Load(&pDial->OwnershipState,XMEMORY_ACQUIRE)&XRT_PROXY_OWNERSHIP_CLEARED))abort();
	xrtAtomicPtrStore(&pDial->OwnershipClaim,NULL,XMEMORY_RELEASE);
}
static void __xrtNetProxyDialOwnershipStopTask(xnetworker* pWorker,ptr pData)
{
	(void)pWorker;xnetproxydial* pDial=pData;__xrtNetProxyDialStopWorker(pDial,false);
	xrtownershipscope Mutation={0};if(!xrtOwnershipMutationBegin(&Mutation))abort();
	xrtAtomic32Store(&pDial->OwnershipStop,0,XMEMORY_RELEASE);
	if(!xrtOwnershipScopeEnd(&Mutation))abort();
	__xrtNetProxyDialTryPublish(pDial); /* last access: borrows real runtime */
}
static xrtownershipprepareresult __xrtNetProxyDialPrepare(const void* pData,const void* pToken)
{
	xnetproxydial* pDial=(xnetproxydial*)pData;xrtownershipscope Freeze={0};
	if(!xrtOwnershipFreezeTryBegin(&Freeze))return XRT_OWNERSHIP_PREPARE_BUSY;
	if(!pToken || xrtAtomicPtrLoad(&pDial->OwnershipClaim,XMEMORY_ACQUIRE)!=pToken)abort();
	xrtownershipprepareresult result=XRT_OWNERSHIP_PREPARE_BUSY;
	if(__xrtNetProxyDialPrepared(pDial))result=XRT_OWNERSHIP_PREPARE_READY;
	else if(!__xrtNetProxyDialBusy(pDial)&&!xrtAtomic32Load(&pDial->OwnershipStop,XMEMORY_ACQUIRE)){
		xrtAtomic32Store(&pDial->OwnershipStop,1,XMEMORY_RELEASE);
		if(!__xrtNetEnginePostInternal(pDial->Worker,&pDial->OwnershipCommand,__xrtNetProxyDialOwnershipStopTask,pDial)){
			xrtAtomic32Store(&pDial->OwnershipStop,0,XMEMORY_RELEASE);result=XRT_OWNERSHIP_PREPARE_FAILED;
		}
	}
	if(!xrtOwnershipScopeEnd(&Freeze))abort();
	return result;
}
static void __xrtNetProxyDialClear(const void* pData,const void* pToken)
{
	xnetproxydial* pDial=(xnetproxydial*)pData;
	if(!pToken || xrtAtomicPtrLoad(&pDial->OwnershipClaim,XMEMORY_ACQUIRE)!=pToken || !__xrtNetProxyDialPrepared(pDial) ||
		(xrtAtomic32Load(&pDial->OwnershipState,XMEMORY_ACQUIRE)&XRT_PROXY_OWNERSHIP_CLEARED))abort();
	xrtAtomic32Store(&pDial->OwnershipState,XRT_PROXY_OWNERSHIP_READY|XRT_PROXY_OWNERSHIP_CLEARED,XMEMORY_RELEASE);
}
XRT_API const xrtownershipadapterv1* xrtNetProxyDialOwnershipAdapterV1(xrtownershipref ref,
	const xnetproxydialownershipv1* const* policies,size_t count,const xrtownershippreparationv1** prep)
{
	static const xrtownershipadapterv1 Adapter={sizeof(Adapter),__xrtNetProxyDialHold,__xrtNetProxyDialDrop,
		__xrtNetProxyDialClaim,__xrtNetProxyDialRestore,NULL,__xrtNetProxyDialClear,NULL};
	static const xrtownershippreparationv1 Preparation={sizeof(Preparation),&Adapter,__xrtNetProxyDialPrepared,__xrtNetProxyDialPrepare};
	if(!prep || ref.Ops!=&__xrtNetProxyDialOps || !ref.Data || (count&&!policies) || __xrtNetProxyDialBusy(ref.Data))return NULL;
	const xnetproxydial* pDial=ref.Data;
	if(pDial->DonePolicy){
		bool known=false;for(size_t i=0;i<count;++i)if(policies[i]==pDial->DonePolicy){known=true;break;}
		if(!known)return NULL;
		const xnetproxydialownershipv1* p=pDial->DonePolicy;
		if(p->size!=sizeof(*p)||p->Done!=pDial->Done||!p->Drop||!p->Ops||!p->Ops->Count||!p->Ops->Trace)return NULL;
	}
	size_t refs;if(!__xrtNetProxyDialCount(ref.Data,&refs))return NULL;
	*prep=&Preparation;return &Adapter;
}

#endif
