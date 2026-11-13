#include <xrpc/tcp.h>
#include "../internal/xrpc_duplex_internal.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <math.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_TCP)
struct xrpctcp {
    xrpctcpconfig Config;
    xrpctcpinfo Info;
    xatomic32 References;
    xmutex Gate,Retirement;
    xcond Retired;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetstream* Stream;
    xnetaddr Remote;
    xrpcduplex* Duplex;
    xrpcframereader* Reader;
    xrpcduplexoutput* Output;
    xnetpost Post;
    uint64 Timer,Token;
    double TimerAt,ConnectDeadline,FrameEntered,ReadEpoch,ChunkEpoch;
    size_t Used,Offset,HeaderBytes,HeaderOffset,BodyOffset;
    bool Active,Started,Posted,EventsGone,NativeClosed,ReadEnded,Open,Offered,AbortIssued,DuplexClosing,TimerUnavailable;
    unsigned char Header[64];
    unsigned char Data[];
};
static void xrpcTcpWork(xnetworker*,ptr);
static void xrpcTcpTimer(xnetworker*,uint64,xnetresult,ptr);
static bool xrpcTcpReport(xerrkind Kind,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",19001,Message);return false;}
static double xrpcTcpDeadline(double Entered,int64 Timeout)
{return Timeout==XRT_WAIT_FOREVER?INFINITY:Timeout==0?Entered:nextafter(Entered+(double)(Timeout/1000)+(double)(Timeout%1000)*.001,INFINITY);}
static int64 xrpcTcpRemaining(double At)
{if(At==INFINITY)return XRT_WAIT_FOREVER;double N=ceil((At-xrtTimer())*1000.);return N<=0?0:N>=0x1p63?INT64_MAX:(int64)N;}
static void xrpcTcpRef(xrpctcp* P)
{xrtAtomic32FetchAdd(&P->References,1,XMEMORY_RELAXED);}
static void xrpcTcpDrop(const void* Data)
{
    xrpctcp* P=(xrpctcp*)Data;xrtMutexLock(&P->Retirement);
    xrtAtomic32FetchSub(&P->References,1,XMEMORY_RELEASE);xrtCondBroadcast(&P->Retired);xrtMutexUnlock(&P->Retirement);
}
static bool xrpcTcpCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpctcp*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcTcpTrace(const void* Data,xrtownershipvisitor Visit,ptr User)
{(void)Data;(void)Visit;(void)User;return false;}
static const xrtownershipops xrpcTcpOps={xrpcTcpCount,xrpcTcpTrace};
static bool xrpcTcpWakeLocked(xrpctcp* P)
{
    if(!P->Active || P->Posted || P->Info.State==XRPC_TCP_CLOSED)return true;
    P->Posted=true;xrpcTcpRef(P);
    if(xrtNetPost(P->Worker,&P->Post,xrpcTcpWork,P))return true;
    P->Posted=false;++P->Info.PostFailures;xrpcTcpDrop(P);return false;
}
static void xrpcTcpWake(ptr Data)
{xrpctcp* P=Data;xrtMutexLock(&P->Gate);(void)xrpcTcpWakeLocked(P);xrtMutexUnlock(&P->Gate);}
static bool xrpcTcpClosing(xrpctcp* P)
{xrtMutexLock(&P->Gate);bool B=P->Info.State>=XRPC_TCP_CLOSING;xrtMutexUnlock(&P->Gate);return B;}
static void xrpcTcpFail(xrpctcp* P,xrpctcpreason Reason)
{
    xrtMutexLock(&P->Gate);if(P->Info.State<XRPC_TCP_CLOSING){P->Info.State=XRPC_TCP_CLOSING;P->Info.Reason=Reason;}xrtMutexUnlock(&P->Gate);
}
static xrpctcpreason xrpcTcpLocalFailure(void)
{return xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_TCP_MEMORY:xrtErrorKind(xrtGetError())==XERR_RANGE?XRPC_TCP_LIMIT:XRPC_TCP_TRANSPORT;}
static xrpccallstatus xrpcTcpCallFailure(xrpctcpreason Reason)
{return Reason==XRPC_TCP_LOCAL?XRPC_CALL_CLOSED:Reason==XRPC_TCP_PROTOCOL?XRPC_CALL_PROTOCOL:Reason==XRPC_TCP_LIMIT?XRPC_CALL_LIMIT:Reason==XRPC_TCP_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT;}
static void xrpcTcpTimerCancel(xrpctcp* P)
{uint64 T=P->Timer;P->Timer=0;P->TimerAt=INFINITY;if(T)(void)xrtNetEngineTimerCancelCurrent(P->Engine,T);}
static void xrpcTcpArm(xrpctcp* P,double At)
{
    if(P->TimerUnavailable)return;
    if(At==INFINITY){xrpcTcpTimerCancel(P);return;}
    if(P->Timer && P->TimerAt<=At)return;
    xrpcTcpTimerCancel(P);xrpcTcpRef(P);P->TimerAt=At;
    P->Timer=xrtNetEngineAfter(P->Engine,xrtNetWorkerIndex(P->Worker),xrpcTcpRemaining(At),xrpcTcpTimer,P);
    if(!P->Timer){P->TimerUnavailable=true;P->TimerAt=INFINITY;xrpcTcpDrop(P);xrtMutexLock(&P->Gate);++P->Info.TimerFailures;xrtMutexUnlock(&P->Gate);xrpcTcpFail(P,XRPC_TCP_LIMIT);xrpcTcpWake(P);}
}
static void xrpcTcpTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xrpctcp* P=Data;if(P->Timer==Id){P->Timer=0;P->TimerAt=INFINITY;
        if(Result!=XNET_RESULT_OK && Result!=XNET_RESULT_CANCELLED)xrpcTcpFail(P,XRPC_TCP_ENGINE);
        xrpcTcpWake(P);}
    xrpcTcpDrop(P);
}
static void xrpcTcpOnOpen(xnetstream* Stream,ptr Data)
{
    (void)Stream;xrpctcp* P=Data;
    if(xrtTimer()>=P->ConnectDeadline){xrpcTcpFail(P,XRPC_TCP_TIMEOUT);xrpcTcpWake(P);return;}
    P->Open=true;xrtMutexLock(&P->Gate);if(P->Info.State<XRPC_TCP_CLOSING)P->Info.State=XRPC_TCP_OPEN;xrtMutexUnlock(&P->Gate);xrpcTcpWake(P);
}
static void xrpcTcpOnRead(xnetstream* Stream,xnetbuf* Buffer,ptr Data)
{(void)Buffer;xrpctcp* P=Data;if(!P->ReadEpoch)P->ReadEpoch=xrtTimer();xrtNetStreamPause(Stream);xrpcTcpWake(P);}
static void xrpcTcpOnEnd(xnetstream* Stream,ptr Data)
{(void)Stream;xrpctcp* P=Data;P->ReadEnded=true;xrpcTcpWake(P);}
static void xrpcTcpOnLow(xnetstream* Stream,size_t Queued,ptr Data)
{(void)Stream;(void)Queued;xrpcTcpWake(Data);}
static void xrpcTcpOnDrain(xnetstream* Stream,ptr Data)
{(void)Stream;xrpcTcpWake(Data);}
static void xrpcTcpOnClose(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{
    (void)Stream;xrpctcp* P=Data;P->NativeClosed=true;
    xrpcTcpFail(P,Result==XNET_RESULT_TIMEOUT?XRPC_TCP_TIMEOUT:xrtErrorKind(Error)==XERR_MEMORY?XRPC_TCP_MEMORY:XRPC_TCP_TRANSPORT);xrpcTcpWake(P);
}
static void xrpcTcpEventsDrop(const void* Data)
{xrpctcp* P=(xrpctcp*)Data;P->EventsGone=true;xrpcTcpWake(P);xrpcTcpDrop(P);}
static const xnetstreameventsownershipv1 xrpcTcpEvents={sizeof(xnetstreameventsownershipv1),{xrpcTcpOnOpen,xrpcTcpOnRead,xrpcTcpOnEnd,NULL,xrpcTcpOnLow,xrpcTcpOnDrain,xrpcTcpOnClose},xrpcTcpEventsDrop,&xrpcTcpOps};
static void xrpcTcpRead(xrpctcp* P)
{
    size_t Budget=P->Config.ReadBytes,Units=P->Config.WorkFrames;
    while(Units-- && !xrpcTcpClosing(P)){
        if(P->Offset==P->Used){P->Offset=P->Used=0;size_t N=xrtNetStreamAvailable(P->Stream);if(!N || !Budget)break;
            if(N>P->Config.ReadChunkBytes)N=P->Config.ReadChunkBytes;
            if(N>Budget)N=Budget;
            P->Used=xrtNetStreamRead(P->Stream,P->Data,N);if(!P->Used)break;Budget-=P->Used;
            P->ChunkEpoch=P->ReadEpoch?P->ReadEpoch:xrtTimer();xrtMutexLock(&P->Gate);P->Info.ReadBytes+=P->Used;xrtMutexUnlock(&P->Gate);}
        if(!P->FrameEntered)P->FrameEntered=P->ChunkEpoch;
        if(xrtTimer()>=xrpcTcpDeadline(P->FrameEntered,P->Config.FrameTimeout)){xrpcTcpFail(P,XRPC_TCP_TIMEOUT);break;}
        size_t Used=0;xrpcframeview Frame;xrpcframestatus S=xrpcFrameReaderFeed(P->Reader,(xbytesview){P->Data+P->Offset,P->Used-P->Offset},&Used,&Frame);P->Offset+=Used;
        if(S==XRPC_FRAME_READY){
            xrpcduplexadmission A=xrpcDuplexReceiveEntered(P->Duplex,(xstrview){(cstr)Frame.Body.Data,Frame.Body.Size},P->Config.RequestContext,P->FrameEntered);P->FrameEntered=0;
            if(A!=XRPC_DUPLEX_ACCEPTED){if(A==XRPC_DUPLEX_OVERLOADED){xrtMutexLock(&P->Gate);++P->Info.Overloaded;xrtMutexUnlock(&P->Gate);xrpcTcpFail(P,XRPC_TCP_LIMIT);}
                else xrpcTcpFail(P,A==XRPC_DUPLEX_PROTOCOL?XRPC_TCP_PROTOCOL:A==XRPC_DUPLEX_CLOSED?XRPC_TCP_LOCAL:xrpcTcpLocalFailure());
                break;}
            xrtMutexLock(&P->Gate);++P->Info.ReadFrames;xrtMutexUnlock(&P->Gate);
        }else if(S!=XRPC_FRAME_MORE){xrpcTcpFail(P,S==XRPC_FRAME_MEMORY?XRPC_TCP_MEMORY:S==XRPC_FRAME_LIMIT?XRPC_TCP_LIMIT:XRPC_TCP_PROTOCOL);break;}
        else if(!Used)break;
    }
    if(xrpcTcpClosing(P))return;
    if(P->Offset<P->Used || xrtNetStreamAvailable(P->Stream))xrpcTcpWake(P);
    else if(P->ReadEnded){xrpcframestatus S=xrpcFrameReaderEnd(P->Reader);xrpcTcpFail(P,S==XRPC_FRAME_END?XRPC_TCP_TRANSPORT:XRPC_TCP_PROTOCOL);}
    else {P->ReadEpoch=0;(void)xrtNetStreamResume(P->Stream);}
}
static void xrpcTcpOutputClear(xrpctcp* P)
{xrpcDuplexOutputRelease(P->Output);P->Output=NULL;P->Token=0;P->HeaderBytes=P->HeaderOffset=P->BodyOffset=0;P->Offered=false;}
static void xrpcTcpWrite(xrpctcp* P)
{
    size_t Budget=P->Config.WriteBytes,Units=P->Config.WorkFrames;
    while(Budget && Units-- && !xrpcTcpClosing(P)){
        xrpcduplexinfo D={0};D.Size=sizeof(D);if(!xrpcDuplexInfo(P->Duplex,&D)){xrpcTcpFail(P,XRPC_TCP_PROTOCOL);break;}
        if(P->Output && !D.Selected)xrpcTcpOutputClear(P);
        if(!P->Output){int N=xrpcDuplexNext(P->Duplex,&P->Output,&P->Token);if(N<0){xrpcTcpFail(P,xrpcTcpLocalFailure());
                break;}if(!N)return;
            xstrview Body=xrpcDuplexOutputWire(P->Output);
            if(!xrpcFrameHeaderWrite(Body.Size,&P->Config.Frame,P->Header,sizeof(P->Header),&P->HeaderBytes)){xrpcTcpFail(P,XRPC_TCP_LIMIT);break;}}
        size_t N=xrtNetStreamWritable(P->Stream);if(!N)return;if(N>Budget)N=Budget;if(N>P->Config.WriteChunkBytes)N=P->Config.WriteChunkBytes;
        xstrview Body=xrpcDuplexOutputWire(P->Output);const unsigned char* Bytes;
        if(P->HeaderOffset<P->HeaderBytes){size_t Left=P->HeaderBytes-P->HeaderOffset;if(N>Left)N=Left;Bytes=P->Header+P->HeaderOffset;}
        else {size_t Left=Body.Size-P->BodyOffset;if(N>Left)N=Left;Bytes=(const unsigned char*)Body.Data+P->BodyOffset;}
        if(!P->Offered){if(!xrpcDuplexStarted(P->Duplex,P->Token)){xrpcTcpWake(P);return;}P->Offered=true;}
        xnetresult S=xrtNetStreamSend(P->Stream,Bytes,N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xrpcTcpFail(P,S==XNET_RESULT_ERROR?xrpcTcpLocalFailure():XRPC_TCP_TRANSPORT);return;}
        if(P->HeaderOffset<P->HeaderBytes)P->HeaderOffset+=N;else P->BodyOffset+=N;
        Budget-=N;xrtMutexLock(&P->Gate);P->Info.WriteBytes+=N;xrtMutexUnlock(&P->Gate);
        if(P->HeaderOffset==P->HeaderBytes && P->BodyOffset==Body.Size){
            if(!xrpcDuplexSent(P->Duplex,P->Token)){xrpcTcpFail(P,XRPC_TCP_PROTOCOL);break;}
            xrtMutexLock(&P->Gate);++P->Info.WriteFrames;xrtMutexUnlock(&P->Gate);xrpcTcpOutputClear(P);}
    }
    if(!xrpcTcpClosing(P) && xrtNetStreamWritable(P->Stream))xrpcTcpWake(P);
}
static void xrpcTcpWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xrpctcp* P=Data;xrtMutexLock(&P->Gate);P->Posted=false;bool Closed=P->Info.State==XRPC_TCP_CLOSED;xrtMutexUnlock(&P->Gate);
    if(Closed){xrpcTcpDrop(P);return;}
    if(!P->Started){P->Started=true;
        if(!P->Stream && !xrpcTcpClosing(P)){
            if(xrtTimer()>=P->ConnectDeadline)xrpcTcpFail(P,XRPC_TCP_TIMEOUT);
            else {xnetstreamconfig C=P->Config.Stream;C.ConnectTimeout=xrpcTcpRemaining(P->ConnectDeadline);xrpcTcpRef(P);
                P->Stream=xrtNetStreamConnectOwnedV1(P->Engine,&P->Remote,xrtNetWorkerIndex(P->Worker),&C,P,&xrpcTcpEvents);
                if(!P->Stream){xrpcTcpDrop(P);P->NativeClosed=P->EventsGone=true;xrpcTcpFail(P,xrpcTcpLocalFailure());}}}
        if(!P->Stream)P->NativeClosed=P->EventsGone=true;
    }
    if(!xrpcDuplexStep(P->Duplex))xrpcTcpFail(P,xrpcTcpLocalFailure());
    xrpcduplexinfo D={0};D.Size=sizeof(D);xrpcDuplexInfo(P->Duplex,&D);
    if(D.Closing && !xrpcTcpClosing(P))xrpcTcpFail(P,D.Reason==XRPC_CALL_PROTOCOL?XRPC_TCP_PROTOCOL:D.Reason==XRPC_CALL_LIMIT?XRPC_TCP_LIMIT:D.Reason==XRPC_CALL_MEMORY?XRPC_TCP_MEMORY:XRPC_TCP_TRANSPORT);
    if(!xrpcTcpClosing(P)){
        if(!P->Open && xrtTimer()>=P->ConnectDeadline)xrpcTcpFail(P,XRPC_TCP_TIMEOUT);
        if(P->FrameEntered && xrtTimer()>=xrpcTcpDeadline(P->FrameEntered,P->Config.FrameTimeout))xrpcTcpFail(P,XRPC_TCP_TIMEOUT);
        if(P->Open && !xrpcTcpClosing(P)){xrpcTcpRead(P);if(!xrpcTcpClosing(P))xrpcTcpWrite(P);}
    }
    xrpcDuplexInfo(P->Duplex,&D);
    if(D.Closing && !xrpcTcpClosing(P))xrpcTcpFail(P,D.Reason==XRPC_CALL_PROTOCOL?XRPC_TCP_PROTOCOL:D.Reason==XRPC_CALL_LIMIT?XRPC_TCP_LIMIT:D.Reason==XRPC_CALL_MEMORY?XRPC_TCP_MEMORY:XRPC_TCP_TRANSPORT);
    bool Done=false;
    if(xrpcTcpClosing(P)){
        xrpcTcpTimerCancel(P);xrpcTcpOutputClear(P);
        if(P->Stream && !P->AbortIssued){P->AbortIssued=true;(void)xrtNetStreamAbort(P->Stream);}
        xrtMutexLock(&P->Gate);xrpctcpreason Reason=P->Info.Reason;size_t Entries=P->Info.Submitters;xrtMutexUnlock(&P->Gate);
        if(!P->DuplexClosing){P->DuplexClosing=true;(void)xrpcDuplexClose(P->Duplex,xrpcTcpCallFailure(Reason));}
        (void)xrpcDuplexStep(P->Duplex);xrpcDuplexInfo(P->Duplex,&D);
        if(!Entries && D.Drained && P->EventsGone){
            /* Recheck admission under Gate before consuming the endpoint. */
            xrtMutexLock(&P->Gate);Done=P->Info.Submitters==0;xrtMutexUnlock(&P->Gate);
            if(Done && xrpcDuplexDestroy(P->Duplex))P->Duplex=NULL;else Done=false;
        }
        if(!Done && D.More)xrpcTcpWake(P);
        else if(!Done && isfinite(D.NextDeadline))xrpcTcpArm(P,D.NextDeadline);
    }else {
        double At=D.NextDeadline;if(!P->Open && P->ConnectDeadline<At)At=P->ConnectDeadline;
        double Frame=P->FrameEntered?xrpcTcpDeadline(P->FrameEntered,P->Config.FrameTimeout):INFINITY;if(Frame<At)At=Frame;
        if(D.More)xrpcTcpWake(P);else xrpcTcpArm(P,At);
    }
    xrpcframeinfo F={0};F.Size=sizeof(F);xrpcFrameReaderInfo(P->Reader,&F);
    xrtMutexLock(&P->Gate);P->Info.Duplex=D;P->Info.FrameStorageBytes=F.StorageBytes;
    if(Done)P->Info.State=XRPC_TCP_CLOSED;
    xrtMutexUnlock(&P->Gate);
    if(Done){xrpcTcpTimerCancel(P);xrpcTcpDrop(P);} /* Runtime reference. */
    xrpcTcpDrop(P); /* Current post. */
}
XRPC_API bool xrpcTcpConfigInit(xrpctcpconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcTcpReport(XERR_ARGUMENT,"invalid RPC TCP config output");
    *C=(xrpctcpconfig){0};C->Size=sizeof(*C);xrpcDuplexConfigInit(&C->Duplex);xrpcFrameConfigInit(&C->Frame);xrtNetStreamConfigInit(&C->Stream);
    C->ReadChunkBytes=4096;C->ReadBytes=C->WriteBytes=65536;C->WriteChunkBytes=16384;C->WorkFrames=32;C->MaxSubmitters=128;C->FrameTimeout=30000;return true;
}
static void xrpcTcpFree(xrpctcp* P)
{
    if(P->Duplex){(void)xrpcDuplexClose(P->Duplex,XRPC_CALL_CLOSED);(void)xrpcDuplexStep(P->Duplex);(void)xrpcDuplexDestroy(P->Duplex);}
    xrpcFrameReaderDestroy(P->Reader);xrtNetStreamDestroy(P->Stream);
    xrtSecureZero((ptr)P->Config.RequestContext.Data,P->Config.RequestContext.Size);xrtCondUnit(&P->Retired);xrtMutexUnit(&P->Retirement);xrtMutexUnit(&P->Gate);
    (void)xrtNetEngineUnpin(P->Engine);xrtFree(P);
}
static xrpctcp* xrpcTcpCreate(xnetengine* E,xnetworker* W,const xrpctcpconfig* Config,xrpctcp** Out,double Entered)
{
    if(!E || !W || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)))))goto Argument;
    xrpctcpconfig C;if(Config)C=*Config;else xrpcTcpConfigInit(&C);
    if(C.Size!=sizeof(C) || C.Duplex.Wake || C.Duplex.User || !C.ReadChunkBytes || C.ReadChunkBytes>1048576u ||
       !C.ReadBytes || !C.WriteBytes || !C.WriteChunkBytes || !C.WorkFrames || C.WorkFrames>65536 || !C.MaxSubmitters || C.MaxSubmitters>65536 || C.FrameTimeout<XRT_WAIT_FOREVER ||
       C.RequestContext.Size>16384 || !xrtMemRangeValid(C.RequestContext.Data,C.RequestContext.Size) || xrtMemRangesOverlap(C.RequestContext.Data,C.RequestContext.Size,Out,sizeof(*Out)) ||
       !xrtNetStreamConfigValid(&C.Stream) || C.ReadChunkBytes>SIZE_MAX-sizeof(xrpctcp)-C.RequestContext.Size)goto Argument;
    if(!xrtNetEnginePin(E))return NULL;
    size_t Bytes=sizeof(xrpctcp)+C.ReadChunkBytes+C.RequestContext.Size;xrpctcp* P=xrtCalloc(1,Bytes);if(!P){xrtNetEngineUnpin(E);goto Memory;}
    P->Engine=E;P->Worker=W;P->Config=C;P->ConnectDeadline=xrpcTcpDeadline(Entered,C.Stream.ConnectTimeout);P->TimerAt=INFINITY;
    P->Info.Size=sizeof(P->Info);P->Info.State=XRPC_TCP_CONNECTING;P->Info.FixedBytes=Bytes;xrtAtomic32Init(&P->References,1);
    if(!xrtMutexInit(&P->Gate)){xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    if(!xrtMutexInit(&P->Retirement)){xrtMutexUnit(&P->Gate);xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    if(!xrtCondInit(&P->Retired)){xrtMutexUnit(&P->Retirement);xrtMutexUnit(&P->Gate);xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    P->Config.RequestContext=(xbytesview){P->Data+C.ReadChunkBytes,C.RequestContext.Size};if(C.RequestContext.Size)memcpy((ptr)P->Config.RequestContext.Data,C.RequestContext.Data,C.RequestContext.Size);
    C.Duplex.Wake=xrpcTcpWake;C.Duplex.User=P;
    if(!xrtNetPostInit(&P->Post) || !xrpcFrameReaderCreate(&C.Frame,&P->Reader) || !xrpcDuplexCreate(&C.Duplex,&P->Duplex)){xrpcTcpFree(P);return NULL;}
    P->Info.Duplex.Size=sizeof(P->Info.Duplex);xrpcDuplexInfo(P->Duplex,&P->Info.Duplex);
    return P;
Argument:
    xrpcTcpReport(XERR_ARGUMENT,"invalid RPC TCP factory inputs/configuration");return NULL;
Memory:
    xrpcTcpReport(XERR_MEMORY,"RPC TCP allocation failed");return NULL;
}
XRPC_API bool xrpcTcpConnect(xnetengine* E,const xnetaddr* Remote,uint64 Affinity,const xrpctcpconfig* Config,xrpctcp** Out)
{
    double Entered=xrtTimer();if(!E || !Remote || !xrtMemRangeValid(Remote,sizeof(*Remote)) || !Out || xrtMemRangesOverlap(Remote,sizeof(*Remote),Out,sizeof(*Out)) ||
       (Remote->Family!=XNET_FAMILY_IPV4 && Remote->Family!=XNET_FAMILY_IPV6) || !Remote->Port)return xrpcTcpReport(XERR_ARGUMENT,"invalid RPC TCP numeric address/output");
    uint32 Count=xrtNetEngineWorkerCount(E);if(!Count)return xrpcTcpReport(XERR_STATE,"RPC TCP engine has no worker");
    xrpctcp* P=xrpcTcpCreate(E,xrtNetEngineWorker(E,(uint32)(Affinity%Count)),Config,Out,Entered);if(!P)return false;P->Remote=*Remote;
    xrtMutexLock(&P->Gate);P->Active=true;xrpcTcpRef(P);bool Posted=xrpcTcpWakeLocked(P);if(Posted)*Out=P;else P->Active=false;xrtMutexUnlock(&P->Gate);
    if(!Posted){xrpcTcpDrop(P);xrpcTcpFree(P);}return Posted;
}
XRPC_API bool xrpcTcpAttach(xnetstream* Stream,const xrpctcpconfig* Config,xrpctcp** Out)
{
    double Entered=xrtTimer();if(!Stream)return xrpcTcpReport(XERR_ARGUMENT,"RPC TCP stream is NULL");xnetworker* W=xrtNetStreamWorker(Stream);
    if(!xrtNetWorkerIsCurrent(W) || xrtNetStreamState(Stream)!=XNET_STREAM_OPEN)return xrpcTcpReport(XERR_STATE,"RPC TCP attach requires the open stream worker");
    xrpctcp* P=xrpcTcpCreate(xrtNetWorkerEngine(W),W,Config,Out,Entered);if(!P)return false;
    xrpcTcpRef(P);if(!xrtNetStreamSetEventsOwnedV1(Stream,P,&xrpcTcpEvents)){xrpcTcpDrop(P);xrpcTcpFree(P);return false;}
    P->Stream=Stream;P->Open=true;P->ConnectDeadline=INFINITY;if(xrtNetStreamAvailable(Stream))P->ReadEpoch=Entered;
    xrtMutexLock(&P->Gate);P->Active=true;P->Info.State=XRPC_TCP_OPEN;xrpcTcpRef(P);*Out=P;(void)xrpcTcpWakeLocked(P);xrtMutexUnlock(&P->Gate);return true;
}
static xrpcduplexadmission xrpcTcpSubmit(xrpctcp* P,const xrpcduplexitem* Items,size_t Count,bool Batch,int64 Timeout,xfuture** Future,double Entered)
{
    if(!P || !Future || !xrtMemRangeValid(Future,sizeof(*Future)) || xrtMemRangesOverlap(P,P->Info.FixedBytes,Future,sizeof(*Future)))return xrpcTcpReport(XERR_ARGUMENT,"invalid RPC TCP submission output"),XRPC_DUPLEX_ERROR;
    xrtMutexLock(&P->Gate);if(P->Info.State>=XRPC_TCP_CLOSING){xrtMutexUnlock(&P->Gate);return XRPC_DUPLEX_CLOSED;}
    if(P->Info.Submitters==P->Config.MaxSubmitters){++P->Info.Overloaded;xrtMutexUnlock(&P->Gate);return XRPC_DUPLEX_OVERLOADED;}
    ++P->Info.Submitters;xrpcTcpRef(P);xrtMutexUnlock(&P->Gate);
    xrpcduplexadmission A=xrpcDuplexSubmitEntered(P->Duplex,Items,Count,Batch,Timeout,Future,Entered);
    xerror* Error=xrtTakeError();xrtMutexLock(&P->Gate);--P->Info.Submitters;if(P->Info.State>=XRPC_TCP_CLOSING)(void)xrpcTcpWakeLocked(P);xrtMutexUnlock(&P->Gate);xrpcTcpDrop(P);xrtClearError();xrtSetErrorTake(Error);return A;
}
XRPC_API xrpcduplexadmission xrpcTcpCall(xrpctcp* P,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double E=xrtTimer();xrpcduplexitem I={false,Method,Params};return xrpcTcpSubmit(P,&I,1,false,Timeout,Future,E);}
XRPC_API xrpcduplexadmission xrpcTcpNotify(xrpctcp* P,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double E=xrtTimer();xrpcduplexitem I={true,Method,Params};return xrpcTcpSubmit(P,&I,1,false,Timeout,Future,E);}
XRPC_API xrpcduplexadmission xrpcTcpBatch(xrpctcp* P,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** Future)
{return xrpcTcpSubmit(P,Items,Count,true,Timeout,Future,xrtTimer());}
XRPC_API bool xrpcTcpClose(xrpctcp* P)
{if(!P)return xrpcTcpReport(XERR_ARGUMENT,"RPC TCP peer is NULL");xrpcTcpFail(P,XRPC_TCP_LOCAL);xrtMutexLock(&P->Gate);bool OK=xrpcTcpWakeLocked(P);xrtMutexUnlock(&P->Gate);return OK;}
XRPC_API bool xrpcTcpInfo(xrpctcp* P,xrpctcpinfo* I)
{
    if(!P || !I || !xrtMemRangeValid(I,sizeof(*I)) || I->Size!=sizeof(*I) || xrtMemRangesOverlap(P,P->Info.FixedBytes,I,sizeof(*I)))return xrpcTcpReport(XERR_ARGUMENT,"invalid RPC TCP info output");
    xrtMutexLock(&P->Gate);*I=P->Info;xrtMutexUnlock(&P->Gate);return true;
}
XRPC_API xwaitresult xrpcTcpPollRetired(xrpctcp* P)
{
    if(!P){xrpcTcpReport(XERR_ARGUMENT,"RPC TCP peer is NULL");return XWAIT_ERROR;}
    xmutextryresult L=xrtMutexTryLockResult(&P->Retirement);if(L!=XMUTEX_TRY_OK)return L==XMUTEX_TRY_BUSY?XWAIT_TIMEOUT:XWAIT_ERROR;
    bool Alone=xrtAtomic32Load(&P->References,XMEMORY_ACQUIRE)==1;xrtMutexUnlock(&P->Retirement);
    L=xrtMutexTryLockResult(&P->Gate);if(L!=XMUTEX_TRY_OK)return L==XMUTEX_TRY_BUSY?XWAIT_TIMEOUT:XWAIT_ERROR;
    bool Closed=P->Info.State==XRPC_TCP_CLOSED;if(!Closed && P->Info.State==XRPC_TCP_CLOSING)(void)xrpcTcpWakeLocked(P);xrtMutexUnlock(&P->Gate);
    if(!Alone || !Closed)return XWAIT_TIMEOUT;
    return P->Stream?xrtNetStreamPollRetired(P->Stream):XWAIT_OK;
}
XRPC_API xwaitresult xrpcTcpWaitFor(xrpctcp* P,int64 Timeout)
{
    if(!P || Timeout<XRT_WAIT_FOREVER){xrpcTcpReport(XERR_ARGUMENT,"invalid RPC TCP wait");return XWAIT_ERROR;}
    if(xrtNetEngineCurrent(P->Engine)){xrpcTcpReport(XERR_STATE,"RPC TCP wait on own engine worker");return XWAIT_ERROR;}
    double At=xrpcTcpDeadline(xrtTimer(),Timeout);
    for(;;){xwaitresult R=xrpcTcpPollRetired(P);if(R!=XWAIT_TIMEOUT)return R;int64 Left=xrpcTcpRemaining(At);if(!Left)return XWAIT_TIMEOUT;
        if(Left==XRT_WAIT_FOREVER || Left>1)Left=1;
        xrtMutexLock(&P->Retirement);(void)xrtCondWaitFor(&P->Retired,&P->Retirement,Left);xrtMutexUnlock(&P->Retirement);}
}
XRPC_API bool xrpcTcpDestroy(xrpctcp* P)
{if(!P)return true;if(xrpcTcpPollRetired(P)!=XWAIT_OK)return xrpcTcpReport(XERR_STATE,"RPC TCP peer still has physical duties");xrpcTcpFree(P);return true;}
#endif
