#include "../test.h"
#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_websocket_stream.h"
#include "../../src/internal/xrt_tcp.h"
#ifndef WS_RETIRE_BACKEND
#define WS_RETIRE_BACKEND XNET_PORT_AUTO
#endif
typedef struct wrAttachPost {xnetpost Post;ptr Context;xtlsstream* TLS;xwsrole Role;} wrAttachPost;
typedef struct wsRetireContext {
    xnetengine* Engine;xnetlistener* Listener;xtlscontext* TLSContext;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig NativeServer;
    xatomicptr Client,Server,TLSClient;bool TLS,Owned,ExpectedPoll;
    xatomic32 Entered,Permit,Returned,Refs,Drops,WaitStarted,WaitDone,WorkerDone,ListenerClosed;
    xnetpost WorkerPost;wrAttachPost Attach[2];
} wsRetireContext;
static void wrAwait(xatomic32* A,uint32 N,cstr M)
{double End=xrtTimer()+10;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,M);xrtSleep(1);}}
static bool wrCount(const void* D,size_t* N){*N=xrtAtomic32Load(&((const wsRetireContext*)D)->Refs,XMEMORY_ACQUIRE);return true;}
static bool wrTrace(const void* D,xrtownershipvisitor V,ptr U){(void)D;(void)V;(void)U;return false;}
static void wrDrop(const void* D){wsRetireContext* C=(wsRetireContext*)D;xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&C->Drops,1,XMEMORY_RELEASE);}
static const xrtownershipops WrOps={wrCount,wrTrace};
static void wrClosed(xwsstream* S,const xwsstreamclose* Info,ptr Data)
{
    (void)Info;wsRetireContext* C=Data;if(xrtWsStreamRole(S)!=XWS_ROLE_CLIENT)return;
    testRequire(xrtWsStreamPollRetired(S)==XWAIT_TIMEOUT,"WS own held callback cannot be retired");
    testRequire(xrtWsStreamWaitRetired(S,0)==XWAIT_ERROR,"WS own Worker wait rejected");xrtClearError();
    xrtAtomic32Store(&C->Entered,1,XMEMORY_RELEASE);wrAwait(&C->Permit,1,"release held WS Close callback");xrtAtomic32Store(&C->Returned,1,XMEMORY_RELEASE);
}
static const xwsstreamevents WrEvents={.Close=wrClosed};
static const xwsstreamownershipv1 WrPolicy={sizeof(xwsstreamownershipv1),{.Close=wrClosed},wrDrop,&WrOps,NULL};
static void wrAttach(wsRetireContext* C,ptr Native,xwsrole Role)
{
    xwsstreamconfig K;xrtWsStreamConfigInit(&K);K.Role=Role;K.Protocol=XRT_STR_LITERAL("retire");xwsstream* S;
    if(C->Owned){xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);S=C->TLS?xrtWsStreamAttachTlsOwnedV1(Native,NULL,&K,C,&WrPolicy):xrtWsStreamAttachOwnedV1(Native,NULL,&K,C,&WrPolicy);}
    else S=C->TLS?xrtWsStreamAttachTls(Native,0,&K,&WrEvents,C):xrtWsStreamAttach(Native,0,&K,&WrEvents,C);
    testRequire(S!=NULL,"WS retirement attach");xrtAtomicPtrStore(Role==XWS_ROLE_CLIENT?&C->Client:&C->Server,S,XMEMORY_RELEASE);
}
static void wrTLSAttachPosted(xnetworker* W,ptr Data)
{(void)W;wrAttachPost* P=Data;wrAttach(P->Context,P->TLS,P->Role);}
static void wrTLSOpen(xtlsstream* T,ptr Data)
{
    wsRetireContext* C=Data;xwsrole Role=xrtTlsSessionRole(xrtTlsStreamSession(T))==XTLS_CLIENT?XWS_ROLE_CLIENT:XWS_ROLE_SERVER;wrAttachPost* P=C->Attach+(Role==XWS_ROLE_CLIENT?0:1);
    P->Context=C;P->TLS=T;P->Role=Role;
    /* Owned TLS event rebinding requires leaving its Open/drive callback. */
    testRequire(xrtNetPostInit(&P->Post)&&xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(T)),&P->Post,wrTLSAttachPosted,P),"post exclusive WSS handoff");
}
static const xtlsstreamevents WrTLSEvents={.Open=wrTLSOpen};
static const xtlsstreamownershipv1 WrTLSPolicy={sizeof(xtlsstreamownershipv1),{.Open=wrTLSOpen},wrDrop,&WrOps};
static xtlsverifydecision wrVerify(const xtlspeer* P,ptr Data)
{wsRetireContext* C=Data;xbytesview Pin={0};return P&&P->CertificateCount&&P->Name.Size==14&&!memcmp(P->Name.Data,"retire.fixture",14)&&xrtTlsIdentityCertificate(C->Identity,0,&Pin)&&P->Certificates[0].Raw.Size==Pin.Size&&!memcmp(P->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
static bool wrAccept(xnetlistener* L,xnetstream* T,ptr Data)
{
    (void)L;wsRetireContext* C=Data;if(!C->TLS){wrAttach(C,T,XWS_ROLE_SERVER);return true;}
    xtlsstream* S=NULL;bool OK;if(C->Owned){xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);OK=xrtTlsStreamAcceptOwnedV1(T,&C->NativeServer,NULL,C,&WrTLSPolicy,&S);}
    else OK=xrtTlsStreamAccept(T,&C->NativeServer,NULL,&WrTLSEvents,C,&S);
    testRequire(OK,"WSS native accept");return true;
}
static void wrTCPOpen(xnetstream* S,ptr Data){wrAttach(Data,S,XWS_ROLE_CLIENT);}
static void wrListenerClose(xnetlistener* L,ptr Data){(void)L;xrtAtomic32Store(&((wsRetireContext*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static void wrStart(wsRetireContext* C,bool TLS,bool Owned)
{
    memset(C,0,sizeof(*C));C->TLS=TLS;C->Owned=Owned;xrtAtomicPtrInit(&C->Client,NULL);xrtAtomicPtrInit(&C->Server,NULL);xrtAtomicPtrInit(&C->TLSClient,NULL);
    xrtAtomic32Init(&C->Entered,0);xrtAtomic32Init(&C->Permit,0);xrtAtomic32Init(&C->Returned,0);xrtAtomic32Init(&C->Refs,1);xrtAtomic32Init(&C->Drops,0);xrtAtomic32Init(&C->WaitStarted,0);xrtAtomic32Init(&C->WaitDone,0);xrtAtomic32Init(&C->WorkerDone,0);xrtAtomic32Init(&C->ListenerClosed,0);
    if(TLS){C->TLSContext=testTlsServerContext();C->Identity=testTlsServerIdentity();testRequire(C->TLSContext&&C->Identity,"WSS fixture");xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=wrVerify;V.Context=C;C->Verifier=xrtTlsVerifierCreate(&V);testRequire(C->Verifier!=NULL,"explicit WSS verifier");xrtTlsServerConfigInit(&C->NativeServer);C->NativeServer.Context=C->TLSContext;C->NativeServer.Identity=C->Identity;}
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=WS_RETIRE_BACKEND;C->Engine=xrtNetEngineCreate(&E);testRequire(C->Engine&&xrtNetEngineStart(C->Engine),"WS retirement Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents LE={wrAccept,NULL,wrListenerClose};C->Listener=xrtNetListen(C->Engine,&L,&LE,NULL,C);xnetaddr A;testRequire(C->Listener&&xrtNetListenerLocal(C->Listener,&A),"WS retirement listener");
    if(TLS){xtlsclientconfig K;xrtTlsClientConfigInit(&K);K.Context=C->TLSContext;K.Verifier=C->Verifier;K.ServerName=XRT_STR_LITERAL("retire.fixture");xtlsstream* T;
        if(Owned){xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);T=xrtTlsStreamConnectOwnedV1(C->Engine,&A,1,NULL,&K,NULL,C,&WrTLSPolicy);}
        else T=xrtTlsStreamConnect(C->Engine,&A,1,NULL,&K,NULL,&WrTLSEvents,C);
        testRequire(T!=NULL,"WSS native connect");xrtAtomicPtrStore(&C->TLSClient,T,XMEMORY_RELEASE);
    }else {xnetstreamevents EV={.Open=wrTCPOpen};testRequire(xrtNetStreamConnect(C->Engine,&A,1,NULL,&EV,C)!=NULL,"WS native connect");}
    double End=xrtTimer()+10;while(!xrtAtomicPtrLoad(&C->Client,XMEMORY_ACQUIRE)||!xrtAtomicPtrLoad(&C->Server,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"WS attached pair");xrtSleep(1);}
}
static void wrWorker(xnetworker* W,ptr Data)
{
    (void)W;wsRetireContext* C=Data;xwsstream* S=xrtAtomicPtrLoad(&C->Client,XMEMORY_ACQUIRE);
    testRequire(xrtWsStreamPollRetired(S)==(C->ExpectedPoll?XWAIT_OK:XWAIT_TIMEOUT),"WS nonblocking Worker poll");
    testRequire(xrtWsStreamWaitRetired(S,0)==XWAIT_ERROR,"WS same-Engine other Worker rejected");xrtClearError();xrtAtomic32Store(&C->WorkerDone,1,XMEMORY_RELEASE);
}
static void wrWorkerCheck(wsRetireContext* C,bool Ready)
{C->ExpectedPoll=Ready;xrtAtomic32Store(&C->WorkerDone,0,XMEMORY_RELEASE);testRequire(xrtNetPostInit(&C->WorkerPost)&&xrtNetPost(xrtNetEngineWorker(C->Engine,0),&C->WorkerPost,wrWorker,C),"WS query post");wrAwait(&C->WorkerDone,1,"Worker query must not wait for retirement locks");}
typedef struct wrWaiter {wsRetireContext* C;xwaitresult Result;} wrWaiter;
static int32 wrWaitThread(ptr Data)
{wrWaiter* W=Data;xrtAtomic32FetchAdd(&W->C->WaitStarted,1,XMEMORY_RELEASE);W->Result=xrtWsStreamWaitRetired(xrtAtomicPtrLoad(&W->C->Client,XMEMORY_ACQUIRE),5000);xrtAtomic32FetchAdd(&W->C->WaitDone,1,XMEMORY_RELEASE);return 0;}
static void wrAmbient(xwsstream* S)
{
    xrtSetErrorInfo(XERR_STATE,"ws.retire.ambient",7,"exact ambient survives");const xerror* Error=xrtGetError();xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"WS poll allocation scope");
    for(unsigned I=0;I<10000;++I)testRequire(xrtWsStreamPollRetired(S)==XWAIT_OK&&xrtGetError()==Error,"WS ready poll/ambient");
    testRequire(xrtAllocationBudgetLeave(&Scope),"WS poll allocation leave");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I)&&!I.LiveBytes&&!I.LiveCount&&!I.Rejections,"WS poll allocated");xrtAllocationBudgetFree(B);xrtClearError();
}
static void wrCase(bool TLS,bool Owned)
{
    wsRetireContext C;wrStart(&C,TLS,Owned);xwsstream* S=xrtAtomicPtrLoad(&C.Client,XMEMORY_ACQUIRE);xwsstream* Server=xrtAtomicPtrLoad(&C.Server,XMEMORY_ACQUIRE);xwsstream* Alias=xrtWsStreamRef(S);xtlsstream* NativeTLS=TLS?xrtTlsStreamRef(xrtAtomicPtrLoad(&C.TLSClient,XMEMORY_ACQUIRE)):NULL;
    xnetstream* TCP=TLS?xrtNetStreamRef(xrtTlsStreamTransport(NativeTLS)):xrtWsStreamTcpRef(S);testRequire(Alias&&TCP&&(!TLS||NativeTLS),"WS real caller/native aliases");testRequire(xrtWsStreamPollRetired(S)==XWAIT_TIMEOUT&&xrtWsStreamWaitRetired(S,0)==XWAIT_TIMEOUT,"WS open not retired");testRequire(xrtWsStreamWaitRetired(S,-2)==XWAIT_ERROR,"WS invalid timeout rejected");xrtClearError();
    testRequire(xrtWsStreamAbort(S),"WS abort");wrAwait(&C.Entered,1,"WS close callback held");testRequire(xrtWsStreamState(S)==XWS_STREAM_CLOSED&&xrtWsStreamWaitRetired(S,20)==XWAIT_TIMEOUT,"WS terminal callback is not retirement");wrWorkerCheck(&C,false);
    wrWaiter Wait[2]={{&C,XWAIT_ERROR},{&C,XWAIT_ERROR}};xthread* Threads[2];for(unsigned I=0;I<2;++I){Threads[I]=xrtThreadCreate(wrWaitThread,Wait+I,0);testRequire(Threads[I]!=NULL,"WS physical waiter");}wrAwait(&C.WaitStarted,2,"WS two waiters entered");
    testRequire(!xrtAtomic32Load(&C.WaitDone,XMEMORY_ACQUIRE),"WS wait ignored callback/aliases");xrtAtomic32Store(&C.Permit,1,XMEMORY_RELEASE);wrAwait(&C.Returned,1,"WS callback returned");testRequire(xrtWsStreamWaitRetired(S,20)==XWAIT_TIMEOUT,"WS extra user alias ignored");xrtWsStreamDestroy(Alias);
    if(TLS){if(Owned)testRequire(xrtWsStreamWaitRetired(S,20)==XWAIT_TIMEOUT,"WSS event binding ignores real TLS alias");xrtTlsStreamDestroy(NativeTLS);}
    double End=xrtTimer()+5;while(__xrtAtomicRefLoad(&S->References)!=1){testRequire(xrtTimer()<End,"WS real references did not retire (TLS cycle?)");xrtSleep(1);}
    testRequire(xrtWsStreamPollRetired(S)==XWAIT_TIMEOUT&&xrtWsStreamWaitRetired(S,20)==XWAIT_TIMEOUT&&!xrtAtomic32Load(&C.WaitDone,XMEMORY_ACQUIRE),"WS ignored native TCP alias");xrtNetStreamDestroy(TCP);
    for(unsigned I=0;I<2;++I){testRequire(xrtThreadWaitFor(Threads[I],5000)==XWAIT_OK&&Wait[I].Result==XWAIT_OK,"WS all waiters notified");xrtThreadDestroy(Threads[I]);}
    testRequire(xrtWsStreamPollRetired(S)==XWAIT_OK,"WS actual final predicate");wrAmbient(S);wrWorkerCheck(&C,true);
    testRequire(xrtMutexLock(&S->RetirementLock),"WS predicate lock hold");wrWorkerCheck(&C,false);testRequire(xrtMutexUnlock(&S->RetirementLock),"WS predicate unlock");
    testRequire(xrtMutexLock(&S->RetirementTcp->RetirementLock),"WS native predicate lock hold");wrWorkerCheck(&C,false);testRequire(xrtMutexUnlock(&S->RetirementTcp->RetirementLock),"WS native predicate unlock");
    if(xrtWsStreamState(Server)!=XWS_STREAM_CLOSED)testRequire(xrtWsStreamAbort(Server)||xrtWsStreamState(Server)==XWS_STREAM_CLOSED,"server WS abort");
    xrtClearError();testRequire(xrtWsStreamWaitRetired(Server,5000)==XWAIT_OK,"server WS retirement");
    testRequire(xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE)==(TLS&&Owned?2u:0u),"WS barrier consumed owned WS Data or leaked native TLS");testRequire(xrtNetListenerClose(C.Listener),"WS listener close");wrAwait(&C.ListenerClosed,1,"listener close callback");xrtNetListenerDestroy(C.Listener);
    xnetretireresult Early=xrtNetEngineTryDestroy(C.Engine);testRequire(Early!=XNET_RETIRE_ERROR,"WS early Engine result");if(Early==XNET_RETIRE_READY){testRequire(xrtWsStreamWaitRetired(S,0)==XWAIT_OK&&xrtWsStreamPollRetired(Server)==XWAIT_OK,"WS safe query after Engine cleanup");puts("post_engine_query: exercised");}
    xrtWsStreamDestroy(S);xrtWsStreamDestroy(Server);if(Early==XNET_RETIRE_BUSY){End=xrtTimer()+5;xnetretireresult R;while((R=xrtNetEngineTryDestroy(C.Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"WS final Engine tail");xrtSleep(1);}testRequire(R==XNET_RETIRE_READY,"WS Engine join");}
    testRequire(xrtAtomic32Load(&C.Refs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE)==(Owned?(TLS?4u:2u):0u),"WS/TLS exact callback credits");xrtTlsVerifierRelease(C.Verifier);xrtTlsIdentityRelease(C.Identity);xrtTlsContextRelease(C.TLSContext);xrtClearError();
}
int main(void)
{
    setbuf(stdout,NULL);testRequire(xrtWsStreamPollRetired(NULL)==XWAIT_ERROR&&xrtWsStreamWaitRetired(NULL,0)==XWAIT_ERROR,"WS null query");xrtClearError();
    for(unsigned T=0;T<2;++T)for(unsigned O=0;O<2;++O){printf("[case] WS retire TLS=%u owned=%u\n",T,O);wrCase(T!=0,O!=0);}
    puts("[PASS] WS physical retirement: held callback/user/native aliases/two waiters/no TLS cycle/one budget/worker rejection/trylock/ambient/no allocation/after Engine cleanup");return 0;
}
