#include <xrpc/stdio.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include "test.h"
#include <stdio.h>
#include <string.h>

typedef struct stdioTestPipe {
    unsigned char Bytes[2048];
    size_t Size,Limit;
    bool End,Fail,InvalidCount;
} stdioTestPipe;
typedef struct stdioTestIO {
    stdioTestPipe* In;
    stdioTestPipe* Out;
    size_t Reads,Writes,OutputLimitAtEOF;
} stdioTestIO;
typedef struct stdioTestHandler {
    xatomic32 Calls,Hold;
} stdioTestHandler;

static xrpcstdioio stdioTestRead(ptr Raw,bytes Data,size_t Size,size_t* Count)
{
    stdioTestIO* IO=Raw;
    ++IO->Reads;
    stdioTestPipe* P=IO->In;
    if(P->Fail)return XRPC_STDIO_IO_ERROR;
    if(P->InvalidCount){*Count=Size+1;return XRPC_STDIO_IO_PROGRESS;}
    if(!P->Size){
        if(P->End && IO->OutputLimitAtEOF)IO->Out->Limit=IO->OutputLimitAtEOF;
        return P->End?XRPC_STDIO_IO_EOF:XRPC_STDIO_IO_AGAIN;
    }
    if(Size>P->Size)Size=P->Size;
    memcpy(Data,P->Bytes,Size);
    P->Size-=Size;
    memmove(P->Bytes,P->Bytes+Size,P->Size);
    *Count=Size;
    return XRPC_STDIO_IO_PROGRESS;
}
static xrpcstdioio stdioTestWrite(ptr Raw,cbytes Data,size_t Size,size_t* Count)
{
    stdioTestIO* IO=Raw;
    ++IO->Writes;
    stdioTestPipe* P=IO->Out;
    if(P->Fail)return XRPC_STDIO_IO_ERROR;
    if(P->InvalidCount){*Count=Size+1;return XRPC_STDIO_IO_PROGRESS;}
    size_t Room=P->Limit-P->Size;
    if(!Room)return XRPC_STDIO_IO_AGAIN;
    if(Size>Room)Size=Room;
    memcpy(P->Bytes+P->Size,Data,Size);
    P->Size+=Size;
    *Count=Size;
    return XRPC_STDIO_IO_PROGRESS;
}
static bool stdioTestEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* Context,ptr Data)
{
    stdioTestHandler* T=Data;
    xrtAtomic32FetchAdd(&T->Calls,1,XMEMORY_RELEASE);
    while(xrtAtomic32Load(&T->Hold,XMEMORY_ACQUIRE)){
        if(xrtCancelRequested(Context->Cancel) || xrtTimer()>=Context->Deadline)return false;
        xrtSleep(1);
    }
    return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}
