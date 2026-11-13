#include "../internal/xrt_tls_stream.h"
#include <xrt/detail/wait.h>
#if defined(XRT_FEATURE_TLS_STREAM_RETIRE)
static bool __xrtTlsStreamTerminal(const xtlsstream* S)
{xtlsstreamstate State=xrtTlsStreamState(S);return State==XTLS_STREAM_CLOSED || State==XTLS_STREAM_FAILED;}
XRT_API xwaitresult xrtTlsStreamPollRetired(xtlsstream* S)
{
    if(!S){__xrtErrorSetInvalidArgument();return XWAIT_ERROR;}
    xmutextryresult Lock=xrtMutexTryLockResult(&S->RetirementLock);
    if(Lock!=XMUTEX_TRY_OK)return Lock==XMUTEX_TRY_BUSY?XWAIT_TIMEOUT:XWAIT_ERROR;
    bool Ready=__xrtTlsStreamTerminal(S) && __xrtAtomicRefLoad(&S->References)==1;
    if(!xrtMutexUnlock(&S->RetirementLock))return XWAIT_ERROR;
    if(!Ready)return XWAIT_TIMEOUT;
    xnetstream* TCP=xrtTlsStreamTransport(S);return TCP?xrtNetStreamPollRetired(TCP):XWAIT_OK;
}
XRT_API xwaitresult xrtTlsStreamWaitRetired(xtlsstream* S,int64 Timeout)
{
    if(!S || Timeout<XRT_WAIT_FOREVER){__xrtErrorSetInvalidArgument();return XWAIT_ERROR;}
    xnetstream* TCP=xrtTlsStreamTransport(S);xnetworker* Current=__xrtNetEngineCurrentThread();
    if(Current && Current->Engine==(TCP?TCP->Engine:S->OwnershipEngine)){__xrtErrorSetInvalidState();return XWAIT_ERROR;}
    if(!Timeout)return xrtTlsStreamPollRetired(S);
    double Deadline=__xrtWaitAfter(Timeout);if(!__xrtWaitValid(Deadline))return XWAIT_ERROR;
    if(!xrtMutexLock(&S->RetirementLock))return XWAIT_ERROR;
    xwaitresult Result=XWAIT_OK;
    while(!__xrtTlsStreamTerminal(S) || __xrtAtomicRefLoad(&S->References)!=1){
        if(__xrtWaitExpired(Deadline)){Result=XWAIT_TIMEOUT;break;}
        Result=xrtCondWaitFor(&S->RetirementReady,&S->RetirementLock,__xrtWaitRemaining(Deadline));
        if(Result==XWAIT_ERROR)break;
    }
    if(Result!=XWAIT_ERROR && __xrtTlsStreamTerminal(S) && __xrtAtomicRefLoad(&S->References)==1)Result=XWAIT_OK;
    if(!xrtMutexUnlock(&S->RetirementLock))return XWAIT_ERROR;
    if(Result!=XWAIT_OK)return Result;
    return TCP?xrtNetStreamWaitRetired(TCP,__xrtWaitRemaining(Deadline)):XWAIT_OK;
}
#endif
