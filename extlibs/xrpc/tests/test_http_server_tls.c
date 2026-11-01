#include "test.h"
#include "support/http_tls_service.h"
#include <xrt/memory_stats.h>
static void serviceTlsTraffic(void)
{
    rpcTlsService T;xrpchttpserverconfig H;xrpcHttpServerConfigInit(&H);H.WriteChunkBytes=3;H.MaxQueuedBytes=64;rpcTlsServiceStart(&T,&H,NULL,true);
    xrpchttpclient* C=rpcTlsServiceClient(&T,NULL);xfuture* F[16];
    for(unsigned N=0;N<16;++N)F[N]=rpcServiceSubmit(C,"echo","[7]",10000);
    for(unsigned N=0;N<16;++N){const xrpccallresult* R=rpcServiceResult(F[N]);xrpcresponseinfo I={0};I.Size=sizeof(I);
        testRequire(R->Status==XRPC_CALL_OK && R->Delivery==XRPC_REPLIED && xrpcResponsesInfo(R->Responses,&I) && I.Messages[0].Result.Raw.Size==3 && !memcmp(I.Messages[0].Result.Raw.Data,"[7]",3),"TLS service partial-credit ordinary reply");if(N)xrtFutureDestroy(F[N]);}
    xfuture* F0=NULL;testRequire(xrpcHttpNotifyAsync(C,XRT_STR_LITERAL("echo"),(xstrview){0},10000,&F0)==XRPC_ADMISSION_ACCEPTED && rpcServiceResult(F0)->Status==XRPC_CALL_NOTIFY_DONE,"TLS service notification");xrtFutureDestroy(F0);
    xrpchttpitem Items[]={{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("echo"),{0}},{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("error"),{0}}};
    testRequire(xrpcHttpBatchAsync(C,Items,3,10000,&F0)==XRPC_ADMISSION_ACCEPTED && rpcServiceResult(F0)->Status==XRPC_CALL_BATCH_DONE,"TLS service mixed batch");xrtFutureDestroy(F0);
    rpcServiceClientClose(C);rpcTlsServiceStop(&T,true);testRequire(rpcServiceResult(F[0])->Status==XRPC_CALL_OK,"TLS server result survives complete engine cleanup");xrtFutureDestroy(F[0]);
}
static void tlsWait(xatomic32* Value,uint32 Minimum)
{double End=xrtTimer()+10;while(xrtAtomic32Load(Value,XMEMORY_ACQUIRE)<Minimum){testRequire(xrtTimer()<End,"TLS service event deadline");xrtSleep(1);}}
static xrpchttpservertlsinfo tlsInfo(rpcTlsService* T)
{xrpchttpservertlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcHttpServerTlsInfo(T->Base.Server,&I),"TLS info");return I;}
static void tlsUnexpected(xnetresult Result,const xerror* Cause,ptr Data)
{fprintf(stderr,"unexpected TLS handshake failure: %d %s\n",xrtErrorCode(Cause),xrtErrorMessage(Cause));rpcTlsServiceError(Result,Cause,Data);}
static void serviceTlsCreditAndSnapshot(void)
{
    rpcTlsService T;xrpchttpserverconfig H;xrpcHttpServerConfigInit(&H);H.MaxQueuedBytes=128;H.WriteChunkBytes=16384;
    H.Listen.Stream.WriteHighWater=16384;H.Listen.Stream.WriteLowWater=8192;
    xrpchttpservertlsconfig K;xrpcHttpServerTlsConfigInit(&K);char Text[]="http/1.1";xstrview ALPN={Text,8};K.Session.Protocols=&ALPN;K.HandshakeError=tlsUnexpected;K.Context=&T;
    rpcTlsServiceCreate(&T,&H,&K,true);memset(Text,'x',8);memset(&K,0,sizeof(K));
    xrtTlsIdentityRelease(T.Identity);T.OwnIdentity=false;T.Identity=NULL;xrtTlsContextRelease(T.Context);T.OwnContext=false;
    rpcTlsServiceListen(&T);xrpchttpclient* C[2];xfuture* F[4];char Params[32773];
    memcpy(Params,"[\"",2);memset(Params+2,'a',32768);memcpy(Params+32770,"\"]",3);
    for(unsigned N=0;N<2;++N)C[N]=rpcTlsServiceClient(&T,NULL);
    for(unsigned N=0;N<4;++N){xrpcadmission A=xrpcHttpCallAsync(C[N%2],XRT_STR_LITERAL("echo"),(xstrview){Params,strlen(Params)},10000,&F[N]);
        if(A!=XRPC_ADMISSION_ACCEPTED){xrpchttpinfo I={0};I.Size=sizeof(I);testRequire(xrpcHttpInfo(C[N%2],&I),"credit client info");const xrpccallresult* V=rpcServiceResult(F[N%2]);fprintf(stderr,"credit submit=%u admission=%d state=%d close=%d cause=%d %s\n",N,(int)A,(int)I.State,(int)I.CloseStatus,xrtErrorCode(V->Cause),xrtErrorMessage(V->Cause));}
        testRequire(A==XRPC_ADMISSION_ACCEPTED,"credit client admission");}
    for(unsigned N=0;N<4;++N){const xrpccallresult* R=rpcServiceResult(F[N]);xrpcresponseinfo I={0};I.Size=sizeof(I);
        testRequire(R->Status==XRPC_CALL_OK && xrpcResponsesInfo(R->Responses,&I) && I.Messages[0].Result.Raw.Size==32772 && !memcmp(I.Messages[0].Result.Raw.Data,Params,32772),"TLS shared credit and copied metadata");
        xrpchttpserverinfo S={0};S.Size=sizeof(S);testRequire(xrpcHttpServerInfo(T.Base.Server,&S) && S.QueuedBytes<=H.MaxQueuedBytes,"TLS wire-credit cap");xrtFutureDestroy(F[N]);}
    for(unsigned N=0;N<2;++N)rpcServiceClientClose(C[N]);
    rpcTlsServiceStop(&T,true);
    rpcTlsServiceCreate(&T,NULL,NULL,true);testRequire(!tlsInfo(&T).Admitted,"create admitted handshake");rpcTlsServiceStop(&T,true);
}
typedef struct tlsHold {xnetstream* Stream;xatomic32 Open,Closed,Received;} tlsHold;
static void tlsHoldOpen(xnetstream* S,ptr Data){(void)S;xrtAtomic32Store(&((tlsHold*)Data)->Open,1,XMEMORY_RELEASE);}
static void tlsHoldRead(xnetstream* S,xnetbuf* B,ptr Data)
{(void)S;size_t N=xrtNetBufSize(B);testRequire(xrtNetBufConsume(B,N),"hold consume");xrtAtomic32FetchAdd(&((tlsHold*)Data)->Received,(uint32)N,XMEMORY_RELEASE);}
static void tlsHoldEnd(xnetstream* S,ptr Data){(void)Data;(void)xrtNetStreamAbort(S);}
static void tlsHoldClosed(xnetstream* S,xnetresult Result,const xerror* Error,ptr Data)
{(void)S;(void)Result;(void)Error;xrtAtomic32Store(&((tlsHold*)Data)->Closed,1,XMEMORY_RELEASE);}
static void tlsHoldConnect(rpcTlsService* T,tlsHold* R)
{
    memset(R,0,sizeof(*R));xrtAtomic32Init(&R->Open,0);xrtAtomic32Init(&R->Closed,0);xrtAtomic32Init(&R->Received,0);
    static const xnetstreamevents Events={tlsHoldOpen,tlsHoldRead,tlsHoldEnd,NULL,NULL,NULL,tlsHoldClosed};
    R->Stream=xrtNetStreamConnect(T->Base.Engine,&T->Base.Address,2,NULL,&Events,R);testRequire(R->Stream!=NULL,"raw handshake hold connect");
    double End=xrtTimer()+5;while(!xrtAtomic32Load(&R->Open,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&R->Closed,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"hold TCP admission");xrtSleep(1);}
}
static void tlsHoldDestroy(tlsHold* R)
{(void)xrtNetStreamAbort(R->Stream);tlsWait(&R->Closed,1);xrtNetStreamDestroy(R->Stream);}
static void serviceTlsAdmission(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){rpcTlsService T;xrpchttpserverconfig H;xrpcHttpServerConfigInit(&H);xrpchttpservertlsconfig K;xrpcHttpServerTlsConfigInit(&K);
        K.MaxHandshakes=Mode==0?1:4;H.MaxConnections=Mode==1?1:4;K.Stream.HandshakeTimeout=Mode>=2?100:5000;
        rpcTlsServiceStart(&T,&H,&K,true);tlsHold A;tlsHoldConnect(&T,&A);
        double End=xrtTimer()+5;while(tlsInfo(&T).ActiveHandshakes!=1){testRequire(xrtTimer()<End,"handshake was not counted before TLS input");xrtSleep(1);}
        if(Mode<2){tlsHold B;tlsHoldConnect(&T,&B);tlsWait(&B.Closed,1);xrpchttpservertlsinfo I=tlsInfo(&T);
            testRequire(I.Admitted==1 && I.ActiveHandshakes==1 && I.Rejected>=1 && I.PeakHandshakes==1,"pre-session handshake/connection cap");tlsHoldDestroy(&B);}
        else if(Mode==3){xrtAtomic32Store(&T.CloseOnError,1,XMEMORY_RELEASE);cstr Bad="POST /rpc HTTP/1.1\r\nHost: localhost\r\n\r\n";testRequire(xrtNetStreamSend(A.Stream,Bad,strlen(Bad))==XNET_RESULT_OK,"plain text to required TLS");}
        else if(Mode==4)testRequire(xrpcHttpServerClose(T.Base.Server),"close admitted handshake");
        if(Mode>=2){tlsWait(&A.Closed,1);tlsWait(&T.Errors,1);testRequire(tlsInfo(&T).Failed==1 && !xrtAtomic32Load(&T.Base.Calls,XMEMORY_ACQUIRE),"failed handshake reached handler");}
        tlsHoldDestroy(&A);rpcTlsServiceStop(&T,true);
    }
}
static void serviceTlsRejectAndJoin(void)
{
    rpcTlsService T;rpcTlsServiceStart(&T,NULL,NULL,true);xrtAtomic32Store(&T.Reject,1,XMEMORY_RELEASE);
    xrpchttpclient* C=rpcTlsServiceClient(&T,NULL);xfuture* F=rpcServiceSubmit(C,"echo",NULL,5000);const xrpccallresult* R=rpcServiceResult(F);
    testRequire(R->Status==XRPC_CALL_TRANSPORT && R->Delivery==XRPC_NOT_SENT && R->Cause && !xrtAtomic32Load(&T.Base.Calls,XMEMORY_ACQUIRE),"trust rejection leaked plaintext");
    rpcServiceClientClose(C);rpcTlsServiceStop(&T,true);xrtFutureDestroy(F);
    rpcTlsServiceStart(&T,NULL,NULL,true);C=rpcTlsServiceClient(&T,NULL);F=rpcServiceSubmit(C,"stubborn",NULL,5000);tlsWait(&T.Base.Entered,1);
    testRequire(xrpcHttpServerClose(T.Base.Server) && xrpcHttpServerWaitFor(T.Base.Server,20)==XWAIT_TIMEOUT,"TLS noncooperative handler lost owner");
    xrtAtomic32Store(&T.Base.Permit,1,XMEMORY_RELEASE);testRequire(rpcServiceResult(F)->Status!=XRPC_CALL_OK,"TLS shutdown fabricated success");
    rpcServiceClientClose(C);rpcTlsServiceStop(&T,true);xrtFutureDestroy(F);
}
static void serviceTlsFactory(void)
{
    rpcTlsService T;rpcTlsServiceCreate(&T,NULL,NULL,true);xrpchttpservertlsconfig K;
    for(unsigned Mode=0;Mode<8;++Mode){xrpcHttpServerTlsConfigInit(&K);K.Session.Identity=T.Identity;K.Session.Context=T.Context;xstrview H2={"h2",2};
        if(Mode==0)K.Size=0;else if(Mode==1)K.MaxHandshakes=0;else if(Mode==2)K.MaxMetadataBytes=1;else if(Mode==3)K.Session.Identity=NULL;
        else if(Mode==4)K.Session.Protocols=&H2;else if(Mode==5)K.Stream.AsyncBytesLimit=0;else if(Mode==6)K.Stream.HandshakeTimeout=-1;else K.MaxMetadataBytes=1048577;
        xrpchttpserver* H=(xrpchttpserver*)(uintptr_t)1;testRequire(!xrpcHttpServerTlsCreate(T.Base.Engine,NULL,&K,&H) && H==(xrpchttpserver*)(uintptr_t)1,"TLS invalid policy modified output");xrtClearError();}
    const size_t Limits[]={1,64,1024,8192,65536,131072,262144,1048576,2097152};unsigned Passed=0,Failed=0;
    xrpcHttpServerTlsConfigInit(&K);K.Session.Context=T.Context;K.Session.Identity=T.Identity;
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"TLS factory budget enter");
        xrpchttpserver* H=NULL;bool OK=xrpcHttpServerTlsCreate(T.Base.Engine,NULL,&K,&H);testRequire(xrtAllocationBudgetLeave(&Scope),"TLS factory budget leave");
        if(OK){++Passed;testRequire(xrpcHttpServerDestroy(H),"TLS factory quota destroy");}else{++Failed;testRequire(!H,"TLS failed factory output");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"TLS factory owner/pin leak");xrtAllocationBudgetFree(B);}
    testRequire(Passed && Failed,"TLS factory threshold coverage");rpcTlsServiceStop(&T,true);
}
typedef struct tlsRaw tlsRaw;
typedef struct tlsWrite {tlsRaw* Raw;const unsigned char* Data;size_t Size,Used;xnetpost Post;xatomic32 Done;bool Failed;} tlsWrite;
struct tlsRaw {xtlsstream* Stream;xnetengine* Engine;xmutex Gate;xatomic32 Open,Closed;unsigned char Data[65536];size_t Used;tlsWrite* Write;};
static void tlsRawPump(xtlsstream* Stream,ptr Data)
{
    tlsRaw* R=Data;tlsWrite* W=R->Write;if(!W)return;
    while(W->Used<W->Size){size_t N=0;xtlsresult S=xrtTlsStreamSend(Stream,W->Data+W->Used,W->Size-W->Used,&N);W->Used+=N;
        if(S!=XTLS_OK && S!=XTLS_AGAIN){W->Failed=true;break;}if(!N)return;}
    R->Write=NULL;xrtAtomic32Store(&W->Done,1,XMEMORY_RELEASE);
}
static void tlsRawWrite(xnetworker* Worker,ptr Data)
{(void)Worker;tlsWrite* W=Data;W->Raw->Write=W;tlsRawPump(W->Raw->Stream,W->Raw);}
static void tlsRawOpen(xtlsstream* Stream,ptr Data){(void)Stream;xrtAtomic32Store(&((tlsRaw*)Data)->Open,1,XMEMORY_RELEASE);}
static void tlsRawRead(xtlsstream* Stream,const xnetbuf* Buffer,ptr Data)
{
    (void)Buffer;tlsRaw* R=Data;xrtMutexLock(&R->Gate);size_t N=0;testRequire(R->Used<sizeof(R->Data) && xrtTlsStreamRead(Stream,R->Data+R->Used,sizeof(R->Data)-R->Used,&N)==XTLS_OK,"raw TLS read");R->Used+=N;xrtMutexUnlock(&R->Gate);
}
static void tlsRawEnd(xtlsstream* Stream,ptr Data){(void)Data;(void)xrtTlsStreamAbort(Stream);}
static void tlsRawClosed(xtlsstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{
    (void)Stream;(void)Result;(void)Error;tlsRaw* R=Data;tlsWrite* W=R->Write;R->Write=NULL;
    if(W){W->Failed=true;xrtAtomic32Store(&W->Done,1,XMEMORY_RELEASE);}xrtAtomic32Store(&R->Closed,1,XMEMORY_RELEASE);
}
static void tlsRawConnect(rpcTlsService* T,tlsRaw* R,bool NoALPN)
{
    memset(R,0,sizeof(*R));R->Engine=T->Base.Engine;xrtAtomic32Init(&R->Open,0);xrtAtomic32Init(&R->Closed,0);testRequire(xrtMutexInit(&R->Gate),"raw TLS mutex");
    xtlsclientconfig K;xrtTlsClientConfigInit(&K);K.Context=T->Context;K.Verifier=T->Verifier;K.VerifyName=(xstrview)XRT_STR_LITERAL("rpc.fixture");
    xstrview ALPN=XRT_STR_LITERAL("http/1.1");K.Protocols=NoALPN?NULL:&ALPN;K.ProtocolCount=NoALPN?0:1;
    static const xtlsstreamevents Events={tlsRawOpen,tlsRawRead,tlsRawEnd,tlsRawPump,tlsRawPump,tlsRawClosed,NULL};
    R->Stream=xrtTlsStreamConnect(R->Engine,&T->Base.Address,2,NULL,&K,NULL,&Events,R);testRequire(R->Stream!=NULL,"raw TLS connect");
    double End=xrtTimer()+10;while(!xrtAtomic32Load(&R->Open,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&R->Closed,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"raw TLS admission deadline");xrtSleep(1);}
}
static void tlsRawSend(tlsRaw* R,const void* Data,size_t N)
{
    tlsWrite W={0};W.Raw=R;W.Data=Data;W.Size=N;xrtAtomic32Init(&W.Done,0);
    testRequire(xrtNetPostInit(&W.Post) && xrtNetPost(xrtNetEngineWorker(R->Engine,2),&W.Post,tlsRawWrite,&W),"raw TLS write post");tlsWait(&W.Done,1);testRequire(!W.Failed,"raw TLS write failed");
}
static bool tlsRawResponse(tlsRaw* R,unsigned* Status,char* Body,size_t Capacity,size_t* Bytes)
{
    double End=xrtTimer()+10;
    for(;;){xrtMutexLock(&R->Gate);xhttpfield Fields[32];xhttp1head H;xrtHttp1HeadInit(&H,Fields,32);
        xhttp1status S=xrtHttp1ResponseParse((xbytesview){R->Data,R->Used},&H,NULL,NULL);testRequire(S==XHTTP1_MORE || S==XHTTP1_READY,"TLS response framing");
        if(S==XHTTP1_READY){xhttp1bodyplan P;testRequire(xrtHttp1ResponseBodyPlan(&H,XRT_STR_LITERAL("POST"),&P) && (P.Mode==XHTTP1_BODY_NONE || P.Mode==XHTTP1_BODY_FIXED),"TLS response body plan");
            size_t N=P.Mode==XHTTP1_BODY_NONE?0:(size_t)P.Length;if(N<=R->Used-H.Bytes){testRequire(N<=Capacity,"TLS response capacity");if(N)memcpy(Body,R->Data+H.Bytes,N);*Bytes=N;*Status=H.Status;
                size_t Used=H.Bytes+N;memmove(R->Data,R->Data+Used,R->Used-Used);R->Used-=Used;xrtMutexUnlock(&R->Gate);return true;}}
        bool Closed=xrtAtomic32Load(&R->Closed,XMEMORY_ACQUIRE)!=0;xrtMutexUnlock(&R->Gate);if(Closed)return false;testRequire(xrtTimer()<End,"TLS response timeout");xrtSleep(1);}
}
static void tlsRawDestroy(tlsRaw* R)
{(void)xrtTlsStreamAbort(R->Stream);tlsWait(&R->Closed,1);xrtTlsStreamDestroy(R->Stream);xrtMutexUnit(&R->Gate);}
static void serviceTlsRawProtocol(void)
{
    rpcTlsService T;xrpchttpserverconfig H;xrpcHttpServerConfigInit(&H);H.MaxQueuedBytes=32;rpcTlsServiceStart(&T,&H,NULL,true);tlsRaw R;tlsRawConnect(&T,&R,false);
    char Wire[2048],Body[2048];unsigned Status;size_t Bytes;cstr Call="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\",\"params\":[7]}";
    int N=snprintf(Wire,sizeof(Wire),"POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nExpect: 100-continue\r\n\r\n",strlen(Call));
    tlsRawSend(&R,Wire,(size_t)N);testRequire(tlsRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==100 && !Bytes,"TLS split 100 Continue");tlsRawSend(&R,Call,strlen(Call));
    testRequire(tlsRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==200,"TLS interim final reply");
    N=snprintf(Wire,sizeof(Wire),"POST https://localhost/rpc HTTP/1.1\r\nHost: other\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\nTrailer: X-Note\r\n\r\n%zx\r\n%s\r\n0\r\nX-Note: tail\r\n\r\n",strlen(Call),Call);
    tlsRawSend(&R,Wire,7);tlsRawSend(&R,Wire+7,(size_t)N-7);testRequire(tlsRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==200,"TLS fragmented HTTPS absolute target/chunk/trailer");
    N=snprintf(Wire,sizeof(Wire),"POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%sPOST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",strlen(Call),Call,strlen(Call),Call);
    tlsRawSend(&R,Wire,(size_t)N);for(unsigned J=0;J<2;++J)testRequire(tlsRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==200,"TLS pipeline ordering");tlsRawDestroy(&R);
    tlsRawConnect(&T,&R,true);cstr Bad="POST http://localhost/rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 0\r\n\r\n";
    tlsRawSend(&R,Bad,strlen(Bad));testRequire(tlsRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==400,"TLS rejected cleartext absolute scheme");tlsRawDestroy(&R);rpcTlsServiceStop(&T,true);
    for(unsigned Mode=0;Mode<2;++Mode){xrpcHttpServerConfigInit(&H);H.IdleTimeout=Mode?5000:100;H.RequestTimeout=100;rpcTlsServiceStart(&T,&H,NULL,true);tlsRawConnect(&T,&R,false);
        if(Mode)tlsRawSend(&R,"P",1);
        tlsWait(&R.Closed,1);xrpchttpserverinfo I={0};I.Size=sizeof(I);
        testRequire(xrpcHttpServerInfo(T.Base.Server,&I) && I.Timeouts && !I.Dispatched,"TLS idle/incomplete request deadline");tlsRawDestroy(&R);rpcTlsServiceStop(&T,true);}
    xrpchttpservertlsconfig K;xrpcHttpServerTlsConfigInit(&K);K.Session.RequireProtocol=true;rpcTlsServiceStart(&T,NULL,&K,true);tlsRawConnect(&T,&R,true);tlsWait(&R.Closed,1);tlsWait(&T.Errors,1);
    testRequire(!xrtAtomic32Load(&T.Base.Calls,XMEMORY_ACQUIRE) && tlsInfo(&T).Failed==1,"required ALPN rejection");tlsRawDestroy(&R);rpcTlsServiceStop(&T,true);
}
typedef struct tlsBudget {xallocationbudget* Budget;xallocationscope Scope;xnetpost Enter,Leave;xatomic32 Entered,Left;} tlsBudget;
static void tlsBudgetEnter(xnetworker* Worker,ptr Data)
{(void)Worker;tlsBudget* B=Data;testRequire(xrtAllocationBudgetEnter(B->Budget,&B->Scope),"peer worker budget enter");xrtAtomic32Store(&B->Entered,1,XMEMORY_RELEASE);}
static void tlsBudgetLeave(xnetworker* Worker,ptr Data)
{(void)Worker;tlsBudget* B=Data;xrtClearError();testRequire(xrtAllocationBudgetLeave(&B->Scope),"peer worker budget leave");xrtAtomic32Store(&B->Left,1,XMEMORY_RELEASE);}
static void serviceTlsPeerOOM(void)
{
    const size_t Limits[]={1,128,1024,4096,16384,65536,262144,1048576};unsigned Rejected=0,Passed=0;
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){rpcTlsService T;xrpchttpserverconfig H;xrpcHttpServerConfigInit(&H);H.Listen.Distribution=XNET_ACCEPT_LOCAL;H.Listen.Affinity=0;
        xrpchttpservertlsconfig K;xrpcHttpServerTlsConfigInit(&K);K.Stream.HandshakeTimeout=500;rpcTlsServiceStart(&T,&H,&K,true);
        tlsBudget B={0};xrtAtomic32Init(&B.Entered,0);xrtAtomic32Init(&B.Left,0);
        testRequire(xrtAllocationBudgetCreate(Limits[N],&B.Budget) && xrtNetPostInit(&B.Enter) && xrtNetPostInit(&B.Leave) && xrtNetPost(xrtNetEngineWorker(T.Base.Engine,0),&B.Enter,tlsBudgetEnter,&B),"peer worker budget setup");tlsWait(&B.Entered,1);
        xrpchttpclient* C=rpcTlsServiceClient(&T,NULL);xfuture* F=rpcServiceSubmit(C,"echo","[7]",2000);const xrpccallresult* R=rpcServiceResult(F);
        testRequire(R->Status==XRPC_CALL_OK || R->Status==XRPC_CALL_TRANSPORT || R->Status==XRPC_CALL_TIMEOUT || R->Status==XRPC_CALL_CLOSED,"OOM peer result type");if(R->Status==XRPC_CALL_OK)++Passed;
        testRequire(xrtNetPost(xrtNetEngineWorker(T.Base.Engine,0),&B.Leave,tlsBudgetLeave,&B),"peer worker leave post");tlsWait(&B.Left,1);
        rpcServiceClientClose(C);rpcTlsServiceStop(&T,true);xrtFutureDestroy(F);xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);
        testRequire(xrtAllocationBudgetInfo(B.Budget,&I) && !I.LiveBytes && !I.LiveCount,"peer/session/native owner budget leak");if(I.Rejections)++Rejected;xrtAllocationBudgetFree(B.Budget);
    }
    testRequire(Rejected && Passed,"peer OOM thresholds missed rejection/success");
}
static xrpchttpauthstatus tlsAuthorize(const xrpchttpauthrequest* R,bytes Out,size_t Cap,size_t* Used,ptr User)
{
    rpcTlsService* T=User;testRequire(R->Secure && !xrtNetEngineCurrent(T->Base.Engine) && R->Head->MethodCode==XHTTP_METHOD_POST,"TLS authorization worker/transport");
    const xhttpfield* F=NULL;if(xrtHttpFieldGetUnique(R->Head->Fields,R->Head->FieldCount,XRT_STR_LITERAL("Authorization"),&F)!=XHTTP_NEXT_ITEM || F->Value.Size!=21 || memcmp(F->Value.Data,"Bearer public-fixture",21))return XRPC_HTTP_AUTH_UNAUTHORIZED;
    testRequire(Cap>=4,"TLS identity capacity");memcpy(Out,"[42]",4);*Used=4;return XRPC_HTTP_AUTH_ALLOW;
}
static void serviceTlsAuthorization(void)
{
    rpcTlsService T;xrpchttpserverconfig H;xrpcHttpServerConfigInit(&H);H.Authorize=tlsAuthorize;H.AuthorizationUser=&T;H.WriteChunkBytes=3;H.MaxQueuedBytes=64;rpcTlsServiceStart(&T,&H,NULL,true);
    xrpchttpconfig K;xrpcHttpConfigInit(&K);char Value[]="Bearer public-fixture";xhttpfield F={XRT_STR_LITERAL("Authorization"),{Value,sizeof(Value)-1}};K.RequestHeaders=&F;K.RequestHeaderCount=1;
    xrpchttpclient* C=rpcTlsServiceClient(&T,&K);memset(Value,'x',sizeof(Value));xfuture* Retained=NULL;
    for(unsigned N=0;N<16;++N){xfuture* Future=rpcServiceSubmit(C,"identity","[]",10000);const xrpccallresult* R=rpcServiceResult(Future);xrpcresponseinfo I={0};I.Size=sizeof(I);
        bool OK=R->Status==XRPC_CALL_OK && xrpcResponsesInfo(R->Responses,&I) && I.Messages[0].Result.Raw.Size==4 && !memcmp(I.Messages[0].Result.Raw.Data,"[42]",4);
        if(!OK){xrpchttpserverinfo Q={0};Q.Size=sizeof(Q);(void)xrpcHttpServerInfo(T.Base.Server,&Q);fprintf(stderr,"auth TLS n=%u status=%d delivery=%d cause=%d allowed=%llu errors=%llu business=%u context=%zu\n",N,(int)R->Status,(int)R->Delivery,xrtErrorCode(R->Cause),(unsigned long long)Q.AuthorizationAllowed,(unsigned long long)Q.AuthorizationErrors,xrtAtomic32Load(&T.Base.Calls,XMEMORY_RELAXED),Q.Execution.ContextBytes);}
        testRequire(OK,"TLS authorized identity snapshot");if(N)xrtFutureDestroy(Future);else Retained=Future;}
    rpcServiceClientClose(C);xrpchttpserverinfo Info={0};Info.Size=sizeof(Info);testRequire(xrpcHttpServerInfo(T.Base.Server,&Info) && Info.AuthorizationAllowed==16,"TLS authorization counter");
    rpcTlsServiceStop(&T,true);testRequire(rpcServiceResult(Retained)->Status==XRPC_CALL_OK,"TLS authorized result retention");xrtFutureDestroy(Retained);
}
int main(void)
{
    puts("[case] TLS traffic");fflush(stdout);serviceTlsTraffic();
    puts("[case] TLS credit and snapshot");fflush(stdout);serviceTlsCreditAndSnapshot();
    puts("[case] TLS admission");fflush(stdout);serviceTlsAdmission();
    puts("[case] TLS trust and join");fflush(stdout);serviceTlsRejectAndJoin();
    puts("[case] TLS factory");fflush(stdout);serviceTlsFactory();
    puts("[case] TLS raw protocol");fflush(stdout);serviceTlsRawProtocol();
    puts("[case] TLS worker OOM");fflush(stdout);serviceTlsPeerOOM();
    puts("[case] TLS authorization");fflush(stdout);serviceTlsAuthorization();
    puts("[PASS] xrpc HTTP TLS server: traffic/shared-credit/snapshot/admission/timeout/reentry/trust/noncooperative-shutdown/factory-and-worker-OOM/expect/chunk/trailer/pipeline/ALPN/physical-close/retained");return 0;
}
