#include <xrt/event_waiter.h>
#include <xrt/detail/wait.h>
#include "../internal/xrt_sync.h"
#if defined(XRT_FEATURE_EVENT_WAITER)
#define XRT_EVENT_WAITER_MAGIC UINT32_C(0x58455731)
typedef struct xrt_event_waiter_impl {
    uint32 Magic;
    xeventwaiterbackend Backend;
    uint64 Owner;
    xevent* Event;
#if defined(_WIN32) || defined(_WIN64)
    HANDLE Timer;
#endif
    bool Active;
} xrt_event_waiter_impl;
typedef char xrt_event_waiter_storage_check[(sizeof(xrt_event_waiter_impl)<=XRT_EVENT_WAITER_STORAGE_SIZE)?1:-1];
static bool __xrtEventWaiterFail(xerrkind Kind,cstr Operation,int32 SystemCode)
{
    xerrordesc D={.Kind=Kind,.Code=(int32)Kind,.SystemCode=SystemCode,.Domain="xrt.event_waiter",.Operation=Operation,.Message="event deadline waiter operation failed"};
    xerror* E=xrtErrorBuild(&D);if(E)xrtSetErrorTake(E);else xrtSetErrorKind(XERR_MEMORY);return false;
}
static xrt_event_waiter_impl* __xrtEventWaiterRequire(const xeventwaiter* W,cstr Operation)
{
    if(!xrtMemRangeValid(W,sizeof(*W))){__xrtEventWaiterFail(XERR_ARGUMENT,Operation,0);return NULL;}
    xrt_event_waiter_impl* I=(xrt_event_waiter_impl*)(uintptr_t)W;
    if(I->Magic!=XRT_EVENT_WAITER_MAGIC){__xrtEventWaiterFail(XERR_STATE,Operation,0);return NULL;}
    return I;
}
#if defined(_WIN32) || defined(_WIN64)
typedef HANDLE (WINAPI *xrt_create_waitable_timer_proc)(LPSECURITY_ATTRIBUTES,LPCWSTR,DWORD,DWORD);
static HANDLE __xrtEventWaiterTimer(uint32 Flags,xeventwaiterbackend* Backend)
{
    xrt_create_waitable_timer_proc Create=NULL;FARPROC Address=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreateWaitableTimerExW");
    typedef char xrt_waiter_function_pointer_size[(sizeof(Create)==sizeof(Address))?1:-1];(void)sizeof(xrt_waiter_function_pointer_size);memcpy(&Create,&Address,sizeof(Create));
    HANDLE Timer=Create?Create(NULL,NULL,2u,UINT32_C(0x00100002)):NULL; /* HIGH_RESOLUTION; SYNCHRONIZE | TIMER_MODIFY_STATE */
    if(Timer){*Backend=XEVENT_WAITER_WINDOWS_HIGH_RESOLUTION;return Timer;}
    DWORD Code=Create?GetLastError():ERROR_CALL_NOT_IMPLEMENTED;
    if(Code!=ERROR_INVALID_PARAMETER && Code!=ERROR_NOT_SUPPORTED && Code!=ERROR_CALL_NOT_IMPLEMENTED){__xrtEventWaiterFail(XERR_IO,"init.timer",(int32)Code);return NULL;}
    if(Flags&XRT_EVENT_WAITER_REQUIRE_PRECISE){__xrtEventWaiterFail(XERR_UNSUPPORTED,"init.precision",(int32)Code);return NULL;}
    Timer=CreateWaitableTimerW(NULL,FALSE,NULL);
    if(!Timer){__xrtEventWaiterFail(XERR_IO,"init.fallback",(int32)GetLastError());return NULL;}
    *Backend=XEVENT_WAITER_WINDOWS_STANDARD;return Timer;
}
static xwaitresult __xrtEventWaiterWindows(xrt_event_waiter_impl* I,double Deadline)
{
    HANDLE Event=__xrtEventImpl(I->Event)->Handle;
    for(;;){
        DWORD Ready=WaitForSingleObject(Event,0);
        if(Ready==WAIT_OBJECT_0)return XWAIT_OK;
        if(Ready!=WAIT_TIMEOUT){__xrtEventWaiterFail(XERR_IO,"wait.poll",(int32)GetLastError());return XWAIT_ERROR;}
        if(Deadline==INFINITY){DWORD R=WaitForSingleObject(Event,INFINITE);if(R==WAIT_OBJECT_0)return XWAIT_OK;__xrtEventWaiterFail(XERR_IO,"wait.forever",(int32)GetLastError());return XWAIT_ERROR;}
        double Now=xrtTimer();if(!isfinite(Now))return XWAIT_ERROR;
        if(Now>=Deadline){Ready=WaitForSingleObject(Event,0);if(Ready==WAIT_OBJECT_0)return XWAIT_OK;if(Ready==WAIT_TIMEOUT)return XWAIT_TIMEOUT;__xrtEventWaiterFail(XERR_IO,"wait.poll",(int32)GetLastError());return XWAIT_ERROR;}
        /* Chunk very large durations before converting outward to signed 100ns.
         * No signed overflow, periodic timer or change to global clock resolution. */
        double Remaining=Deadline-Now;if(Remaining>4294967.0)Remaining=4294967.0;
        LARGE_INTEGER Due;Due.QuadPart=-(LONGLONG)ceil(Remaining*1e7);if(Due.QuadPart==0)Due.QuadPart=-1;
        if(!SetWaitableTimer(I->Timer,&Due,0,NULL,NULL,FALSE)){__xrtEventWaiterFail(XERR_IO,"wait.arm",(int32)GetLastError());return XWAIT_ERROR;}
        HANDLE Handles[2]={Event,I->Timer};DWORD R=WaitForMultipleObjects(2,Handles,FALSE,INFINITE);
        if(R==WAIT_OBJECT_0){if(!CancelWaitableTimer(I->Timer)){__xrtEventWaiterFail(XERR_IO,"wait.cancel",(int32)GetLastError());return XWAIT_ERROR;}return XWAIT_OK;}
        if(R==WAIT_OBJECT_0+1)continue; /* Recheck original monotonic budget and ready event. */
        DWORD Code=GetLastError();(void)CancelWaitableTimer(I->Timer);__xrtEventWaiterFail(XERR_IO,"wait.backend",(int32)Code);return XWAIT_ERROR;
    }
}
#endif
XRT_API bool xrtEventWaiterInit(xeventwaiter* W,xevent* Event,uint32 Flags)
{
    if(!xrtMemRangeValid(W,sizeof(*W)) || !xrtMemRangeValid(Event,sizeof(*Event)) || xrtMemRangesOverlap(W,sizeof(*W),Event,sizeof(*Event)) || (Flags&~XRT_EVENT_WAITER_REQUIRE_PRECISE))return __xrtEventWaiterFail(XERR_ARGUMENT,"init",0);
    xrt_event_impl* E=__xrtEventImpl(Event);if(E->Magic!=XRT_EVENT_MAGIC)return __xrtEventWaiterFail(XERR_STATE,"init.event",0);
    xrt_event_waiter_impl I;memset(&I,0,sizeof(I));I.Magic=XRT_EVENT_WAITER_MAGIC;I.Owner=__xrtCurrentThreadId();I.Event=Event;
#if defined(_WIN32) || defined(_WIN64)
    I.Timer=__xrtEventWaiterTimer(Flags,&I.Backend);if(!I.Timer)return false;
#else
    I.Backend=E->Monotonic?XEVENT_WAITER_POSIX_MONOTONIC:XEVENT_WAITER_PLATFORM;
    if((Flags&XRT_EVENT_WAITER_REQUIRE_PRECISE) && !E->Monotonic)return __xrtEventWaiterFail(XERR_UNSUPPORTED,"init.precision",0);
#endif
    memset(W,0,sizeof(*W));memcpy(W,&I,sizeof(I));return true;
}
XRT_API bool xrtEventWaiterUnit(xeventwaiter* W)
{
    xrt_event_waiter_impl* I=__xrtEventWaiterRequire(W,"unit");if(!I)return false;
    if(I->Active)return __xrtEventWaiterFail(XERR_STATE,"unit.active",0);
#if defined(_WIN32) || defined(_WIN64)
    if(!CloseHandle(I->Timer))return __xrtEventWaiterFail(XERR_IO,"unit.timer",(int32)GetLastError());
#endif
    memset(W,0,sizeof(*W));return true;
}
XRT_API xwaitresult xrtEventWaiterWaitFor(xeventwaiter* W,int64 Timeout)
{
    xrt_event_waiter_impl* I=__xrtEventWaiterRequire(W,"wait");if(!I)return XWAIT_ERROR;
    if(Timeout<XRT_WAIT_FOREVER){__xrtEventWaiterFail(XERR_ARGUMENT,"wait.timeout",0);return XWAIT_ERROR;}
    if(I->Owner!=__xrtCurrentThreadId() || I->Active){__xrtEventWaiterFail(XERR_STATE,"wait.owner",0);return XWAIT_ERROR;}
    if(__xrtEventImpl(I->Event)->Magic!=XRT_EVENT_MAGIC){__xrtEventWaiterFail(XERR_STATE,"wait.event",0);return XWAIT_ERROR;}
    double Deadline=__xrtWaitAfter(Timeout);if(isnan(Deadline))return XWAIT_ERROR;
    I->Active=true;xwaitresult R;
#if defined(_WIN32) || defined(_WIN64)
    R=__xrtEventWaiterWindows(I,Deadline);
#else
    R=__xrtEventWaitUntil(I->Event,Deadline);
#endif
    I->Active=false;return R;
}
XRT_API bool xrtEventWaiterGet(const xeventwaiter* W,xeventwaiterinfo* Info)
{
    xrt_event_waiter_impl* I=__xrtEventWaiterRequire(W,"get");if(!I)return false;
    if(!xrtMemRangeValid(Info,sizeof(*Info)) || xrtMemRangesOverlap(Info,sizeof(*Info),W,sizeof(*W)) || xrtMemRangesOverlap(Info,sizeof(*Info),I->Event,sizeof(*I->Event)))return __xrtEventWaiterFail(XERR_ARGUMENT,"get.output",0);
    *Info=(xeventwaiterinfo){sizeof(*W),I->Backend,
#if defined(_WIN32) || defined(_WIN64)
        1
#else
        0
#endif
    };return true;
}
#endif
