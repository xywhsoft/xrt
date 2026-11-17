#if !defined(_WIN32) && !defined(_WIN64) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdbool.h>
static bool LearnErrorKey, HaveErrorKey;
static _Thread_local unsigned NativeCountdown, NativeFaults, NativeWrites, WriteMark;
static _Thread_local bool PersistentFault, KeepFailing;
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
static DWORD ErrorKey;
static BOOL WINAPI deNativeSet(DWORD Key, PVOID Value)
{
    if (LearnErrorKey) { ErrorKey=Key; HaveErrorKey=true; LearnErrorKey=false; }
    if (HaveErrorKey && Key==ErrorKey) {
        ++NativeWrites;
        if (KeepFailing || (NativeCountdown && !--NativeCountdown)) {
            ++NativeFaults; KeepFailing=PersistentFault; return FALSE;
        }
    }
    return FlsSetValue(Key,Value);
}
#define FlsSetValue deNativeSet
#else
#include <pthread.h>
#include <errno.h>
static pthread_key_t ErrorKey;
static int deNativeSet(pthread_key_t Key, const void* Value)
{
    if (LearnErrorKey) { ErrorKey=Key; HaveErrorKey=true; LearnErrorKey=false; }
    if (HaveErrorKey && Key==ErrorKey) {
        ++NativeWrites;
        if (KeepFailing || (NativeCountdown && !--NativeCountdown)) {
            ++NativeFaults; KeepFailing=PersistentFault; return ENOMEM;
        }
    }
    return pthread_setspecific(Key,Value);
}
#define pthread_setspecific deNativeSet
#endif
#define XRPC_MODULE_XRPC_DISPATCH
#define XRT_MODULE_MEMORY_STATS
#include <xrpc/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#if defined(_WIN32) || defined(_WIN64)
#undef FlsSetValue
#else
#undef pthread_setspecific
#endif
#define XRPC_IMPLEMENTATION
#include "../../../../single/extlibs/xrpc.h"
#define main deNormalMain
#include "../test_dispatch_errors.c"
#undef main

