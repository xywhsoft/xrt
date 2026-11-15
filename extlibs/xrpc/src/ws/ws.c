#include <xrpc/ws.h>
#include "../internal/xrpc_duplex_internal.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <math.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_WS_DIAL)
#include <xrpc/ws_dial.h>
#endif
#if defined(XRPC_FEATURE_XRPC_WS_URI)
#include <xrpc/ws_uri.h>
#include <xrt/charset.h>
#endif
#if defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
#include <xrpc/ws_deflate.h>
#endif
#if defined(XRPC_FEATURE_XRPC_WS_HTTP)
#include "../internal/xrpc_ws_http_internal.h"
#include <xrt/http1_net.h>
#if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
#include <xrt/http1_tls.h>
#endif
#endif
#if defined(XRPC_FEATURE_XRPC_WS)
struct xrpcws {
    xrpcwsconfig Config;xrpcwsinfo Info;xatomic32 References;
    #if defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
    bool DeflateReady;
    #endif
    xmutex Gate,Retirement;xcond Retired;xnetengine* Engine;xnetworker* Worker;xwsstream* Stream;
    xrpcduplex* Duplex;xrpcduplexoutput* Output;unsigned char* Receive;
    xnetpost Post;uint64 Timer,Token;double TimerAt,MessageEntered;size_t Used,Capacity,ReadTurnBytes;
    bool Active,Posted,EventsGone,NativeClosed,NativeFailed,MessageActive,Complete,Offered,AbortIssued,DuplexClosing,TimerUnavailable,DestroyingNative;
    unsigned CleanupDelay;xrpcwsreason NativeReason;
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP)
    xrpcwsupgrade* Upgrade;xrpcwshttpinfo UpgradeInfo;bool UpgradeAbort;
    #endif
    unsigned char Context[];
};
static void xrpcWsWork(xnetworker*,ptr);
static void xrpcWsTimer(xnetworker*,uint64,xnetresult,ptr);
#if defined(XRPC_FEATURE_XRPC_WS_HTTP)
static void xrpcWsHttpStep(xrpcws*);
static bool xrpcWsHttpRetire(xrpcws*);
static void xrpcWsHttpDiscard(xrpcws*);
#if defined(XRPC_FEATURE_XRPC_WS_DIAL)
static void xrpcWsDialStep(xrpcws*);
static bool xrpcWsDialRetire(xrpcws*);
#endif
#endif
static bool xrpcWsReport(xerrkind K,cstr M){xrtSetErrorInfo(K,"xrt.rpc",19401,M);return false;}
static double xrpcWsDeadline(double Entered,int64 Timeout)
{return Timeout==XRT_WAIT_FOREVER?INFINITY:Timeout==0?Entered:nextafter(Entered+(double)(Timeout/1000)+(double)(Timeout%1000)*.001,INFINITY);}
static int64 xrpcWsRemaining(double At)
{if(At==INFINITY)return XRT_WAIT_FOREVER;double N=ceil((At-xrtTimer())*1000.);return N<=0?0:N>=0x1p63?INT64_MAX:(int64)N;}
static void xrpcWsRef(xrpcws* P)
{xrtAtomic32FetchAdd(&P->References,1,XMEMORY_RELAXED);}
static void xrpcWsDrop(const void* Data)
{
    xrpcws* P=(xrpcws*)Data;xrtMutexLock(&P->Retirement);
    xrtAtomic32FetchSub(&P->References,1,XMEMORY_RELEASE);xrtCondBroadcast(&P->Retired);xrtMutexUnlock(&P->Retirement);
}
static bool xrpcWsCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpcws*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcWsTrace(const void* Data,xrtownershipvisitor Visit,ptr User)
{(void)Data;(void)Visit;(void)User;return false;}
static const xrtownershipops xrpcWsOps={xrpcWsCount,xrpcWsTrace};
static bool xrpcWsWakeLocked(xrpcws* P)
{
    if(!P->Active || P->Posted || P->Info.State==XRPC_WS_CLOSED)return true;
    P->Posted=true;xrpcWsRef(P);
    if(xrtNetPost(P->Worker,&P->Post,xrpcWsWork,P))return true;
    P->Posted=false;++P->Info.PostFailures;xrpcWsDrop(P);return false;
}
static void xrpcWsWake(ptr Data)
{xrpcws* P=Data;xrtMutexLock(&P->Gate);(void)xrpcWsWakeLocked(P);xrtMutexUnlock(&P->Gate);}
static bool xrpcWsTerminal(xrpcwsstate State)
{
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP)
    return State==XRPC_WS_CLOSING || State==XRPC_WS_CLOSED;
    #else
    return State>=XRPC_WS_CLOSING;
    #endif
}
static bool xrpcWsClosing(xrpcws* P)
{xrtMutexLock(&P->Gate);bool B=xrpcWsTerminal(P->Info.State);xrtMutexUnlock(&P->Gate);return B;}
static void xrpcWsFail(xrpcws* P,xrpcwsreason Reason)
{
    xrtMutexLock(&P->Gate);if(!xrpcWsTerminal(P->Info.State)){P->Info.State=XRPC_WS_CLOSING;P->Info.Reason=Reason;}xrtMutexUnlock(&P->Gate);
}
static xrpcwsreason xrpcWsLocalFailure(void)
{return xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_WS_MEMORY:xrtErrorKind(xrtGetError())==XERR_RANGE?XRPC_WS_LIMIT:XRPC_WS_TRANSPORT;}
static xrpccallstatus xrpcWsCallFailure(xrpcwsreason Reason)
{return Reason==XRPC_WS_LOCAL?XRPC_CALL_CLOSED:Reason==XRPC_WS_PROTOCOL?XRPC_CALL_PROTOCOL:(Reason==XRPC_WS_HTTP||Reason==XRPC_WS_AUTH)?XRPC_CALL_HTTP_ERROR:Reason==XRPC_WS_LIMIT?XRPC_CALL_LIMIT:Reason==XRPC_WS_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT;}
static void xrpcWsNativeError(xrpcws* P,const xerror* E)
{
    if(!E)return;
    xstrview D=xrtErrorDomainView(E);size_t N=D.Size;if(N>63)N=63;
    xrtMutexLock(&P->Gate);P->Info.NativeErrorKind=xrtErrorKind(E);P->Info.NativeErrorCode=xrtErrorCode(E);P->Info.NativeErrorDomainTruncated=D.Size>N;
    if(N)memcpy(P->Info.NativeErrorDomain,D.Data,N);
    P->Info.NativeErrorDomain[N]=0;xrtMutexUnlock(&P->Gate);
}
static void xrpcWsTimerCancel(xrpcws* P)
{uint64 T=P->Timer;P->Timer=0;P->TimerAt=INFINITY;if(T)(void)xrtNetEngineTimerCancelCurrent(P->Engine,T);}
static void xrpcWsArm(xrpcws* P,double At)
{
    if(P->TimerUnavailable)return;
    if(At==INFINITY){xrpcWsTimerCancel(P);return;}
    if(P->Timer && P->TimerAt<=At)return;
    xrpcWsTimerCancel(P);xrpcWsRef(P);P->TimerAt=At;
    P->Timer=xrtNetEngineAfter(P->Engine,xrtNetWorkerIndex(P->Worker),xrpcWsRemaining(At),xrpcWsTimer,P);
    if(!P->Timer){P->TimerUnavailable=true;P->TimerAt=INFINITY;xrpcWsDrop(P);xrtMutexLock(&P->Gate);++P->Info.TimerFailures;xrtMutexUnlock(&P->Gate);xrpcWsFail(P,XRPC_WS_LIMIT);xrpcWsWake(P);}
}
static void xrpcWsTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xrpcws* P=Data;if(P->Timer==Id){P->Timer=0;P->TimerAt=INFINITY;
        if(Result!=XNET_RESULT_OK && Result!=XNET_RESULT_CANCELLED)xrpcWsFail(P,XRPC_WS_ENGINE);
        xrpcWsWake(P);}
    xrpcWsDrop(P);
}
static void xrpcWsInputStats(xrpcws* P)
{xrtMutexLock(&P->Gate);P->Info.ReceiveBytes=P->Used;P->Info.ReceiveCapacity=P->Capacity;xrtMutexUnlock(&P->Gate);}
static void xrpcWsReceiveClear(xrpcws* P,bool All)
{P->Used=0;P->MessageActive=P->Complete=false;P->MessageEntered=0;if(All || P->Capacity>P->Config.KeepReceiveBytes){xrtFree(P->Receive);P->Receive=NULL;P->Capacity=0;}xrpcWsInputStats(P);}
static void xrpcWsBegin(xwsstream* S,const xwsmessageinfo* M,ptr Data)
{
    xrpcws* P=Data;if(xrpcWsClosing(P)){xrtWsStreamPause(S);return;}
    if(P->MessageActive || (M->Opcode!=XWS_OPCODE_TEXT && (!P->Config.AllowBinary || M->Opcode!=XWS_OPCODE_BINARY))){xrpcWsFail(P,XRPC_WS_PROTOCOL);}
    else {P->MessageActive=true;P->MessageEntered=xrtTimer();P->Used=0;}
    if(xrpcWsClosing(P))xrtWsStreamPause(S);
    xrpcWsWake(P);
}
static void xrpcWsData(xwsstream* S,xbytesview Bytes,ptr Data)
{
    xrpcws* P=Data;if(xrpcWsClosing(P)){xrtWsStreamPause(S);return;}
    if(!P->MessageActive || Bytes.Size>P->Config.MaxMessageBytes-P->Used){xrpcWsFail(P,XRPC_WS_LIMIT);goto Done;}
    size_t Need=P->Used+Bytes.Size;
    if(Need>P->Capacity){
        size_t Capacity=P->Capacity?P->Capacity:256;
        while(Capacity<Need){if(Capacity>P->Config.MaxMessageBytes/2){Capacity=P->Config.MaxMessageBytes;break;}Capacity*=2;}
        if(Capacity>P->Config.MaxMessageBytes)Capacity=P->Config.MaxMessageBytes;
        if(Capacity<Need || Capacity>P->Config.MaxReceiveBytes-P->Capacity){xrpcWsFail(P,XRPC_WS_LIMIT);goto Done;}
        unsigned char* Next=xrtMalloc(Capacity);if(!Next){xrpcWsFail(P,XRPC_WS_MEMORY);goto Done;}
        if(P->Used)memcpy(Next,P->Receive,P->Used);
        xrtMutexLock(&P->Gate);++P->Info.ReceiveAllocations;size_t Peak=P->Capacity+Capacity;if(Peak>P->Info.PeakReceiveBytes)P->Info.PeakReceiveBytes=Peak;xrtMutexUnlock(&P->Gate);
        xrtFree(P->Receive);P->Receive=Next;P->Capacity=Capacity;
    }
    if(Bytes.Size)memcpy(P->Receive+P->Used,Bytes.Data,Bytes.Size);
    P->Used=Need;P->ReadTurnBytes+=Bytes.Size;xrpcWsInputStats(P);
    xrtMutexLock(&P->Gate);P->Info.ReadBytes+=Bytes.Size;xrtMutexUnlock(&P->Gate);
Done:
    if(xrpcWsClosing(P) || P->ReadTurnBytes>=P->Config.ReadBytes)xrtWsStreamPause(S);
    xrpcWsWake(P);
}
static void xrpcWsEnd(xwsstream* S,ptr Data)
{xrpcws* P=Data;if(!xrpcWsClosing(P)){if(!P->MessageActive)xrpcWsFail(P,XRPC_WS_PROTOCOL);else P->Complete=true;}xrtWsStreamPause(S);xrpcWsWake(P);}
static void xrpcWsWritable(xwsstream* S,size_t Pending,ptr Data){(void)S;(void)Pending;xrpcWsWake(Data);}
static void xrpcWsDrain(xwsstream* S,ptr Data){(void)S;xrpcWsWake(Data);}
static void xrpcWsError(xwsstream* S,const xerror* Error,ptr Data)
{
    (void)S;xrpcws* P=Data;if(P->NativeFailed)return;P->NativeFailed=true;xrpcWsNativeError(P,Error);xerrkind K=xrtErrorKind(Error);
    P->NativeReason=K==XERR_MEMORY?XRPC_WS_MEMORY:K==XERR_RANGE?XRPC_WS_LIMIT:K==XERR_TIMEOUT?XRPC_WS_TIMEOUT:K==XERR_PROTOCOL?XRPC_WS_PROTOCOL:XRPC_WS_TRANSPORT;
    xrpcWsWake(P);
}
static void xrpcWsClosed(xwsstream* S,const xwsstreamclose* Close,ptr Data)
{
    (void)S;xrpcws* P=Data;P->NativeClosed=true;xrtMutexLock(&P->Gate);
    if(Close){P->Info.NativeCloseFlags=Close->Flags;P->Info.LocalCode=Close->LocalCode;P->Info.RemoteCode=Close->RemoteCode;}xrtMutexUnlock(&P->Gate);xrpcWsWake(P);
}
static void xrpcWsEventsDrop(const void* Data)
{xrpcws* P=(xrpcws*)Data;P->EventsGone=true;if(!P->DestroyingNative)xrpcWsWake(P);xrpcWsDrop(P);}
static const xwsstreamownershipv1 xrpcWsEvents={sizeof(xwsstreamownershipv1),{.MessageBegin=xrpcWsBegin,.MessageData=xrpcWsData,.MessageEnd=xrpcWsEnd,.Writable=xrpcWsWritable,.Drain=xrpcWsDrain,.Error=xrpcWsError,.Close=xrpcWsClosed},xrpcWsEventsDrop,&xrpcWsOps,NULL};
static void xrpcWsOutputClear(xrpcws* P)
{if(P->Output)xrpcDuplexOutputRelease(P->Output);P->Output=NULL;P->Token=0;P->Offered=false;}
static void xrpcWsWireDrop(ptr Data,cbytes Bytes,size_t Size)
{(void)Bytes;(void)Size;xrpcDuplexOutputRelease(Data);}
static void xrpcWsWrite(xrpcws* P)
{
    size_t Budget=P->Config.WriteBytes,Units=P->Config.WorkMessages;
    while(Budget && Units-- && !xrpcWsClosing(P)){
        xrpcduplexinfo D={0};D.Size=sizeof(D);if(!xrpcDuplexInfo(P->Duplex,&D)){xrpcWsFail(P,XRPC_WS_PROTOCOL);return;}
        if(P->Output && !D.Selected)xrpcWsOutputClear(P);
        if(!P->Output){int R=xrpcDuplexNext(P->Duplex,&P->Output,&P->Token);if(R<0){xrpcWsFail(P,xrpcWsLocalFailure());return;}if(!R)return;}
        xstrview Wire=xrpcDuplexOutputWire(P->Output);size_t Required=0;bool Compress=false;
        #if defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
        Compress=P->Config.CompressOutput&&P->Config.Stream.DeflateEnabled&&Wire.Size>=P->Config.CompressMinBytes;
        #endif
        if(Wire.Size>P->Config.MaxMessageBytes || !xrtWsStreamSendSize(P->Stream,Compress?0:Wire.Size,&Required) || Required>xrtWsStreamWriteLimit(P->Stream)){xrpcWsFail(P,XRPC_WS_LIMIT);return;}
        if(Wire.Size>Budget){xrpcWsWake(P);return;}
        if(Required>xrtWsStreamWritable(P->Stream))return;
        if(!P->Offered){if(!xrpcDuplexStarted(P->Duplex,P->Token)){xrpcWsWake(P);return;}P->Offered=true;}
        xnetresult R;
        #if defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
        if(Compress)R=xrtWsStreamSendCompressed(P->Stream,XWS_OPCODE_TEXT,(xbytesview){(cbytes)Wire.Data,Wire.Size});else
        #endif
        {
            if(!xrpcDuplexOutputRetain(P->Output)){xrpcWsFail(P,xrpcWsLocalFailure());return;}
            xnetref Ref={(cbytes)Wire.Data,Wire.Size,xrpcWsWireDrop,P->Output};R=xrtWsStreamSendRef(P->Stream,XWS_OPCODE_TEXT,&Ref);
            if(R!=XNET_RESULT_OK)xrpcDuplexOutputRelease(P->Output);
        }
        if(R!=XNET_RESULT_OK){if(R!=XNET_RESULT_AGAIN){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,R==XNET_RESULT_ERROR?xrpcWsLocalFailure():XRPC_WS_TRANSPORT);}return;}
        if(!xrpcDuplexSent(P->Duplex,P->Token)){xrpcWsFail(P,XRPC_WS_PROTOCOL);return;}
        Budget-=Wire.Size;xrtMutexLock(&P->Gate);P->Info.WriteBytes+=Wire.Size;++P->Info.WriteMessages;xrtMutexUnlock(&P->Gate);xrpcWsOutputClear(P);
    }
    if(!xrpcWsClosing(P) && xrtWsStreamWritable(P->Stream))xrpcWsWake(P);
}
static void xrpcWsWork(xnetworker* W,ptr Data)
{
    (void)W;xerror* Ambient=xrtTakeError();xrpcws* P=Data;xrtMutexLock(&P->Gate);P->Posted=false;bool Closed=P->Info.State==XRPC_WS_CLOSED;xrtMutexUnlock(&P->Gate);
    if(Closed)goto End;
    if(!xrpcDuplexStep(P->Duplex))xrpcWsFail(P,xrpcWsLocalFailure());
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP)
    if(P->Upgrade){xrpcWsHttpStep(P);
    xrtMutexLock(&P->Gate);bool Upgrading=P->Info.State==XRPC_WS_UPGRADING;xrtMutexUnlock(&P->Gate);
    if(Upgrading){xrpcduplexinfo U={0};U.Size=sizeof(U);xrpcDuplexInfo(P->Duplex,&U);
        if(U.Closing)xrpcWsFail(P,U.Reason==XRPC_CALL_PROTOCOL?XRPC_WS_PROTOCOL:U.Reason==XRPC_CALL_MEMORY?XRPC_WS_MEMORY:U.Reason==XRPC_CALL_LIMIT?XRPC_WS_LIMIT:XRPC_WS_TRANSPORT);
        if(!xrpcWsClosing(P)){double At=U.NextDeadline;if(P->Upgrade->Deadline<At)At=P->Upgrade->Deadline;if(U.More)xrpcWsWake(P);else xrpcWsArm(P,At);xrtMutexLock(&P->Gate);P->Info.Duplex=U;xrtMutexUnlock(&P->Gate);goto End;}
    }
    }
    #endif
    if(!xrpcWsClosing(P) && !P->NativeFailed && P->MessageActive && xrtTimer()>=xrpcWsDeadline(P->MessageEntered,P->Config.MessageTimeout))xrpcWsFail(P,XRPC_WS_TIMEOUT);
    if(!xrpcWsClosing(P) && P->Complete){
        xrpcduplexadmission A=xrpcDuplexReceiveEntered(P->Duplex,(xstrview){(cstr)P->Receive,P->Used},P->Config.RequestContext,P->MessageEntered);
        if(A!=XRPC_DUPLEX_ACCEPTED){if(A==XRPC_DUPLEX_OVERLOADED){xrtMutexLock(&P->Gate);++P->Info.Overloaded;xrtMutexUnlock(&P->Gate);}xrpcWsFail(P,A==XRPC_DUPLEX_OVERLOADED?XRPC_WS_LIMIT:A==XRPC_DUPLEX_PROTOCOL?XRPC_WS_PROTOCOL:xrpcWsLocalFailure());}
        else {xrtMutexLock(&P->Gate);++P->Info.ReadMessages;xrtMutexUnlock(&P->Gate);}
        xrpcWsReceiveClear(P,false);if(!xrpcDuplexStep(P->Duplex))xrpcWsFail(P,xrpcWsLocalFailure());
    }
    if((P->NativeFailed || P->NativeClosed) && !xrpcWsClosing(P))xrpcWsFail(P,P->NativeReason);
    xrpcduplexinfo D={0};D.Size=sizeof(D);xrpcDuplexInfo(P->Duplex,&D);
    if(D.Closing && !xrpcWsClosing(P))xrpcWsFail(P,D.Reason==XRPC_CALL_PROTOCOL?XRPC_WS_PROTOCOL:D.Reason==XRPC_CALL_LIMIT?XRPC_WS_LIMIT:D.Reason==XRPC_CALL_MEMORY?XRPC_WS_MEMORY:XRPC_WS_TRANSPORT);
    if(!xrpcWsClosing(P))xrpcWsWrite(P);
    xrpcDuplexInfo(P->Duplex,&D);if(D.Closing && !xrpcWsClosing(P))xrpcWsFail(P,D.Reason==XRPC_CALL_PROTOCOL?XRPC_WS_PROTOCOL:D.Reason==XRPC_CALL_MEMORY?XRPC_WS_MEMORY:D.Reason==XRPC_CALL_LIMIT?XRPC_WS_LIMIT:XRPC_WS_TRANSPORT);
    if(xrpcWsClosing(P)){
        xrpcWsTimerCancel(P);xrpcWsOutputClear(P);xrpcWsReceiveClear(P,true);
        #if defined(XRPC_FEATURE_XRPC_WS_HTTP)
        bool HttpRetired=xrpcWsHttpRetire(P);
        #endif
        if(P->Stream && !P->AbortIssued){P->AbortIssued=true;if(xrtWsStreamState(P->Stream)!=XWS_STREAM_CLOSED)(void)xrtWsStreamAbort(P->Stream);}
        if(P->Stream && xrtWsStreamPollRetired(P->Stream)==XWAIT_OK){xwsstream* S=P->Stream;P->Stream=NULL;P->DestroyingNative=true;xrtWsStreamDestroy(S);P->DestroyingNative=false;}
        xrtMutexLock(&P->Gate);xrpcwsreason Reason=P->Info.Reason;size_t Entries=P->Info.Submitters;xrtMutexUnlock(&P->Gate);
        if(!P->DuplexClosing){xrpccallstatus Status=xrpcWsCallFailure(Reason);
            #if defined(XRPC_FEATURE_XRPC_WS_HTTP)
            if(Reason==XRPC_WS_TIMEOUT&&P->UpgradeInfo.Size&&P->UpgradeInfo.Outcome!=XRPC_WS_HTTP_OPEN)Status=XRPC_CALL_TIMEOUT;
            #endif
            if(xrpcDuplexClose(P->Duplex,Status))P->DuplexClosing=true;else{xrpcWsNativeError(P,xrtGetError());xrpcWsWake(P);}}
        (void)xrpcDuplexStep(P->Duplex);xrpcDuplexInfo(P->Duplex,&D);
        bool Done=!Entries&&D.Drained&&!P->Stream&&P->EventsGone;
        #if defined(XRPC_FEATURE_XRPC_WS_HTTP)
        Done=Done&&HttpRetired;
        #endif
        if(Done){xrtMutexLock(&P->Gate);Done=P->Info.Submitters==0;xrtMutexUnlock(&P->Gate);}
        if(Done && xrpcDuplexDestroy(P->Duplex)){P->Duplex=NULL;xrtMutexLock(&P->Gate);P->Info.State=XRPC_WS_CLOSED;xrtMutexUnlock(&P->Gate);xrpcWsDrop(P);}
        else if(D.More)xrpcWsWake(P);
        else {double At=D.NextDeadline;bool NativePending=P->Stream!=NULL;
            #if defined(XRPC_FEATURE_XRPC_WS_HTTP)
            NativePending=NativePending||P->Upgrade!=NULL;
            #endif
            if(NativePending){if(!P->CleanupDelay)P->CleanupDelay=1;else if(P->CleanupDelay<32)P->CleanupDelay*=2;double Retry=xrpcWsDeadline(xrtTimer(),P->CleanupDelay);if(Retry<At)At=Retry;}xrpcWsArm(P,At);}
    }else {
        P->ReadTurnBytes=0;
        if(xrtWsStreamPaused(P->Stream) && !xrtWsStreamResume(P->Stream)){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());xrpcWsWake(P);}
        double At=D.NextDeadline;if(P->MessageActive){double Deadline=xrpcWsDeadline(P->MessageEntered,P->Config.MessageTimeout);if(Deadline<At)At=Deadline;}
        if(D.More)xrpcWsWake(P);else xrpcWsArm(P,At);
    }
    xrtMutexLock(&P->Gate);P->Info.Duplex=D;xrtMutexUnlock(&P->Gate);
