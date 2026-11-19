#include "test.h"
#include "network_extensions.h"
#include <xrpc/ws.h>
#include <xrt/memory_stats.h>
#include "../examples/common/ws_service.h"
static void wsResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{const xrpccallresult* R=rpcWsServiceResult(F);if(R->Status!=Status||R->Delivery!=Delivery)fprintf(stderr,"got %d/%d expected %d/%d\n",R->Status,R->Delivery,Status,Delivery);testRequire(R->Status==Status&&R->Delivery==Delivery,"WS typed status/delivery");}
static void wsRoundtrip(bool TLS)
{
    rpcWsService S;rpcWsServiceStart(&S,TLS,NULL,false);xfuture* F[12];for(unsigned N=0;N<8;++N)testRequire(xrpcWsCall(N&1?S.Accepted:S.Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),5000,F+N)==XRPC_DUPLEX_ACCEPTED,"bidirectional queued WS calls");
    for(unsigned N=0;N<8;++N){wsResult(F[N],XRPC_CALL_OK,XRPC_REPLIED);const xrpccallresult* R=rpcWsServiceResult(F[N]);xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(xrpcResponsesInfo(R->Responses,&I)&&R->Index<I.Count&&I.Messages[R->Index].Result.Raw.Size==3&&!memcmp(I.Messages[R->Index].Result.Raw.Data,"[7]",3),"exact WS bytes");}
    testRequire(xrpcWsNotify(S.Client,XRT_STR_LITERAL("note"),(xstrview){0},5000,F+8)==XRPC_DUPLEX_ACCEPTED,"WS notification");wsResult(F[8],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
    xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{true,XRT_STR_LITERAL("note"),{0}}};testRequire(xrpcWsBatch(S.Client,Items,2,5000,F+9)==XRPC_DUPLEX_ACCEPTED,"WS batch");wsResult(F[9],XRPC_CALL_BATCH_DONE,XRPC_REPLIED);
    xrpcwsinfo Info={0};Info.Size=sizeof(Info);testRequire(xrpcWsInfo(S.Client,&Info)&&Info.State==XRPC_WS_OPEN&&Info.ProtocolBytes==7&&!memcmp(Info.Protocol,"jsonrpc",7)&&Info.Encrypted==TLS&&(!TLS||Info.Version)&&Info.ReadMessages&&Info.WriteMessages&&Info.PeakReceiveBytes<=S.ClientConfig.MaxReceiveBytes,"WS native metadata/ledger");
    rpcWsServiceStop(&S);wsResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);for(unsigned N=0;N<10;++N)xrtFutureDestroy(F[N]);
}
static void wsExtensions(bool TLS)
{
    xrpcwsconfig Config;
    xrpcWsConfigInit(&Config);
    Config.Duplex.IncomingTimeout=RPC_TEST_REMOTE_LIMIT;
    rpcWsService S;
    rpcWsServiceStart(&S,TLS,&Config,true);
    testRequire(xrpcWsEnableExtensions(S.Client,RPC_TEST_EXTENSIONS)==XRPC_DUPLEX_ACCEPTED &&
        xrpcWsEnableExtensions(S.Accepted,RPC_TEST_EXTENSIONS)==XRPC_DUPLEX_ACCEPTED,
        "opt in with native WS and WSS reference sends");
    xfuture* F=NULL;
    testRequire(xrpcWsNegotiateExtension(S.Client,XRPC_DUPLEX_EXTENSION_CANCELLATION,5000,&F)==
        XRPC_DUPLEX_ACCEPTED,"negotiate WS cancellation");
    rpcTestNetworkOfferResult(F,true);
    testRequire(xrpcWsNegotiateExtension(S.Client,XRPC_DUPLEX_EXTENSION_DEADLINE,5000,&F)==
        XRPC_DUPLEX_ACCEPTED,"negotiate WS deadline");
    rpcTestNetworkOfferResult(F,true);

    xrpcwsinfo Info={.Size=sizeof(Info)};
    testRequire(xrpcWsInfo(S.Accepted,&Info),"WS receiver before sending the job");
    uint64 Read=Info.ReadMessages,Written=Info.WriteMessages;
    testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("held"),(xstrview){0},5000,&F)==
        XRPC_DUPLEX_ACCEPTED,"send WS request before remote cancellation");
    rpcWsServiceAwait(&S.HandlerEntered);
    testRequire(xrtFutureCancel(F),"cancel a fully sent WS request");
    wsResult(F,XRPC_CALL_CANCELLED,XRPC_MAY_HAVE_EXECUTED);
    double End=xrtTimer()+8.;
    do {
        testRequire(xrpcWsInfo(S.Accepted,&Info) && xrtTimer()<End,"observe WS abandonment message");
        xrtSleep(1);
    } while(Info.ReadMessages<Read+2);
    testRequire(Info.ReadMessages==Read+2 && Info.State==XRPC_WS_OPEN,
        "only cancellation control reached the running WS document");
    xrtAtomic32Store(&S.HandlerPermit,1,XMEMORY_RELEASE);
    do {
        testRequire(xrpcWsInfo(S.Accepted,&Info) && xrtTimer()<End,"observe physical WS handler retirement");
        xrtSleep(1);
    } while(Info.Duplex.Incoming);
    testRequire(Info.State==XRPC_WS_OPEN && Info.WriteMessages==Written,"WS peer suppresses abandoned result");
    xrtFutureDestroy(F);

    xrtAtomic32Store(&S.HandlerEntered,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&S.HandlerPermit,0,XMEMORY_RELEASE);
    testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("held"),(xstrview){0},5000,&F)==
        XRPC_DUPLEX_ACCEPTED,"admit second WS request with deadline");
    rpcWsServiceAwait(&S.HandlerEntered);
    double Release=xrtTimer()+RPC_TEST_REMOTE_LIMIT*.001+.01;
    while(xrtTimer()<Release)xrtSleep(1);
    xrtAtomic32Store(&S.HandlerPermit,1,XMEMORY_RELEASE);
    rpcTestNetworkDeadlineResult(F);
    testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),5000,&F)==
        XRPC_DUPLEX_ACCEPTED,"reuse WS connection after expiry");
    wsResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    xrtFutureDestroy(F);
    rpcWsServiceStop(&S);
}

