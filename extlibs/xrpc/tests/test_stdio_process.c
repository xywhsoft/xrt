#include <xrpc/stdio_process.h>
#include <xrt/thread.h>
#include "test.h"
#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <io.h>
#endif

static bool processTestFrameWrite(bool Binary,cstr Text)
{
    size_t N=strlen(Text);
    if(Binary){
        unsigned char Header[]={0,0,(unsigned char)(N>>8),(unsigned char)N};
        if(fwrite(Header,1,4,stdout)!=4)return false;
    }else if(printf("Content-Length: %u\r\n\r\n",(unsigned)N)<0)return false;
    return fwrite(Text,1,N,stdout)==N && fflush(stdout)==0;
}

static bool processTestFrameRead(bool Binary,char Body[512])
{
    size_t N=0;
    if(Binary){
        unsigned char Header[4];
        if(fread(Header,1,4,stdin)!=4 || Header[0] || Header[1])return false;
        N=(size_t)Header[2]*256u+Header[3];
    }else{
        char Line[64];
        unsigned Len=0;
        if(!fgets(Line,sizeof(Line),stdin) || sscanf(Line,"Content-Length: %u",&Len)!=1 ||
           !fgets(Line,sizeof(Line),stdin) || strcmp(Line,"\r\n"))return false;
        N=Len;
    }
    if(!N || N>=512 || fread(Body,1,N,stdin)!=N)return false;
    Body[N]=0;
    return true;
}