End:
    xrtClearError();xrtSetErrorTake(Ambient);xrpcWsDrop(P);
}
XRPC_API bool xrpcWsConfigInit(xrpcwsconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcWsReport(XERR_ARGUMENT,"invalid WS config output");
    *C=(xrpcwsconfig){0};C->Size=sizeof(*C);xrpcDuplexConfigInit(&C->Duplex);xrtWsStreamConfigInit(&C->Stream);C->MaxMessageBytes=1048576;C->MaxReceiveBytes=3145728;C->KeepReceiveBytes=4096;C->ReadBytes=65536;C->WriteBytes=1048576;C->WorkMessages=32;C->MaxSubmitters=128;C->MessageTimeout=30000;C->CompressMinBytes=256;return true;
}
static void xrpcWsFree(xrpcws* P)
{
    if(P->Duplex){(void)xrpcDuplexClose(P->Duplex,XRPC_CALL_CLOSED);(void)xrpcDuplexStep(P->Duplex);(void)xrpcDuplexDestroy(P->Duplex);}
    xrtWsStreamDestroy(P->Stream);xrtFree(P->Receive);xrtSecureZero((ptr)P->Config.RequestContext.Data,P->Config.RequestContext.Size);xrtCondUnit(&P->Retired);xrtMutexUnit(&P->Retirement);xrtMutexUnit(&P->Gate);(void)xrtNetEngineUnpin(P->Engine);xrtFree(P);
}
static xrpcws* xrpcWsCreate(xnetworker* W,const xrpcwsconfig* Config,xnetbuf* Initial,xrpcws** Out)
{
    if(!W || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (Initial && (!xrtMemRangeValid(Initial,sizeof(*Initial))||xrtMemRangesOverlap(Initial,sizeof(*Initial),Out,sizeof(*Out)))) || (Config&&(!xrtMemRangeValid(Config,sizeof(*Config))||xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)))))goto Argument;
    xrpcwsconfig C={0};if(Config)C=*Config;else if(!xrpcWsConfigInit(&C))return NULL;
    if(C.Size!=sizeof(C)||C.Duplex.Wake||C.Duplex.User||!C.MaxMessageBytes||C.MaxMessageBytes>16777216||C.MaxReceiveBytes<C.MaxMessageBytes||C.MaxReceiveBytes>33554432||C.KeepReceiveBytes>C.MaxMessageBytes||!C.ReadBytes||C.ReadBytes>16777216||C.WriteBytes<C.MaxMessageBytes||C.WriteBytes>33554432||!C.WorkMessages||C.WorkMessages>65536||!C.MaxSubmitters||C.MaxSubmitters>65536||C.MessageTimeout<XRT_WAIT_FOREVER||C.RequestContext.Size>16384||!xrtMemRangeValid(C.RequestContext.Data,C.RequestContext.Size)||xrtMemRangesOverlap(C.RequestContext.Data,C.RequestContext.Size,Out,sizeof(*Out))||C.Stream.Protocol.Size>255||xrtMemRangesOverlap(C.Stream.Protocol.Data,C.Stream.Protocol.Size,Out,sizeof(*Out))||!xrtWsStreamConfigValid(&C.Stream))goto Argument;
    #if !defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
    if(C.CompressOutput)goto Argument;
    #endif
    if(C.Stream.Inflater.OutputLimit>C.MaxMessageBytes)C.Stream.Inflater.OutputLimit=C.MaxMessageBytes;
    xnetengine* E=xrtNetWorkerEngine(W);if(!xrtNetEnginePin(E))return NULL;
    size_t Bytes=sizeof(xrpcws)+C.RequestContext.Size;xrpcws* P=xrtCalloc(1,Bytes);if(!P){xrtNetEngineUnpin(E);return NULL;}
    P->Engine=E;P->Worker=W;P->Config=C;P->TimerAt=INFINITY;P->NativeReason=XRPC_WS_TRANSPORT;P->EventsGone=true;P->Info.Size=sizeof(P->Info);P->Info.FixedBytes=Bytes;xrtAtomic32Init(&P->References,1);
    if(!xrtMutexInit(&P->Gate)){xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    if(!xrtMutexInit(&P->Retirement)){xrtMutexUnit(&P->Gate);xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    if(!xrtCondInit(&P->Retired)){xrtMutexUnit(&P->Retirement);xrtMutexUnit(&P->Gate);xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    P->Config.RequestContext=(xbytesview){P->Context,C.RequestContext.Size};if(C.RequestContext.Size)memcpy(P->Context,C.RequestContext.Data,C.RequestContext.Size);
    C.Duplex.Wake=xrpcWsWake;C.Duplex.User=P;if(!xrtNetPostInit(&P->Post)||!xrpcDuplexCreate(&C.Duplex,&P->Duplex)){xerror* Error=xrtTakeError();xrpcWsFree(P);xrtClearError();xrtSetErrorTake(Error);return NULL;}
    P->Info.Duplex.Size=sizeof(P->Info.Duplex);xrpcDuplexInfo(P->Duplex,&P->Info.Duplex);return P;
Argument:
    xrpcWsReport(XERR_ARGUMENT,"invalid WS config/initial/output ranges or bounds");return NULL;
}
static bool xrpcWsAttach(ptr Native,bool TLS,xnetbuf* Initial,const xrpcwsconfig* Config,xrpcws** Out)
{
    if(!Native)return xrpcWsReport(XERR_ARGUMENT,"WS transport is NULL");
    xnetworker* W=NULL;
    #if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(TLS){xnetstream* Raw=xrtTlsStreamTransport(Native);W=Raw?xrtNetStreamWorker(Raw):NULL;if(xrtTlsStreamState(Native)!=XTLS_STREAM_OPEN)return xrpcWsReport(XERR_STATE,"WSS requires verified OPEN TLS");}else
    #else
    (void)TLS;
    #endif
    {W=xrtNetStreamWorker(Native);if(xrtNetStreamState(Native)!=XNET_STREAM_OPEN)return xrpcWsReport(XERR_STATE,"WS requires OPEN TCP");}
    if(!xrtNetWorkerIsCurrent(W))return xrpcWsReport(XERR_STATE,"WS attach requires exclusive transport Worker");
    xrpcws* P=xrpcWsCreate(W,Config,Initial,Out);if(!P)return false;xwsstreamconfig K=P->Config.Stream;if(K.MessageLimit>P->Config.MaxMessageBytes)K.MessageLimit=P->Config.MaxMessageBytes;if(K.FrameLimit>P->Config.MaxMessageBytes)K.FrameLimit=P->Config.MaxMessageBytes;
    xrpcWsRef(P);P->EventsGone=false;
    #if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(TLS){xtlssession* Session=xrtTlsStreamSession(Native);P->Info.Encrypted=true;P->Info.Version=xrtTlsSessionVersion(Session);P->Info.Cipher=xrtTlsSessionCipher(Session);P->Stream=xrtWsStreamAttachTlsOwnedV1(Native,Initial,&K,P,&xrpcWsEvents);}else
    #endif
    P->Stream=xrtWsStreamAttachOwnedV1(Native,Initial,&K,P,&xrpcWsEvents);
    if(!P->Stream){xerror* Error=xrtTakeError();P->EventsGone=true;xrpcWsDrop(P);xrpcWsFree(P);xrtClearError();xrtSetErrorTake(Error);return false;}
    P->Config.Stream.Protocol=xrtWsStreamProtocol(P->Stream);P->Info.ProtocolBytes=P->Config.Stream.Protocol.Size;if(P->Info.ProtocolBytes)memcpy(P->Info.Protocol,P->Config.Stream.Protocol.Data,P->Info.ProtocolBytes);
    xrtMutexLock(&P->Gate);P->Active=true;P->Info.State=XRPC_WS_OPEN;
    #if defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
    P->DeflateReady=true;
    #endif
    xrpcWsRef(P);*Out=P;if(!xrpcWsWakeLocked(P)){P->Info.State=XRPC_WS_CLOSING;P->Info.Reason=XRPC_WS_LIMIT;}xrtMutexUnlock(&P->Gate);return true;
}
XRPC_API bool xrpcWsAttachTcp(xnetstream* S,xnetbuf* Initial,const xrpcwsconfig* C,xrpcws** Out){return xrpcWsAttach(S,false,Initial,C,Out);}
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
XRPC_API bool xrpcWsAttachTls(xtlsstream* S,xnetbuf* Initial,const xrpcwsconfig* C,xrpcws** Out){return xrpcWsAttach(S,true,Initial,C,Out);}
#endif
static xrpcduplexadmission xrpcWsSubmit(xrpcws* P,const xrpcduplexitem* Items,size_t Count,bool Batch,int64 Timeout,xfuture** Future,double Entered)
{
    if(!P || !Future || !xrtMemRangeValid(Future,sizeof(*Future)) || xrtMemRangesOverlap(P,P->Info.FixedBytes,Future,sizeof(*Future)))return xrpcWsReport(XERR_ARGUMENT,"invalid RPC WS submission output"),XRPC_DUPLEX_ERROR;
    xrtMutexLock(&P->Gate);if(xrpcWsTerminal(P->Info.State)){xrtMutexUnlock(&P->Gate);return XRPC_DUPLEX_CLOSED;}
    if(P->Info.Submitters==P->Config.MaxSubmitters){++P->Info.Overloaded;xrtMutexUnlock(&P->Gate);return XRPC_DUPLEX_OVERLOADED;}
    ++P->Info.Submitters;xrpcWsRef(P);xrtMutexUnlock(&P->Gate);
    xrpcduplexadmission A=xrpcDuplexSubmitEntered(P->Duplex,Items,Count,Batch,Timeout,Future,Entered);
    xerror* Error=xrtTakeError();xrtMutexLock(&P->Gate);--P->Info.Submitters;if(xrpcWsTerminal(P->Info.State))(void)xrpcWsWakeLocked(P);xrtMutexUnlock(&P->Gate);xrpcWsDrop(P);xrtClearError();xrtSetErrorTake(Error);return A;
}
XRPC_API xrpcduplexadmission xrpcWsCall(xrpcws* P,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double E=xrtTimer();xrpcduplexitem I={false,Method,Params};return xrpcWsSubmit(P,&I,1,false,Timeout,Future,E);}
XRPC_API xrpcduplexadmission xrpcWsNotify(xrpcws* P,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double E=xrtTimer();xrpcduplexitem I={true,Method,Params};return xrpcWsSubmit(P,&I,1,false,Timeout,Future,E);}
XRPC_API xrpcduplexadmission xrpcWsBatch(xrpcws* P,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** Future)
{return xrpcWsSubmit(P,Items,Count,true,Timeout,Future,xrtTimer());}
XRPC_API bool xrpcWsClose(xrpcws* P)
{if(!P)return xrpcWsReport(XERR_ARGUMENT,"RPC WS peer is NULL");xrpcWsFail(P,XRPC_WS_LOCAL);xrtMutexLock(&P->Gate);
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP)
    P->UpgradeAbort=true;
    #endif
    bool OK=xrpcWsWakeLocked(P);xrtMutexUnlock(&P->Gate);return OK;}
XRPC_API bool xrpcWsInfo(xrpcws* P,xrpcwsinfo* I)
{
    if(!P || !I || !xrtMemRangeValid(I,sizeof(*I)) || I->Size!=sizeof(*I) || xrtMemRangesOverlap(P,P->Info.FixedBytes,I,sizeof(*I)))return xrpcWsReport(XERR_ARGUMENT,"invalid RPC WS info output");
    xrtMutexLock(&P->Gate);*I=P->Info;xrtMutexUnlock(&P->Gate);return true;
}
#if defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
XRPC_API bool xrpcWsDeflateInfo(xrpcws* P,xrpcwsdeflateinfo* I)
{
    if(!P||!I||!xrtMemRangeValid(I,sizeof(*I))||I->Size!=sizeof(*I)||xrtMemRangesOverlap(P,P->Info.FixedBytes,I,sizeof(*I)))return xrpcWsReport(XERR_ARGUMENT,"invalid RPC WS deflate info");
    xrtMutexLock(&P->Gate);bool Ready=P->DeflateReady;
    if(Ready)*I=(xrpcwsdeflateinfo){.Size=sizeof(*I),.Negotiated=P->Config.Stream.Deflate,.CompressMinBytes=P->Config.CompressMinBytes,.Enabled=P->Config.Stream.DeflateEnabled,.CompressOutput=P->Config.CompressOutput};
    xrtMutexUnlock(&P->Gate);return Ready?true:xrpcWsReport(XERR_STATE,"RPC WS negotiation is not complete");
}
#endif
XRPC_API xwaitresult xrpcWsPollRetired(xrpcws* P)
{
    if(!P){xrpcWsReport(XERR_ARGUMENT,"RPC WS peer is NULL");return XWAIT_ERROR;}
    xmutextryresult L=xrtMutexTryLockResult(&P->Retirement);if(L!=XMUTEX_TRY_OK)return L==XMUTEX_TRY_BUSY?XWAIT_TIMEOUT:XWAIT_ERROR;
    bool Alone=xrtAtomic32Load(&P->References,XMEMORY_ACQUIRE)==1;xrtMutexUnlock(&P->Retirement);
    L=xrtMutexTryLockResult(&P->Gate);if(L!=XMUTEX_TRY_OK)return L==XMUTEX_TRY_BUSY?XWAIT_TIMEOUT:XWAIT_ERROR;
    bool Closed=P->Info.State==XRPC_WS_CLOSED;if(!Closed && P->Info.State==XRPC_WS_CLOSING)(void)xrpcWsWakeLocked(P);xrtMutexUnlock(&P->Gate);
    if(!Alone || !Closed)return XWAIT_TIMEOUT;
    return XWAIT_OK;
}
XRPC_API xwaitresult xrpcWsWaitFor(xrpcws* P,int64 Timeout)
{
    if(!P || Timeout<XRT_WAIT_FOREVER){xrpcWsReport(XERR_ARGUMENT,"invalid RPC WS wait");return XWAIT_ERROR;}
    if(xrtNetEngineCurrent(P->Engine)){xrpcWsReport(XERR_STATE,"RPC WS wait on own engine worker");return XWAIT_ERROR;}
    double At=xrpcWsDeadline(xrtTimer(),Timeout);
    for(;;){xwaitresult R=xrpcWsPollRetired(P);if(R!=XWAIT_TIMEOUT)return R;int64 Left=xrpcWsRemaining(At);if(!Left)return XWAIT_TIMEOUT;
        if(Left==XRT_WAIT_FOREVER || Left>1)Left=1;
        xrtMutexLock(&P->Retirement);(void)xrtCondWaitFor(&P->Retired,&P->Retirement,Left);xrtMutexUnlock(&P->Retirement);}
}
XRPC_API bool xrpcWsDestroy(xrpcws* P)
{if(!P)return true;if(xrpcWsPollRetired(P)!=XWAIT_OK)return xrpcWsReport(XERR_STATE,"RPC WS peer still has physical duties");xrpcWsFree(P);return true;}
#if defined(XRPC_FEATURE_XRPC_WS_HTTP)
static xnetstream* xrpcWsHttpRaw(xrpcwsupgrade* H)
{
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(H->TLS)return xrtTlsStreamTransport(H->Native);
    #endif
    return H->Native;
}
static const xnetbuf* xrpcWsHttpBuffer(xrpcwsupgrade* H)
{
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(H->TLS)return xrtTlsStreamBuffer(H->Native);
    #endif
    return xrtNetStreamBuffer(H->Native);
}
static bool xrpcWsHttpConsume(xrpcwsupgrade* H,size_t N)
{
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(H->TLS)return xrtTlsStreamConsume(H->Native,N);
    #endif
    return xrtNetStreamConsume(H->Native,N)==N;
}
static bool xrpcWsHttpEqual(xstrview A,xstrview B,bool Fold)
{
    if(A.Size!=B.Size)return false;
    for(size_t I=0;I<A.Size;++I){unsigned char X=(unsigned char)A.Data[I],Y=(unsigned char)B.Data[I];if(Fold){if(X>='A'&&X<='Z')X+=32;if(Y>='A'&&Y<='Z')Y+=32;}if(X!=Y)return false;}
    return true;
}
static bool xrpcWsHttpName(xstrview Name,cstr Literal)
{return xrpcWsHttpEqual(Name,(xstrview){Literal,strlen(Literal)},true);}
static bool xrpcWsHttpManaged(xstrview Name)
{
    static const cstr Names[]={"Host","Connection","Upgrade","Content-Length","Transfer-Encoding","Expect","Trailer","Sec-WebSocket-Key","Sec-WebSocket-Accept","Sec-WebSocket-Version","Sec-WebSocket-Protocol","Sec-WebSocket-Extensions","Origin","WWW-Authenticate"};
    for(size_t I=0;I<sizeof(Names)/sizeof(*Names);++I)if(xrpcWsHttpName(Name,Names[I]))return true;
    return false;
}
static void xrpcWsHttpCostAdd(xrpcws* P,size_t N)
{
    xrtMutexLock(&P->Gate);P->UpgradeInfo.ColdBytes+=N;if(P->UpgradeInfo.ColdBytes>P->UpgradeInfo.PeakColdBytes)P->UpgradeInfo.PeakColdBytes=P->UpgradeInfo.ColdBytes;xrtMutexUnlock(&P->Gate);
}
static void xrpcWsHttpCostDrop(xrpcws* P,size_t N)
{xrtMutexLock(&P->Gate);P->UpgradeInfo.ColdBytes-=N;xrtMutexUnlock(&P->Gate);}
static void xrpcWsHttpRead(xnetstream* S,xnetbuf* B,ptr Data)
{(void)S;(void)B;xrpcWsWake(Data);}
static void xrpcWsHttpWritable(xnetstream* S,ptr Data)
{(void)S;xrpcWsWake(Data);}
static void xrpcWsHttpLowWater(xnetstream* S,size_t N,ptr Data)
{(void)S;(void)N;xrpcWsWake(Data);}
static void xrpcWsHttpEnd(xnetstream* S,ptr Data)
{(void)S;xrpcws* P=Data;if(P->Upgrade)P->Upgrade->InputEnded=true;xrpcWsWake(P);}
static void xrpcWsHttpClosed(xnetstream* S,xnetresult R,const xerror* E,ptr Data)
{(void)S;(void)R;xrpcws* P=Data;xrpcWsNativeError(P,E);if(P->Upgrade){P->Upgrade->NativeClosed=true;
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
    if(P->Upgrade->Establishing)xrpcWsFail(P,R==XNET_RESULT_TIMEOUT||xrtErrorKind(E)==XERR_TIMEOUT?XRPC_WS_TIMEOUT:xrtErrorKind(E)==XERR_MEMORY?XRPC_WS_MEMORY:XRPC_WS_TRANSPORT);
    #endif
    }xrpcWsWake(P);}
static void xrpcWsHttpEventsDrop(const void* Data)
{xrpcws* P=(xrpcws*)Data;if(P->Upgrade)P->Upgrade->EventGone=true;xrpcWsWake(P);xrpcWsDrop(P);}
#if defined(XRPC_FEATURE_XRPC_WS_DIAL)
static void xrpcWsHttpOpened(xnetstream* S,ptr Data)
{(void)S;xrpcws* P=Data;if(P->Upgrade)P->Upgrade->Establishing=false;xrpcWsWake(P);}
#endif
static const xnetstreameventsownershipv1 xrpcWsHttpTcpEvents={sizeof(xnetstreameventsownershipv1),{
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
    .Open=xrpcWsHttpOpened,
    #endif
    .Read=xrpcWsHttpRead,.End=xrpcWsHttpEnd,.LowWater=xrpcWsHttpLowWater,.Drain=xrpcWsHttpWritable,.Close=xrpcWsHttpClosed},xrpcWsHttpEventsDrop,&xrpcWsOps};
#if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
static void xrpcWsHttpTlsRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{(void)S;(void)B;xrpcWsWake(Data);}
static void xrpcWsHttpTlsWritable(xtlsstream* S,ptr Data)
{(void)S;xrpcWsWake(Data);}
static void xrpcWsHttpTlsEnd(xtlsstream* S,ptr Data)
{(void)S;xrpcws* P=Data;if(P->Upgrade)P->Upgrade->InputEnded=true;xrpcWsWake(P);}
static void xrpcWsHttpTlsClosed(xtlsstream* S,xnetresult R,const xerror* E,ptr Data)
{(void)S;(void)R;xrpcws* P=Data;xrpcWsNativeError(P,E);if(P->Upgrade){P->Upgrade->NativeClosed=true;
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
    if(P->Upgrade->Establishing)xrpcWsFail(P,R==XNET_RESULT_TIMEOUT||xrtErrorKind(E)==XERR_TIMEOUT?XRPC_WS_TIMEOUT:xrtErrorKind(E)==XERR_MEMORY?XRPC_WS_MEMORY:XRPC_WS_TRANSPORT);
    #endif
    }xrpcWsWake(P);}
#if defined(XRPC_FEATURE_XRPC_WS_DIAL)
static void xrpcWsHttpTlsOpened(xtlsstream* S,ptr Data)
{
    xrpcws* P=Data;if(P->Upgrade)P->Upgrade->Establishing=false;xtlssession* Session=xrtTlsStreamSession(S);xbytesview Protocol={0};(void)xrtTlsSessionProtocol(Session,&Protocol);
    xrtMutexLock(&P->Gate);P->Info.Encrypted=true;P->Info.Version=xrtTlsSessionVersion(Session);P->Info.Cipher=xrtTlsSessionCipher(Session);xrtMutexUnlock(&P->Gate);
    if(Protocol.Size&&(Protocol.Size!=8||memcmp(Protocol.Data,"http/1.1",8)))xrpcWsFail(P,XRPC_WS_PROTOCOL);
    xrpcWsWake(P);
}
#endif
static const xtlsstreamownershipv1 xrpcWsHttpTlsEvents={sizeof(xtlsstreamownershipv1),{
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
    .Open=xrpcWsHttpTlsOpened,
    #endif
    .Read=xrpcWsHttpTlsRead,.End=xrpcWsHttpTlsEnd,.Writable=xrpcWsHttpTlsWritable,.Drain=xrpcWsHttpTlsWritable,.Close=xrpcWsHttpTlsClosed},xrpcWsHttpEventsDrop,&xrpcWsOps};
#endif
static void xrpcWsHttpAuthRelease(xrpcwsauthorization* A)
{
    if(!A || xrtAtomic32FetchSub(&A->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrpcws* P=A->Peer;size_t N=A->Bytes;xrtCancelDestroy(A->Work.Cancel);xrtSecureZero(A,N);xrtFree(A);xrpcWsHttpCostDrop(P,N);xrpcWsDrop(P);
}
static void xrpcWsHttpAuthCall(xrpcserverwork* Work,xrpcserverstatus Status)
{
    xrpcwsauthorization* A=(xrpcwsauthorization*)Work;A->Status=503;if(Status!=XRPC_SERVER_DONE)return;
    size_t Used=0;xrpcwshttpauthstatus R=A->Call(&A->Request,A->Context,A->Capacity,&Used,A->User);
    if(xrtTimer()>=Work->Deadline || xrtCancelRequested(Work->Cancel))return;
    if((unsigned)R>(unsigned)XRPC_WS_HTTP_AUTH_ERROR || Used>A->Capacity){A->Status=500;return;}
    A->Status=R==XRPC_WS_HTTP_ALLOW?200:R==XRPC_WS_HTTP_UNAUTHORIZED?401:R==XRPC_WS_HTTP_FORBIDDEN?403:503;
    if(R==XRPC_WS_HTTP_ALLOW)A->ContextBytes=Used;
}
static void xrpcWsHttpAuthDrop(xrpcserverwork* Work)
{
    xrpcwsauthorization* A=(xrpcwsauthorization*)Work;
    xrtAtomic32Store(&A->Ready,1,XMEMORY_RELEASE);xrpcWsWake(A->Peer);xrpcWsHttpAuthRelease(A);
}
static xstrview xrpcWsHttpRelocate(xstrview V,cbytes From,cbytes To)
{return V.Size?(xstrview){(cstr)(To+(V.Data-(cstr)From)),V.Size}:(xstrview){0};}
static bool xrpcWsHttpAuthorize(xrpcws* P,const xhttp1head* Head)
{
    xrpcwsupgrade* H=P->Upgrade;size_t Fields=Head->FieldCount*sizeof(xhttpfield);
    size_t N=sizeof(xrpcwsauthorization)+Fields+Head->Bytes+H->Config.MaxAuthorizationContextBytes;
    xrpcwsauthorization* A=xrtCalloc(1,N);if(!A)return false;
    xnetspan Span={0};if(H->Cache)Span=(xnetspan){H->Cache,H->CacheUsed};else if(!xrtNetBufFront(xrpcWsHttpBuffer(H),&Span)){xrtFree(A);return xrpcWsReport(XERR_STATE,"parsed HTTP head is not contiguous");}
    if(Span.Size<Head->Bytes){xrtFree(A);return xrpcWsReport(XERR_STATE,"parsed HTTP head exceeds its source");}
    xrtAtomic32Init(&A->References,1);xrtAtomic32Init(&A->Ready,0);A->Peer=P;xrpcWsRef(P);A->Bytes=N;xrpcWsHttpCostAdd(P,N);A->Call=H->Config.Authorize;A->User=H->Config.AuthorizationUser;A->Capacity=H->Config.MaxAuthorizationContextBytes;
    A->Head=*Head;A->Head.Fields=(xhttpfield*)(A+1);A->Head.FieldCapacity=Head->FieldCount;bytes Raw=(bytes)A->Head.Fields+Fields;memcpy(Raw,Span.Data,Head->Bytes);xrtMutexLock(&P->Gate);P->UpgradeInfo.HeaderCopyBytes+=Head->Bytes;xrtMutexUnlock(&P->Gate);
    A->Head.Method=xrpcWsHttpRelocate(Head->Method,Span.Data,Raw);A->Head.Target=xrpcWsHttpRelocate(Head->Target,Span.Data,Raw);A->Head.Reason=(xstrview){0};
    for(size_t I=0;I<Head->FieldCount;++I)A->Head.Fields[I]=(xhttpfield){xrpcWsHttpRelocate(Head->Fields[I].Name,Span.Data,Raw),xrpcWsHttpRelocate(Head->Fields[I].Value,Span.Data,Raw)};
    A->Context=Raw+Head->Bytes;A->Work.Bytes=N;A->Work.Deadline=H->Deadline;A->Work.Call=xrpcWsHttpAuthCall;A->Work.Drop=xrpcWsHttpAuthDrop;A->Work.Cancel=xrpcServerCancelChild(P->Config.Duplex.Server);
    if(!A->Work.Cancel){xrpcWsHttpAuthRelease(A);return false;}
    A->Request.Head=&A->Head;A->Request.Cancel=A->Work.Cancel;A->Request.Deadline=H->Deadline;A->Request.Secure=H->TLS;
    if(!xrtNetStreamRemote(xrpcWsHttpRaw(H),&A->Request.Remote)){xrpcWsHttpAuthRelease(A);return false;}
    H->Authorization=A;xrtAtomic32FetchAdd(&A->References,1,XMEMORY_RELAXED);
    if(xrpcServerWorkSubmit(P->Config.Duplex.Server,&A->Work)!=XRPC_SERVER_ACCEPTED){H->Authorization=NULL;xrpcWsHttpAuthRelease(A);xrpcWsHttpAuthRelease(A);return false;}
    xrtMutexLock(&P->Gate);P->UpgradeInfo.AuthorizationPending=true;xrtMutexUnlock(&P->Gate);return true;
}
static void xrpcWsHttpFreeControl(xrpcwsupgrade* H)
{if(H){if(H->Cache){xrtSecureZero(H->Cache,H->Config.Head.MaxHead);xrtFree(H->Cache);}size_t N=H->Bytes;xrtSecureZero(H,N);xrtFree(H);}}
static void xrpcWsHttpDiscard(xrpcws* P)
{xrpcwsupgrade* H=P->Upgrade;P->Upgrade=NULL;if(H){size_t N=H->Bytes+(H->Cache?H->Config.Head.MaxHead:0);xrpcWsHttpFreeControl(H);xrpcWsHttpCostDrop(P,N);}}
XRPC_API bool xrpcWsHttpConfigInit(xrpcwshttpconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcWsReport(XERR_ARGUMENT,"invalid WS HTTP config output");
    *C=(xrpcwshttpconfig){0};C->Size=sizeof(*C);xrtHttp1LimitsInit(&C->Head);C->Head.MaxHead=16384;if(C->Head.MaxStartLine>16384)C->Head.MaxStartLine=16384;if(C->Head.MaxFieldLine>16384)C->Head.MaxFieldLine=16384;if(C->Head.MaxFields>128)C->Head.MaxFields=128;
    C->MaxWriteBytes=32768;C->SetupTimeout=10000;C->Target=XRT_STR_LITERAL("/rpc");C->UnauthorizedChallenge=XRT_STR_LITERAL("Bearer");C->RequireProtocol=true;xrtWsUpgradeClientConfigInit(&C->Client);xrtWsUpgradeServerConfigInit(&C->Server);C->Client.Protocols=C->Server.Protocols=XRT_STR_LITERAL("jsonrpc");return true;
}
static xstrview xrpcWsHttpCopyView(xstrview V,bytes* At)
{if(!V.Size)return (xstrview){0};xstrview Out={(cstr)*At,V.Size};memcpy(*At,V.Data,V.Size);*At+=V.Size;return Out;}
static xrpcwsupgrade* xrpcWsHttpControl(const xrpcwshttpconfig* Config,bool Server,xrpcws** Out,double Entered)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (Config&&(!xrtMemRangeValid(Config,sizeof(*Config))||xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)))))goto Argument;
    xrpcwshttpconfig C={0};if(Config)C=*Config;else if(!xrpcWsHttpConfigInit(&C))return NULL;
    if(C.Size!=sizeof(C)||!C.Head.MaxHead||C.Head.MaxHead>16384||!C.Head.MaxStartLine||C.Head.MaxStartLine>C.Head.MaxHead||!C.Head.MaxFieldLine||C.Head.MaxFieldLine>C.Head.MaxHead||!C.Head.MaxFields||C.Head.MaxFields>128||C.MaxWriteBytes<256||C.MaxWriteBytes>65536||C.SetupTimeout<XRT_WAIT_FOREVER||C.FieldCount>32||C.MaxAuthorizationContextBytes>16384||(!Server&&C.Authorize)||!C.Target.Size||C.Target.Size>4096||C.Host.Size>1024||(!Server&&!C.Host.Size)||C.Origin.Size>2048||C.UnauthorizedChallenge.Size>128||C.Client.Protocols.Size>1024||C.Server.Protocols.Size>1024||!xrtWsUpgradeClientConfigValid(&C.Client)||!xrtWsUpgradeServerConfigValid(&C.Server))goto Argument;
    #if !defined(XRPC_FEATURE_XRPC_WS_HTTP_DEFLATE)
    if(C.Client.EnableDeflate||C.Client.RequireDeflate||C.Server.EnableDeflate||C.Server.RequireDeflate||C.Server.AcceptDeflate)goto Argument;
    #endif
    xstrview Views[]={C.Host,C.Target,C.Origin,C.UnauthorizedChallenge,C.Client.Protocols,C.Server.Protocols};size_t Strings=0;
    for(size_t I=0;I<sizeof(Views)/sizeof(*Views);++I){xstrview V=Views[I];if(!xrtMemRangeValid(V.Data,V.Size)||xrtMemRangesOverlap(V.Data,V.Size,Out,sizeof(*Out)))goto Argument;Strings+=V.Size;}
    if((C.Host.Size&&!xrtHttpHostValid(C.Host))||!xrtHttpFieldValueValid(C.Origin)||!xrtHttpFieldValueValid(C.UnauthorizedChallenge))goto Argument;
    if(C.Target.Data[0]!='/' || memchr(C.Target.Data,'#',C.Target.Size) || (C.Authorize&&!C.UnauthorizedChallenge.Size) || !xrtMemRangeValid(C.Fields,C.FieldCount*sizeof(xhttpfield)) || xrtMemRangesOverlap(C.Fields,C.FieldCount*sizeof(xhttpfield),Out,sizeof(*Out)))goto Argument;
    size_t Extras=0;unsigned Credentials=0;
    for(size_t I=0;I<C.FieldCount;++I){xhttpfield F=C.Fields[I];if(!F.Name.Size||F.Name.Size>256||F.Value.Size>4096||!xrtMemRangeValid(F.Name.Data,F.Name.Size)||!xrtMemRangeValid(F.Value.Data,F.Value.Size)||xrtMemRangesOverlap(F.Name.Data,F.Name.Size,Out,sizeof(*Out))||xrtMemRangesOverlap(F.Value.Data,F.Value.Size,Out,sizeof(*Out))||xrpcWsHttpManaged(F.Name))goto Argument;if(xrpcWsHttpName(F.Name,"Authorization")&&++Credentials>1)goto Argument;Extras+=F.Name.Size+F.Value.Size;if(Extras>16384)goto Argument;}
    size_t N=sizeof(xrpcwsupgrade)+(C.Head.MaxFields+C.FieldCount)*sizeof(xhttpfield)+Strings+Extras+C.MaxWriteBytes+(C.Authorize?C.MaxAuthorizationContextBytes:0);
    xrpcwsupgrade* H=xrtCalloc(1,N);if(!H)return NULL;H->Bytes=N;H->Config=C;H->Server=Server;H->Reason=XRPC_WS_TRANSPORT;H->Deadline=xrpcWsDeadline(Entered,C.SetupTimeout);H->EventGone=true;
    H->Fields=(xhttpfield*)(H+1);xhttpfield* Fields=H->Fields+C.Head.MaxFields;H->Config.Fields=Fields;bytes At=(bytes)(Fields+C.FieldCount);
    H->Config.Host=xrpcWsHttpCopyView(C.Host,&At);H->Config.Target=xrpcWsHttpCopyView(C.Target,&At);H->Config.Origin=xrpcWsHttpCopyView(C.Origin,&At);H->Config.UnauthorizedChallenge=xrpcWsHttpCopyView(C.UnauthorizedChallenge,&At);H->Config.Client.Protocols=xrpcWsHttpCopyView(C.Client.Protocols,&At);H->Config.Server.Protocols=xrpcWsHttpCopyView(C.Server.Protocols,&At);
    for(size_t I=0;I<C.FieldCount;++I)Fields[I]=(xhttpfield){xrpcWsHttpCopyView(C.Fields[I].Name,&At),xrpcWsHttpCopyView(C.Fields[I].Value,&At)};
    H->Wire=At;At+=C.MaxWriteBytes;H->ZeroContext=At;
    /* Validate target/header grammar before consuming any native owner. */
    size_t Used=0;
    if(!xrtHttp1RequestWrite(XRT_STR_LITERAL("GET"),H->Config.Target,XHTTP_VERSION_1_1,NULL,0,H->Wire,C.MaxWriteBytes,&Used)||!xrtHttp1ResponseWrite(XHTTP_VERSION_1_1,400,XRT_STR_LITERAL("Bad Request"),H->Config.Fields,C.FieldCount,H->Wire,C.MaxWriteBytes,&Used)){xrpcWsHttpFreeControl(H);return NULL;}
    return H;
