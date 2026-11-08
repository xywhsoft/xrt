#include "../test.h"
#include <xrt/event_waiter.h>
#include <limits.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
#if defined(XRT_FEATURE_THREAD)
typedef struct waiterthread {xevent* Event;xeventwaiter* Waiter;bool Foreign,Unit;int64 Delay;} waiterthread;
static int32 waiterThread(ptr Raw)
{
    waiterthread* T=Raw;if(T->Delay)xrtSleep(T->Delay);
    if(T->Foreign){xeventwaiterinfo Info;testRequire(xrtEventWaiterGet(T->Waiter,&Info),"foreign metadata inspection");testRequire(xrtEventWaiterWaitFor(T->Waiter,0)==XWAIT_ERROR && xrtErrorKind(xrtGetError())==XERR_STATE,"foreign native wait rejected without consuming signal");xrtClearError();}
    if(T->Unit)testRequire(xrtEventWaiterUnit(T->Waiter),"physical handoff allows caller-exclusive unit");
    else testRequire(xrtEventSet(T->Event),"cross-thread signal");
    return 0;
}
static void waiterJoin(xthread* T)
{testRequire(T && xrtThreadWaitFor(T,10000)==XWAIT_OK,"waiter signal thread joined");xrtThreadDestroy(T);}
#endif
static void waiterBasics(void)
{
    for(unsigned Manual=0;Manual<2;++Manual){xevent E;xeventwaiter W;xeventwaiterinfo Info;testRequire(xrtEventInit(&E,Manual!=0,false) && xrtEventWaiterInit(&W,&E,0),"borrowed-event deadline context init");
        testRequire(xrtEventWaiterGet(&W,&Info) && Info.StorageBytes==sizeof(W),"fixed context storage metadata");
#if defined(_WIN32) || defined(_WIN64)
        testRequire(Info.NativeTimers==1 && (Info.Backend==XEVENT_WAITER_WINDOWS_HIGH_RESOLUTION || Info.Backend==XEVENT_WAITER_WINDOWS_STANDARD),"one reusable native timer");
#else
        testRequire(!Info.NativeTimers && (Info.Backend==XEVENT_WAITER_POSIX_MONOTONIC || Info.Backend==XEVENT_WAITER_PLATFORM),"POSIX event condition reused");
#endif
        xrtSetErrorInfo(XERR_IO,"waiter.sentinel",4,"ambient");const xerror* Ambient=xrtGetError();double Start=xrtTimer();
        testRequire(xrtEventWaiterWaitFor(&W,2)==XWAIT_TIMEOUT && xrtTimer()-Start>=0.002 && xrtGetError()==Ambient,"positive timeout never early and preserves ambient");
        testRequire(xrtEventSet(&E) && xrtEventWaiterWaitFor(&W,0)==XWAIT_OK && xrtGetError()==Ambient,"ready wins with zero budget");
        if(Manual)testRequire(xrtEventWaiterWaitFor(&W,0)==XWAIT_OK,"manual signal remains signalled");
        else testRequire(xrtEventWaiterWaitFor(&W,0)==XWAIT_TIMEOUT,"auto signal consumed exactly once");
        testRequire(xrtEventReset(&E) && xrtEventWaiterWaitFor(&W,0)==XWAIT_TIMEOUT,"normal reset remains authoritative");
#if defined(XRT_FEATURE_THREAD)
        waiterthread F={.Event=&E,.Waiter=&W,.Foreign=true};waiterJoin(xrtThreadCreate(waiterThread,&F,0));testRequire(xrtEventWaiterWaitFor(&W,0)==XWAIT_OK,"foreign failure left ready event unconsumed");xrtEventReset(&E);
        F=(waiterthread){.Event=&E,.Waiter=&W,.Delay=2};xthread* T=xrtThreadCreate(waiterThread,&F,0);testRequire(T && xrtEventWaiterWaitFor(&W,XRT_WAIT_FOREVER)==XWAIT_OK,"infinite wait interrupted by ordinary event");waiterJoin(T);xrtEventReset(&E);
        T=xrtThreadCreate(waiterThread,&F,0);Start=xrtTimer();testRequire(T && xrtEventWaiterWaitFor(&W,INT64_MAX)==XWAIT_OK && xrtTimer()-Start<5,"huge finite budget cannot overflow and is interruptible");waiterJoin(T);xrtEventReset(&E);
        /* An event-won timed wait must not leave a stale timer shortening later waits. */
        T=xrtThreadCreate(waiterThread,&F,0);testRequire(T && xrtEventWaiterWaitFor(&W,50)==XWAIT_OK,"event interrupts armed timer");waiterJoin(T);xrtEventReset(&E);
        Start=xrtTimer();testRequire(xrtEventWaiterWaitFor(&W,4)==XWAIT_TIMEOUT && xrtTimer()-Start>=0.004,"rearmed timer never inherits earlier event-won state");
#endif
        testRequire(xrtGetError()==Ambient,"all successful wait paths preserve original error");xrtClearError();testRequire(xrtEventWaiterUnit(&W) && xrtEventSet(&E) && xrtEventWaitFor(&E,0)==XWAIT_OK && xrtEventUnit(&E),"unit consumes no borrowed event resource");
    }
#if defined(XRT_FEATURE_THREAD)
    xevent E;xeventwaiter W;testRequire(xrtEventInit(&E,false,false) && xrtEventWaiterInit(&W,&E,0),"cross-thread physical handoff fixture");waiterthread F={.Event=&E,.Waiter=&W,.Unit=true};waiterJoin(xrtThreadCreate(waiterThread,&F,0));testRequire(xrtEventUnit(&E),"host releases event after handoff cleanup");
#endif
}
static void waiterArguments(void)
{
    xevent E;xeventwaiter W,Before;memset(&W,0xa5,sizeof(W));Before=W;testRequire(xrtEventInit(&E,false,true),"argument event fixture");
    testRequire(!xrtEventWaiterInit(&W,&E,2) && !memcmp(&W,&Before,sizeof(W)) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"unknown flag leaves storage untouched");xrtClearError();
    testRequire(!xrtEventWaiterInit((xeventwaiter*)&E,&E,0) && xrtEventWaitFor(&E,0)==XWAIT_OK,"alias init cannot overwrite borrowed event");xrtClearError();
    testRequire(!xrtEventWaiterInit((xeventwaiter*)(uintptr_t)(UINTPTR_MAX-2),&E,0),"wrapped context range rejected");xrtClearError();
    testRequire(xrtEventWaiterInit(&W,&E,0) && xrtEventSet(&E),"valid argument context");testRequire(xrtEventWaiterWaitFor(&W,-2)==XWAIT_ERROR && xrtErrorKind(xrtGetError())==XERR_ARGUMENT && xrtEventWaiterWaitFor(&W,0)==XWAIT_OK,"invalid timeout consumes no event");xrtClearError();
    Before=W;xevent SavedEvent;memcpy(&SavedEvent,&E,sizeof(E));
    testRequire(!xrtEventWaiterGet(&W,(xeventwaiterinfo*)&W) && !memcmp(&W,&Before,sizeof(W)) && !memcmp(&E,&SavedEvent,sizeof(E)),"context output alias preserves every context/event byte");xrtClearError();
    testRequire(!xrtEventWaiterGet(&W,(xeventwaiterinfo*)&E) && !memcmp(&W,&Before,sizeof(W)) && !memcmp(&E,&SavedEvent,sizeof(E)),"event output alias preserves every context/event byte");xrtClearError();
    testRequire(xrtEventWaiterUnit(&W) && !xrtEventWaiterUnit(&W) && xrtErrorKind(xrtGetError())==XERR_STATE && xrtEventUnit(&E),"double unit detected");xrtClearError();
}
static void waiterWarm(void)
{
    xevent E;xeventwaiter W;testRequire(xrtEventInit(&E,false,false) && xrtEventWaiterInit(&W,&E,0),"zero-heap context fixture");xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B),"warm quota");xrtSetErrorInfo(XERR_IO,"waiter.sentinel",5,"warm ambient");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(B,&Scope),"enter zero-allocation scope");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned N=0;N<20000;++N)testRequire(xrtEventSet(&E) && xrtEventWaiterWaitFor(&W,0)==XWAIT_OK && xrtGetError()==Ambient,"zero-heap ready signal cycle");
    for(unsigned N=0;N<32;++N){double Start=xrtTimer();testRequire(xrtEventWaiterWaitFor(&W,1)==XWAIT_TIMEOUT && xrtTimer()-Start>=0.001 && xrtGetError()==Ambient,"zero-heap actual timer rearm cycle");}
    testRequire(xrtEventWaiterUnit(&W),"release first warm timer");
#if defined(_WIN32) || defined(_WIN64)
    DWORD HandlesBefore,HandlesAfter;testRequire(GetProcessHandleCount(GetCurrentProcess(),&HandlesBefore),"native handle baseline");
#endif
    for(unsigned N=0;N<1000;++N)testRequire(xrtEventWaiterInit(&W,&E,0) && xrtEventWaiterUnit(&W) && xrtGetError()==Ambient,"zero-heap repeated cold context lifetime");
#if defined(_WIN32) || defined(_WIN64)
    testRequire(GetProcessHandleCount(GetCurrentProcess(),&HandlesAfter) && HandlesBefore==HandlesAfter,"all 1000 native timers return handle baseline");
#endif
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"wait/create/unit use no requested heap");
#endif
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave warm quota");xrtClearError();xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.LiveCount && !BI.Rejections,"no hidden requested allocation or rejection");xrtAllocationBudgetFree(B);testRequire(xrtEventUnit(&E),"warm event cleanup");
}
int main(void)
{waiterBasics();waiterArguments();waiterWarm();puts("[PASS] event waiter: auto/manual/timeout budgets/interrupt/rearm/owner/handoff/aliases/20000 ready/32 armed/1000 zero-heap handle-balanced lifetimes");return 0;}
