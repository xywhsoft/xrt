#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_tls_stream.h"
#ifndef TEST_WRITE_PROGRESS_BACKEND
#define TEST_WRITE_PROGRESS_BACKEND XNET_PORT_SELECT
#endif

typedef struct progressTest {
    xnetengine* Engine;xnetlistener* Listener;xatomicptr Server;xtlsstream* WorkerClient;
    xtlsserverconfig ServerConfig;xtlsstreamconfig Config;
    xatomic32 Cut,Done,Allow,Received,Closes,ListenerClosed,Stopping;
    unsigned Mode;bool InSend,Extra;uint64 First,Last;size_t Tail;
    unsigned char Payload[16384];
} progressTest;

static void progressWait(xatomic32* A,uint32 N,cstr Message)
{
    double Deadline=xrtTimer()+30;
    while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<Deadline,Message);xrtSleep(1);}
}
static void progressSnapshot(xtlsstream* S,xtlswriteprogress* W)
{W->Size=sizeof(*W);testRequire(xrtTlsStreamWriteProgress(S,W),"worker progress snapshot");testRequire(W->Completed<=W->Produced,"monotonic ciphertext cursors");}
static void progressAppend(xtlsstream* S,progressTest* T)
{
    if(T->Extra)return;
    size_t N=0;T->InSend=true;xtlsresult R=xrtTlsStreamSend(S,"MORE",4,&N);T->InSend=false;
    testRequire((R==XTLS_OK||R==XTLS_AGAIN)&&(N==0||N==4),"reentrant bounded write");
    if(N){xtlswriteprogress W;T->Extra=true;progressSnapshot(S,&W);T->Last=W.Produced;}
}
static void progressNotify(xtlsstream* S,ptr Data)
{
    progressTest* T=Data;testRequire(!T->InSend,"no progress notification inside Send/SendVec");
    if(T->Mode){
        xtlswriteprogress W={sizeof(W),17,19},Saved=W;
        testRequire(S->WriteProgressOverflow && S->WriteCompleted==UINT64_MAX-1,"native counter overflow never wraps");
        testRequire(!xrtTlsStreamWriteProgress(S,&W)&&xrtErrorKind(xrtGetError())==XERR_RANGE&&!memcmp(&W,&Saved,sizeof(W)),"overflow leaves snapshot unchanged");xrtClearError();
        xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);return;
    }
    xtlswriteprogress W;progressSnapshot(S,&W);
    if(W.Completed>=T->First && W.Produced>W.Completed)xrtAtomic32Store(&T->Cut,1,XMEMORY_RELEASE);
    if(xrtAtomic32Load(&T->Allow,XMEMORY_ACQUIRE))progressAppend(S,T);
    progressSnapshot(S,&W);
    if(T->Extra && W.Completed>=T->Last){
        xtlswriteprogress Before=W,After;
        testRequire(xrtTlsStreamSetWriteProgress(S,NULL),"remove observer without resetting epoch");progressSnapshot(S,&After);
        testRequire(Before.Produced==After.Produced&&Before.Completed==After.Completed,"removal preserves cursor epoch");
        testRequire(xrtTlsStreamSetWriteProgress(S,progressNotify),"restore observer");
        xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);
    }
}
static void progressOpen(xtlsstream* S,ptr Data)
{
    progressTest* T=Data;if(xrtTlsSessionRole(xrtTlsStreamSession(S))==XTLS_SERVER)return;
    xtlswriteprogress Invalid={sizeof(Invalid),31,37},Saved=Invalid;
    testRequire(!xrtTlsStreamWriteProgress(S,&Invalid)&&!memcmp(&Invalid,&Saved,sizeof(Invalid)),"query requires activation");xrtClearError();
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"allocation scope");
    testRequire(xrtTlsStreamSetWriteProgress(S,progressNotify),"activate native prefix progress");
    xtlswriteprogress Initial;progressSnapshot(S,&Initial);
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave allocation scope");
    xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);
    testRequire(xrtAllocationBudgetInfo(Budget,&BI)&&!BI.PeakBytes&&!BI.LiveCount&&!BI.Rejections,"activation and snapshot allocate nothing");xrtAllocationBudgetFree(Budget);
    Invalid.Size=0;Saved=Invalid;testRequire(!xrtTlsStreamWriteProgress(S,&Invalid)&&!memcmp(&Invalid,&Saved,sizeof(Invalid)),"size validation is failure atomic");xrtClearError();
    if(T->Mode==2){
        xnetstream* TCP=xrtTlsStreamTransport(S);xnetstreamevents E=TCP->Events;
        testRequire(xrtNetStreamSetEvents(TCP,&E,S),"native event rebind");
        xtlswriteprogress W={sizeof(W),61,67},Old=W;
        testRequire(!xrtTlsStreamWriteProgress(S,&W)&&!memcmp(&W,&Old,sizeof(W)),"native rebind invalidates old epoch");xrtClearError();
        testRequire(!xrtTlsStreamSetWriteProgress(S,NULL),"invalid epoch cannot be silently reset");xrtClearError();
        xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);return;
    }
    if(T->Mode==3){
        xtlsstreamevents E=S->Events;testRequire(xrtTlsStreamSetEvents(S,&E,T),"TLS event handoff");
        xtlswriteprogress After;progressSnapshot(S,&After);
        testRequire(!S->WriteProgress&&!S->WriteProgressPending&&Initial.Produced==After.Produced&&Initial.Completed==After.Completed,"TLS handoff removes callback and preserves epoch");
        xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);return;
    }
    if(T->Mode==1){S->WriteCompleted=UINT64_MAX-1;size_t N=0;T->InSend=true;testRequire(xrtTlsStreamSend(S,"Z",1,&N)==XTLS_OK&&N==1,"overflow input");T->InSend=false;return;}
    testRequire(xrtNetSocketSet(xrtNetStreamSocket(xrtTlsStreamTransport(S)),XNET_OPTION_SEND_BUFFER,1024),"small local send buffer");
    size_t N=0;T->InSend=true;testRequire(xrtTlsStreamSend(S,"HEAD",4,&N)==XTLS_OK&&N==4,"first plaintext accepted");
    xtlswriteprogress W;progressSnapshot(S,&W);T->First=W.Produced;
    for(unsigned I=0;I<256;++I){N=0;xtlsresult R=xrtTlsStreamSend(S,T->Payload,sizeof(T->Payload),&N);testRequire(R==XTLS_OK||R==XTLS_AGAIN,"bounded tail accepted");T->Tail+=N;if(!N)break;}
    T->InSend=false;progressSnapshot(S,&W);testRequire(W.Produced>W.Completed&&T->Tail,"later output remains queued");
}
static void progressRead(xtlsstream* S,const xnetbuf* Buffer,ptr Data)
{
    (void)Buffer;progressTest* T=Data;if(!T->Mode&&!xrtAtomic32Load(&T->Allow,XMEMORY_ACQUIRE))return;
    unsigned char Bytes[16384];size_t N=0;
    while(xrtTlsStreamAvailable(S)){
        testRequire(xrtTlsStreamRead(S,Bytes,sizeof(Bytes),&N)==XTLS_OK&&N,"consume plaintext");
        size_t Have=xrtAtomic32Load(&T->Received,XMEMORY_RELAXED);
        for(size_t I=0;I<N;++I){size_t At=Have+I;unsigned char Expected;
            if(T->Mode){testRequire(At==0,"one overflow-test byte");Expected='Z';}
            else if(At<4)Expected=(unsigned char)"HEAD"[At];
            else if(At<4+T->Tail)Expected=0x5a;
            else {testRequire(At<8+T->Tail,"exact final plaintext length");Expected=(unsigned char)"MORE"[At-4-T->Tail];}
            testRequire(Bytes[I]==Expected,"exact decrypted FIFO bytes");}
        xrtAtomic32Store(&T->Received,(uint32)(Have+N),XMEMORY_RELEASE);
    }
}
static void progressResume(xnetworker* W,ptr Data)
{(void)W;progressTest* T=Data;xtlsstream* S=xrtAtomicPtrLoad(&T->Server,XMEMORY_ACQUIRE);progressRead(S,NULL,T);progressAppend(T->WorkerClient,T);}
static void progressEnd(xtlsstream* S,ptr Data)
{(void)Data;(void)xrtTlsStreamAbort(S);}
static void progressClose(xtlsstream* S,xnetresult R,const xerror* E,ptr Data)
{
    (void)R;progressTest* T=Data;testRequire(!S->WriteProgress&&!S->WriteProgressPending,"terminal state clears observer");
    if(!xrtAtomic32Load(&T->Stopping,XMEMORY_ACQUIRE)){
        fprintf(stderr,"premature TLS close: %s\n",E?xrtErrorMessage(E):"no cause");testRequire(false,"no early TLS failure");}
    xrtAtomic32FetchAdd(&T->Closes,1,XMEMORY_RELEASE);
}
static const xtlsstreamevents ProgressEvents={progressOpen,progressRead,progressEnd,NULL,NULL,progressClose,NULL};
static bool progressAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;progressTest* T=Data;xtlsstream* TLS=NULL;
    if(!xrtTlsStreamAccept(S,&T->ServerConfig,&T->Config,&ProgressEvents,T,&TLS))return false;
    xrtAtomicPtrStore(&T->Server,TLS,XMEMORY_RELEASE);return true;
}
static void progressListenerClose(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((progressTest*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static void progressRun(unsigned Mode,xnetportbackend Backend)
{
    progressTest T={0};T.Mode=Mode;memset(T.Payload,0x5a,sizeof(T.Payload));xrtAtomicPtrInit(&T.Server,NULL);
    xtlscontext* Context=testTlsServerContext();xtlsidentity* Identity=testTlsServerIdentity();
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=testTlsServerAccept;xtlsverifier* Verifier=xrtTlsVerifierCreate(&V);testRequire(Context&&Identity&&Verifier,"TLS fixture resources");
    xrtTlsServerConfigInit(&T.ServerConfig);T.ServerConfig.Context=Context;T.ServerConfig.Identity=Identity;xrtTlsStreamConfigInit(&T.Config);
    xtlsclientconfig C;xrtTlsClientConfigInit(&C);C.Context=Context;C.Verifier=Verifier;C.ServerName=XRT_STR_LITERAL("example.com");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=1;E.Backend=Backend;T.Engine=xrtNetEngineCreate(&E);testRequire(T.Engine&&xrtNetEngineStart(T.Engine),"TLS prefix engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);L.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents LE={progressAccept,NULL,progressListenerClose};T.Listener=xrtNetListen(T.Engine,&L,&LE,NULL,&T);xnetaddr A;testRequire(T.Listener&&xrtNetListenerLocal(T.Listener,&A),"TLS prefix listener");
    xtlsstream* Client=xrtTlsStreamConnect(T.Engine,&A,0,NULL,&C,&T.Config,&ProgressEvents,&T);testRequire(Client!=NULL,"TLS prefix client");
    if(!Mode){progressWait(&T.Cut,1,"saved prefix retires with later data still pending");
        T.WorkerClient=Client;xrtAtomic32Store(&T.Allow,1,XMEMORY_RELEASE);testRequire(xrtNetEnginePost(T.Engine,0,progressResume,&T),"resume slow TLS reader");}
    progressWait(&T.Done,1,"deferred write progress callback");progressWait(&T.Received,Mode==1?1:Mode?0:(uint32)(T.Tail+8),"exact plaintext delivered");
    xtlswriteprogress Wrong={sizeof(Wrong),41,43},Saved=Wrong;
    testRequire(!xrtTlsStreamWriteProgress(Client,&Wrong)&&!memcmp(&Wrong,&Saved,sizeof(Wrong)),"foreign thread query rejected unchanged");xrtClearError();
    testRequire(!xrtTlsStreamSetWriteProgress(Client,NULL),"foreign thread setter rejected");xrtClearError();
    xtlsstream* Server=xrtAtomicPtrLoad(&T.Server,XMEMORY_ACQUIRE);testRequire(Server!=NULL,"server handle publication");
    xrtAtomic32Store(&T.Stopping,1,XMEMORY_RELEASE);xrtTlsStreamAbort(Client);xrtTlsStreamAbort(Server);progressWait(&T.Closes,2,"both TLS streams close");
    xrtTlsStreamDestroy(Client);xrtTlsStreamDestroy(Server);xrtNetListenerClose(T.Listener);progressWait(&T.ListenerClosed,1,"listener close");xrtNetListenerDestroy(T.Listener);
    testRequire(xrtNetEngineDestroy(T.Engine),"join all native and callback tails");xrtTlsVerifierRelease(Verifier);xrtTlsIdentityRelease(Identity);xrtTlsContextRelease(Context);
}
int main(void)
{
    for(unsigned Mode=0;Mode<4;++Mode){progressRun(Mode,TEST_WRITE_PROGRESS_BACKEND);
#ifndef TEST_WRITE_PROGRESS_NATIVE
        progressRun(Mode,XNET_PORT_AUTO);
#endif
    }
    puts("[PASS] TLS prefix progress: slow peer, FIFO bytes, deferred/reentrant callbacks, epoch and overflow");return 0;
}