Argument:
    xrpcWsReport(XERR_ARGUMENT,"invalid WS HTTP config, ranges, policy or bounds");return NULL;
}
static bool xrpcWsHttpRequest(xrpcwsupgrade* H)
{
    if(!xrtWsKeyGenerate(H->Key,sizeof(H->Key)))return false;
    xhttpfield Fields[XWS_UPGRADE_REQUEST_FIELDS_MAX+33];size_t N=0;
    xstrview Extension={0};
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_DEFLATE)
    char Offer[XWS_DEFLATE_MAX_SIZE];size_t OfferBytes=0;
    if(H->Config.Client.EnableDeflate){if(!xrtWsDeflateOfferWrite(&H->Config.Client.Deflate,Offer,sizeof(Offer),&OfferBytes))return false;Extension=(xstrview){Offer,OfferBytes};}
    #endif
    if(!xrtWsUpgradeRequestFields(H->Config.Host,(xstrview){H->Key,XWS_KEY_SIZE},H->Config.Client.Protocols,Extension,Fields,sizeof(Fields)/sizeof(*Fields),&N))return false;
    if(H->Config.Origin.Size)Fields[N++]=(xhttpfield){XRT_STR_LITERAL("Origin"),H->Config.Origin};
    for(size_t I=0;I<H->Config.FieldCount;++I)Fields[N++]=H->Config.Fields[I];
    return xrtHttp1RequestWrite(XRT_STR_LITERAL("GET"),H->Config.Target,XHTTP_VERSION_1_1,Fields,N,H->Wire,H->Config.MaxWriteBytes,&H->WireBytes);
}
static bool xrpcWsHttpResponse(xrpcws* P,uint16 Status)
{
    xrpcwsupgrade* H=P->Upgrade;xhttpfield Fields[XWS_UPGRADE_RESPONSE_FIELDS_MAX+36];size_t N=0;xstrview Reason=XRT_STR_LITERAL("Bad Request");
    if(Status==101){Reason=XRT_STR_LITERAL("Switching Protocols");if(!xrtWsUpgradeResponseFields((xstrview){H->Negotiated.Accept,XWS_ACCEPT_SIZE},H->Negotiated.Protocol,(xstrview){H->Negotiated.Extensions,H->Negotiated.ExtensionSize},Fields,sizeof(Fields)/sizeof(*Fields),&N))return false;}
    else{Fields[N++]=(xhttpfield){XRT_STR_LITERAL("Connection"),XRT_STR_LITERAL("close")};Fields[N++]=(xhttpfield){XRT_STR_LITERAL("Content-Length"),XRT_STR_LITERAL("0")};
        switch(Status){case 401:Reason=XRT_STR_LITERAL("Unauthorized");Fields[N++]=(xhttpfield){XRT_STR_LITERAL("WWW-Authenticate"),H->Config.UnauthorizedChallenge};break;case 403:Reason=XRT_STR_LITERAL("Forbidden");break;case 404:Reason=XRT_STR_LITERAL("Not Found");break;case 405:Reason=XRT_STR_LITERAL("Method Not Allowed");Fields[N++]=(xhttpfield){XRT_STR_LITERAL("Allow"),XRT_STR_LITERAL("GET")};break;case 426:Reason=XRT_STR_LITERAL("Upgrade Required");Fields[N++]=(xhttpfield){XRT_STR_LITERAL("Upgrade"),XRT_STR_LITERAL("websocket")};Fields[N++]=(xhttpfield){XRT_STR_LITERAL("Sec-WebSocket-Version"),XRT_STR_LITERAL("13")};break;case 431:Reason=XRT_STR_LITERAL("Request Header Fields Too Large");break;case 500:Reason=XRT_STR_LITERAL("Internal Server Error");break;case 503:Reason=XRT_STR_LITERAL("Service Unavailable");break;default:break;}}
    for(size_t I=0;I<H->Config.FieldCount;++I)Fields[N++]=H->Config.Fields[I];
    if(!xrtHttp1ResponseWrite(XHTTP_VERSION_1_1,Status,Reason,Fields,N,H->Wire,H->Config.MaxWriteBytes,&H->WireBytes))return false;
    H->WireOffset=0;xrtMutexLock(&P->Gate);P->UpgradeInfo.Status=Status;xrtMutexUnlock(&P->Gate);return true;
}
static bool xrpcWsHttpWrite(xrpcws* P)
{
    xrpcwsupgrade* H=P->Upgrade;if(H->WireOffset==H->WireBytes)return true;size_t N=H->WireBytes-H->WireOffset;bool Waiting=false;
    if(N>P->Config.WriteBytes)N=P->Config.WriteBytes;
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(H->TLS){size_t Written=0;xtlsresult R=xrtTlsStreamSend(H->Native,H->Wire+H->WireOffset,N,&Written);H->WireOffset+=Written;
        if(R!=XTLS_OK&&R!=XTLS_AGAIN){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());return false;}Waiting=R==XTLS_AGAIN;
    }else
    #endif
    {size_t Available=xrtNetStreamWritable(H->Native);if(N>Available)N=Available;if(!N)return false;xnetresult R=xrtNetStreamSend(H->Native,H->Wire+H->WireOffset,N);if(R==XNET_RESULT_AGAIN)return false;if(R!=XNET_RESULT_OK){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());return false;}H->WireOffset+=N;}
    xrtMutexLock(&P->Gate);P->UpgradeInfo.WriteBytes=H->WireOffset;xrtMutexUnlock(&P->Gate);
    if(H->WireOffset!=H->WireBytes){if(!Waiting)xrpcWsWake(P);return false;}return true;
}
static void xrpcWsHttpOutcome(xrpcws* P,xrpcwshttpoutcome Outcome)
{xrtMutexLock(&P->Gate);if(P->UpgradeInfo.Outcome==XRPC_WS_HTTP_PENDING)P->UpgradeInfo.Outcome=Outcome;xrtMutexUnlock(&P->Gate);}
static void xrpcWsHttpReject(xrpcws* P,uint16 Status,xrpcwsreason Reason)
{
    xrpcwsupgrade* H=P->Upgrade;H->Reason=Reason;H->Rejected=true;xrpcWsHttpOutcome(P,XRPC_WS_HTTP_REJECTED);
    if(!H->Server){xrpcWsFail(P,Reason);return;}
    if(!xrpcWsHttpResponse(P,Status)){H->Rejected=false;xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());}
}
/* Partial TCP heads and exceptional unpublished TLS/full-record-growth cases
 * use bounded cold storage, without mutating a native borrowed read buffer.
 * Peek may preview bytes past CRLFCRLF, but consume exactly through its end:
 * native WS data stays native, including a final message before TLS input EOF. */