static xrpcserver* stdioTestServer(stdioTestHandler* H)
{
    xrpcserverconfig C;
    testRequire(xrpcServerConfigInit(&C),"stdio server config");
    C.Call=stdioTestEcho;
    C.User=H;
    xrpcserver* S=NULL;
    testRequire(xrpcServerCreate(&C,&S),"stdio server");
    return S;
}
static xrpcstdioconfig stdioTestConfig(stdioTestIO* P,xrpcserver* Server,xrpcframeformat Format)
{
    xrpcstdioconfig C;
    testRequire(xrpcStdioConfigInit(&C),"stdio defaults");
    C.Duplex.Server=Server;
    C.Frame.Format=Format;
    C.Read=stdioTestRead;
    C.Write=stdioTestWrite;
    C.User=P;
    return C;
}
static xrpcstdio* stdioTestCreate(const xrpcstdioconfig* Config)
{
    xrpcstdio* S=NULL;
    testRequire(xrpcStdioCreate(Config,&S),"stdio create");
    return S;
}
static xrpcstdioinfo stdioTestInfo(xrpcstdio* S)
{
    xrpcstdioinfo I={.Size=sizeof(I)};
    testRequire(xrpcStdioInfo(S,&I),"stdio info");
    return I;
}
static void stdioTestDrain(xrpcstdio* S,xrpcstdioreason Reason)
{
    double Deadline=xrtTimer()+10;
    while(!stdioTestInfo(S).Drained){
        testRequire(xrpcStdioStep(S) && xrtTimer()<Deadline,"stdio owner drains");
        xrtSleep(0);
    }
    testRequire(stdioTestInfo(S).Reason==Reason && xrpcStdioDestroy(S),"stdio terminal reason and destroy");
}
static const xrpccallresult* stdioTestResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{
    xfutureresult X;
    testRequire(xrtFutureResult(F,&X) && X.State==XFUTURE_RESOLVED && X.Value,"stdio typed result");
    const xrpccallresult* V=X.Value;
    testRequire(V->Status==Status && V->Delivery==Delivery,"stdio typed status and delivery");
    return V;
}
static void stdioTestFrame(stdioTestPipe* P,xrpcframeformat Format,cstr Body)
{
    size_t N=strlen(Body),Head=4;
    unsigned char* Out=P->Bytes+P->Size;
    if(Format==XRPC_FRAME_CONTENT_LENGTH)
        Head=(size_t)snprintf((char*)Out,64,"Content-Length: %zu\r\n\r\n",N);
    else for(unsigned I=0;I<4;++I)Out[I]=(unsigned char)(N>>((3u-I)*8u));
    testRequire(P->Size+Head+N<=sizeof(P->Bytes),"script frame fixture capacity");
    memcpy(Out+Head,Body,N);
    P->Size+=Head+N;
}
static void stdioTestPair(xrpcframeformat Format)
{
    stdioTestPipe AB={.Limit=19},BA={.Limit=13};
    stdioTestIO A={.In=&BA,.Out=&AB},B={.In=&AB,.Out=&BA};
    stdioTestHandler H={0};
    xrpcserver* Server=stdioTestServer(&H);
    xrpcstdioconfig C=stdioTestConfig(&A,Server,Format);
    C.ReadBytes=7;C.WriteChunkBytes=11;C.MaxIOWork=1;C.Duplex.MaxWork=1;
    xrpcstdio* Left=stdioTestCreate(&C);
    C.User=&B;
    xrpcstdio* Right=stdioTestCreate(&C);
    xfuture *Call=NULL,*Batch=NULL;
    xrpcduplexitem Items[]={
        {true,XRT_STR_LITERAL("n"),{0}},
        {false,XRT_STR_LITERAL("b"),XRT_STR_LITERAL("[2]")}
    };
    testRequire(xrpcStdioCall(Left,XRT_STR_LITERAL("a"),XRT_STR_LITERAL("[1]"),5000,&Call)==XRPC_DUPLEX_ACCEPTED,
                "stdio first peer call");
    testRequire(xrpcStdioBatch(Right,Items,2,5000,&Batch)==XRPC_DUPLEX_ACCEPTED,"stdio second peer batch");
    double Deadline=xrtTimer()+10;
    while(!xrtFutureDone(Call) || !xrtFutureDone(Batch)){
        size_t Calls=A.Reads+A.Writes+B.Reads+B.Writes;
        testRequire(xrpcStdioStep(Left) && xrpcStdioStep(Right) && xrtTimer()<Deadline,"stdio independent owner turns");
        testRequire(A.Reads+A.Writes+B.Reads+B.Writes-Calls<=4,"failed I/O attempts bounded per lane");
        xrtSleep(0);
    }
    stdioTestResult(Call,XRPC_CALL_OK,XRPC_REPLIED);
    const xrpcbatchcallresult* V=(const xrpcbatchcallresult*)stdioTestResult(Batch,XRPC_CALL_BATCH_DONE,XRPC_REPLIED);
    testRequire(V->Count==2 && V->Indices[0]==SIZE_MAX && V->Indices[1]==0,"stdio batch response map");
    testRequire(xrtAtomic32Load(&H.Calls,XMEMORY_ACQUIRE)==3 && A.Writes>4 && B.Writes>4,"full duplex partial writes");
    testRequire(xrpcStdioClose(Left,XRPC_CALL_CLOSED) && xrpcStdioClose(Right,XRPC_CALL_CLOSED),"stdio closing");
    stdioTestDrain(Left,XRPC_STDIO_CLOSED);stdioTestDrain(Right,XRPC_STDIO_CLOSED);
    testRequire(xrpcServerDestroy(Server),"stdio shared executor retirement");
    xrtFutureDestroy(Call);xrtFutureDestroy(Batch);
}
static void stdioTestBackpressure(xrpcframeformat Format)
{
    stdioTestPipe In={.End=true},Out={.Limit=sizeof(Out.Bytes)};
    stdioTestIO IO={.In=&In,.Out=&Out};
    stdioTestHandler H={0};
    xrtAtomic32Store(&H.Hold,1,XMEMORY_RELEASE);
    xrpcserver* Server=stdioTestServer(&H);
    xrpcstdioconfig C=stdioTestConfig(&IO,Server,Format);
    C.Duplex.MaxIncoming=1;
    xrpcstdio* S=stdioTestCreate(&C);
    stdioTestFrame(&In,Format,"{\"jsonrpc\":\"2.0\",\"method\":\"a\",\"id\":1}");
    stdioTestFrame(&In,Format,"{\"jsonrpc\":\"2.0\",\"method\":\"b\",\"id\":2}");
    testRequire(xrpcStdioStep(S),"stdio two coalesced offers");
    xrpcstdioinfo I=stdioTestInfo(S);
    testRequire(I.Duplex.Accepted==1 && I.Frame.Ready && !I.WantRead && !I.InputEnded && IO.Reads==1,
                "stdio preserves overloaded borrowed frame and does not over-read");
    xrtAtomic32Store(&H.Hold,0,XMEMORY_RELEASE);
    stdioTestDrain(S,XRPC_STDIO_EOF);
    xrpcframereader* Reader=NULL;
    testRequire(xrpcFrameReaderCreate(&C.Frame,&Reader),"verify stdio output frames");
    size_t At=0;
    unsigned Ids=0;
    for(unsigned N=0;N<2;++N){
        size_t Used=0;
        xrpcframeview Frame;
        testRequire(xrpcFrameReaderFeed(Reader,(xbytesview){Out.Bytes+At,Out.Size-At},&Used,&Frame)==XRPC_FRAME_READY,
                    "accepted request response retained through input EOF");
        xrpcmessage M; xrpcdocument D;
        testRequire(xrpcDecode((xstrview){(cstr)Frame.Body.Data,Frame.Body.Size},NULL,&M,1,&D)==XRPC_DECODE_DONE &&
                    M.Kind==XRPC_MESSAGE_RESULT && M.Id.Raw.Size==1,"literal response shape after EOF");
        unsigned Id=(unsigned)(M.Id.Raw.Data[0]-'0');
        testRequire(Id==1 || Id==2,"literal response ID");
        Ids|=1u<<Id;
        At+=Used;
    }
    testRequire(At==Out.Size && Ids==6 && xrtAtomic32Load(&H.Calls,XMEMORY_ACQUIRE)==2,"no frame loss/replay through overload");
    xrpcFrameReaderDestroy(Reader);
    testRequire(xrpcServerDestroy(Server),"stdio overload executor teardown");
}
static void stdioTestEOF(unsigned Mode)
{
    stdioTestPipe In={0},Out={.Limit=sizeof(Out.Bytes)};
    stdioTestIO IO={.In=&In,.Out=&Out};
    xrpcstdioconfig C=stdioTestConfig(&IO,NULL,XRPC_FRAME_UINT32_BE);
    C.MaxIOWork=1;
    C.WriteChunkBytes=1;
    if(Mode==3){
        C.DrainTimeout=0;
        C.MaxIOWork=64;
        Out.Limit=1;
        IO.OutputLimitAtEOF=sizeof(Out.Bytes);
    }
    xrpcstdio* S=stdioTestCreate(&C);
    xfuture* F=NULL;
    testRequire(xrpcStdioCall(S,XRT_STR_LITERAL("call"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"stdio EOF local call");
    if(Mode)testRequire(xrpcStdioStep(S) && Out.Size==1,"stdio selects and emits exactly one framing byte");
    if(Mode>=2)Out.Limit=Out.Size;
    if(Mode==3)testRequire(xrpcStdioStep(S) && Out.Size==1,"stdio holds both lanes before EOF");
    In.End=true;
    testRequire(xrpcStdioStep(S) && stdioTestInfo(S).InputEnded,"stdio clean EOF observed promptly");
    xfuture* Untouched=(xfuture*)(uintptr_t)1;
    testRequire(xrpcStdioNotify(S,XRT_STR_LITERAL("late"),(xstrview){0},1,&Untouched)==XRPC_DUPLEX_CLOSED &&
                Untouched==(xfuture*)(uintptr_t)1,"EOF stops new submissions without modifying output");
    if(Mode>=2){
        if(Mode==2)testRequire(xrtFutureCancel(F),"cancel blocked partial write after EOF");
        stdioTestDrain(S,Mode==2?XRPC_STDIO_DUPLEX:XRPC_STDIO_TIMEOUT);
        stdioTestResult(F,Mode==2?XRPC_CALL_CANCELLED:XRPC_CALL_TIMEOUT,XRPC_MAY_HAVE_EXECUTED);
        testRequire(Out.Size==1,"aborted partial frame was not resumed");
    }else {
        stdioTestDrain(S,XRPC_STDIO_EOF);
        stdioTestResult(F,XRPC_CALL_TRANSPORT,Mode?XRPC_MAY_HAVE_EXECUTED:XRPC_NOT_SENT);
        if(Mode){
            testRequire(Out.Size>4 && Out.Bytes[0]==0 && Out.Bytes[1]==0 && Out.Bytes[2]==0 &&
                        Out.Bytes[3]==Out.Size-4,"a started frame is completed after read EOF");
        }else testRequire(!Out.Size,"EOF suppresses an unstarted local request");
    }
    xrtFutureDestroy(F);
}
static void stdioTestFailure(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){
        stdioTestPipe In={0},Out={.Limit=sizeof(Out.Bytes)};
        stdioTestIO IO={.In=&In,.Out=&Out};
        xrpcstdioconfig C=stdioTestConfig(&IO,NULL,XRPC_FRAME_CONTENT_LENGTH);
        xrpcstdio* S=stdioTestCreate(&C);
        xfuture* F=NULL;
        if(Mode==0){
            memcpy(In.Bytes,"Content-Length: 10\r\n\r\n{}",24);
            In.Size=24;In.End=true;
        }else if(Mode==1){
            memcpy(In.Bytes,"Content-Length: -1\r\n\r\n",22);
            In.Size=22;
        }else if(Mode==2)In.InvalidCount=true;
        else if(Mode==3){
            Out.Fail=true;
            testRequire(xrpcStdioNotify(S,XRT_STR_LITERAL("note"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"stdio I/O failure outgoing offer");
        }else {
            In.Fail=true;
        }
        stdioTestDrain(S,Mode==0?XRPC_STDIO_TRUNCATED:Mode==1?XRPC_STDIO_FRAME:XRPC_STDIO_IO);
        if(F){
            stdioTestResult(F,XRPC_CALL_TRANSPORT,XRPC_MAY_HAVE_EXECUTED);
            xrtFutureDestroy(F);
        }
    }
}
static void stdioTestOwnership(void)
{
    stdioTestPipe In={0},Out={.Limit=sizeof(Out.Bytes)};
    stdioTestIO IO={.In=&In,.Out=&Out};
    const size_t Limit[]={1,1024,32768,65536,1048576};
    unsigned Passed=0,Failed=0;
    xrpcstdioconfig C=stdioTestConfig(&IO,NULL,XRPC_FRAME_CONTENT_LENGTH);
    for(size_t N=0;N<sizeof(Limit)/sizeof(Limit[0]);++N){
        xallocationbudget* B=NULL;
        xallocationscope Scope={0};
        testRequire(xrtAllocationBudgetCreate(Limit[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"stdio allocation prefix");
        xrpcstdio* S=(xrpcstdio*)(uintptr_t)1;
        bool OK=xrpcStdioCreate(&C,&S);
        testRequire(xrtAllocationBudgetLeave(&Scope),"stdio allocation scope");
        if(OK){
            ++Passed;testRequire(xrpcStdioClose(S,XRPC_CALL_CLOSED),"stdio valid constructor teardown");
            stdioTestDrain(S,XRPC_STDIO_CLOSED);
        }else{
            ++Failed;testRequire(S==(xrpcstdio*)(uintptr_t)1,"stdio constructor leaves output");
        }
        xrtClearError();
        xallocationbudgetinfo I={.Size=sizeof(I)};
        testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"stdio failure returns allocations");
        xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed,"stdio success and failed allocation prefixes");

    C.ReadBytes=5;
    xrpcstdio* S=stdioTestCreate(&C);
    stdioTestFrame(&In,C.Frame.Format,"{\"jsonrpc\":\"2.0\",\"method\":\"ignored\"}");
    xallocationbudget* B=NULL;
    xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"stdio framing fault budget");
    testRequire(xrpcStdioStep(S) && xrtAllocationBudgetLeave(&Scope),"stdio maps fragment allocation failure");
    stdioTestDrain(S,XRPC_STDIO_MEMORY);
    xrtClearError();
    xallocationbudgetinfo I={.Size=sizeof(I)};
    testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"stdio parser failure returns allocations");
    xrtAllocationBudgetFree(B);
}
int main(void)
{
    for(unsigned Format=1;Format<=2;++Format){
        stdioTestPair((xrpcframeformat)Format);
        stdioTestBackpressure((xrpcframeformat)Format);
    }
    for(unsigned Mode=0;Mode<4;++Mode)stdioTestEOF(Mode);
    stdioTestFailure();
    stdioTestOwnership();
    puts("[PASS] xrpc stdio: profiles, bidirectional calls, partial writes, backpressure, read EOF drain, I/O errors, constructor rollback");
    return 0;
}
