#include "../test.h"
static xnetportbackend WpBackend;
#ifndef WS_RETIRE_BACKEND
#define WS_RETIRE_BACKEND WpBackend
#endif
#define main originalWsRetireMain
#include "test_stream_retire.c"
#undef main
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_WRITE_PROGRESS)
#include "../../src/internal/xrt_tls_stream.h"
#endif
static void wpFixture(wsRetireContext* C,bool TLS,bool Owned)
{
    memset(C,0,sizeof(*C));C->TLS=TLS;C->Owned=Owned;xrtAtomicPtrInit(&C->Client,NULL);xrtAtomicPtrInit(&C->Server,NULL);xrtAtomicPtrInit(&C->TLSClient,NULL);
    xrtAtomic32Init(&C->Entered,0);xrtAtomic32Init(&C->Permit,0);xrtAtomic32Init(&C->Returned,0);xrtAtomic32Init(&C->Refs,1);xrtAtomic32Init(&C->Drops,0);xrtAtomic32Init(&C->WaitStarted,0);xrtAtomic32Init(&C->WaitDone,0);xrtAtomic32Init(&C->WorkerDone,0);xrtAtomic32Init(&C->ListenerClosed,0);
    if(TLS){xtlslimits Limits;xrtTlsLimitsInit(&Limits);Limits.PlainLimit=32768;Limits.FeedLimit=65536;Limits.SendLimit=65536;C->TLSContext=testTlsServerContextWithLimits(&Limits);C->Identity=testTlsServerIdentity();testRequire(C->TLSContext&&C->Identity,"WSS fixture");xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=wrVerify;V.Context=C;C->Verifier=xrtTlsVerifierCreate(&V);testRequire(C->Verifier!=NULL,"explicit WSS verifier");xrtTlsServerConfigInit(&C->NativeServer);C->NativeServer.Context=C->TLSContext;C->NativeServer.Identity=C->Identity;}
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=WS_RETIRE_BACKEND;C->Engine=xrtNetEngineCreate(&E);testRequire(C->Engine&&xrtNetEngineStart(C->Engine),"WS retirement Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);L.Stream.ReadLimit=32768;xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents LE={wrAccept,NULL,wrListenerClose};C->Listener=xrtNetListen(C->Engine,&L,&LE,NULL,C);xnetaddr A;testRequire(C->Listener&&xrtNetListenerLocal(C->Listener,&A),"WS retirement listener");
    if(TLS){xtlsclientconfig K;xrtTlsClientConfigInit(&K);K.Context=C->TLSContext;K.Verifier=C->Verifier;K.ServerName=XRT_STR_LITERAL("retire.fixture");xtlsstream* T;
        if(Owned){xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);T=xrtTlsStreamConnectOwnedV1(C->Engine,&A,1,NULL,&K,NULL,C,&WrTLSPolicy);}
        else T=xrtTlsStreamConnect(C->Engine,&A,1,NULL,&K,NULL,&WrTLSEvents,C);
        testRequire(T!=NULL,"WSS native connect");xrtAtomicPtrStore(&C->TLSClient,T,XMEMORY_RELEASE);
    }else {xnetstreamevents EV={.Open=wrTCPOpen};testRequire(xrtNetStreamConnect(C->Engine,&A,1,NULL,&EV,C)!=NULL,"WS native connect");}
    double End=xrtTimer()+10;while(!xrtAtomicPtrLoad(&C->Client,XMEMORY_ACQUIRE)||!xrtAtomicPtrLoad(&C->Server,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"WS attached pair");xrtSleep(1);}
}
typedef struct wpTest {wsRetireContext Base;xatomic32 Done,Cut,Paused,Received,Released;bool InSend,Extra;unsigned Mode;uint64 First,Last;size_t Tail;} wpTest;
static void wpSnapshot(xwsstream* S,xwswriteprogress* W){W->Size=sizeof(*W);testRequire(xrtWsStreamWriteProgress(S,W)==XNET_RESULT_OK&&W->Completed<=W->Produced,"actual native write prefix");}
static void wpProgress(xwsstream* S,ptr Data)
{
    wpTest* T=Data;testRequire(!T->InSend,"observer deferred outside Send/native completion");
    if(T->Mode==1){xwswriteprogress W={.Size=sizeof(W),.Produced=11,.Completed=13},Before=W;bool Overflow=S->WriteProgressOverflow;
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_WRITE_PROGRESS)
        if(T->Base.TLS)Overflow=xrtWsStreamTls(S)->WriteProgressOverflow;
#endif
        testRequire(Overflow&&xrtWsStreamWriteProgress(S,&W)==XNET_RESULT_ERROR&&xrtErrorKind(xrtGetError())==XERR_RANGE&&!memcmp(&Before,&W,sizeof(W)),"real write counter overflow never wraps or mutates output");xrtClearError();xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);return;}
    xwswriteprogress W={.Size=sizeof(W)};wpSnapshot(S,&W);
    if(W.Staged)testRequire(T->Base.TLS&&S->OutputHead,"Staged reflects actual WSS plaintext suffix");
    if(T->Last==UINT64_MAX&&!W.Staged)T->Last=W.Produced;
    if(W.Completed>=T->First&&(W.Produced>W.Completed||W.Staged))xrtAtomic32Store(&T->Cut,1,XMEMORY_RELEASE);
    /* Native completion can certify an entire batch at once. Keep later real
     * output pending by writing reentrantly before publishing the saved cut. */
    if(!T->Mode&&!xrtAtomic32Load(&T->Cut,XMEMORY_ACQUIRE)&&W.Completed>=T->First&&W.Produced==W.Completed&&!W.Staged&&!T->Extra){
        static unsigned char More[262144];memset(More,0x5a,sizeof(More));T->Extra=true;T->InSend=true;
        testRequire(xrtWsStreamBinary(S,(xbytesview){More,sizeof(More)})==XNET_RESULT_OK,"observer reentrant bounded tail");T->InSend=false;T->Tail+=sizeof(More);wpSnapshot(S,&W);T->Last=W.Staged?UINT64_MAX:W.Produced;
        testRequire(W.Completed>=T->First&&(W.Produced>W.Completed||W.Staged),"saved cut remains retired with later real output pending");xrtAtomic32Store(&T->Cut,1,XMEMORY_RELEASE);
    }
    if(T->Mode==2){testRequire(xrtWsStreamAbort(S),"observer can abort its real transport");xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);return;}
    if(!W.Staged&&W.Completed>=T->Last){xwswriteprogress Before=W,After;testRequire(xrtWsStreamSetWriteProgress(S,NULL),"remove opaque observer");wpSnapshot(S,&After);testRequire(Before.Produced==After.Produced&&Before.Completed==After.Completed,"removal preserves permanent epoch");testRequire(xrtWsStreamSetWriteProgress(S,wpProgress),"restore observer without reset");xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);}
}
static void wpRelease(ptr Data,cbytes B,size_t N){(void)B;(void)N;xrtAtomic32FetchAdd(&((wpTest*)Data)->Released,1,XMEMORY_RELEASE);}
static void wpRead(xwsstream* S,xbytesview Data,ptr User)
{(void)S;wpTest* T=User;size_t Have=xrtAtomic32Load(&T->Received,XMEMORY_RELAXED);for(size_t I=0;I<Data.Size;++I){unsigned char Expected=Have+I<4?(unsigned char)"HEAD"[Have+I]:0x5a;testRequire(Data.Data[I]==Expected,"exact masked/decrypted data across WS frame boundaries");}xrtAtomic32Store(&T->Received,(uint32)(Have+Data.Size),XMEMORY_RELEASE);}
static void wpPause(xnetworker* W,ptr Data)
{(void)W;wpTest* T=Data;xwsstream* S=xrtAtomicPtrLoad(&T->Base.Server,XMEMORY_ACQUIRE);S->Events.MessageData=wpRead;xrtWsStreamPause(S);xrtAtomic32Store(&T->Paused,1,XMEMORY_RELEASE);}
static void wpStart(xnetworker* Worker,ptr Data)
{
    (void)Worker;wpTest* T=Data;xwsstream* S=xrtAtomicPtrLoad(&T->Base.Client,XMEMORY_ACQUIRE);xwswriteprogress V={.Size=sizeof(V),.Produced=17,.Completed=19},Saved=V;
    testRequire(xrtWsStreamWriteProgress(S,&V)==XNET_RESULT_ERROR&&!memcmp(&V,&Saved,sizeof(V)),"cursor requires enable");xrtClearError();
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"actual one-byte budget");testRequire(xrtWsStreamSetWriteProgress(S,wpProgress),"activate native WS prefix");wpSnapshot(S,&V);for(unsigned N=0;N<10000;++N)wpSnapshot(S,&V);testRequire(xrtAllocationBudgetLeave(&Scope),"leave budget");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(B,&I)&&!I.Rejections&&!I.LiveCount&&!I.PeakBytes,"activation and successful queries allocate nothing");xrtAllocationBudgetFree(B);
    V.Size=0;Saved=V;testRequire(xrtWsStreamWriteProgress(S,&V)==XNET_RESULT_ERROR&&!memcmp(&V,&Saved,sizeof(V)),"wrong output Size is atomic");xrtClearError();
    if(T->Mode==1){if(T->Base.TLS)xrtWsStreamTls(S)->WriteCompleted=UINT64_MAX-1;else S->WriteCompleted=UINT64_MAX-1;T->InSend=true;testRequire(xrtWsStreamBinary(S,(xbytesview){(cbytes)"HEAD",4})==XNET_RESULT_OK,"real write crosses UINT64");T->InSend=false;return;}
    xnetstream* Raw=xrtWsStreamTcp(S);if(!Raw)Raw=xrtTlsStreamTransport(xrtWsStreamTls(S));testRequire(xrtNetSocketSet(xrtNetStreamSocket(Raw),XNET_OPTION_SEND_BUFFER,1024),"small actual native send buffer");
    T->InSend=true;xnetref Ref={(cbytes)"HEAD",4,wpRelease,T};testRequire(xrtWsStreamBinaryRef(S,&Ref)==XNET_RESULT_OK,"first immutable masked prefix");wpSnapshot(S,&V);T->First=V.Produced;
    static unsigned char Payload[16384];memset(Payload,0x5a,sizeof(Payload));
    if(!T->Mode)for(unsigned N=0;N<256;++N){xnetresult R=xrtWsStreamBinary(S,(xbytesview){Payload,sizeof(Payload)});testRequire(R==XNET_RESULT_OK||R==XNET_RESULT_AGAIN,"bounded queued WS tail");if(R==XNET_RESULT_AGAIN)break;T->Tail+=sizeof(Payload);}
    V=(xwswriteprogress){.Size=sizeof(V)};wpSnapshot(S,&V);T->InSend=false;
    if(V.Staged){testRequire(T->Base.TLS&&S->OutputHead,"new cut explicitly excludes actual local plaintext suffix");T->Last=UINT64_MAX;}else T->Last=V.Produced;
    testRequire(xrtAtomic32Load(&T->Released,XMEMORY_ACQUIRE)==1,"masked input ownership retires independently");xrtAtomic32Store(&T->Base.WorkerDone,1,XMEMORY_RELEASE);
}
static void wpResume(xnetworker* Worker,ptr Data)
{
    (void)Worker;wpTest* T=Data;xwsstream* S=xrtAtomicPtrLoad(&T->Base.Server,XMEMORY_ACQUIRE);testRequire(xrtWsStreamResume(S),"resume real slow peer");
}
static void wpFinalCut(xnetworker* Worker,ptr Data)
{
    (void)Worker;wpTest* T=Data;xwsstream* S=xrtAtomicPtrLoad(&T->Base.Client,XMEMORY_ACQUIRE);xwswriteprogress W={.Size=sizeof(W)};testRequire(xrtWsStreamWriteProgress(S,&W)==XNET_RESULT_OK,"all physical WS tails finish");T->Last=W.Produced;if(W.Completed>=T->Last)xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);
}
static void wpCase(bool TLS,unsigned Mode,xnetportbackend Backend)
{
    WpBackend=Backend;wpTest T;wpFixture(&T.Base,TLS,false);T.Mode=Mode;T.InSend=T.Extra=false;T.First=T.Last=T.Tail=0;xrtAtomic32Init(&T.Done,0);xrtAtomic32Init(&T.Cut,0);xrtAtomic32Init(&T.Paused,0);xrtAtomic32Init(&T.Received,0);xrtAtomic32Init(&T.Released,0);xrtAtomic32Store(&T.Base.Permit,1,XMEMORY_RELEASE);
    xwsstream* C=xrtAtomicPtrLoad(&T.Base.Client,XMEMORY_ACQUIRE);xwsstream* S=xrtAtomicPtrLoad(&T.Base.Server,XMEMORY_ACQUIRE);
    if(Mode!=1){testRequire(xrtNetPostInit(&T.Base.Attach[1].Post)&&xrtNetPost(xrtWsStreamWorker(S),&T.Base.Attach[1].Post,wpPause,&T),"post pause");wrAwait(&T.Paused,1,"real receiver pause");}
    testRequire(xrtNetPostInit(&T.Base.WorkerPost)&&xrtNetPost(xrtWsStreamWorker(C),&T.Base.WorkerPost,wpStart,&T),"post initial prefix");
    if(!Mode){wrAwait(&T.Base.WorkerDone,1,"send phase complete");testRequire(T.Tail>0,"real queued tail");testRequire(xrtNetPostInit(&T.Base.Attach[1].Post)&&xrtNetPost(xrtWsStreamWorker(S),&T.Base.Attach[1].Post,wpResume,&T),"post resume");wrAwait(&T.Done,1,"native prefix completion without a whole-stream Drain assumption");wrAwait(&T.Cut,1,"saved cut stays independent of later real output");wrAwait(&T.Received,(uint32)(4+T.Tail),"all exact binary bytes delivered");
        if(T.Last==UINT64_MAX){testRequire(xrtNetPostInit(&T.Base.WorkerPost)&&xrtNetPost(xrtWsStreamWorker(C),&T.Base.WorkerPost,wpFinalCut,&T),"post deferred WSS cut");}}
    wrAwait(&T.Done,1,"physical native cursor completion");xwswriteprogress Wrong={.Size=sizeof(Wrong),.Produced=31,.Completed=37},Before=Wrong;testRequire(xrtWsStreamWriteProgress(C,&Wrong)==XNET_RESULT_ERROR&&!memcmp(&Wrong,&Before,sizeof(Wrong))&&!xrtWsStreamSetWriteProgress(C,NULL),"foreign owner leaves output unchanged");xrtClearError();
    xrtWsStreamAbort(C);xrtWsStreamAbort(S);testRequire(xrtWsStreamWaitRetired(C,5000)==XWAIT_OK&&xrtWsStreamWaitRetired(S,5000)==XWAIT_OK,"physical WS/native retirement");testRequire(!C->WriteProgress&&!C->WriteProgressPending,"close clears observer before context retirement");xrtWsStreamDestroy(C);xrtWsStreamDestroy(S);xrtNetListenerClose(T.Base.Listener);wrAwait(&T.Base.ListenerClosed,1,"listener physically closed");xrtNetListenerDestroy(T.Base.Listener);testRequire(xrtNetEngineDestroy(T.Base.Engine),"Engine joins all actual native and deferred callbacks");xrtTlsVerifierRelease(T.Base.Verifier);xrtTlsIdentityRelease(T.Base.Identity);xrtTlsContextRelease(T.Base.TLSContext);printf("[PASS] WS prefix TLS%u mode%u\n",TLS,Mode);
}
int main(int Argc,char** Argv)
{setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;wpCase(false,0,B);wpCase(true,0,B);wpCase(false,1,B);wpCase(true,1,B);wpCase(false,2,B);wpCase(true,2,B);return 0;}
