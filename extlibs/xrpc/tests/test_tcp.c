#include "test.h"
#include <xrpc/tcp.h>
#include <xrt/thread.h>
#include <string.h>
typedef struct tcpTest {
    xnetengine* Engine;xnetlistener* Listener;xrpcserver* Server;xrpctcp *Left,*Right;
    xrpctcpconfig Config;xatomic32 Ready,Calls,Held,Permit;char Context[4];
} tcpTest;
static bool tcpTestMethod(const xrpcmessage* M,cstr Name)
{size_t N=strlen(Name);return M->Method.Raw.Size==N+2 && !memcmp(M->Method.Raw.Data+1,Name,N);}
static bool tcpTestBusiness(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* Context,ptr User)
{
    tcpTest* T=User;testRequire(!xrtNetEngineCurrent(T->Engine),"TCP business executed on a network worker");
    testRequire(Context->Data.Size==4 && !memcmp(Context->Data.Data,"meta",4),"TCP request context not independently copied");
    xrtAtomic32FetchAdd(&T->Calls,1,XMEMORY_RELAXED);
    if(tcpTestMethod(M,"hold")){xrtAtomic32Store(&T->Held,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&T->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
    if(tcpTestMethod(M,"error"))return xrpcReplyError(R,XRT_STR_LITERAL("-100"),XRT_STR_LITERAL("error"),(xstrview){0});
    return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}
static bool tcpTestAccept(xnetlistener* L,xnetstream* S,ptr User)
{
    (void)L;tcpTest* T=User;bool OK=xrpcTcpAttach(S,&T->Config,&T->Right);
    if(OK){testRequire(xrpcTcpWaitFor(T->Right,0)==XWAIT_ERROR && !xrpcTcpDestroy(T->Right),"TCP self-worker wait/destroy accepted");xrtClearError();xrtAtomic32Store(&T->Ready,1,XMEMORY_RELEASE);}return OK;
}
static void tcpTestStartEx(tcpTest* T,unsigned Format,bool Fragment,size_t TimerLimit)
{
    memset(T,0,sizeof(*T));xrtAtomic32Init(&T->Ready,0);xrtAtomic32Init(&T->Calls,0);xrtAtomic32Init(&T->Held,0);xrtAtomic32Init(&T->Permit,0);memcpy(T->Context,"meta",4);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;if(TimerLimit)E.TimerLimit=TimerLimit;T->Engine=xrtNetEngineCreate(&E);testRequire(T->Engine && xrtNetEngineStart(T->Engine),"TCP Engine start");
    xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=tcpTestBusiness;S.User=T;testRequire(xrpcServerCreate(&S,&T->Server),"TCP shared executor");
    xrpcTcpConfigInit(&T->Config);T->Config.Duplex.Server=T->Server;T->Config.RequestContext=(xbytesview){(const unsigned char*)T->Context,4};T->Config.Frame.Format=(xrpcframeformat)Format;
    if(Fragment){T->Config.ReadChunkBytes=3;T->Config.ReadBytes=7;T->Config.WriteChunkBytes=1;T->Config.WriteBytes=5;T->Config.WorkFrames=2;T->Config.Stream.WriteHighWater=32;T->Config.Stream.WriteLowWater=16;T->Config.Stream.WriteLimit=64;}
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);L.Stream=T->Config.Stream;
    xnetlistenerevents Events={tcpTestAccept,NULL,NULL};T->Listener=xrtNetListen(T->Engine,&L,&Events,NULL,T);testRequire(T->Listener!=NULL,"TCP listen");xnetaddr Remote;
    testRequire(xrtNetListenerLocal(T->Listener,&Remote) && xrpcTcpConnect(T->Engine,&Remote,1,&T->Config,&T->Left),"TCP numeric connect");
    double End=xrtTimer()+10;while(!xrtAtomic32Load(&T->Ready,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TCP accept did not attach");xrtSleep(1);}memset(T->Context,'x',4);
}
static void tcpTestStart(tcpTest* T,unsigned Format,bool Fragment)
{tcpTestStartEx(T,Format,Fragment,0);}
static const xrpccallresult* tcpTestResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{xfutureresult R;testRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"TCP typed Future completion");const xrpccallresult* C=R.Value;testRequire(C->Status==Status && C->Delivery==Delivery,"TCP result status/delivery");return C;}
static void tcpTestFinish(tcpTest* T)
{
    testRequire(xrpcTcpClose(T->Left) && xrpcTcpClose(T->Right),"TCP close");
    testRequire(xrpcTcpWaitFor(T->Left,10000)==XWAIT_OK && xrpcTcpWaitFor(T->Right,10000)==XWAIT_OK,"TCP physical peer retirement");
    xrpctcpinfo I={0};I.Size=sizeof(I);testRequire(xrpcTcpInfo(T->Left,&I) && I.State==XRPC_TCP_CLOSED && !I.Submitters && I.Duplex.Drained && !I.PostFailures,"TCP close accounting");
    testRequire(xrpcTcpDestroy(T->Left) && xrpcTcpDestroy(T->Right),"TCP destroy");
    testRequire(xrtNetListenerClose(T->Listener),"TCP listener close");xrtNetListenerDestroy(T->Listener);testRequire(xrpcServerDestroy(T->Server),"TCP executor physical join");
    double End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(T->Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"TCP engine cleanup/pin/handle leak");xrtSleep(1);}testRequire(Q==XNET_RETIRE_READY,"TCP engine physical join");
}
static void testTcpDuplex(void)
{
    for(unsigned Format=1;Format<=2;++Format){tcpTest T;tcpTestStart(&T,Format,true);xfuture* F[8];
        char Method[]="echo",Params[]="[1,2,3]";
        testRequire(xrpcTcpCall(T.Left,(xstrview){Method,4},(xstrview){Params,7},10000,F)==XRPC_DUPLEX_ACCEPTED,"TCP call admit");memset(Method,'x',4);memset(Params,'x',7);
        testRequire(xrpcTcpCall(T.Right,XRT_STR_LITERAL("error"),(xstrview){0},10000,F+1)==XRPC_DUPLEX_ACCEPTED,"TCP reverse call admit");
        testRequire(xrpcTcpNotify(T.Left,XRT_STR_LITERAL("note"),(xstrview){0},10000,F+2)==XRPC_DUPLEX_ACCEPTED,"TCP notify admit");
        xrpcduplexitem Batch[]={{false,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]")},{true,XRT_STR_LITERAL("note"),(xstrview){0}},{false,XRT_STR_LITERAL("error"),(xstrview){0}}};
        testRequire(xrpcTcpBatch(T.Right,Batch,3,10000,F+3)==XRPC_DUPLEX_ACCEPTED,"TCP batch admit");
        const xrpccallresult* R=tcpTestResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);xrpcresponseinfo I={0};I.Size=sizeof(I);
        testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[R->Index].Result.Raw.Size==7 && !memcmp(I.Messages[R->Index].Result.Raw.Data,"[1,2,3]",7),"TCP input snapshot/correlation");
        tcpTestResult(F[1],XRPC_CALL_REMOTE_ERROR,XRPC_REPLIED);tcpTestResult(F[2],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
        const xrpcbatchcallresult* B=(const xrpcbatchcallresult*)tcpTestResult(F[3],XRPC_CALL_BATCH_DONE,XRPC_REPLIED);testRequire(B->Count==3 && B->Indices[0]==0 && B->Indices[1]==SIZE_MAX && B->Indices[2]==1,"TCP batch notification map");
        testRequire(xrpcTcpCall(T.Left,XRT_STR_LITERAL("echo"),(xstrview){0},0,F+4)==XRPC_DUPLEX_ACCEPTED,"TCP zero deadline admit");tcpTestResult(F[4],XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);
        tcpTestFinish(&T);tcpTestResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);for(size_t N=0;N<5;++N)xrtFutureDestroy(F[N]);
    }
}
static void testTcpStops(void)
{
    tcpTest T;tcpTestStart(&T,1,false);xfuture* F=NULL;
    testRequire(xrpcTcpCall(T.Left,XRT_STR_LITERAL("hold"),(xstrview){0},1000,&F)==XRPC_DUPLEX_ACCEPTED,"TCP hold call");
    double End=xrtTimer()+5;while(!xrtAtomic32Load(&T.Held,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TCP business did not enter");xrtSleep(1);}
    testRequire(xrtFutureCancel(F),"TCP sent cancellation");tcpTestResult(F,XRPC_CALL_CANCELLED,XRPC_MAY_HAVE_EXECUTED);xrtFutureDestroy(F);
    testRequire(xrpcTcpClose(T.Right) && xrpcTcpWaitFor(T.Right,0)==XWAIT_TIMEOUT && !xrpcTcpDestroy(T.Right),"TCP close ignored held business duty");xrtClearError();
    xrtAtomic32Store(&T.Permit,1,XMEMORY_RELEASE);tcpTestFinish(&T);
}

typedef struct tcpProducer {xrpctcp* Peer;unsigned Index;} tcpProducer;
static int32 tcpTestProduce(ptr Data)
{
    tcpProducer* P=Data;
    for(unsigned K=0;K<80;++K){char Params[48];int N=snprintf(Params,sizeof(Params),"[%u,%u]",P->Index,K);xfuture* F=NULL;
        testRequire(xrpcTcpCall(P->Peer,XRT_STR_LITERAL("echo"),(xstrview){Params,(size_t)N},5000,&F)==XRPC_DUPLEX_ACCEPTED,"TCP concurrent admission");
        if(K%7==0)(void)xrtFutureCancel(F);
        xfutureresult V;testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureResult(F,&V),"TCP concurrent completion");
        const xrpccallresult* R=V.Value;testRequire(R && (R->Status==XRPC_CALL_OK || R->Status==XRPC_CALL_CANCELLED),"TCP concurrent result");
        if(R->Status==XRPC_CALL_OK){xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[R->Index].Result.Raw.Size==(size_t)N && !memcmp(I.Messages[R->Index].Result.Raw.Data,Params,(size_t)N),"TCP concurrent correlation");}
        xrtFutureDestroy(F);
    }return 0;
}
static void testTcpConcurrency(void)
{
    tcpTest T;tcpTestStart(&T,2,false);tcpProducer P[4];xthread* Workers[4];
    for(unsigned K=0;K<4;++K){P[K]=(tcpProducer){K&1?T.Right:T.Left,K};Workers[K]=xrtThreadCreate(tcpTestProduce,P+K,0);testRequire(Workers[K]!=NULL,"TCP producer start");}
    for(unsigned K=0;K<4;++K){testRequire(xrtThreadWaitFor(Workers[K],15000)==XWAIT_OK,"TCP producer join");xrtThreadDestroy(Workers[K]);}tcpTestFinish(&T);
}
typedef struct tcpRaw {tcpTest T;xnetstream* Stream;xrpcframereader* Reader;unsigned Mode;xatomic32 Open;} tcpRaw;
static void tcpRawOpen(xnetstream* S,ptr Data)
{
    tcpRaw* R=Data;static const char* const Faults[]={"Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}","Content-Length: 1024\r\n\r\n","Content-Length: 9\r\n\r\n{}","Content-Leng"};
    if(R->Mode<4){testRequire(xrtNetStreamSend(S,Faults[R->Mode],strlen(Faults[R->Mode]))==XNET_RESULT_OK,"TCP raw send");if(R->Mode==2)testRequire(xrtNetStreamShutdownWrite(S),"TCP raw EOF");}
    xrtAtomic32Store(&R->Open,1,XMEMORY_RELEASE);
}
static void tcpRawRead(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    (void)Buffer;tcpRaw* R=Data;xrtNetStreamPause(S);unsigned char DataBytes[4096];size_t N;
    while((N=xrtNetStreamRead(S,DataBytes,sizeof(DataBytes)))!=0){
        if(R->Mode!=4)continue;
        size_t Offset=0;while(Offset<N){size_t Used=0;xrpcframeview F;xrpcframestatus Q=xrpcFrameReaderFeed(R->Reader,(xbytesview){DataBytes+Offset,N-Offset},&Used,&F);Offset+=Used;
            if(Q!=XRPC_FRAME_MORE && Q!=XRPC_FRAME_READY){fprintf(stderr,"raw frame status=%d used=%zu n=%zu offset=%zu error=%s bytes=",Q,Used,N,Offset,xrtErrorMessage(xrtGetError()));for(size_t Z=0;Z<N;++Z)fprintf(stderr,"%02x",DataBytes[Z]);fputc(10,stderr);}
            testRequire(Q==XRPC_FRAME_MORE || Q==XRPC_FRAME_READY,"TCP raw outgoing frame");
            if(Q==XRPC_FRAME_READY){static const char Reply[]="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":[9]}";unsigned char Header[64];size_t Bytes=0;xrpcframeconfig C;xrpcFrameConfigInit(&C);testRequire(xrpcFrameHeaderWrite(sizeof(Reply)-1,&C,Header,sizeof(Header),&Bytes) && xrtNetStreamSend(S,Header,Bytes)==XNET_RESULT_OK && xrtNetStreamSend(S,Reply,sizeof(Reply)-1)==XNET_RESULT_OK && xrtNetStreamShutdownWrite(S),"TCP final response+EOF");}
        }
    }
    (void)xrtNetStreamResume(S);
}
static void testTcpFaults(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){tcpRaw R;memset(&R,0,sizeof(R));R.Mode=Mode;tcpTest* T=&R.T;xrtAtomic32Init(&T->Ready,0);xrtAtomic32Init(&R.Open,0);
        xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;T->Engine=xrtNetEngineCreate(&E);testRequire(T->Engine && xrtNetEngineStart(T->Engine),"TCP raw engine");
        xrpcTcpConfigInit(&T->Config);T->Config.ReadChunkBytes=3;T->Config.ReadBytes=7;T->Config.WorkFrames=2;T->Config.Frame.MaxBodyBytes=256;T->Config.FrameTimeout=Mode==3?40:5000;
        testRequire(xrpcFrameReaderCreate(NULL,&R.Reader),"TCP raw reader");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents A={tcpTestAccept,NULL,NULL};T->Listener=xrtNetListen(T->Engine,&L,&A,NULL,T);xnetaddr Remote;testRequire(T->Listener && xrtNetListenerLocal(T->Listener,&Remote),"TCP raw listener");
        xnetstreamevents Events={tcpRawOpen,tcpRawRead,NULL,NULL,NULL,NULL,NULL};R.Stream=xrtNetStreamConnect(T->Engine,&Remote,1,NULL,&Events,&R);testRequire(R.Stream!=NULL,"TCP raw connect");
        double End=xrtTimer()+5;while(!xrtAtomic32Load(&T->Ready,XMEMORY_ACQUIRE) || !xrtAtomic32Load(&R.Open,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TCP raw attach/open");xrtSleep(1);}
        xfuture* F=NULL;if(Mode==4){testRequire(xrpcTcpCall(T->Right,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"TCP final response call");tcpTestResult(F,XRPC_CALL_OK,XRPC_REPLIED);}
        testRequire(xrpcTcpWaitFor(T->Right,5000)==XWAIT_OK,"TCP fault retirement");xrpctcpinfo I={0};I.Size=sizeof(I);testRequire(xrpcTcpInfo(T->Right,&I),"TCP fault info");
        xrpctcpreason Expected=Mode==0 || Mode==2?XRPC_TCP_PROTOCOL:Mode==1?XRPC_TCP_LIMIT:Mode==3?XRPC_TCP_TIMEOUT:XRPC_TCP_TRANSPORT;testRequire(I.Reason==Expected,"TCP fault classification");
        testRequire(xrpcTcpDestroy(T->Right) && xrtNetStreamAbort(R.Stream),"TCP fault cleanup");End=xrtTimer()+5;while(xrtNetStreamPollRetired(R.Stream)==XWAIT_TIMEOUT){testRequire(xrtTimer()<End,"TCP raw retirement");xrtSleep(1);}xrtNetStreamDestroy(R.Stream);xrpcFrameReaderDestroy(R.Reader);xrtFutureDestroy(F);
        xrtNetListenerClose(T->Listener);xrtNetListenerDestroy(T->Listener);End=xrtTimer()+5;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(T->Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"TCP raw engine tail");xrtSleep(1);}testRequire(Q==XNET_RETIRE_READY,"TCP raw engine join");
    }
}
static void tcpOccupiedTimer(xnetworker* W,uint64 Id,xnetresult Result,ptr Data)
{(void)W;(void)Id;(void)Result;(void)Data;}
typedef struct tcpTimerFill {xnetpost First,Second;xatomic32 Filled;} tcpTimerFill;
static void tcpTestFillTimers(xnetworker* W,ptr Data)
{
    tcpTimerFill* F=Data;
    for(unsigned N=0;N<2;++N)testRequire(xrtNetEngineAfter(xrtNetWorkerEngine(W),xrtNetWorkerIndex(W),60000,tcpOccupiedTimer,NULL)!=0,"TCP actual owner timer insertion");
    xrtAtomic32Store(&F->Filled,1,XMEMORY_RELEASE);
}
static void tcpTestTimerBarrier(xnetworker* W,ptr Data)
{
    tcpTimerFill* F=Data;
    /* OnOpen can publish OPEN before its Wake is posted. A second FIFO turn
     * follows that Wake, so the idle peer has cancelled its connect timer. */
    testRequire(xrtNetPost(W,&F->Second,tcpTestFillTimers,F),"TCP timer second barrier");
}
static void testTcpTimerExhaustion(void)
{
    tcpTest T;tcpTestStartEx(&T,1,false,2);
    double ReadyEnd=xrtTimer()+5;xrpctcpinfo Ready={0};Ready.Size=sizeof(Ready);
    do{testRequire(xrpcTcpInfo(T.Left,&Ready) && xrtTimer()<ReadyEnd,"TCP timer left open");xrtSleep(1);}while(Ready.State!=XRPC_TCP_OPEN);
    tcpTimerFill Fill;xrtAtomic32Init(&Fill.Filled,0);testRequire(xrtNetPostInit(&Fill.First) && xrtNetPostInit(&Fill.Second) && xrtNetPost(xrtNetEngineWorker(T.Engine,1),&Fill.First,tcpTestTimerBarrier,&Fill),"TCP timer owner barrier");
    while(!xrtAtomic32Load(&Fill.Filled,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<ReadyEnd,"TCP owner timer quota not filled");xrtSleep(1);}
    xfuture* F=NULL;testRequire(xrpcTcpCall(T.Left,XRT_STR_LITERAL("hold"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"TCP timer fault call");
    double End=xrtTimer()+5;xrpctcpinfo I={0};I.Size=sizeof(I);do{testRequire(xrpcTcpInfo(T.Left,&I) && xrtTimer()<End,"TCP timer fault not observed");xrtSleep(1);}while(!I.TimerFailures);
    testRequire(I.Reason==XRPC_TCP_LIMIT,"TCP timer quota reason");xrtAtomic32Store(&T.Permit,1,XMEMORY_RELEASE);tcpTestFinish(&T);xrtFutureDestroy(F);
}
static void testTcpTimerAndFactory(void)
{
    xnetengineconfig C;xrtNetEngineConfigInit(&C);C.Workers=1;C.TimerLimit=1;xnetengine* E=xrtNetEngineCreate(&C);testRequire(E && xrtNetEngineStart(E),"TCP timer engine");
    testRequire(xrtNetEngineAfter(E,0,60000,tcpOccupiedTimer,NULL)!=0,"TCP occupy timer quota");
    xnetaddr Remote;xrtNetAddrParse(&Remote,"127.0.0.1",9);xrpctcpconfig K;xrpcTcpConfigInit(&K);K.Stream.ConnectTimeout=0;xrpctcp* P=NULL;
    testRequire(xrpcTcpConnect(E,&Remote,0,&K,&P) && xrpcTcpWaitFor(P,5000)==XWAIT_OK && xrpcTcpDestroy(P),"TCP zero connect deadline cleanup");
    unsigned Failed=0,Passed=0;static const size_t Limits[]={1,64,512,8192,65536,131072,262144,1048576};
    for(size_t N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"TCP constructor budget");
        P=NULL;bool OK=xrpcTcpConnect(E,&Remote,0,&K,&P);testRequire(xrtAllocationBudgetLeave(&Scope),"TCP constructor budget leave");
        if(OK){++Passed;testRequire(xrpcTcpClose(P) && xrpcTcpWaitFor(P,5000)==XWAIT_OK && xrpcTcpDestroy(P),"TCP OOM close");}else{++Failed;testRequire(!P,"TCP failed constructor changed output");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"TCP constructor OOM leak");xrtAllocationBudgetFree(B);
    }
    testRequire(Failed && Passed,"TCP constructor failure/success boundary");testRequire(xrtNetEngineDestroy(E),"TCP timer/constructor pin leak");
}
int main(void)
{testTcpDuplex();testTcpStops();testTcpConcurrency();testTcpFaults();testTcpTimerExhaustion();testTcpTimerAndFactory();puts("[PASS] xrpc TCP: actual native bidirectional sockets/content-length/binary32/partial-copy/backpressure/notification/batch/deadline/cancel/concurrent-producers/correlation/retained-result/shared-executor/physical-retirement/protocol-limit-truncated-EOF-timeout/final-response-before-EOF/timer-exhaustion/constructor-OOM");return 0;}