static void deDisarm(void)
{ NativeCountdown=0; PersistentFault=false; KeepFailing=false; }
static void deArm(unsigned Point)
{ WriteMark=NativeWrites; NativeCountdown=Point; }
typedef struct defaultfixture {
    deobserver Observer;
    unsigned Mode;
    bool Clear;
    const xerror* Borrowed;
    xrpcreply* Reply;
} defaultfixture;
static bool deFaultEmit(const xrpcoutgoing* R, bool Batch, size_t Bytes, ptr P)
{
    defaultfixture* F=P; deEmit(R,Batch,Bytes,&F->Observer);
    if (F->Mode==1) {
        xrtSetErrorInfo(XERR_STATE,"dispatch.callback",2,"accepted output notice");
        /* Arm after the complete diagnostic/handler-protection boundary. */
        if (F->Observer.Emits==2) deArm(1);
    } else if (F->Mode==6) {
        xrtSetErrorInfo(XERR_STATE,"dispatch.callback",2,"abort during accepted output");
        deArm(1); testRequire(!xrpcReplyAbort(F->Reply,xrtGetError()),"reentrant abort fails locally");
    }
    return true;
}
static bool deFaultCall(const xrpcmessage* R, xrpcreply* Reply, ptr P)
{
    (void)R; defaultfixture* F=P; ++F->Observer.Calls; F->Reply=Reply;
    if (F->Mode==1 || F->Mode==6) return xrpcReplyResult(Reply,XRT_STR_LITERAL("true"));
    if (F->Mode==5) {
        deArm(1); testRequire(!xrpcReplyAbort(Reply,F->Borrowed),"borrowed abort stops business");
    } else {
        xrtSetErrorInfo(XERR_STATE,"dispatch.callback",2,"first local diagnostic");
        if (F->Mode==2 || F->Mode==3) deArm(F->Mode-1);
        testRequire(!xrpcReplyAbort(Reply,xrtGetError()),"current error abort stops business");
        if (F->Mode==4) {
            if (F->Clear) xrtClearError();
            deArm(1);
        }
    }
    /* Repeat calls after a known slot failure must not retry the native write. */
    if (F->Mode!=4) testRequire(!xrpcReplyResult(Reply,XRT_STR_LITERAL("0")),"sticky failure cannot be retried");
    return true;
}
static void deFailure(unsigned Mode, bool Ambient, bool Persistent, bool Clear)
{
    xallocationbudget* Budget=NULL; xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(65536,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"owned diagnostics ledger");
    defaultfixture F={.Mode=Mode,.Clear=Clear};
    if (Ambient) { xrtSetErrorInfo(XERR_STATE,"dispatch.ambient",1,"caller owner"); F.Observer.Ambient=xrtGetError(); }
    xerror* Borrowed=Mode==5?xrtErrorCreate(XERR_AGAIN,"dispatch.borrowed",3,"external owner"):NULL;
    testRequire(Mode!=5 || Borrowed!=NULL,"borrowed error fixture"); F.Borrowed=Borrowed;
    xrtSetErrorHandler(deNotice,&F.Observer);
    xrpcdispatchinfo I,Keep; memset(&I,0x5a,sizeof(I)); I.Size=sizeof(I); Keep=I;
    xrpcmessage Scratch[2]; unsigned Faults=NativeFaults; PersistentFault=Persistent;
    if (!Mode) deArm(1);
    bool OK=xrpcDispatch(XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":2}]"),
        NULL,Scratch,2,deFaultCall,&F,deFaultEmit,&F,&I);
    unsigned ExpectedWrites=Mode==3?2u:1u;
    testRequire(!OK && NativeFaults==Faults+1 && NativeWrites==WriteMark+ExpectedWrites && !NativeCountdown,"one native slot failure and no recursive/repeated setter retry");
    if (!Mode) testRequire(!memcmp(&I,&Keep,sizeof(I)) && !F.Observer.Calls && !F.Observer.Emits && xrtGetError()==F.Observer.Ambient,"entry failure leaves Info/ambient unchanged and runs no callback");
    else testRequire(I.Batch && I.Messages==2 && I.Invoked==(Mode==1?2u:1u) && I.Visited==(Mode==1?2u:0u) &&
        I.Replies==(Mode==1?2u:Mode==6?1u:0u) && I.Replies==F.Observer.Emits && I.Invoked==F.Observer.Calls &&
        I.OutputBytes==(I.Replies?F.Observer.Bytes+I.Replies+1u:0u),"post-admission failure publishes exact irreversible callback/output effects; later handlers stop on sticky failure");
    testRequire(!F.Observer.Old && F.Observer.Fresh==(!Mode || Mode==5?0u:Mode==1?2u:1u),"restoration and retained errors never notify again");
    if (Mode==1 || Mode==2 || Mode==6 || (Mode==4&&!Clear))
        testRequire(xrtErrorFind(xrtGetError(),"dispatch.callback",2)!=NULL,"failed transfer leaves existing published edge alive");
    else if (Mode) testRequire(!xrtGetError(),"failed exchange may leave an empty slot, without fabricated diagnostic");
    if (Borrowed) testRequire(xrtErrorCode(Borrowed)==3,"borrowed caller reference never consumed");
    deDisarm(); xrtSetErrorHandler(NULL,NULL); xrtClearError(); xrtErrorFree(Borrowed);
    xallocationbudgetinfo BI={.Size=sizeof(BI)};
    testRequire(xrtAllocationBudgetLeave(&Scope) && xrtAllocationBudgetInfo(Budget,&BI) && !BI.LiveBytes && !BI.LiveCount && !BI.Rejections,"all saved/captured/borrowed/provisional diagnostic owners physically refunded");
    xrtAllocationBudgetFree(Budget);
}
static void deBound(bool Ambient)
{
    xrt_error_context Context={0}; xrt_error_context* Prior=__xrtErrorContextSwap(&Context);
    deobserver O={.Notice=true,.Nested=true}; if (Ambient) { xrtSetErrorInfo(XERR_STATE,"dispatch.ambient",1,"bound caller"); O.Ambient=xrtGetError(); }
    xrtSetErrorHandler(deNotice,&O); unsigned Writes=NativeWrites,Faults=NativeFaults; deArm(1); PersistentFault=true;
    xrpcmessage Scratch[1]; xrpcdispatchinfo I={.Size=sizeof(I)};
    testRequire(xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1}"),NULL,Scratch,1,deCall,&O,deEmit,&O,&I) &&
        NativeWrites==Writes && NativeFaults==Faults && NativeCountdown==1 && xrtGetError()==O.Ambient && !O.Old && O.Fresh==1,"bound independent nested contexts avoid native error-key setters and remain quiet");
    deDisarm(); xrtSetErrorHandler(NULL,NULL); xrtClearError(); testRequire(__xrtErrorContextSwap(Prior)==&Context && !Context.Error && !xrtGetError(),"outer context restored");
}
static bool deSaturatedCall(const xrpcmessage* R, xrpcreply* Reply, ptr P)
{
    (void)R; xerror** Error=P; xrtSetErrorInfo(XERR_STATE,"dispatch.saturated",4,"test-only retain boundary"); *Error=(xerror*)xrtGetError();
    (*Error)->RefCount=INT32_MAX;
    testRequire(!xrpcReplyAbort(Reply,*Error) && !xrtGetError(),"current capture survives deliberately saturated re-exposure Ref");
    return true;
}
static void deSaturated(void)
{
    xrpcmessage Scratch[1]; xrpcdispatchinfo I={.Size=sizeof(I)}; xerror* Error=NULL; deobserver O={0};
    testRequire(!xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1}"),NULL,Scratch,1,deSaturatedCall,&Error,deEmit,&O,&I) &&
        xrtGetError()==Error && I.Invoked==1 && !I.Visited && !I.Replies,"final owning transfer needs no additional reference");
    Error->RefCount=1; xrtClearError();
}
int main(void)
{
    xerror* Old=NULL; LearnErrorKey=true;
    testRequire(xrtTrySwapErrorTake(NULL,&Old) && HaveErrorKey && !LearnErrorKey && !Old,"learn only the real native error key");
    testRequire(!deNormalMain(),"normal cases with inactive native shim"); unsigned Cases=0;
    for (unsigned M=0;M<7;++M) for (unsigned A=0;A<2;++A) for (unsigned P=0;P<2;++P) for (unsigned C=0;C<2;++C) { deFailure(M,A!=0,P!=0,C!=0); ++Cases; }
    deBound(false); deBound(true); deSaturated();
    printf("[PASS] %u native error-key failure cases,2 bound nested contexts,1 artificial retain saturation boundary\n",Cases); return 0;
}
