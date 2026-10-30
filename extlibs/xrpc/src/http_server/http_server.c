#include <xrpc/http_server.h>
#include "../internal/xrpc_server_internal.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <xrt/memory.h>
#include <xrt/http_expect.h>
#include <xrt/http_trailer.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_SERVER)
typedef struct xrpchspeer xrpchspeer;
typedef struct xrpchsop xrpchsop;
typedef struct xrpchsshard {
    xrpchttpserver* Server;xnetworker* Worker;xnetpost Post;xatomic32 Posted;
    xrpchspeer *Peers,*Writers,*WritersTail;
} xrpchsshard;
struct xrpchttpserver {
    xatomic32 References;
    xmutex Gate;xcond Idle;
    xnetengine* Engine;xnetlistener* Listener;xrpcserver* Execution;
    xrpchttpserverconfig Config;xrpchttpserverinfo Info;xnetaddr Local;
    size_t OwnedBytes,InputLimit,ShardCount,WaitingWriters;
    xrpchsshard Shards[];
};
struct xrpchspeer {
    xatomic32 References,Posted;
    xrpchttpserver* Server;xrpchsshard* Shard;xnetstream* Stream;xnetpost Post;
    xrpchspeer *Next,*Previous,*WriterNext,*WriterPrevious;
    xrpchsop* Operation;
    xhttpfield *Fields,*Trailers;
    unsigned char *Input,*Payload;
    size_t FixedBytes,InputCapacity,InputUsed,InputScan,PayloadCapacity,PayloadUsed,WireBytes;
    double InputEpoch,ReadEpoch,Entered;uint64 Timer;
    xhttp1body Body;xhttpversion Version;
    bool Linked,Waiting,Closed,Aborting,ReadEnded,Active,BodyStarted,CloseAfter,Continue;
};
struct xrpchsop {
    xatomic64 References,Queued;
    xrpchspeer* Peer;xfuture* Future;xfuturewatch Watch;
    xstrview Body;size_t HeadSize,HeadOffset,BodyOffset;uint16 Status;
    bool Prepared;
    char Head[320];
};
static void xrpcHsPeerWork(xnetworker* Worker,ptr Data);
static void xrpcHsShardWork(xnetworker* Worker,ptr Data);
static void xrpcHsTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data);
static void xrpcHsPeerWake(xrpchspeer* P);
static bool xrpcHsReport(xerrkind Kind,xrpchttpservererror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false;}
static double xrpcHsDeadline(double Began,int64 Timeout)
{return Timeout==XRT_WAIT_FOREVER?INFINITY:nextafter(Began+(double)(Timeout/1000)+(double)(Timeout%1000)*.001,INFINITY);}
static int64 xrpcHsRemaining(double End)
{if(End==INFINITY)return XRT_WAIT_FOREVER;double N=ceil((End-xrtTimer())*1000.);return N<=0?0:N>=0x1p63?INT64_MAX:(int64)N;}
static void xrpcHsRef(xrpchttpserver* H){xrtAtomic32FetchAdd(&H->References,1,XMEMORY_RELAXED);}
static void xrpcHsDrop(const void* Data)
{xrpchttpserver* H=(xrpchttpserver*)Data;xrtMutexLock(&H->Gate);xrtAtomic32FetchSub(&H->References,1,XMEMORY_ACQ_REL);xrtCondBroadcast(&H->Idle);xrtMutexUnlock(&H->Gate);}
static bool xrpcHsCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpchttpserver*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcHsOpaque(const void* Data,xrtownershipvisitor Visit,ptr Context)
{(void)Data;(void)Visit;(void)Context;return false;}
static const xrtownershipops xrpcHsOps={xrpcHsCount,xrpcHsOpaque};
static bool xrpcHsClosing(xrpchttpserver* H)
{xrtMutexLock(&H->Gate);bool C=H->Info.State>=XRPC_HTTP_SERVER_CLOSING;xrtMutexUnlock(&H->Gate);return C;}
static bool xrpcHsReserve(xrpchttpserver* H,size_t Bytes)
{xrtMutexLock(&H->Gate);bool OK=Bytes<=H->Config.MaxBufferBytes-H->Info.BufferBytes;if(OK)H->Info.BufferBytes+=Bytes;xrtMutexUnlock(&H->Gate);return OK;}
static void xrpcHsRefund(xrpchttpserver* H,size_t Bytes)
{xrtMutexLock(&H->Gate);H->Info.BufferBytes-=Bytes;xrtMutexUnlock(&H->Gate);}
static void xrpcHsShardWake(xrpchsshard* S)
{
    uint32 Old=0;if(!xrtAtomic32CompareExchange(&S->Posted,&Old,1,XMEMORY_ACQ_REL,XMEMORY_RELAXED))return;
    xrpcHsRef(S->Server);if(!xrtNetPost(S->Worker,&S->Post,xrpcHsShardWork,S)){xrtAtomic32Store(&S->Posted,0,XMEMORY_RELEASE);xrpcHsDrop(S->Server);}
}
static void xrpcHsCredit(xrpchttpserver* H,size_t Bytes)
{
    xrtMutexLock(&H->Gate);H->Info.QueuedBytes-=Bytes;bool Wake=H->WaitingWriters!=0;xrtMutexUnlock(&H->Gate);
    if(Wake)for(size_t K=0;K<H->ShardCount;++K)xrpcHsShardWake(&H->Shards[K]);
}
static void xrpcHsPeerRef(xrpchspeer* P){xrtAtomic32FetchAdd(&P->References,1,XMEMORY_RELAXED);}
static void xrpcHsPeerDrop(const void* Data)
{
    xrpchspeer* P=(xrpchspeer*)Data;if(xrtAtomic32FetchSub(&P->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrpchttpserver* H=P->Server;size_t Bytes=P->FixedBytes+P->InputCapacity+P->PayloadCapacity;
    xrtNetStreamDestroy(P->Stream);xrtFree(P->Input);xrtFree(P->Payload);xrtFree(P);
    xrtMutexLock(&H->Gate);--H->Info.Connections;H->Info.BufferBytes-=Bytes;xrtMutexUnlock(&H->Gate);xrpcHsDrop(H);
}
static bool xrpcHsPeerCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpchspeer*)Data)->References,XMEMORY_RELAXED);return true;}
static const xrtownershipops xrpcHsPeerOps={xrpcHsPeerCount,xrpcHsOpaque};
static void xrpcHsPeerWake(xrpchspeer* P)
{
    uint32 Old=0;if(!xrtAtomic32CompareExchange(&P->Posted,&Old,1,XMEMORY_ACQ_REL,XMEMORY_RELAXED))return;
    xrpcHsPeerRef(P);if(!xrtNetPost(P->Shard->Worker,&P->Post,xrpcHsPeerWork,P)){xrtAtomic32Store(&P->Posted,0,XMEMORY_RELEASE);xrpcHsPeerDrop(P);}
}
static void xrpcHsUnwait(xrpchspeer* P)
{
    if(!P->Waiting)return;
    xrpchsshard* S=P->Shard;
    if(P->WriterPrevious)P->WriterPrevious->WriterNext=P->WriterNext;else S->Writers=P->WriterNext;
    if(P->WriterNext)P->WriterNext->WriterPrevious=P->WriterPrevious;else S->WritersTail=P->WriterPrevious;
    P->WriterNext=P->WriterPrevious=NULL;P->Waiting=false;
    xrtMutexLock(&P->Server->Gate);--P->Server->WaitingWriters;xrtMutexUnlock(&P->Server->Gate);
}
static size_t xrpcHsSendReserve(xrpchspeer* P,size_t Bytes)
{
    xrpchttpserver* H=P->Server;xrtMutexLock(&H->Gate);size_t Left=H->Config.MaxQueuedBytes-H->Info.QueuedBytes;
    if(Bytes>Left)Bytes=Left;
    if(Bytes)H->Info.QueuedBytes+=Bytes;
    else if(!P->Waiting){P->Waiting=true;++H->WaitingWriters;P->WriterPrevious=P->Shard->WritersTail;
        if(P->WriterPrevious)P->WriterPrevious->WriterNext=P;else P->Shard->Writers=P;P->Shard->WritersTail=P;}
    xrtMutexUnlock(&H->Gate);return Bytes;
}
static void xrpcHsOpRef(xrpchsop* O){xrtAtomic64FetchAdd(&O->References,1,XMEMORY_RELAXED);}
static void xrpcHsOpDrop(ptr Data)
{
    xrpchsop* O=Data;if(xrtAtomic64FetchSub(&O->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrpchspeer* P=O->Peer;xrpchttpserver* H=P->Server;xrtFutureDestroy(O->Future);xrtFree(O);
    xrtMutexLock(&H->Gate);--H->Info.Operations;H->Info.BufferBytes-=sizeof(xrpchsop);xrtMutexUnlock(&H->Gate);xrpcHsPeerDrop(P);
}
static void xrpcHsReady(ptr Data){xrpcHsPeerWake(((xrpchsop*)Data)->Peer);}
static xrpchsop* xrpcHsOpCreate(xrpchspeer* P)
{
    xrpchttpserver* H=P->Server;xrtMutexLock(&H->Gate);
    bool OK=H->Info.Operations<H->Config.MaxOperations && sizeof(xrpchsop)<=H->Config.MaxBufferBytes-H->Info.BufferBytes;
    if(OK){++H->Info.Operations;H->Info.BufferBytes+=sizeof(xrpchsop);}else ++H->Info.OperationRejected;
    xrtMutexUnlock(&H->Gate);if(!OK)return NULL;
    xrpchsop* O=xrtCalloc(1,sizeof(*O));if(!O){xrtMutexLock(&H->Gate);--H->Info.Operations;H->Info.BufferBytes-=sizeof(*O);++H->Info.OperationRejected;xrtMutexUnlock(&H->Gate);return NULL;}
    xrtAtomic64Init(&O->References,1);xrtAtomic64Init(&O->Queued,0);O->Peer=P;xrpcHsPeerRef(P);return O;
}
static void xrpcHsSent(ptr Data,cbytes Bytes,size_t Size)
{(void)Bytes;xrpchsop* O=Data;xrtAtomic64FetchSub(&O->Queued,(uint64)Size,XMEMORY_ACQ_REL);xrpcHsCredit(O->Peer->Server,Size);xrpcHsPeerWake(O->Peer);xrpcHsOpDrop(O);}
static void xrpcHsContinueSent(ptr Data,cbytes Bytes,size_t Size)
{(void)Bytes;xrpchspeer* P=Data;xrpcHsCredit(P->Server,Size);xrpcHsPeerWake(P);xrpcHsPeerDrop(P);}
static bool xrpcHsGrow(xrpchspeer* P,bool Body,size_t Need)
{
    size_t Old=Body?P->PayloadCapacity:P->InputCapacity,Limit=Body?(size_t)P->Server->Config.Body.MaxBody:P->Server->InputLimit;
    if(Need<=Old)return true;
    if(Need>Limit)return false;
    size_t N=Old?Old:512;
    if(N>Limit)N=Limit;
    while(N<Need){if(N>Limit/2u){N=Limit;break;}N*=2u;}
    if(!xrpcHsReserve(P->Server,N))return false;
    ptr Q=xrtRealloc(Body?P->Payload:P->Input,N);
    if(!Q){xrpcHsRefund(P->Server,N);return false;}
    if(Body){P->Payload=Q;P->PayloadCapacity=N;}else{P->Input=Q;P->InputCapacity=N;}
    xrpcHsRefund(P->Server,Old);return true;
}
static void xrpcHsTimerCancel(xrpchspeer* P)
{uint64 Id=P->Timer;P->Timer=0;if(Id)(void)xrtNetEngineTimerCancelCurrent(P->Server->Engine,Id);}
static bool xrpcHsTimerArm(xrpchspeer* P)
{
    xrpcHsTimerCancel(P);int64 T=P->Active?P->Server->Config.RequestTimeout:P->Server->Config.IdleTimeout;
    if(T==XRT_WAIT_FOREVER)return true;
    double End=xrpcHsDeadline(P->Entered,T);xrpcHsPeerRef(P);
    P->Timer=__xrtNetEngineSchedule(P->Server->Engine,xrtNetWorkerIndex(P->Shard->Worker),End,xrpcHsTimer,P);
    if(P->Timer)return true;
    xrpcHsPeerDrop(P);return false;
}
static void xrpcHsTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xrpchspeer* P=Data;
    if(P->Timer==Id){P->Timer=0;if(Result!=XNET_RESULT_CANCELLED){P->Aborting=true;xrtMutexLock(&P->Server->Gate);if(Result==XNET_RESULT_OK)++P->Server->Info.Timeouts;else ++P->Server->Info.TransportErrors;xrtMutexUnlock(&P->Server->Gate);}xrpcHsPeerWake(P);}
    xrpcHsPeerDrop(P);
}
static bool xrpcHsHeader(xrpchsop* O)
{
    xrpchspeer* P=O->Peer;char Length[24],Date[40];int N=snprintf(Length,sizeof(Length),"%zu",O->Body.Size);if(N<=0)return false;
    xhttpfield F[5];size_t Count=0;
    if(O->Status!=204)F[Count++]=(xhttpfield){XRT_STR_LITERAL("Content-Length"),(xstrview){Length,(size_t)N}};
    if(O->Body.Size)F[Count++]=(xhttpfield){XRT_STR_LITERAL("Content-Type"),XRT_STR_LITERAL("application/json")};
    F[Count++]=(xhttpfield){XRT_STR_LITERAL("Connection"),(P->CloseAfter || P->ReadEnded)?XRT_STR_LITERAL("close"):XRT_STR_LITERAL("keep-alive")};
    if(O->Status==405)F[Count++]=(xhttpfield){XRT_STR_LITERAL("Allow"),XRT_STR_LITERAL("POST")};
    size_t D=xrtTimeWriteHTTPDate(Date,sizeof(Date),xrtNow());if(D)F[Count++]=(xhttpfield){XRT_STR_LITERAL("Date"),(xstrview){Date,D}};
    if(!xrtHttp1ResponseWrite(P->Version,O->Status,(xstrview){0},F,Count,O->Head,sizeof(O->Head),&O->HeadSize))return false;
    O->Prepared=true;return true;
}
static void xrpcHsError(xrpchspeer* P,uint16 Status)
{
    if(P->PayloadCapacity){size_t Bytes=P->PayloadCapacity;xrtFree(P->Payload);P->Payload=NULL;P->PayloadCapacity=P->PayloadUsed=0;xrpcHsRefund(P->Server,Bytes);}
    P->CloseAfter=true;P->Continue=false;P->Operation=xrpcHsOpCreate(P);
    if(!P->Operation){P->Aborting=true;return;}P->Operation->Status=Status;
    if(!xrpcHsHeader(P->Operation))P->Aborting=true;
    if(Status==400 || Status==413 || Status==415 || Status==417 || Status==431 || Status==501){xrtMutexLock(&P->Server->Gate);++P->Server->Info.ProtocolErrors;xrtMutexUnlock(&P->Server->Gate);}
}
static void xrpcHsSubmit(xrpchspeer* P,xstrview Body)
{
    xrpchttpserver* H=P->Server;xrtMutexLock(&H->Gate);++H->Info.Requests;xrtMutexUnlock(&H->Gate);
    P->Operation=xrpcHsOpCreate(P);if(!P->Operation){P->Aborting=true;return;}xrpchsop* O=P->Operation;
    xrpcserveradmission A=xrpcServerSubmitEntered(H->Execution,Body,H->Config.RequestTimeout,&O->Future,P->Entered);
    if(P->PayloadCapacity>65536u){size_t Bytes=P->PayloadCapacity;xrtFree(P->Payload);P->Payload=NULL;P->PayloadCapacity=P->PayloadUsed=0;xrpcHsRefund(H,Bytes);}
    if(A!=XRPC_SERVER_ACCEPTED){xrtMutexLock(&H->Gate);++H->Info.OperationRejected;xrtMutexUnlock(&H->Gate);O->Status=503;P->CloseAfter=true;if(!xrpcHsHeader(O))P->Aborting=true;return;}
    xrtMutexLock(&H->Gate);++H->Info.Dispatched;xrtMutexUnlock(&H->Gate);
    if(!xrtFutureWatchInit(&O->Watch,xrpcHsReady,xrpcHsOpDrop,O)){P->Aborting=true;return;}
    xrpcHsOpRef(O);xfuturewatchresult R=xrtFutureWatchAdd(O->Future,&O->Watch);
    if(R!=XFUTURE_WATCH_PENDING){if(R==XFUTURE_WATCH_ERROR)P->Aborting=true;else xrpcHsReady(O);xrpcHsOpDrop(O);}
}
static bool xrpcHsMedia(xstrview V)
{
    size_t N=0;while(N<V.Size && V.Data[N]!=';')++N;
    if(!xrtHttpTokenEqual(xrtHttpOwsTrim((xstrview){V.Data,N}),XRT_STR_LITERAL("application/json")))return false;
    xstrview Params={V.Data+N,V.Size-N};size_t At=0;bool Charset=false;for(;;){xhttpparam Param;xhttpnext R=xrtHttpNameValueNext(Params,&At,&Param,';',true);
        if(R==XHTTP_NEXT_END)return true;
        if(R==XHTTP_NEXT_ERROR)return false;
        if(!(Param.Flags&XHTTP_PARAM_HAS_VALUE))return false;
        if(xrtHttpTokenEqual(Param.Name,XRT_STR_LITERAL("charset"))){if(Charset || !xrtHttpParamTokenEqual(&Param,XRT_STR_LITERAL("utf-8")))return false;Charset=true;}}
}
static uint16 xrpcHsValidate(xrpchspeer* P,const xhttp1head* Head,xhttp1bodyplan* Plan)
{
    const xhttpfield* Host=NULL;xhttpnext R=xrtHttpFieldGetUnique(Head->Fields,Head->FieldCount,XRT_STR_LITERAL("Host"),&Host);
    if(R==XHTTP_NEXT_ERROR || (Head->Version==XHTTP_VERSION_1_1 && R!=XHTTP_NEXT_ITEM) || (Host && !xrtHttpHostValid(Host->Value)))return 400;
    if(Head->MethodCode!=XHTTP_METHOD_POST)return 405;
    xhttptarget T;if(!xrtHttpTargetParse(Head->Method,Head->Target,&T))return 400;
    if(T.Form!=XHTTP_TARGET_ORIGIN && T.Form!=XHTTP_TARGET_ABSOLUTE)return 400;
    if(T.Form==XHTTP_TARGET_ABSOLUTE && !xrtHttpTokenEqual(T.Scheme,XRT_STR_LITERAL("http")))return 400;
    xstrview Path=T.Path.Size?T.Path:XRT_STR_LITERAL("/");
    if(Path.Size!=P->Server->Config.Path.Size || memcmp(Path.Data,P->Server->Config.Path.Data,Path.Size))return 404;
    if((Head->Flags&XHTTP1_UPGRADE) || (Head->Flags&XHTTP1_TRANSFER_OTHER))return 501;
    const xhttpfield* Type=NULL;if(xrtHttpFieldGetUnique(Head->Fields,Head->FieldCount,XRT_STR_LITERAL("Content-Type"),&Type)!=XHTTP_NEXT_ITEM || !xrpcHsMedia(Type->Value))return 415;
    for(size_t K=0;K<Head->FieldCount;++K)if(xrtHttpFieldNameEqual(Head->Fields[K].Name,XRT_STR_LITERAL("Content-Encoding")) && !xrtHttpTokenEqual(Head->Fields[K].Value,XRT_STR_LITERAL("identity")))return 415;
    size_t Names=0;if(!xrtHttpTrailerCount(Head->Fields,Head->FieldCount,&Names))return 400;
    xhttpexpectresult Expect=xrtHttpExpectFields(Head->Fields,Head->FieldCount);if(Expect==XHTTP_EXPECT_ERROR)return 400;if(Expect==XHTTP_EXPECT_UNSUPPORTED)return 417;
    if(!xrtHttp1RequestBodyPlan(Head,Plan))return 400;
    if(Plan->Mode==XHTTP1_BODY_FIXED && Plan->Length>P->Server->Config.Body.MaxBody)return 413;
    P->Continue=Expect==XHTTP_EXPECT_CONTINUE && Head->Version==XHTTP_VERSION_1_1 && (Plan->Mode==XHTTP1_BODY_CHUNKED || (Plan->Mode==XHTTP1_BODY_FIXED && Plan->Length));
    return 0;
}
static uint16 xrpcHsParseStatus(const xhttp1errorinfo* E)
{
    if(xrtErrorKind(xrtGetError())==XERR_MEMORY)return 503;
    switch(E->Code){case XHTTP1_ERROR_HEAD_TOO_LARGE:case XHTTP1_ERROR_FIELD_LINE_TOO_LARGE:case XHTTP1_ERROR_START_LINE_TOO_LARGE:case XHTTP1_ERROR_TOO_MANY_FIELDS:return 431;
    case XHTTP1_ERROR_BODY_TOO_LARGE:case XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE:case XHTTP1_ERROR_TRAILER_TOO_LARGE:case XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE:case XHTTP1_ERROR_TOO_MANY_TRAILERS:return 413;default:return 400;}
}
static bool xrpcHsWire(xrpchspeer* P,size_t Bytes)
{if(Bytes>P->Server->Config.MaxRequestWireBytes-P->WireBytes){xrpcHsError(P,413);return false;}P->WireBytes+=Bytes;return true;}
static void xrpcHsParse(xrpchspeer* P,bool End)
{
    size_t Offset=0;
    if(!P->Active){P->Active=true;P->Entered=P->InputEpoch?P->InputEpoch:xrtTimer();if(!xrpcHsTimerArm(P)){P->Aborting=true;return;}}
    while(!P->Operation && !P->Aborting){
        if(!P->BodyStarted){size_t HeadEnd=0;
            while(P->InputScan<P->InputUsed){size_t N=++P->InputScan;if(N>=4 && !memcmp(P->Input+N-4,"\r\n\r\n",4)){HeadEnd=N;break;}if(N>=P->Server->Config.Head.MaxHead)break;}
            if(!HeadEnd){if(End || P->InputUsed>=P->Server->Config.Head.MaxHead || P->InputUsed>P->Server->Config.MaxRequestWireBytes)xrpcHsError(P,End?400:P->InputUsed>=P->Server->Config.Head.MaxHead?431:413);break;}
            xhttp1head Head;xrtHttp1HeadInit(&Head,P->Fields,P->Server->Config.Head.MaxFields);xhttp1errorinfo E={0};
            if(xrtHttp1RequestParse((xbytesview){P->Input,HeadEnd},&Head,&P->Server->Config.Head,&E)!=XHTTP1_READY){xrpcHsError(P,xrpcHsParseStatus(&E));break;}
            P->Version=Head.Version;P->CloseAfter=(Head.Flags&XHTTP1_CONNECTION_CLOSE)!=0 || (Head.Version==XHTTP_VERSION_1_0 && !(Head.Flags&XHTTP1_KEEP_ALIVE));
            xhttp1bodyplan Plan;uint16 Bad=xrpcHsValidate(P,&Head,&Plan);if(Bad){xrpcHsError(P,xrtErrorKind(xrtGetError())==XERR_MEMORY?503:Bad);break;}
            if(!xrpcHsWire(P,HeadEnd))break;
            Offset=HeadEnd;P->BodyStarted=true;
            if((Plan.Mode==XHTTP1_BODY_FIXED && Plan.Length<=P->InputUsed-Offset) || Plan.Mode==XHTTP1_BODY_NONE){
                size_t N=Plan.Mode==XHTTP1_BODY_NONE?0:(size_t)Plan.Length;P->Continue=false;
                if(xrpcHsWire(P,N)){xrpcHsSubmit(P,(xstrview){(cstr)P->Input+Offset,N});Offset+=N;}break;
            }
            if(!xrtHttp1BodyInit(&P->Body,&Plan,P->Trailers,P->Server->Config.Body.MaxTrailers,&P->Server->Config.Body)){P->Aborting=true;break;}
        }
        if(P->Continue)break;
        size_t Used=0;xbytesview Data={0};xhttp1errorinfo E={0};
        xhttp1bodystatus S=xrtHttp1BodyRead(&P->Body,(xbytesview){P->Input+Offset,P->InputUsed-Offset},End,&Used,&Data,&E);
        if(!xrpcHsWire(P,Used))break;
        Offset+=Used;
        if(S==XHTTP1_BODY_ERROR || S==XHTTP1_BODY_FIELDS){xrpcHsError(P,S==XHTTP1_BODY_FIELDS?413:xrpcHsParseStatus(&E));break;}
        if(S==XHTTP1_BODY_DATA){
            if(Data.Size>(size_t)P->Server->Config.Body.MaxBody-P->PayloadUsed){xrpcHsError(P,413);break;}
            if(!xrpcHsGrow(P,true,P->PayloadUsed+Data.Size)){xrpcHsError(P,503);break;}
            memcpy(P->Payload+P->PayloadUsed,Data.Data,Data.Size);P->PayloadUsed+=Data.Size;
        }
        if(S==XHTTP1_BODY_DONE){
            if(!xrtHttpTrailerSectionValid(P->Body.Trailers,P->Body.TrailerCount))xrpcHsError(P,400);
            else xrpcHsSubmit(P,(xstrview){(cstr)P->Payload,P->PayloadUsed});
            break;
        }
        if(!Used || (S==XHTTP1_BODY_MORE && Offset==P->InputUsed))break;
    }
    if(Offset){memmove(P->Input,P->Input+Offset,P->InputUsed-Offset);P->InputUsed-=Offset;P->InputScan=0;if(!P->InputUsed)P->InputEpoch=0;}
    if(!P->Operation && !P->Aborting && P->InputUsed>P->Server->Config.MaxRequestWireBytes-P->WireBytes)xrpcHsError(P,413);
}
static void xrpcHsRead(xrpchspeer* P)
{
    size_t Budget=P->Server->Config.ReadBytes;
    if(P->InputUsed)xrpcHsParse(P,false);
    while(Budget && !P->Operation && !P->Aborting && !P->Continue){
        size_t Available=xrtNetStreamAvailable(P->Stream);if(!Available)break;
        if(P->InputUsed==P->InputCapacity && !xrpcHsGrow(P,false,P->InputUsed+1u)){xrpcHsError(P,503);break;}
        size_t N=P->InputCapacity-P->InputUsed;if(N>Available)N=Available;if(N>Budget)N=Budget;
        if(!P->InputUsed)P->InputEpoch=P->ReadEpoch?P->ReadEpoch:xrtTimer();
        N=xrtNetStreamRead(P->Stream,P->Input+P->InputUsed,N);if(!N)break;P->InputUsed+=N;Budget-=N;
        if(!xrtNetStreamAvailable(P->Stream))P->ReadEpoch=0;
        xrpcHsParse(P,false);
    }
    if(P->ReadEnded && !P->Operation && !P->Aborting && !xrtNetStreamAvailable(P->Stream)){
        if(P->Active || P->InputUsed)xrpcHsParse(P,true);else P->Aborting=true;
    }
}
static void xrpcHsWrite(xrpchspeer* P)
{
    xrpchsop* O=P->Operation;
    if(!O->Prepared){xfutureresult R;if(xrtFutureState(O->Future)==XFUTURE_PENDING)return;
        if(!xrtFutureResult(O->Future,&R) || R.State!=XFUTURE_RESOLVED || !R.Value){O->Status=500;P->CloseAfter=true;}
        else{const xrpcserverresult* V=R.Value;
            if(V->Status==XRPC_SERVER_DONE){O->Body=V->Wire;O->Status=O->Body.Size?200:204;}
            else{O->Status=(V->Status==XRPC_SERVER_TIMEOUT || V->Status==XRPC_SERVER_CANCELLED)?503:500;P->CloseAfter=true;}}
        if(!xrpcHsHeader(O)){P->Aborting=true;return;}}
    size_t Budget=P->Server->Config.WriteBytes;
    while(Budget && !P->Aborting){bool Head=O->HeadOffset<O->HeadSize;size_t Offset=Head?O->HeadOffset:O->BodyOffset;
        size_t Left=(Head?O->HeadSize:O->Body.Size)-Offset;if(!Left)break;
        size_t N=Left;if(N>P->Server->Config.WriteChunkBytes)N=P->Server->Config.WriteChunkBytes;if(N>Budget)N=Budget;
        xnetstreamstats Stats;if(!xrtNetStreamStats(P->Stream,&Stats)){P->Aborting=true;break;}
        size_t Room=P->Server->Config.Listen.Stream.WriteLimit-Stats.QueuedBytes;if(N>Room)N=Room;if(!N)break;
        N=xrpcHsSendReserve(P,N);if(!N)break;
        xrpcHsOpRef(O);xrtAtomic64FetchAdd(&O->Queued,(uint64)N,XMEMORY_RELAXED);
        xnetresult R=xrtNetStreamSendRef(P->Stream,(const unsigned char*)(Head?O->Head:O->Body.Data)+Offset,N,xrpcHsSent,O);
        if(R!=XNET_RESULT_OK){xrtAtomic64FetchSub(&O->Queued,(uint64)N,XMEMORY_RELAXED);xrpcHsCredit(P->Server,N);xrpcHsOpDrop(O);if(R!=XNET_RESULT_AGAIN)P->Aborting=true;break;}
        if(Head)O->HeadOffset+=N;else O->BodyOffset+=N;Budget-=N;
    }
    if(O->HeadOffset==O->HeadSize && O->BodyOffset==O->Body.Size && !xrtAtomic64Load(&O->Queued,XMEMORY_ACQUIRE)){
        P->Operation=NULL;xrpcHsOpDrop(O);xrtMutexLock(&P->Server->Gate);++P->Server->Info.Responses;xrtMutexUnlock(&P->Server->Gate);
        if(P->CloseAfter || P->ReadEnded){(void)xrtNetStreamClose(P->Stream);return;}
        P->Active=P->BodyStarted=false;P->PayloadUsed=P->WireBytes=P->InputScan=0;P->Version=XHTTP_VERSION_1_1;P->Entered=xrtTimer();
        if(!P->InputUsed && P->InputCapacity>65536u && P->InputCapacity>P->Server->Config.InitialInputBytes){
            size_t Bytes=P->InputCapacity;xrtFree(P->Input);P->Input=NULL;P->InputCapacity=0;xrpcHsRefund(P->Server,Bytes);
            if(!xrpcHsGrow(P,false,P->Server->Config.InitialInputBytes))P->Aborting=true;
        }
        if(!xrpcHsTimerArm(P))P->Aborting=true;
    }else if(!Budget && !P->Waiting)xrpcHsPeerWake(P);
}
static void xrpcHsContinue(xrpchspeer* P)
{
    static const char Text[]="HTTP/1.1 100 Continue\r\n\r\n";
    /* One short interim reference; require its complete budget atomically. */
    xrpchttpserver* H=P->Server;xrtMutexLock(&H->Gate);bool Room=sizeof(Text)-1<=H->Config.MaxQueuedBytes-H->Info.QueuedBytes;
    if(Room)H->Info.QueuedBytes+=sizeof(Text)-1;
    xrtMutexUnlock(&H->Gate);
    if(!Room){(void)xrpcHsSendReserve(P,0);return;}
    xrpcHsPeerRef(P);xnetresult R=xrtNetStreamSendRef(P->Stream,Text,sizeof(Text)-1,xrpcHsContinueSent,P);
    if(R==XNET_RESULT_OK)P->Continue=false;
    else{xrpcHsCredit(H,sizeof(Text)-1);xrpcHsPeerDrop(P);if(R!=XNET_RESULT_AGAIN)P->Aborting=true;}
}
static void xrpcHsPeerWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xrpchspeer* P=Data;xrtAtomic32Store(&P->Posted,0,XMEMORY_RELEASE);xrtClearError();xrpcHsUnwait(P);
    if(xrpcHsClosing(P->Server))P->Aborting=true;
    if(P->Closed || P->Aborting){
        xrpcHsTimerCancel(P);
        if(P->Operation){xrpchsop* O=P->Operation;P->Operation=NULL;if(O->Future)(void)xrtFutureCancel(O->Future);xrpcHsOpDrop(O);}
        if(!P->Closed)(void)xrtNetStreamAbort(P->Stream);
        if(P->Closed && P->Linked){if(P->Previous)P->Previous->Next=P->Next;else P->Shard->Peers=P->Next;if(P->Next)P->Next->Previous=P->Previous;P->Linked=false;xrpcHsPeerDrop(P);}
    }else{
        if(P->Operation)xrpcHsWrite(P);
        if(!P->Operation && !P->Aborting && xrtNetStreamState(P->Stream)==XNET_STREAM_OPEN){
            if(P->Continue)xrpcHsContinue(P);
            if(!P->Continue){xrpcHsRead(P);if(P->Operation || P->Continue)xrpcHsPeerWake(P);}
        }
        if(P->Aborting)xrpcHsPeerWake(P);
        else if(P->Operation || P->Continue)xrtNetStreamPause(P->Stream);
        else if(xrtNetStreamState(P->Stream)==XNET_STREAM_OPEN){if(xrtNetStreamAvailable(P->Stream))xrpcHsPeerWake(P);else (void)xrtNetStreamResume(P->Stream);}
    }
    xrpcHsPeerDrop(P);
}
static void xrpcHsShardWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xrpchsshard* S=Data;xrtAtomic32Store(&S->Posted,0,XMEMORY_RELEASE);
    if(xrpcHsClosing(S->Server))for(xrpchspeer* P=S->Peers;P;P=P->Next){P->Aborting=true;xrpcHsPeerWake(P);}
    /* Only current waiters, no idle-connection scan or timer polling. Each
     * owning worker serializes re-registration after this bounded list pass. */
    while(S->Writers){xrpchspeer* P=S->Writers;xrpcHsUnwait(P);xrpcHsPeerWake(P);}
    xrpcHsDrop(S->Server);
}
static void xrpcHsOnRead(xnetstream* Stream,xnetbuf* Buffer,ptr Data)
{(void)Buffer;xrpchspeer* P=Data;if(!P->ReadEpoch)P->ReadEpoch=xrtTimer();xrtNetStreamPause(Stream);xrpcHsPeerWake(P);}
static void xrpcHsOnEnd(xnetstream* Stream,ptr Data)
{(void)Stream;xrpchspeer* P=Data;P->ReadEnded=true;xrpcHsPeerWake(P);}
static void xrpcHsOnDrain(xnetstream* Stream,ptr Data){(void)Stream;xrpcHsPeerWake(Data);}
static void xrpcHsOnLow(xnetstream* Stream,size_t Bytes,ptr Data){(void)Bytes;xrpcHsOnDrain(Stream,Data);}
static void xrpcHsOnClosed(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{(void)Stream;(void)Error;xrpchspeer* P=Data;P->Closed=true;if(Result!=XNET_RESULT_OK && !P->Aborting){xrtMutexLock(&P->Server->Gate);++P->Server->Info.TransportErrors;xrtMutexUnlock(&P->Server->Gate);}xrpcHsPeerWake(P);}
static void xrpcHsEventsDrop(const void* Data){xrpcHsPeerWake((xrpchspeer*)Data);xrpcHsPeerDrop(Data);}
static const xnetstreameventsownershipv1 xrpcHsEvents={sizeof(xnetstreameventsownershipv1),{NULL,xrpcHsOnRead,xrpcHsOnEnd,NULL,xrpcHsOnLow,xrpcHsOnDrain,xrpcHsOnClosed},xrpcHsEventsDrop,&xrpcHsPeerOps};
static bool xrpcHsAccept(xnetlistener* Listener,xnetstream* Stream,ptr Data)
{
    (void)Listener;xrpchttpserver* H=Data;size_t Fixed=sizeof(xrpchspeer)+(H->Config.Head.MaxFields+H->Config.Body.MaxTrailers)*sizeof(xhttpfield);
    size_t Bytes=Fixed+H->Config.InitialInputBytes;xrtMutexLock(&H->Gate);
    bool OK=H->Info.State==XRPC_HTTP_SERVER_RUNNING && H->Info.Connections<H->Config.MaxConnections && Bytes<=H->Config.MaxBufferBytes-H->Info.BufferBytes;
    if(OK){++H->Info.Connections;H->Info.BufferBytes+=Bytes;xrpcHsRef(H);}else ++H->Info.Rejected;xrtMutexUnlock(&H->Gate);if(!OK)return false;
    xrpchspeer* P=xrtCalloc(1,Fixed);
    if(!P){xrtMutexLock(&H->Gate);--H->Info.Connections;H->Info.BufferBytes-=Bytes;++H->Info.Rejected;xrtMutexUnlock(&H->Gate);xrpcHsDrop(H);return false;}
    xrtAtomic32Init(&P->References,1);xrtAtomic32Init(&P->Posted,0);P->Server=H;P->FixedBytes=Fixed;P->InputCapacity=H->Config.InitialInputBytes;
    P->Fields=(xhttpfield*)(P+1);P->Trailers=P->Fields+H->Config.Head.MaxFields;P->Input=xrtMalloc(P->InputCapacity);
    P->Shard=&H->Shards[xrtNetWorkerIndex(xrtNetStreamWorker(Stream))];P->Version=XHTTP_VERSION_1_1;P->Entered=xrtTimer();
    if(!P->Input || !xrtNetPostInit(&P->Post)){xrtMutexLock(&H->Gate);++H->Info.Rejected;xrtMutexUnlock(&H->Gate);xrpcHsPeerDrop(P);return false;}
    xrpcHsPeerRef(P);if(!xrtNetStreamSetEventsOwnedV1(Stream,P,&xrpcHsEvents)){xrtMutexLock(&H->Gate);++H->Info.Rejected;xrtMutexUnlock(&H->Gate);xrpcHsPeerDrop(P);xrpcHsPeerDrop(P);return false;}
    P->Stream=Stream;P->Next=P->Shard->Peers;if(P->Next)P->Next->Previous=P;P->Shard->Peers=P;P->Linked=true;
    xrtMutexLock(&H->Gate);++H->Info.Accepted;xrtMutexUnlock(&H->Gate);
    if(!xrpcHsTimerArm(P))P->Aborting=true;
    xrpcHsPeerWake(P);return true;
}
static void xrpcHsListenError(xnetlistener* Listener,const xerror* Error,ptr Data)
{(void)Listener;(void)Error;xrpchttpserver* H=Data;xrtMutexLock(&H->Gate);++H->Info.TransportErrors;xrtMutexUnlock(&H->Gate);(void)xrpcHttpServerClose(H);}
static void xrpcHsListenClosed(xnetlistener* Listener,ptr Data){(void)Listener;(void)Data;}
static const xnetlistenereventsownershipv1 xrpcHsListener={sizeof(xnetlistenereventsownershipv1),{xrpcHsAccept,xrpcHsListenError,xrpcHsListenClosed},xrpcHsDrop,&xrpcHsOps};
XRPC_API bool xrpcHttpServerConfigInit(xrpchttpserverconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcHsReport(XERR_ARGUMENT,XRPC_HTTP_SERVER_ARGUMENT,"invalid HTTP server config output");
    *C=(xrpchttpserverconfig){0};C->Size=sizeof(*C);xrtNetListenConfigInit(&C->Listen);xrtNetAddrParse(&C->Listen.Address,"127.0.0.1",0);
    xrpcServerConfigInit(&C->Execution);xrtHttp1LimitsInit(&C->Head);xrtHttp1BodyLimitsInit(&C->Body);C->Body.MaxBody=1024u*1024u;C->Path=XRT_STR_LITERAL("/rpc");
    C->MaxConnections=1024;C->MaxOperations=256;C->MaxBufferBytes=16u*1024u*1024u;C->MaxQueuedBytes=8u*1024u*1024u;C->MaxRequestWireBytes=2u*1024u*1024u;
    C->InitialInputBytes=4096;C->ReadBytes=C->WriteBytes=64u*1024u;C->WriteChunkBytes=16u*1024u;C->IdleTimeout=C->RequestTimeout=30000;return true;
}
XRPC_API bool xrpcHttpServerCreate(xnetengine* Engine,const xrpchttpserverconfig* Config,xrpchttpserver** Result)
{
    if(!Engine || !Result || !xrtMemRangeValid(Result,sizeof(*Result)) || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Result,sizeof(*Result)))))goto Argument;
    xrpchttpserverconfig C;if(Config)C=*Config;else xrpcHttpServerConfigInit(&C);
    if(C.Size!=sizeof(C) || !xrtMemRangeValid(C.Path.Data,C.Path.Size) || xrtMemRangesOverlap(C.Path.Data,C.Path.Size,Result,sizeof(*Result)) || !C.Path.Size || C.Path.Data[0]!='/' ||
        !C.MaxConnections || C.MaxConnections>1000000u || !C.MaxOperations || C.MaxOperations>1000000u || !C.MaxBufferBytes || C.MaxQueuedBytes<25u || !C.MaxRequestWireBytes ||
        !C.InitialInputBytes || !C.ReadBytes || !C.WriteBytes || !C.WriteChunkBytes || (C.IdleTimeout<=0 && C.IdleTimeout!=XRT_WAIT_FOREVER) || (C.RequestTimeout<=0 && C.RequestTimeout!=XRT_WAIT_FOREVER) ||
        !C.Head.MaxHead || !C.Head.MaxFields || !C.Head.MaxStartLine || !C.Head.MaxFieldLine || !C.Body.MaxBody || C.Body.MaxBody>SIZE_MAX || !C.Body.MaxTrailer || !C.Body.MaxTrailers || !C.Body.MaxChunkLine || !C.Body.MaxTrailerLine ||
        C.Head.MaxFields>(SIZE_MAX-sizeof(xrpchspeer))/sizeof(xhttpfield) || C.Body.MaxTrailers>(SIZE_MAX-sizeof(xrpchspeer))/sizeof(xhttpfield)-C.Head.MaxFields ||
        (C.Listen.Address.Family!=XNET_FAMILY_IPV4 && C.Listen.Address.Family!=XNET_FAMILY_IPV6) || C.Listen.Stream.WriteLimit<25u || !xrtNetStreamConfigValid(&C.Listen.Stream))goto Argument;
    xhttptarget Route;if(!xrtHttpTargetParse(XRT_STR_LITERAL("POST"),C.Path,&Route) || Route.Form!=XHTTP_TARGET_ORIGIN || (Route.Flags&XHTTP_TARGET_HAS_QUERY))goto Argument;
    size_t Limit=C.Head.MaxHead;if(Limit<C.Body.MaxTrailer)Limit=C.Body.MaxTrailer;if(Limit<C.Body.MaxChunkLine)Limit=C.Body.MaxChunkLine;
    if(Limit>SIZE_MAX-C.ReadBytes)goto Argument;
    Limit+=C.ReadBytes;if(C.InitialInputBytes>Limit)C.InitialInputBytes=Limit;
    size_t Fixed=sizeof(xrpchspeer)+(C.Head.MaxFields+C.Body.MaxTrailers)*sizeof(xhttpfield);if(Fixed>SIZE_MAX-C.InitialInputBytes)goto Argument;
    uint32 Workers=xrtNetEngineWorkerCount(Engine);size_t Base=sizeof(xrpchttpserver);
    if(!Workers || Workers>65536u || Workers>(SIZE_MAX-Base)/sizeof(xrpchsshard))goto Argument;
    Base+=(size_t)Workers*sizeof(xrpchsshard);
    if(C.Path.Size>SIZE_MAX-Base || Base+C.Path.Size>C.MaxBufferBytes)goto Argument;
    Base+=C.Path.Size;
    if(!xrtNetEnginePin(Engine))return false;
    xrpchttpserver* H=xrtCalloc(1,Base);if(!H){xrtNetEngineUnpin(Engine);return false;}
    H->Engine=Engine;H->Config=C;H->OwnedBytes=Base;H->InputLimit=Limit;H->ShardCount=Workers;xrtAtomic32Init(&H->References,1);
    H->Config.Path=(xstrview){(cstr)(H->Shards+Workers),C.Path.Size};memcpy((ptr)H->Config.Path.Data,C.Path.Data,C.Path.Size);
    if(!xrtMutexInit(&H->Gate)){xrtFree(H);xrtNetEngineUnpin(Engine);return false;}
    if(!xrtCondInit(&H->Idle)){xrtMutexUnit(&H->Gate);xrtFree(H);xrtNetEngineUnpin(Engine);return false;}
    H->Info.Size=sizeof(H->Info);H->Info.BufferBytes=Base;
    for(size_t K=0;K<Workers;++K){H->Shards[K].Server=H;H->Shards[K].Worker=xrtNetEngineWorker(Engine,(uint32)K);xrtAtomic32Init(&H->Shards[K].Posted,0);xrtNetPostInit(&H->Shards[K].Post);}
    if(!xrpcServerCreate(&C.Execution,&H->Execution)){xerror* E=xrtTakeError();xrtCondUnit(&H->Idle);xrtMutexUnit(&H->Gate);xrtFree(H);xrtNetEngineUnpin(Engine);xrtClearError();xrtSetErrorTake(E);return false;}
    *Result=H;return true;
