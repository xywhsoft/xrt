#include "test.h"
#include <xrpc/duplex.h>
#include <xrt/thread.h>
#include <string.h>
#include <math.h>
typedef struct duplexTestContext {xatomic32 Calls,Entered,Permit,Wakes;uint64 Owner;} duplexTestContext;
static void duplexTestInit(duplexTestContext* C)
{memset(C,0,sizeof(*C));xrtAtomic32Init(&C->Calls,0);xrtAtomic32Init(&C->Entered,0);xrtAtomic32Init(&C->Permit,0);xrtAtomic32Init(&C->Wakes,0);C->Owner=xrtThreadCurrentId();}
static void duplexTestWake(ptr Data)
{duplexTestContext* C=Data;xrtAtomic32FetchAdd(&C->Wakes,1,XMEMORY_RELAXED);}
static bool duplexTestMethod(const xrpcmessage* R,cstr Name)
{size_t N=strlen(Name);return R->Method.Raw.Size==N+2 && !memcmp(R->Method.Raw.Data+1,Name,N);}
static bool duplexTestBusiness(const xrpcmessage* R,xrpcreply* Reply,const xrpcservercontext* Context,ptr Data)
{
    duplexTestContext* C=Data;testRequire(xrtThreadCurrentId()!=C->Owner,"duplex business entered I/O owner");
    xrtAtomic32FetchAdd(&C->Calls,1,XMEMORY_RELAXED);
    if(duplexTestMethod(R,"hold")){xrtAtomic32Store(&C->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&C->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
    if(duplexTestMethod(R,"context"))testRequire(Context->Data.Size==4 && !memcmp(Context->Data.Data,"data",4),"duplex context was not copied");
    if(duplexTestMethod(R,"error"))return xrpcReplyError(Reply,XRT_STR_LITERAL("-100"),XRT_STR_LITERAL("error"),(xstrview){0});
    if(duplexTestMethod(R,"missing"))return false;
    return xrpcReplyResult(Reply,R->Params.Raw.Size?R->Params.Raw:XRT_STR_LITERAL("true"));
}
static xrpcserver* duplexTestServer(duplexTestContext* C)
{xrpcserverconfig K;xrpcServerConfigInit(&K);K.Call=duplexTestBusiness;K.User=C;xrpcserver* S=NULL;testRequire(xrpcServerCreate(&K,&S),"duplex shared server");return S;}
static xrpcduplex* duplexTestCreate(xrpcserver* S,duplexTestContext* C,xrpcduplexconfig* Config)
{xrpcduplexconfig K;if(Config)K=*Config;else xrpcDuplexConfigInit(&K);K.Server=S;K.Wake=duplexTestWake;K.User=C;xrpcduplex* D=NULL;testRequire(xrpcDuplexCreate(&K,&D),"duplex create");return D;}
static xrpcduplexinfo duplexTestInfo(xrpcduplex* D)
{xrpcduplexinfo I={0};I.Size=sizeof(I);testRequire(xrpcDuplexInfo(D,&I),"duplex info");return I;}
static void duplexTestClose(xrpcduplex* D)
{
    testRequire(xrpcDuplexClose(D,XRPC_CALL_CLOSED),"duplex close");double End=xrtTimer()+10;
    while(!duplexTestInfo(D).Drained){testRequire(xrpcDuplexStep(D) && xrtTimer()<End,"duplex close did not drain");xrtSleep(0);}
    xrpcduplexinfo I=duplexTestInfo(D);testRequire(!I.Operations && !I.PendingCalls && !I.Incoming && !I.ControlBlocks && I.Accepted==I.Completed,"duplex duty accounting");
    testRequire(xrpcDuplexDestroy(D),"duplex destroy");
}
static const xrpccallresult* duplexTestResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{xfutureresult R;testRequire(xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"duplex typed Future");const xrpccallresult* C=R.Value;testRequire(C->Status==Status && C->Delivery==Delivery,"duplex wrong status/delivery");return C;}
static bool duplexTestPump(xrpcduplex* A,xrpcduplex* B)
{
    /* MaxWork=1 can require further owner turns to unselect a stopped output. */
    if(duplexTestInfo(A).Selected)return false;
    xrpcduplexoutput* O=NULL;uint64 Token=0;int N=xrpcDuplexNext(A,&O,&Token);testRequire(N>=0,"duplex select");if(!N)return false;
    /* A concurrent observer can cancel after selection and before the first
     * offer. A sender must retire that stale selection without sending it. */
    if(!xrpcDuplexStarted(A,Token)){xrpcDuplexOutputRelease(O);testRequire(xrpcDuplexStep(A),"stopped selected output retirement");return true;}
    testRequire(xrpcDuplexSent(A,Token),"duplex output completion");
    testRequire(xrpcDuplexReceive(B,xrpcDuplexOutputWire(O),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED,"duplex peer receive");
    xrpcDuplexOutputRelease(O);return true;
}
static void duplexTestUntil(xrpcduplex* A,xrpcduplex* B,xfuture** Futures,size_t Count)
{
    double End=xrtTimer()+10;
    for(;;){bool Done=true;for(size_t I=0;I<Count;++I)Done&=xrtFutureDone(Futures[I]);if(Done)return;
        testRequire(xrpcDuplexStep(A) && xrpcDuplexStep(B),"duplex step");
        bool Progress=duplexTestPump(A,B);Progress|=duplexTestPump(B,A);testRequire(xrtTimer()<End,"duplex response timeout");if(!Progress)xrtSleep(0);
    }
}
static void testDuplexBidirectional(void)
{
    duplexTestContext C;duplexTestInit(&C);xrpcserver* S=duplexTestServer(&C);xrpcduplex* A=duplexTestCreate(S,&C,NULL),*B=duplexTestCreate(S,&C,NULL);
    xfuture* F[5]={0};char Method[]="echo",Params[]="[1,2,3]";
    testRequire(xrpcDuplexCall(A,(xstrview){Method,4},(xstrview){Params,7},5000,F)==XRPC_DUPLEX_ACCEPTED &&
        xrpcDuplexCall(B,XRT_STR_LITERAL("error"),(xstrview){0},5000,F+1)==XRPC_DUPLEX_ACCEPTED &&
        xrpcDuplexNotify(A,XRT_STR_LITERAL("missing"),(xstrview){0},5000,F+2)==XRPC_DUPLEX_ACCEPTED,"duplex simultaneous admissions");
    memset(Method,'x',4);memset(Params,'x',7);
    xrpcduplexitem Batch[]={{false,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]")},{true,XRT_STR_LITERAL("echo"),(xstrview){0}},{false,XRT_STR_LITERAL("error"),(xstrview){0}}};
    testRequire(xrpcDuplexBatch(B,Batch,3,5000,F+3)==XRPC_DUPLEX_ACCEPTED,"duplex batch admit");
    Batch[0].Notify=true;Batch[2].Notify=true;testRequire(xrpcDuplexBatch(A,Batch,3,5000,F+4)==XRPC_DUPLEX_ACCEPTED,"duplex notification batch");
    duplexTestUntil(A,B,F,5);
    const xrpccallresult* R=duplexTestResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);xrpcresponseinfo I={0};I.Size=sizeof(I);
    testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[R->Index].Result.Raw.Size==7 && !memcmp(I.Messages[R->Index].Result.Raw.Data,"[1,2,3]",7),"duplex copied input/result");
    duplexTestResult(F[1],XRPC_CALL_REMOTE_ERROR,XRPC_REPLIED);duplexTestResult(F[2],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
    const xrpcbatchcallresult* BR=(const xrpcbatchcallresult*)duplexTestResult(F[3],XRPC_CALL_BATCH_DONE,XRPC_REPLIED);
    testRequire(BR->Count==3 && BR->Indices[0]==0 && BR->Indices[1]==SIZE_MAX && BR->Indices[2]==1,"duplex batch map");
    duplexTestResult(F[4],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
    duplexTestClose(A);duplexTestClose(B);testRequire(xrpcServerDestroy(S),"duplex shared server close");
    duplexTestResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);for(size_t N=0;N<5;++N)xrtFutureDestroy(F[N]);
}
static xrpcduplexoutput* duplexTestSend(xrpcduplex* D,uint64* Token)
{testRequire(xrpcDuplexStep(D),"duplex send step");xrpcduplexoutput* O=NULL;testRequire(xrpcDuplexNext(D,&O,Token)==1 && xrpcDuplexStarted(D,*Token) && xrpcDuplexSent(D,*Token),"duplex manual send");return O;}
static void testDuplexStops(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(NULL,&C,NULL);xfuture* F=NULL;uint64 T=0;xrpcduplexoutput* O=NULL;
        testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("effect"),(xstrview){0},Mode==0?0:XRT_WAIT_FOREVER,&F)==XRPC_DUPLEX_ACCEPTED,"duplex stop admit");
        if(Mode==1)testRequire(xrtFutureCancel(F),"duplex cancel before send");
        if(Mode>=2){testRequire(xrpcDuplexStep(D) && xrpcDuplexNext(D,&O,&T)==1 && xrpcDuplexStarted(D,T),"duplex partial send");
            if(Mode>=3)testRequire(xrpcDuplexSent(D,T),"duplex full send");
            if(Mode==4)testRequire(xrpcDuplexClose(D,XRPC_CALL_TRANSPORT),"duplex uncertain disconnect");else testRequire(xrtFutureCancel(F),"duplex cancel after send");}
        testRequire(xrpcDuplexStep(D),"duplex stopped step");
        duplexTestResult(F,Mode==0?XRPC_CALL_TIMEOUT:Mode==4?XRPC_CALL_TRANSPORT:XRPC_CALL_CANCELLED,Mode>=2?XRPC_MAY_HAVE_EXECUTED:XRPC_NOT_SENT);
        xrpcduplexinfo I=duplexTestInfo(D);testRequire(I.Closing==(Mode==2 || Mode==4),"partial cancel did not retire transport");
        if(Mode==3){testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":99}"),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED && duplexTestInfo(D).UnknownResponses==1,"late response consumed a new call");}
        duplexTestClose(D);if(O){testRequire(xrpcDuplexOutputWire(O).Size>0,"retained wire after endpoint destroy");xrpcDuplexOutputRelease(O);}xrtFutureDestroy(F);
    }
}
static void testDuplexResponseAtomicity(void)
{
    static const char* Bad[]={
        "[{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1},{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":2}]",
        "[{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1}]",
        "[{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1},{\"jsonrpc\":\"2.0\",\"id\":\"9\",\"result\":2}]",
        "[{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1},{\"jsonrpc\":\"2.0\",\"method\":\"effect\"}]",
        "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1}",
        "[{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1},{\"jsonrpc\":\"2.0\",\"id\":\"2\",\"result\":2,\"error\":{}}]"};
    for(size_t K=0;K<sizeof(Bad)/sizeof(Bad[0]);++K){duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(NULL,&C,NULL);xfuture* F=NULL;
        xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("echo"),(xstrview){0}},{true,XRT_STR_LITERAL("note"),(xstrview){0}},{false,XRT_STR_LITERAL("echo"),(xstrview){0}}};
        testRequire(xrpcDuplexBatch(D,Items,3,5000,&F)==XRPC_DUPLEX_ACCEPTED,"duplex atomic batch admit");uint64 T;xrpcduplexoutput* O=duplexTestSend(D,&T);
        testRequire(xrpcDuplexReceive(D,(xstrview){Bad[K],strlen(Bad[K])},(xbytesview){0})==XRPC_DUPLEX_PROTOCOL && xrtFutureState(F)==XFUTURE_PENDING,"bad response partially completed live batch");
        testRequire(xrpcDuplexStep(D),"duplex protocol close step");duplexTestResult(F,XRPC_CALL_PROTOCOL,XRPC_MAY_HAVE_EXECUTED);duplexTestClose(D);xrpcDuplexOutputRelease(O);xrtFutureDestroy(F);xrtClearError();
    }
    duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(NULL,&C,NULL);xfuture* F=NULL;
    testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED && xrpcDuplexStep(D),"duplex unsent admit");
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":true}"),(xbytesview){0})==XRPC_DUPLEX_PROTOCOL,"unsent response accepted");
    duplexTestClose(D);duplexTestResult(F,XRPC_CALL_PROTOCOL,XRPC_NOT_SENT);xrtFutureDestroy(F);
    D=duplexTestCreate(NULL,&C,NULL);xrpcduplexitem B[]={{false,XRT_STR_LITERAL("a"),(xstrview){0}},{true,XRT_STR_LITERAL("b"),(xstrview){0}},{false,XRT_STR_LITERAL("c"),(xstrview){0}}};
    testRequire(xrpcDuplexBatch(D,B,3,5000,&F)==XRPC_DUPLEX_ACCEPTED,"duplex reordered admit");uint64 T;xrpcduplexoutput* O=duplexTestSend(D,&T);
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":\"2\",\"result\":2},{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1}]"),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED,"duplex reordered response");
    const xrpcbatchcallresult* R=(const xrpcbatchcallresult*)duplexTestResult(F,XRPC_CALL_BATCH_DONE,XRPC_REPLIED);testRequire(R->Indices[0]==1 && R->Indices[1]==SIZE_MAX && R->Indices[2]==0,"duplex reordered mapping");
    duplexTestClose(D);xrpcDuplexOutputRelease(O);xrtFutureDestroy(F);
}
static void testDuplexLimitsAndJobs(void)
{
    duplexTestContext C;duplexTestInit(&C);xrpcduplexconfig K;xrpcDuplexConfigInit(&K);K.MaxOperations=2;K.MaxIncoming=1;K.MaxPendingCalls=2;K.MaxOutputs=1;K.MaxWork=1;
    xrpcduplex* D=duplexTestCreate(NULL,&C,&K);xfuture* F=NULL,*G=(xfuture*)(uintptr_t)1;uint64 T;
    testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("m"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"duplex wire quota first");
    testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("m"),(xstrview){0},5000,&G)==XRPC_DUPLEX_OVERLOADED && G==(xfuture*)(uintptr_t)1,"wire quota failed transfer");
    testRequire(xrpcDuplexStep(D) && duplexTestInfo(D).More,"bounded attach requests another turn");xrpcduplexoutput* O=NULL;
    testRequire(xrpcDuplexNext(D,&O,&T)==1 && xrpcDuplexStarted(D,T) && xrpcDuplexSent(D,T) && xrtFutureCancel(F) && xrpcDuplexStep(D),"retained wire setup");
    testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("m"),(xstrview){0},5000,&G)==XRPC_DUPLEX_OVERLOADED && duplexTestInfo(D).Outputs==1,"logical completion refunded physical wire");
    xrpcDuplexOutputRelease(O);testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("m"),(xstrview){0},5000,&G)==XRPC_DUPLEX_ACCEPTED,"released wire did not refund");
    duplexTestClose(D);xrtFutureDestroy(F);xrtFutureDestroy(G);
    xrpcserver* S=duplexTestServer(&C);D=duplexTestCreate(S,&C,&K);
    char Context[]="data";
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"hold\",\"id\":1}"),(xbytesview){(const unsigned char*)Context,4})==XRPC_DUPLEX_ACCEPTED,"incoming hold admission");
    double End=xrtTimer()+5;while(!xrtAtomic32Load(&C.Entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"duplex hold enter");xrtSleep(1);}
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"context\",\"id\":2}"),(xbytesview){(const unsigned char*)Context,4})==XRPC_DUPLEX_OVERLOADED,"incoming quota did not refuse");
    testRequire(xrpcDuplexClose(D,XRPC_CALL_CLOSED) && xrpcDuplexStep(D) && !duplexTestInfo(D).Drained && !xrpcDuplexDestroy(D),"close ignored live handler duty");xrtClearError();
    xrtAtomic32Store(&C.Permit,1,XMEMORY_RELEASE);duplexTestClose(D);testRequire(xrpcServerDestroy(S),"incoming server retirement");
}
static void testDuplexAllocation(void)
{
    duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(NULL,&C,NULL);unsigned Failed=0,Passed=0;
    for(size_t Limit=1;Limit<=4096;++Limit){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"duplex allocation budget");
        xfuture* F=NULL;xrpcduplexadmission A=xrpcDuplexCall(D,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1]"),5000,&F);
        testRequire(xrtAllocationBudgetLeave(&Scope),"duplex allocation leave");
        if(A==XRPC_DUPLEX_ACCEPTED){++Passed;testRequire(xrtFutureCancel(F) && xrpcDuplexStep(D),"duplex OOM accepted cleanup");xrtFutureDestroy(F);}else{++Failed;testRequire(A==XRPC_DUPLEX_ERROR && !F,"duplex OOM output transfer");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"duplex OOM prefix leaked");xrtAllocationBudgetFree(B);
    }
    testRequire(Failed && Passed,"duplex OOM prefixes did not cross success");duplexTestClose(D);
}
typedef struct duplexTestProducer {xrpcduplex* Duplex;xatomic32* Done;unsigned Index;bool CloseRace;} duplexTestProducer;
static int32 duplexTestProduce(ptr Data)
{
    duplexTestProducer* P=Data;
    for(unsigned N=0;N<64;++N){char Params[64];int Bytes=snprintf(Params,sizeof(Params),"[%u,%u]",P->Index,N);xfuture* F=NULL;
        xrpcduplexadmission A;
        do {A=xrpcDuplexCall(P->Duplex,XRT_STR_LITERAL("echo"),(xstrview){Params,(size_t)Bytes},5000,&F);
            if(A==XRPC_DUPLEX_OVERLOADED)xrtSleep(0);
        }while(A==XRPC_DUPLEX_OVERLOADED);
        if(A==XRPC_DUPLEX_CLOSED && P->CloseRace)break;
        testRequire(A==XRPC_DUPLEX_ACCEPTED,"concurrent duplex admission");
        bool Cancel=N%3==0;if(Cancel)xrtFutureCancel(F);
        testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK,"concurrent duplex Future wait");
        xfutureresult R;testRequire(xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"concurrent duplex result");
        const xrpccallresult* V=R.Value;
        if(V->Status==XRPC_CALL_OK){xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(xrpcResponsesInfo(V->Responses,&I) && I.Messages[V->Index].Result.Raw.Size==(size_t)Bytes && !memcmp(I.Messages[V->Index].Result.Raw.Data,Params,(size_t)Bytes),"cross-thread duplex result miscorrelation");}
        else testRequire((Cancel && V->Status==XRPC_CALL_CANCELLED) || (P->CloseRace && V->Status==XRPC_CALL_CLOSED),"concurrent duplex unexpected terminal outcome");
        xrtFutureDestroy(F);
    }
    xrtAtomic32FetchAdd(P->Done,1,XMEMORY_RELEASE);return 0;
}
static void testDuplexConcurrency(void)
{
    for(unsigned Race=0;Race<2;++Race){duplexTestContext C;duplexTestInit(&C);xrpcserver* S=duplexTestServer(&C);
        xrpcduplexconfig K;xrpcDuplexConfigInit(&K);K.MaxWork=1;K.MaxOperations=8;K.MaxIncoming=4;
        xrpcduplex* A=duplexTestCreate(S,&C,&K),*B=duplexTestCreate(S,&C,NULL);
        xatomic32 Done;xrtAtomic32Init(&Done,0);duplexTestProducer P[4];xthread* T[4];
        for(unsigned N=0;N<4;++N){P[N]=(duplexTestProducer){A,&Done,N,Race!=0};T[N]=xrtThreadCreate(duplexTestProduce,P+N,0);testRequire(T[N]!=NULL,"duplex producer thread");}
        double End=xrtTimer()+20;
        while(xrtAtomic32Load(&Done,XMEMORY_ACQUIRE)!=4){
            testRequire(xrpcDuplexStep(A) && xrpcDuplexStep(B) && xrtTimer()<End,"concurrent duplex owner stalled");
            if(Race){if(duplexTestInfo(A).Accepted>=4)testRequire(xrpcDuplexClose(A,XRPC_CALL_CLOSED),"concurrent admission close");}
            else {duplexTestPump(A,B);duplexTestPump(B,A);}
            xrtSleep(0);
        }
        for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(T[N],5000)==XWAIT_OK,"duplex producer physical join");xrtThreadDestroy(T[N]);}
        duplexTestClose(A);duplexTestClose(B);testRequire(xrpcServerDestroy(S),"concurrent duplex shared executor join");
    }
}
static void testDuplexIncomingDocuments(void)
{
    duplexTestContext C;duplexTestInit(&C);xrpcserver* S=duplexTestServer(&C);xrpcduplex* D=duplexTestCreate(S,&C,NULL);
    const char* Wire[]={"[{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"id\":1},!","[]","{\"jsonrpc\":\"2.0\",\"id\":\"x\\u0000y\",\"method\":\"context\",\"params\":[7]}"};
    for(unsigned N=0;N<3;++N){char Context[]="data";testRequire(xrpcDuplexReceive(D,(xstrview){Wire[N],strlen(Wire[N])},(xbytesview){(const unsigned char*)Context,4})==XRPC_DUPLEX_ACCEPTED,"incoming document receive");memset(Context,'x',4);
        double End=xrtTimer()+10;xrpcduplexoutput* O=NULL;uint64 Token=0;
        while(!O){testRequire(xrpcDuplexStep(D) && xrtTimer()<End,"incoming document completion");int Out=xrpcDuplexNext(D,&O,&Token);testRequire(Out>=0,"incoming output select");if(!Out)xrtSleep(0);}
        testRequire(!xrpcDuplexSent(D,Token),"response sent without first acceptance");
        xrpcdecodeconfig K;xrpcDecodeConfigInit(&K);K.Mode=XRPC_DECODE_RESPONSES;xrpcmessage Message;xrpcdocument Doc;
        testRequire(xrpcDecode(xrpcDuplexOutputWire(O),&K,&Message,1,&Doc)==XRPC_DECODE_DONE,"incoming standard response");
        if(N<2)testRequire(Message.Kind==XRPC_MESSAGE_ERROR && xrtAtomic32Load(&C.Calls,XMEMORY_RELAXED)==0,"malformed/invalid request executed business");
        else testRequire(Message.Kind==XRPC_MESSAGE_RESULT && Message.Id.Raw.Size==10,"incoming raw escaped ID changed");
        testRequire(xrpcDuplexStarted(D,Token) && xrpcDuplexSent(D,Token),"incoming response acceptance");
        if(N==2){duplexTestClose(D);testRequire(xrpcServerDestroy(S),"retained server-wire join");testRequire(xrpcDuplexOutputWire(O).Size>0,"server response wire did not outlive server/endpoint");}
        xrpcDuplexOutputRelease(O);
    }
}
typedef struct duplexTestReentry {xrpcduplex* D;xfuture* Extra;unsigned Calls;} duplexTestReentry;
static void duplexTestReenter(ptr Data)
{
    duplexTestReentry* R=Data;++R->Calls;xrpcduplexoutput* O=NULL;uint64 T=0;
    testRequire(!xrpcDuplexDestroy(R->D) && xrpcDuplexNext(R->D,&O,&T)==-1 && !xrpcDuplexStep(R->D),"owner mutation reentry accepted");xrtClearError();
    testRequire(xrpcDuplexCall(R->D,XRT_STR_LITERAL("extra"),(xstrview){0},0,&R->Extra)==XRPC_DUPLEX_ACCEPTED,"completion Watch concurrent-style admission refused");
}
static void testDuplexFailureAndReentry(void)
{
    duplexTestContext C;duplexTestInit(&C);
    const size_t Limits[]={1,64,256,1024,8192,65536,1048576,8388608,16777216};unsigned Passed=0,Failed=0;
    for(size_t N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"duplex constructor budget enter");
        xrpcduplex* D=NULL;bool OK=xrpcDuplexCreate(NULL,&D);testRequire(xrtAllocationBudgetLeave(&Scope),"duplex constructor budget leave");
        if(OK){++Passed;duplexTestClose(D);}else{++Failed;testRequire(!D,"duplex failed constructor changed output");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"duplex failed constructor leak");xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed,"duplex constructor OOM limits");
    xrpcduplex* D=duplexTestCreate(NULL,&C,NULL);xfuture* F=NULL;testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("m"),(xstrview){0},0,&F)==XRPC_DUPLEX_ACCEPTED,"reentry call");
    duplexTestReentry R={D,NULL,0};xfuturewatch W;testRequire(xrtFutureWatchInit(&W,duplexTestReenter,NULL,&R) && xrtFutureWatchAdd(F,&W)==XFUTURE_WATCH_PENDING,"reentry watch");
    testRequire(xrpcDuplexStep(D) && R.Calls==1 && R.Extra,"reentry completion publication");duplexTestClose(D);xrtFutureDestroy(F);xrtFutureDestroy(R.Extra);
    xrpcduplexconfig K;xrpcDuplexConfigInit(&K);K.MaxFixedBytes=1;D=(xrpcduplex*)(uintptr_t)1;
    testRequire(!xrpcDuplexCreate(&K,&D) && D==(xrpcduplex*)(uintptr_t)1,"fixed-storage bound changed output");xrtClearError();
    xrpcserver* S=duplexTestServer(&C);xrpcDuplexConfigInit(&K);K.MaxOutputBytes=1;D=duplexTestCreate(S,&C,&K);
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\"}"),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED,"oversized response incoming");
    double End=xrtTimer()+5;while(!duplexTestInfo(D).Closing){testRequire(xrpcDuplexStep(D) && xrtTimer()<End,"impossible reply size retried forever");xrtSleep(0);}
    testRequire(duplexTestInfo(D).Reason==XRPC_CALL_LIMIT && duplexTestInfo(D).OutputRefusals>0,"impossible output typed limit/counter");duplexTestClose(D);testRequire(xrpcServerDestroy(S),"reply-limit executor join");
}
static void testDuplexCompactCorrelation(void)
{
    duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(NULL,&C,NULL);
    testRequire(duplexTestInfo(D).OwnedBytes<256u*1024u,"compact default endpoint exceeds 256 KiB");
    xfuture* F=NULL;testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"escaped response call");
    uint64 T;xrpcduplexoutput* O=duplexTestSend(D,&T);
    const char* Unknown[]={"null","1","1.0","1e0","18446744073709551616","\"\"","\"0\"","\"01\"","\"+1\"","\"-1\"","\"1.0\"","\"1e0\"","\"18446744073709551616\"","\"\\u00001\"","\"\\ud83d\\ude00\"","\"\\u0661\""};
    char Wire[4300];
    for(size_t N=0;N<sizeof(Unknown)/sizeof(Unknown[0]);++N){int Bytes=snprintf(Wire,sizeof(Wire),"{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":true}",Unknown[N]);
        testRequire(xrpcDuplexReceive(D,(xstrview){Wire,(size_t)Bytes},(xbytesview){0})==XRPC_DUPLEX_ACCEPTED && !xrtFutureDone(F),"unknown ID matched a canonical local ID");}
    char Id[4097];Id[0]='"';memset(Id+1,'9',4094);Id[4095]='"';Id[4096]=0;
    int Bytes=snprintf(Wire,sizeof(Wire),"{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":true}",Id);
    testRequire(xrpcDuplexReceive(D,(xstrview){Wire,(size_t)Bytes},(xbytesview){0})==XRPC_DUPLEX_ACCEPTED && !xrtFutureDone(F),"large valid unknown string required a large pending key slot");
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"\\u0031\",\"result\":true}"),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED,"escaped ID did not equal issued string ID");
    const xrpccallresult* R=duplexTestResult(F,XRPC_CALL_OK,XRPC_REPLIED);xrpcresponseinfo I={0};I.Size=sizeof(I);
    testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[R->Index].Id.Raw.Size==8 && !memcmp(I.Messages[R->Index].Id.Raw.Data,"\"\\u0031\"",8),"correlation normalized retained response bytes");
    duplexTestClose(D);xrpcDuplexOutputRelease(O);xrtFutureDestroy(F);
    D=duplexTestCreate(NULL,&C,NULL);xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("a"),(xstrview){0}},{false,XRT_STR_LITERAL("b"),(xstrview){0}}};
    testRequire(xrpcDuplexBatch(D,Items,2,5000,&F)==XRPC_DUPLEX_ACCEPTED,"escaped duplicate group admit");O=duplexTestSend(D,&T);
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1},{\"jsonrpc\":\"2.0\",\"id\":\"\\u0031\",\"result\":2}]"),(xbytesview){0})==XRPC_DUPLEX_PROTOCOL && !xrtFutureDone(F),"escaped duplicate partially completed a batch");
    duplexTestClose(D);xrpcDuplexOutputRelease(O);xrtFutureDestroy(F);
}
int main(void)
{testDuplexBidirectional();testDuplexStops();testDuplexResponseAtomicity();testDuplexLimitsAndJobs();testDuplexAllocation();testDuplexConcurrency();testDuplexIncomingDocuments();testDuplexFailureAndReentry();testDuplexCompactCorrelation();puts("[PASS] xrpc duplex: bidirectional/shared-executor/batch-map/notifications/atomic-response-admission/stop-delivery/partial-send-close/retained-wire/quota/4096-OOM-prefixes/concurrent-producers-close/complete-request-validation/constructor-OOM/reentry/impossible-reply-bound/compact-escaped-and-unknown-IDs");return 0;}
