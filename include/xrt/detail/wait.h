#ifndef XRT_DETAIL_XRT_WAIT_H
#define XRT_DETAIL_XRT_WAIT_H
#include <xrt.h>
#include <math.h>

#if defined(XRT_FEATURE_WAIT)
/* Private Timer arithmetic. Public waits receive signed milliseconds only. */
static inline void __xrtWaitInvalid(void)
{
    xerrordesc Desc = {0}; xerror* Error;
    Desc.Kind = XERR_ARGUMENT; Desc.Domain = "xrt.wait"; Desc.Code = 1;
    Desc.Operation = "timeout"; Desc.Message = "invalid millisecond timeout or timer value";
    Error = xrtErrorBuild(&Desc);
    if ( Error != NULL ) { xrtSetErrorTake(Error); }
}
static inline double __xrtWaitAfter(int64 Milliseconds)
{
    double Now, Seconds, Result;
    if ( Milliseconds == XRT_WAIT_FOREVER ) { return INFINITY; }
    if ( Milliseconds < 0 ) { __xrtWaitInvalid(); return NAN; }
    Now = xrtTimer();
    if ( !isfinite(Now) ) { return NAN; }
    if ( Milliseconds == 0 ) { return Now; }
    Seconds = (double)(Milliseconds / 1000) + (double)(Milliseconds % 1000) * 0.001;
    /* Round outward, never turning a positive interval into immediate expiry. */
    Result = nextafter(Now + Seconds, INFINITY);
    return Result;
}
static inline bool __xrtWaitValid(double Limit)
{
    if ( isnan(Limit) || Limit < 0 ) { __xrtWaitInvalid(); return false; }
    return true;
}
static inline bool __xrtWaitExpired(double Limit)
{
    double Now;
    if ( Limit == INFINITY ) { return false; }
    if ( !__xrtWaitValid(Limit) ) { return true; }
    Now = xrtTimer();
    return !isfinite(Now) || Now >= Limit;
}
static inline int64 __xrtWaitRemaining(double Limit)
{
    double Remaining;
    if ( Limit == INFINITY ) { return XRT_WAIT_FOREVER; }
    if ( !__xrtWaitValid(Limit) ) { return -2; }
    Remaining = Limit - xrtTimer();
    if ( !isfinite(Remaining) ) { return -2; }
    if ( Remaining <= 0 ) { return 0; }
    Remaining = ceil(Remaining * 1000.0);
    return Remaining >= 0x1p63 ? INT64_MAX : (int64)Remaining;
}
#endif

XRT_EXTERN_C_BEGIN
#if (defined(XRT_FEATURE_CHANNEL))
XRT_API xwaitresult __xrtChannelSendUntil(
	xchannel* pChannel,
	ptr pItem,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL))
XRT_API xwaitresult __xrtChannelRecvUntil(
	xchannel* pChannel,
	ptr* pItem,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_CANCEL))
XRT_API xwaitresult __xrtChannelSendUntilCancel(
	xchannel* pChannel,
	ptr pItem,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_CANCEL))
XRT_API xwaitresult __xrtChannelRecvUntilCancel(
	xchannel* pChannel,
	ptr* pItem,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_SELECT))
XRT_API xchannelselectresult __xrtChannelSelectUntil(
	const xchannelcase* pCases,
	size_t iCount,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL))
XRT_API xchannelselectresult __xrtChannelSelectUntilCancel(
	const xchannelcase* pCases,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_COROUTINE))
XRT_API xwaitresult __xrtChannelSendAwaitUntil(
	xchannel* pChannel,
	ptr pItem,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_COROUTINE))
XRT_API xwaitresult __xrtChannelRecvAwaitUntil(
	xchannel* pChannel,
	ptr* pItem,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_CHANNEL)) && (defined(XRT_FEATURE_CHANNEL_COROUTINE))
XRT_API xchannelselectresult __xrtChannelSelectAwaitUntil(
	const xchannelcase* pCases,
	size_t iCount,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_COROUTINE_SCHEDULER))
XRT_API xwaitresult __xrtCoSchedPollUntil(xcosched* pSched, double iDeadline);
#endif
#if (defined(XRT_FEATURE_COROUTINE_SCHEDULER))
XRT_API xwaitresult __xrtCoParkUntil(double iDeadline);
#endif
#if (defined(XRT_FEATURE_COROUTINE_SCHEDULER))
XRT_API xwaitresult __xrtCoSleepUntil(double iDeadline);
#endif
#if (defined(XRT_FEATURE_COROUTINE_SCHEDULER))
XRT_API xwaitresult __xrtCoJoinUntil(xcoro* pCo, double iDeadline);
#endif
#if (defined(XRT_FEATURE_COROUTINE_EVENT))
XRT_API xwaitresult __xrtCoEventAwaitUntil(
	xcoevent* pEvent,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_EXECUTOR))
XRT_API xwaitresult __xrtExecutorWaitUntil(
	xexecutor* pExecutor,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_FUTURE))
XRT_API xwaitresult __xrtFutureWaitUntil(xfuture* pFuture, double iDeadline);
#endif
#if (defined(XRT_FEATURE_FUTURE))
XRT_API xwaitresult __xrtFutureWaitUntilCancel(
	xfuture* pFuture,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_FUTURE_COROUTINE))