/* Probe only whether the authoritative parser can still return MORE. Keep the
 * line position across input fragments; malformed endings and line limits go
 * immediately to Core for its precise error, complete heads for all semantics.
 * A trailing CR is revisited once when its following byte arrives. */
static bool xrpcWsHttpProbeHeader(xrpcwsupgrade* H,size_t* End)
{
    while(H->ScanOffset<H->CacheUsed){
        size_t I=H->ScanOffset,Limit=H->ScanLine?H->Config.Head.MaxFieldLine:H->Config.Head.MaxStartLine;
        if(I>=H->Config.Head.MaxHead || H->Cache[I]=='\n')return true;
        if(H->Cache[I]=='\r'){
            if(I+1==H->CacheUsed)break;
            if(H->Cache[I+1]!='\n' || I-H->ScanLineStart>Limit)return true;
            H->ScanOffset=I+2;
            if(H->ScanLine && I==H->ScanLineStart){*End=I+2;return true;}
            H->ScanLineStart=I+2;++H->ScanLine;
        }else{
            if(I-H->ScanLineStart>=Limit)return true;
            ++H->ScanOffset;
        }
    }
    return H->CacheUsed==H->Config.Head.MaxHead;
}
static xhttp1status xrpcWsHttpCachedHeader(xrpcws* P,xhttp1head* Head,xhttp1errorinfo* Error)
{
    xrpcwsupgrade* H=P->Upgrade;if(!H->Cache){H->Cache=xrtMalloc(H->Config.Head.MaxHead);if(!H->Cache)return XHTTP1_ERROR;xrpcWsHttpCostAdd(P,H->Config.Head.MaxHead);}
    size_t Old=H->CacheUsed,Left=H->Config.Head.MaxHead-Old;
    size_t N=xrtNetBufPeek(xrpcWsHttpBuffer(H),0,H->Cache+Old,Left),End=Old+N;
    H->CacheUsed=End;bool Parse=xrpcWsHttpProbeHeader(H,&End);H->CacheUsed=End;
    xrtMutexLock(&P->Gate);P->UpgradeInfo.HeaderCopyBytes+=N;xrtMutexUnlock(&P->Gate);
    if(End>Old&&!xrpcWsHttpConsume(H,End-Old))return XHTTP1_ERROR;
    if(!Parse)return XHTTP1_MORE;
    xbytesview Input={H->Cache,H->CacheUsed};xhttp1status R=H->Server?xrtHttp1RequestParse(Input,Head,&H->Config.Head,Error):xrtHttp1ResponseParse(Input,Head,&H->Config.Head,Error);
    if(R==XHTTP1_MORE&&H->CacheUsed==H->Config.Head.MaxHead){Error->Code=XHTTP1_ERROR_HEAD_TOO_LARGE;xrpcWsReport(XERR_RANGE,"WS HTTP retained header reached its hard limit");return XHTTP1_ERROR;}
    return R;
}
static bool xrpcWsHttpHeader(xrpcws* P)
{
    xrpcwsupgrade* H=P->Upgrade;xhttp1head Head={.Fields=H->Fields,.FieldCapacity=H->Config.Head.MaxFields};xhttp1errorinfo Error={0};xhttp1status R;
    if(H->Cache)R=xrpcWsHttpCachedHeader(P,&Head,&Error);else {
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(H->TLS)R=H->Server?xrtHttp1RequestParseTls(H->Native,&Head,&H->Config.Head,&Error):xrtHttp1ResponseParseTls(H->Native,&Head,&H->Config.Head,&Error);else
    #endif
    {
        /* IOCP may already own a write reservation for its next receive.
         * Parse a contiguous borrowed prefix without a mutable Pullup; retain
         * a partial/cross-block head in our own bounded cold cache instead. */
        xnetspan Span={0};const xnetbuf* Buffer=xrpcWsHttpBuffer(H);(void)xrtNetBufFront(Buffer,&Span);
        R=H->Server?xrtHttp1RequestParse((xbytesview){Span.Data,Span.Size},&Head,&H->Config.Head,&Error):xrtHttp1ResponseParse((xbytesview){Span.Data,Span.Size},&Head,&H->Config.Head,&Error);
        if(R==XHTTP1_MORE&&xrtNetBufSize(Buffer))R=xrpcWsHttpCachedHeader(P,&Head,&Error);
    }
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    xerrkind K=xrtErrorKind(xrtGetError());
    if(H->TLS&&R==XHTTP1_ERROR&&xrtTlsStreamState(H->Native)==XTLS_STREAM_OPEN&&xrtNetBufSize(xrpcWsHttpBuffer(H))&&(K==XERR_STATE||(K==XERR_RANGE&&(Error.Code==0||Error.Code==XHTTP1_ERROR_HEAD_INCOMPLETE)))){xrtClearError();Error=(xhttp1errorinfo){0};R=xrpcWsHttpCachedHeader(P,&Head,&Error);}
    #endif
    }
    if(R==XHTTP1_MORE){if(H->InputEnded){if(H->Server)xrpcWsHttpReject(P,400,XRPC_WS_PROTOCOL);else xrpcWsFail(P,XRPC_WS_TRANSPORT);return false;}
        /* ParseTls already requests growth. It may synchronously grow the
         * prefix before returning MORE; a second ReadMore could then retain
         * an unnecessary read-ahead request across a complete HTTP header. */
        return false;
    }
    if(R!=XHTTP1_READY){xrpcWsNativeError(P,xrtGetError());if(xrtErrorKind(xrtGetError())==XERR_MEMORY)xrpcWsFail(P,XRPC_WS_MEMORY);else if(H->Server)xrpcWsHttpReject(P,R==XHTTP1_FIELDS||Error.Code==XHTTP1_ERROR_HEAD_TOO_LARGE||Error.Code==XHTTP1_ERROR_TOO_MANY_FIELDS?431:400,XRPC_WS_PROTOCOL);else xrpcWsFail(P,XRPC_WS_PROTOCOL);return false;}
    xrtMutexLock(&P->Gate);P->UpgradeInfo.HeaderBytes=Head.Bytes;if(!H->Server)P->UpgradeInfo.Status=Head.Status;xrtMutexUnlock(&P->Gate);
    if(H->Server){
        unsigned Origins=0,Credentials=0;for(size_t I=0;I<Head.FieldCount;++I){Origins+=xrpcWsHttpName(Head.Fields[I].Name,"Origin");Credentials+=xrpcWsHttpName(Head.Fields[I].Name,"Authorization");}
        if(Origins>1||Credentials>1){xrpcWsHttpReject(P,400,XRPC_WS_PROTOCOL);return false;}
        if(!xrpcWsHttpEqual(Head.Method,XRT_STR_LITERAL("GET"),false)){xrpcWsHttpReject(P,405,XRPC_WS_HTTP);return false;}
        if(!xrpcWsHttpEqual(Head.Target,H->Config.Target,false)){xrpcWsHttpReject(P,404,XRPC_WS_HTTP);return false;}
        const xhttpfield* Host=xrtHttp1Field(&Head,XRT_STR_LITERAL("Host"));const xhttpfield* Origin=xrtHttp1Field(&Head,XRT_STR_LITERAL("Origin"));
        if(H->Config.Host.Size&&(!Host||!xrpcWsHttpEqual(Host->Value,H->Config.Host,true))){xrpcWsHttpReject(P,400,XRPC_WS_HTTP);return false;}
        if(H->Config.Origin.Size&&(!Origin||!xrpcWsHttpEqual(Origin->Value,H->Config.Origin,false))){xrpcWsHttpReject(P,403,XRPC_WS_AUTH);return false;}
        if(!xrtWsUpgradeRequestCheck(&Head,&H->Config.Server,&H->Negotiated)){
            xrpcWsNativeError(P,xrtGetError());const xhttpfield* Version=xrtHttp1Field(&Head,XRT_STR_LITERAL("Sec-WebSocket-Version"));xrpcWsHttpReject(P,Version&&!xrpcWsHttpEqual(Version->Value,XRT_STR_LITERAL("13"),false)?426:400,XRPC_WS_PROTOCOL);return false;
        }
    }else{
        if(Head.Status!=101){xrpcWsHttpReject(P,Head.Status,XRPC_WS_HTTP);return false;}
        if(!xrtWsUpgradeResponseCheck(&Head,(xstrview){H->Key,XWS_KEY_SIZE},&H->Config.Client,&H->Negotiated)){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,XRPC_WS_PROTOCOL);return false;}
    }
    if((H->Config.RequireProtocol&&!H->Negotiated.Protocol.Size)||H->Negotiated.Protocol.Size>255){if(H->Server)xrpcWsHttpReject(P,400,XRPC_WS_PROTOCOL);else xrpcWsFail(P,XRPC_WS_PROTOCOL);return false;}
    if(H->Negotiated.Protocol.Size)memcpy(H->Protocol,H->Negotiated.Protocol.Data,H->Negotiated.Protocol.Size);
    H->Negotiated.Protocol.Data=H->Protocol;
    H->HeaderBytes=Head.Bytes;H->Checked=true;
    if(H->Server&&H->Config.Authorize){if(!H->TLS){xrtNetStreamPause(xrpcWsHttpRaw(H));H->Paused=true;}if(!xrpcWsHttpAuthorize(P,&Head)){xrpcWsNativeError(P,xrtGetError());xrpcWsHttpReject(P,503,xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_WS_MEMORY:XRPC_WS_LIMIT);}}
    if(H->Cache){size_t N=H->Config.Head.MaxHead;xrtSecureZero(H->Cache,N);xrtFree(H->Cache);H->Cache=NULL;H->CacheUsed=0;xrpcWsHttpCostDrop(P,N);}
    else if(!xrpcWsHttpConsume(H,Head.Bytes)){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());return false;}
    if(H->Server&&!H->Authorization&&!H->Rejected&&!xrpcWsHttpResponse(P,101)){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());return false;}
    return true;
}
static void xrpcWsHttpHandoff(xrpcws* P)
{
    xrpcwsupgrade* H=P->Upgrade;xwsstreamconfig K=P->Config.Stream;K.Role=H->Server?XWS_ROLE_SERVER:XWS_ROLE_CLIENT;K.Protocol=H->Negotiated.Protocol;K.Deflate=H->Negotiated.Deflate;K.DeflateEnabled=H->Negotiated.DeflateEnabled;
    if(K.MessageLimit>P->Config.MaxMessageBytes)K.MessageLimit=P->Config.MaxMessageBytes;
    if(K.FrameLimit>P->Config.MaxMessageBytes)K.FrameLimit=P->Config.MaxMessageBytes;
    xnetstream* Raw=xrpcWsHttpRaw(H);xrpcWsRef(P);P->EventsGone=false;
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    xtlsstreambindingv1 Previous={0};
    #endif
    xwsstream* Stream;
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(H->TLS){xtlsstreambindingv1 Expected={P,&xrpcWsHttpTlsEvents};Stream=xrtWsStreamAttachTlsExchangeOwnedV1(H->Native,NULL,&K,P,&xrpcWsEvents,&Expected,&Previous);}else
    #endif
    Stream=xrtWsStreamAttachOwnedV1(H->Native,NULL,&K,P,&xrpcWsEvents);
    if(!Stream){xerror* Error=xrtTakeError();P->EventsGone=true;xrpcWsDrop(P);xrtSetErrorTake(Error);xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());return;}
    H->Native=NULL;P->Config.Stream.Protocol=xrtWsStreamProtocol(Stream);
    xrtMutexLock(&P->Gate);P->Stream=Stream;P->Config.Stream.Deflate=H->Negotiated.Deflate;P->Config.Stream.DeflateEnabled=H->Negotiated.DeflateEnabled;
    #if defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
    P->DeflateReady=true;
    #endif
    P->Info.ProtocolBytes=P->Config.Stream.Protocol.Size;if(P->Info.ProtocolBytes)memcpy(P->Info.Protocol,P->Config.Stream.Protocol.Data,P->Info.ProtocolBytes);if(P->Info.State==XRPC_WS_UPGRADING)P->Info.State=XRPC_WS_OPEN;P->UpgradeInfo.Outcome=XRPC_WS_HTTP_OPEN;xrtMutexUnlock(&P->Gate);
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(Previous.Policy)Previous.Policy->Drop(Previous.Data);
    #endif
    if(H->Paused&&!xrtNetStreamResume(Raw)){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());}
    if(H->EventGone
        #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
        &&!H->Dial
        #endif
        )xrpcWsHttpDiscard(P);
}
static void xrpcWsHttpNativeClose(xrpcwsupgrade* H,bool Abort)
{
    if(!H->Native)return;
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(H->TLS){if(Abort)(void)xrtTlsStreamAbort(H->Native);else (void)xrtTlsStreamClose(H->Native);}else
    #endif
    {if(Abort)(void)xrtNetStreamAbort(H->Native);else (void)xrtNetStreamClose(H->Native);}
    H->Closing=true;
}
static void xrpcWsHttpStep(xrpcws* P)
{
    xrpcwsupgrade* H=P->Upgrade;if(!H||xrpcWsClosing(P))return;
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
    if(H->Dial||H->Establishing){
        if(!P->Stream&&xrtTimer()>=H->Deadline){xrpcWsHttpOutcome(P,XRPC_WS_HTTP_TIMED_OUT);xrpcWsFail(P,XRPC_WS_TIMEOUT);return;}
        xrpcWsDialStep(P);if(xrpcWsClosing(P)||H->Establishing)return;
    }
    #endif
    if(!H->Native){if(H->EventGone
        #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
        &&!H->Dial
        #endif
        ){xrpcWsHttpDiscard(P);}return;}
    if(xrtTimer()>=H->Deadline){xrpcWsHttpOutcome(P,XRPC_WS_HTTP_TIMED_OUT);xrpcWsFail(P,XRPC_WS_TIMEOUT);return;}
    if(H->NativeClosed){xrpcWsFail(P,XRPC_WS_TRANSPORT);return;}
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    if(H->BindTLS){
        /* A freshly created TLS stream's root owner is not its replaceable
         * event slot. Bind a separate real credit after verified Open, outside
         * the native driver, before any HTTP parsing or event exchange. */
        xrpcWsRef(P);
        if(!xrtTlsStreamBindEventsOwnedV1(H->Native,P,&xrpcWsHttpTlsEvents)){xrpcWsDrop(P);xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsLocalFailure());return;}
        H->EventGone=false;H->BindTLS=false;
    }
    #endif
    if(!xrpcWsHttpWrite(P))return;
    if(H->Rejected){xrpcWsHttpNativeClose(H,false);xrpcWsFail(P,H->Reason);return;}
    if(!H->Checked&&!xrpcWsHttpHeader(P)){
        if(H->Rejected&&!xrpcWsClosing(P))xrpcWsWake(P);
        return;
    }
    if(xrpcWsClosing(P)||H->NativeClosed)return;
    if(H->Authorization){xrpcwsauthorization* A=H->Authorization;if(!xrtAtomic32Load(&A->Ready,XMEMORY_ACQUIRE))return;
        uint16 Status=A->Status;xrtMutexLock(&P->Gate);P->UpgradeInfo.AuthorizationPending=false;P->UpgradeInfo.AuthorizationStatus=Status;xrtMutexUnlock(&P->Gate);
        if(Status==200){if(A->ContextBytes)memcpy(P->Context,A->Context,A->ContextBytes);P->Config.RequestContext.Size=A->ContextBytes;}
        H->Authorization=NULL;xrpcWsHttpAuthRelease(A);
        if(Status!=200){xrpcWsHttpReject(P,Status,Status==401||Status==403?XRPC_WS_AUTH:XRPC_WS_LIMIT);xrpcWsWake(P);return;}
        if(!xrpcWsHttpResponse(P,101)){xrpcWsFail(P,xrpcWsLocalFailure());return;}
    }
    if(H->Rejected){xrpcWsWake(P);return;}
    if(!xrpcWsHttpWrite(P))return;
    xrpcWsHttpHandoff(P);
}
static bool xrpcWsHttpRetire(xrpcws* P)
{
    xrpcwsupgrade* H=P->Upgrade;if(!H)return true;
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
    (void)xrpcWsDialRetire(P);
    #endif
    xrtMutexLock(&P->Gate);xrpcwsreason Reason=P->Info.Reason;bool Force=P->UpgradeAbort;xrtMutexUnlock(&P->Gate);xrpcWsHttpOutcome(P,Reason==XRPC_WS_TIMEOUT?XRPC_WS_HTTP_TIMED_OUT:Reason==XRPC_WS_LOCAL?XRPC_WS_HTTP_CLOSED:XRPC_WS_HTTP_FAILED);
    if(H->Authorization){xrpcwsauthorization* A=H->Authorization;(void)xrtCancelRequest(A->Work.Cancel);if(xrtAtomic32Load(&A->Ready,XMEMORY_ACQUIRE)){H->Authorization=NULL;xrtMutexLock(&P->Gate);P->UpgradeInfo.AuthorizationPending=false;xrtMutexUnlock(&P->Gate);xrpcWsHttpAuthRelease(A);}}
    if(H->Native){bool Abort=Force||!H->Server||!H->Rejected||Reason==XRPC_WS_LOCAL||xrtTimer()>=H->Deadline;if(!H->Closing||Abort)xrpcWsHttpNativeClose(H,Abort);
        bool Ready;
        #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
        if(H->TLS)Ready=xrtTlsStreamPollRetired(H->Native)==XWAIT_OK;else
        #endif
        Ready=xrtNetStreamPollRetired(H->Native)==XWAIT_OK;
        if(Ready){ptr Native=H->Native;H->Native=NULL;
            #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
            if(H->TLS)xrtTlsStreamDestroy(Native);else
            #endif
            xrtNetStreamDestroy(Native);
        }
    }
    if(!H->Native&&H->EventGone&&!H->Authorization
        #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
        &&!H->Dial
        #endif
        ){xrpcWsHttpDiscard(P);return true;}return false;
}
static bool xrpcWsHttpCreate(ptr Native,bool TLS,bool Server,const xrpcwsconfig* Config,const xrpcwshttpconfig* HTTP,xrpcws** Out,double Entered)
{
    if(!Native)return xrpcWsReport(XERR_ARGUMENT,"WS HTTP transport is NULL");
    xnetworker* W=NULL;
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(TLS){W=xrtNetStreamWorker(xrtTlsStreamTransport(Native));if(xrtTlsStreamState(Native)!=XTLS_STREAM_OPEN)return xrpcWsReport(XERR_STATE,"WSS HTTP requires authenticated OPEN TLS");}else
    #endif
    {W=xrtNetStreamWorker(Native);if(xrtNetStreamState(Native)!=XNET_STREAM_OPEN)return xrpcWsReport(XERR_STATE,"WS HTTP requires OPEN TCP");}
    if(!xrtNetWorkerIsCurrent(W))return xrpcWsReport(XERR_STATE,"WS HTTP factory requires exclusive native Worker");
    xrpcwsupgrade* H=xrpcWsHttpControl(HTTP,Server,Out,Entered);if(!H)return false;
    if(Config&&(!xrtMemRangeValid(Config,sizeof(*Config))||xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)))){xrpcWsHttpFreeControl(H);return xrpcWsReport(XERR_ARGUMENT,"invalid RPC WS config");}
    xrpcwsconfig C={0};if(Config)C=*Config;else xrpcWsConfigInit(&C);
    if(H->Config.Authorize&&(!C.Duplex.Server||C.RequestContext.Size)){xrpcWsHttpFreeControl(H);return xrpcWsReport(XERR_ARGUMENT,"WS HTTP authorization needs shared Server and empty preconfigured context");}
    C.Stream.Role=Server?XWS_ROLE_SERVER:XWS_ROLE_CLIENT;C.Stream.Protocol=(xstrview){0};C.Stream.DeflateEnabled=false;
    if(H->Config.Authorize)C.RequestContext=(xbytesview){H->ZeroContext,H->Config.MaxAuthorizationContextBytes};
    if(!Server&&!xrpcWsHttpRequest(H)){xrpcWsHttpFreeControl(H);return false;}
    xrpcws* P=xrpcWsCreate(W,&C,NULL,Out);if(!P){xrpcWsHttpFreeControl(H);return false;}
    P->Upgrade=H;H->Native=Native;H->TLS=TLS;P->Info.State=XRPC_WS_UPGRADING;P->UpgradeInfo=(xrpcwshttpinfo){.Size=sizeof(xrpcwshttpinfo),.Deadline=H->Deadline,.Server=Server,.Secure=TLS};xrpcWsHttpCostAdd(P,H->Bytes);
    bool Bound;xrpcWsRef(P);H->EventGone=false;
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
    if(TLS){xtlssession* S=xrtTlsStreamSession(Native);P->Info.Encrypted=true;P->Info.Version=xrtTlsSessionVersion(S);P->Info.Cipher=xrtTlsSessionCipher(S);Bound=xrtTlsStreamBindEventsOwnedV1(Native,P,&xrpcWsHttpTlsEvents);}else
    #endif
    Bound=xrtNetStreamSetEventsOwnedV1(Native,P,&xrpcWsHttpTcpEvents);
    if(!Bound){xerror* Error=xrtTakeError();H->EventGone=true;H->Native=NULL;xrpcWsDrop(P);xrpcWsHttpDiscard(P);xrpcWsFree(P);xrtSetErrorTake(Error);return false;}
    xrtMutexLock(&P->Gate);P->Active=true;xrpcWsRef(P);*Out=P;if(!xrpcWsWakeLocked(P)){P->Info.State=XRPC_WS_CLOSING;P->Info.Reason=XRPC_WS_LIMIT;}xrtMutexUnlock(&P->Gate);return true;
}
#if defined(XRPC_FEATURE_XRPC_WS_DIAL)
struct xrpcwsdialdata {
    size_t Bytes;xrpcwsdialconfig Config;xnetdialconfig Native;
    xnetresolver* Resolver;xnetaddrlist* Addresses;xnetdial* Dial;xnetstream* Raw;
    xnetaddr Remote;uint64 Affinity;uint16 Port;
    bool Numeric,Started,Pending,RawOpen,RawGone,Handoff;
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    xtlssession* Session;xtlsstreamconfig TLS;
    #endif
    char Name[];
};
#if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
static void xrpcWsDialTlsRootDrop(const void* Data)
{xrpcws* P=(xrpcws*)Data;xrpcWsWake(P);xrpcWsDrop(P);}
static const xtlsstreamownershipv1 xrpcWsDialTlsRootEvents={sizeof(xtlsstreamownershipv1),{
    .Open=xrpcWsHttpTlsOpened,.Read=xrpcWsHttpTlsRead,.End=xrpcWsHttpTlsEnd,
    .Writable=xrpcWsHttpTlsWritable,.Drain=xrpcWsHttpTlsWritable,.Close=xrpcWsHttpTlsClosed},xrpcWsDialTlsRootDrop,&xrpcWsOps};
