#include "test.h"
#include <xrpc/duplex.h>
#include <xrt/thread.h>
#include <math.h>
#include "../examples/duplex/app_dispatch.h"

typedef struct duplexTestContext {xatomic32 Wakes;} duplexTestContext;

static void duplexTestInit(duplexTestContext* Context)
{
    xrtAtomic32Init(&Context->Wakes, 0);
}

static void duplexTestWake(ptr Data)
{
    duplexTestContext* Context = Data;
    xrtAtomic32FetchAdd(&Context->Wakes, 1, XMEMORY_RELAXED);
}

static xrpcduplex* duplexTestCreate(duplexTestContext* Context, const xrpcduplexconfig* Config)
{
    xrpcduplexconfig K;
    if (Config) K = *Config;
    else xrpcDuplexConfigInit(&K);
    K.Wake = duplexTestWake;
    K.User = Context;
    xrpcduplex* D = NULL;
    testRequire(xrpcDuplexCreate(&K, &D), "duplex create");
    return D;
}

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
static xrpcduplexoutput* duplexTestSend(xrpcduplex* D,uint64* Token)
{testRequire(xrpcDuplexStep(D),"duplex send step");xrpcduplexoutput* O=NULL;testRequire(xrpcDuplexNext(D,&O,Token)==1 && xrpcDuplexStarted(D,*Token) && xrpcDuplexSent(D,*Token),"duplex manual send");return O;}
static void testDuplexStops(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(&C,NULL);xfuture* F=NULL;uint64 T=0;xrpcduplexoutput* O=NULL;
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
static void testDuplexCancelledBatchReply(void)
{
	duplexTestContext Context;
	duplexTestInit(&Context);
	xrpcduplex* D = duplexTestCreate(&Context, NULL);
	xfuture *Batch = NULL, *Live = NULL;
	const xrpcduplexitem Items[] = {
		{false, XRT_STR_LITERAL("echo"), (xstrview){0}},
		{false, XRT_STR_LITERAL("echo"), (xstrview){0}}
	};
	testRequire(xrpcDuplexBatch(D, Items, 2, XRT_WAIT_FOREVER, &Batch) == XRPC_DUPLEX_ACCEPTED &&
		xrpcDuplexCall(D, XRT_STR_LITERAL("echo"), (xstrview){0}, XRT_WAIT_FOREVER, &Live) ==
		XRPC_DUPLEX_ACCEPTED, "admit a batch and a separate live call");
	uint64 Token = 0;
	xrpcDuplexOutputRelease(duplexTestSend(D, &Token));
	xrpcDuplexOutputRelease(duplexTestSend(D, &Token));
	testRequire(xrtFutureCancel(Batch) && !xrtFutureDone(Batch), "request batch cancellation before owner step");

	const xstrview Reply = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":\"2\",\"result\":2},"
		"{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1}]");
	testRequire(xrpcDuplexReceive(D, Reply, (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED,
		"receive reordered batch reply before cancellation cleanup");
	const xrpccallresult* Result = duplexTestResult(Batch, XRPC_CALL_CANCELLED, XRPC_REPLIED);
	testRequire(!Result->Responses, "discard cancelled batch payload while recording delivery");
	testRequire(xrpcDuplexReceive(D, Reply, (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED &&
		duplexTestInfo(D).UnknownResponses == 2 && !xrtFutureDone(Live),
		"replayed batch replies have no association with a live call");

	const xstrview LiveReply = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"3\",\"result\":42}");
	testRequire(xrpcDuplexReceive(D, LiveReply, (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED,
		"accept live reply after stale batch replay");
	Result = duplexTestResult(Live, XRPC_CALL_OK, XRPC_REPLIED);
	xrpcresponseinfo Info = {0};
	Info.Size = sizeof(Info);
	testRequire(Result->Responses && xrpcResponsesInfo(Result->Responses, &Info) && Info.Count == 1 && Result->Index == 0 &&
		Info.Messages[Result->Index].Result.Raw.Size == 2 &&
		memcmp(Info.Messages[Result->Index].Result.Raw.Data, "42", 2) == 0, "preserve the live call payload");
	duplexTestClose(D);
	xrtFutureDestroy(Batch);
	xrtFutureDestroy(Live);
}

static void testDuplexResponseSnapshot(void)
{
	static const cstr Replies[] = {
		"{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"error\":{\"code\":-32603,\"message\":\"failed\",\"data\":{\"why\":[1,2]}}}",
		"{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"error\":{\"code\":-32603,\"message\":\"failed\",\"data\":{\"why\":[1,2]}},"
		"\"params\":1,\"params\":[]}"
	};
	for (size_t Index = 0; Index < sizeof(Replies) / sizeof(Replies[0]); ++Index) {
		duplexTestContext Context;
		duplexTestInit(&Context);
		xrpcduplex* D = duplexTestCreate(&Context, NULL);
		xfuture* F = NULL;
		testRequire(xrpcDuplexCall(D, XRT_STR_LITERAL("effect"), (xstrview){0}, XRT_WAIT_FOREVER, &F) ==
			XRPC_DUPLEX_ACCEPTED, "admit a call for the response snapshot");
		uint64 Token = 0;
		xrpcDuplexOutputRelease(duplexTestSend(D, &Token));
		char Input[160];
		size_t Size = strlen(Replies[Index]);
		testRequire(Size <= sizeof(Input), "response fixture fits its mutable buffer");
		memcpy(Input, Replies[Index], Size);
		testRequire(xrpcDuplexReceive(D, (xstrview){Input, Size}, (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED,
			"accept both strict responses and existing response-mode extensions");
		memset(Input, 0, Size);
		duplexTestClose(D);

		const xrpccallresult* Result = duplexTestResult(F, XRPC_CALL_REMOTE_ERROR, XRPC_REPLIED);
		xrpcresponseinfo Info = {0};
		Info.Size = sizeof(Info);
		testRequire(Result->Responses && xrpcResponsesInfo(Result->Responses, &Info) &&
			Info.Count == 1 && Result->Index == 0, "response survives the input and endpoint retirement");
		const xstrview Expected = XRT_STR_LITERAL("{\"why\":[1,2]}");
		const xstrview Retained = Info.Messages[0].ErrorData.Raw;
		testRequire(Retained.Size == Expected.Size && memcmp(Retained.Data, Expected.Data, Expected.Size) == 0,
			"retained error data points into the owned response snapshot");
		xrtFutureDestroy(F);
	}
}

static void testDuplexResponseQuota(void)
{
    duplexTestContext Context;
    duplexTestInit(&Context);
    xrpcduplexconfig Config;
    testRequire(xrpcDuplexConfigInit(&Config), "response quota config");
    Config.MaxResults = 3;
    xrpcduplex* D = duplexTestCreate(&Context, &Config);
    xfuture *Held = NULL, *Live = NULL;
    testRequire(xrpcDuplexCall(D, XRT_STR_LITERAL("held"), (xstrview){0}, XRT_WAIT_FOREVER, &Held) == XRPC_DUPLEX_ACCEPTED &&
        xrpcDuplexCall(D, XRT_STR_LITERAL("live"), (xstrview){0}, XRT_WAIT_FOREVER, &Live) == XRPC_DUPLEX_ACCEPTED,
        "admit two calls sharing the result quota");
    uint64 Token;
    xrpcDuplexOutputRelease(duplexTestSend(D, &Token));
    xrpcDuplexOutputRelease(duplexTestSend(D, &Token));
    testRequire(xrpcDuplexReceive(D, XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1}"),
        (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED, "fill remaining result quota with a retained snapshot");
    duplexTestResult(Held, XRPC_CALL_OK, XRPC_REPLIED);
    testRequire(duplexTestInfo(D).Results == 3, "result quota is full");

    const xstrview Reply = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"2\",\"result\":2}");
    testRequire(xrpcDuplexReceive(D, Reply, (xbytesview){0}) == XRPC_DUPLEX_ERROR && !xrtFutureDone(Live),
        "matched reply can be retried after a result quota refusal");
    xrtClearError();
    uint64 Refusals = duplexTestInfo(D).ResultRefusals;
    const xstrview Stale = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1},"
        "{\"jsonrpc\":\"2.0\",\"id\":\"99\",\"result\":99,\"params\":1,\"params\":[]}]");
    testRequire(xrpcDuplexReceive(D, Stale, (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED,
        "unmatched replies require no result quota");
    xrpcduplexinfo Info = duplexTestInfo(D);
    testRequire(Info.UnknownResponses == 2 && Info.Results == 3 && Info.ResultRefusals == Refusals &&
        !Info.Closing && !xrtFutureDone(Live), "ignored responses leave the live call and quota intact");

    xrtFutureDestroy(Held);
    testRequire(xrpcDuplexReceive(D, Reply, (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED,
        "retry the matched reply after releasing held credit");
    duplexTestResult(Live, XRPC_CALL_OK, XRPC_REPLIED);
    duplexTestClose(D);
    xrtFutureDestroy(Live);
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
    for(size_t K=0;K<sizeof(Bad)/sizeof(Bad[0]);++K){
        duplexTestContext C;duplexTestInit(&C);
        xrpcduplexconfig Config;
        testRequire(xrpcDuplexConfigInit(&Config),"response atomicity config");
        Config.MaxResults=1;
        xrpcduplex* D=duplexTestCreate(&C,&Config);xfuture* F=NULL;
        xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("echo"),(xstrview){0}},{true,XRT_STR_LITERAL("note"),(xstrview){0}},{false,XRT_STR_LITERAL("echo"),(xstrview){0}}};
        testRequire(xrpcDuplexBatch(D,Items,3,5000,&F)==XRPC_DUPLEX_ACCEPTED,"duplex atomic batch admit");uint64 T;xrpcduplexoutput* O=duplexTestSend(D,&T);
        testRequire(xrpcDuplexReceive(D,(xstrview){Bad[K],strlen(Bad[K])},(xbytesview){0})==XRPC_DUPLEX_PROTOCOL && xrtFutureState(F)==XFUTURE_PENDING,"bad response partially completed live batch");
        testRequire(xrpcDuplexStep(D),"duplex protocol close step");duplexTestResult(F,XRPC_CALL_PROTOCOL,XRPC_MAY_HAVE_EXECUTED);duplexTestClose(D);xrpcDuplexOutputRelease(O);xrtFutureDestroy(F);xrtClearError();
    }
    duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(&C,NULL);xfuture* F=NULL;
    testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED && xrpcDuplexStep(D),"duplex unsent admit");
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":true}"),(xbytesview){0})==XRPC_DUPLEX_PROTOCOL,"unsent response accepted");
    duplexTestClose(D);duplexTestResult(F,XRPC_CALL_PROTOCOL,XRPC_NOT_SENT);xrtFutureDestroy(F);
    D=duplexTestCreate(&C,NULL);xrpcduplexitem B[]={{false,XRT_STR_LITERAL("a"),(xstrview){0}},{true,XRT_STR_LITERAL("b"),(xstrview){0}},{false,XRT_STR_LITERAL("c"),(xstrview){0}}};
    testRequire(xrpcDuplexBatch(D,B,3,5000,&F)==XRPC_DUPLEX_ACCEPTED,"duplex reordered admit");uint64 T;xrpcduplexoutput* O=duplexTestSend(D,&T);
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":\"2\",\"result\":2},{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1}]"),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED,"duplex reordered response");
    const xrpcbatchcallresult* R=(const xrpcbatchcallresult*)duplexTestResult(F,XRPC_CALL_BATCH_DONE,XRPC_REPLIED);testRequire(R->Indices[0]==1 && R->Indices[1]==SIZE_MAX && R->Indices[2]==0,"duplex reordered mapping");
    duplexTestClose(D);xrpcDuplexOutputRelease(O);xrtFutureDestroy(F);
}
static void testDuplexAllocation(void)
{
    duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(&C,NULL);unsigned Failed=0,Passed=0;
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
    for(unsigned Race=0;Race<2;++Race){duplexTestContext C;duplexTestInit(&C);
        xrpcduplexconfig K;xrpcDuplexConfigInit(&K);K.MaxWork=1;K.MaxOperations=8;K.MaxIncoming=4;
        xrpcduplex* A=duplexTestCreate(&C,&K),*B=NULL;
        apprequest Job={0};
        xrpcDuplexConfigInit(&K); K.OnRequest=appTakeRequest; K.User=&Job;
        testRequire(xrpcDuplexCreate(&K,&B),"APP hosted concurrent peer");
        xatomic32 Done;xrtAtomic32Init(&Done,0);duplexTestProducer P[4];xthread* T[4];
        for(unsigned N=0;N<4;++N){P[N]=(duplexTestProducer){A,&Done,N,Race!=0};T[N]=xrtThreadCreate(duplexTestProduce,P+N,0);testRequire(T[N]!=NULL,"duplex producer thread");}
        double End=xrtTimer()+20;
        while(xrtAtomic32Load(&Done,XMEMORY_ACQUIRE)!=4){
            testRequire(xrpcDuplexStep(A) && xrpcDuplexStep(B) && xrtTimer()<End,"concurrent duplex owner stalled");
            if(Race){if(duplexTestInfo(A).Accepted>=4)testRequire(xrpcDuplexClose(A,XRPC_CALL_CLOSED),"concurrent admission close");}
            else {duplexTestPump(A,B);testRequire(appFinish(B,&Job),"APP completes deferred echo");duplexTestPump(B,A);}
            xrtSleep(0);
        }
        for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(T[N],5000)==XWAIT_OK,"duplex producer physical join");xrtThreadDestroy(T[N]);}
        duplexTestClose(A);duplexTestClose(B);
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
    xrpcduplex* D=duplexTestCreate(&C,NULL);xfuture* F=NULL;testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("m"),(xstrview){0},0,&F)==XRPC_DUPLEX_ACCEPTED,"reentry call");
    duplexTestReentry R={D,NULL,0};xfuturewatch W;testRequire(xrtFutureWatchInit(&W,duplexTestReenter,NULL,&R) && xrtFutureWatchAdd(F,&W)==XFUTURE_WATCH_PENDING,"reentry watch");
    testRequire(xrpcDuplexStep(D) && R.Calls==1 && R.Extra,"reentry completion publication");duplexTestClose(D);xrtFutureDestroy(F);xrtFutureDestroy(R.Extra);
    xrpcduplexconfig K;xrpcDuplexConfigInit(&K);K.MaxFixedBytes=1;D=(xrpcduplex*)(uintptr_t)1;
    testRequire(!xrpcDuplexCreate(&K,&D) && D==(xrpcduplex*)(uintptr_t)1,"fixed-storage bound changed output");xrtClearError();
}
static void testDuplexCompactCorrelation(void)
{
    duplexTestContext C;duplexTestInit(&C);xrpcduplex* D=duplexTestCreate(&C,NULL);
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
    D=duplexTestCreate(&C,NULL);xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("a"),(xstrview){0}},{false,XRT_STR_LITERAL("b"),(xstrview){0}}};
    testRequire(xrpcDuplexBatch(D,Items,2,5000,&F)==XRPC_DUPLEX_ACCEPTED,"escaped duplicate group admit");O=duplexTestSend(D,&T);
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1},{\"jsonrpc\":\"2.0\",\"id\":\"\\u0031\",\"result\":2}]"),(xbytesview){0})==XRPC_DUPLEX_PROTOCOL && !xrtFutureDone(F),"escaped duplicate partially completed a batch");
    duplexTestClose(D);xrpcDuplexOutputRelease(O);xrtFutureDestroy(F);
}
static void testDuplexSetupTimeout(void)
{
    for(unsigned Reason=0;Reason<2;++Reason)for(unsigned Batch=0;Batch<2;++Batch)for(unsigned Mode=0;Mode<3;++Mode){
        duplexTestContext C;duplexTestInit(&C);xrpcduplexconfig K;xrpcDuplexConfigInit(&K);K.MaxWork=1;xrpcduplex* D=duplexTestCreate(&C,&K);xfuture* F=NULL;
        xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("a"),(xstrview){0}},{true,XRT_STR_LITERAL("note"),(xstrview){0}}};
        testRequire((Batch?xrpcDuplexBatch(D,Items,2,60000,&F):xrpcDuplexCall(D,XRT_STR_LITERAL("a"),(xstrview){0},60000,&F))==XRPC_DUPLEX_ACCEPTED,"setup timeout long-lived call/batch");
        xrpcduplexoutput* O=NULL;uint64 Token=0;double End=xrtTimer()+1;int Selected=0;
        while(!(Selected=xrpcDuplexNext(D,&O,&Token))){testRequire(xrpcDuplexStep(D)&&xrtTimer()<End,"setup timeout bounded selection");}
        testRequire(Selected==1&&!xrtFutureDone(F),"setup timeout does not depend on call's later deadline");
        if(Mode)testRequire(xrpcDuplexStarted(D,Token),"setup timeout preserves conservative started delivery");
        if(Mode==2)testRequire(xrpcDuplexSent(D,Token),"setup timeout sent boundary");
        xrpccallstatus Status=Reason?XRPC_CALL_HTTP_ERROR:XRPC_CALL_TIMEOUT;testRequire(xrpcDuplexClose(D,Status),"adapter setup failure is a valid typed close reason");
        while(!duplexTestInfo(D).Drained){testRequire(xrpcDuplexStep(D)&&xrtTimer()<End,"setup timeout promptly drains every owner unit");}
        const xrpccallresult* R=duplexTestResult(F,Status,Mode?XRPC_MAY_HAVE_EXECUTED:XRPC_NOT_SENT);testRequire(!R->Responses,"setup timeout retains no fabricated response");
        if(Batch){const xrpcbatchcallresult* B=(const xrpcbatchcallresult*)R;testRequire(B->Count==2&&B->Indices[0]==SIZE_MAX&&B->Indices[1]==SIZE_MAX,"timeout batch map has no reply indices");}
        testRequire(xrpcDuplexDestroy(D)&&xrpcDuplexOutputWire(O).Size,"retained output outlives setup timeout and duplex destruction");xrpcDuplexOutputRelease(O);xrtFutureDestroy(F);
    }
}
static xrpcduplexinfo duplexTestQuiet(xrpcduplex* D)
{
    double End=xrtTimer()+5;
    xrpcduplexinfo I=duplexTestInfo(D);
    while(I.More){
        testRequire(xrpcDuplexStep(D) && xrtTimer()<End,"bounded cohort reaches quiescence");
        I=duplexTestInfo(D);
    }
    return I;
}
static void testDuplexIndexedDeadlines(void)
{
    enum {Window=128};
    xfuture* Older[Window]={0};
    duplexTestContext C;duplexTestInit(&C);
    xrpcduplexconfig K;xrpcDuplexConfigInit(&K);
    K.MaxOperations=Window+1;K.MaxWork=1;
    xrpcduplex* D=duplexTestCreate(&C,&K);
    for(size_t N=0;N<Window;++N)
        testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("held"),(xstrview){0},N?60000:30000,Older+N)==XRPC_DUPLEX_ACCEPTED,
                    "populate mixed-deadline pending window");
    double First=duplexTestQuiet(D).NextDeadline;
    testRequire(First>xrtTimer(),"initial finite minimum");
    for(size_t N=0;N<Window;++N){
        xrpcduplexoutput* O=NULL;uint64 Token;
        testRequire(xrpcDuplexNext(D,&O,&Token)==1 && xrpcDuplexStarted(D,Token) && xrpcDuplexSent(D,Token),
                    "fill actual in-flight window");
        xrpcDuplexOutputRelease(O);
    }
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"64\",\"result\":1}"),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED,
                "retire a nonminimum deadline on reply");
    duplexTestResult(Older[63],XRPC_CALL_OK,XRPC_REPLIED);
    testRequire(duplexTestInfo(D).NextDeadline==First,"nonminimum reply preserves exact minimum");
    testRequire(xrpcDuplexReceive(D,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":1}"),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED,
                "retire the minimum deadline on reply");
    duplexTestResult(Older[0],XRPC_CALL_OK,XRPC_REPLIED);
    double Remaining=duplexTestInfo(D).NextDeadline;
    testRequire(Remaining>First,"minimum advances after reply");

    xfuture* Short=NULL;
    double Entered=xrtTimer();
    testRequire(xrpcDuplexCall(D,XRT_STR_LITERAL("expire"),(xstrview){0},350,&Short)==XRPC_DUPLEX_ACCEPTED,
                "add an earlier deadline at the end of the cohort");
    double Latest=xrtTimer()+.351;
    double Due=duplexTestQuiet(D).NextDeadline;
    testRequire(Due>=Entered+.349 && Due<=Latest && !xrtFutureDone(Short),"next deadline uses the native call clock");
    while(xrtTimer()<Due)xrtSleep(1);
    for(unsigned Turn=0;Turn<2 && !xrtFutureDone(Short);++Turn)
        testRequire(xrpcDuplexStep(D),"service due call within two MaxWork=1 turns");
    testRequire(xrtFutureDone(Short),"tail deadline completes within two owner turns");
    duplexTestResult(Short,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);
    testRequire(!duplexTestInfo(D).Closing,"unsent expiration leaves the channel usable");
    testRequire(duplexTestQuiet(D).NextDeadline==Remaining,"expiration exposes surviving minimum");

    xcancel* Raw=xrtFutureCancelToken(Older[Window-1]);
    testRequire(Raw && xrtCancelRequest(Raw),"request raw cancellation on a later call");
    xrtCancelDestroy(Raw);
    testRequire(xrpcDuplexStep(D),"start bounded raw cancellation cohort");
    duplexTestQuiet(D);
    duplexTestResult(Older[Window-1],XRPC_CALL_CANCELLED,XRPC_MAY_HAVE_EXECUTED);
    duplexTestClose(D);
    for(size_t N=0;N<Window;++N)xrtFutureDestroy(Older[N]);
    xrtFutureDestroy(Short);
}
typedef struct duplexHostTest {
    uint64 Token;
    size_t Requests;
    size_t Count;
    bool ParseError;
    bool Reject;
} duplexHostTest;

