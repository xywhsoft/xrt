#include <xrpc/http_client.h>
#include <xrpc/id.h>
#include "../internal/xrpc_call_internal.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_CLIENT)
typedef struct xrpchttprequest xrpchttprequest;
struct xrpchttprequest {
    xatomic32 References;
    xrpchttpclient* Client;
    xrpccall* Call;
    xnetpost CancelPost;
    xrpchttprequest *Next, *Previous;
    unsigned char* Wire;
    size_t WireBytes, IdBytes;
    char Id[24];
    uint64 Timer;
    bool Linked, Sent, Completing;
    xrpccallstatus Status;
    xrpcresponses* Response;
};
struct xrpchttpclient {
    xatomic32 References;
    xmutex Gate;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetstream* Stream;
    xnetaddr Remote;
    xrpchttpconfig Config;
    xrpchttpinfo Info;
    xnetpost Wake;
    xpromise* ClosePromise;
    xfuture* CloseFuture;
    xrpchttprequest *SubmitHead, *SubmitTail, *Head, *Tail, *Active;
    xerror* Cause;
    uint64 Serial;
    bool WakeQueued, Started, TransportClosed, EnginePinned, CloseDone;
    bool ReadEnded, BodyStarted, ConnectionClose;
    size_t InputUsed, InputScan, InputCapacity, BodyUsed, BodyCapacity, ResponseWireBytes;
    uint32 Informational;
    unsigned char *Input, *Payload;
    xhttpfield *Fields, *Trailers;
    xhttp1body Body;
    xstrview Host, Target;
    unsigned char Text[];
};
static void xrpcHttpWork(xnetworker* Worker,ptr Data);
static void xrpcHttpTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data);
static bool xrpcHttpWakeLocked(xrpchttpclient* C);
static bool xrpcHttpReport(xerrkind Kind,xrpchttperror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false;}
static void xrpcHttpRef(xrpchttpclient* C)
{(void)xrtAtomic32FetchAdd(&C->References,1,XMEMORY_RELAXED);}
XRPC_API bool xrpcHttpRetain(xrpchttpclient* C)
{
    if(!C)return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"HTTP client is NULL");
    uint32 N=xrtAtomic32Load(&C->References,XMEMORY_RELAXED);
    while(N<UINT32_MAX/2u){if(xrtAtomic32CompareExchange(&C->References,&N,N+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
    return xrpcHttpReport(XERR_RANGE,XRPC_HTTP_STATE,"HTTP client reference limit");
}
XRPC_API void xrpcHttpRelease(xrpchttpclient* C)
{
    if(!C || xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtNetStreamDestroy(C->Stream);xrtPromiseDestroy(C->ClosePromise);xrtFutureDestroy(C->CloseFuture);
    xrtErrorFree(C->Cause);xrtFree(C->Input);xrtFree(C->Payload);xrtFree(C->Fields);xrtFree(C->Trailers);
    if(C->EnginePinned)xrtNetEngineUnpin(C->Engine);
    xrtMutexUnit(&C->Gate);xrtFree(C);
}
static bool xrpcHttpWakeLocked(xrpchttpclient* C)
{
    if(C->WakeQueued || C->CloseDone)return true;
    C->WakeQueued=true;xrpcHttpRef(C);
    if(xrtNetPost(C->Worker,&C->Wake,xrpcHttpWork,C))return true;
    C->WakeQueued=false;xrpcHttpRelease(C);return false;
}
static void xrpcHttpWake(xrpchttpclient* C)
{xrtMutexLock(&C->Gate);(void)xrpcHttpWakeLocked(C);xrtMutexUnlock(&C->Gate);}
static bool xrpcHttpClosing(xrpchttpclient* C)
{xrtMutexLock(&C->Gate);bool R=C->Info.State>=XRPC_HTTP_CLOSING;xrtMutexUnlock(&C->Gate);return R;}
static void xrpcHttpFail(xrpchttpclient* C,xrpccallstatus Status,const xerror* Cause)
{
    xrtMutexLock(&C->Gate);
    if(C->Info.State<XRPC_HTTP_CLOSING){C->Info.State=XRPC_HTTP_CLOSING;C->Info.CloseStatus=Status;C->Cause=Cause?xrtErrorRef(Cause):NULL;}
    (void)xrpcHttpWakeLocked(C);xrtMutexUnlock(&C->Gate);
}
static xrpccallstatus xrpcHttpParseFailure(const xhttp1errorinfo* E)
{
    if(xrtErrorKind(xrtGetError())==XERR_MEMORY)return XRPC_CALL_MEMORY;
    switch(E->Code){
        case XHTTP1_ERROR_HEAD_TOO_LARGE:case XHTTP1_ERROR_START_LINE_TOO_LARGE:
        case XHTTP1_ERROR_FIELD_LINE_TOO_LARGE:case XHTTP1_ERROR_TOO_MANY_FIELDS:
        case XHTTP1_ERROR_BODY_TOO_LARGE:case XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE:
        case XHTTP1_ERROR_TRAILER_TOO_LARGE:case XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE:
        case XHTTP1_ERROR_TOO_MANY_TRAILERS:return XRPC_CALL_LIMIT;
        default:return XRPC_CALL_PROTOCOL;
    }
}
static void xrpcHttpReservationDrop(xrpchttpclient* C,size_t Bytes)
{
    xrtMutexLock(&C->Gate);--C->Info.Requests;C->Info.RequestBytes-=Bytes;
    if(C->Info.State>=XRPC_HTTP_CLOSING)(void)xrpcHttpWakeLocked(C);
    xrtMutexUnlock(&C->Gate);xrpcHttpRelease(C);
}
static void xrpcHttpRequestRef(xrpchttprequest* R)
{(void)xrtAtomic32FetchAdd(&R->References,1,XMEMORY_RELAXED);}
static void xrpcHttpRequestDrop(ptr Value)
{
    xrpchttprequest* R=Value;if(xrtAtomic32FetchSub(&R->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrpchttpclient* C=R->Client;size_t Bytes=R->WireBytes;
    xrpcResponsesRelease(R->Response);xrtFree(R->Wire);xrtFree(R);xrpcHttpReservationDrop(C,Bytes);
}
static void xrpcHttpReset(xrpchttpclient* C)
{C->Active=NULL;C->InputUsed=0;C->InputScan=0;C->BodyUsed=0;C->ResponseWireBytes=0;C->Informational=0;C->BodyStarted=false;C->ConnectionClose=false;}
static bool xrpcHttpFinish(xrpchttprequest* R,xrpccallstatus Status,const xerror* Cause)
{
    xrpchttpclient* C=R->Client;
    if(R->Completing){Status=R->Status;Cause=NULL;}
    xrpccallfinish F=xrpcCallFinish(R->Call,Status,R->Completing?R->Response:NULL,0,Cause);
    if(F==XRPC_CALL_FINISH_ERROR)return false;
    if(R->Previous)R->Previous->Next=R->Next;else C->Head=R->Next;
    if(R->Next)R->Next->Previous=R->Previous;else C->Tail=R->Previous;
    R->Linked=false;R->Next=R->Previous=NULL;
    if(C->Active==R)xrpcHttpReset(C);
    if(R->Timer)(void)xrtNetEngineTimerCancelCurrent(C->Engine,R->Timer);
    xrtMutexLock(&C->Gate);++C->Info.Completed;xrtMutexUnlock(&C->Gate);
    (void)xrpcCallDestroy(R->Call);R->Call=NULL;xrpcHttpRequestDrop(R);return true;
}
static void xrpcHttpStop(xrpchttprequest* R)
{
    if(!R->Call || !R->Linked)return;
    if(xrpcCallPoll(R->Call)==XRPC_CALL_FINISHED){
        xrpchttpclient* C=R->Client;bool Sent=R->Sent;
        if(Sent)xrpcHttpFail(C,XRPC_CALL_CLOSED,NULL);
        (void)xrpcHttpFinish(R,XRPC_CALL_CLOSED,NULL);
    }
}
static void xrpcHttpCancelWork(xnetworker* Worker,ptr Data)
{(void)Worker;xrpchttprequest* R=Data;xrpcHttpStop(R);xrpcHttpWake(R->Client);xrpcHttpRequestDrop(R);}
static void xrpcHttpCancelWake(ptr Data)
{
    xrpchttprequest* R=Data;xrpcHttpRequestRef(R);
    if(!xrtNetPost(R->Client->Worker,&R->CancelPost,xrpcHttpCancelWork,R)){
        xrpcHttpWake(R->Client);xrpcHttpRequestDrop(R);
    }
}
static void xrpcHttpTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xrpchttprequest* R=Data;
    if(R->Timer==Id)R->Timer=0;
    if(Result==XNET_RESULT_OK)xrpcHttpStop(R);
    else if(Result!=XNET_RESULT_CANCELLED && R->Call)xrpcHttpFail(R->Client,XRPC_CALL_CLOSED,NULL);
    xrpcHttpWake(R->Client);xrpcHttpRequestDrop(R);
}
static bool xrpcHttpArm(xrpchttprequest* R)
{
    double Deadline=xrpcCallDeadline(R->Call);if(Deadline==INFINITY)return true;
    xrpcHttpRequestRef(R);
    R->Timer=__xrtNetEngineSchedule(R->Client->Engine,xrtNetWorkerIndex(R->Client->Worker),Deadline,xrpcHttpTimer,R);
    if(R->Timer)return true;
    xrpcHttpRequestDrop(R);return false;
}
static void xrpcHttpIngest(xrpchttpclient* C)
{
    for(size_t N=0;N<C->Config.WorkRequests;++N){
        xrtMutexLock(&C->Gate);xrpchttprequest* R=C->SubmitHead;
        if(R){C->SubmitHead=R->Next;if(!C->SubmitHead)C->SubmitTail=NULL;}
        xrtMutexUnlock(&C->Gate);if(!R)break;
        R->Next=NULL;R->Previous=C->Tail;R->Linked=true;
        if(C->Tail)C->Tail->Next=R;else C->Head=R;C->Tail=R;
        if(!xrpcHttpClosing(C) && !xrpcHttpArm(R))xrpcHttpFail(C,XRPC_CALL_MEMORY,xrtGetError());
        xrpcHttpStop(R);
    }
}
static xrpccallstatus xrpcHttpAppend(xrpchttpclient* C,xbytesview Data)
{
    size_t Limit=(size_t)C->Config.Body.MaxBody;
    if(Data.Size>Limit-C->BodyUsed)return XRPC_CALL_LIMIT;
    size_t Need=C->BodyUsed+Data.Size;
    if(Need>C->BodyCapacity){
        size_t Capacity=C->BodyCapacity?C->BodyCapacity:256u;if(Capacity>Limit)Capacity=Limit;
        while(Capacity<Need){if(Capacity>Limit/2u){Capacity=Limit;break;}Capacity*=2u;}
        ptr P=xrtRealloc(C->Payload,Capacity);if(!P)return XRPC_CALL_MEMORY;C->Payload=P;C->BodyCapacity=Capacity;
    }
    if(Data.Size)memcpy(C->Payload+C->BodyUsed,Data.Data,Data.Size);
    C->BodyUsed=Need;return XRPC_CALL_OK;
}
static bool xrpcHttpReply(xrpchttpclient* C)
{
    xrpchttprequest* R=C->Active;xrpcmessage Scratch;
    xrpcresponsestatus S=xrpcResponsesRead((xstrview){(cstr)C->Payload,C->BodyUsed},&C->Config.Responses,&Scratch,1,&R->Response);
    if(S!=XRPC_RESPONSE_DONE){
        xrpccallstatus Status=XRPC_CALL_PROTOCOL;
        if(xrtErrorKind(xrtGetError())==XERR_MEMORY)Status=XRPC_CALL_MEMORY;
        else if(S==XRPC_RESPONSE_LOCAL_ERROR && (xrtErrorCode(xrtGetError())==XRPC_CODEC_LIMIT || xrtErrorCode(xrtGetError())==XRPC_CODEC_CAPACITY || xrtErrorCode(xrtGetError())==XRPC_RESPONSE_LIMIT))Status=XRPC_CALL_LIMIT;
        xrpcHttpFail(C,Status,xrtGetError());return false;
    }
    xrpcresponseinfo I={0};I.Size=sizeof(I);if(!xrpcResponsesInfo(R->Response,&I))return false;
    unsigned char Expected[64],Actual[64];size_t A=0,B=0;xrpcidconfig K;xrpcIdConfigInit(&K);K.MaxKeyBytes=sizeof(Expected);K.MaxStringBytes=sizeof(Expected);
    K.MaxInputBytes=C->Config.Responses.Decode.MaxIdBytes;
    if(I.Batch || I.Count!=1 || !xrpcIdKeyWrite(Expected,sizeof(Expected),(xstrview){R->Id,R->IdBytes},&K,&A) ||
        !xrpcIdKeyWrite(Actual,sizeof(Actual),I.Ids[0],&K,&B) || A!=B || memcmp(Expected,Actual,A)){
        xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);return false;
    }
    R->Completing=true;R->Status=I.Messages[0].Kind==XRPC_MESSAGE_RESULT?XRPC_CALL_OK:XRPC_CALL_REMOTE_ERROR;
    bool Close=C->ConnectionClose;if(Close)xrpcHttpFail(C,XRPC_CALL_CLOSED,NULL);
    return xrpcHttpFinish(R,R->Status,NULL);
}
/* Incremental head delimiter scan prevents reparsing growing fragmented heads.
 * Body state belongs to the public HTTP/1 reader; only an incomplete trailer
 * prefix is retained/compacted and may be rescanned by that reader. */
static bool xrpcHttpParse(xrpchttpclient* C,bool End)
{
    size_t Offset=0;
    while(C->Active && !C->Active->Completing){
        if(!C->BodyStarted){
            size_t HeadEnd=0;
            while(C->InputScan<C->InputUsed){size_t N=++C->InputScan;
                if(N>=4 && !memcmp(C->Input+N-4,"\r\n\r\n",4)){HeadEnd=N;break;}
                if(N>=C->Config.Head.MaxHead)break;
            }
            if(!HeadEnd){
                if(End || C->InputUsed>=C->Config.Head.MaxHead)xrpcHttpFail(C,End?XRPC_CALL_PROTOCOL:XRPC_CALL_LIMIT,NULL);
                break;
            }
            xhttp1head Head;xrtHttp1HeadInit(&Head,C->Fields,C->Config.Head.MaxFields);
            xhttp1errorinfo Error={0};
            if(xrtHttp1ResponseParse((xbytesview){C->Input,HeadEnd},&Head,&C->Config.Head,&Error)!=XHTTP1_READY){xrpcHttpFail(C,xrpcHttpParseFailure(&Error),xrtGetError());break;}
            if(Head.Status<200){
                if(Head.Status==101 || (Head.Flags&(XHTTP1_CONTENT_LENGTH|XHTTP1_TRANSFER_ENCODING))){xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;}
                if(++C->Informational>C->Config.MaxInformational){xrpcHttpFail(C,XRPC_CALL_LIMIT,NULL);break;}
                memmove(C->Input,C->Input+HeadEnd,C->InputUsed-HeadEnd);C->InputUsed-=HeadEnd;C->InputScan=0;continue;
            }
            xhttp1bodyplan Plan;
            bool EncodingOK=true;
            for(size_t I=0;I<Head.FieldCount;++I)if(xrtHttpFieldNameEqual(Head.Fields[I].Name,XRT_STR_LITERAL("Content-Encoding")) &&
                !xrtHttpTokenEqual(Head.Fields[I].Value,XRT_STR_LITERAL("identity")))EncodingOK=false;
            if(!EncodingOK || (Head.Flags&XHTTP1_TRANSFER_OTHER) || !xrtHttp1ResponseBodyPlan(&Head,XRT_STR_LITERAL("POST"),&Plan) || Plan.Mode==XHTTP1_BODY_TUNNEL){
                xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;
            }
            if(Plan.Mode==XHTTP1_BODY_FIXED && Plan.Length>C->Config.Body.MaxBody){xrpcHttpFail(C,XRPC_CALL_LIMIT,NULL);break;}
            if(!xrtHttp1BodyInit(&C->Body,&Plan,C->Trailers,C->Config.Body.MaxTrailers,&C->Config.Body)){
                xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;
            }
            C->ConnectionClose=(Head.Flags&XHTTP1_CONNECTION_CLOSE)!=0 ||
                (Head.Version==XHTTP_VERSION_1_0 && !(Head.Flags&XHTTP1_KEEP_ALIVE)) || Plan.Mode==XHTTP1_BODY_CLOSE;
            C->BodyStarted=true;Offset=HeadEnd;
        }
        xbytesview Data={0};size_t Used=0;
        xhttp1errorinfo Error={0};
        xhttp1bodystatus S=xrtHttp1BodyRead(&C->Body,(xbytesview){C->Input+Offset,C->InputUsed-Offset},End,&Used,&Data,&Error);
        Offset+=Used;
        if(S==XHTTP1_BODY_ERROR || S==XHTTP1_BODY_FIELDS){xrpcHttpFail(C,S==XHTTP1_BODY_FIELDS?XRPC_CALL_LIMIT:xrpcHttpParseFailure(&Error),xrtGetError());break;}
        if(S==XHTTP1_BODY_DATA){xrpccallstatus Appended=xrpcHttpAppend(C,Data);
            if(Appended!=XRPC_CALL_OK){xrpcHttpFail(C,Appended,Appended==XRPC_CALL_MEMORY?xrtGetError():NULL);break;}}
        if(S==XHTTP1_BODY_DONE){
            if(Offset!=C->InputUsed){xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;}
            C->InputUsed=0;C->InputScan=0;return xrpcHttpReply(C);
        }
        if(!Used || (S==XHTTP1_BODY_MORE && Offset==C->InputUsed))break;
    }
    if(Offset){memmove(C->Input,C->Input+Offset,C->InputUsed-Offset);C->InputUsed-=Offset;}
    return !xrpcHttpClosing(C);
}
static void xrpcHttpRead(xrpchttpclient* C)
{
    size_t Budget=C->Config.ReadBytes;
    while(Budget && C->Stream && !C->TransportClosed && !xrpcHttpClosing(C)){
        size_t Available=xrtNetStreamAvailable(C->Stream);if(!Available)break;
        if(!C->Active || !C->Active->Sent){xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;}
        size_t N=C->InputCapacity-C->InputUsed;if(N>Budget)N=Budget;if(N>Available)N=Available;
        size_t Remaining=C->Config.MaxResponseWireBytes-C->ResponseWireBytes;if(N>Remaining)N=Remaining;
        if(!N){xrpcHttpFail(C,XRPC_CALL_LIMIT,NULL);break;}
        N=xrtNetStreamRead(C->Stream,C->Input+C->InputUsed,N);if(!N)break;
        Budget-=N;C->InputUsed+=N;C->ResponseWireBytes+=N;if(!xrpcHttpParse(C,false))break;
    }
    if(C->ReadEnded && !xrpcHttpClosing(C) && (!C->Stream || xrtNetStreamAvailable(C->Stream)==0)){
        if(C->Active)(void)xrpcHttpParse(C,true);
        if(!xrpcHttpClosing(C))xrpcHttpFail(C,XRPC_CALL_TRANSPORT,NULL);
    }
    if(C->Stream && !C->TransportClosed && !xrpcHttpClosing(C)){
        if(xrtNetStreamAvailable(C->Stream))xrpcHttpWake(C);
        else (void)xrtNetStreamResume(C->Stream);
    }
}
static void xrpcHttpOnOpen(xnetstream* Stream,ptr Data)
{(void)Stream;xrpchttpclient* C=Data;xrtMutexLock(&C->Gate);if(C->Info.State<XRPC_HTTP_CLOSING)C->Info.State=XRPC_HTTP_OPEN;xrtMutexUnlock(&C->Gate);xrpcHttpWake(C);}
static void xrpcHttpOnRead(xnetstream* Stream,xnetbuf* Buffer,ptr Data)
{(void)Buffer;xrtNetStreamPause(Stream);xrpcHttpWake(Data);}
static void xrpcHttpOnEnd(xnetstream* Stream,ptr Data)
{(void)Stream;xrpchttpclient* C=Data;C->ReadEnded=true;xrpcHttpWake(C);}
static void xrpcHttpOnDrain(xnetstream* Stream,ptr Data)
{(void)Stream;xrpcHttpWake(Data);}
static void xrpcHttpOnLow(xnetstream* Stream,size_t Queued,ptr Data)
{(void)Queued;xrpcHttpOnDrain(Stream,Data);}
static void xrpcHttpOnClose(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{
    (void)Stream;(void)Result;xrpchttpclient* C=Data;C->TransportClosed=true;
    xrpcHttpFail(C,xrtErrorKind(Error)==XERR_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT,Error);
}
static const xnetstreamevents xrpcHttpEvents={xrpcHttpOnOpen,xrpcHttpOnRead,xrpcHttpOnEnd,NULL,xrpcHttpOnLow,xrpcHttpOnDrain,xrpcHttpOnClose};
static void xrpcHttpWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xrpchttpclient* C=Data;
    xrtMutexLock(&C->Gate);C->WakeQueued=false;xrtMutexUnlock(&C->Gate);
    if(!C->Started){
        C->Started=true;
        if(!xrpcHttpClosing(C)){
            C->Stream=xrtNetStreamConnect(C->Engine,&C->Remote,xrtNetWorkerIndex(C->Worker),&C->Config.Stream,&xrpcHttpEvents,C);
            if(!C->Stream){C->TransportClosed=true;xrpcHttpFail(C,xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT,xrtGetError());}
        }else C->TransportClosed=true;
    }
    xrpcHttpIngest(C);
    if(!xrpcHttpClosing(C)){
        xrpcHttpRead(C);
        if(C->Active && C->Active->Completing)(void)xrpcHttpFinish(C->Active,C->Active->Status,NULL);
        if(!xrpcHttpClosing(C) && !C->Active && C->Head && C->Stream && xrtNetStreamState(C->Stream)==XNET_STREAM_OPEN){
            xrpchttprequest* R=C->Head;C->Active=R;
        }
        if(!xrpcHttpClosing(C) && C->Active && !C->Active->Sent){
            xrpchttprequest* R=C->Active;xrpcHttpStop(R);
            if(C->Active==R){
                xnetresult S=xrtNetStreamSend(C->Stream,R->Wire,R->WireBytes);
                if(S==XNET_RESULT_OK){R->Sent=true;(void)xrpcCallMarkSent(R->Call);}
                else if(S!=XNET_RESULT_AGAIN)xrpcHttpFail(C,S==XNET_RESULT_ERROR && xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT,S==XNET_RESULT_ERROR?xrtGetError():NULL);
            }
        }
    }
    if(xrpcHttpClosing(C)){
        if(C->Stream && !C->TransportClosed)(void)xrtNetStreamAbort(C->Stream);
        for(size_t N=0;C->Head && N<C->Config.WorkRequests;++N)if(!xrpcHttpFinish(C->Head,C->Info.CloseStatus,C->Cause))break;
    }
    xrtMutexLock(&C->Gate);
    if(C->SubmitHead || (C->Info.State>=XRPC_HTTP_CLOSING && C->Head) || (!C->Active && C->Head && C->Info.State==XRPC_HTTP_OPEN))
        (void)xrpcHttpWakeLocked(C);
    bool Done=C->TransportClosed && C->Info.Requests==0 && !C->WakeQueued && !C->CloseDone;
    if(Done){C->CloseDone=true;C->Info.State=XRPC_HTTP_CLOSED;C->EnginePinned=false;}
    xrtMutexUnlock(&C->Gate);
    if(Done){
        xrtNetStreamDestroy(C->Stream);C->Stream=NULL;
        xpromise* P=C->ClosePromise;C->ClosePromise=NULL;
        (void)xrtNetEngineUnpin(C->Engine);(void)xrtPromiseResolve(P,NULL);xrtPromiseDestroy(P);
        xrpcHttpRelease(C); /* Runtime activation. */
    }
    xrpcHttpRelease(C); /* Current embedded post. */
}
XRPC_API bool xrpcHttpConfigInit(xrpchttpconfig* Config)
{
    if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP config output");
    *Config=(xrpchttpconfig){0};Config->Size=sizeof(*Config);xrtNetStreamConfigInit(&Config->Stream);
    xrtHttp1LimitsInit(&Config->Head);xrtHttp1BodyLimitsInit(&Config->Body);xrpcResponseConfigInit(&Config->Responses);
    Config->Body.MaxBody=1024u*1024u;Config->MaxRequests=1024;Config->MaxRequestBytes=256u*1024u;
    if(Config->MaxRequestBytes>Config->Stream.WriteLimit)Config->MaxRequestBytes=Config->Stream.WriteLimit;
    Config->MaxRequestBytesTotal=8u*1024u*1024u;Config->WorkRequests=64;Config->ReadBytes=64u*1024u;Config->MaxResponseWireBytes=2u*1024u*1024u;Config->MaxInformational=8;return true;
}
XRPC_API bool xrpcHttpConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    xstrview Host,xstrview Target,const xrpchttpconfig* Config,xrpchttpclient** Client)
{
    if(!Engine || !Remote || !Client || !xrtMemRangeValid(Remote,sizeof(*Remote)) || !xrtMemRangeValid(Client,sizeof(*Client)) ||
        xrtMemRangesOverlap(Remote,sizeof(*Remote),Client,sizeof(*Client)) || !xrtMemRangeValid(Host.Data,Host.Size) || !xrtMemRangeValid(Target.Data,Target.Size) ||
        (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Client,sizeof(*Client)))) ||
        xrtMemRangesOverlap(Host.Data,Host.Size,Client,sizeof(*Client)) || xrtMemRangesOverlap(Target.Data,Target.Size,Client,sizeof(*Client)))
        return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP connect inputs");
    xrpchttpconfig K;if(Config)K=*Config;else xrpcHttpConfigInit(&K);
    const xrpcdecodeconfig* D=&K.Responses.Decode;
    if(K.Size!=sizeof(K) || !K.MaxRequests || K.MaxRequests>1000000u || !K.MaxRequestBytes || K.MaxRequestBytes>K.MaxRequestBytesTotal ||
        K.MaxRequestBytes>K.Stream.WriteLimit || !K.WorkRequests || !K.ReadBytes || !K.MaxResponseWireBytes || !K.Head.MaxHead || !K.Head.MaxFields || !K.Head.MaxStartLine || !K.Head.MaxFieldLine ||
        !K.Body.MaxBody || K.Body.MaxBody>SIZE_MAX || !K.Body.MaxTrailer || !K.Body.MaxTrailers || !K.Body.MaxChunkLine || !K.Body.MaxTrailerLine || !K.MaxInformational ||
        K.Responses.Size!=sizeof(K.Responses) || !K.Responses.MaxOwnedBytes || D->Size!=sizeof(*D) || D->Mode!=XRPC_DECODE_RESPONSES ||
        !D->MaxInputBytes || !D->MaxDepth || D->MaxDepth>XJSON_DEPTH_DEFAULT || !D->MaxStringBytes || !D->MaxValues || !D->MaxContainerItems || !D->MaxMessages || D->MaxIdBytes<22u || !D->MaxMethodBytes ||
        K.Head.MaxFields>SIZE_MAX/sizeof(xhttpfield) || K.Body.MaxTrailers>SIZE_MAX/sizeof(xhttpfield) ||
        Host.Size>K.MaxRequestBytes || Target.Size>K.MaxRequestBytes-Host.Size ||
        Host.Size>SIZE_MAX-sizeof(xrpchttpclient) || Target.Size>SIZE_MAX-sizeof(xrpchttpclient)-Host.Size ||
        (Remote->Family!=XNET_FAMILY_IPV4 && Remote->Family!=XNET_FAMILY_IPV6) || !Remote->Port || !Host.Size || !Target.Size || Target.Data[0]!='/' ||
        !xrtHttpHostValid(Host) || !xrtHttp1TargetValid(Target) || !xrtNetStreamConfigValid(&K.Stream))
        return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP endpoint/configuration");
    size_t Capacity=K.Head.MaxHead>K.Body.MaxTrailer?K.Head.MaxHead:K.Body.MaxTrailer;
    if(Capacity>K.Stream.ReadLimit || Capacity>SIZE_MAX-K.ReadBytes)return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"HTTP input bounds exceed stream/size limit");
    if(!xrtNetEnginePin(Engine))return false;
    xrpchttpclient* C=xrtCalloc(1,sizeof(*C)+Host.Size+Target.Size);
    if(!C){xrtNetEngineUnpin(Engine);return false;}
    xrtAtomic32Init(&C->References,1);C->Engine=Engine;C->EnginePinned=true;C->Remote=*Remote;C->Config=K;
    C->Worker=xrtNetEngineWorker(Engine,(uint32)(Affinity%xrtNetEngineWorkerCount(Engine)));
    C->Info.Size=sizeof(C->Info);C->Info.CloseStatus=XRPC_CALL_CLOSED;C->InputCapacity=Capacity+K.ReadBytes;
    C->Host=(xstrview){(cstr)C->Text,Host.Size};C->Target=(xstrview){(cstr)C->Text+Host.Size,Target.Size};
    memcpy((ptr)C->Host.Data,Host.Data,Host.Size);memcpy((ptr)C->Target.Data,Target.Data,Target.Size);
    if(!xrtMutexInit(&C->Gate)){xrtNetEngineUnpin(Engine);xrtFree(C);return false;}
    C->Input=xrtMalloc(C->InputCapacity);C->Fields=xrtMalloc(K.Head.MaxFields*sizeof(xhttpfield));C->Trailers=xrtMalloc(K.Body.MaxTrailers*sizeof(xhttpfield));
    if(!C->Input || !C->Fields || !C->Trailers || !xrtNetPostInit(&C->Wake) || !(C->ClosePromise=xrtPromiseCreate(&C->CloseFuture,NULL))){xrpcHttpRelease(C);return false;}
    xrpcHttpRef(C);xrtMutexLock(&C->Gate);bool Posted=xrpcHttpWakeLocked(C);xrtMutexUnlock(&C->Gate);
    if(!Posted){xrpcHttpRelease(C);xrpcHttpRelease(C);return false;}
    *Client=C;return true;
}
XRPC_API xrpcadmission xrpcHttpCallAsync(xrpchttpclient* C,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C || !Future || !xrtMemRangeValid(Future,sizeof(*Future)) || !xrtMemRangeValid(Method.Data,Method.Size) || !xrtMemRangeValid(Params.Data,Params.Size) ||
        xrtMemRangesOverlap(Future,sizeof(*Future),Method.Data,Method.Size) || xrtMemRangesOverlap(Future,sizeof(*Future),Params.Data,Params.Size) || Timeout<XRT_WAIT_FOREVER ||
        xrtMemRangesOverlap(C,sizeof(*C)+C->Host.Size+C->Target.Size,Future,sizeof(*Future))){
        xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP call inputs");return XRPC_ADMISSION_ERROR;
    }
    xrtMutexLock(&C->Gate);
    xrpcadmission Admission=XRPC_ADMISSION_ACCEPTED;
    if(C->Info.State>=XRPC_HTTP_CLOSING)Admission=XRPC_ADMISSION_CLOSED;
    else if(C->Info.Requests>=C->Config.MaxRequests)Admission=XRPC_ADMISSION_OVERLOADED;
    if(Admission!=XRPC_ADMISSION_ACCEPTED){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return Admission;}
    if(C->Serial==UINT64_MAX){xrtMutexUnlock(&C->Gate);xrpcHttpReport(XERR_RANGE,XRPC_HTTP_ID_EXHAUSTED,"RPC wire ID space exhausted");return XRPC_ADMISSION_ERROR;}
    uint64 Serial=++C->Serial;++C->Info.Requests;xrpcHttpRef(C);xrtMutexUnlock(&C->Gate);
    xrpchttprequest* R=xrtCalloc(1,sizeof(*R));if(!R){xrpcHttpReservationDrop(C,0);return XRPC_ADMISSION_ERROR;}
    R->Client=C;xrtAtomic32Init(&R->References,1);xfuture* F=NULL;
    xrpccallconfig CallConfig;xrpcCallConfigInit(&CallConfig);CallConfig.Timeout=Timeout;CallConfig.User=R;CallConfig.Wake=xrpcHttpCancelWake;CallConfig.Drop=xrpcHttpRequestDrop;
    xrpcHttpRequestRef(R);
    if(!xrpcCallCreateEntered(&CallConfig,&R->Call,&F,Entered)){xrpcHttpRequestDrop(R);goto Error;}
    if(!xrtNetPostInit(&R->CancelPost))goto Error;
    int N=snprintf(R->Id,sizeof(R->Id),"\"%"PRIu64"\"",Serial);if(N<=0 || (size_t)N>=sizeof(R->Id))goto Error;R->IdBytes=(size_t)N;
    xrpcoutgoing Message={0};Message.Kind=XRPC_MESSAGE_CALL;Message.Id=(xstrview){R->Id,R->IdBytes};Message.Method=Method;Message.Params=Params;
    xrpcencodeconfig Encode;xrpcEncodeConfigInit(&Encode);Encode.MaxOutputBytes=C->Config.MaxRequestBytes;
    size_t BodyBytes=0,HeadBytes=0;if(!xrpcEncodeSize(&Message,1,false,&Encode,&BodyBytes))goto Error;
    char Length[24];N=snprintf(Length,sizeof(Length),"%zu",BodyBytes);if(N<=0 || (size_t)N>=sizeof(Length))goto Error;
    xhttpfield Fields[]={{XRT_STR_LITERAL("Host"),C->Host},{XRT_STR_LITERAL("Content-Type"),XRT_STR_LITERAL("application/json")},
        {XRT_STR_LITERAL("Content-Length"),(xstrview){Length,(size_t)N}},
        {XRT_STR_LITERAL("Accept-Encoding"),XRT_STR_LITERAL("identity")}};
    if(!xrtHttp1RequestWrite(XRT_STR_LITERAL("POST"),C->Target,XHTTP_VERSION_1_1,Fields,4,NULL,0,&HeadBytes))goto Error;
    if(BodyBytes>C->Config.MaxRequestBytes || HeadBytes>C->Config.MaxRequestBytes-BodyBytes){Admission=XRPC_ADMISSION_OVERLOADED;goto Refused;}
    size_t Bytes=HeadBytes+BodyBytes;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XRPC_HTTP_CLOSING)Admission=XRPC_ADMISSION_CLOSED;
    else if(Bytes>C->Config.MaxRequestBytesTotal-C->Info.RequestBytes)Admission=XRPC_ADMISSION_OVERLOADED;
    if(Admission==XRPC_ADMISSION_ACCEPTED){C->Info.RequestBytes+=Bytes;R->WireBytes=Bytes;}
    xrtMutexUnlock(&C->Gate);if(Admission!=XRPC_ADMISSION_ACCEPTED)goto Refused;
    R->Wire=xrtMalloc(Bytes);size_t Written=0;
    if(!R->Wire || !xrtHttp1RequestWrite(XRT_STR_LITERAL("POST"),C->Target,XHTTP_VERSION_1_1,Fields,4,R->Wire,HeadBytes,&Written) || Written!=HeadBytes ||
        !xrpcEncodeWrite(R->Wire+HeadBytes,BodyBytes,&Message,1,false,&Encode,NULL))goto Error;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XRPC_HTTP_CLOSING){xrtMutexUnlock(&C->Gate);Admission=XRPC_ADMISSION_CLOSED;goto Refused;}
    if(!xrpcHttpWakeLocked(C)){xrtMutexUnlock(&C->Gate);goto Error;}
    xrpcHttpRequestRef(R);if(C->SubmitTail)C->SubmitTail->Next=R;else C->SubmitHead=R;C->SubmitTail=R;++C->Info.Accepted;
    xrtMutexUnlock(&C->Gate);*Future=F;xrpcHttpRequestDrop(R);return XRPC_ADMISSION_ACCEPTED;