static void wsCancellation(bool TLS)
{
    for(unsigned Mode=0;Mode<3;++Mode){rpcWsService S;rpcWsServiceStart(&S,TLS,NULL,true);xfuture* F=NULL;testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("held"),XRT_STR_LITERAL("[]"),Mode==0?50:5000,&F)==XRPC_DUPLEX_ACCEPTED,"held WS call");rpcWsServiceAwait(&S.HandlerEntered);
        if(Mode==0)wsResult(F,XRPC_CALL_TIMEOUT,XRPC_MAY_HAVE_EXECUTED);
        else if(Mode==1){testRequire(xrtFutureCancel(F),"cancel WS call");wsResult(F,XRPC_CALL_CANCELLED,XRPC_MAY_HAVE_EXECUTED);}
        else {testRequire(xrpcWsClose(S.Accepted)&&xrpcWsWaitFor(S.Accepted,20)==XWAIT_TIMEOUT,"WS close waits shared handler physical tail");}
        xrtAtomic32Store(&S.HandlerPermit,1,XMEMORY_RELEASE);if(Mode==2)wsResult(F,XRPC_CALL_TRANSPORT,XRPC_MAY_HAVE_EXECUTED);
        rpcWsServiceStop(&S);xrtFutureDestroy(F);
    }
}
#include "ws_raw.h"
typedef struct wsProducer {xrpcws* Peer;unsigned Index;} wsProducer;
static int32 wsProduce(ptr Data)
{
    wsProducer* P=Data;for(unsigned K=0;K<40;++K){char Text[40];int N=snprintf(Text,sizeof(Text),"[%u,%u]",P->Index,K);xfuture* F=NULL;testRequire(xrpcWsCall(P->Peer,XRT_STR_LITERAL("echo"),(xstrview){Text,(size_t)N},5000,&F)==XRPC_DUPLEX_ACCEPTED,"WS concurrent admission");if(K%7==0)(void)xrtFutureCancel(F);const xrpccallresult* R=rpcWsServiceResult(F);testRequire(R->Status==XRPC_CALL_OK||R->Status==XRPC_CALL_CANCELLED,"WS producer terminal result");if(R->Status==XRPC_CALL_OK){xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(xrpcResponsesInfo(R->Responses,&I)&&I.Messages[R->Index].Result.Raw.Size==(size_t)N&&!memcmp(I.Messages[R->Index].Result.Raw.Data,Text,(size_t)N),"WS concurrent exact correlation");}xrtFutureDestroy(F);}return 0;
}
typedef struct wsOwnerGate {xnetpost Post;xatomic32 Ready,Permit,Left;} wsOwnerGate;
static void wsGate(xnetworker* W,ptr Data)
{(void)W;wsOwnerGate* G=Data;xrtAtomic32Store(&G->Ready,1,XMEMORY_RELEASE);rpcWsServiceAwait(&G->Permit);xrtAtomic32Store(&G->Left,1,XMEMORY_RELEASE);}
static void wsConcurrency(bool TLS)
{
    rpcWsService S;rpcWsServiceStart(&S,TLS,NULL,false);wsProducer P[4];xthread* Workers[4];for(unsigned K=0;K<4;++K){P[K]=(wsProducer){K&1?S.Accepted:S.Client,K};Workers[K]=xrtThreadCreate(wsProduce,P+K,0);testRequire(Workers[K]!=NULL,"WS producer start");}for(unsigned K=0;K<4;++K){testRequire(xrtThreadWaitFor(Workers[K],15000)==XWAIT_OK,"WS producer join");xrtThreadDestroy(Workers[K]);}
    wsOwnerGate G={0};xrtAtomic32Init(&G.Ready,0);xrtAtomic32Init(&G.Permit,0);xrtAtomic32Init(&G.Left,0);testRequire(xrtNetPostInit(&G.Post)&&xrtNetPost(xrtNetEngineWorker(S.Engine,1),&G.Post,wsGate,&G),"WS owner gate");rpcWsServiceAwait(&G.Ready);
    xfuture *Timeout=NULL,*Cancel=NULL;testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),(xstrview){0},0,&Timeout)==XRPC_DUPLEX_ACCEPTED&&xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&Cancel)==XRPC_DUPLEX_ACCEPTED&&xrtFutureCancel(Cancel),"WS queued original-entry deadline/cancel");xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);rpcWsServiceAwait(&G.Left);wsResult(Timeout,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);wsResult(Cancel,XRPC_CALL_CANCELLED,XRPC_NOT_SENT);rpcWsServiceStop(&S);xrtFutureDestroy(Timeout);xrtFutureDestroy(Cancel);
}
typedef struct wsTimerFill {xnetpost Post;xatomic32 Ready;} wsTimerFill;
static void wsOccupied(xnetworker* W,uint64 Id,xnetresult Result,ptr Data)
{(void)W;(void)Id;(void)Result;(void)Data;}
static void wsFillTimers(xnetworker* W,ptr Data)
{
    wsTimerFill* T=Data;unsigned Count=0;while(xrtNetEngineAfter(xrtNetWorkerEngine(W),xrtNetWorkerIndex(W),60000,wsOccupied,NULL))++Count;
    testRequire(Count&&Count<=4,"actual owner timer quota occupied");xrtClearError();xrtAtomic32Store(&T->Ready,1,XMEMORY_RELEASE);
}
static void wsTimerQuota(bool TLS)
{
    rpcWsService S;rpcWsServiceStartEx(&S,TLS,NULL,true,4);wsTimerFill T;memset(&T,0,sizeof(T));xrtAtomic32Init(&T.Ready,0);testRequire(xrtNetPostInit(&T.Post)&&xrtNetPost(xrtNetEngineWorker(S.Engine,1),&T.Post,wsFillTimers,&T),"timer quota owner barrier");rpcWsServiceAwait(&T.Ready);
    xfuture* F=NULL;testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("held"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"timer exhaustion call");
    double End=xrtTimer()+5;xrpcwsinfo I={0};I.Size=sizeof(I);do{testRequire(xrpcWsInfo(S.Client,&I)&&xrtTimer()<End,"WS timer exhaustion observed");xrtSleep(1);}while(!I.TimerFailures);
    testRequire(I.Reason==XRPC_WS_LIMIT,"timer quota typed reason");xrtAtomic32Store(&S.HandlerPermit,1,XMEMORY_RELEASE);rpcWsServiceStop(&S);testRequire(rpcWsServiceResult(F)->Status==XRPC_CALL_LIMIT,"timer failure terminal result");xrtFutureDestroy(F);
}
int main(void)
{
    setbuf(stdout,NULL);puts("[case] WS basic");wsRoundtrip(false);puts("[case] WS extensions");wsExtensions(false);puts("[case] WS cancel");wsCancellation(false);puts("[case] WS concurrent");wsConcurrency(false);puts("[case] WS timer quota");wsTimerQuota(false);
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    puts("[case] WSS basic");wsRoundtrip(true);puts("[case] WSS extensions");wsExtensions(true);puts("[case] WSS cancel");wsCancellation(true);puts("[case] WSS concurrent");wsConcurrency(true);puts("[case] WSS timer quota");wsTimerQuota(true);
#endif
    wsFaults();puts("[PASS] WS bidirectional calls/batch/notify/retained results/reference sends/metadata/physical close/call timeout+cancel/raw fragmentation+ping/UTF8+mask+opcode+binary faults/message+growth peak bounds/assembly timeout/final response before EOF/constructor OOM+rollback/Initial transfer/native alias/negotiated-network-extensions");return 0;
}
