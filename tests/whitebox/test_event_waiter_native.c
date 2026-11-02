#include "../test.h"
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <xrt/event_waiter.h>
static unsigned NativeFault,NativeFallbacks,NativeReentry;
static xeventwaiter* NativeActive;
XRT_API bool nativeTestWaiterUnit(xeventwaiter* W);
static HANDLE WINAPI nativeTestCreate(LPSECURITY_ATTRIBUTES A,LPCWSTR N,DWORD F,DWORD R)
{
    if(NativeFault==1 || NativeFault==3){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
    if(NativeFault==2){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
    typedef HANDLE (WINAPI *proc)(LPSECURITY_ATTRIBUTES,LPCWSTR,DWORD,DWORD);proc P=NULL;FARPROC Raw=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreateWaitableTimerExW");memcpy(&P,&Raw,sizeof(P));return P(A,N,F,R);
}
static FARPROC WINAPI nativeTestAddress(HMODULE M,LPCSTR N)
{if(strcmp(N,"CreateWaitableTimerExW"))return GetProcAddress(M,N);FARPROC R=NULL;HANDLE (WINAPI *P)(LPSECURITY_ATTRIBUTES,LPCWSTR,DWORD,DWORD)=nativeTestCreate;memcpy(&R,&P,sizeof(R));return R;}
static HANDLE WINAPI nativeTestFallback(LPSECURITY_ATTRIBUTES A,BOOL M,LPCWSTR N)
{++NativeFallbacks;if(NativeFault==3){SetLastError(ERROR_ACCESS_DENIED);return NULL;}return CreateWaitableTimerW(A,M,N);}
static BOOL WINAPI nativeTestArm(HANDLE T,const LARGE_INTEGER* D,LONG P,PTIMERAPCROUTINE C,LPVOID A,BOOL R)
{
    if(NativeFault==4){testRequire(NativeActive && !nativeTestWaiterUnit(NativeActive) && xrtErrorKind(xrtGetError())==XERR_STATE,"active native-call reentry cannot unit live context");++NativeReentry;xrtClearError();SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    return SetWaitableTimer(T,D,P,C,A,R);
}
static DWORD WINAPI nativeTestWait(DWORD N,const HANDLE* H,BOOL A,DWORD T)
{if(NativeFault==5){SetLastError(ERROR_INVALID_HANDLE);return WAIT_FAILED;}return WaitForMultipleObjects(N,H,A,T);}
static BOOL WINAPI nativeTestCancel(HANDLE T)
{if(NativeFault==6){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}return CancelWaitableTimer(T);}
static BOOL WINAPI nativeTestClose(HANDLE T)
{if(NativeFault==7){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}return CloseHandle(T);}
#define GetProcAddress nativeTestAddress
#define CreateWaitableTimerW nativeTestFallback
#define SetWaitableTimer nativeTestArm
#define WaitForMultipleObjects nativeTestWait
#define CancelWaitableTimer nativeTestCancel
#define CloseHandle nativeTestClose
#define xrtEventWaiterInit nativeTestWaiterInit
#define xrtEventWaiterUnit nativeTestWaiterUnit
#define xrtEventWaiterWaitFor nativeTestWaiterWaitFor
#define xrtEventWaiterGet nativeTestWaiterGet
#include "../../src/concurrency/event_waiter.c"
#undef GetProcAddress
#undef CreateWaitableTimerW
#undef SetWaitableTimer
#undef WaitForMultipleObjects
#undef CancelWaitableTimer
#undef CloseHandle
#undef xrtEventWaiterInit
#undef xrtEventWaiterUnit
#undef xrtEventWaiterWaitFor
#undef xrtEventWaiterGet
static int32 nativeTestSignal(ptr Raw)
{xrtSleep(1);return xrtEventSet(Raw)?0:1;}
int main(void)
{
    xevent E;xeventwaiter W,Before;testRequire(xrtEventInit(&E,false,false),"native fault event");memset(&W,0xa5,sizeof(W));Before=W;NativeFault=1;
    testRequire(!nativeTestWaiterInit(&W,&E,XRT_EVENT_WAITER_REQUIRE_PRECISE) && xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED && !memcmp(&W,&Before,sizeof(W)) && !NativeFallbacks,"strict precise unsupported consumes nothing");xrtClearError();
    testRequire(nativeTestWaiterInit(&W,&E,0) && NativeFallbacks==1,"unsupported precise backend permits explicit default fallback");xeventwaiterinfo Info;testRequire(nativeTestWaiterGet(&W,&Info) && Info.Backend==XEVENT_WAITER_WINDOWS_STANDARD && nativeTestWaiterUnit(&W),"fallback capability reported and resource released");
    NativeFault=2;testRequire(!nativeTestWaiterInit(&W,&E,0) && xrtErrorKind(xrtGetError())==XERR_IO && xrtErrorSystemCode(xrtGetError())==ERROR_NOT_ENOUGH_MEMORY && NativeFallbacks==1,"resource exhaustion is never masked by fallback");xrtClearError();
    NativeFault=3;testRequire(!nativeTestWaiterInit(&W,&E,0) && xrtErrorSystemCode(xrtGetError())==ERROR_ACCESS_DENIED && NativeFallbacks==2,"fallback creation failure is observable");xrtClearError();
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"native failure diagnostic OOM");NativeFault=1;Before=W;testRequire(!nativeTestWaiterInit(&W,&E,XRT_EVENT_WAITER_REQUIRE_PRECISE) && xrtErrorKind(xrtGetError())==XERR_MEMORY && !memcmp(&W,&Before,sizeof(W)),"failure output contract survives diagnostic OOM");testRequire(xrtAllocationBudgetLeave(&Scope),"leave native diagnostic quota");xrtClearError();xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.LiveCount,"native failed init owns no quota residue");xrtAllocationBudgetFree(B);
    NativeFault=0;testRequire(nativeTestWaiterInit(&W,&E,0),"native fault wait context");NativeActive=&W;
    for(unsigned Mode=4;Mode<=5;++Mode){NativeFault=Mode;testRequire(nativeTestWaiterWaitFor(&W,1)==XWAIT_ERROR && xrtErrorKind(xrtGetError())==XERR_IO,"arm/backend failure observable");xrtClearError();NativeFault=0;double Start=xrtTimer();testRequire(nativeTestWaiterWaitFor(&W,1)==XWAIT_TIMEOUT && xrtTimer()-Start>=0.001,"native failed wait leaves reusable context, no shortened budget");}
    NativeFault=6;xthread* T=xrtThreadCreate(nativeTestSignal,&E,0);testRequire(T && nativeTestWaiterWaitFor(&W,1000)==XWAIT_ERROR && xrtErrorSystemCode(xrtGetError())==ERROR_ACCESS_DENIED,"event-won timer cancellation failure observable");xrtClearError();testRequire(xrtThreadWaitFor(T,10000)==XWAIT_OK,"native signal thread completes");xrtThreadDestroy(T);NativeFault=0;testRequire(nativeTestWaiterWaitFor(&W,1)==XWAIT_TIMEOUT,"rearm resets an event-won stale timer");
    NativeFault=7;Before=W;testRequire(!nativeTestWaiterUnit(&W) && !memcmp(&W,&Before,sizeof(W)) && xrtErrorSystemCode(xrtGetError())==ERROR_ACCESS_DENIED,"unit failure retains context for retry");xrtClearError();NativeFault=0;testRequire(nativeTestWaiterUnit(&W) && NativeReentry==1 && xrtEventUnit(&E),"native resource retries and reentry balance");puts("[PASS] native event waiter: unsupported/fallback/resource/diagnostic OOM/arm/wait/cancel/unit/reentry");return 0;
}
#else
#include "../../src/internal/xrt_sync.h"
int main(void)
{
    xevent E;xeventwaiter W,Before;testRequire(xrtEventInit(&E,false,false),"POSIX fallback event");xrt_event_impl* I=__xrtEventImpl(&E);
    /* Real default-clock condition, not a mismatched flag on a monotonic object. */
    testRequire(!pthread_cond_destroy(&I->Condition) && !pthread_cond_init(&I->Condition,NULL),"install real realtime fallback condition");I->Monotonic=false;memset(&W,0xa5,sizeof(W));Before=W;
    testRequire(!xrtEventWaiterInit(&W,&E,XRT_EVENT_WAITER_REQUIRE_PRECISE) && xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED && !memcmp(&W,&Before,sizeof(W)),"strict POSIX fallback rejection preserves storage");xrtClearError();
    xeventwaiterinfo Info;testRequire(xrtEventWaiterInit(&W,&E,0) && xrtEventWaiterGet(&W,&Info) && Info.Backend==XEVENT_WAITER_PLATFORM && !Info.NativeTimers,"POSIX fallback capability visible");
    double Start=xrtTimer();testRequire(xrtEventWaiterWaitFor(&W,2)==XWAIT_TIMEOUT && xrtTimer()-Start>=0.002 && xrtEventSet(&E) && xrtEventWaiterWaitFor(&W,0)==XWAIT_OK,"real fallback condition deadline and event signal");testRequire(xrtEventWaiterUnit(&W) && xrtEventUnit(&E),"POSIX fallback cleanup");puts("[PASS] POSIX event waiter: real realtime-condition fallback/strict rejection/deadline/signal/lifetime");return 0;
}
#endif