#endif
static xrpcwsreason xrpcWsDialFailure(xnetresult R,const xerror* E)
{xerrkind K=xrtErrorKind(E);return R==XNET_RESULT_TIMEOUT||K==XERR_TIMEOUT?XRPC_WS_TIMEOUT:K==XERR_MEMORY?XRPC_WS_MEMORY:K==XERR_RANGE||K==XERR_AGAIN?XRPC_WS_LIMIT:XRPC_WS_TRANSPORT;}
static void xrpcWsDialFree(xrpcwsdialdata* D)
{
    if(!D)return;
    xrtNetDialDestroy(D->Dial);xrtNetAddrListDestroy(D->Addresses);
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    xrtTlsSessionDestroy(D->Session);
    #endif
    size_t N=D->Bytes;xrtSecureZero(D,N);xrtFree(D);
}
static void xrpcWsDialDiscard(xrpcws* P)
{xrpcwsdialdata* D=P->Upgrade->Dial;P->Upgrade->Dial=NULL;size_t N=D->Bytes;xrpcWsDialFree(D);xrpcWsHttpCostDrop(P,N);}
static void xrpcWsDialRawOpen(xnetstream* S,ptr Data)
{(void)S;xrpcws* P=Data;P->Upgrade->Dial->RawOpen=true;xrpcWsWake(P);}
static void xrpcWsDialRawRead(xnetstream* S,xnetbuf* B,ptr Data)
{(void)B;xrpcws* P=Data;if(!P->Upgrade->Dial->Handoff)(void)xrtNetStreamPause(S);xrpcWsWake(P);}
static void xrpcWsDialRawEnd(xnetstream* S,ptr Data)
{(void)S;xrpcWsFail(Data,XRPC_WS_TRANSPORT);xrpcWsWake(Data);}
static void xrpcWsDialRawClose(xnetstream* S,xnetresult R,const xerror* E,ptr Data)
{(void)S;xrpcws* P=Data;xrpcWsNativeError(P,E);xrpcWsFail(P,xrpcWsDialFailure(R,E));xrpcWsWake(P);}
static void xrpcWsDialRawDrop(const void* Data)
{xrpcws* P=(xrpcws*)Data;P->Upgrade->Dial->RawGone=true;xrpcWsWake(P);xrpcWsDrop(P);}
static const xnetstreameventsownershipv1 xrpcWsDialRawEvents={sizeof(xnetstreameventsownershipv1),{.Open=xrpcWsDialRawOpen,.Read=xrpcWsDialRawRead,.End=xrpcWsDialRawEnd,.Close=xrpcWsDialRawClose},xrpcWsDialRawDrop,&xrpcWsOps};
static void xrpcWsDialDone(xnetdial* Dial,xnetresult Result,xnetstream* Stream,const xerror* Cause,ptr Data)
{
    (void)Dial;xrpcws* P=Data;xrpcwsupgrade* H=P->Upgrade;xrpcwsdialdata* D=H->Dial;xrpcWsNativeError(P,Cause);
    if(xrtTimer()>=H->Deadline)xrpcWsFail(P,XRPC_WS_TIMEOUT);
    if(!Stream){xrpcWsFail(P,xrpcWsDialFailure(Result,Cause));xrpcWsWake(P);return;}
    D->Raw=Stream;
    if(!xrpcWsClosing(P)){
        xrpcWsRef(P);D->RawGone=false;
        if(!xrtNetStreamSetEventsOwnedV1(Stream,P,&xrpcWsDialRawEvents)){D->RawGone=true;xrpcWsDrop(P);xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsDialFailure(XNET_RESULT_ERROR,xrtGetError()));}
        else D->RawOpen=true;
    }
    if(xrpcWsClosing(P))(void)xrtNetStreamAbort(Stream);
    xrpcWsWake(P);
}
static void xrpcWsDialDropped(const void* Data)
{xrpcws* P=(xrpcws*)Data;P->Upgrade->Dial->Pending=false;xrpcWsWake(P);xrpcWsDrop(P);}
static const xnetdialownershipv1 xrpcWsDialEvents={sizeof(xnetdialownershipv1),xrpcWsDialDone,xrpcWsDialDropped,&xrpcWsOps};
static void xrpcWsDialStep(xrpcws* P)
{
    xrpcwsupgrade* H=P->Upgrade;xrpcwsdialdata* D=H->Dial;if(!D)return;
    if(!D->Started){
        D->Started=true;
        if(D->Numeric){
            D->RawGone=false;xrpcWsRef(P);D->Raw=xrtNetStreamConnectOwnedV1(P->Engine,&D->Remote,D->Affinity,&D->Config.Stream,P,&xrpcWsDialRawEvents);
            if(!D->Raw){D->RawGone=true;xrpcWsDrop(P);xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsDialFailure(XNET_RESULT_ERROR,xrtGetError()));return;}
        }else{
            D->Pending=true;xrpcWsRef(P);
            D->Dial=D->Addresses?xrtNetDialAddressesOwnedV1(P->Engine,D->Addresses,&D->Native,NULL,NULL,P,&xrpcWsDialEvents):xrtNetDialOwnedV1(P->Engine,D->Resolver,D->Name,D->Port,&D->Native,NULL,NULL,P,&xrpcWsDialEvents);
            if(!D->Dial){D->Pending=false;xrpcWsDrop(P);xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsDialFailure(XNET_RESULT_ERROR,xrtGetError()));return;}
            xrtNetAddrListDestroy(D->Addresses);D->Addresses=NULL;
        }
    }
    if(D->Raw&&D->RawOpen){
        D->Handoff=true;bool Ready=xrtNetStreamResume(D->Raw);D->Handoff=false;
        if(!Ready||xrpcWsClosing(P)){if(!xrpcWsClosing(P))xrpcWsFail(P,xrpcWsDialFailure(XNET_RESULT_ERROR,xrtGetError()));return;}
        bool Bound;H->EventGone=false;xrpcWsRef(P);
        #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
        if(H->TLS){xtlsstream* S=NULL;Bound=xrtTlsStreamAttachOwnedV1(D->Raw,D->Session,&D->TLS,P,&xrpcWsDialTlsRootEvents,&S);if(Bound){H->Native=S;D->Session=NULL;H->EventGone=true;}}
        else
        #endif
        {Bound=xrtNetStreamSetEventsOwnedV1(D->Raw,P,&xrpcWsHttpTcpEvents);if(Bound){H->Native=D->Raw;H->Establishing=false;}}
        if(!Bound){H->EventGone=true;xrpcWsDrop(P);xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,xrpcWsDialFailure(XNET_RESULT_ERROR,xrtGetError()));return;}
        D->Raw=NULL;
    }
    if(D->Dial&&!D->Pending){xrtNetDialDestroy(D->Dial);D->Dial=NULL;}
    if(!D->Pending&&!D->Raw&&D->RawGone&&(H->Native||P->Stream))xrpcWsDialDiscard(P);
}
static bool xrpcWsDialRetire(xrpcws* P)
{
    xrpcwsdialdata* D=P->Upgrade->Dial;if(!D)return true;
    if(D->Pending)(void)xrtNetDialCancel(D->Dial);
    if(D->Raw){(void)xrtNetStreamAbort(D->Raw);if(xrtNetStreamPollRetired(D->Raw)==XWAIT_OK){xnetstream* S=D->Raw;D->Raw=NULL;xrtNetStreamDestroy(S);}}
    if(!D->Pending&&!D->Raw&&D->RawGone){xrpcWsDialDiscard(P);return true;}return false;
}
XRPC_API bool xrpcWsDialConfigInit(xrpcwsdialconfig* C)
{
    if(!C||!xrtMemRangeValid(C,sizeof(*C)))return xrpcWsReport(XERR_ARGUMENT,"invalid WS dial config output");
    *C=(xrpcwsdialconfig){0};C->Size=sizeof(*C);xrtNetStreamConfigInit(&C->Stream);C->Family=XNET_FAMILY_UNSPEC;C->FallbackDelay=250;C->MaxAttempts=8;C->MaxHostBytes=1024;C->MaxMetadataBytes=16384;return true;
}
static bool xrpcWsDialCreate(xnetengine* E,const xnetaddr* Remote,xnetresolver* Resolver,xnetaddrlist* Addresses,xstrview Name,uint16 Port,uint64 Affinity,
    bool Secure,const void* Client,const void* NativeTLS,const xrpcwsconfig* Config,const xrpcwshttpconfig* HTTP,const xrpcwsdialconfig* Dial,xrpcws** Out,double Entered)
{
    if(!E||!Out||!xrtMemRangeValid(Out,sizeof(*Out))||(Dial&&(!xrtMemRangeValid(Dial,sizeof(*Dial))||xrtMemRangesOverlap(Dial,sizeof(*Dial),Out,sizeof(*Out)))))goto Argument;
    xrpcwsdialconfig K={0};if(Dial)K=*Dial;else if(!xrpcWsDialConfigInit(&K))return false;
    if(K.Size!=sizeof(K)||(K.Family!=XNET_FAMILY_UNSPEC&&K.Family!=XNET_FAMILY_IPV4&&K.Family!=XNET_FAMILY_IPV6)||K.FallbackDelay<0||!K.MaxAttempts||K.MaxAttempts>64||!K.MaxHostBytes||K.MaxHostBytes>65535||!K.MaxMetadataBytes||K.MaxMetadataBytes>16384||!xrtNetStreamConfigValid(&K.Stream))goto Argument;
    if(Remote){if(!xrtMemRangeValid(Remote,sizeof(*Remote))||xrtMemRangesOverlap(Remote,sizeof(*Remote),Out,sizeof(*Out))||(Remote->Family!=XNET_FAMILY_IPV4&&Remote->Family!=XNET_FAMILY_IPV6)||!Remote->Port||(K.Family!=XNET_FAMILY_UNSPEC&&K.Family!=Remote->Family))goto Argument;}
    else if(Addresses){
        size_t Count=xrtNetAddrListCount(Addresses),Eligible=0;if(!Count||Count>64)goto Argument;
        for(size_t I=0;I<Count;++I){const xnetaddr* A=xrtNetAddrListGet(Addresses,I);if(!A)return false;if(xrtMemRangesOverlap(A,sizeof(*A),Out,sizeof(*Out)))goto Argument;if(K.Family!=XNET_FAMILY_UNSPEC&&K.Family!=A->Family)continue;if(!A->Port)goto Argument;++Eligible;}if(!Eligible)goto Argument;
    }else if(!Resolver||!Port||!Name.Size||Name.Size>K.MaxHostBytes||!xrtMemRangeValid(Name.Data,Name.Size)||xrtMemRangesOverlap(Name.Data,Name.Size,Out,sizeof(*Out))||memchr(Name.Data,0,Name.Size))goto Argument;
    uint32 Workers=xrtNetEngineWorkerCount(E);if(!Workers)return xrpcWsReport(XERR_STATE,"WS dial Engine has no Worker");
    xrpcwsupgrade* H=xrpcWsHttpControl(HTTP,false,Out,Entered);if(!H)return false;
    if((Config&&(!xrtMemRangeValid(Config,sizeof(*Config))||xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))))||H->Config.Authorize){xrpcWsHttpFreeControl(H);goto Argument;}
    xrpcwsconfig C={0};if(Config)C=*Config;else xrpcWsConfigInit(&C);C.Stream.Role=XWS_ROLE_CLIENT;C.Stream.Protocol=(xstrview){0};C.Stream.DeflateEnabled=false;
    if(!xrpcWsHttpRequest(H)){xrpcWsHttpFreeControl(H);return false;}
    xrpcwsdialdata* D=xrtCalloc(1,sizeof(*D)+Name.Size+1);if(!D){xrpcWsHttpFreeControl(H);return false;}
    D->Bytes=sizeof(*D)+Name.Size+1;D->Config=K;D->Resolver=Resolver;D->Affinity=Affinity;D->Port=Port;D->RawGone=true;D->Numeric=Remote!=NULL;if(Remote)D->Remote=*Remote;
    xrtNetDialConfigInit(&D->Native);D->Native.Stream=K.Stream;D->Native.Family=K.Family;D->Native.Affinity=Affinity;D->Native.FallbackDelay=K.FallbackDelay;D->Native.MaxAttempts=K.MaxAttempts;D->Native.Timeout=0;
    if(Addresses){D->Addresses=xrtNetAddrListRef(Addresses);if(!D->Addresses)goto Fail;}else if(Name.Size)memcpy(D->Name,Name.Data,Name.Size);
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    if(Secure){
        const xtlsclientconfig* T=Client;
        if(!T||!xrtMemRangeValid(T,sizeof(*T))||xrtMemRangesOverlap(T,sizeof(*T),Out,sizeof(*Out))||(!T->ServerName.Size&&!T->VerifyName.Size)||T->ProtocolCount>K.MaxMetadataBytes/sizeof(xstrview))goto BadTLS;
        size_t Bytes=T->ProtocolCount*sizeof(xstrview);if(!xrtMemRangeValid(T->Protocols,Bytes)||xrtMemRangesOverlap(T->Protocols,Bytes,Out,sizeof(*Out)))goto BadTLS;
        for(size_t I=0;I<T->ProtocolCount+2;++I){xstrview V=I==0?T->ServerName:I==1?T->VerifyName:T->Protocols[I-2];if(V.Size>K.MaxMetadataBytes-Bytes||!xrtMemRangeValid(V.Data,V.Size)||xrtMemRangesOverlap(V.Data,V.Size,Out,sizeof(*Out))||(I<2&&V.Size&&memchr(V.Data,0,V.Size)))goto BadTLS;Bytes+=V.Size;}
        xrtTlsStreamConfigInit(&D->TLS);if(NativeTLS){if(!xrtMemRangeValid(NativeTLS,sizeof(D->TLS))||xrtMemRangesOverlap(NativeTLS,sizeof(D->TLS),Out,sizeof(*Out)))goto BadTLS;D->TLS=*(const xtlsstreamconfig*)NativeTLS;}
        if(D->TLS.HandshakeTimeout<0||D->TLS.CloseTimeout<0||!D->TLS.AsyncBytesLimit||!D->TLS.AsyncCountLimit||!D->TLS.AsyncBatch)goto BadTLS;
        D->Session=xrtTlsClientCreate(T,NULL);if(!D->Session)goto Fail;
    }
    #else
    (void)Client;(void)NativeTLS;if(Secure)goto BadTLS;
    #endif
    xrpcws* P=xrpcWsCreate(xrtNetEngineWorker(E,(uint32)(Affinity%Workers)),&C,NULL,Out);if(!P)goto Fail;
    H->TLS=Secure;H->Establishing=true;H->BindTLS=Secure;H->EventGone=true;H->Dial=D;P->Upgrade=H;P->Info.State=XRPC_WS_UPGRADING;P->Info.Encrypted=Secure;
    P->UpgradeInfo=(xrpcwshttpinfo){.Size=sizeof(xrpcwshttpinfo),.Deadline=H->Deadline,.Secure=Secure};xrpcWsHttpCostAdd(P,H->Bytes+D->Bytes);
    xrtMutexLock(&P->Gate);P->Active=true;xrpcWsRef(P);bool Posted=xrpcWsWakeLocked(P);if(Posted)*Out=P;else P->Active=false;xrtMutexUnlock(&P->Gate);
    if(!Posted){xerror* Error=xrtTakeError();xrpcWsDrop(P);H->Dial=NULL;xrpcWsDialFree(D);xrpcWsHttpDiscard(P);xrpcWsFree(P);xrtSetErrorTake(Error);}return Posted;
