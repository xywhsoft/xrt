#include <xrpc/tls.h>
#include "../internal/xrpc_duplex_internal.h"
#include "../internal/xrpc_tls_connect.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <math.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_TLS)
struct xrpctls {
    xrpctlsconfig Config;
    xrpctlsinfo Info;
    xatomic32 References;
    xmutex Gate,Retirement;
    xcond Retired;
    xnetengine* Engine;
    xnetworker* Worker;
    xtlsstream* Stream;
    xnetstream* Raw;
    xtlssession* Session;
    xnetaddr Remote;
    xrpcduplex* Duplex;
    xrpcframereader* Reader;
    xrpcduplexoutput* Output;
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
    xrpctlsconnect* Connect; /* Cold only; gone after owned native Done retires. */
#endif
    xnetpost Post;
    uint64 Timer,Token;
    double TimerAt,ConnectDeadline,FrameEntered,ReadEpoch,ChunkEpoch;
    size_t Used,Offset,HeaderBytes,HeaderOffset,BodyOffset;
    bool Active,Started,Posted,EventsGone,NativeClosed,ReadEnded,Open,Offered,AbortIssued,DuplexClosing,TimerUnavailable;
    bool RawOpen,RawEventsGone,DestroyingNative,Handoff;
    unsigned CleanupDelay;
    xrpctlsreason NativeReason;
    unsigned char Header[64];
    unsigned char Data[];
};
static void xrpcTlsPeerWork(xnetworker*,ptr);
static void xrpcTlsPeerTimer(xnetworker*,uint64,xnetresult,ptr);
static bool xrpcTlsPeerReport(xerrkind Kind,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",19101,Message);return false;}
static double xrpcTlsPeerDeadline(double Entered,int64 Timeout)
{return Timeout==XRT_WAIT_FOREVER?INFINITY:Timeout==0?Entered:nextafter(Entered+(double)(Timeout/1000)+(double)(Timeout%1000)*.001,INFINITY);}
static int64 xrpcTlsPeerRemaining(double At)
{if(At==INFINITY)return XRT_WAIT_FOREVER;double N=ceil((At-xrtTimer())*1000.);return N<=0?0:N>=0x1p63?INT64_MAX:(int64)N;}
static void xrpcTlsPeerRef(xrpctls* P)
{xrtAtomic32FetchAdd(&P->References,1,XMEMORY_RELAXED);}
static void xrpcTlsPeerDrop(const void* Data)
{
    xrpctls* P=(xrpctls*)Data;xrtMutexLock(&P->Retirement);
    xrtAtomic32FetchSub(&P->References,1,XMEMORY_RELEASE);xrtCondBroadcast(&P->Retired);xrtMutexUnlock(&P->Retirement);
}
static bool xrpcTlsPeerCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpctls*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcTlsPeerTrace(const void* Data,xrtownershipvisitor Visit,ptr User)
{(void)Data;(void)Visit;(void)User;return false;}
static const xrtownershipops xrpcTlsPeerOps={xrpcTlsPeerCount,xrpcTlsPeerTrace};
static bool xrpcTlsPeerWakeLocked(xrpctls* P)
{
    if(!P->Active || P->Posted || P->Info.State==XRPC_TLS_CLOSED)return true;
    P->Posted=true;xrpcTlsPeerRef(P);
    if(xrtNetPost(P->Worker,&P->Post,xrpcTlsPeerWork,P))return true;
    P->Posted=false;++P->Info.PostFailures;xrpcTlsPeerDrop(P);return false;
}
static void xrpcTlsPeerWake(ptr Data)
{xrpctls* P=Data;xrtMutexLock(&P->Gate);(void)xrpcTlsPeerWakeLocked(P);xrtMutexUnlock(&P->Gate);}
static bool xrpcTlsPeerClosing(xrpctls* P)
{xrtMutexLock(&P->Gate);bool B=P->Info.State>=XRPC_TLS_CLOSING;xrtMutexUnlock(&P->Gate);return B;}
static void xrpcTlsPeerFail(xrpctls* P,xrpctlsreason Reason)
{
    xrtMutexLock(&P->Gate);if(P->Info.State<XRPC_TLS_CLOSING){P->Info.State=XRPC_TLS_CLOSING;P->Info.Reason=Reason;}xrtMutexUnlock(&P->Gate);
}
static xrpctlsreason xrpcTlsPeerLocalFailure(void)
{return xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_TLS_MEMORY:xrtErrorKind(xrtGetError())==XERR_RANGE?XRPC_TLS_LIMIT:XRPC_TLS_TRANSPORT;}
static xrpccallstatus xrpcTlsPeerCallFailure(xrpctlsreason Reason)
{return Reason==XRPC_TLS_LOCAL?XRPC_CALL_CLOSED:Reason==XRPC_TLS_PROTOCOL?XRPC_CALL_PROTOCOL:Reason==XRPC_TLS_LIMIT?XRPC_CALL_LIMIT:Reason==XRPC_TLS_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT;}
static void xrpcTlsPeerTimerCancel(xrpctls* P)
{uint64 T=P->Timer;P->Timer=0;P->TimerAt=INFINITY;if(T)(void)xrtNetEngineTimerCancelCurrent(P->Engine,T);}
static void xrpcTlsPeerArm(xrpctls* P,double At)
{
    if(P->TimerUnavailable)return;
    if(At==INFINITY){xrpcTlsPeerTimerCancel(P);return;}
    if(P->Timer && P->TimerAt<=At)return;
    xrpcTlsPeerTimerCancel(P);xrpcTlsPeerRef(P);P->TimerAt=At;
    P->Timer=xrtNetEngineAfter(P->Engine,xrtNetWorkerIndex(P->Worker),xrpcTlsPeerRemaining(At),xrpcTlsPeerTimer,P);
    if(!P->Timer){P->TimerUnavailable=true;P->TimerAt=INFINITY;xrpcTlsPeerDrop(P);xrtMutexLock(&P->Gate);++P->Info.TimerFailures;xrtMutexUnlock(&P->Gate);xrpcTlsPeerFail(P,XRPC_TLS_LIMIT);xrpcTlsPeerWake(P);}
}
static void xrpcTlsPeerTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xrpctls* P=Data;if(P->Timer==Id){P->Timer=0;P->TimerAt=INFINITY;
        if(Result!=XNET_RESULT_OK && Result!=XNET_RESULT_CANCELLED)xrpcTlsPeerFail(P,XRPC_TLS_ENGINE);
        xrpcTlsPeerWake(P);}
    xrpcTlsPeerDrop(P);
}
static void xrpcTlsPeerNativeError(xrpctls* P,const xerror* Error)
{
    if(!Error)return;
    xstrview Domain=xrtErrorDomainView(Error);size_t N=Domain.Size;if(N>63)N=63;
    xrtMutexLock(&P->Gate);P->Info.NativeErrorKind=xrtErrorKind(Error);P->Info.NativeErrorCode=xrtErrorCode(Error);
    P->Info.NativeErrorDomainTruncated=Domain.Size>N;if(N)memcpy(P->Info.NativeErrorDomain,Domain.Data,N);P->Info.NativeErrorDomain[N]=0;xrtMutexUnlock(&P->Gate);
}
static xrpctlsreason xrpcTlsPeerStreamFailure(xrpctls* P)
{
    const xerror* Error=xrtTlsStreamError(P->Stream);if(!Error)Error=xrtGetError();xrpcTlsPeerNativeError(P,Error);
    return xrtErrorKind(Error)==XERR_MEMORY?XRPC_TLS_MEMORY:XRPC_TLS_TRANSPORT;
}
static void xrpcTlsPeerOnOpen(xtlsstream* Stream,ptr Data)
{
    xrpctls* P=Data;if(xrtTimer()>=P->ConnectDeadline){xrpcTlsPeerFail(P,XRPC_TLS_TIMEOUT);xrpcTlsPeerWake(P);return;}
    size_t Minimum=0;if(!xrtTlsStreamSendBound(Stream,1,&Minimum) || Minimum>P->Config.MaxCipherBytes){xrpcTlsPeerFail(P,XRPC_TLS_LIMIT);xrpcTlsPeerWake(P);return;}
    xtlssession* S=xrtTlsStreamSession(Stream);xbytesview Protocol={0};(void)xrtTlsSessionProtocol(S,&Protocol);
    if(Protocol.Size>255){xrpcTlsPeerFail(P,XRPC_TLS_PROTOCOL);xrpcTlsPeerWake(P);return;}
    P->Open=true;xrtMutexLock(&P->Gate);if(P->Info.State<XRPC_TLS_CLOSING)P->Info.State=XRPC_TLS_OPEN;
    P->Info.Version=xrtTlsSessionVersion(S);P->Info.Cipher=xrtTlsSessionCipher(S);P->Info.Resumed=xrtTlsSessionRole(S)==XTLS_CLIENT?xrtTlsClientResumed(S):xrtTlsServerResumed(S);
    P->Info.ProtocolBytes=Protocol.Size;if(Protocol.Size)memcpy(P->Info.Protocol,Protocol.Data,Protocol.Size);xrtMutexUnlock(&P->Gate);xrpcTlsPeerWake(P);
}
static void xrpcTlsPeerOnRead(xtlsstream* Stream,const xnetbuf* Buffer,ptr Data)
{(void)Stream;(void)Buffer;xrpctls* P=Data;if(!P->ReadEpoch)P->ReadEpoch=xrtTimer();xrpcTlsPeerWake(P);}
static void xrpcTlsPeerOnEnd(xtlsstream* Stream,ptr Data)
{(void)Stream;xrpctls* P=Data;P->ReadEnded=true;xrpcTlsPeerWake(P);}
static void xrpcTlsPeerOnWritable(xtlsstream* Stream,ptr Data)
{(void)Stream;xrpcTlsPeerWake(Data);}
static void xrpcTlsPeerOnClose(xtlsstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{
    (void)Stream;xrpctls* P=Data;P->NativeClosed=true;xrpcTlsPeerNativeError(P,Error);
    P->NativeReason=Result==XNET_RESULT_TIMEOUT?XRPC_TLS_TIMEOUT:xrtErrorKind(Error)==XERR_MEMORY?XRPC_TLS_MEMORY:P->Open?XRPC_TLS_TRANSPORT:XRPC_TLS_HANDSHAKE;
    if(!P->Open)xrpcTlsPeerFail(P,P->NativeReason);
    xrpcTlsPeerWake(P);
}
static void xrpcTlsPeerEventsDrop(const void* Data)
{xrpctls* P=(xrpctls*)Data;P->EventsGone=true;if(!P->DestroyingNative)xrpcTlsPeerWake(P);xrpcTlsPeerDrop(P);}
static const xtlsstreamownershipv1 xrpcTlsPeerEvents={sizeof(xtlsstreamownershipv1),{.Open=xrpcTlsPeerOnOpen,.Read=xrpcTlsPeerOnRead,.End=xrpcTlsPeerOnEnd,.Writable=xrpcTlsPeerOnWritable,.Drain=xrpcTlsPeerOnWritable,.Close=xrpcTlsPeerOnClose},xrpcTlsPeerEventsDrop,&xrpcTlsPeerOps};
static void xrpcTlsPeerRawOpen(xnetstream* Stream,ptr Data)
{
    (void)Stream;xrpctls* P=Data;P->RawOpen=true;xrtMutexLock(&P->Gate);if(P->Info.State<XRPC_TLS_CLOSING)P->Info.State=XRPC_TLS_HANDSHAKING;xrtMutexUnlock(&P->Gate);xrpcTlsPeerWake(P);
}
static void xrpcTlsPeerRawRead(xnetstream* Stream,xnetbuf* Buffer,ptr Data)
{(void)Buffer;xrpctls* P=Data;if(!P->Handoff)(void)xrtNetStreamPause(Stream);xrpcTlsPeerWake(P);}
static void xrpcTlsPeerRawEnd(xnetstream* Stream,ptr Data)
{(void)Stream;xrpcTlsPeerFail(Data,XRPC_TLS_TRANSPORT);xrpcTlsPeerWake(Data);}
static void xrpcTlsPeerRawClose(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{
    (void)Stream;xrpctls* P=Data;P->NativeClosed=true;xrpcTlsPeerNativeError(P,Error);xrpcTlsPeerFail(P,Result==XNET_RESULT_TIMEOUT?XRPC_TLS_TIMEOUT:xrtErrorKind(Error)==XERR_MEMORY?XRPC_TLS_MEMORY:XRPC_TLS_TRANSPORT);xrpcTlsPeerWake(P);
}
static void xrpcTlsPeerRawDrop(const void* Data)
{xrpctls* P=(xrpctls*)Data;P->RawEventsGone=true;xrpcTlsPeerWake(P);xrpcTlsPeerDrop(P);}
static const xnetstreameventsownershipv1 xrpcTlsPeerRawEvents={sizeof(xnetstreameventsownershipv1),{.Open=xrpcTlsPeerRawOpen,.Read=xrpcTlsPeerRawRead,.End=xrpcTlsPeerRawEnd,.Close=xrpcTlsPeerRawClose},xrpcTlsPeerRawDrop,&xrpcTlsPeerOps};
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
static xrpctlsreason xrpcTlsPeerConnectFailure(xnetresult Result,const xerror* Cause)
{
    xerrkind Kind=xrtErrorKind(Cause);
    return Result==XNET_RESULT_TIMEOUT || Kind==XERR_TIMEOUT?XRPC_TLS_TIMEOUT:Result==XNET_RESULT_CANCELLED?XRPC_TLS_LOCAL:
        Kind==XERR_MEMORY?XRPC_TLS_MEMORY:(Kind==XERR_AGAIN || Kind==XERR_RANGE)?XRPC_TLS_LIMIT:XRPC_TLS_TRANSPORT;
}
static void xrpcTlsPeerConnectRelease(xrpctls* P)
{xrpctlsconnect* C=P->Connect;P->Connect=NULL;if(C)C->Drop(C->Data);}
void xrpcTlsConnectRetired(xrpctls* P)
{P->Connect->Pending=false;xrpcTlsPeerConnectRelease(P);xrpcTlsPeerWake(P);}
void xrpcTlsConnected(xrpctls* P,xnetresult Result,xnetstream* Stream,const xerror* Cause)
{
    xrpcTlsPeerNativeError(P,Cause);
    if(xrtTimer()>=P->ConnectDeadline)xrpcTlsPeerFail(P,XRPC_TLS_TIMEOUT);
    if(Stream){
        P->Raw=Stream;P->NativeClosed=xrtNetStreamState(Stream)==XNET_STREAM_CLOSED;
        if(!xrpcTlsPeerClosing(P) && xrtNetStreamState(Stream)==XNET_STREAM_OPEN){
            P->RawEventsGone=false;xrpcTlsPeerRef(P);
            if(xrtNetStreamSetEventsOwnedV1(Stream,P,&xrpcTlsPeerRawEvents)){
                P->RawOpen=true;
                /* Close may win during binding; never reopen or begin TLS. */
                xrtMutexLock(&P->Gate);if(P->Info.State<XRPC_TLS_CLOSING)P->Info.State=XRPC_TLS_HANDSHAKING;xrtMutexUnlock(&P->Gate);
            }else {P->RawEventsGone=true;xrpcTlsPeerDrop(P);xrpcTlsPeerNativeError(P,xrtGetError());xrpcTlsPeerFail(P,xrpcTlsPeerConnectFailure(XNET_RESULT_ERROR,xrtGetError()));}
        }else if(!xrpcTlsPeerClosing(P))xrpcTlsPeerFail(P,XRPC_TLS_TRANSPORT);
        if(xrpcTlsPeerClosing(P))(void)xrtNetStreamAbort(Stream);
    }else {P->NativeClosed=true;xrpcTlsPeerFail(P,xrpcTlsPeerConnectFailure(Result,Cause));}
    xrpcTlsPeerWake(P);
}
#endif
static bool xrpcTlsPeerInstall(xrpctls* P,xnetstream* Raw)
{
    /* A buffered ClientHello may have paused the temporary raw binding.
     * Resume while we still own TCP; no raw IO state changes after transfer. */
    P->Handoff=true;bool Ready=xrtNetStreamResume(Raw);P->Handoff=false;
    if(!Ready || xrpcTlsPeerClosing(P))return false;
    P->EventsGone=false;xrpcTlsPeerRef(P);
    if(!xrtTlsStreamAttachOwnedV1(Raw,P->Session,&P->Config.TLS,P,&xrpcTlsPeerEvents,&P->Stream)){
        P->EventsGone=true;xrpcTlsPeerDrop(P);return false;}
    P->Session=NULL;P->Raw=NULL;return true;
}
static void xrpcTlsPeerRead(xrpctls* P)
{
    size_t Budget=P->Config.ReadBytes,Units=P->Config.WorkFrames;
    while(Units-- && !xrpcTlsPeerClosing(P)){
        if(P->Offset==P->Used){P->Offset=P->Used=0;size_t N=xrtTlsStreamAvailable(P->Stream);if(!N || !Budget)break;
            if(N>P->Config.ReadChunkBytes)N=P->Config.ReadChunkBytes;
            if(N>Budget)N=Budget;
            xtlsresult R=xrtTlsStreamRead(P->Stream,P->Data,N,&P->Used);
            if(R==XTLS_ERROR){xrpcTlsPeerFail(P,xrpcTlsPeerStreamFailure(P));break;}
            if(!P->Used)break;
            Budget-=P->Used;P->ChunkEpoch=P->ReadEpoch?P->ReadEpoch:xrtTimer();xrtMutexLock(&P->Gate);P->Info.ReadBytes+=P->Used;xrtMutexUnlock(&P->Gate);}
        if(!P->FrameEntered)P->FrameEntered=P->ChunkEpoch;
        if(xrtTimer()>=xrpcTlsPeerDeadline(P->FrameEntered,P->Config.FrameTimeout)){xrpcTlsPeerFail(P,XRPC_TLS_TIMEOUT);break;}
        size_t Used=0;xrpcframeview Frame;xrpcframestatus S=xrpcFrameReaderFeed(P->Reader,(xbytesview){P->Data+P->Offset,P->Used-P->Offset},&Used,&Frame);P->Offset+=Used;
        if(S==XRPC_FRAME_READY){
            xrpcduplexadmission A=xrpcDuplexReceiveEntered(P->Duplex,(xstrview){(cstr)Frame.Body.Data,Frame.Body.Size},P->Config.RequestContext,P->FrameEntered);P->FrameEntered=0;
            if(A!=XRPC_DUPLEX_ACCEPTED){if(A==XRPC_DUPLEX_OVERLOADED){xrtMutexLock(&P->Gate);++P->Info.Overloaded;xrtMutexUnlock(&P->Gate);xrpcTlsPeerFail(P,XRPC_TLS_LIMIT);}
                else xrpcTlsPeerFail(P,A==XRPC_DUPLEX_PROTOCOL?XRPC_TLS_PROTOCOL:A==XRPC_DUPLEX_CLOSED?XRPC_TLS_LOCAL:xrpcTlsPeerLocalFailure());
                break;}
            xrtMutexLock(&P->Gate);++P->Info.ReadFrames;xrtMutexUnlock(&P->Gate);
        }else if(S!=XRPC_FRAME_MORE){xrpcTlsPeerFail(P,S==XRPC_FRAME_MEMORY?XRPC_TLS_MEMORY:S==XRPC_FRAME_LIMIT?XRPC_TLS_LIMIT:XRPC_TLS_PROTOCOL);break;}
        else if(!Used)break;
    }
    if(xrpcTlsPeerClosing(P))return;
    if(P->Offset<P->Used || xrtTlsStreamAvailable(P->Stream))xrpcTlsPeerWake(P);
    else if(P->ReadEnded || P->NativeClosed){xrpcframestatus S=xrpcFrameReaderEnd(P->Reader);xrpcTlsPeerFail(P,P->NativeClosed && (P->NativeReason==XRPC_TLS_MEMORY || P->NativeReason==XRPC_TLS_TIMEOUT)?P->NativeReason:S==XRPC_FRAME_END?XRPC_TLS_TRANSPORT:XRPC_TLS_PROTOCOL);}
    else P->ReadEpoch=0;
}

static void xrpcTlsPeerOutputClear(xrpctls* P)
{xrpcDuplexOutputRelease(P->Output);P->Output=NULL;P->Token=0;P->HeaderBytes=P->HeaderOffset=P->BodyOffset=0;P->Offered=false;}
static bool xrpcTlsPeerBound(xrpctls* P,size_t N,size_t* Bound)
{
    size_t Head=P->HeaderBytes-P->HeaderOffset,H=Head<N?Head:N,A=0,B=0;
    if(H && !xrtTlsStreamSendBound(P->Stream,H,&A))return false;
    if(N>H && !xrtTlsStreamSendBound(P->Stream,N-H,&B))return false;
    if(A>SIZE_MAX-B)return false;
    *Bound=A+B;return true;
}
static size_t xrpcTlsPeerWriteRoom(xrpctls* P,size_t N)
{
    size_t Pending=xrtTlsStreamPending(P->Stream);if(Pending>=P->Config.MaxCipherBytes)return 0;
    size_t Room=P->Config.MaxCipherBytes-Pending,Bound=0;if(!xrpcTlsPeerBound(P,N,&Bound)){xrpcTlsPeerFail(P,xrpcTlsPeerLocalFailure());return 0;}
    if(Bound<=Room)return N;
    size_t Low=0,High=N;
    while(Low<High){size_t Mid=Low+(High-Low+1)/2;if(!xrpcTlsPeerBound(P,Mid,&Bound)){xrpcTlsPeerFail(P,xrpcTlsPeerLocalFailure());return 0;}if(Bound<=Room)Low=Mid;else High=Mid-1;}
    return Low;
}
static void xrpcTlsPeerWrite(xrpctls* P)
{
    size_t Budget=P->Config.WriteBytes,Units=P->Config.WorkFrames;
    if(xrtTlsStreamState(P->Stream)!=XTLS_STREAM_OPEN)return;
    while(Budget && Units-- && !xrpcTlsPeerClosing(P)){
        xrpcduplexinfo D={0};D.Size=sizeof(D);if(!xrpcDuplexInfo(P->Duplex,&D)){xrpcTlsPeerFail(P,XRPC_TLS_PROTOCOL);break;}
        if(P->Output && !D.Selected)xrpcTlsPeerOutputClear(P);
        if(!P->Output){int Next=xrpcDuplexNext(P->Duplex,&P->Output,&P->Token);if(Next<0){xrpcTlsPeerFail(P,xrpcTlsPeerLocalFailure());break;}if(!Next)return;
            xstrview Body=xrpcDuplexOutputWire(P->Output);if(!xrpcFrameHeaderWrite(Body.Size,&P->Config.Frame,P->Header,sizeof(P->Header),&P->HeaderBytes)){xrpcTlsPeerFail(P,XRPC_TLS_LIMIT);break;}}
        xstrview Body=xrpcDuplexOutputWire(P->Output);size_t Head=P->HeaderBytes-P->HeaderOffset,Left=Head+Body.Size-P->BodyOffset,N=Left;
        if(N>Budget)N=Budget;
        if(N>P->Config.WriteChunkBytes)N=P->Config.WriteChunkBytes;
        N=xrpcTlsPeerWriteRoom(P,N);if(!N)return;
        size_t H=Head<N?Head:N;xnetspan Spans[2];size_t Count=0;if(H)Spans[Count++]=(xnetspan){P->Header+P->HeaderOffset,H};if(N>H)Spans[Count++]=(xnetspan){(const unsigned char*)Body.Data+P->BodyOffset,N-H};
        if(!P->Offered){if(!xrpcDuplexStarted(P->Duplex,P->Token)){xrpcTlsPeerWake(P);return;}P->Offered=true;}
        size_t Written=0;xtlsresult S=Count==1?xrtTlsStreamSend(P->Stream,Spans[0].Data,Spans[0].Size,&Written):xrtTlsStreamSendVec(P->Stream,Spans,Count,&Written);
        if(Written>N){xrpcTlsPeerFail(P,XRPC_TLS_PROTOCOL);return;}size_t TakenHead=Written<H?Written:H;P->HeaderOffset+=TakenHead;P->BodyOffset+=Written-TakenHead;Budget-=Written;
        xrtMutexLock(&P->Gate);P->Info.WriteBytes+=Written;if(Written && Written<N)++P->Info.ShortWrites;if(S==XTLS_AGAIN)++P->Info.BlockedWrites;xrtMutexUnlock(&P->Gate);
        if(S==XTLS_ERROR || S==XTLS_CLOSED || (S==XTLS_OK && !Written)){xrpcTlsPeerFail(P,S==XTLS_ERROR?xrpcTlsPeerStreamFailure(P):XRPC_TLS_TRANSPORT);return;}
        if(P->HeaderOffset==P->HeaderBytes && P->BodyOffset==Body.Size){if(!xrpcDuplexSent(P->Duplex,P->Token)){xrpcTlsPeerFail(P,XRPC_TLS_PROTOCOL);return;}
            xrtMutexLock(&P->Gate);++P->Info.WriteFrames;xrtMutexUnlock(&P->Gate);xrpcTlsPeerOutputClear(P);}
        if(S==XTLS_AGAIN)return;
        if(Written<N){xrpcTlsPeerWake(P);return;}
    }
    if(P->Output && !xrpcTlsPeerClosing(P) && xrpcTlsPeerWriteRoom(P,1))xrpcTlsPeerWake(P);
}
static bool xrpcTlsPeerRetireNative(xrpctls* P)
{
    if(P->Stream && xrtTlsStreamPollRetired(P->Stream)==XWAIT_OK){xtlsstream* S=P->Stream;P->Stream=NULL;P->DestroyingNative=true;xrtTlsStreamDestroy(S);P->DestroyingNative=false;}
    if(P->Raw && xrtNetStreamPollRetired(P->Raw)==XWAIT_OK){xnetstream* S=P->Raw;P->Raw=NULL;xrtNetStreamDestroy(S);}
    if(!P->Stream && !P->Raw && P->Session){xrtTlsSessionDestroy(P->Session);P->Session=NULL;}
    return !P->Stream && !P->Raw && P->EventsGone && P->RawEventsGone;
}

static void xrpcTlsPeerWork(xnetworker* Worker,ptr Data)
{
    xerror* Ambient=xrtTakeError(); /* An unrelated Worker task is not this operation's cause. */
    (void)Worker;xrpctls* P=Data;xrtMutexLock(&P->Gate);P->Posted=false;bool Closed=P->Info.State==XRPC_TLS_CLOSED;xrtMutexUnlock(&P->Gate);
    if(Closed){xrtClearError();xrtSetErrorTake(Ambient);xrpcTlsPeerDrop(P);return;}
    if(!P->Started){P->Started=true;
        if(!P->Stream && !P->Raw && !xrpcTlsPeerClosing(P)){
            if(xrtTimer()>=P->ConnectDeadline)xrpcTlsPeerFail(P,XRPC_TLS_TIMEOUT);
            else {
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
                if(P->Connect){
                    P->Connect->Pending=true;bool OK=P->Connect->Start(P->Connect->Data,P,P->Engine,xrtNetWorkerIndex(P->Worker),&P->Config.Stream);
                    if(!OK){P->Connect->Pending=false;xrpcTlsPeerNativeError(P,xrtGetError());xrpcTlsPeerFail(P,xrpcTlsPeerConnectFailure(XNET_RESULT_ERROR,xrtGetError()));}
                }else
#endif
                {xnetstreamconfig C=P->Config.Stream;C.ConnectTimeout=xrpcTlsPeerRemaining(P->ConnectDeadline);P->RawEventsGone=false;xrpcTlsPeerRef(P);
                P->Raw=xrtNetStreamConnectOwnedV1(P->Engine,&P->Remote,xrtNetWorkerIndex(P->Worker),&C,P,&xrpcTlsPeerRawEvents);
                if(!P->Raw){P->RawEventsGone=true;xrpcTlsPeerDrop(P);xrpcTlsPeerFail(P,xrpcTlsPeerLocalFailure());}}}}
    }
    if(P->Raw && P->RawOpen && P->Session && !xrpcTlsPeerClosing(P)){
        if(xrtTimer()>=P->ConnectDeadline)xrpcTlsPeerFail(P,XRPC_TLS_TIMEOUT);
        else if(!xrpcTlsPeerInstall(P,P->Raw))xrpcTlsPeerFail(P,xrpcTlsPeerLocalFailure());
    }
    if(!xrpcDuplexStep(P->Duplex))xrpcTlsPeerFail(P,xrpcTlsPeerLocalFailure());
    xrpcduplexinfo D={0};D.Size=sizeof(D);xrpcDuplexInfo(P->Duplex,&D);
    if(D.Closing && !xrpcTlsPeerClosing(P))xrpcTlsPeerFail(P,D.Reason==XRPC_CALL_PROTOCOL?XRPC_TLS_PROTOCOL:D.Reason==XRPC_CALL_LIMIT?XRPC_TLS_LIMIT:D.Reason==XRPC_CALL_MEMORY?XRPC_TLS_MEMORY:XRPC_TLS_TRANSPORT);
    if(!xrpcTlsPeerClosing(P)){
        if(!P->Open && xrtTimer()>=P->ConnectDeadline)xrpcTlsPeerFail(P,XRPC_TLS_TIMEOUT);
        if(P->FrameEntered && xrtTimer()>=xrpcTlsPeerDeadline(P->FrameEntered,P->Config.FrameTimeout))xrpcTlsPeerFail(P,XRPC_TLS_TIMEOUT);
        if(P->Open && !xrpcTlsPeerClosing(P)){xrpcTlsPeerRead(P);if(!xrpcTlsPeerClosing(P))xrpcTlsPeerWrite(P);}
    }
    xrpcDuplexInfo(P->Duplex,&D);
    if(D.Closing && !xrpcTlsPeerClosing(P))xrpcTlsPeerFail(P,D.Reason==XRPC_CALL_PROTOCOL?XRPC_TLS_PROTOCOL:D.Reason==XRPC_CALL_LIMIT?XRPC_TLS_LIMIT:D.Reason==XRPC_CALL_MEMORY?XRPC_TLS_MEMORY:XRPC_TLS_TRANSPORT);
    bool Done=false;
    if(xrpcTlsPeerClosing(P)){
        xrpcTlsPeerTimerCancel(P);xrpcTlsPeerOutputClear(P);
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
        if(P->Connect){if(P->Connect->Pending)P->Connect->Cancel(P->Connect->Data);else xrpcTlsPeerConnectRelease(P);}
#endif
        if(!P->AbortIssued){P->AbortIssued=true;if(P->Stream)(void)xrtTlsStreamAbort(P->Stream);else if(P->Raw)(void)xrtNetStreamAbort(P->Raw);}
        bool NativeDone=xrpcTlsPeerRetireNative(P);
        xrtMutexLock(&P->Gate);xrpctlsreason Reason=P->Info.Reason;size_t Entries=P->Info.Submitters;xrtMutexUnlock(&P->Gate);
        if(!P->DuplexClosing){P->DuplexClosing=true;(void)xrpcDuplexClose(P->Duplex,xrpcTlsPeerCallFailure(Reason));}
        (void)xrpcDuplexStep(P->Duplex);xrpcDuplexInfo(P->Duplex,&D);
        if(!Entries && D.Drained && NativeDone
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
            && !P->Connect
#endif
        ){
            /* Recheck admission under Gate before consuming the endpoint. */
            xrtMutexLock(&P->Gate);Done=P->Info.Submitters==0;xrtMutexUnlock(&P->Gate);
            if(Done && xrpcDuplexDestroy(P->Duplex))P->Duplex=NULL;else Done=false;
        }
        if(!Done && D.More)xrpcTlsPeerWake(P);
        else if(!Done){double At=D.NextDeadline;if(!NativeDone){if(!P->CleanupDelay)P->CleanupDelay=1;else if(P->CleanupDelay<32)P->CleanupDelay*=2;double Retry=xrpcTlsPeerDeadline(xrtTimer(),P->CleanupDelay);if(Retry<At)At=Retry;}xrpcTlsPeerArm(P,At);}
    }else {
        double At=D.NextDeadline;if(!P->Open && P->ConnectDeadline<At)At=P->ConnectDeadline;
        double Frame=P->FrameEntered?xrpcTlsPeerDeadline(P->FrameEntered,P->Config.FrameTimeout):INFINITY;if(Frame<At)At=Frame;
        if(D.More)xrpcTlsPeerWake(P);else xrpcTlsPeerArm(P,At);
    }
    xrpcframeinfo F={0};F.Size=sizeof(F);xrpcFrameReaderInfo(P->Reader,&F);
    size_t Cipher=P->Stream?xrtTlsStreamPending(P->Stream):0;
    xrtMutexLock(&P->Gate);P->Info.CipherBytes=Cipher;if(Cipher>P->Info.MaxObservedCipherBytes)P->Info.MaxObservedCipherBytes=Cipher;P->Info.Duplex=D;P->Info.FrameStorageBytes=F.StorageBytes;
    if(Done)P->Info.State=XRPC_TLS_CLOSED;
    xrtMutexUnlock(&P->Gate);
    if(Done){xrpcTlsPeerTimerCancel(P);xrpcTlsPeerDrop(P);} /* Runtime reference. */
    xrtClearError();xrtSetErrorTake(Ambient);
    xrpcTlsPeerDrop(P); /* Current post. */
}
XRPC_API bool xrpcTlsConfigInit(xrpctlsconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS config output");
    *C=(xrpctlsconfig){0};C->Size=sizeof(*C);xrpcDuplexConfigInit(&C->Duplex);xrpcFrameConfigInit(&C->Frame);xrtNetStreamConfigInit(&C->Stream);xrtTlsStreamConfigInit(&C->TLS);
    C->ReadChunkBytes=4096;C->ReadBytes=C->WriteBytes=65536;C->WriteChunkBytes=16384;C->WorkFrames=32;C->MaxSubmitters=128;C->FrameTimeout=30000;C->MaxCipherBytes=1048576;C->MaxMetadataBytes=65536;return true;
}
static void xrpcTlsPeerFree(xrpctls* P)
{
    if(P->Duplex){(void)xrpcDuplexClose(P->Duplex,XRPC_CALL_CLOSED);(void)xrpcDuplexStep(P->Duplex);(void)xrpcDuplexDestroy(P->Duplex);}
    xrpcFrameReaderDestroy(P->Reader);xrtTlsStreamDestroy(P->Stream);xrtNetStreamDestroy(P->Raw);xrtTlsSessionDestroy(P->Session);
    xrtSecureZero((ptr)P->Config.RequestContext.Data,P->Config.RequestContext.Size);xrtCondUnit(&P->Retired);xrtMutexUnit(&P->Retirement);xrtMutexUnit(&P->Gate);
    (void)xrtNetEngineUnpin(P->Engine);xrtFree(P);
}
static xrpctls* xrpcTlsPeerCreate(xnetengine* E,xnetworker* W,const xrpctlsconfig* Config,xrpctls** Out,double Entered)
{
    if(!E || !W || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)))))goto Argument;
    xrpctlsconfig C={0};if(Config)C=*Config;else if(!xrpcTlsConfigInit(&C))return NULL;
    if(C.Size!=sizeof(C) || C.Duplex.Wake || C.Duplex.User || !C.ReadChunkBytes || C.ReadChunkBytes>1048576u ||
       !C.ReadBytes || !C.WriteBytes || !C.WriteChunkBytes || !C.WorkFrames || C.WorkFrames>65536 || !C.MaxSubmitters || C.MaxSubmitters>65536 || C.FrameTimeout<XRT_WAIT_FOREVER || C.ReadBytes>1048576 || C.WriteBytes>1048576 || C.WriteChunkBytes>1048576 ||
       C.MaxCipherBytes<64 || C.MaxCipherBytes>1073741824u || !C.MaxMetadataBytes || C.MaxMetadataBytes>1048576 || C.TLS.HandshakeTimeout<0 || C.TLS.CloseTimeout<0 || !C.TLS.AsyncBytesLimit || !C.TLS.AsyncCountLimit || !C.TLS.AsyncBatch ||
       C.RequestContext.Size>16384 || !xrtMemRangeValid(C.RequestContext.Data,C.RequestContext.Size) || xrtMemRangesOverlap(C.RequestContext.Data,C.RequestContext.Size,Out,sizeof(*Out)) ||
       !xrtNetStreamConfigValid(&C.Stream) || C.ReadChunkBytes>SIZE_MAX-sizeof(xrpctls)-C.RequestContext.Size)goto Argument;
    if(!xrtNetEnginePin(E))return NULL;
    size_t Bytes=sizeof(xrpctls)+C.ReadChunkBytes+C.RequestContext.Size;xrpctls* P=xrtCalloc(1,Bytes);if(!P){xrtNetEngineUnpin(E);goto Memory;}
    P->Engine=E;P->Worker=W;P->Config=C;P->ConnectDeadline=xrpcTlsPeerDeadline(Entered,C.Stream.ConnectTimeout);P->TimerAt=INFINITY;P->EventsGone=P->RawEventsGone=true;
    P->Info.Size=sizeof(P->Info);P->Info.State=XRPC_TLS_CONNECTING;P->Info.FixedBytes=Bytes;xrtAtomic32Init(&P->References,1);
    if(!xrtMutexInit(&P->Gate)){xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    if(!xrtMutexInit(&P->Retirement)){xrtMutexUnit(&P->Gate);xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    if(!xrtCondInit(&P->Retired)){xrtMutexUnit(&P->Retirement);xrtMutexUnit(&P->Gate);xrtFree(P);xrtNetEngineUnpin(E);return NULL;}
    P->Config.RequestContext=(xbytesview){P->Data+C.ReadChunkBytes,C.RequestContext.Size};if(C.RequestContext.Size)memcpy((ptr)P->Config.RequestContext.Data,C.RequestContext.Data,C.RequestContext.Size);
    C.Duplex.Wake=xrpcTlsPeerWake;C.Duplex.User=P;
    if(!xrtNetPostInit(&P->Post) || !xrpcFrameReaderCreate(&C.Frame,&P->Reader) || !xrpcDuplexCreate(&C.Duplex,&P->Duplex)){xerror* Error=xrtTakeError();xrpcTlsPeerFree(P);xrtClearError();xrtSetErrorTake(Error);return NULL;}
    P->Info.Duplex.Size=sizeof(P->Info.Duplex);xrpcDuplexInfo(P->Duplex,&P->Info.Duplex);
    return P;
Argument:
    xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS factory inputs/configuration");return NULL;
Memory:
    xrpcTlsPeerReport(XERR_MEMORY,"RPC TLS allocation failed");return NULL;
}
static bool xrpcTlsPeerMetadata(xrpctls* P,const xstrview* Protocols,size_t Count,xstrview Name,xstrview Verify,const void* Out)
{
    if(Count>P->Config.MaxMetadataBytes/sizeof(xstrview))return xrpcTlsPeerReport(XERR_RANGE,"RPC TLS metadata descriptors exceed limit");
    size_t Bytes=Count*sizeof(xstrview);if(!xrtMemRangeValid(Protocols,Bytes) || xrtMemRangesOverlap(Protocols,Bytes,Out,sizeof(xrpctls*)))return xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS ALPN descriptors");
    for(size_t I=0;I<Count+2;++I){xstrview V=I==0?Name:I==1?Verify:Protocols[I-2];
        if(V.Size>P->Config.MaxMetadataBytes-Bytes || !xrtMemRangeValid(V.Data,V.Size) || xrtMemRangesOverlap(V.Data,V.Size,Out,sizeof(xrpctls*)) || (I<2 && V.Size && memchr(V.Data,0,V.Size)))return xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS bounded metadata");
        Bytes+=V.Size;}
    return true;
}
XRPC_API bool xrpcTlsConnect(xnetengine* E,const xnetaddr* Remote,uint64 Affinity,const xtlsclientconfig* Client,const xrpctlsconfig* Config,xrpctls** Out)
{
    double Entered=xrtTimer();if(!E || !Remote || !xrtMemRangeValid(Remote,sizeof(*Remote)) || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || xrtMemRangesOverlap(Remote,sizeof(*Remote),Out,sizeof(*Out)) ||
       !Client || !xrtMemRangeValid(Client,sizeof(*Client)) || xrtMemRangesOverlap(Client,sizeof(*Client),Out,sizeof(*Out)) || (!Client->ServerName.Size && !Client->VerifyName.Size) ||
       (Remote->Family!=XNET_FAMILY_IPV4 && Remote->Family!=XNET_FAMILY_IPV6) || !Remote->Port)return xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS numeric address/identity/output");
    uint32 Count=xrtNetEngineWorkerCount(E);if(!Count)return xrpcTlsPeerReport(XERR_STATE,"RPC TLS engine has no worker");
    xrpctls* P=xrpcTlsPeerCreate(E,xrtNetEngineWorker(E,(uint32)(Affinity%Count)),Config,Out,Entered);if(!P)return false;P->Remote=*Remote;
    if(!xrpcTlsPeerMetadata(P,Client->Protocols,Client->ProtocolCount,Client->ServerName,Client->VerifyName,Out) || !(P->Session=xrtTlsClientCreate(Client,NULL))){xerror* Error=xrtTakeError();xrpcTlsPeerFree(P);xrtClearError();xrtSetErrorTake(Error);return false;}
    xrtMutexLock(&P->Gate);P->Active=true;xrpcTlsPeerRef(P);bool Posted=xrpcTlsPeerWakeLocked(P);if(Posted)*Out=P;else P->Active=false;xrtMutexUnlock(&P->Gate);
    if(!Posted){xerror* Error=xrtTakeError();xrpcTlsPeerDrop(P);xrpcTlsPeerFree(P);xrtClearError();xrtSetErrorTake(Error);}return Posted;
}
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
bool xrpcTlsCreatePlanned(xnetengine* E,uint64 Affinity,const xtlsclientconfig* Client,const xrpctlsconfig* Config,xrpctlsconnect* Plan,xrpctls** Out,double Entered)
{
    if(!E || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || !Client || !xrtMemRangeValid(Client,sizeof(*Client)) ||
        xrtMemRangesOverlap(Client,sizeof(*Client),Out,sizeof(*Out)) || (!Client->ServerName.Size && !Client->VerifyName.Size) ||
        !Plan || !Plan->Start || !Plan->Cancel || !Plan->Drop)return xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS source/identity/output");
    uint32 Count=xrtNetEngineWorkerCount(E);if(!Count)return xrpcTlsPeerReport(XERR_STATE,"RPC TLS engine has no worker");
    xrpctls* P=xrpcTlsPeerCreate(E,xrtNetEngineWorker(E,(uint32)(Affinity%Count)),Config,Out,Entered);if(!P)return false;
    if(!xrpcTlsPeerMetadata(P,Client->Protocols,Client->ProtocolCount,Client->ServerName,Client->VerifyName,Out) || !(P->Session=xrtTlsClientCreate(Client,NULL))){xerror* Error=xrtTakeError();xrpcTlsPeerFree(P);xrtClearError();xrtSetErrorTake(Error);return false;}
    P->Connect=Plan;xrtMutexLock(&P->Gate);P->Active=true;xrpcTlsPeerRef(P);bool Posted=xrpcTlsPeerWakeLocked(P);if(Posted)*Out=P;else P->Active=false;xrtMutexUnlock(&P->Gate);
    if(!Posted){xerror* Error=xrtTakeError();P->Connect=NULL;xrpcTlsPeerDrop(P);xrpcTlsPeerFree(P);xrtClearError();xrtSetErrorTake(Error);}return Posted;
}
#endif
XRPC_API bool xrpcTlsAccept(xnetstream* Stream,const xtlsserverconfig* Server,const xrpctlsconfig* Config,xrpctls** Out)
{
    double Entered=xrtTimer();if(!Stream || !Server || !xrtMemRangeValid(Server,sizeof(*Server)) || !Out || xrtMemRangesOverlap(Server,sizeof(*Server),Out,sizeof(*Out)))return xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS server inputs");
    xnetworker* W=xrtNetStreamWorker(Stream);if(!xrtNetWorkerIsCurrent(W) || xrtNetStreamState(Stream)!=XNET_STREAM_OPEN)return xrpcTlsPeerReport(XERR_STATE,"RPC TLS accept requires open TCP Worker");
    xrpctls* P=xrpcTlsPeerCreate(xrtNetWorkerEngine(W),W,Config,Out,Entered);if(!P)return false;
    if(!xrpcTlsPeerMetadata(P,Server->Protocols,Server->ProtocolCount,(xstrview){0},(xstrview){0},Out) || !(P->Session=xrtTlsServerCreate(Server,NULL))){
        xerror* Error=xrtTakeError();xrpcTlsPeerFree(P);xrtClearError();xrtSetErrorTake(Error);return false;}
    P->RawEventsGone=false;xrpcTlsPeerRef(P);
    if(!xrtNetStreamSetEventsOwnedV1(Stream,P,&xrpcTlsPeerRawEvents)){xerror* Error=xrtTakeError();P->RawEventsGone=true;xrpcTlsPeerDrop(P);xrpcTlsPeerFree(P);xrtClearError();xrtSetErrorTake(Error);return false;}
    /* Listener publishes Open after its Accept callback returns. Post the
     * snapshotted Session handoff; also drive an already-published OPEN stream. */
    P->Raw=Stream;P->RawOpen=true;P->Started=true;xrtMutexLock(&P->Gate);P->Active=true;if(P->Info.State<XRPC_TLS_CLOSING)P->Info.State=XRPC_TLS_HANDSHAKING;xrpcTlsPeerRef(P);*Out=P;
    if(!xrpcTlsPeerWakeLocked(P)){P->Info.State=XRPC_TLS_CLOSING;P->Info.Reason=XRPC_TLS_LIMIT;}xrtMutexUnlock(&P->Gate);return true;
}
XRPC_API bool xrpcTlsAttach(xtlsstream* Stream,const xrpctlsconfig* Config,xrpctls** Out)
{
    double Entered=xrtTimer();if(!Stream)return xrpcTlsPeerReport(XERR_ARGUMENT,"RPC TLS stream is NULL");xnetstream* Raw=xrtTlsStreamTransport(Stream);xnetworker* W=Raw?xrtNetStreamWorker(Raw):NULL;
    if(!xrtNetWorkerIsCurrent(W) || xrtTlsStreamState(Stream)!=XTLS_STREAM_OPEN)return xrpcTlsPeerReport(XERR_STATE,"RPC TLS attach requires exclusive open owned TLS Worker");
    xrpctls* P=xrpcTlsPeerCreate(xrtNetWorkerEngine(W),W,Config,Out,Entered);if(!P)return false;
    P->EventsGone=false;xrpcTlsPeerRef(P);if(!xrtTlsStreamBindEventsOwnedV1(Stream,P,&xrpcTlsPeerEvents)){xerror* Error=xrtTakeError();P->EventsGone=true;xrpcTlsPeerDrop(P);xrpcTlsPeerFree(P);xrtClearError();xrtSetErrorTake(Error);return false;}
    P->Stream=Stream;P->Started=true;P->ConnectDeadline=INFINITY;xrpcTlsPeerOnOpen(Stream,P);if(xrtTlsStreamAvailable(Stream))P->ReadEpoch=Entered;
    xrtMutexLock(&P->Gate);P->Active=true;xrpcTlsPeerRef(P);*Out=P;if(!xrpcTlsPeerWakeLocked(P)){P->Info.State=XRPC_TLS_CLOSING;P->Info.Reason=XRPC_TLS_LIMIT;}xrtMutexUnlock(&P->Gate);return true;
}

static xrpcduplexadmission xrpcTlsPeerSubmit(xrpctls* P,const xrpcduplexitem* Items,size_t Count,bool Batch,int64 Timeout,xfuture** Future,double Entered)
{
    if(!P || !Future || !xrtMemRangeValid(Future,sizeof(*Future)) || xrtMemRangesOverlap(P,P->Info.FixedBytes,Future,sizeof(*Future)))return xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS submission output"),XRPC_DUPLEX_ERROR;
    xrtMutexLock(&P->Gate);if(P->Info.State>=XRPC_TLS_CLOSING){xrtMutexUnlock(&P->Gate);return XRPC_DUPLEX_CLOSED;}
    if(P->Info.Submitters==P->Config.MaxSubmitters){++P->Info.Overloaded;xrtMutexUnlock(&P->Gate);return XRPC_DUPLEX_OVERLOADED;}
    ++P->Info.Submitters;xrpcTlsPeerRef(P);xrtMutexUnlock(&P->Gate);
    xrpcduplexadmission A=xrpcDuplexSubmitEntered(P->Duplex,Items,Count,Batch,Timeout,Future,Entered);
    xerror* Error=xrtTakeError();xrtMutexLock(&P->Gate);--P->Info.Submitters;if(P->Info.State>=XRPC_TLS_CLOSING)(void)xrpcTlsPeerWakeLocked(P);xrtMutexUnlock(&P->Gate);xrpcTlsPeerDrop(P);xrtClearError();xrtSetErrorTake(Error);return A;
}
XRPC_API xrpcduplexadmission xrpcTlsCall(xrpctls* P,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double E=xrtTimer();xrpcduplexitem I={false,Method,Params};return xrpcTlsPeerSubmit(P,&I,1,false,Timeout,Future,E);}
XRPC_API xrpcduplexadmission xrpcTlsNotify(xrpctls* P,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double E=xrtTimer();xrpcduplexitem I={true,Method,Params};return xrpcTlsPeerSubmit(P,&I,1,false,Timeout,Future,E);}
XRPC_API xrpcduplexadmission xrpcTlsBatch(xrpctls* P,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** Future)
{return xrpcTlsPeerSubmit(P,Items,Count,true,Timeout,Future,xrtTimer());}
XRPC_API bool xrpcTlsClose(xrpctls* P)
{if(!P)return xrpcTlsPeerReport(XERR_ARGUMENT,"RPC TLS peer is NULL");xrpcTlsPeerFail(P,XRPC_TLS_LOCAL);xrtMutexLock(&P->Gate);bool OK=xrpcTlsPeerWakeLocked(P);xrtMutexUnlock(&P->Gate);return OK;}
XRPC_API bool xrpcTlsInfo(xrpctls* P,xrpctlsinfo* I)
{
    if(!P || !I || !xrtMemRangeValid(I,sizeof(*I)) || I->Size!=sizeof(*I) || xrtMemRangesOverlap(P,P->Info.FixedBytes,I,sizeof(*I)))return xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS info output");
    xrtMutexLock(&P->Gate);*I=P->Info;xrtMutexUnlock(&P->Gate);return true;
}
XRPC_API xwaitresult xrpcTlsPollRetired(xrpctls* P)
{
    if(!P){xrpcTlsPeerReport(XERR_ARGUMENT,"RPC TLS peer is NULL");return XWAIT_ERROR;}
    xmutextryresult L=xrtMutexTryLockResult(&P->Retirement);if(L!=XMUTEX_TRY_OK)return L==XMUTEX_TRY_BUSY?XWAIT_TIMEOUT:XWAIT_ERROR;
    bool Alone=xrtAtomic32Load(&P->References,XMEMORY_ACQUIRE)==1;xrtMutexUnlock(&P->Retirement);
    L=xrtMutexTryLockResult(&P->Gate);if(L!=XMUTEX_TRY_OK)return L==XMUTEX_TRY_BUSY?XWAIT_TIMEOUT:XWAIT_ERROR;
    bool Closed=P->Info.State==XRPC_TLS_CLOSED;if(!Closed && P->Info.State==XRPC_TLS_CLOSING)(void)xrpcTlsPeerWakeLocked(P);xrtMutexUnlock(&P->Gate);
    if(!Alone || !Closed)return XWAIT_TIMEOUT;
    return XWAIT_OK;
}
XRPC_API xwaitresult xrpcTlsWaitFor(xrpctls* P,int64 Timeout)
{
    if(!P || Timeout<XRT_WAIT_FOREVER){xrpcTlsPeerReport(XERR_ARGUMENT,"invalid RPC TLS wait");return XWAIT_ERROR;}
    if(xrtNetEngineCurrent(P->Engine)){xrpcTlsPeerReport(XERR_STATE,"RPC TLS wait on own engine worker");return XWAIT_ERROR;}
    double At=xrpcTlsPeerDeadline(xrtTimer(),Timeout);
    for(;;){xwaitresult R=xrpcTlsPollRetired(P);if(R!=XWAIT_TIMEOUT)return R;int64 Left=xrpcTlsPeerRemaining(At);if(!Left)return XWAIT_TIMEOUT;
        if(Left==XRT_WAIT_FOREVER || Left>1)Left=1;
        xrtMutexLock(&P->Retirement);(void)xrtCondWaitFor(&P->Retired,&P->Retirement,Left);xrtMutexUnlock(&P->Retirement);}
}
XRPC_API bool xrpcTlsDestroy(xrpctls* P)
{if(!P)return true;if(xrpcTlsPollRetired(P)!=XWAIT_OK)return xrpcTlsPeerReport(XERR_STATE,"RPC TLS peer still has physical duties");xrpcTlsPeerFree(P);return true;}
#endif