Refused:
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    {xerror* E=xrtTakeError();if(R->Call)(void)xrpcCallDestroy(R->Call);xrtFutureDestroy(F);xrpcHttpRequestDrop(R);xrtClearError();xrtSetErrorTake(E);}return Admission;
Error:
    {xerror* E=xrtTakeError();if(R->Call)(void)xrpcCallDestroy(R->Call);xrtFutureDestroy(F);xrpcHttpRequestDrop(R);xrtClearError();xrtSetErrorTake(E);}return XRPC_ADMISSION_ERROR;
}
XRPC_API bool xrpcHttpInfo(xrpchttpclient* C,xrpchttpinfo* Info)
{
    if(!C || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) ||
        xrtMemRangesOverlap(C,sizeof(*C)+C->Host.Size+C->Target.Size,Info,sizeof(*Info)))return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP info output");
    xrtMutexLock(&C->Gate);*Info=C->Info;xrtMutexUnlock(&C->Gate);return true;
}
XRPC_API bool xrpcHttpClose(xrpchttpclient* C)
{
    if(!C)return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"HTTP client is NULL");
    xrtMutexLock(&C->Gate);if(C->Info.State<XRPC_HTTP_CLOSING){C->Info.State=XRPC_HTTP_CLOSING;C->Info.CloseStatus=XRPC_CALL_CLOSED;}
    bool R=xrpcHttpWakeLocked(C);xrtMutexUnlock(&C->Gate);return R;
}
XRPC_API xfuture* xrpcHttpClosed(xrpchttpclient* C)
{if(!C){xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"HTTP client is NULL");return NULL;}return xrtFutureRef(C->CloseFuture);}
#endif
