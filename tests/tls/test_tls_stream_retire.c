#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_tls_stream.h"
typedef struct tlsRetireContext {
    xnetengine* Engine;xnetlistener* Listener;xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;
    xtlsstream* Client;xatomicptr Server;xtlsserverconfig ServerConfig;
    xatomic32 Opened,Entered,Permit,Returned,WorkerDone,BindDone,Refs,Drops,WaitStarted,WaitDone,ListenerClosed;
    bool Owned,Reject,Hold,ExpectedPoll; xnetpost WorkerPost,BindPost;
} tlsRetireContext;
static void tlsRetireAwait(xatomic32* A,uint32 N,cstr Message)
{double End=xrtTimer()+10;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,Message);xrtSleep(1);}}
static bool tlsRetireCount(const void* Data,size_t* Count)
{*Count=xrtAtomic32Load(&((const tlsRetireContext*)Data)->Refs,XMEMORY_ACQUIRE);return true;}
static bool tlsRetireTrace(const void* D,xrtownershipvisitor V,ptr U)
{(void)D;(void)V;(void)U;return false;}
static void tlsRetireDrop(const void* Data)
{tlsRetireContext* C=(tlsRetireContext*)Data;xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&C->Drops,1,XMEMORY_RELEASE);}
static const xrtownershipops TlsRetireOps={tlsRetireCount,tlsRetireTrace};
static void tlsRetireOpen(xtlsstream* S,ptr Data)
{(void)S;tlsRetireContext* C=Data;xrtAtomic32FetchAdd(&C->Opened,1,XMEMORY_RELEASE);}
static void tlsRetireClosed(xtlsstream* S,xnetresult R,const xerror* Error,ptr Data)
{
    (void)R;(void)Error;tlsRetireContext* C=Data;if(xrtTlsSessionRole(xrtTlsStreamSession(S))!=XTLS_CLIENT)return;
    testRequire(xrtTlsStreamPollRetired(S)==XWAIT_TIMEOUT,"TLS own close callback cannot be retired");
    testRequire(xrtTlsStreamWaitRetired(S,0)==XWAIT_ERROR,"TLS close callback Worker wait rejected");xrtClearError();
    xrtAtomic32Store(&C->Entered,1,XMEMORY_RELEASE);if(C->Hold)tlsRetireAwait(&C->Permit,1,"TLS held close callback release");
    xrtAtomic32Store(&C->Returned,1,XMEMORY_RELEASE);
}
static const xtlsstreamevents TlsRetireEvents={.Open=tlsRetireOpen,.Close=tlsRetireClosed};
static const xtlsstreamownershipv1 TlsRetirePolicy={sizeof(xtlsstreamownershipv1),{.Open=tlsRetireOpen,.Close=tlsRetireClosed},tlsRetireDrop,&TlsRetireOps};
static xtlsverifydecision tlsRetireVerify(const xtlspeer* Peer,ptr Data)
{
    tlsRetireContext* C=Data;xbytesview Pin={0};
    if(C->Reject || !Peer || Peer->Role!=XTLS_SERVER || !Peer->CertificateCount || Peer->Name.Size!=14 || memcmp(Peer->Name.Data,"retire.fixture",14) || !xrtTlsIdentityCertificate(C->Identity,0,&Pin))return XTLS_VERIFY_REJECT;
    return Peer->Certificates[0].Raw.Size==Pin.Size && !memcmp(Peer->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;
}
static bool tlsRetireAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;tlsRetireContext* C=Data;xtlsstream* TLS=NULL;bool OK;
    if(C->Owned){xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);OK=xrtTlsStreamAcceptOwnedV1(S,&C->ServerConfig,NULL,C,&TlsRetirePolicy,&TLS);if(!OK)xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_RELAXED);}
    else OK=xrtTlsStreamAccept(S,&C->ServerConfig,NULL,&TlsRetireEvents,C,&TLS);
    testRequire(OK,"TLS retirement server accept");xrtAtomicPtrStore(&C->Server,TLS,XMEMORY_RELEASE);return true;
}
static void tlsRetireListenerClose(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((tlsRetireContext*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static void tlsRetireStart(tlsRetireContext* C,bool Owned,bool Reject,bool Hold)
{
    memset(C,0,sizeof(*C));C->Owned=Owned;C->Reject=Reject;C->Hold=Hold;xrtAtomicPtrInit(&C->Server,NULL);
    xrtAtomic32Init(&C->Opened,0);xrtAtomic32Init(&C->Entered,0);xrtAtomic32Init(&C->Permit,0);xrtAtomic32Init(&C->Returned,0);xrtAtomic32Init(&C->WorkerDone,0);xrtAtomic32Init(&C->BindDone,0);xrtAtomic32Init(&C->Refs,1);xrtAtomic32Init(&C->Drops,0);xrtAtomic32Init(&C->WaitStarted,0);xrtAtomic32Init(&C->WaitDone,0);xrtAtomic32Init(&C->ListenerClosed,0);
    C->Context=testTlsServerContext();C->Identity=testTlsServerIdentity();testRequire(C->Context && C->Identity,"TLS retirement fixture identity");
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=tlsRetireVerify;V.Context=C;C->Verifier=xrtTlsVerifierCreate(&V);testRequire(C->Verifier!=NULL,"TLS retirement explicit DER pin");
    xrtTlsServerConfigInit(&C->ServerConfig);C->ServerConfig.Context=C->Context;C->ServerConfig.Identity=C->Identity;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;C->Engine=xrtNetEngineCreate(&E);testRequire(C->Engine && xrtNetEngineStart(C->Engine),"TLS retirement Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents Events={tlsRetireAccept,NULL,tlsRetireListenerClose};C->Listener=xrtNetListen(C->Engine,&L,&Events,NULL,C);xnetaddr Remote;
    testRequire(C->Listener && xrtNetListenerLocal(C->Listener,&Remote),"TLS retirement listener");xtlsclientconfig Client;xrtTlsClientConfigInit(&Client);Client.Context=C->Context;Client.Verifier=C->Verifier;Client.ServerName=XRT_STR_LITERAL("retire.fixture");
    if(Owned){xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);C->Client=xrtTlsStreamConnectOwnedV1(C->Engine,&Remote,1,NULL,&Client,NULL,C,&TlsRetirePolicy);}
    else C->Client=xrtTlsStreamConnect(C->Engine,&Remote,1,NULL,&Client,NULL,&TlsRetireEvents,C);
    testRequire(C->Client!=NULL,"TLS retirement client connect");
}
static void tlsRetireWorker(xnetworker* W,ptr Data)
{
    (void)W;tlsRetireContext* C=Data;testRequire(xrtTlsStreamPollRetired(C->Client)==(C->ExpectedPoll?XWAIT_OK:XWAIT_TIMEOUT),"TLS Worker nonblocking query");
    testRequire(xrtTlsStreamWaitRetired(C->Client,0)==XWAIT_ERROR,"TLS every same-Engine Worker rejects wait");xrtClearError();xrtAtomic32Store(&C->WorkerDone,1,XMEMORY_RELEASE);
}
static void tlsRetireWorkerCheck(tlsRetireContext* C,bool Ready)
{
    C->ExpectedPoll=Ready;xrtAtomic32Store(&C->WorkerDone,0,XMEMORY_RELEASE);testRequire(xrtNetPostInit(&C->WorkerPost) && xrtNetPost(xrtNetEngineWorker(C->Engine,0),&C->WorkerPost,tlsRetireWorker,C),"TLS retirement query post");tlsRetireAwait(&C->WorkerDone,1,"TLS retirement Worker query must not block");
}
static void tlsRetireBind(xnetworker* W,ptr Data)
{
    (void)W;tlsRetireContext* C=Data;testRequire(xrtTlsStreamBindEventsOwnedV1(C->Client,C,&TlsRetirePolicy),"TLS exclusive owned event handoff");xrtAtomic32Store(&C->BindDone,1,XMEMORY_RELEASE);
}
typedef struct tlsRetireWaiter {tlsRetireContext* C;xwaitresult Result;} tlsRetireWaiter;
static int32 tlsRetireWaitThread(ptr Data)
{
    tlsRetireWaiter* W=Data;xrtAtomic32FetchAdd(&W->C->WaitStarted,1,XMEMORY_RELEASE);W->Result=xrtTlsStreamWaitRetired(W->C->Client,5000);xrtAtomic32FetchAdd(&W->C->WaitDone,1,XMEMORY_RELEASE);return 0;
}
static void tlsRetireAmbient(xtlsstream* S)
{
    xrtSetErrorInfo(XERR_STATE,"retire.ambient",7,"keep exact error");const xerror* Error=xrtGetError();xallocationbudget* B=NULL;xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"TLS allocation-free poll scope");
    for(unsigned N=0;N<10000;++N)testRequire(xrtTlsStreamPollRetired(S)==XWAIT_OK && xrtGetError()==Error,"TLS poll preserves exact ambient error");
    testRequire(xrtAllocationBudgetLeave(&Scope),"TLS poll scope leave");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes && !I.Rejections,"TLS ready poll made allocation");xrtAllocationBudgetFree(B);xrtClearError();
}
static void tlsRetireCase(bool Owned,bool Reject)
{
    tlsRetireContext C;tlsRetireStart(&C,Owned,Reject,true);xtlsstream* Alias=xrtTlsStreamRef(C.Client);xnetstream* TCP=xrtNetStreamRef(xrtTlsStreamTransport(C.Client));testRequire(Alias && TCP,"TLS actual caller aliases");
    if(!Reject){tlsRetireAwait(&C.Opened,2,"TLS real verified handshake");if(Owned){xrtAtomic32FetchAdd(&C.Refs,1,XMEMORY_RELAXED);testRequire(xrtNetPostInit(&C.BindPost) && xrtNetPost(xrtNetEngineWorker(C.Engine,1),&C.BindPost,tlsRetireBind,&C),"TLS bind post");tlsRetireAwait(&C.BindDone,1,"TLS bind complete");}}
    xfuture* Closed=xrtTlsStreamWaitAsync(C.Client,XTLS_STREAM_WAIT_CLOSE);testRequire(Closed!=NULL,"TLS actual async condition");if(!Reject)testRequire(xrtTlsStreamAbort(C.Client),"TLS retirement abort");
    tlsRetireAwait(&C.Entered,1,"TLS terminal callback entered");testRequire((xrtTlsStreamState(C.Client)==XTLS_STREAM_CLOSED || xrtTlsStreamState(C.Client)==XTLS_STREAM_FAILED) && xrtTlsStreamPollRetired(C.Client)==XWAIT_TIMEOUT && xrtTlsStreamWaitRetired(C.Client,0)==XWAIT_TIMEOUT,"TLS terminal state is not retirement");tlsRetireWorkerCheck(&C,false);
    tlsRetireWaiter W[2]={{&C,XWAIT_ERROR},{&C,XWAIT_ERROR}};xthread* Threads[2];for(unsigned N=0;N<2;++N){Threads[N]=xrtThreadCreate(tlsRetireWaitThread,W+N,0);testRequire(Threads[N]!=NULL,"TLS retirement waiter thread");}tlsRetireAwait(&C.WaitStarted,2,"TLS retirement waiters entered");
    testRequire(xrtTlsStreamWaitRetired(C.Client,20)==XWAIT_TIMEOUT && !xrtAtomic32Load(&C.WaitDone,XMEMORY_ACQUIRE),"TLS held callback timeout/early success");
    xrtTlsStreamDestroy(Alias);xrtAtomic32Store(&C.Permit,1,XMEMORY_RELEASE);tlsRetireAwait(&C.Returned,1,"TLS close callback returned");testRequire(xrtFutureWaitFor(Closed,5000)==XWAIT_OK,"TLS async close completion");xrtFutureDestroy(Closed);
    double End=xrtTimer()+5;while(__xrtAtomicRefLoad(&C.Client->References)!=1){testRequire(xrtTimer()<End,"TLS transient references did not retire");xrtSleep(1);}
    testRequire(xrtTlsStreamPollRetired(C.Client)==XWAIT_TIMEOUT && xrtTlsStreamWaitRetired(C.Client,20)==XWAIT_TIMEOUT && !xrtAtomic32Load(&C.WaitDone,XMEMORY_ACQUIRE),"TLS predicate omitted native TCP alias/tail");xrtNetStreamDestroy(TCP);
    for(unsigned N=0;N<2;++N){testRequire(xrtThreadWaitFor(Threads[N],5000)==XWAIT_OK && W[N].Result==XWAIT_OK,"TLS all physical waiters notified");xrtThreadDestroy(Threads[N]);}
    testRequire(xrtTlsStreamPollRetired(C.Client)==XWAIT_OK,"TLS final physical predicate");tlsRetireAmbient(C.Client);tlsRetireWorkerCheck(&C,true);
    testRequire(xrtMutexLock(&C.Client->RetirementLock),"TLS hold predicate mutex");tlsRetireWorkerCheck(&C,false);testRequire(xrtMutexUnlock(&C.Client->RetirementLock),"TLS predicate unlock");
    TCP=xrtTlsStreamTransport(C.Client);testRequire(xrtMutexLock(&TCP->RetirementLock),"TLS hold native predicate mutex");tlsRetireWorkerCheck(&C,false);testRequire(xrtMutexUnlock(&TCP->RetirementLock),"TLS native predicate unlock");
    xtlsstream* Server=xrtAtomicPtrLoad(&C.Server,XMEMORY_ACQUIRE);testRequire(Server && xrtTlsStreamAbort(Server) && xrtTlsStreamWaitRetired(Server,5000)==XWAIT_OK,"TLS server physical retirement");
    testRequire(!xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE),"TLS retirement consumed session/event context owner");
    testRequire(xrtNetListenerClose(C.Listener),"TLS listener close");tlsRetireAwait(&C.ListenerClosed,1,"TLS listener callback");xrtNetListenerDestroy(C.Listener);
    xnetretireresult Early=xrtNetEngineTryDestroy(C.Engine);testRequire(Early!=XNET_RETIRE_ERROR,"TLS early Engine retirement result");
    if(Early==XNET_RETIRE_READY){testRequire(xrtTlsStreamWaitRetired(C.Client,0)==XWAIT_OK && xrtTlsStreamPollRetired(Server)==XWAIT_OK,"TLS retired owners after Engine cleanup");puts("post_engine_query: exercised");}
    /* Retired caller-owned sessions may still hold pooled ciphertext/plaintext
     * buffers. They are resources, not live callbacks; drop the owners first. */
    xrtTlsStreamDestroy(C.Client);xrtTlsStreamDestroy(Server);
    if(Early==XNET_RETIRE_BUSY){End=xrtTimer()+5;xnetretireresult R;while((R=xrtNetEngineTryDestroy(C.Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"TLS Engine tail after owner destruction");xrtSleep(1);}testRequire(R==XNET_RETIRE_READY,"TLS final Engine cleanup");}
    testRequire(xrtAtomic32Load(&C.Refs,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE)==(Owned?(Reject?2u:3u):0u),"TLS exact context owner drops");xrtTlsVerifierRelease(C.Verifier);xrtTlsIdentityRelease(C.Identity);xrtTlsContextRelease(C.Context);xrtClearError();
}
int main(void)
{
    testRequire(xrtTlsStreamPollRetired(NULL)==XWAIT_ERROR && xrtTlsStreamWaitRetired(NULL,0)==XWAIT_ERROR,"TLS null retirement arguments");xrtClearError();tlsRetireCase(true,false);tlsRetireCase(false,false);tlsRetireCase(true,true);puts("[PASS] TLS retirement: terminal callback/aliases/native tails/two waiters/deadline/worker rejection/nonblocking mutex contention/zero allocation/ambient preservation/owned binding/failed verify/resource-aware Engine cleanup");return 0;
}
