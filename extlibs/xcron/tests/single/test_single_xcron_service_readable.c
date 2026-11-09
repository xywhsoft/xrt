#define XCRON_MODULE_XCRON_SERVICE
#define XRT_MODULE_MEMORY_STATS
#include <xcron/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static xatomic64 ReadableTarget;
static xatomic32 ReadableBroadcasts,ReadableFailBroadcast,ReadableFailWait;
static bool readableBroadcast(xcond* C)
{
    if((uintptr_t)C==(uintptr_t)xrtAtomic64Load(&ReadableTarget,XMEMORY_ACQUIRE)){
        xrtAtomic32FetchAdd(&ReadableBroadcasts,1,XMEMORY_RELAXED);
        if(xrtAtomic32Exchange(&ReadableFailBroadcast,0,XMEMORY_ACQ_REL)){xrtSetErrorInfo(XERR_IO,"readable.inject",71,"notification failed");return false;}
    }
    return xrtCondBroadcast(C);
}
static xwaitresult readableWait(xcond* C,xmutex* M,int64 Timeout)
{
    if((uintptr_t)C==(uintptr_t)xrtAtomic64Load(&ReadableTarget,XMEMORY_ACQUIRE) && xrtAtomic32Exchange(&ReadableFailWait,0,XMEMORY_ACQ_REL)){xrtSetErrorInfo(XERR_IO,"readable.inject",72,"wait failed with mutex retained");return XWAIT_ERROR;}
    return xrtCondWaitFor(C,M,Timeout);
}
#define xrtCondBroadcast readableBroadcast
#define xrtCondWaitFor readableWait
#define XCRON_IMPLEMENTATION
#include "../../../../single/extlibs/xcron.h"
#undef xrtCondBroadcast
#undef xrtCondWaitFor
#include "../test.h"
#include "../service_readable_cases.h"
static uint32 readableWaiting(xcronservice* S)
{testRequire(xrtMutexLock(&S->Mutex),"inspect private readable wait registration");uint32 N=S->ReadWaiters;testRequire(xrtMutexUnlock(&S->Mutex),"release readable registration lock");return N;}
static void readableWaitCount(xcronservice* S,uint32 N)
{double End=xrtTimer()+10;while(readableWaiting(S)!=N){testRequire(xrtTimer()<End,"actual readable waiters registered");xrtThreadYield();}}
static void testReadablePublication(void)
{
    for(unsigned Failure=0;Failure<2;++Failure){rwfixture F=rwFixture(false);xexecutor* E=rwStart(&F);xrtAtomic32Store(&F.Gate,0,XMEMORY_RELEASE);rwAdd(&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+1000);rwUntil(&F.Started,1,"publication body ready");xrtAtomic64Store(&ReadableTarget,(uintptr_t)&F.Service->Changed,XMEMORY_RELEASE);xrtAtomic32Store(&ReadableBroadcasts,0,XMEMORY_RELEASE);rwwaiter W[2]={{.F=&F,.Timeout=10000},{.F=&F,.Timeout=10000}};xthread* T[2]={rwSpawn(&W[0]),rwSpawn(&W[1])};readableWaitCount(F.Service,2);
        if(Failure)xrtAtomic32Store(&ReadableFailBroadcast,1,XMEMORY_RELEASE);
        xrtAtomic32Store(&F.Gate,1,XMEMORY_RELEASE);for(unsigned N=0;N<2;++N){rwJoin(T[N]);testRequire(W[N].Result==XWAIT_OK && W[N].Ambient,"published metadata wins even after notification failure");}testRequire(!readableWaiting(F.Service),"all condition registrations physically returned");
        if(Failure){testRequire(xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK && rwStats(&F).Failure.Code==XCRON_SERVICE_SYSTEM,"notification failure becomes sticky controlled service failure");xcronservicecompletion D;testRequire(rwDrain(&F,&D)==1 && !D.Error,"failed notification cannot roll back published physical result");xcronserviceissue I;testRequire(xcronServiceWaitReadable(F.Service,0,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_SYSTEM,"stopped empty reports notification failure");xrtClearError();}
        else {testRequire(xrtAtomic32Load(&ReadableBroadcasts,XMEMORY_ACQUIRE)==1,"one empty-to-readable broadcast wakes both actual waiters");xrtAtomic64Store(&F.Wall,(uint64)(XRT_TIME_UNIX_EPOCH+1000),XMEMORY_RELEASE);xrtAtomic64Store(&F.Mono,1100,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);double End=xrtTimer()+10;while(rwStats(&F).ReadyCompletions!=2){testRequire(xrtTimer()<End,"second physical record appended");xrtThreadYield();}testRequire(xrtAtomic32Load(&ReadableBroadcasts,XMEMORY_ACQUIRE)==1,"nonempty queue publication with no readers emits no broadcast");}
        xrtAtomic64Store(&ReadableTarget,0,XMEMORY_RELEASE);rwClear(&F,E);
    }
}
static void testReadableFailuresAndSpurious(void)
{
    rwfixture F=rwFixture(false);xexecutor* E=rwStart(&F);xrtAtomic64Store(&ReadableTarget,(uintptr_t)&F.Service->Changed,XMEMORY_RELEASE);xcronserviceissue I;testRequire(xrtMutexLock(&F.Service->Mutex),"set bounded waiter limit");F.Service->ReadWaiters=UINT32_MAX;xrtMutexUnlock(&F.Service->Mutex);testRequire(xcronServiceWaitReadable(F.Service,10,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_LIMIT && readableWaiting(F.Service)==UINT32_MAX,"waiter saturation rejects before wrap");xrtClearError();xrtMutexLock(&F.Service->Mutex);F.Service->ReadWaiters=0;xrtMutexUnlock(&F.Service->Mutex);
    xrtAtomic32Store(&ReadableFailWait,1,XMEMORY_RELEASE);testRequire(xcronServiceWaitReadable(F.Service,10,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_SYSTEM && !readableWaiting(F.Service),"native wait failure releases registration while retaining all duties");xrtClearError();
    xallocationbudget* B=NULL;testRequire(xrtAllocationBudgetCreate(1,&B),"readable diagnostic OOM budget");xallocationscope Scope={0};testRequire(xrtAllocationBudgetEnter(B,&Scope),"enter readable diagnostic OOM");xrtMutexLock(&F.Service->Mutex);F.Service->ReadWaiters=UINT32_MAX;xrtMutexUnlock(&F.Service->Mutex);testRequire(xcronServiceWaitReadable(F.Service,10,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_LIMIT,"typed readable failure survives diagnostic OOM");xrtAllocationBudgetLeave(&Scope);xrtMutexLock(&F.Service->Mutex);F.Service->ReadWaiters=0;xrtMutexUnlock(&F.Service->Mutex);xrtClearError();xrtAllocationBudgetFree(B);
    rwwaiter W={.F=&F,.Timeout=60};xthread* T=rwSpawn(&W);double End=xrtTimer()+2;while(!xrtAtomic32Load(&W.Done,XMEMORY_ACQUIRE)){xrtMutexLock(&F.Service->Mutex);xrtCondBroadcast(&F.Service->Changed);xrtMutexUnlock(&F.Service->Mutex);testRequire(xrtTimer()<End,"spurious wake total timeout");xrtThreadYield();}rwJoin(T);testRequire(W.Result==XWAIT_TIMEOUT && W.Elapsed<1000 && W.Ambient && !readableWaiting(F.Service),"actual spurious wakeups cannot reset timeout or leak waiter count");xrtAtomic64Store(&ReadableTarget,0,XMEMORY_RELEASE);rwClear(&F,E);
}
static void testReadableMissingDiagnostic(void)
{
    rwfixture F=rwFixture(false);xexecutor* E=rwStart(&F);xrtAtomic32Store(&F.FailClock,1,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);testRequire(xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK && rwStats(&F).Failure.Code==XCRON_SERVICE_CLOCK,"stop owner before diagnostic-OOM handoff");xrtMutexLock(&F.Service->Mutex);xerror* Error=F.Service->FatalError;F.Service->FatalError=NULL;F.Service->Stats.Failure.Cause=XERR_MEMORY;xrtMutexUnlock(&F.Service->Mutex);xrtErrorFree(Error);xrtSetErrorInfo(XERR_IO,"readable.unrelated",93,"unrelated caller error");xcronserviceissue I;testRequire(xcronServiceWaitReadable(F.Service,0,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_CLOCK && I.Cause==XERR_MEMORY && !xrtGetError(),"typed fatal handoff survives missing diagnostic without attributing caller error");rwClear(&F,E);
}
int main(void)
{testServiceReadable(false);testReadablePublication();testReadableFailuresAndSpurious();testReadableMissingDiagnostic();puts("[PASS] readable internals: real registration barrier/coalescing, notification and wait failures, saturation/OOM, spurious total timeout and missing fatal diagnostic");return 0;}
