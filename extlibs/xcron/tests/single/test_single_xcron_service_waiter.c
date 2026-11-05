/* Real Core waiters with one-shot failures at the public service boundary. */
#define XCRON_MODULE_XCRON_SERVICE
#define XRT_MODULE_MEMORY_STATS
#include <xcron/features.h>
#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string.h>
static volatile LONG ForceFallback;
static FARPROC WINAPI serviceWaiterLookup(HMODULE Module,LPCSTR Name)
{
    if(InterlockedCompareExchange(&ForceFallback,0,0) && !strcmp(Name,"CreateWaitableTimerExW"))return NULL;
    return GetProcAddress(Module,Name);
}
#define GetProcAddress serviceWaiterLookup
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#if defined(_WIN32) || defined(_WIN64)
#undef GetProcAddress
#endif
static xatomic32 FaultInit,FaultWait,FaultUnit,LiveWaiters;
static xatomic64 InitOwner,UnitOwner;
static bool serviceWaiterInit(xeventwaiter* W,xevent* E,uint32 Flags)
{
    xrtAtomic64Store(&InitOwner,xrtThreadCurrentId(),XMEMORY_RELEASE);
    if(xrtAtomic32Exchange(&FaultInit,0,XMEMORY_ACQ_REL)){
        xrtSetErrorInfo(XERR_IO,"test.waiter",1,"init resource failure");return false;
    }
    bool OK=xrtEventWaiterInit(W,E,Flags);
    if(OK)xrtAtomic32FetchAdd(&LiveWaiters,1,XMEMORY_ACQ_REL);
    return OK;
}
static xwaitresult serviceWaiterWait(xeventwaiter* W,int64 Timeout)
{
    if(xrtAtomic32Exchange(&FaultWait,0,XMEMORY_ACQ_REL)){
        xrtSetErrorInfo(XERR_IO,"test.waiter",2,"native wait failure");return XWAIT_ERROR;
    }
    return xrtEventWaiterWaitFor(W,Timeout);
}
static bool serviceWaiterUnit(xeventwaiter* W)
{
    xrtAtomic64Store(&UnitOwner,xrtThreadCurrentId(),XMEMORY_RELEASE);
    if(xrtAtomic32Exchange(&FaultUnit,0,XMEMORY_ACQ_REL)){
        xrtSetErrorInfo(XERR_IO,"test.waiter",3,"native release failure");return false;
    }
    bool OK=xrtEventWaiterUnit(W);
    if(OK)xrtAtomic32FetchSub(&LiveWaiters,1,XMEMORY_ACQ_REL);
    return OK;
}
#define xrtEventWaiterInit serviceWaiterInit
#define xrtEventWaiterWaitFor serviceWaiterWait
#define xrtEventWaiterUnit serviceWaiterUnit
#define XCRON_IMPLEMENTATION
#include "../../../../single/extlibs/xcron.h"
#undef xrtEventWaiterInit
#undef xrtEventWaiterWaitFor
#undef xrtEventWaiterUnit
#define main serviceOriginalSuite
#include "../test_service.c"
#undef main
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
static void testWaiterFaults(void)
{
    servicefixture F=serviceFixture();xexecutor* E=servicePool(1,8);
    xcronserviceconfig C=serviceConfig(E,&F);xcronserviceissue I;
    xallocationbudget* B=NULL;testRequire(xrtAllocationBudgetCreate(SIZE_MAX,&B),"waiter startup budget");C.InitializationBudget=B;
    xcronservice* Sentinel=(xcronservice*)(uintptr_t)1;
    xrtAtomic32Store(&FaultInit,1,XMEMORY_RELEASE);
    testRequire(!xcronServiceCreate(&C,&Sentinel,&I) && Sentinel==(xcronservice*)(uintptr_t)1 && I.Code==XCRON_SERVICE_SYSTEM && I.Cause==XERR_IO,"late waiter init rolls back adapter/kernel and preserves output");
    xrtClearError();xallocationbudgetinfo BI={.Size=sizeof(BI)};
    testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.LiveCount && !xrtAtomic32Load(&LiveWaiters,XMEMORY_ACQUIRE),"failed owner startup returns every charged allocation");
    xrtAllocationBudgetFree(B);C.InitializationBudget=NULL;
    testRequire(xrtAtomic64Load(&InitOwner,XMEMORY_ACQUIRE)!=xrtThreadCurrentId(),"Init belongs to native owner, not creator");
    for(unsigned Mode=0;Mode<2;++Mode){
        F=serviceFixture();C=serviceConfig(E,&F);
        xrtAtomic32Store(Mode?&FaultUnit:&FaultWait,1,XMEMORY_RELEASE);
        serviceStart(&F,&C);
        if(Mode)testRequire(xcronServiceClose(F.Service,false,NULL),"close before injected release failure");
        testRequire(xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK,"native waiter failure still physically joins owner");
        xcronservicestats T=serviceStats(F.Service);
        testRequire(T.State==XCRON_SERVICE_FAILED && T.Failure.Code==XCRON_SERVICE_SYSTEM && T.Failure.Cause==XERR_IO,"wait/release failure remains observable");
        xerror* Error=NULL;testRequire(xcronServiceFailure(F.Service,&Error,NULL) && Error,"first waiter failure is owned");xrtErrorFree(Error);
        testRequire(T.ActiveNativeTimers==(Mode?T.Waiter.NativeTimers:0) && xrtAtomic32Load(&LiveWaiters,XMEMORY_ACQUIRE)==Mode,"failed release retains resource; successful stop releases before join");
        if(Mode){
            xrtAtomic32Store(&FaultUnit,1,XMEMORY_RELEASE);
            testRequire(!xcronServiceDestroy(F.Service,&I) && I.Code==XCRON_SERVICE_SYSTEM,"failed exclusive cleanup preserves object for retry");xrtClearError();
            testRequire(serviceStats(F.Service).State==XCRON_SERVICE_FAILED,"failed destroy preserves inspectable kernel/object");
        }
        serviceClear(&F);
        testRequire(!xrtAtomic32Load(&LiveWaiters,XMEMORY_ACQUIRE),"physical handoff retries waiter cleanup");
        if(Mode)testRequire(xrtAtomic64Load(&UnitOwner,XMEMORY_ACQUIRE)==xrtThreadCurrentId(),"Unit permits joined exclusive creator handoff");
    }