static bool duplexHostRequest(uint64 Token, xstrview Input, xbytesview Context,
    const xrpcdocument* Document, const xrpcmessage* Messages, ptr User)
{
    duplexHostTest* Host = User;
    testRequire(Token && Input.Size && Context.Size == 1 &&
        Context.Data[0] == 42 && (Messages != NULL) == (Document != NULL),
        "host receives borrowed complete request and APP context");
    if (Host->Reject) return false;
    Host->Token = Token;
    Host->Count = Document ? Document->Count : 0;
    Host->ParseError = !Document;
    ++Host->Requests;
    return true;
}

static void testDuplexHost(void)
{
    duplexHostTest Host = {0};
    xrpcduplexconfig Config;
    testRequire(xrpcDuplexConfigInit(&Config), "host config");
    Config.OnRequest = duplexHostRequest;
    Config.User = &Host;
    Config.MaxIncoming = 1;
    Config.MaxOutputs = 1;
    xrpcduplex *Client = NULL, *Server = NULL;
    testRequire(xrpcDuplexCreate(NULL, &Client) && xrpcDuplexCreate(&Config, &Server),
        "host endpoints need no executor instance");
    unsigned char Byte = 42;
    xbytesview Context = {&Byte, 1};

    xfuture* Future = NULL;
    testRequire(xrpcDuplexCall(Client, XRT_STR_LITERAL("echo"), XRT_STR_LITERAL("[42]"),
        5000, &Future) == XRPC_DUPLEX_ACCEPTED, "host peer call");
    uint64 Selection = 0;
    xrpcduplexoutput* Wire = duplexTestSend(Client, &Selection);
    testRequire(xrpcDuplexReceive(Server, xrpcDuplexOutputWire(Wire), Context) ==
        XRPC_DUPLEX_ACCEPTED && Host.Count == 1 && Host.Requests == 1,
        "host observes parsed call");
    xrpcDuplexOutputRelease(Wire);

    const xstrview Request = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"ping\"}");
    testRequire(xrpcDuplexReceive(Server, Request, Context) == XRPC_DUPLEX_OVERLOADED &&
        Host.Requests == 1, "host bounded pending tokens");
    const xstrview Wrong = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"other\",\"result\":0}");
    uint64 Token = Host.Token;
    testRequire(xrpcDuplexReply(Server, Token, Wrong) == XRPC_DUPLEX_ERROR &&
        duplexTestInfo(Server).Incoming == 1, "mismatched reply preserves token");
    xrtClearError();
    const xstrview Reply = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":42}");
    testRequire(xrpcDuplexReply(Server, Token, Reply) == XRPC_DUPLEX_ACCEPTED &&
        xrpcDuplexReply(Server, Token, Reply) == XRPC_DUPLEX_ERROR, "host reply consumes token once");
    xrtClearError();
    Wire = duplexTestSend(Server, &Selection);
    testRequire(xrpcDuplexReceive(Client, xrpcDuplexOutputWire(Wire), (xbytesview){0}) ==
        XRPC_DUPLEX_ACCEPTED && xrpcDuplexStep(Client), "host response reaches other endpoint");
    duplexTestResult(Future, XRPC_CALL_OK, XRPC_REPLIED);
    xrtFutureDestroy(Future);

    Host.Reject = true;
    testRequire(xrpcDuplexReceive(Server, Request, Context) == XRPC_DUPLEX_OVERLOADED &&
        !duplexTestInfo(Server).Incoming, "host rejection rolls back reservation");
    Host.Reject = false;
    testRequire(xrpcDuplexReceive(Server, Request, Context) == XRPC_DUPLEX_ACCEPTED &&
        xrpcDuplexReply(Server, Host.Token, (xstrview){0}) == XRPC_DUPLEX_ACCEPTED,
        "notification succeeds without output");

    const xstrview Batch = XRT_STR_LITERAL(
        "[{\"jsonrpc\":\"2.0\",\"method\":\"call\",\"id\":71},{\"jsonrpc\":\"2.0\",\"method\":\"note\"}]");
    const xstrview BatchReply = XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":71,\"result\":true}]");
    testRequire(xrpcDuplexReceive(Server, Batch, Context) == XRPC_DUPLEX_ACCEPTED &&
        Host.Count == 2 && xrpcDuplexReply(Server, Host.Token, BatchReply) ==
        XRPC_DUPLEX_OVERLOADED && duplexTestInfo(Server).Incoming == 1,
        "retained output backpressure preserves deferred batch");
    xrpcDuplexOutputRelease(Wire);
    testRequire(xrpcDuplexReply(Server, Host.Token, BatchReply) == XRPC_DUPLEX_ACCEPTED,
        "retry host reply after output credit returns");
    Wire = duplexTestSend(Server, &Selection);
    xrpcDuplexOutputRelease(Wire);

    testRequire(xrpcDuplexReceive(Server, XRT_STR_LITERAL("["), Context) == XRPC_DUPLEX_ACCEPTED &&
        Host.ParseError && !Host.Count, "host handles malformed JSON independently");
    const xstrview ParseReply = XRT_STR_LITERAL(
        "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}");
    testRequire(xrpcDuplexReply(Server, Host.Token, ParseReply) == XRPC_DUPLEX_ACCEPTED,
        "parse error accepts null ID");
    Wire = duplexTestSend(Server, &Selection);
    xrpcDuplexOutputRelease(Wire);
    testRequire(xrpcDuplexReceive(Server, Batch, Context) == XRPC_DUPLEX_ACCEPTED,
        "last host job remains open for close test");
    duplexTestClose(Server);
    duplexTestClose(Client);
}