XRT_API xwaitresult __xrtFutureAwaitUntil(xfuture* pFuture, double iDeadline);
#endif
#if (defined(XRT_FEATURE_NET_PORT))
XRT_API xnetresult __xrtNetPortWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	double iDeadline, size_t* pCount);
#endif
#if (defined(XRT_FEATURE_NET_ENGINE))
XRT_API uint64 __xrtNetEngineScheduleOwnedV1(xnetengine* pEngine, uint64 iAffinity,
	double iDeadline, ptr pData, const xnettimerownershipv1* pPolicy);
#endif
#if (defined(XRT_FEATURE_NET_ENGINE))
XRT_API uint64 __xrtNetEngineSchedule(xnetengine* pEngine,
	uint64 iAffinity, double iDeadline,
	xnettimerproc pProc, ptr pData);
#endif
#if (defined(XRT_FEATURE_PROCESS))
XRT_API xwaitresult __xrtProcessWaitUntil(
	xprocess* pProcess,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_PROCESS_RUN))
XRT_API xwaitresult __xrtProcessWaitUntilCancel(
	xprocess* pProcess,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_COND))
XRT_API xwaitresult __xrtCondWaitUntil(
	xcond* pCond,
	xmutex* pMutex,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_SEM))
XRT_API xwaitresult __xrtSemWaitUntil(xsem* pSem, double iDeadline);
#endif
#if (defined(XRT_FEATURE_EVENT))
XRT_API xwaitresult __xrtEventWaitUntil(xevent* pEvent, double iDeadline);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP))
XRT_API xwaitresult __xrtTaskGroupWaitUntil(
	xtaskgroup* pGroup,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP))
XRT_API xwaitresult __xrtTaskGroupWaitUntilCancel(
	xtaskgroup* pGroup,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_TASK_POOL))
XRT_API xfuture* __xrtTaskSubmitUntil(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_TASK_POOL))
XRT_API xfuture* __xrtTaskSubmitUntilCancel(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_TASK_POOL))
XRT_API xwaitresult __xrtTaskPoolWaitUntil(xtaskpool* pPool, double iDeadline);
#endif
#if (defined(XRT_FEATURE_TASK_POOL))
XRT_API xwaitresult __xrtTaskPoolWaitUntilCancel(
	xtaskpool* pPool,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP_POOL))
XRT_API xfuture* __xrtTaskGroupSubmitUntil(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP_POOL))
XRT_API xfuture* __xrtTaskGroupSubmitUntilCancel(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_TASK_NET))
XRT_API xfuture* __xrtTaskNetUntil(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_TASK_GROUP_NET))
XRT_API xfuture* __xrtTaskGroupNetUntil(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_SYNC))
XRT_API bool __xrtNetStreamWait(
	xnetstream* pStream,
	xnetstreamwait Wait,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_SYNC))
XRT_API bool __xrtNetStreamWaitAvailable(
	xnetstream* pStream,
	size_t iMinimum,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_SYNC))
XRT_API xnetstream* __xrtNetListenerAcceptWait(
	xnetlistener* pListener,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_SYNC))
XRT_API xnetbytes* __xrtNetStreamRecv(
	xnetstream* pStream,
	size_t iMaxBytes,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP)) && (defined(XRT_FEATURE_NET_TCP_DIAL_SYNC))
XRT_API xnetstream* __xrtNetConnect(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_TCP_SERVER)) && (defined(XRT_FEATURE_NET_TCP_SERVER_SYNC))
XRT_API xnetstream* __xrtNetServerAcceptWait(
	xnetserver* pServer,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_THREAD))