BadTLS:
    xrpcWsReport(XERR_ARGUMENT,"invalid WS TLS identity/metadata/stream policy");
Fail:
    {xerror* Error=xrtTakeError();xrpcWsDialFree(D);xrpcWsHttpFreeControl(H);xrtClearError();xrtSetErrorTake(Error);}return false;
Argument:
    return xrpcWsReport(XERR_ARGUMENT,"invalid WS dial policy/address/input/output");
}
XRPC_API bool xrpcWsConnect(xnetengine* E,const xnetaddr* A,uint64 F,const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{return xrpcWsDialCreate(E,A,NULL,NULL,(xstrview){0},0,F,false,NULL,NULL,C,H,D,Out,xrtTimer());}
XRPC_API bool xrpcWsDial(xnetengine* E,xnetresolver* R,xstrview N,uint16 Port,uint64 F,const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{return xrpcWsDialCreate(E,NULL,R,NULL,N,Port,F,false,NULL,NULL,C,H,D,Out,xrtTimer());}
XRPC_API bool xrpcWsDialAddresses(xnetengine* E,xnetaddrlist* A,uint64 F,const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{if(!A)return xrpcWsReport(XERR_ARGUMENT,"WS endpoints are NULL");return xrpcWsDialCreate(E,NULL,NULL,A,(xstrview){0},0,F,false,NULL,NULL,C,H,D,Out,xrtTimer());}
#if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
XRPC_API bool xrpcWsConnectTls(xnetengine* E,const xnetaddr* A,uint64 F,const xtlsclientconfig* T,const xtlsstreamconfig* S,const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{return xrpcWsDialCreate(E,A,NULL,NULL,(xstrview){0},0,F,true,T,S,C,H,D,Out,xrtTimer());}
XRPC_API bool xrpcWsDialTls(xnetengine* E,xnetresolver* R,xstrview N,uint16 Port,uint64 F,const xtlsclientconfig* T,const xtlsstreamconfig* S,const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{return xrpcWsDialCreate(E,NULL,R,NULL,N,Port,F,true,T,S,C,H,D,Out,xrtTimer());}
XRPC_API bool xrpcWsDialAddressesTls(xnetengine* E,xnetaddrlist* A,uint64 F,const xtlsclientconfig* T,const xtlsstreamconfig* S,const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{if(!A)return xrpcWsReport(XERR_ARGUMENT,"WSS endpoints are NULL");return xrpcWsDialCreate(E,NULL,NULL,A,(xstrview){0},0,F,true,T,S,C,H,D,Out,xrtTimer());}
#endif
#endif
#if defined(XRPC_FEATURE_XRPC_WS_URI)
static int xrpcWsUriHex(unsigned char C)
{return C>='0'&&C<='9'?C-'0':C>='a'&&C<='f'?C-'a'+10:C>='A'&&C<='F'?C-'A'+10:-1;}
static bool xrpcWsUriRoute(xnetengine* E,xnetresolver* R,const xrpcwsuri* U,xstrview Target,uint64 Affinity,bool Secure,
    const void* Client,const void* NativeTLS,const xrpcwsconfig* Config,const xrpcwshttpconfig* HTTP,const xrpcwsdialconfig* Dial,xrpcws** Out,double Entered)
{
    char Decoded[XRPC_WS_URI_MAX_AUTHORITY+1];xstrview Route=U->Host;
    if(memchr(Route.Data,'%',Route.Size)){
        size_t N=0;for(size_t I=0;I<Route.Size;++I){unsigned char C=(unsigned char)Route.Data[I];if(C=='%'){int A=xrpcWsUriHex((unsigned char)Route.Data[I+1]),B=xrpcWsUriHex((unsigned char)Route.Data[I+2]);C=(unsigned char)(A*16+B);I+=2;}
            if(C<=32||C==127||C=='/'||C==':'||C=='?'||C=='#'||C=='@'||C=='['||C==']'){return xrpcWsReport(XERR_ARGUMENT,"decoded URI host contains a forbidden routing byte");}
            Decoded[N++]=(char)C;}
        Route=(xstrview){Decoded,N};if(!xrtUtf8Valid(Route,NULL))return xrpcWsReport(XERR_ARGUMENT,"decoded URI host is not UTF-8");
    }
    bool Numeric=U->HostKind==XRPC_WS_URI_IPV4||U->HostKind==XRPC_WS_URI_IPV6||xrtHttpIpv4Valid(Route);xnetaddr Address;
    if(Numeric){char IP[64];if(Route.Size>=sizeof(IP))return xrpcWsReport(XERR_ARGUMENT,"URI IP literal exceeds bound");memcpy(IP,Route.Data,Route.Size);IP[Route.Size]=0;if(!xrtNetAddrParse(&Address,IP,U->Port))return false;}
    xrpcwshttpconfig H={0};if(HTTP){if(!xrtMemRangeValid(HTTP,sizeof(*HTTP))||xrtMemRangesOverlap(HTTP,sizeof(*HTTP),Out,sizeof(*Out)))return xrpcWsReport(XERR_ARGUMENT,"invalid URI HTTP policy/output overlap");H=*HTTP;}else xrpcWsHttpConfigInit(&H);
    H.Host=U->Authority;H.Target=Target;
    #if defined(XRPC_FEATURE_XRPC_WS_URI_TLS)
    xtlsclientconfig TLS;
    if(Secure){if(!Client||!xrtMemRangeValid(Client,sizeof(TLS))||xrtMemRangesOverlap(Client,sizeof(TLS),Out,sizeof(*Out)))return xrpcWsReport(XERR_ARGUMENT,"URI TLS requires an explicit client/trust policy");TLS=*(const xtlsclientconfig*)Client;if(!TLS.VerifyName.Size)TLS.VerifyName=Route;if(!TLS.ServerName.Size&&!Numeric)TLS.ServerName=Route;Client=&TLS;}
    #endif
    return xrpcWsDialCreate(E,Numeric?&Address:NULL,Numeric?NULL:R,NULL,Numeric?(xstrview){0}:Route,Numeric?0:U->Port,Affinity,Secure,Client,NativeTLS,Config,&H,Dial,Out,Entered);
}
static bool xrpcWsUriCreate(xnetengine* E,xnetresolver* R,xstrview URI,uint64 Affinity,bool Secure,const void* Client,const void* NativeTLS,
    const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out,double Entered)
{
    if(!Out||!xrtMemRangeValid(Out,sizeof(*Out))||!xrtMemRangeValid(URI.Data,URI.Size)||xrtMemRangesOverlap(URI.Data,URI.Size,Out,sizeof(*Out)))return xrpcWsReport(XERR_ARGUMENT,"invalid URI/output range or overlap");
    xrpcwsuri U={.Size=sizeof(U)};if(!xrpcWsUriParse(URI,&U))return false;
    if(U.Secure!=Secure)return xrpcWsReport(XERR_ARGUMENT,"URI scheme does not match the explicit secure/plain factory");
    if(U.HostKind==XRPC_WS_URI_IPVFUTURE)return xrpcWsReport(XERR_UNSUPPORTED,"URI IPvFuture address mechanism is unsupported");
    if(!U.Path.Size&&U.Query.Size){char Target[XRPC_WS_URI_MAX_TARGET];Target[0]='/';Target[1]='?';memcpy(Target+2,U.Query.Data,U.Query.Size);return xrpcWsUriRoute(E,R,&U,(xstrview){Target,U.TargetBytes},Affinity,Secure,Client,NativeTLS,C,H,D,Out,Entered);}
    xstrview Target=U.Path.Size?(xstrview){U.Path.Data,U.TargetBytes}:XRT_STR_LITERAL("/");
    return xrpcWsUriRoute(E,R,&U,Target,Affinity,Secure,Client,NativeTLS,C,H,D,Out,Entered);
}
XRPC_API bool xrpcWsConnectUri(xnetengine* E,xnetresolver* R,xstrview URI,uint64 A,const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{return xrpcWsUriCreate(E,R,URI,A,false,NULL,NULL,C,H,D,Out,xrtTimer());}
#if defined(XRPC_FEATURE_XRPC_WS_URI_TLS)
XRPC_API bool xrpcWsConnectUriTls(xnetengine* E,xnetresolver* R,xstrview URI,uint64 A,const xtlsclientconfig* T,const xtlsstreamconfig* S,const xrpcwsconfig* C,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{return xrpcWsUriCreate(E,R,URI,A,true,T,S,C,H,D,Out,xrtTimer());}
#endif
#endif
XRPC_API bool xrpcWsHttpClientTcp(xnetstream* S,const xrpcwsconfig* C,const xrpcwshttpconfig* H,xrpcws** Out)
{return xrpcWsHttpCreate(S,false,false,C,H,Out,xrtTimer());}
XRPC_API bool xrpcWsHttpServerTcp(xnetstream* S,const xrpcwsconfig* C,const xrpcwshttpconfig* H,xrpcws** Out)
{return xrpcWsHttpCreate(S,false,true,C,H,Out,xrtTimer());}
#if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
XRPC_API bool xrpcWsHttpClientTls(xtlsstream* S,const xrpcwsconfig* C,const xrpcwshttpconfig* H,xrpcws** Out)
{return xrpcWsHttpCreate(S,true,false,C,H,Out,xrtTimer());}
XRPC_API bool xrpcWsHttpServerTls(xtlsstream* S,const xrpcwsconfig* C,const xrpcwshttpconfig* H,xrpcws** Out)
{return xrpcWsHttpCreate(S,true,true,C,H,Out,xrtTimer());}
#endif
XRPC_API bool xrpcWsHttpInfo(xrpcws* P,xrpcwshttpinfo* I)
{
    if(!P||!I||!xrtMemRangeValid(I,sizeof(*I))||I->Size!=sizeof(*I)||xrtMemRangesOverlap(P,P->Info.FixedBytes,I,sizeof(*I)))return xrpcWsReport(XERR_ARGUMENT,"invalid WS HTTP info output");
    xrtMutexLock(&P->Gate);bool Valid=P->UpgradeInfo.Size==sizeof(P->UpgradeInfo);if(Valid)*I=P->UpgradeInfo;xrtMutexUnlock(&P->Gate);return Valid?true:xrpcWsReport(XERR_STATE,"peer did not use HTTP Upgrade factory");
}
#endif
#endif