static void testDuplexHostTokenReuse(size_t Limit)
{
    duplexHostTest Host = {0};
    xrpcduplexconfig Config;
    testRequire(xrpcDuplexConfigInit(&Config), "indexed host config");
    Config.MaxIncoming = Config.MaxOperations = Limit;
    Config.MaxControlBytes = 8u * 1024u * 1024u;
    Config.MaxWork = 1;
    xrpcduplex* Client = NULL;
    testRequire(xrpcDuplexCreate(&Config, &Client), "client without host slots");
    size_t ClientBytes = duplexTestInfo(Client).OwnedBytes;
    duplexTestClose(Client);

    Config.OnRequest = duplexHostRequest;
    Config.User = &Host;
    xrpcduplexconfig Limited = Config;
    Limited.MaxFixedBytes = ClientBytes;
    xrpcduplex* Server = NULL;
    testRequire(!xrpcDuplexCreate(&Limited, &Server) && !Server,
        "host index obeys fixed storage budget");
    xrtClearError();
    testRequire(xrpcDuplexCreate(&Config, &Server) &&
        duplexTestInfo(Server).OwnedBytes > ClientBytes, "reserve host index");

    unsigned char Byte = 42;
    xbytesview Context = {&Byte, 1};
    xstrview Note = XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"ping\"}");
    xstrview Empty = {0};
    uint64 Middle = 0;
    for (size_t Index = 0; Index < Limit; ++Index) {
        testRequire(xrpcDuplexReceive(Server, Note, Context) == XRPC_DUPLEX_ACCEPTED,
            "fill incoming host slots");
        if (Index == Limit / 2) Middle = Host.Token;
    }
    uint64 Last = Host.Token;
    testRequire(xrpcDuplexReceive(Server, Note, Context) == XRPC_DUPLEX_OVERLOADED &&
        duplexTestInfo(Server).Incoming == Limit, "host slots enforce admission limit");
    testRequire(xrpcDuplexReply(Server, Last, Empty) == XRPC_DUPLEX_ACCEPTED &&
        xrpcDuplexReply(Server, Middle, Empty) == XRPC_DUPLEX_ACCEPTED,
        "retire newest and middle jobs out of order");
    testRequire(xrpcDuplexReceive(Server, Note, Context) == XRPC_DUPLEX_ACCEPTED &&
        xrpcDuplexReceive(Server, Note, Context) == XRPC_DUPLEX_ACCEPTED,
        "two released slots accept new jobs");
    testRequire(xrpcDuplexReply(Server, Middle, Empty) == XRPC_DUPLEX_ERROR &&
        xrpcDuplexReply(Server, Last, Empty) == XRPC_DUPLEX_ERROR &&
        duplexTestInfo(Server).Incoming == Limit, "stale tokens cannot consume reused slots");
    xrtClearError();
    testRequire(xrpcDuplexReply(Server, Host.Token, Empty) == XRPC_DUPLEX_ACCEPTED,
        "current token remains usable after stale reply");
    testRequire(xrpcDuplexClose(Server, XRPC_CALL_CLOSED) &&
        xrpcDuplexReply(Server, Host.Token, Empty) == XRPC_DUPLEX_CLOSED,
        "closed endpoint refuses APP replies");
    for (size_t Remaining = Limit - 1; Remaining; --Remaining) {
        testRequire(xrpcDuplexStep(Server) &&
            duplexTestInfo(Server).Incoming == Remaining - 1, "bounded host slot retirement");
    }
    testRequire(duplexTestInfo(Server).Drained, "indexed host drain finishes");
    duplexTestClose(Server);
}

int main(void)
{
    testDuplexHost();
    testDuplexHostTokenReuse(16);
    testDuplexHostTokenReuse(65536);
    testDuplexStops();
    testDuplexCancelledBatchReply();
    testDuplexResponseSnapshot();
    testDuplexResponseQuota();
    testDuplexResponseAtomicity();
    testDuplexAllocation();
    testDuplexConcurrency();
    testDuplexFailureAndReentry();
    testDuplexCompactCorrelation();
    testDuplexSetupTimeout();
    testDuplexIndexedDeadlines();
    puts("[PASS] xrpc duplex: APP reply tokens, wire/backpressure, exact IDs, typed stops, OOM, producers, lifetime and deadlines");
    return 0;
}
