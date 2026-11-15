#include <xrpc/ws.h>
#include "../internal/xrpc_duplex_internal.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <math.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_WS)
struct xrpcws {
    xrpcwsconfig Config;xrpcwsinfo Info;xatomic32 References;
    xmutex Gate,Retirement;xcond Retired;xnetengine* Engine;xnetworker* Worker;xwsstream* Stream;
    xrpcduplex* Duplex;xrpcduplexoutput* Output;unsigned char* Receive;
    xnetpost Post;uint64 Timer,Token;double TimerAt,MessageEntered;size_t Used,Capacity,ReadTurnBytes;
    bool Active,Posted,EventsGone,NativeClosed,NativeFailed,MessageActive,Complete,Offered,AbortIssued,DuplexClosing,TimerUnavailable,DestroyingNative;
    unsigned CleanupDelay;xrpcwsreason NativeReason;unsigned char Context[];
};
static void xrpcWsWork(xnetworker*,ptr);
static void xrpcWsTimer(xnetworker*,uint64,xnetresult,ptr);
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
static bool xrpcWsClosing(xrpcws* P)
{xrtMutexLock(&P->Gate);bool B=P->Info.State>=XRPC_WS_CLOSING;xrtMutexUnlock(&P->Gate);return B;}
static void xrpcWsFail(xrpcws* P,xrpcwsreason Reason)
{
    xrtMutexLock(&P->Gate);if(P->Info.State<XRPC_WS_CLOSING){P->Info.State=XRPC_WS_CLOSING;P->Info.Reason=Reason;}xrtMutexUnlock(&P->Gate);
}
static xrpcwsreason xrpcWsLocalFailure(void)
{return xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_WS_MEMORY:xrtErrorKind(xrtGetError())==XERR_RANGE?XRPC_WS_LIMIT:XRPC_WS_TRANSPORT;}
static xrpccallstatus xrpcWsCallFailure(xrpcwsreason Reason)
{return Reason==XRPC_WS_LOCAL?XRPC_CALL_CLOSED:Reason==XRPC_WS_PROTOCOL?XRPC_CALL_PROTOCOL:Reason==XRPC_WS_LIMIT?XRPC_CALL_LIMIT:Reason==XRPC_WS_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT;}
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
        xstrview Wire=xrpcDuplexOutputWire(P->Output);size_t Required=0;
        if(Wire.Size>P->Config.MaxMessageBytes || !xrtWsStreamSendSize(P->Stream,Wire.Size,&Required) || Required>xrtWsStreamWriteLimit(P->Stream)){xrpcWsFail(P,XRPC_WS_LIMIT);return;}
        if(Wire.Size>Budget){xrpcWsWake(P);return;}
        if(Required>xrtWsStreamWritable(P->Stream))return;
        if(!P->Offered){if(!xrpcDuplexStarted(P->Duplex,P->Token)){xrpcWsWake(P);return;}P->Offered=true;}
        if(!xrpcDuplexOutputRetain(P->Output)){xrpcWsFail(P,xrpcWsLocalFailure());return;}
        xnetref Ref={(cbytes)Wire.Data,Wire.Size,xrpcWsWireDrop,P->Output};xnetresult R=xrtWsStreamSendRef(P->Stream,XWS_OPCODE_TEXT,&Ref);
        if(R!=XNET_RESULT_OK){xrpcDuplexOutputRelease(P->Output);if(R!=XNET_RESULT_AGAIN){xrpcWsNativeError(P,xrtGetError());xrpcWsFail(P,R==XNET_RESULT_ERROR?xrpcWsLocalFailure():XRPC_WS_TRANSPORT);}return;}
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
        if(P->Stream && !P->AbortIssued){P->AbortIssued=true;if(xrtWsStreamState(P->Stream)!=XWS_STREAM_CLOSED)(void)xrtWsStreamAbort(P->Stream);}
        if(P->Stream && xrtWsStreamPollRetired(P->Stream)==XWAIT_OK){xwsstream* S=P->Stream;P->Stream=NULL;P->DestroyingNative=true;xrtWsStreamDestroy(S);P->DestroyingNative=false;}
        xrtMutexLock(&P->Gate);xrpcwsreason Reason=P->Info.Reason;size_t Entries=P->Info.Submitters;xrtMutexUnlock(&P->Gate);
        if(!P->DuplexClosing){P->DuplexClosing=true;(void)xrpcDuplexClose(P->Duplex,xrpcWsCallFailure(Reason));}
        (void)xrpcDuplexStep(P->Duplex);xrpcDuplexInfo(P->Duplex,&D);
        bool Done=!Entries&&D.Drained&&!P->Stream&&P->EventsGone;
        if(Done){xrtMutexLock(&P->Gate);Done=P->Info.Submitters==0;xrtMutexUnlock(&P->Gate);}
        if(Done && xrpcDuplexDestroy(P->Duplex)){P->Duplex=NULL;xrtMutexLock(&P->Gate);P->Info.State=XRPC_WS_CLOSED;xrtMutexUnlock(&P->Gate);xrpcWsDrop(P);}
        else if(D.More)xrpcWsWake(P);
        else {double At=D.NextDeadline;if(P->Stream){if(!P->CleanupDelay)P->CleanupDelay=1;else if(P->CleanupDelay<32)P->CleanupDelay*=2;double Retry=xrpcWsDeadline(xrtTimer(),P->CleanupDelay);if(Retry<At)At=Retry;}xrpcWsArm(P,At);}
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
    *C=(xrpcwsconfig){0};C->Size=sizeof(*C);xrpcDuplexConfigInit(&C->Duplex);xrtWsStreamConfigInit(&C->Stream);C->MaxMessageBytes=1048576;C->MaxReceiveBytes=3145728;C->KeepReceiveBytes=4096;C->ReadBytes=65536;C->WriteBytes=1048576;C->WorkMessages=32;C->MaxSubmitters=128;C->MessageTimeout=30000;return true;
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
    xrtMutexLock(&P->Gate);P->Active=true;P->Info.State=XRPC_WS_OPEN;xrpcWsRef(P);*Out=P;if(!xrpcWsWakeLocked(P)){P->Info.State=XRPC_WS_CLOSING;P->Info.Reason=XRPC_WS_LIMIT;}xrtMutexUnlock(&P->Gate);return true;
}
XRPC_API bool xrpcWsAttachTcp(xnetstream* S,xnetbuf* Initial,const xrpcwsconfig* C,xrpcws** Out){return xrpcWsAttach(S,false,Initial,C,Out);}
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
XRPC_API bool xrpcWsAttachTls(xtlsstream* S,xnetbuf* Initial,const xrpcwsconfig* C,xrpcws** Out){return xrpcWsAttach(S,true,Initial,C,Out);}
#endif
static xrpcduplexadmission xrpcWsSubmit(xrpcws* P,const xrpcduplexitem* Items,size_t Count,bool Batch,int64 Timeout,xfuture** Future,double Entered)
{
    if(!P || !Future || !xrtMemRangeValid(Future,sizeof(*Future)) || xrtMemRangesOverlap(P,P->Info.FixedBytes,Future,sizeof(*Future)))return xrpcWsReport(XERR_ARGUMENT,"invalid RPC WS submission output"),XRPC_DUPLEX_ERROR;
    xrtMutexLock(&P->Gate);if(P->Info.State>=XRPC_WS_CLOSING){xrtMutexUnlock(&P->Gate);return XRPC_DUPLEX_CLOSED;}
    if(P->Info.Submitters==P->Config.MaxSubmitters){++P->Info.Overloaded;xrtMutexUnlock(&P->Gate);return XRPC_DUPLEX_OVERLOADED;}
    ++P->Info.Submitters;xrpcWsRef(P);xrtMutexUnlock(&P->Gate);
    xrpcduplexadmission A=xrpcDuplexSubmitEntered(P->Duplex,Items,Count,Batch,Timeout,Future,Entered);
    xerror* Error=xrtTakeError();xrtMutexLock(&P->Gate);--P->Info.Submitters;if(P->Info.State>=XRPC_WS_CLOSING)(void)xrpcWsWakeLocked(P);xrtMutexUnlock(&P->Gate);xrpcWsDrop(P);xrtClearError();xrtSetErrorTake(Error);return A;
}
XRPC_API xrpcduplexadmission xrpcWsCall(xrpcws* P,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double E=xrtTimer();xrpcduplexitem I={false,Method,Params};return xrpcWsSubmit(P,&I,1,false,Timeout,Future,E);}
XRPC_API xrpcduplexadmission xrpcWsNotify(xrpcws* P,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double E=xrtTimer();xrpcduplexitem I={true,Method,Params};return xrpcWsSubmit(P,&I,1,false,Timeout,Future,E);}
XRPC_API xrpcduplexadmission xrpcWsBatch(xrpcws* P,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** Future)
{return xrpcWsSubmit(P,Items,Count,true,Timeout,Future,xrtTimer());}
XRPC_API bool xrpcWsClose(xrpcws* P)
{if(!P)return xrpcWsReport(XERR_ARGUMENT,"RPC WS peer is NULL");xrpcWsFail(P,XRPC_WS_LOCAL);xrtMutexLock(&P->Gate);bool OK=xrpcWsWakeLocked(P);xrtMutexUnlock(&P->Gate);return OK;}
XRPC_API bool xrpcWsInfo(xrpcws* P,xrpcwsinfo* I)
{
    if(!P || !I || !xrtMemRangeValid(I,sizeof(*I)) || I->Size!=sizeof(*I) || xrtMemRangesOverlap(P,P->Info.FixedBytes,I,sizeof(*I)))return xrpcWsReport(XERR_ARGUMENT,"invalid RPC WS info output");
    xrtMutexLock(&P->Gate);*I=P->Info;xrtMutexUnlock(&P->Gate);return true;
}
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
#endif