Argument:
    return xrpcHsReport(XERR_ARGUMENT,XRPC_HTTP_SERVER_ARGUMENT,"invalid HTTP server config/span/limit");
}
XRPC_API bool xrpcHttpServerStart(xrpchttpserver* H)
{
    if(!H)return xrpcHsReport(XERR_ARGUMENT,XRPC_HTTP_SERVER_ARGUMENT,"HTTP server is NULL");
    xrtMutexLock(&H->Gate);if(H->Info.State!=XRPC_HTTP_SERVER_CREATED){xrtMutexUnlock(&H->Gate);return xrpcHsReport(XERR_STATE,XRPC_HTTP_SERVER_STATE,"HTTP server already started/closed");}
    xrpcHsRef(H);H->Listener=xrtNetListenOwnedV1(H->Engine,&H->Config.Listen,H,&xrpcHsListener);
    if(!H->Listener){xrtAtomic32FetchSub(&H->References,1,XMEMORY_RELAXED);xrtMutexUnlock(&H->Gate);return false;}
    bool OK=xrtNetListenerLocal(H->Listener,&H->Local);H->Info.State=XRPC_HTTP_SERVER_RUNNING;xrtMutexUnlock(&H->Gate);
    if(!OK)(void)xrpcHttpServerClose(H);
    return OK;
}
XRPC_API bool xrpcHttpServerLocal(xrpchttpserver* H,xnetaddr* Address)
{
    if(!H || !Address || !xrtMemRangeValid(Address,sizeof(*Address)) || xrtMemRangesOverlap(Address,sizeof(*Address),H,H->OwnedBytes))return xrpcHsReport(XERR_ARGUMENT,XRPC_HTTP_SERVER_ARGUMENT,"invalid HTTP local-address output");
    xrtMutexLock(&H->Gate);bool OK=H->Listener!=NULL;if(OK)*Address=H->Local;xrtMutexUnlock(&H->Gate);return OK || xrpcHsReport(XERR_STATE,XRPC_HTTP_SERVER_STATE,"HTTP server not bound");
}
XRPC_API bool xrpcHttpServerInfo(xrpchttpserver* H,xrpchttpserverinfo* Info)
{
    if(!H || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(Info,sizeof(*Info),H,H->OwnedBytes))return xrpcHsReport(XERR_ARGUMENT,XRPC_HTTP_SERVER_ARGUMENT,"invalid HTTP server info output");
    xrpcserverinfo E={0};E.Size=sizeof(E);if(!xrpcServerInfo(H->Execution,&E))return false;
    xrtMutexLock(&H->Gate);*Info=H->Info;Info->Execution=E;xrtMutexUnlock(&H->Gate);return true;
}
XRPC_API bool xrpcHttpServerClose(xrpchttpserver* H)
{
    if(!H)return xrpcHsReport(XERR_ARGUMENT,XRPC_HTTP_SERVER_ARGUMENT,"HTTP server is NULL");
    xrtMutexLock(&H->Gate);if(H->Info.State<XRPC_HTTP_SERVER_CLOSING)H->Info.State=XRPC_HTTP_SERVER_CLOSING;xnetlistener* L=H->Listener;xrtMutexUnlock(&H->Gate);
    bool OK=!L || xrtNetListenerClose(L);for(size_t K=0;K<H->ShardCount;++K)xrpcHsShardWake(&H->Shards[K]);
    bool Cancel=xrpcServerCancel(H->Execution);return OK && Cancel;
}
XRPC_API xwaitresult xrpcHttpServerWaitFor(xrpchttpserver* H,int64 Timeout)
{
    double Entered=xrtTimer();if(!H || Timeout<XRT_WAIT_FOREVER){xrpcHsReport(XERR_ARGUMENT,XRPC_HTTP_SERVER_ARGUMENT,"invalid HTTP server wait");return XWAIT_ERROR;}
    if(xrtNetEngineCurrent(H->Engine)){xrpcHsReport(XERR_STATE,XRPC_HTTP_SERVER_STATE,"cannot wait on a network worker");return XWAIT_ERROR;}
    double End=Timeout==0?Entered:xrpcHsDeadline(Entered,Timeout);if(!xrpcHttpServerClose(H))return XWAIT_ERROR;
    xwaitresult R=xrpcServerWaitFor(H->Execution,0);if(R==XWAIT_ERROR)return R;
    xrtMutexLock(&H->Gate);
    while(xrtAtomic32Load(&H->References,XMEMORY_ACQUIRE)!=1){int64 Left=xrpcHsRemaining(End);if(!Left){R=XWAIT_TIMEOUT;break;}
        R=xrtCondWaitFor(&H->Idle,&H->Gate,Left);if(R==XWAIT_ERROR)break;if(xrtAtomic32Load(&H->References,XMEMORY_ACQUIRE)==1){R=XWAIT_OK;break;}if(R==XWAIT_TIMEOUT)break;}
    bool Ready=xrtAtomic32Load(&H->References,XMEMORY_ACQUIRE)==1;xrtMutexUnlock(&H->Gate);if(!Ready)return R;
    R=xrpcServerWaitFor(H->Execution,xrpcHsRemaining(End));if(R==XWAIT_OK){xrtMutexLock(&H->Gate);H->Info.State=XRPC_HTTP_SERVER_CLOSED;xrtMutexUnlock(&H->Gate);}return R;
}
XRPC_API bool xrpcHttpServerDestroy(xrpchttpserver* H)
{
    if(!H)return true;
    if(xrpcHttpServerWaitFor(H,XRT_WAIT_FOREVER)!=XWAIT_OK || !xrpcServerDestroy(H->Execution))return false;
    xrtNetListenerDestroy(H->Listener);xrtNetEngineUnpin(H->Engine);xrtCondUnit(&H->Idle);xrtMutexUnit(&H->Gate);xrtFree(H);return true;
}
#endif