#if defined(_WIN32) || defined(_WIN64)
    InterlockedExchange(&ForceFallback,1);
    F=serviceFixture();C=serviceConfig(E,&F);serviceStart(&F,&C);
    xcronservicestats Fallback=serviceStats(F.Service);
    testRequire(Fallback.Waiter.Backend==XEVENT_WAITER_WINDOWS_STANDARD && Fallback.ActiveNativeTimers==1,"unsupported precise timer selects an actual observable standard timer");
    serviceStop(&F);serviceClear(&F);InterlockedExchange(&ForceFallback,0);
#endif
    /* Warm once before observing native handle balance. */
    F=serviceFixture();C=serviceConfig(E,&F);serviceStart(&F,&C);serviceStop(&F);serviceClear(&F);
#if defined(_WIN32) || defined(_WIN64)
    DWORD Before=0,After=0;testRequire(GetProcessHandleCount(GetCurrentProcess(),&Before)!=0,"handle baseline");
#endif
    for(unsigned N=0;N<64;++N){F=serviceFixture();C=serviceConfig(E,&F);serviceStart(&F,&C);serviceStop(&F);testRequire(!serviceStats(F.Service).ActiveNativeTimers,"timer released even before result consumption/destroy");serviceClear(&F);}
#if defined(_WIN32) || defined(_WIN64)
    testRequire(GetProcessHandleCount(GetCurrentProcess(),&After)!=0 && After==Before,"64 owner timer lifetimes return handle count to baseline");
#endif
    servicePoolEnd(E);
}
int main(void)
{
    xrtAtomic32Init(&FaultInit,0);xrtAtomic32Init(&FaultWait,0);xrtAtomic32Init(&FaultUnit,0);xrtAtomic32Init(&LiveWaiters,0);xrtAtomic64Init(&InitOwner,0);xrtAtomic64Init(&UnitOwner,0);
    testWaiterFaults();int Result=serviceOriginalSuite();
    testRequire(!xrtAtomic32Load(&LiveWaiters,XMEMORY_ACQUIRE),"all normal suite waiters released");
    puts("[PASS] service precise waiter: startup/wait/release faults, joined retry, backend accounting and 64 handle lifetimes");return Result;
}
