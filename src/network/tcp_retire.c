#include "../internal/xrt_tcp.h"
#include <xrt/detail/wait.h>

#if defined(XRT_FEATURE_NET_TCP_RETIRE)
#if !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_COND)
#error "TCP retirement waiting requires TCP and condition variables"
#endif

XRT_API xwaitresult xrtNetStreamWaitRetired(xnetstream* Stream,int64 Timeout)
{
    if (!Stream || Timeout<XRT_WAIT_FOREVER) {
        __xrtErrorSetInvalidArgument();return XWAIT_ERROR;
    }
    /* Inspect only this live thread's certified worker context. A Stream can
     * outlive its Engine, including while real caller aliases remain. */
    xnetworker* Current=__xrtNetEngineCurrentThread();
    if (Current && Current->Engine==Stream->Engine) {
        __xrtErrorSetInvalidState();return XWAIT_ERROR;
    }
    double Deadline=__xrtWaitAfter(Timeout);if(!__xrtWaitValid(Deadline))return XWAIT_ERROR;
    if(!xrtMutexLock(&Stream->RetirementLock))return XWAIT_ERROR;
    xwaitresult Result=XWAIT_OK;
    while(xrtNetStreamState(Stream)!=XNET_STREAM_CLOSED || __xrtAtomicRefLoad(&Stream->References)!=1) {
        if(__xrtWaitExpired(Deadline)){Result=XWAIT_TIMEOUT;break;}
        Result=xrtCondWaitFor(&Stream->RetirementReady,&Stream->RetirementLock,__xrtWaitRemaining(Deadline));
        if(Result!=XWAIT_OK && Result!=XWAIT_TIMEOUT)break;
        /* Recheck even after a timeout/spurious wake under the predicate lock. */
    }
    if(Result!=XWAIT_ERROR && xrtNetStreamState(Stream)==XNET_STREAM_CLOSED && __xrtAtomicRefLoad(&Stream->References)==1)Result=XWAIT_OK;
    if(!xrtMutexUnlock(&Stream->RetirementLock))return XWAIT_ERROR;
    return Result;
}
#endif
