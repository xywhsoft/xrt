#include "test.h"
#include <xrpc/server.h>
#include <xrt/thread.h>
#include <string.h>
typedef struct serverTestContext {
    xatomic32 Entered,Permit,Calls;
    uint64 OwnerThread;
    xrpcserver* Server;
    xallocationbudget* Budget;
    char Large[4098];
} serverTestContext;
static void serverTestInit(serverTestContext* C)
{memset(C,0,sizeof(*C));xrtAtomic32Init(&C->Entered,0);xrtAtomic32Init(&C->Permit,0);xrtAtomic32Init(&C->Calls,0);C->OwnerThread=xrtThreadCurrentId();C->Large[0]='"';memset(C->Large+1,'x',4096);C->Large[4097]='"';}
static bool serverTestMethod(const xrpcmessage* R,cstr M)
{size_t N=strlen(M);return R->Method.Raw.Size==N+2 && !memcmp(R->Method.Raw.Data+1,M,N);}
static bool serverTestCall(const xrpcmessage* R,xrpcreply* Reply,const xrpcservercontext* Context,ptr Data)
{
    serverTestContext* C=Data;testRequire(xrtThreadCurrentId()!=C->OwnerThread,"business ran on submitter");
    xrtAtomic32FetchAdd(&C->Calls,1,XMEMORY_RELAXED);
    if(serverTestMethod(R,"hold")){
        xrtAtomic32FetchAdd(&C->Entered,1,XMEMORY_RELEASE);
        while(!xrtAtomic32Load(&C->Permit,XMEMORY_ACQUIRE))xrtSleep(1);
    }
    if(serverTestMethod(R,"self")){
        testRequire(xrpcServerWaitFor(C->Server,0)==XWAIT_ERROR && !xrpcServerDestroy(C->Server),"executor self wait/destroy accepted");xrtClearError();
    }
    if(serverTestMethod(R,"unknown"))return false;
    if(serverTestMethod(R,"omit"))return true;
    if(serverTestMethod(R,"error"))return xrpcReplyError(Reply,XRT_STR_LITERAL("-1"),XRT_STR_LITERAL("business error"),(xstrview){0});
    testRequire(Context->Cancel && Context->Deadline>0,"server handler stop context");
    xallocationscope Scope={0};if(C->Budget)testRequire(xrtAllocationBudgetEnter(C->Budget,&Scope),"worker quota enter");
    bool OK=xrpcReplyResult(Reply,serverTestMethod(R,"large")?(xstrview){C->Large,sizeof(C->Large)}:R->Params.Raw.Size?R->Params.Raw:XRT_STR_LITERAL("null"));
    if(C->Budget)testRequire(xrtAllocationBudgetLeave(&Scope),"worker quota leave");
    return OK;
}
static xrpcserver* serverTestCreate(serverTestContext* C,xrpcserverconfig* Config)
{
    xrpcserverconfig K;if(Config)K=*Config;else xrpcServerConfigInit(&K);K.Call=serverTestCall;K.User=C;
    xrpcserver* S=NULL;testRequire(xrpcServerCreate(&K,&S),"server create");C->Server=S;return S;
}
static xfuture* serverTestSubmit(xrpcserver* S,cstr Wire,int64 Timeout)
{xfuture* F=NULL;testRequire(xrpcServerSubmit(S,(xstrview){Wire,strlen(Wire)},Timeout,&F)==XRPC_SERVER_ACCEPTED,"server submit");return F;}
static const xrpcserverresult* serverTestResult(xfuture* F)
{xfutureresult R;testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"server result");return R.Value;}
static void serverTestIdle(xrpcserver* S)
{double End=xrtTimer()+10;for(;;){xrpcserverinfo I={0};I.Size=sizeof(I);testRequire(xrpcServerInfo(S,&I),"server info");if(!I.Operations)return;testRequire(xrtTimer()<End,"server operation tails leaked");xrtSleep(0);}}
static void serverTestZero(xrpcserver* S,size_t Results)
{testRequire(xrpcServerWaitFor(S,10000)==XWAIT_OK,"server drain");xrpcserverinfo I={0};I.Size=sizeof(I);testRequire(xrpcServerInfo(S,&I) && !I.Accepting && !I.Operations && !I.RequestBytes && !I.Running && I.Results==Results && I.Accepted==I.Completed && !I.PublicationFailures,"server physical accounting");}
static void testServerDocuments(void)
{
    serverTestContext C;serverTestInit(&C);xrpcserver* S=serverTestCreate(&C,NULL);
    char Wire[]="[{\"jsonrpc\":\"2.0\",\"id\":123456789012345678901234567890,\"method\":\"echo\",\"params\":[7]},2,{\"jsonrpc\":\"2.0\",\"method\":\"echo\"},{\"jsonrpc\":\"2.0\",\"id\":\"e\",\"method\":\"error\"}]";
    xfuture* F=serverTestSubmit(S,Wire,5000);memset(Wire,'x',sizeof(Wire));const xrpcserverresult* R=serverTestResult(F);
    testRequire(R->Status==XRPC_SERVER_DONE && R->MayHaveExecuted && R->Dispatch.Batch && R->Dispatch.Replies==3 && R->Dispatch.Notifications==1,"mixed server document outcome");
    xrpcmessage Scratch[3];xrpcdecodeconfig Decode;xrpcDecodeConfigInit(&Decode);Decode.Mode=XRPC_DECODE_RESPONSES;xrpcdocument Doc;testRequire(xrpcDecode(R->Wire,&Decode,Scratch,3,&Doc)==XRPC_DECODE_DONE,"server batch wire");
    testRequire(Doc.Batch && Doc.Count==3 && Scratch[0].Id.Raw.Size==30 && Scratch[0].Result.Raw.Size==3 && !memcmp(Scratch[0].Result.Raw.Data,"[7]",3) && Scratch[1].Kind==XRPC_MESSAGE_ERROR && Scratch[2].Kind==XRPC_MESSAGE_ERROR,"server raw-id/map/error serialization");
    xfuture* Note=serverTestSubmit(S,"[{\"jsonrpc\":\"2.0\",\"method\":\"unknown\"},{\"jsonrpc\":\"2.0\",\"method\":\"echo\"}]",5000);
    R=serverTestResult(Note);testRequire(R->Status==XRPC_SERVER_DONE && !R->Wire.Size && R->Dispatch.Notifications==2 && !R->Dispatch.Replies,"notification server output");xrtFutureDestroy(Note);
    uint32 Calls=xrtAtomic32Load(&C.Calls,XMEMORY_RELAXED);Note=serverTestSubmit(S,"[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\"},!",5000);R=serverTestResult(Note);
    testRequire(R->Status==XRPC_SERVER_DONE && R->Dispatch.ParseError && !R->MayHaveExecuted && xrtAtomic32Load(&C.Calls,XMEMORY_RELAXED)==Calls,"late malformed input executed handler");xrtFutureDestroy(Note);
    Note=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"omit\"}",5000);R=serverTestResult(Note);testRequire(R->Status==XRPC_SERVER_DONE && R->Dispatch.Replies==1,"missing reply standard error");xrtFutureDestroy(Note);
    serverTestZero(S,1);testRequire(xrpcServerDestroy(S),"server destroy with retained response");R=serverTestResult(F);testRequire(R->Wire.Size && R->Dispatch.Batch,"result outlived server/workers");xrtFutureDestroy(F);
}
typedef struct serverTestWatch {xrpcserver* Server;unsigned Calls;} serverTestWatch;
static void serverTestNotify(ptr Data)
{serverTestWatch* W=Data;++W->Calls;testRequire(xrpcServerClose(W->Server) && xrpcServerWaitFor(W->Server,0)==XWAIT_ERROR && !xrpcServerDestroy(W->Server),"completion self wait/destroy");xrtSetErrorInfo(XERR_VALUE,"xrt.test",999,"notification");}
static void testServerStops(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){serverTestContext C;serverTestInit(&C);xrpcserverconfig K;xrpcServerConfigInit(&K);K.Executor.Threads=1;xrpcserver* S=serverTestCreate(&C,&K);
        xfuture* Hold=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"hold\"}",Mode==2?1:5000);
        double End=xrtTimer()+5;while(!xrtAtomic32Load(&C.Entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"hold worker did not enter");xrtSleep(1);}
        xfuture* Queued=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"echo\"}",5000);
        xfuture* Due=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"echo\"}",0);testRequire(xrtFutureCancel(Queued),"queued server cancel token");
        serverTestWatch W={S,0};xfuturewatch Watch;
        testRequire(xrtFutureWatchInit(&Watch,serverTestNotify,NULL,&W) && xrtFutureWatchAdd(Queued,&Watch)==XFUTURE_WATCH_PENDING,"server completion watch");
        if(Mode==0){xrtSetErrorInfo(XERR_VALUE,"xrt.test",123,"caller");const xerror* Ambient=xrtGetError();testRequire(xrpcServerCancel(S) && xrtGetError()==Ambient,"server cancel changed ambient via watches");xrtClearError();
            testRequire(xrtFutureState(Hold)==XFUTURE_PENDING,"cancel forcibly completed running handler");}
        else if(Mode==1)testRequire(xrtFutureCancel(Hold),"running future cancel");else xrtSleep(5);
        testRequire(xrpcServerWaitFor(S,0)==XWAIT_TIMEOUT,"server drain ignored active handler");xrtAtomic32Store(&C.Permit,1,XMEMORY_RELEASE);
        const xrpcserverresult* R=serverTestResult(Hold);testRequire(R->Status==(Mode==2?XRPC_SERVER_TIMEOUT:XRPC_SERVER_CANCELLED) && R->MayHaveExecuted && !R->Wire.Size,"running stop/side effect knowledge");
        R=serverTestResult(Queued);testRequire(R->Status==XRPC_SERVER_CANCELLED && !R->MayHaveExecuted && !R->Wire.Size,"queued stop executed handler");
        R=serverTestResult(Due);testRequire(R->Status==XRPC_SERVER_TIMEOUT && !R->MayHaveExecuted,"deadline priority");xrtFutureWatchRemove(Queued,&Watch);testRequire(W.Calls==1,"queued completion reentry count");
        xrtFutureDestroy(Hold);xrtFutureDestroy(Queued);xrtFutureDestroy(Due);serverTestZero(S,0);testRequire(xrpcServerDestroy(S),"stopped server destroy");
    }
}
static void testServerLimitsAndWorkerOOM(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){serverTestContext C;serverTestInit(&C);xrpcserverconfig K;xrpcServerConfigInit(&K);K.InitialOutputBytes=32;
        if(Mode==0)K.MaxResultBytes=512;else if(Mode==1)K.Dispatch.MaxOutputBytes=128;else if(Mode==2)K.Dispatch.Decode.MaxValues=3;
        else if(Mode==3)testRequire(xrtAllocationBudgetCreate(1,&C.Budget),"worker output quota create");else K.MaxResultBytes=2400;
        char Batch[2000],Payload[705];Payload[0]='[';Payload[1]='"';memset(Payload+2,'a',700);Payload[702]='"';Payload[703]=']';Payload[704]=0;
        snprintf(Batch,sizeof(Batch),"[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\",\"params\":%s},{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"echo\",\"params\":%s}]",Payload,Payload);
        xrpcserver* S=serverTestCreate(&C,&K);xfuture* F=serverTestSubmit(S,Mode==4?Batch:"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"large\"}",5000);const xrpcserverresult* R=serverTestResult(F);
        if(R->Status!=(Mode==3?XRPC_SERVER_MEMORY:XRPC_SERVER_LIMIT) || R->Wire.Size || R->MayHaveExecuted!=(Mode!=2))
            fprintf(stderr,"mode=%u status=%d wire=%zu entered=%d cause=%d\n",Mode,(int)R->Status,R->Wire.Size,(int)R->MayHaveExecuted,xrtErrorCode(R->Cause));
        testRequire(R->Status==(Mode==3?XRPC_SERVER_MEMORY:XRPC_SERVER_LIMIT) && !R->Wire.Size && R->MayHaveExecuted==(Mode!=2),"worker growth/decode/output cap outcome");
        if(Mode==4)testRequire(R->Dispatch.Replies==1 && R->Dispatch.Invoked==2,"realloc peak quota did not preserve partial-effects accounting");
        xrtFutureDestroy(F);serverTestZero(S,0);testRequire(xrpcServerDestroy(S),"worker limits cleanup");
        if(C.Budget){xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(C.Budget,&I) && !I.LiveBytes,"worker quota leaked");xrtAllocationBudgetFree(C.Budget);}
    }
    serverTestContext C;serverTestInit(&C);xrpcserverconfig K;xrpcServerConfigInit(&K);K.MaxResults=2;xrpcserver* S=serverTestCreate(&C,&K);
    xfuture* A=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\"}",5000);xfuture* B=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"echo\"}",5000);
    (void)serverTestResult(A);(void)serverTestResult(B);serverTestIdle(S);xfuture* Out=(xfuture*)(uintptr_t)1;
    xrtSetErrorInfo(XERR_VALUE,"xrt.test",321,"ambient");const xerror* Ambient=xrtGetError();
    testRequire(xrpcServerSubmit(S,XRT_STR_LITERAL("{}"),5000,&Out)==XRPC_SERVER_OVERLOADED && Out==(xfuture*)(uintptr_t)1 && xrtGetError()==Ambient,"retained result admission/ambient/output");xrtClearError();
    xrtFutureDestroy(A);A=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"echo\"}",5000);testRequire(serverTestResult(A)->Status==XRPC_SERVER_DONE,"released result did not return credit");
    serverTestZero(S,2);testRequire(xrpcServerDestroy(S),"retained quota ledger server destroy");testRequire(serverTestResult(B)->Wire.Size,"quota ledger result lifetime");xrtFutureDestroy(A);xrtFutureDestroy(B);
}
static void testServerConstructionOOM(void)
{
    serverTestContext C;serverTestInit(&C);xrpcserver* S=serverTestCreate(&C,NULL);unsigned Passed=0,Failed=0;
    for(size_t Limit=1;Limit<=2048;++Limit){xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"server request quota enter");
        xfuture* F=NULL;xrpcserveradmission A=xrpcServerSubmit(S,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\"}"),5000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"server request quota leave");
        if(A==XRPC_SERVER_ACCEPTED){++Passed;testRequire(serverTestResult(F)->Status==XRPC_SERVER_DONE,"admitted quota server result");xrtFutureDestroy(F);serverTestIdle(S);}
        else{++Failed;testRequire(A==XRPC_SERVER_ADMISSION_ERROR && !F,"server failed request published output");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(Budget,&I) && !I.LiveBytes,"server request quota leaked");xrtAllocationBudgetFree(Budget);
    }
    testRequire(Passed && Failed,"server request allocation boundaries");testRequire(xrpcServerDestroy(S),"OOM server destroy");
    const size_t Limits[]={1,64,4096,65536,131072,262144,1048576};Passed=Failed=0;
    for(size_t K=0;K<sizeof(Limits)/sizeof(Limits[0]);++K){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[K],&B) && xrtAllocationBudgetEnter(B,&Scope),"server factory quota enter");
        S=NULL;bool OK=xrpcServerCreate(NULL,&S);testRequire(xrtAllocationBudgetLeave(&Scope),"server factory quota leave");
        if(OK){++Passed;testRequire(xrpcServerDestroy(S),"factory quota server join");}else{++Failed;testRequire(!S,"factory failure published server");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"factory quota leaked");xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed,"server factory allocation boundaries");
}
static void testServerRetainedLedgerAndAdmission(void)
{
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1048576,&B) && xrtAllocationBudgetEnter(B,&Scope),"retained ledger budget enter");
    xrpcserver* S=NULL;testRequire(xrpcServerCreate(NULL,&S),"retained ledger server create");xfuture* F=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"absent\"}",5000);
    testRequire(xrtAllocationBudgetLeave(&Scope),"retained ledger scope leave");testRequire(serverTestResult(F)->Status==XRPC_SERVER_DONE && xrpcServerDestroy(S),"retained ledger physical destroy");
    xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && I.LiveBytes>0,"retained quota ledger not accounted");xrtFutureDestroy(F);xrtClearError();
    testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"retained ledger/input/token/payload quota leaked");xrtAllocationBudgetFree(B);
    for(unsigned Mode=0;Mode<3;++Mode){serverTestContext C;serverTestInit(&C);xrpcserverconfig K;xrpcServerConfigInit(&K);
        if(Mode==0)K.MaxRequestBytesTotal=1;else if(Mode==1)K.MaxResultBytes=1;else K.Dispatch.Decode.MaxInputBytes=1;
        S=serverTestCreate(&C,&K);F=(xfuture*)(uintptr_t)1;xrtSetErrorInfo(XERR_VALUE,"xrt.test",321,"ambient");const xerror* E=xrtGetError();
        testRequire(xrpcServerSubmit(S,XRT_STR_LITERAL("{}"),5000,&F)==XRPC_SERVER_OVERLOADED && F==(xfuture*)(uintptr_t)1 && xrtGetError()==E,"byte/input refusal changed output/ambient");xrtClearError();
        testRequire(xrpcServerClose(S) && xrpcServerSubmit(S,XRT_STR_LITERAL("{}"),5000,&F)==XRPC_SERVER_CLOSED && F==(xfuture*)(uintptr_t)1 && xrpcServerDestroy(S),"closed admission/cleanup");
    }
}
typedef struct serverTestProducer {xrpcserver* Server;unsigned Index;xfuture* F[32];} serverTestProducer;
static int32 serverTestProduce(ptr Data)
{serverTestProducer* P=Data;for(unsigned K=0;K<32;++K){char Text[150];int N=snprintf(Text,sizeof(Text),"{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"echo\",\"params\":[%u]}",P->Index*32+K,P->Index*32+K);testRequire(xrpcServerSubmit(P->Server,(xstrview){Text,(size_t)N},5000,&P->F[K])==XRPC_SERVER_ACCEPTED,"concurrent server submit");memset(Text,'x',sizeof(Text));}return 0;}
static void testServerConcurrencyAndAliases(void)
{
    serverTestContext C;serverTestInit(&C);xrpcserverconfig K;xrpcServerConfigInit(&K);K.MaxOperations=256;xrpcserver* S=serverTestCreate(&C,&K);serverTestProducer P[4];xthread* T[4];
    for(unsigned N=0;N<4;++N){P[N]=(serverTestProducer){0};P[N].Server=S;P[N].Index=N;T[N]=xrtThreadCreate(serverTestProduce,&P[N],0);testRequire(T[N]!=NULL,"producer thread");}
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWait(T[N])==XWAIT_OK,"producer join");xrtThreadDestroy(T[N]);
        for(unsigned J=0;J<32;++J){const xrpcserverresult* R=serverTestResult(P[N].F[J]);xrpcmessage M;xrpcdecodeconfig D;xrpcDecodeConfigInit(&D);D.Mode=XRPC_DECODE_RESPONSES;xrpcdocument Doc;testRequire(R->Status==XRPC_SERVER_DONE && xrpcDecode(R->Wire,&D,&M,1,&Doc)==XRPC_DECODE_DONE && Doc.Count==1,"concurrent server response");
            char Expected[32];int Len=snprintf(Expected,sizeof(Expected),"[%u]",N*32+J);testRequire(M.Result.Raw.Size==(size_t)Len && !memcmp(M.Result.Raw.Data,Expected,(size_t)Len),"concurrent owned input mixed jobs");xrtFutureDestroy(P[N].F[J]);}}
    testRequire(xrpcServerSubmit(S,XRT_STR_LITERAL("{}"),0,(xfuture**)S)==XRPC_SERVER_ADMISSION_ERROR,"server output alias");xrtClearError();
    testRequire(!xrpcServerCreate(&K,(xrpcserver**)&K),"server config alias");xrtClearError();
    xfuture* Self=serverTestSubmit(S,"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"self\"}",5000);testRequire(serverTestResult(Self)->Status==XRPC_SERVER_DONE,"self refusal damaged live server");xrtFutureDestroy(Self);serverTestZero(S,0);testRequire(xrpcServerDestroy(S),"concurrent server destroy");
}
int main(void)
{testServerDocuments();testServerStops();testServerLimitsAndWorkerOOM();testServerConstructionOOM();testServerRetainedLedgerAndAdmission();testServerConcurrencyAndAliases();puts("[PASS] xrpc server: owned-input/whole-batch/worker-dispatch/retained-quota/growth/deadline/cancel/reentry/self-wait/2048-OOM/concurrent-submit/physical-join");return 0;}
