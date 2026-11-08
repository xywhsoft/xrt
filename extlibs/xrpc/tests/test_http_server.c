#include "test.h"
#include "../examples/common/http_service.h"
#include <xrt/memory_stats.h>
static void testServiceCalls(void)
{
    rpcService S;rpcServiceStart(&S,NULL);xrpchttpclient* C=rpcServiceClient(&S,NULL);xfuture* F[32];
    for(unsigned K=0;K<32;++K){char Params[32];snprintf(Params,sizeof(Params),"[%u]",K);F[K]=rpcServiceSubmit(C,"echo",Params,5000);}
    for(unsigned K=0;K<32;++K){const xrpccallresult* R=rpcServiceResult(F[K]);testRequire(R->Status==XRPC_CALL_OK && R->Delivery==XRPC_REPLIED,"real HTTP ordinary outcome");
        xrpcresponseinfo I={0};I.Size=sizeof(I);char Text[32];int N=snprintf(Text,sizeof(Text),"[%u]",K);
        testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[0].Result.Raw.Size==(size_t)N && !memcmp(I.Messages[0].Result.Raw.Data,Text,(size_t)N),"HTTP payload correlation");if(K)xrtFutureDestroy(F[K]);}
    xfuture* Error=rpcServiceSubmit(C,"error",NULL,5000);testRequire(rpcServiceResult(Error)->Status==XRPC_CALL_REMOTE_ERROR,"server business error response");xrtFutureDestroy(Error);
    xfuture* Note=NULL;testRequire(xrpcHttpNotifyAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&Note)==XRPC_ADMISSION_ACCEPTED && rpcServiceResult(Note)->Status==XRPC_CALL_NOTIFY_DONE,"server notification HTTP completion");xrtFutureDestroy(Note);
    xrpchttpitem Items[]={{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]")},{XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("echo"),{0}},{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("error"),{0}}};
    testRequire(xrpcHttpBatchAsync(C,Items,3,5000,&Note)==XRPC_ADMISSION_ACCEPTED,"server batch admission");const xrpcbatchcallresult* B=(const xrpcbatchcallresult*)rpcServiceResult(Note);
    testRequire(B->Call.Status==XRPC_CALL_BATCH_DONE && B->Indices[0]==0 && B->Indices[1]==SIZE_MAX && B->Indices[2]==1,"real HTTP mixed batch");xrtFutureDestroy(Note);
    rpcServiceClientClose(C);rpcServiceStop(&S);testRequire(rpcServiceResult(F[0])->Status==XRPC_CALL_OK,"client retained server response after full cleanup");xrtFutureDestroy(F[0]);
}
static void testServiceShutdown(void)
{
    rpcService S;rpcServiceStart(&S,NULL);xrpchttpclient* C=rpcServiceClient(&S,NULL);xfuture* F=rpcServiceSubmit(C,"hold",NULL,5000);
    double End=xrtTimer()+5;while(!xrtAtomic32Load(&S.Entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"server handler did not start");xrtSleep(1);}
    testRequire(xrpcHttpServerClose(S.Server) && xrpcHttpServerWaitFor(S.Server,5000)==XWAIT_OK,"close pending business request");
    testRequire(rpcServiceResult(F)->Status!=XRPC_CALL_OK,"shutdown fabricated successful response");xrtFutureDestroy(F);rpcServiceClientClose(C);rpcServiceStop(&S);
}
typedef struct serviceRaw {
    xnetstream* Stream;xmutex Gate;xatomic32 Open,Closed;
    unsigned char* Data;size_t Used;char Challenge[256];
} serviceRaw;
static void serviceRawOpen(xnetstream* Stream,ptr Data)
{(void)Stream;xrtAtomic32Store(&((serviceRaw*)Data)->Open,1,XMEMORY_RELEASE);}
static void serviceRawRead(xnetstream* Stream,xnetbuf* Buffer,ptr Data)
{(void)Stream;serviceRaw* R=Data;size_t N=xrtNetBufSize(Buffer);xrtMutexLock(&R->Gate);testRequire(N<=1048576u-R->Used,"raw receive bound");R->Used+=xrtNetBufRead(Buffer,R->Data+R->Used,N);xrtMutexUnlock(&R->Gate);}
static void serviceRawEnd(xnetstream* Stream,ptr Data){(void)Data;(void)xrtNetStreamAbort(Stream);}
static void serviceRawClosed(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{(void)Stream;(void)Result;(void)Error;xrtAtomic32Store(&((serviceRaw*)Data)->Closed,1,XMEMORY_RELEASE);}
static void serviceRawConnect(rpcService* S,serviceRaw* R)
{
    memset(R,0,sizeof(*R));xrtAtomic32Init(&R->Open,0);xrtAtomic32Init(&R->Closed,0);testRequire(xrtMutexInit(&R->Gate),"raw mutex");R->Data=xrtMalloc(1048576u);testRequire(R->Data!=NULL,"raw buffer");
    static const xnetstreamevents Events={serviceRawOpen,serviceRawRead,serviceRawEnd,NULL,NULL,NULL,serviceRawClosed};
    xnetstreamconfig K;xrtNetStreamConfigInit(&K);K.WriteLimit=2u*1024u*1024u;K.WriteHighWater=1048576u;K.WriteLowWater=524288u;
    R->Stream=xrtNetStreamConnect(S->Engine,&S->Address,2,&K,&Events,R);testRequire(R->Stream!=NULL,"raw connect");
    double End=xrtTimer()+5;while(!xrtAtomic32Load(&R->Open,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End && !xrtAtomic32Load(&R->Closed,XMEMORY_ACQUIRE),"raw open wait");xrtSleep(1);}
}
static void serviceRawSend(serviceRaw* R,const void* Bytes,size_t N)
{testRequire(xrtNetStreamSend(R->Stream,Bytes,N)==XNET_RESULT_OK,"raw send");}
static bool serviceRawResponse(serviceRaw* R,unsigned* Status,char* Body,size_t Capacity,size_t* Bytes)
{
    double End=xrtTimer()+10;
    for(;;){xrtMutexLock(&R->Gate);xhttpfield Fields[32];xhttp1head H;xrtHttp1HeadInit(&H,Fields,32);
        xhttp1status S=xrtHttp1ResponseParse((xbytesview){R->Data,R->Used},&H,NULL,NULL);
        testRequire(S==XHTTP1_MORE || S==XHTTP1_READY,"raw response framing");
        if(S==XHTTP1_READY){xhttp1bodyplan P;testRequire(xrtHttp1ResponseBodyPlan(&H,XRT_STR_LITERAL("POST"),&P) && (P.Mode==XHTTP1_BODY_NONE || P.Mode==XHTTP1_BODY_FIXED),"raw response body plan");
            size_t N=P.Mode==XHTTP1_BODY_NONE?0:(size_t)P.Length;
            if(N<=R->Used-H.Bytes){testRequire(N<=Capacity,"raw response capacity");if(N)memcpy(Body,R->Data+H.Bytes,N);*Bytes=N;*Status=H.Status;
                if(H.Status>=200){const xhttpfield* Date=xrtHttp1Field(&H,XRT_STR_LITERAL("Date"));xtime Parsed;testRequire(Date && xrtTimeParseHTTPDate(Date->Value,&Parsed),"HTTP response date");
                    const xhttpfield* Challenge=xrtHttp1Field(&H,XRT_STR_LITERAL("WWW-Authenticate"));R->Challenge[0]=0;if(Challenge){testRequire(Challenge->Value.Size<sizeof(R->Challenge),"challenge bound");memcpy(R->Challenge,Challenge->Value.Data,Challenge->Value.Size);R->Challenge[Challenge->Value.Size]=0;}}
                size_t Used=H.Bytes+N;memmove(R->Data,R->Data+Used,R->Used-Used);R->Used-=Used;xrtMutexUnlock(&R->Gate);return true;}}
        bool Closed=xrtAtomic32Load(&R->Closed,XMEMORY_ACQUIRE)!=0;xrtMutexUnlock(&R->Gate);if(Closed)return false;
        testRequire(xrtTimer()<End,"raw response timeout");xrtSleep(1);
    }
}
static void serviceRawDestroy(serviceRaw* R)
{(void)xrtNetStreamAbort(R->Stream);double End=xrtTimer()+10;while(!xrtAtomic32Load(&R->Closed,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"raw close wait");xrtSleep(1);}xrtNetStreamDestroy(R->Stream);xrtMutexUnit(&R->Gate);xrtFree(R->Data);}
static void testServiceRawProtocol(void)
{
    rpcService S;rpcServiceStart(&S,NULL);serviceRaw R;serviceRawConnect(&S,&R);char Body[16384];unsigned Status;size_t Bytes;
    cstr Call="{\"jsonrpc\":\"2.0\",\"id\":1e9999,\"method\":\"echo\",\"params\":[7]}";char Wire[16384];
    int N=snprintf(Wire,sizeof(Wire),"POST /rpc?trace=1 HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json; charset=\"UTF-8\"\r\nContent-Length: %zu\r\n\r\n%s",strlen(Call),Call);
    for(int K=0;K<N;++K){serviceRawSend(&R,Wire+K,1);xrtSleep(0);}
    bool Got=serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes);
    if(!Got || Status!=200)fprintf(stderr,"fragmented got=%d status=%u bytes=%zu\n",(int)Got,Got?Status:0u,Got?Bytes:0u);
    testRequire(Got && Status==200,"fragmented request/charset/query");
    xrpcmessage M;xrpcdocument D;xrpcdecodeconfig DC;xrpcDecodeConfigInit(&DC);DC.Mode=XRPC_DECODE_RESPONSES;
    testRequire(xrpcDecode((xstrview){Body,Bytes},&DC,&M,1,&D)==XRPC_DECODE_DONE && M.Id.Raw.Size==6 && !memcmp(M.Id.Raw.Data,"1e9999",6),"HTTP lossless huge exponent");
    N=snprintf(Wire,sizeof(Wire),"POST http://localhost/rpc HTTP/1.1\r\nHost: example.invalid\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\nTrailer: X-Note\r\n\r\n%zx;extension=value\r\n%s\r\n0\r\nX-Note: tail\r\n\r\n",strlen(Call),Call);
    serviceRawSend(&R,Wire,(size_t)N);testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==200,"absolute target/chunk extensions/trailers");
    N=snprintf(Wire,sizeof(Wire),"POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nExpect: 100-continue\r\n\r\n",strlen(Call));serviceRawSend(&R,Wire,(size_t)N);
    testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==100 && !Bytes,"100 Continue before body");serviceRawSend(&R,Call,strlen(Call));
    testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==200,"100 Continue final response");
    N=snprintf(Wire,sizeof(Wire),"POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%sPOST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",strlen(Call),Call,strlen(Call),Call);
    serviceRawSend(&R,Wire,(size_t)N);for(unsigned K=0;K<2;++K)testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==200,"pipelined request ordering");serviceRawDestroy(&R);
    struct {cstr Wire;unsigned Status;} Bad[]={
        {"POST /rpc HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: 0\r\n\r\n",400},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nHost: b\r\nContent-Type: application/json\r\nContent-Length: 0\r\n\r\n",400},
        {"GET /rpc HTTP/1.1\r\nHost: a\r\n\r\n",405},
        {"POST /other HTTP/1.1\r\nHost: a\r\n\r\n",404},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n",415},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json; charset=latin1\r\nContent-Length: 0\r\n\r\n",415},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nContent-Encoding: gzip\r\nContent-Length: 0\r\n\r\n",415},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nContent-Length: 0\r\nExpect: magic\r\n\r\n",417},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\nContent-Length: 1\r\n\r\n0\r\n\r\n",400},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nHost: forged\r\n\r\n",400},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\nTrailer: Authorization\r\n\r\n0\r\n\r\n",400},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\nz\r\n",400},
        {"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nTransfer-Encoding: gzip, chunked\r\n\r\n0\r\n\r\n",501}};
    uint32 Calls=xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED);
    for(size_t K=0;K<sizeof(Bad)/sizeof(Bad[0]);++K){serviceRawConnect(&S,&R);serviceRawSend(&R,Bad[K].Wire,strlen(Bad[K].Wire));
        bool Replied=serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes);
        if(!Replied || Status!=Bad[K].Status || Bytes)fprintf(stderr,"bad case=%zu got=%d status=%u expected=%u bytes=%zu\n",K,(int)Replied,Replied?Status:0,Bad[K].Status,Replied?Bytes:0);
        testRequire(Replied && Status==Bad[K].Status && !Bytes,"HTTP syntax/binding rejection");serviceRawDestroy(&R);}
    testRequire(xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED)==Calls,"rejected wire invoked business");rpcServiceStop(&S);
}
static void testServiceDocumentsAndLimits(void)
{
    rpcService S;rpcServiceStart(&S,NULL);
    struct {cstr Json;unsigned Status;int Code;} Cases[]={
        {"{\"jsonrpc\":\"2.0\",\"method\":\"echo\"}",204,0},
        {"[{\"jsonrpc\":\"2.0\",\"method\":\"echo\"},{\"jsonrpc\":\"2.0\",\"method\":\"error\"}]",204,0},
        {"[]",200,-32600},{"{",200,-32700},{"[7,{\"jsonrpc\":\"2.0\",\"method\":\"echo\"}]",200,-32600}};
    for(size_t K=0;K<sizeof(Cases)/sizeof(Cases[0]);++K){serviceRaw R;serviceRawConnect(&S,&R);char Wire[1024],Body[1024];unsigned Status;size_t Bytes;
        int N=snprintf(Wire,sizeof(Wire),"POST /rpc HTTP/1.0\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",strlen(Cases[K].Json),Cases[K].Json);
        serviceRawSend(&R,Wire,(size_t)N);testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==Cases[K].Status,"document HTTP policy");
        if(Cases[K].Code){xrpcmessage M[2];xrpcdocument D;xrpcdecodeconfig DC;xrpcDecodeConfigInit(&DC);DC.Mode=XRPC_DECODE_RESPONSES;
            cstr Code=Cases[K].Code==-32600?"-32600":"-32700";
            testRequire(xrpcDecode((xstrview){Body,Bytes},&DC,M,2,&D)==XRPC_DECODE_DONE && M[0].ErrorCode.Raw.Size==6 && !memcmp(M[0].ErrorCode.Raw.Data,Code,6),"document RPC error");}
        else testRequire(!Bytes,"notification produced wire");
        serviceRawDestroy(&R);
    }
    rpcServiceStop(&S);
    for(unsigned Mode=0;Mode<5;++Mode){xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);cstr Tail="Content-Length: 2048\r\n\r\n";unsigned Expected=413;
        if(Mode==0)K.Body.MaxBody=16;
        if(Mode==1){K.Head.MaxHead=64;Expected=431;}
        if(Mode==2){K.MaxRequestWireBytes=16;Expected=413;}
        if(Mode==3){K.Body.MaxTrailer=8;Tail="Transfer-Encoding: chunked\r\n\r\n0\r\nX-Long: abcdefghijklmnop\r\n\r\n";}
        if(Mode==4){K.Body.MaxBody=16;Tail="Transfer-Encoding: chunked\r\n\r\n100\r\n";}
        rpcServiceStart(&S,&K);serviceRaw R;serviceRawConnect(&S,&R);char Wire[512],Body[512];unsigned Status;size_t Bytes;
        int N=snprintf(Wire,sizeof(Wire),"POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\n%s",Tail);serviceRawSend(&R,Wire,(size_t)N);
        testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==Expected && !Bytes && !xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"head/body/trailer/wire limit before business");serviceRawDestroy(&R);rpcServiceStop(&S);
    }
}
static void testServiceAdmissionAndJoin(void)
{
    rpcService S;rpcServiceStart(&S,NULL);size_t Base=S.BaseBytes;rpcServiceStop(&S);
    for(unsigned Mode=0;Mode<4;++Mode){xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);
        if(Mode==0)K.MaxConnections=1;else if(Mode==1)K.MaxOperations=1;else if(Mode==2)K.MaxBufferBytes=Base+1;else K.Execution.MaxOperations=1;
        rpcServiceStart(&S,&K);xrpchttpclient* A=NULL;xfuture* F=NULL;
        if(Mode!=2){A=rpcServiceClient(&S,NULL);F=rpcServiceSubmit(A,"hold",NULL,5000);double End=xrtTimer()+5;
            while(!xrtAtomic32Load(&S.Entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"quota first handler");xrtSleep(1);}}
        xrpchttpclient* B=rpcServiceClient(&S,NULL);xfuture* G=rpcServiceSubmit(B,"echo",NULL,5000);
        testRequire(rpcServiceResult(G)->Status!=XRPC_CALL_OK,"admission exceeded hard bound");xrtFutureDestroy(G);rpcServiceClientClose(B);
        xrpchttpserverinfo I={0};I.Size=sizeof(I);testRequire(xrpcHttpServerInfo(S.Server,&I) && (Mode==0 || Mode==2?I.Rejected:I.OperationRejected)>0,"admission refusal counter");
        if(A){xrtAtomic32Store(&S.Permit,1,XMEMORY_RELEASE);testRequire(rpcServiceResult(F)->Status==XRPC_CALL_OK,"quota poisoned existing call");xrtFutureDestroy(F);rpcServiceClientClose(A);}rpcServiceStop(&S);
    }
    rpcServiceStart(&S,NULL);xrpchttpclient* C=rpcServiceClient(&S,NULL);xfuture* F=rpcServiceSubmit(C,"stubborn",NULL,5000);double End=xrtTimer()+5;
    while(!xrtAtomic32Load(&S.Entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"stubborn start");xrtSleep(1);}
    testRequire(xrpcHttpServerClose(S.Server) && xrpcHttpServerWaitFor(S.Server,20)==XWAIT_TIMEOUT,"noncooperative handler lost owner");
    xrtAtomic32Store(&S.Permit,1,XMEMORY_RELEASE);(void)rpcServiceResult(F);xrtFutureDestroy(F);rpcServiceClientClose(C);rpcServiceStop(&S);
    rpcServiceStart(&S,NULL);C=rpcServiceClient(&S,NULL);F=rpcServiceSubmit(C,"self",NULL,5000);(void)rpcServiceResult(F);xrtFutureDestroy(F);
    testRequire(xrpcHttpServerWaitFor(S.Server,5000)==XWAIT_OK && xrtAtomic32Load(&S.Window,XMEMORY_ACQUIRE)==1,"self wait/destroy refusal");rpcServiceClientClose(C);rpcServiceStop(&S);
}
static void testServiceCreateOOMAndBind(void)
{
    rpcService S;rpcServiceStart(&S,NULL);const size_t Limits[]={1,64,4096,65536,131072,262144,1048576,4194304};unsigned Passed=0,Failed=0;
    for(size_t N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"HTTP factory quota enter");
        xrpchttpserver* H=NULL;bool OK=xrpcHttpServerCreate(S.Engine,NULL,&H);testRequire(xrtAllocationBudgetLeave(&Scope),"HTTP factory quota leave");
        if(OK){++Passed;testRequire(xrpcHttpServerDestroy(H),"HTTP factory quota destroy");}else{++Failed;testRequire(!H,"HTTP factory failed output");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"HTTP factory allocation/pin leak");xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed,"HTTP factory thresholds");xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);K.Listen.Address=S.Address;K.Listen.ReuseAddress=false;
    xrpchttpserver* H=NULL;testRequire(xrpcHttpServerCreate(S.Engine,&K,&H) && !xrpcHttpServerStart(H),"duplicate bind failure");xrtClearError();
    testRequire(xrpcHttpServerClose(S.Server) && xrpcHttpServerWaitFor(S.Server,5000)==XWAIT_OK,"original listener drain");
    testRequire(xrpcHttpServerStart(H) && xrpcHttpServerDestroy(H),"bind failure retry and cleanup");rpcServiceStop(&S);
}
static void testServiceBackpressureAndTimeouts(void)
{
    xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);K.MaxQueuedBytes=25;K.WriteBytes=31;K.WriteChunkBytes=17;K.ReadBytes=23;K.InitialInputBytes=16;
    K.Listen.Stream.WriteLimit=512;K.Listen.Stream.WriteHighWater=256;K.Listen.Stream.WriteLowWater=128;
    rpcService S;rpcServiceStart(&S,&K);xrpchttpclient* C[8];xfuture* F[8];char Params[4101];Params[0]='[';Params[1]='"';memset(Params+2,'a',4096);Params[4098]='"';Params[4099]=']';Params[4100]=0;
    for(unsigned N=0;N<8;++N){C[N]=rpcServiceClient(&S,NULL);F[N]=rpcServiceSubmit(C[N],"echo",Params,10000);}
    for(unsigned N=0;N<8;++N){const xrpccallresult* R=rpcServiceResult(F[N]);testRequire(R->Status==XRPC_CALL_OK,"global send-credit writer failed to resume");xrpcresponseinfo I={0};I.Size=sizeof(I);
        testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[0].Result.Raw.Size==4100 && !memcmp(I.Messages[0].Result.Raw.Data,Params,4100),"fragmented/backpressured response bytes");xrtFutureDestroy(F[N]);rpcServiceClientClose(C[N]);}rpcServiceStop(&S);
    xrpcHttpServerConfigInit(&K);K.IdleTimeout=100;K.RequestTimeout=100;rpcServiceStart(&S,&K);serviceRaw Raw;serviceRawConnect(&S,&Raw);
    serviceRawSend(&Raw,"P",1);double End=xrtTimer()+5;while(!xrtAtomic32Load(&Raw.Closed,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"incomplete-header deadline failed");xrtSleep(1);}serviceRawDestroy(&Raw);
    xrpchttpserverinfo Info={0};Info.Size=sizeof(Info);testRequire(xrpcHttpServerInfo(S.Server,&Info) && Info.Timeouts>0 && !Info.Dispatched,"timeout dispatched incomplete request");rpcServiceStop(&S);
    xrpcHttpServerConfigInit(&K);K.RequestTimeout=500;rpcServiceStart(&S,&K);serviceRawConnect(&S,&Raw);char Head[256],Response[256];unsigned Status;size_t Bytes;
    cstr Body="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"budget\"}";int N=snprintf(Head,sizeof(Head),"POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nExpect: 100-continue\r\n\r\n",strlen(Body));
    serviceRawSend(&Raw,Head,(size_t)N);testRequire(serviceRawResponse(&Raw,&Status,Response,sizeof(Response),&Bytes) && Status==100,"deadline header barrier");
    xrtSleep(200);serviceRawSend(&Raw,Body,strlen(Body));testRequire(serviceRawResponse(&Raw,&Status,Response,sizeof(Response),&Bytes) && Status==200,"remaining body/execution deadline");
    testRequire(xrtAtomic32Load(&S.Window,XMEMORY_ACQUIRE)>0 && xrtAtomic32Load(&S.Window,XMEMORY_ACQUIRE)<400,"execution deadline restarted after body");serviceRawDestroy(&Raw);rpcServiceStop(&S);
}
typedef struct authTest {
    rpcService* Service;xatomic32 Calls,Entered,Permit;unsigned Mode;
} authTest;
static void authInit(authTest* A,rpcService* S)
{memset(A,0,sizeof(*A));A->Service=S;xrtAtomic32Init(&A->Calls,0);xrtAtomic32Init(&A->Entered,0);xrtAtomic32Init(&A->Permit,0);}
static void authWait(xatomic32* Value,uint32 N)
{double End=xrtTimer()+10;while(xrtAtomic32Load(Value,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,"authorization event timeout");xrtSleep(1);}}
static bool authValueEqual(xstrview V,xstrview Expected)
{return V.Size==Expected.Size && (!V.Size || !memcmp(V.Data,Expected.Data,V.Size));}
static xrpchttpauthstatus authorizeTest(const xrpchttpauthrequest* R,bytes Context,size_t Capacity,size_t* Used,ptr Data)
{
    authTest* A=Data;testRequire(!xrtNetEngineCurrent(A->Service->Engine) && R->Head->MethodCode==XHTTP_METHOD_POST && R->Cancel && R->Deadline>0 && !R->Secure,"authorization callback context/worker");
    xrtAtomic32FetchAdd(&A->Calls,1,XMEMORY_RELAXED);
    if(A->Mode){xrtAtomic32FetchAdd(&A->Entered,1,XMEMORY_RELEASE);
        if(A->Mode==4){testRequire(xrpcHttpServerWaitFor(A->Service->Server,0)==XWAIT_ERROR && !xrpcHttpServerDestroy(A->Service->Server),"authorizer self-wait/destroy");xrtClearError();return XRPC_HTTP_AUTH_ALLOW;}
        while(!xrtAtomic32Load(&A->Permit,XMEMORY_ACQUIRE)){if(A->Mode!=2 && (xrtCancelRequested(R->Cancel) || xrtTimer()>=R->Deadline))return XRPC_HTTP_AUTH_ERROR;xrtSleep(1);}}
    const xhttpfield* F=NULL;xhttpnext N=xrtHttpFieldGetUnique(R->Head->Fields,R->Head->FieldCount,XRT_STR_LITERAL("Authorization"),&F);
    if(N!=XHTTP_NEXT_ITEM)return XRPC_HTTP_AUTH_UNAUTHORIZED;
    if(authValueEqual(F->Value,XRT_STR_LITERAL("Bearer forbidden")))return XRPC_HTTP_AUTH_FORBIDDEN;
    if(authValueEqual(F->Value,XRT_STR_LITERAL("Bearer error")))return XRPC_HTTP_AUTH_ERROR;
    if(authValueEqual(F->Value,XRT_STR_LITERAL("Bearer overflow"))){*Used=Capacity+1;return XRPC_HTTP_AUTH_ALLOW;}
    if(authValueEqual(F->Value,XRT_STR_LITERAL("Bearer invalid")))return (xrpchttpauthstatus)99;
    if(!authValueEqual(F->Value,XRT_STR_LITERAL("Bearer public-fixture")))return XRPC_HTTP_AUTH_UNAUTHORIZED;
    testRequire(Capacity>=4,"authorization output capacity");memcpy(Context,"[42]",4);*Used=4;return XRPC_HTTP_AUTH_ALLOW;
}
static void testServiceAuthorization(void)
{
    rpcService S;authTest A;authInit(&A,&S);xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);K.Authorize=authorizeTest;K.AuthorizationUser=&A;K.Execution.MaxOperations=1;K.Execution.Executor.Threads=1;
    char Challenge[]="Bearer realm=\"rpc\"";K.UnauthorizedChallenge=(xstrview){Challenge,sizeof(Challenge)-1};rpcServiceStart(&S,&K);memset(Challenge,'x',sizeof(Challenge));
    const struct {cstr Header;unsigned Status;} Cases[]={{"",401},{"Authorization: Bearer forbidden\r\n",403},{"Authorization: Bearer error\r\n",503},{"Authorization: Bearer overflow\r\n",500},{"Authorization: Bearer invalid\r\n",500},{"Authorization: a\r\nauthorization: b\r\n",400}};
    for(size_t N=0;N<sizeof(Cases)/sizeof(*Cases);++N){serviceRaw R;serviceRawConnect(&S,&R);char Wire[1024],Body[128];unsigned Status;size_t Bytes;
        int Len=snprintf(Wire,sizeof(Wire),"POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 123\r\nExpect: 100-continue\r\n%s\r\n",Cases[N].Header);
        serviceRawSend(&R,Wire,(size_t)Len);testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==Cases[N].Status && !Bytes,"authorization rejection/100-continue policy");
        if(Status==401)testRequire(!strcmp(R.Challenge,"Bearer realm=\"rpc\""),"owned WWW-Authenticate challenge");
        serviceRawDestroy(&R);}
    testRequire(!xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED) && xrtAtomic32Load(&A.Calls,XMEMORY_RELAXED)==5,"unauthorized requests entered business/duplicate credentials entered authorization");
    xrpchttpconfig Client;xrpcHttpConfigInit(&Client);xhttpfield F={XRT_STR_LITERAL("Authorization"),XRT_STR_LITERAL("Bearer public-fixture")};Client.RequestHeaders=&F;Client.RequestHeaderCount=1;xrpchttpclient* C=rpcServiceClient(&S,&Client);
    for(unsigned N=0;N<16;++N){xfuture* Future=rpcServiceSubmit(C,"identity","[]",5000);const xrpccallresult* R=rpcServiceResult(Future);xrpcresponseinfo I={0};I.Size=sizeof(I);
        testRequire(R->Status==XRPC_CALL_OK && xrpcResponsesInfo(R->Responses,&I) && I.Messages[0].Result.Raw.Size==4 && !memcmp(I.Messages[0].Result.Raw.Data,"[42]",4),"authorized identity transfer / one-operation execution credit");xrtFutureDestroy(Future);}
    xrpchttpitem Items[]={{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("identity"),{0}},{XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("identity"),{0}}};xfuture* Future=NULL;
    testRequire(xrpcHttpBatchAsync(C,Items,2,5000,&Future)==XRPC_ADMISSION_ACCEPTED && rpcServiceResult(Future)->Status==XRPC_CALL_BATCH_DONE,"authorization whole-document context");xrtFutureDestroy(Future);rpcServiceClientClose(C);
    xrpchttpserverinfo I={0};I.Size=sizeof(I);testRequire(xrpcHttpServerInfo(S.Server,&I) && I.AuthorizationAllowed==17 && I.AuthorizationDenied==2 && I.AuthorizationErrors==3,"authorization status counters");rpcServiceStop(&S);
}
static void testServiceAuthorizationWaits(void)
{
    for(unsigned Mode=0;Mode<4;++Mode){rpcService S;authTest A;authInit(&A,&S);A.Mode=Mode==1?2:Mode==3?4:1;xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);K.Authorize=authorizeTest;K.AuthorizationUser=&A;K.RequestTimeout=Mode==2?50:5000;
        rpcServiceStart(&S,&K);serviceRaw R;serviceRawConnect(&S,&R);cstr Head="POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 44\r\nAuthorization: Bearer public-fixture\r\nExpect: 100-continue\r\n\r\n";
        serviceRawSend(&R,Head,strlen(Head));authWait(&A.Entered,1);
        if(!Mode){xrtMutexLock(&R.Gate);testRequire(!R.Used,"100 Continue preceded authorization");xrtMutexUnlock(&R.Gate);xrtAtomic32Store(&A.Permit,1,XMEMORY_RELEASE);
            char Body[256];unsigned Status;size_t Bytes;testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==100,"authorized Continue");
            cstr Input="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"identity\"}";testRequire(strlen(Input)==44,"test request length");serviceRawSend(&R,Input,strlen(Input));testRequire(serviceRawResponse(&R,&Status,Body,sizeof(Body),&Bytes) && Status==200,"authorized body/handler");}
        else if(Mode==1){testRequire(xrpcHttpServerClose(S.Server) && xrpcHttpServerWaitFor(S.Server,0)==XWAIT_TIMEOUT,"noncooperative authorization lost physical owner");xrtAtomic32Store(&A.Permit,1,XMEMORY_RELEASE);}
        else if(Mode==2){authWait(&R.Closed,1);testRequire(!xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"expired authorization entered handler");}
        else authWait(&R.Closed,1);
        serviceRawDestroy(&R);rpcServiceStop(&S);}
}
static void testServiceAuthorizationBounds(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){rpcService S;authTest A;authInit(&A,&S);A.Mode=2;xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);K.Authorize=authorizeTest;K.AuthorizationUser=&A;K.Execution.Executor.Threads=1;
        if(!Mode)K.MaxAuthorizations=1;else if(Mode==1)K.Execution.MaxOperations=1;else K.Execution.Executor.QueueLimit=1;
        rpcServiceStart(&S,&K);serviceRaw First,Second,Third;serviceRawConnect(&S,&First);
        cstr Head="POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 44\r\nAuthorization: Bearer public-fixture\r\n\r\n";
        serviceRawSend(&First,Head,strlen(Head));authWait(&A.Entered,1);serviceRawConnect(&S,&Second);serviceRawSend(&Second,Head,strlen(Head));
        serviceRaw* Refused=&Second;
        if(Mode==2){double End=xrtTimer()+5;for(;;){xrpchttpserverinfo I={0};I.Size=sizeof(I);testRequire(xrpcHttpServerInfo(S.Server,&I),"queued authorization info");if(I.Execution.Auxiliary==2)break;testRequire(xrtTimer()<End,"authorization queue did not fill");xrtSleep(1);}
            serviceRawConnect(&S,&Third);serviceRawSend(&Third,Head,strlen(Head));Refused=&Third;}
        char Body[64];unsigned Status;size_t Bytes;testRequire(serviceRawResponse(Refused,&Status,Body,sizeof(Body),&Bytes) && Status==503,"authorization admission boundary");
        testRequire(xrtAtomic32Load(&A.Calls,XMEMORY_RELAXED)==1 && !xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"refused authorization executed callback/business");
        testRequire(xrpcHttpServerClose(S.Server),"authorization queued cancel");xrtAtomic32Store(&A.Permit,1,XMEMORY_RELEASE);
        serviceRawDestroy(&First);serviceRawDestroy(&Second);if(Mode==2)serviceRawDestroy(&Third);rpcServiceStop(&S);}
    rpcService S;authTest A;authInit(&A,&S);
    for(unsigned Mode=0;Mode<4;++Mode){xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);K.Authorize=authorizeTest;K.AuthorizationUser=&A;
        if(!Mode)K.MaxAuthorizations=0;else if(Mode==1)K.MaxAuthorizationContextBytes=K.Execution.MaxContextBytes+1;else if(Mode==2)K.UnauthorizedChallenge=XRT_STR_LITERAL("Bearer\r\nX: injected");else K.UnauthorizedChallenge=(xstrview){0};
        xnetengineconfig C;xrtNetEngineConfigInit(&C);xnetengine* E=xrtNetEngineCreate(&C);testRequire(E && xrtNetEngineStart(E),"invalid auth factory engine");xrpchttpserver* H=(xrpchttpserver*)(uintptr_t)1;
        testRequire(!xrpcHttpServerCreate(E,&K,&H) && H==(xrpchttpserver*)(uintptr_t)1,"invalid authorization factory published output");xrtClearError();testRequire(xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"invalid auth factory pin leak");}
}
int main(void)
{testServiceCalls();testServiceShutdown();testServiceRawProtocol();testServiceDocumentsAndLimits();testServiceAdmissionAndJoin();testServiceCreateOOMAndBind();testServiceBackpressureAndTimeouts();testServiceAuthorization();testServiceAuthorizationWaits();testServiceAuthorizationBounds();puts("[PASS] xrpc HTTP server: real-client/call/notification/batch/raw-HTTP/chunk/trailer/expect/pipeline/worker-isolation/backpressure/deadline/admission/OOM/bind-retry/self-wait/retained-response/physical-close/async-authorization/identity/challenge");return 0;}