static int processTestChild(cstr Mode,bool Binary)
{
#if defined(_WIN32) || defined(_WIN64)
    if(_setmode(_fileno(stdin),_O_BINARY)<0 || _setmode(_fileno(stdout),_O_BINARY)<0)return 31;
#endif
    if(!strcmp(Mode,"idle")){
        xrtSleep(30000);
        return 0;
    }
    if(!strcmp(Mode,"truncated")){
        if(Binary){
            const unsigned char Bytes[]={0,0,0,50,'{'};
            return fwrite(Bytes,1,sizeof(Bytes),stdout)==sizeof(Bytes)?0:32;
        }
        return fputs("Content-Length: 50\r\n\r\n{",stdout)>=0?0:33;
    }

    if(!strcmp(Mode,"cancel")){
        char Body[512];
        if(!processTestFrameRead(Binary,Body) || !strstr(Body,"\"method\":\"rpc.xrt.cancel.v1\"") ||
           !strstr(Body,"\"id\":\"1\"") ||
           !processTestFrameWrite(Binary,"{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":true}"))return 40;
        if(!processTestFrameRead(Binary,Body) || !strstr(Body,"\"method\":\"hold\"") ||
           !strstr(Body,"\"id\":\"2\""))return 41;
        if(!processTestFrameRead(Binary,Body) ||
           strcmp(Body,"{\"jsonrpc\":\"2.0\",\"method\":\"rpc.xrt.cancel\",\"params\":[\"2\"]}"))return 42;
        if(!processTestFrameRead(Binary,Body) || !strstr(Body,"\"method\":\"call\"") ||
           !strstr(Body,"\"id\":\"3\"") ||
           !processTestFrameWrite(Binary,"{\"jsonrpc\":\"2.0\",\"id\":\"3\",\"result\":7}"))return 43;
        return 0;
    }
    if(!processTestFrameWrite(Binary,"{\"jsonrpc\":\"2.0\",\"method\":\"reverse\",\"params\":[11],\"id\":9}"))
        return 34;
    if(!strcmp(Mode,"reverse-idle")){
        xrtSleep(30000);
        return 0;
    }
    bool Response=false,Request=false;
    for(unsigned I=0;I<2;++I){
        char Body[512];
        if(!processTestFrameRead(Binary,Body))return 35;
        if(strstr(Body,"\"method\":\"call\"") && strstr(Body,"\"id\":\"1\"")){
            if(Request || !processTestFrameWrite(Binary,"{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":7}"))
                return 36;
            Request=true;
        }else if(strstr(Body,"\"id\":9") && strstr(Body,"\"result\":[11]"))Response=true;
        else return 37;
    }
    return Response && Request?0:38;
}

static void processTestConfig(xprocessconfig* C,cstr Bin,const cstr Args[3])
{
    testRequire(xrtProcessConfigInit(C),"stdio process native config");
    C->Program=Bin;
    C->Args=Args;
    C->ArgCount=3;
    C->Stderr.Mode=XPROCESS_IO_NULL;
}

static void processTestSpawn(cstr Bin,cstr Mode,const xrpcstdioconfig* C,
    xprocess** Child,xrpcstdioprocess** P)
{
    const cstr Args[]={"--process-child",Mode,C->Frame.Format==XRPC_FRAME_UINT32_BE?"be":"text"};
    xprocessconfig Native;
    processTestConfig(&Native,Bin,Args);
    testRequire(xrpcStdioProcessSpawn(&Native,C,Child,P),"spawn actual framed peer");
}

static void processTestStep(xrpcstdioprocess* P,xrpcstdioinfo* Info)
{
    testRequire(xrpcStdioProcessStep(P,Info),"stdio process owner step");
}

static void processTestDrive(xrpcstdioprocess* P,xfuture* F,xrpcstdioinfo* Info)
{
    double Until=xrtTimer()+8;
    for(;;){
        processTestStep(P,Info);
        if(F?xrtFutureDone(F):Info->Drained)return;
        xwaitresult Wake=xrpcStdioProcessWait(P,500);
        testRequire((Wake==XWAIT_OK || Wake==XWAIT_TIMEOUT) && xrtTimer()<Until,
            "stdio process bounded drive");
    }
}

static void processTestFinish(xrpcstdioprocess* P,xprocess* Child,xrpcstdioreason Reason,bool Kill)
{
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    processTestDrive(P,NULL,&Info);
    testRequire(Info.Reason==Reason && xrpcStdioProcessWait(P,0)==XWAIT_CLOSED &&
        xrpcStdioProcessDestroy(P),"stdio process terminal and physical retire");
    if(Kill)testRequire(xrtProcessWaitFor(Child,0)==XWAIT_TIMEOUT && xrtProcessKill(Child),
        "adapter destruction leaves the child independently controllable");
    testRequire(xrtProcessWaitFor(Child,5000)==XWAIT_OK,"wait for retired peer");
    xprocessstatus Exit;
    testRequire(xrtProcessStatus(Child,&Exit) && (Kill || (Exit.Kind==XPROCESS_EXIT_CODE && !Exit.Code)),
        "expected independent child status");
    xrtProcessDestroy(Child);
}

typedef struct processTestHandler {
    xatomic32 Entered,Release;
} processTestHandler;

static bool processTestReverse(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* Context,ptr User)
{
    (void)Context;
    processTestHandler* Hold=User;
    if(Hold){
        xrtAtomic32Store(&Hold->Entered,1,XMEMORY_RELEASE);
        while(!xrtAtomic32Load(&Hold->Release,XMEMORY_ACQUIRE))xrtSleep(1);
    }
    return xrpcReplyResult(R,M->Params.Raw);
}

typedef struct processTestSubmission {
    xrpcstdioprocess* Owner;
    xfuture* Future;
    int64 Timeout;
} processTestSubmission;

static int32 processTestSubmit(ptr Data)
{
    processTestSubmission* T=Data;
    xrtSleep(20);
    return xrpcStdioCall(xrpcStdioProcessEndpoint(T->Owner),XRT_STR_LITERAL("call"),
        XRT_STR_LITERAL("[1]"),T->Timeout,&T->Future)==XRPC_DUPLEX_ACCEPTED?0:1;
}

typedef struct processTestReentry {
    xrpcstdioprocess* Owner;
    bool Called,Refused;
} processTestReentry;

static void processTestWatch(ptr Data)
{
    processTestReentry* R=Data;
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    R->Called=true;
    R->Refused=!xrpcStdioProcessStep(R->Owner,&Info) &&
        xrpcStdioProcessWait(R->Owner,0)==XWAIT_ERROR && !xrpcStdioProcessDestroy(R->Owner);
    xrtClearError();
}

static void processTestDuplex(cstr Bin,xrpcframeformat Format)
{
    xrpcserverconfig S;
    xrpcServerConfigInit(&S);
    S.Call=processTestReverse;
    xrpcserver* Server=NULL;
    testRequire(xrpcServerCreate(&S,&Server),"spawn-independent inbound executor");
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    C.Frame.Format=Format;
    C.Duplex.Server=Server;
    C.MaxIOWork=3;
    xprocess* Child=NULL;
    xrpcstdioprocess* P=NULL;
    processTestSpawn(Bin,"duplex",&C,&Child,&P);
    testRequire(!xrpcStdioProcessDestroy(P),"open child rejects early destroy");
    xrtClearError();
    processTestSubmission Submit={.Owner=P,.Timeout=5000};
    xthread* Producer=xrtThreadCreate(processTestSubmit,&Submit,0);
    testRequire(Producer && xrpcStdioProcessWait(P,0)==XWAIT_OK,"initial owner turn");
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    processTestStep(P,&Info);
    testRequire(xrpcStdioProcessWait(P,5000)==XWAIT_OK &&
        xrtThreadWait(Producer)==XWAIT_OK && Submit.Future,"concurrent admission wakes the owner");
    xrtThreadDestroy(Producer);

    xfuturewatch Watch;
    processTestReentry Reentry={.Owner=P};
    testRequire(xrtFutureWatchInit(&Watch,processTestWatch,NULL,&Reentry) &&
        xrtFutureWatchAdd(Submit.Future,&Watch)==XFUTURE_WATCH_PENDING,"register owner reentry check");
    processTestDrive(P,Submit.Future,&Info);
    const xrpccallresult* V=xrtFutureValue(Submit.Future);
    testRequire(V && V->Status==XRPC_CALL_OK && V->Delivery==XRPC_REPLIED &&
        Reentry.Called && Reentry.Refused,"reply and reentry guard");
    processTestFinish(P,Child,XRPC_STDIO_EOF,false);
    testRequire(xrpcServerDestroy(Server),"server survives and retires separately");
    const xrpccallresult* After=xrtFutureValue(Submit.Future);
    testRequire(After==V && V->Responses,"result retains its snapshot independently");
    xrtFutureDestroy(Submit.Future);
}

static void processTestDeadline(cstr Bin)
{
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    xprocess* Child=NULL;
    xrpcstdioprocess* P=NULL;
    processTestSpawn(Bin,"idle",&C,&Child,&P);
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    processTestStep(P,&Info);
    testRequire(xrpcStdioProcessWait(P,10)==XWAIT_TIMEOUT && Info.WantRead,
        "caller wait expires while peer is silent");
    processTestSubmission Submit={.Owner=P,.Timeout=250};
    xthread* Producer=xrtThreadCreate(processTestSubmit,&Submit,0);
    testRequire(Producer && xrpcStdioProcessWait(P,2000)==XWAIT_OK &&
        xrtThreadWait(Producer)==XWAIT_OK && Submit.Future,"submission wakes a silent native pipe");
    xrtThreadDestroy(Producer);
    xfuture* F=Submit.Future;
    do{processTestStep(P,&Info);}while(Info.More);
    /* The RPC deadline precedes the user's wait budget. */
    double Until=xrtTimer()+3.0;
    while(!xrtFutureDone(F)){
        testRequire(xrpcStdioProcessWait(P,2000)==XWAIT_OK && xrtTimer()<Until,"RPC deadline wakes the owner");
        processTestStep(P,&Info);
    }
    const xrpccallresult* V=xrtFutureValue(F);
    testRequire(V && V->Status==XRPC_CALL_TIMEOUT && V->Delivery==XRPC_MAY_HAVE_EXECUTED &&
        Info.Reason==XRPC_STDIO_OPEN,"sent call times out without sealing idle channel");
    testRequire(xrpcStdioClose(xrpcStdioProcessEndpoint(P),XRPC_CALL_CLOSED),"seal silent channel");
    processTestFinish(P,Child,XRPC_STDIO_CLOSED,true);
    xrtFutureDestroy(F);
}

static void processTestBlocked(cstr Bin)
{
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    C.WriteChunkBytes=8192;
    xprocess* Child=NULL;
    xrpcstdioprocess* P=NULL;
    processTestSpawn(Bin,"idle",&C,&Child,&P);
    size_t N=768u*1024u;
    char* Text=xrtMalloc(N);
    testRequire(Text!=NULL,"backpressure payload");
    memset(Text,'x',N);
    Text[0]='[';Text[1]='"';Text[N-2]='"';Text[N-1]=']';
    xfuture* F=NULL;
    testRequire(xrpcStdioCall(xrpcStdioProcessEndpoint(P),XRT_STR_LITERAL("put"),
        (xstrview){Text,N},5000,&F)==XRPC_DUPLEX_ACCEPTED,"submit large frame");
    xrtFree(Text);
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    do{processTestStep(P,&Info);}while(Info.More);
    testRequire(Info.WantWrite && Info.Duplex.Started && !xrtFutureDone(F),"unread child exhausts pipe credit");
    testRequire(xrtFutureCancel(F) && xrpcStdioProcessWait(P,2000)==XWAIT_OK,"cancel wakes blocked pipe");
    processTestDrive(P,F,&Info);
    const xrpccallresult* V=xrtFutureValue(F);
    testRequire(V && V->Status==XRPC_CALL_CANCELLED && V->Delivery==XRPC_MAY_HAVE_EXECUTED,
        "partial frame cancellation keeps uncertain outcome");
    processTestFinish(P,Child,XRPC_STDIO_DUPLEX,true);
    xrtFutureDestroy(F);
}

static void processTestHandlerTail(cstr Bin)
{
    processTestHandler Hold={0};
    xrpcserverconfig S;
    xrpcServerConfigInit(&S);
    S.Call=processTestReverse;
    S.User=&Hold;
    xrpcserver* Server=NULL;
    testRequire(xrpcServerCreate(&S,&Server),"held handler executor");
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    C.Duplex.Server=Server;
    xprocess* Child=NULL;
    xrpcstdioprocess* P=NULL;
    processTestSpawn(Bin,"reverse-idle",&C,&Child,&P);
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    double Until=xrtTimer()+8;
    while(!xrtAtomic32Load(&Hold.Entered,XMEMORY_ACQUIRE)){
        processTestStep(P,&Info);
        testRequire(xrtTimer()<Until && xrpcStdioProcessWait(P,10)!=XWAIT_ERROR,
            "real child reverse request enters executor");
    }
    testRequire(xrpcStdioClose(xrpcStdioProcessEndpoint(P),XRPC_CALL_CLOSED),"close with executed handler");
    processTestStep(P,&Info);
    testRequire(Info.Reason==XRPC_STDIO_CLOSED && !Info.Drained && !xrpcStdioProcessDestroy(P),
        "adapter retains wake and pipe until the handler physically retires");
    xrtClearError();
    xrtAtomic32Store(&Hold.Release,1,XMEMORY_RELEASE);
    processTestFinish(P,Child,XRPC_STDIO_CLOSED,true);
    testRequire(xrpcServerDestroy(Server),"independent executor retires after transport");
}

static void processTestNegotiatedCancellation(cstr Bin,xrpcframeformat Format)
{
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    C.Frame.Format=Format;
    xprocess* Child=NULL;
    xrpcstdioprocess* P=NULL;
    processTestSpawn(Bin,"cancel",&C,&Child,&P);
    xrpcstdio* RPC=xrpcStdioProcessEndpoint(P);
    xfuture* Offer=NULL;
    testRequire(xrpcStdioEnableCancellation(RPC) &&
        xrpcStdioNegotiateCancellation(RPC,5000,&Offer)==XRPC_DUPLEX_ACCEPTED,
        "negotiate through real redirected stdio");
    xrpcstdioinfo Info={.Size=sizeof(Info)};
    processTestDrive(P,Offer,&Info);
    const xrpccallresult* Accepted=xrtFutureValue(Offer);
    testRequire(Accepted && Accepted->Status==XRPC_CALL_OK,"native peer acknowledges capability");
    xrtFutureDestroy(Offer);

    xfuture* Held=NULL;
    testRequire(xrpcStdioCall(RPC,XRT_STR_LITERAL("hold"),(xstrview){0},5000,&Held)==
        XRPC_DUPLEX_ACCEPTED,"send cancellable request");
    double Until=xrtTimer()+8;
    for(;;){
        processTestStep(P,&Info);
        if(Info.Duplex.PendingCalls==1 && !Info.Duplex.Outputs && !Info.Duplex.Selected)break;
        testRequire(xrpcStdioProcessWait(P,500)==XWAIT_OK && xrtTimer()<Until,
            "wait for complete original send");
    }
    xfuture* Following=NULL;
    testRequire(xrtFutureCancel(Held) &&
        xrpcStdioCall(RPC,XRT_STR_LITERAL("call"),(xstrview){0},5000,&Following)==XRPC_DUPLEX_ACCEPTED,
        "cancel original and continue on same native channel");
    processTestDrive(P,Following,&Info);
    const xrpccallresult* Cancelled=xrtFutureValue(Held);
    const xrpccallresult* Replied=xrtFutureValue(Following);
    testRequire(Cancelled && Cancelled->Status==XRPC_CALL_CANCELLED &&
        Cancelled->Delivery==XRPC_MAY_HAVE_EXECUTED && Replied && Replied->Status==XRPC_CALL_OK,
        "native peer checks ordered control before following request");
    processTestFinish(P,Child,XRPC_STDIO_EOF,false);
    xrtFutureDestroy(Held);xrtFutureDestroy(Following);
}

static void processTestTruncated(cstr Bin,xrpcframeformat Format)
{
    xrpcstdioconfig C;
    xrpcStdioConfigInit(&C);
    C.Frame.Format=Format;
    xprocess* Child=NULL;
    xrpcstdioprocess* P=NULL;
    processTestSpawn(Bin,"truncated",&C,&Child,&P);
    testRequire(xrtProcessWaitFor(Child,5000)==XWAIT_OK,"child exits before owner reads");
    processTestFinish(P,Child,XRPC_STDIO_TRUNCATED,false);
}

static void processTestRollback(void)
{
    const cstr Args[]={"--process-child","idle","text"};
    xprocessconfig Native;
    processTestConfig(&Native,"xrt-missing-stdio-process-a53d6d98",Args);
    xprocess* Child=(xprocess*)(uintptr_t)1;
    xrpcstdioprocess* P=(xrpcstdioprocess*)(uintptr_t)2;
    Native.Stderr.Mode=XPROCESS_IO_MERGE;
    testRequire(!xrpcStdioProcessSpawn(&Native,NULL,&Child,&P) &&
        xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"reject mixed diagnostics");
    Native.Stderr.Mode=XPROCESS_IO_NULL;
    xrtClearError();
    const size_t Bytes[]={1,256,16384,65536,1048576,4194304};
    for(size_t I=0;I<sizeof(Bytes)/sizeof(Bytes[0]);++I){
        xallocationbudget* B=NULL;
        xallocationscope Scope={0};
        testRequire(xrtAllocationBudgetCreate(Bytes[I],&B) && xrtAllocationBudgetEnter(B,&Scope),
            "apply bounded failure injection");
        testRequire(!xrpcStdioProcessSpawn(&Native,NULL,&Child,&P) && xrtAllocationBudgetLeave(&Scope) &&
            Child==(xprocess*)(uintptr_t)1 && P==(xrpcstdioprocess*)(uintptr_t)2 && xrtGetError(),
            "failed spawn preserves outputs and error");
        xrtClearError();
        xallocationbudgetinfo Budget={.Size=sizeof(Budget)};
        testRequire(xrtAllocationBudgetInfo(B,&Budget) && !Budget.LiveBytes,"rollback returns allocation budget");
        xrtAllocationBudgetFree(B);
    }
}

int main(int Argc,char** Argv)
{
    if(Argc==4 && !strcmp(Argv[1],"--process-child"))return processTestChild(Argv[2],!strcmp(Argv[3],"be"));
    processTestRollback();
    for(unsigned I=1;I<=2;++I){
        processTestDuplex(Argv[0],(xrpcframeformat)I);
        processTestTruncated(Argv[0],(xrpcframeformat)I);
        processTestNegotiatedCancellation(Argv[0],(xrpcframeformat)I);
    }
    processTestDeadline(Argv[0]);
    processTestBlocked(Argv[0]);
    processTestHandlerTail(Argv[0]);
    puts("[PASS] xrpc stdio process: duplex, wake, deadline, cancellation, EOF, reentry, handler tail, rollback and negotiated cancellation");
    return 0;
}
