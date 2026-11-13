#include "../internal/xrt_websocket_stream.h"
#include "../internal/xrt_tcp.h"
#include <xrt/detail/wait.h>
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_RETIRE)
XRT_API xwaitresult xrtWsStreamPollRetired(xwsstream* S)
{
    if(!S || !__xrtWsStreamRangeValid(S)){__xrtErrorSetInvalidArgument();return XWAIT_ERROR;}
    xmutextryresult Lock=xrtMutexTryLockResult(&S->RetirementLock);
    if(Lock!=XMUTEX_TRY_OK)return Lock==XMUTEX_TRY_BUSY?XWAIT_TIMEOUT:XWAIT_ERROR;
    bool Ready=xrtWsStreamState(S)==XWS_STREAM_CLOSED && __xrtAtomicRefLoad(&S->References)==1;
    xnetstream* TCP=S->RetirementTcp;
    if(!xrtMutexUnlock(&S->RetirementLock))return XWAIT_ERROR;
    if(!Ready)return XWAIT_TIMEOUT;
    if(!TCP){__xrtErrorSetInvalidState();return XWAIT_ERROR;}
    return xrtNetStreamPollRetired(TCP);
}
XRT_API xwaitresult xrtWsStreamWaitRetired(xwsstream* S,int64 Timeout)
{
    if(!S || !__xrtWsStreamRangeValid(S) || Timeout<XRT_WAIT_FOREVER){__xrtErrorSetInvalidArgument();return XWAIT_ERROR;}
    xnetstream* TCP=S->RetirementTcp;if(!TCP){__xrtErrorSetInvalidState();return XWAIT_ERROR;}
    xnetworker* Current=__xrtNetEngineCurrentThread();if(Current && Current->Engine==TCP->Engine){__xrtErrorSetInvalidState();return XWAIT_ERROR;}
    if(!Timeout)return xrtWsStreamPollRetired(S);
    double Deadline=__xrtWaitAfter(Timeout);if(!__xrtWaitValid(Deadline))return XWAIT_ERROR;
    if(!xrtMutexLock(&S->RetirementLock))return XWAIT_ERROR;
    xwaitresult Result=XWAIT_OK;
    while(xrtWsStreamState(S)!=XWS_STREAM_CLOSED || __xrtAtomicRefLoad(&S->References)!=1){
        if(__xrtWaitExpired(Deadline)){Result=XWAIT_TIMEOUT;break;}
        Result=xrtCondWaitFor(&S->RetirementReady,&S->RetirementLock,__xrtWaitRemaining(Deadline));if(Result==XWAIT_ERROR)break;
    }
    if(Result!=XWAIT_ERROR && xrtWsStreamState(S)==XWS_STREAM_CLOSED && __xrtAtomicRefLoad(&S->References)==1)Result=XWAIT_OK;
    if(!xrtMutexUnlock(&S->RetirementLock))return XWAIT_ERROR;
    return Result==XWAIT_OK?xrtNetStreamWaitRetired(TCP,__xrtWaitRemaining(Deadline)):Result;
}
#endif