XRT_API xwaitresult __xrtThreadWaitUntil(xthread* pThread, double iDeadline);
#endif
#if (defined(XRT_FEATURE_TLS_STREAM)) && (defined(XRT_FEATURE_TLS_STREAM_LISTENER_SYNC))
XRT_API xtlsstream* __xrtTlsListenerAcceptWait(
	xtlslistener* pListener,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API bool __xrtNetUdpWait(
	xnetudp* pUdp,
	xnetudpwait Wait,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API bool __xrtNetUdpWritable(
	xnetudp* pUdp,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API xnetudppacket* __xrtNetUdpReceiveWait(
	xnetudp* pUdp,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API xnetudperrorpacket* __xrtNetUdpReceiveErrorWait(
	xnetudp* pUdp,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XRT_FEATURE_NET_UDP)) && (defined(XRT_FEATURE_NET_UDP_SYNC))
XRT_API xnetudpbatch* __xrtNetUdpReceiveBatchWait(
	xnetudp* pUdp,
	size_t iCapacity,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendBegin(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendEnd(
	ximapclient* pClient,
	ximapappendresult* pResult,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppend(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	const void* pData,
	ximapappendresult* pResult,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_AUTH))
XRT_API bool __xrtImapClientAuth(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API ximapclient* __xrtImapClientOpen(
	const ximapclientconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientSend(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientSendParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientContinue(
	ximapclient* pClient,
	xstrview Data,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientReceive(
	ximapclient* pClient,
	ximapevent* pEvent,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientReadLiteral(
	ximapclient* pClient,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientBegin(
	ximapclient* pClient,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientBeginParts(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API xmailnext __xrtImapClientNext(
	ximapclient* pClient,
	ximapevent* pEvent,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientRefresh(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientLogout(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientClose(
	ximapclient* pClient,
	double iDeadline
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientNoop(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientExamine(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCheck(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientUnselect(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCloseMailbox(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCreateMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientDeleteMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientRenameMailbox(
	ximapclient* pClient,
	xstrview Source,
	xstrview Target,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientSubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientUnsubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginList(
	ximapclient* pClient,
	xstrview Reference,
	xstrview Pattern,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginStatus(
	ximapclient* pClient,
	xstrview Mailbox,
	xstrview Items,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginSearch(
	ximapclient* pClient,
	xstrview Criteria,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginFetch(
	ximapclient* pClient,
	xstrview Set,
	xstrview Items,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginStore(
	ximapclient* pClient,
	xstrview Set,
	ximapstoremode Mode,
	xstrview Flags,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginCopy(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginExpunge(
	ximapclient* pClient,
	xstrview UidSet,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginIdle(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientEndIdle(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMPRESS))
XRT_API bool __xrtImapClientCompress(
	ximapclient* pClient,
	const ximapcompressconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bool __xrtImapClientBodyWrite(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bytes __xrtImapClientBodyBytes(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	size_t* pOutputSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bool __xrtImapClientMessageTree(
	ximapclient* pClient,
	uint32 iMessage,
	bool bUid,
	bool bPeek,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_AUTH))
XRT_API bool __xrtPop3ClientAuth(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_AUTH))
XRT_API bool __xrtPop3ClientLogin(
	xpop3client* pClient,
	xstrview Username,
	xstrview Password,
	bool AllowPlaintext,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API xpop3client* __xrtPop3ClientOpen(
	const xpop3clientconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientSend(
	xpop3client* pClient,
	xstrview Line,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientAuthLine(
	xpop3client* pClient,
	xstrview Line,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientLine(
	xpop3client* pClient,
	xstrview* pLine,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientReceive(
	xpop3client* pClient,
	xpop3reply* pReply,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientCommand(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xpop3reply* pReply,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientBegin(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API xmailnext __xrtPop3ClientNext(
	xpop3client* pClient,
	xstrview* pLine,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientStat(
	xpop3client* pClient,
	xpop3stat* pStat,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientList(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3listview* pItem,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientListAll(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientUidl(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3uidlview* pItem,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientUidlAll(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientRetr(
	xpop3client* pClient,
	uint64 iMessage,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientTop(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientDelete(
	xpop3client* pClient,
	uint64 iMessage,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientReset(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientNoop(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientQuit(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientClose(
	xpop3client* pClient,
	double iDeadline
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bool __xrtPop3ClientRetrWrite(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bool __xrtPop3ClientTopWrite(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bytes __xrtPop3ClientRetrBytes(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	size_t* pOutputSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bytes __xrtPop3ClientTopBytes(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	size_t* pOutputSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bool __xrtPop3ClientRetrTree(
	xpop3client* pClient,
	uint64 iMessage,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_AUTH))
XRT_API bool __xrtSmtpClientAuth(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API xsmtpclient* __xrtSmtpClientOpen(
	const xsmtpclientconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientSend(
	xsmtpclient* pClient,
	xstrview Line,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientAuthLine(
	xsmtpclient* pClient,
	xstrview Line,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientReceive(
	xsmtpclient* pClient,
	xsmtpreply* pReply,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientCommand(
	xsmtpclient* pClient,
	xstrview Verb,
	xstrview Arguments,
	xsmtpreply* pReply,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientMail(
	xsmtpclient* pClient,
	xstrview ReversePath,
	xstrview Parameters,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientRcpt(
	xsmtpclient* pClient,
	xstrview ForwardPath,
	xstrview Parameters,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientDataBegin(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientDataWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientDataEnd(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientData(
	xsmtpclient* pClient,
	xstrview Message,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientBdatBegin(
	xsmtpclient* pClient,
	size_t iChunkSize,
	bool Last,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientBdatWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientBdatEnd(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientBdat(
	xsmtpclient* pClient,
	xbytesview Data,
	bool Last,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientReset(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientNoop(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientQuit(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientClose(
	xsmtpclient* pClient,
	double iDeadline
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_SUBMIT))
XRT_API bool __xrtSmtpSubmitEnvelope(
	xsmtpclient* pClient,
	const xsmtpenvelope* pEnvelope,
	const xmailmessage* pMessage,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_SUBMIT))
XRT_API bool __xrtSmtpSubmit(
	xsmtpclient* pClient,
	const xmailmessage* pMessage,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE))
XRT_API xwaitresult __xrtWsGroupOpWaitUntil(
	xwsgroupop* pOperation,
	double iDeadline
);
#endif
#if (defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE))
XRT_API xwaitresult __xrtWsGroupOpWaitUntilCancel(
	xwsgroupop* pOperation,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
