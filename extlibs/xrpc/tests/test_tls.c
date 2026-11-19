#include "test.h"
#include "network_extensions.h"
#include <xrpc/tls.h>
#include <xrt/thread.h>
#include "../../../tests/fixtures/tls_server.h"
#include <string.h>
typedef struct tlsTest {
    xnetengine* Engine;xnetlistener* Listener;xrpcserver* Server;xrpctls *Left,*Right;
    xrpctlsconfig Config;xatomic32 Ready,Calls,Held,Permit,RemoteEntered,RemoteCancelled;char Context[4];
    xtlscontext* TLSContext;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig TLSConfig;
    bool Reject,AdoptExisting,AdoptPosted;
    char ServerProtocolData[8];xstrview ServerProtocol;
    xtlsstream *ToAdopt,*Alias;xfuture* Early;
    xnetpost AdoptPost;xatomic32 ContextRefs,ContextDrops,BufferedBeforeAdopt,VerifyEntered,VerifyPermit;
} tlsTest;
static tlsTest* TlsActive;
static size_t TlsTestTimerLimit;
static bool TlsTestAlpn;
static bool tlsTestMethod(const xrpcmessage* M,cstr Name)
{size_t N=strlen(Name);return M->Method.Raw.Size==N+2 && !memcmp(M->Method.Raw.Data+1,Name,N);}
static bool tlsTestBusiness(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* Context,ptr User)
{
    tlsTest* T=User;testRequire(!xrtNetEngineCurrent(T->Engine),"TLS business executed on a network worker");
    testRequire(Context->Data.Size==4 && !memcmp(Context->Data.Data,"meta",4),"TLS request context not independently copied");
    xrtAtomic32FetchAdd(&T->Calls,1,XMEMORY_RELAXED);
    if(tlsTestMethod(M,"hold")){xrtAtomic32Store(&T->Held,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&T->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
    if(tlsTestMethod(M,"remote-hold"))
        return rpcTestNetworkHeld(Context,&T->RemoteEntered,&T->RemoteCancelled);
    if(tlsTestMethod(M,"error"))return xrpcReplyError(R,XRT_STR_LITERAL("-100"),XRT_STR_LITERAL("error"),(xstrview){0});
    return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}
static xtlsverifydecision tlsTestVerify(const xtlspeer* Peer,ptr Data)
{
    tlsTest* T=Data;xbytesview Pin={0};if(T->Reject){xrtAtomic32Store(&T->VerifyEntered,1,XMEMORY_RELEASE);double End=xrtTimer()+10;while(!xrtAtomic32Load(&T->VerifyPermit,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TLS held verifier release");xrtSleep(1);}return XTLS_VERIFY_REJECT;}if(!Peer || !Peer->CertificateCount || Peer->Name.Size!=11 || memcmp(Peer->Name.Data,"rpc.fixture",11) || !xrtTlsIdentityCertificate(T->Identity,0,&Pin))return XTLS_VERIFY_REJECT;
    return Peer->Certificates[0].Raw.Size==Pin.Size && !memcmp(Peer->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;
}
static bool tlsTestOriginalCount(const void* Data,size_t* Count)
{*Count=xrtAtomic32Load(&((const tlsTest*)Data)->ContextRefs,XMEMORY_ACQUIRE);return true;}
static bool tlsTestOriginalTrace(const void* Data,xrtownershipvisitor V,ptr U)
{(void)Data;(void)V;(void)U;return false;}
static void tlsTestOriginalDrop(const void* Data)
{tlsTest* T=(tlsTest*)Data;xrtAtomic32FetchSub(&T->ContextRefs,1,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&T->ContextDrops,1,XMEMORY_RELEASE);}
static const xrtownershipops TlsTestOriginalOps={tlsTestOriginalCount,tlsTestOriginalTrace};
static void tlsTestAdoptWorker(xnetworker* W,ptr Data)
{
    (void)W;tlsTest* T=Data;T->Alias=xrtTlsStreamRef(T->ToAdopt);testRequire(T->Alias && xrtTlsStreamAvailable(T->ToAdopt)>0,"TLS adoption lost already buffered plaintext");
    testRequire(xrpcTlsAttach(T->ToAdopt,&T->Config,&T->Right),"TLS exclusive native event handoff");xrtAtomic32Store(&T->Ready,1,XMEMORY_RELEASE);
}
static void tlsTestOriginalOpen(xtlsstream* S,ptr Data)
{
    tlsTest* T=Data;xrpctls* Out=NULL;testRequire(!xrpcTlsAttach(S,&T->Config,&Out) && !Out,"TLS active callback adopted");xrtClearError();
}
static void tlsTestOriginalRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{
    (void)B;tlsTest* T=Data;if(T->AdoptPosted)return;T->AdoptPosted=true;xrtAtomic32Store(&T->BufferedBeforeAdopt,1,XMEMORY_RELEASE);
    testRequire(xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(S)),&T->AdoptPost,tlsTestAdoptWorker,T),"TLS owner post after real buffered Read");
}
static const xtlsstreamownershipv1 TlsTestOriginalPolicy={sizeof(xtlsstreamownershipv1),{.Open=tlsTestOriginalOpen,.Read=tlsTestOriginalRead},tlsTestOriginalDrop,&TlsTestOriginalOps};
static bool tlsTestAccept(xnetlistener* L,xnetstream* S,ptr User)
{
    (void)L;tlsTest* T=User;
    if(T->AdoptExisting){xrtAtomic32FetchAdd(&T->ContextRefs,1,XMEMORY_RELAXED);bool Native=xrtTlsStreamAcceptOwnedV1(S,&T->TLSConfig,&T->Config.TLS,T,&TlsTestOriginalPolicy,&T->ToAdopt);if(!Native)xrtAtomic32FetchSub(&T->ContextRefs,1,XMEMORY_RELAXED);return Native;}
    bool OK=xrpcTlsAccept(S,&T->TLSConfig,&T->Config,&T->Right);
    if(!OK)fprintf(stderr,"TLS accept failed: %s %d %s\n",xrtErrorDomain(xrtGetError()),(int)xrtErrorCode(xrtGetError()),xrtErrorMessage(xrtGetError()));
    if(OK){testRequire(xrpcTlsWaitFor(T->Right,0)==XWAIT_ERROR && !xrpcTlsDestroy(T->Right),"TLS self-worker wait/destroy accepted");xrtClearError();xrtAtomic32Store(&T->Ready,1,XMEMORY_RELEASE);}return OK;
}
static void tlsTestStartEx(tlsTest* T,unsigned Format,bool Fragment,bool Adopt,bool Reject,int64 Incoming)
{
    TlsActive=T;memset(T,0,sizeof(*T));T->AdoptExisting=Adopt;T->Reject=Reject;xrtAtomic32Init(&T->VerifyEntered,0);xrtAtomic32Init(&T->VerifyPermit,0);xrtAtomic32Init(&T->ContextRefs,1);xrtAtomic32Init(&T->ContextDrops,0);xrtAtomic32Init(&T->BufferedBeforeAdopt,0);testRequire(xrtNetPostInit(&T->AdoptPost),"TLS adoption post init");xrtAtomic32Init(&T->Ready,0);xrtAtomic32Init(&T->Calls,0);xrtAtomic32Init(&T->Held,0);xrtAtomic32Init(&T->Permit,0);xrtAtomic32Init(&T->RemoteEntered,0);xrtAtomic32Init(&T->RemoteCancelled,0);memcpy(T->Context,"meta",4);
    T->TLSContext=testTlsServerContext();T->Identity=testTlsServerIdentity();testRequire(T->TLSContext && T->Identity,"TLS fixture context/identity");
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=tlsTestVerify;V.Context=T;T->Verifier=xrtTlsVerifierCreate(&V);testRequire(T->Verifier!=NULL,"TLS explicit fixture pin");
    xrtTlsServerConfigInit(&T->TLSConfig);T->TLSConfig.Context=T->TLSContext;T->TLSConfig.Identity=T->Identity;
    if(TlsTestAlpn){memcpy(T->ServerProtocolData,"rpc-test",8);T->ServerProtocol=(xstrview){T->ServerProtocolData,8};T->TLSConfig.Protocols=&T->ServerProtocol;T->TLSConfig.ProtocolCount=1;}
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;if(TlsTestTimerLimit)E.TimerLimit=TlsTestTimerLimit;T->Engine=xrtNetEngineCreate(&E);testRequire(T->Engine && xrtNetEngineStart(T->Engine),"TLS Engine start");
    xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=tlsTestBusiness;S.User=T;testRequire(xrpcServerCreate(&S,&T->Server),"TLS shared executor");
    xrpcTlsConfigInit(&T->Config);T->Config.Duplex.Server=T->Server;T->Config.RequestContext=(xbytesview){(const unsigned char*)T->Context,4};T->Config.Frame.Format=(xrpcframeformat)Format;
    T->Config.Duplex.IncomingTimeout=Incoming;
    if(Incoming==RPC_TEST_REMOTE_LIMIT)T->Config.Frame.MaxBodyBytes=256;
    if(Fragment){T->Config.ReadChunkBytes=3;T->Config.ReadBytes=7;T->Config.WriteChunkBytes=1;T->Config.WriteBytes=5;T->Config.WorkFrames=2;T->Config.MaxCipherBytes=256;}
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);L.Stream=T->Config.Stream;
    xnetlistenerevents Events={tlsTestAccept,NULL,NULL};T->Listener=xrtNetListen(T->Engine,&L,&Events,NULL,T);testRequire(T->Listener!=NULL,"TLS listen");xnetaddr Remote;
    xtlsclientconfig Client;xrtTlsClientConfigInit(&Client);Client.Context=T->TLSContext;Client.Verifier=T->Verifier;char Name[]="rpc.fixture";Client.ServerName=(xstrview){Name,11};char ProtocolData[]="rpc-test";xstrview Protocol={ProtocolData,8};if(TlsTestAlpn){Client.Protocols=&Protocol;Client.ProtocolCount=1;}
    testRequire(xrtNetListenerLocal(T->Listener,&Remote) && xrpcTlsConnect(T->Engine,&Remote,1,&Client,&T->Config,&T->Left),"TLS numeric connect");memset(Name,'x',11);memset(ProtocolData,'x',8);
    if(Adopt)testRequire(xrpcTlsCall(T->Left,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[42]"),5000,&T->Early)==XRPC_DUPLEX_ACCEPTED,"TLS pre-adoption call");
    double End=xrtTimer()+10;while(!xrtAtomic32Load(&T->Ready,XMEMORY_ACQUIRE)){if(xrtTimer()>=End){xrpctlsinfo I={0};I.Size=sizeof(I);xrpcTlsInfo(T->Left,&I);fprintf(stderr,"left state=%d reason=%d timer=%llu posts=%llu native=%s/%d/%d\n",I.State,I.Reason,(unsigned long long)I.TimerFailures,(unsigned long long)I.PostFailures,I.NativeErrorDomain,I.NativeErrorKind,I.NativeErrorCode);testRequire(false,"TLS accept did not attach");}xrtSleep(1);}memset(T->Context,'x',4);if(TlsTestAlpn)memset(T->ServerProtocolData,'x',8);
}
static void tlsTestStart(tlsTest* T,unsigned Format,bool Fragment)
{tlsTestStartEx(T,Format,Fragment,false,false,5000);}
static const xrpccallresult* tlsTestResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{xfutureresult R;testRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"TLS typed Future completion");const xrpccallresult* C=R.Value;if(C->Status!=Status || C->Delivery!=Delivery){fprintf(stderr,"TLS result actual=%d/%d expected=%d/%d\n",C->Status,C->Delivery,Status,Delivery);if(TlsActive){for(unsigned N=0;N<2;++N){xrpctls* P=N?TlsActive->Right:TlsActive->Left;if(!P)continue;xrpctlsinfo I={0};I.Size=sizeof(I);xrpcTlsInfo(P,&I);fprintf(stderr,"peer%u state=%d reason=%d bytes=%llu/%llu frames=%llu/%llu cipher=%zu blocked=%llu short=%llu native=%s/%d/%d\n",N,I.State,I.Reason,(unsigned long long)I.ReadBytes,(unsigned long long)I.WriteBytes,(unsigned long long)I.ReadFrames,(unsigned long long)I.WriteFrames,I.CipherBytes,(unsigned long long)I.BlockedWrites,(unsigned long long)I.ShortWrites,I.NativeErrorDomain,I.NativeErrorKind,I.NativeErrorCode);}}}testRequire(C->Status==Status && C->Delivery==Delivery,"TLS result status/delivery");return C;}
static void tlsTestFinish(tlsTest* T)
{
    testRequire(xrpcTlsClose(T->Left) && xrpcTlsClose(T->Right),"TLS close");
    testRequire(xrpcTlsWaitFor(T->Left,10000)==XWAIT_OK && xrpcTlsWaitFor(T->Right,10000)==XWAIT_OK,"TLS physical peer retirement");
    xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(T->Left,&I) && I.State==XRPC_TLS_CLOSED && !I.Submitters && I.Duplex.Drained && !I.PostFailures,"TLS close accounting");
    testRequire(xrpcTlsDestroy(T->Left) && xrpcTlsDestroy(T->Right),"TLS destroy");
    testRequire(xrtNetListenerClose(T->Listener),"TLS listener close");xrtNetListenerDestroy(T->Listener);testRequire(xrpcServerDestroy(T->Server),"TLS executor physical join");
    double End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(T->Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"TLS engine cleanup/pin/handle leak");xrtSleep(1);}testRequire(Q==XNET_RETIRE_READY,"TLS engine physical join");
    xrtTlsVerifierRelease(T->Verifier);xrtTlsIdentityRelease(T->Identity);xrtTlsContextRelease(T->TLSContext);
}
static void testTlsDuplex(void)
{
    for(unsigned Format=1;Format<=2;++Format){printf("[case] TLS duplex format%u\n",Format);tlsTest T;tlsTestStart(&T,Format,true);xfuture* F[8];
        char Method[]="echo",Params[]="[1,2,3]";
        testRequire(xrpcTlsCall(T.Left,(xstrview){Method,4},(xstrview){Params,7},10000,F)==XRPC_DUPLEX_ACCEPTED,"TLS call admit");memset(Method,'x',4);memset(Params,'x',7);
        testRequire(xrpcTlsCall(T.Right,XRT_STR_LITERAL("error"),(xstrview){0},10000,F+1)==XRPC_DUPLEX_ACCEPTED,"TLS reverse call admit");
        testRequire(xrpcTlsNotify(T.Left,XRT_STR_LITERAL("note"),(xstrview){0},10000,F+2)==XRPC_DUPLEX_ACCEPTED,"TLS notify admit");
        xrpcduplexitem Batch[]={{false,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]")},{true,XRT_STR_LITERAL("note"),(xstrview){0}},{false,XRT_STR_LITERAL("error"),(xstrview){0}}};
        testRequire(xrpcTlsBatch(T.Right,Batch,3,10000,F+3)==XRPC_DUPLEX_ACCEPTED,"TLS batch admit");
        const xrpccallresult* R=tlsTestResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);xrpcresponseinfo I={0};I.Size=sizeof(I);
        testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[R->Index].Result.Raw.Size==7 && !memcmp(I.Messages[R->Index].Result.Raw.Data,"[1,2,3]",7),"TLS input snapshot/correlation");
        tlsTestResult(F[1],XRPC_CALL_REMOTE_ERROR,XRPC_REPLIED);tlsTestResult(F[2],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
        const xrpcbatchcallresult* B=(const xrpcbatchcallresult*)tlsTestResult(F[3],XRPC_CALL_BATCH_DONE,XRPC_REPLIED);testRequire(B->Count==3 && B->Indices[0]==0 && B->Indices[1]==SIZE_MAX && B->Indices[2]==1,"TLS batch notification map");
        testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("echo"),(xstrview){0},0,F+4)==XRPC_DUPLEX_ACCEPTED,"TLS zero deadline admit");tlsTestResult(F[4],XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);
        tlsTestFinish(&T);tlsTestResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);for(size_t N=0;N<5;++N)xrtFutureDestroy(F[N]);
    }
}
static void testTlsExtensions(void)
{
    for(unsigned Format=1;Format<=2;++Format){
        tlsTest T;
        tlsTestStartEx(&T,Format,true,false,false,RPC_TEST_REMOTE_LIMIT);
        testRequire(xrpcTlsEnableExtensions(T.Left,RPC_TEST_EXTENSIONS)==XRPC_DUPLEX_ACCEPTED &&
            xrpcTlsEnableExtensions(T.Right,RPC_TEST_EXTENSIONS)==XRPC_DUPLEX_ACCEPTED,
            "enable network extensions on encrypted peers");
        xfuture* F=NULL;
        testRequire(xrpcTlsNegotiateExtension(T.Left,XRPC_DUPLEX_EXTENSION_DEADLINE,5000,&F)==XRPC_DUPLEX_ACCEPTED,
            "negotiate deadline across encrypted frames");
        rpcTestNetworkOfferResult(F,true);
        testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("remote-hold"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,
            "send finite encrypted request");
        rpcTestNetworkDeadlineResult(F);

        xrtAtomic32Store(&T.RemoteEntered,0,XMEMORY_RELEASE);
        testRequire(xrpcTlsNegotiateExtension(T.Left,XRPC_DUPLEX_EXTENSION_CANCELLATION,5000,&F)==
            XRPC_DUPLEX_ACCEPTED,"negotiate remote cancellation after deadline");
        rpcTestNetworkOfferResult(F,true);
        testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("remote-hold"),(xstrview){0},5000,&F)==
            XRPC_DUPLEX_ACCEPTED,"send job for remote cancellation");
        rpcTestNetworkWait(&T.RemoteEntered);
        testRequire(xrtFutureCancel(F),"cancel network Future");
        rpcTestNetworkReply(F,XRPC_CALL_CANCELLED,XRPC_MAY_HAVE_EXECUTED);
        rpcTestNetworkWait(&T.RemoteCancelled);
        xrtFutureDestroy(F);

        char Params[181];
        memset(Params,' ',sizeof(Params));
        Params[0]='[';
        Params[sizeof(Params)-1]=']';
        xfuture* Outside=(xfuture*)(uintptr_t)1;
        testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("echo"),(xstrview){Params,sizeof(Params)},5000,&Outside)==
            XRPC_DUPLEX_ERROR && xrtErrorCode(xrtGetError())==XRPC_DUPLEX_LIMIT && Outside==(xfuture*)(uintptr_t)1,
            "deadline envelope counts against network frame limit before admission");
        xrtClearError();
        testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("echo"),(xstrview){Params,sizeof(Params)},XRT_WAIT_FOREVER,&F)==
            XRPC_DUPLEX_ACCEPTED,"infinite timeout retains ordinary wire and connection");
        rpcTestNetworkReply(F,XRPC_CALL_OK,XRPC_REPLIED);
        xrtFutureDestroy(F);
        tlsTestFinish(&T);
    }
}

static void testTlsStops(void)
{
    tlsTest T;tlsTestStart(&T,1,false);xfuture* F=NULL;
    testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("hold"),(xstrview){0},1000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS hold call");
    double End=xrtTimer()+5;while(!xrtAtomic32Load(&T.Held,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TLS business did not enter");xrtSleep(1);}
    testRequire(xrtFutureCancel(F),"TLS sent cancellation");tlsTestResult(F,XRPC_CALL_CANCELLED,XRPC_MAY_HAVE_EXECUTED);xrtFutureDestroy(F);
    testRequire(xrpcTlsClose(T.Right) && xrpcTlsWaitFor(T.Right,0)==XWAIT_TIMEOUT && !xrpcTlsDestroy(T.Right),"TLS close ignored held business duty");xrtClearError();
    xrtAtomic32Store(&T.Permit,1,XMEMORY_RELEASE);tlsTestFinish(&T);
}

typedef struct tlsProducer {xrpctls* Peer;unsigned Index;} tlsProducer;
static int32 tlsTestProduce(ptr Data)
{
    tlsProducer* P=Data;
    for(unsigned K=0;K<80;++K){char Params[48];int N=snprintf(Params,sizeof(Params),"[%u,%u]",P->Index,K);xfuture* F=NULL;
        testRequire(xrpcTlsCall(P->Peer,XRT_STR_LITERAL("echo"),(xstrview){Params,(size_t)N},5000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS concurrent admission");
        if(K%7==0)(void)xrtFutureCancel(F);
        xfutureresult V;testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureResult(F,&V),"TLS concurrent completion");
        const xrpccallresult* R=V.Value;testRequire(R && (R->Status==XRPC_CALL_OK || R->Status==XRPC_CALL_CANCELLED),"TLS concurrent result");
        if(R->Status==XRPC_CALL_OK){xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[R->Index].Result.Raw.Size==(size_t)N && !memcmp(I.Messages[R->Index].Result.Raw.Data,Params,(size_t)N),"TLS concurrent correlation");}
        xrtFutureDestroy(F);
    }return 0;
}
static void testTlsConcurrency(void)
{
    tlsTest T;tlsTestStart(&T,2,false);tlsProducer P[4];xthread* Workers[4];
    for(unsigned K=0;K<4;++K){P[K]=(tlsProducer){K&1?T.Right:T.Left,K};Workers[K]=xrtThreadCreate(tlsTestProduce,P+K,0);testRequire(Workers[K]!=NULL,"TLS producer start");}
    for(unsigned K=0;K<4;++K){testRequire(xrtThreadWaitFor(Workers[K],15000)==XWAIT_OK,"TLS producer join");xrtThreadDestroy(Workers[K]);}tlsTestFinish(&T);
}

static void testTlsAdopt(void)
{
    tlsTest T;tlsTestStartEx(&T,1,false,true,false,5000);testRequire(xrtAtomic32Load(&T.BufferedBeforeAdopt,XMEMORY_ACQUIRE)==1,"TLS adoption test lacked actual buffered input");
    tlsTestResult(T.Early,XRPC_CALL_OK,XRPC_REPLIED);xfuture* F=NULL;testRequire(xrpcTlsCall(T.Right,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[8]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS adopted reverse call");tlsTestResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    testRequire(xrpcTlsClose(T.Right) && xrpcTlsWaitFor(T.Right,20)==XWAIT_TIMEOUT && !xrpcTlsDestroy(T.Right),"TLS native alias omitted from physical barrier");xrtClearError();
    testRequire(!xrtAtomic32Load(&T.ContextDrops,XMEMORY_ACQUIRE),"TLS original session data dropped before native owner");xrtTlsStreamDestroy(T.Alias);T.Alias=NULL;
    tlsTestFinish(&T);testRequire(xrtAtomic32Load(&T.ContextRefs,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&T.ContextDrops,XMEMORY_ACQUIRE)==1,"TLS original owned context credit leak");xrtFutureDestroy(T.Early);xrtFutureDestroy(F);
}
static void testTlsRejectedVerifier(void)
{
    tlsTest T;tlsTestStartEx(&T,2,false,false,true,5000);double End=xrtTimer()+5;while(!xrtAtomic32Load(&T.VerifyEntered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TLS verifier did not enter");xrtSleep(1);}
    xfuture* F=NULL;testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS queued before verification");xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(T.Left,&I) && !I.WriteBytes && !I.WriteFrames && xrtFutureState(F)==XFUTURE_PENDING,"TLS application data passed held verification");xrtAtomic32Store(&T.VerifyPermit,1,XMEMORY_RELEASE);
    tlsTestResult(F,XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);testRequire(xrpcTlsWaitFor(T.Left,5000)==XWAIT_OK,"TLS rejected verifier retirement");
    testRequire(xrpcTlsInfo(T.Left,&I) && I.Reason==XRPC_TLS_HANDSHAKE && I.NativeErrorKind && I.NativeErrorCode && I.NativeErrorDomain[0] && !I.ReadFrames && !I.WriteFrames,"TLS verification failure classification/unverified application IO");tlsTestFinish(&T);xrtFutureDestroy(F);
}
typedef struct tlsRaw {tlsTest T;xtlsstream* Stream;xrpcframereader* Reader;unsigned Mode;xatomic32 Open;} tlsRaw;
static void tlsRawSend(xtlsstream* S,const void* Data,size_t Size)
{size_t N=0;testRequire(xrtTlsStreamSend(S,Data,Size,&N)==XTLS_OK && N==Size,"TLS raw complete native test offer");}
static void tlsRawOpen(xtlsstream* S,ptr Data)
{
    tlsRaw* R=Data;static const char* const Faults[]={"Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}","Content-Length: 1024\r\n\r\n","Content-Length: 9\r\n\r\n{}","Content-Leng"};
    if(R->Mode<4){tlsRawSend(S,Faults[R->Mode],strlen(Faults[R->Mode]));if(R->Mode==2)testRequire(xrtTlsStreamClose(S),"TLS authenticated truncated-frame EOF");}
    if(R->Mode==5){static const char* const Requests[]={"{\"jsonrpc\":\"2.0\",\"id\":\"a\",\"method\":\"hold\"}","{\"jsonrpc\":\"2.0\",\"id\":\"b\",\"method\":\"hold\"}"};
        for(unsigned I=0;I<2;++I){unsigned char Header[64];size_t Count=0;xrpcframeconfig C;xrpcFrameConfigInit(&C);testRequire(xrpcFrameHeaderWrite(strlen(Requests[I]),&C,Header,sizeof(Header),&Count),"TLS overload header");tlsRawSend(S,Header,Count);tlsRawSend(S,Requests[I],strlen(Requests[I]));}}
    xrtAtomic32Store(&R->Open,1,XMEMORY_RELEASE);
}
static void tlsRawRead(xtlsstream* S,const xnetbuf* Buffer,ptr Data)
{
    (void)Buffer;tlsRaw* R=Data;unsigned char Bytes[4096];size_t N=0;
    while(xrtTlsStreamAvailable(S)){
        testRequire(xrtTlsStreamRead(S,Bytes,sizeof(Bytes),&N)==XTLS_OK && N,"TLS raw plaintext read");if(R->Mode!=4)continue;
        size_t Offset=0;while(Offset<N){size_t Used=0;xrpcframeview F;xrpcframestatus Q=xrpcFrameReaderFeed(R->Reader,(xbytesview){Bytes+Offset,N-Offset},&Used,&F);Offset+=Used;testRequire(Q==XRPC_FRAME_MORE || Q==XRPC_FRAME_READY,"TLS raw outgoing frame");
            if(Q==XRPC_FRAME_READY){static const char Reply[]="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":[9]}";unsigned char Header[64];size_t Count=0;xrpcframeconfig C;xrpcFrameConfigInit(&C);testRequire(xrpcFrameHeaderWrite(sizeof(Reply)-1,&C,Header,sizeof(Header),&Count),"TLS raw reply header");tlsRawSend(S,Header,Count);tlsRawSend(S,Reply,sizeof(Reply)-1);testRequire(xrtTlsStreamClose(S),"TLS final response and close_notify");}
        }
    }
}
static void testTlsFaults(void)
{
    for(unsigned Mode=0;Mode<6;++Mode){tlsRaw R;memset(&R,0,sizeof(R));R.Mode=Mode;tlsTest* T=&R.T;TlsActive=T;xrtAtomic32Init(&T->Ready,0);xrtAtomic32Init(&R.Open,0);
        T->TLSContext=testTlsServerContext();T->Identity=testTlsServerIdentity();testRequire(T->TLSContext && T->Identity,"TLS raw fixture");xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=tlsTestVerify;V.Context=T;T->Verifier=xrtTlsVerifierCreate(&V);testRequire(T->Verifier!=NULL,"TLS raw explicit pin");xrtTlsServerConfigInit(&T->TLSConfig);T->TLSConfig.Context=T->TLSContext;T->TLSConfig.Identity=T->Identity;
        xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;T->Engine=xrtNetEngineCreate(&E);testRequire(T->Engine && xrtNetEngineStart(T->Engine),"TLS raw engine");
        xrpcTlsConfigInit(&T->Config);T->Config.ReadChunkBytes=3;T->Config.ReadBytes=7;T->Config.WorkFrames=2;T->Config.Frame.MaxBodyBytes=256;T->Config.FrameTimeout=Mode==3?40:5000;
        if(Mode==5){memcpy(T->Context,"meta",4);xrtAtomic32Init(&T->Calls,0);xrtAtomic32Init(&T->Held,0);xrtAtomic32Init(&T->Permit,0);xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=tlsTestBusiness;S.User=T;testRequire(xrpcServerCreate(&S,&T->Server),"TLS overload executor");T->Config.Duplex.Server=T->Server;T->Config.Duplex.MaxIncoming=1;T->Config.RequestContext=(xbytesview){(cbytes)T->Context,4};}
        testRequire(xrpcFrameReaderCreate(NULL,&R.Reader),"TLS raw frame reader");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents A={tlsTestAccept,NULL,NULL};T->Listener=xrtNetListen(T->Engine,&L,&A,NULL,T);xnetaddr Remote;testRequire(T->Listener && xrtNetListenerLocal(T->Listener,&Remote),"TLS raw listener");
        xtlsclientconfig C;xrtTlsClientConfigInit(&C);C.Context=T->TLSContext;C.Verifier=T->Verifier;C.ServerName=XRT_STR_LITERAL("rpc.fixture");xtlsstreamevents Events={.Open=tlsRawOpen,.Read=tlsRawRead};R.Stream=xrtTlsStreamConnect(T->Engine,&Remote,1,NULL,&C,NULL,&Events,&R);testRequire(R.Stream!=NULL,"TLS raw connect");
        double End=xrtTimer()+5;while(!xrtAtomic32Load(&T->Ready,XMEMORY_ACQUIRE) || !xrtAtomic32Load(&R.Open,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TLS raw attach/open");xrtSleep(1);}
        xfuture* F=NULL;if(Mode==4){testRequire(xrpcTlsCall(T->Right,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS final response call");tlsTestResult(F,XRPC_CALL_OK,XRPC_REPLIED);}
        if(Mode==5){xrpctlsinfo Q={0};Q.Size=sizeof(Q);End=xrtTimer()+5;do{testRequire(xrpcTlsInfo(T->Right,&Q) && xrtTimer()<End,"TLS actual incoming overload not observed");xrtSleep(1);}while(!Q.Overloaded);testRequire(Q.Reason==XRPC_TLS_LIMIT,"TLS overload failure classification");xrtAtomic32Store(&T->Permit,1,XMEMORY_RELEASE);}
        testRequire(xrpcTlsWaitFor(T->Right,5000)==XWAIT_OK,"TLS fault physical retirement");xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(T->Right,&I),"TLS fault info");
        xrpctlsreason Expected=Mode==0 || Mode==2?XRPC_TLS_PROTOCOL:(Mode==1 || Mode==5)?XRPC_TLS_LIMIT:Mode==3?XRPC_TLS_TIMEOUT:XRPC_TLS_TRANSPORT;testRequire(I.Reason==Expected,"TLS fault classification");
        testRequire(xrpcTlsDestroy(T->Right) && xrtTlsStreamAbort(R.Stream) && xrtTlsStreamWaitRetired(R.Stream,5000)==XWAIT_OK,"TLS raw final retirement");xrtTlsStreamDestroy(R.Stream);xrpcFrameReaderDestroy(R.Reader);xrtFutureDestroy(F);
        xrtNetListenerClose(T->Listener);xrtNetListenerDestroy(T->Listener);if(T->Server)testRequire(xrpcServerDestroy(T->Server),"TLS raw executor join");End=xrtTimer()+5;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(T->Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"TLS raw Engine pin/resource tail");xrtSleep(1);}testRequire(Q==XNET_RETIRE_READY,"TLS raw Engine join");xrtTlsVerifierRelease(T->Verifier);xrtTlsIdentityRelease(T->Identity);xrtTlsContextRelease(T->TLSContext);
    }
}
static void testTlsFactoryBudgets(void)
{
    xtlscontext* Context=testTlsServerContext();xtlsidentity* Identity=testTlsServerIdentity();tlsTest T={0};T.Identity=Identity;xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=tlsTestVerify;V.Context=&T;xtlsverifier* Verifier=xrtTlsVerifierCreate(&V);testRequire(Context && Identity && Verifier,"TLS constructor fixture");
    xtlsclientconfig Client;xrtTlsClientConfigInit(&Client);Client.Context=Context;Client.Verifier=Verifier;Client.ServerName=XRT_STR_LITERAL("rpc.fixture");xnetengineconfig C;xrtNetEngineConfigInit(&C);C.Workers=1;xnetengine* E=xrtNetEngineCreate(&C);testRequire(E && xrtNetEngineStart(E),"TLS constructor Engine");
    xnetaddr Address;xrtNetAddrParse(&Address,"127.0.0.1",9);xrpctlsconfig K;xrpcTlsConfigInit(&K);K.Stream.ConnectTimeout=0;
    unsigned Failed=0,Passed=0;static const size_t Limits[]={1,64,512,8192,65536,131072,262144,1048576};
    for(size_t N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"TLS constructor budget");xrpctls* P=NULL;
        bool OK=xrpcTlsConnect(E,&Address,0,&Client,&K,&P);testRequire(xrtAllocationBudgetLeave(&Scope),"TLS constructor budget leave");if(OK){++Passed;testRequire(xrpcTlsWaitFor(P,5000)==XWAIT_OK && xrpcTlsDestroy(P),"TLS zero original connect deadline cleanup");}else{++Failed;testRequire(!P,"TLS failed constructor wrote output");}xrtClearError();
        xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes && !I.LiveCount,"TLS constructor OOM/ref/pin leak");xrtAllocationBudgetFree(B);
    }
    testRequire(Failed && Passed,"TLS constructor allocation boundaries");testRequire(xrtNetEngineDestroy(E),"TLS constructor Engine pin leak");xrtTlsVerifierRelease(Verifier);xrtTlsIdentityRelease(Identity);xrtTlsContextRelease(Context);
}
static void tlsOccupiedTimer(xnetworker* W,uint64 Id,xnetresult Result,ptr Data)
{(void)W;(void)Id;(void)Result;(void)Data;}
typedef struct tlsTimerFill {xnetpost First,Second;xatomic32 Filled;} tlsTimerFill;
static void tlsTestFillTimers(xnetworker* W,ptr Data)
{
    tlsTimerFill* F=Data;for(unsigned N=0;N<2;++N)testRequire(xrtNetEngineAfter(xrtNetWorkerEngine(W),xrtNetWorkerIndex(W),60000,tlsOccupiedTimer,NULL)!=0,"TLS actual owner timer quota insertion");xrtAtomic32Store(&F->Filled,1,XMEMORY_RELEASE);
}
static void tlsTestTimerBarrier(xnetworker* W,ptr Data)
{tlsTimerFill* F=Data;testRequire(xrtNetPost(W,&F->Second,tlsTestFillTimers,F),"TLS second FIFO timer barrier");}
static void testTlsTimerExhaustion(void)
{
    TlsTestTimerLimit=2;tlsTest T;tlsTestStart(&T,1,false);TlsTestTimerLimit=0;double End=xrtTimer()+5;xrpctlsinfo I={0};I.Size=sizeof(I);
    do{testRequire(xrpcTlsInfo(T.Left,&I) && xrtTimer()<End,"TLS timer client not OPEN");xrtSleep(1);}while(I.State!=XRPC_TLS_OPEN);
    tlsTimerFill Fill;xrtAtomic32Init(&Fill.Filled,0);testRequire(xrtNetPostInit(&Fill.First) && xrtNetPostInit(&Fill.Second) && xrtNetPost(xrtNetEngineWorker(T.Engine,1),&Fill.First,tlsTestTimerBarrier,&Fill),"TLS timer owner barrier");
    while(!xrtAtomic32Load(&Fill.Filled,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TLS timer quota not filled");xrtSleep(1);}
    xfuture* F=NULL;testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("hold"),(xstrview){0},5000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS timer fault call");
    End=xrtTimer()+5;do{testRequire(xrpcTlsInfo(T.Left,&I) && xrtTimer()<End,"TLS timer exhaustion not counted");xrtSleep(1);}while(!I.TimerFailures);
    testRequire(I.Reason==XRPC_TLS_LIMIT,"TLS timer failure did not close with limit");xrtAtomic32Store(&T.Permit,1,XMEMORY_RELEASE);tlsTestFinish(&T);xrtFutureDestroy(F);
}
static void testTlsMetadata(void)
{
    TlsTestAlpn=true;tlsTest T;tlsTestStart(&T,1,false);TlsTestAlpn=false;xfuture* F=NULL;testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS ALPN snapshot call");tlsTestResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(T.Left,&I) && I.Version && I.Cipher && I.ProtocolBytes==8 && !memcmp(I.Protocol,"rpc-test",8),"TLS negotiated metadata lost copied policy");
    testRequire(xrpcTlsClose(T.Left) && xrpcTlsWaitFor(T.Left,5000)==XWAIT_OK && xrpcTlsInfo(T.Left,&I) && I.ProtocolBytes==8 && !memcmp(I.Protocol,"rpc-test",8),"TLS negotiated snapshot depended on destroyed native session");tlsTestFinish(&T);xrtFutureDestroy(F);
}
static bool tlsPlainAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;xrtAtomicPtrStore((xatomicptr*)Data,S,XMEMORY_RELEASE);return true;}
static void testTlsSetupDeadline(void)
{
    tlsTest T={0};TlsActive=&T;T.Identity=testTlsServerIdentity();xtlscontext* Context=testTlsServerContext();testRequire(T.Identity && Context,"TLS setup deadline fixture");xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=tlsTestVerify;V.Context=&T;xtlsverifier* Verifier=xrtTlsVerifierCreate(&V);testRequire(Verifier!=NULL,"TLS setup deadline verifier");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;T.Engine=xrtNetEngineCreate(&E);testRequire(T.Engine && xrtNetEngineStart(T.Engine),"TLS setup deadline Engine");xatomicptr Raw;xrtAtomicPtrInit(&Raw,NULL);xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents Events={tlsPlainAccept,NULL,NULL};T.Listener=xrtNetListen(T.Engine,&L,&Events,NULL,&Raw);xnetaddr Address;testRequire(T.Listener && xrtNetListenerLocal(T.Listener,&Address),"TLS plain stalled listener");
    xrpctlsconfig C;xrpcTlsConfigInit(&C);C.Stream.ConnectTimeout=40;C.TLS.HandshakeTimeout=2000;xtlsclientconfig Client;xrtTlsClientConfigInit(&Client);Client.Context=Context;Client.Verifier=Verifier;Client.ServerName=XRT_STR_LITERAL("rpc.fixture");double Began=xrtTimer();testRequire(xrpcTlsConnect(T.Engine,&Address,1,&Client,&C,&T.Left),"TLS setup deadline client");
    xfuture* F=NULL;testRequire(xrpcTlsCall(T.Left,XRT_STR_LITERAL("echo"),(xstrview){0},1000,&F)==XRPC_DUPLEX_ACCEPTED,"TLS pre-open deadline admission");tlsTestResult(F,XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);testRequire(xrpcTlsWaitFor(T.Left,5000)==XWAIT_OK,"TLS setup deadline retirement");xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(T.Left,&I) && I.Reason==XRPC_TLS_TIMEOUT && !I.WriteFrames && xrtTimer()-Began<1.0,"TLS reset original deadline to native handshake timeout");testRequire(xrpcTlsDestroy(T.Left),"TLS setup deadline destroy");xrtFutureDestroy(F);
    xnetstream* S=xrtAtomicPtrLoad(&Raw,XMEMORY_ACQUIRE);testRequire(S && xrtNetStreamAbort(S) && xrtNetStreamWaitRetired(S,5000)==XWAIT_OK,"TLS plain peer tail");xrtNetStreamDestroy(S);xrtNetListenerClose(T.Listener);xrtNetListenerDestroy(T.Listener);double End=xrtTimer()+5;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(T.Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"TLS setup deadline Engine tail");xrtSleep(1);}testRequire(Q==XNET_RETIRE_READY,"TLS setup deadline Engine join");xrtTlsVerifierRelease(Verifier);xrtTlsIdentityRelease(T.Identity);xrtTlsContextRelease(Context);
}

int main(void)
{setbuf(stdout,NULL);testTlsDuplex();testTlsExtensions();puts("[case] TLS stops");testTlsStops();puts("[case] TLS concurrency");testTlsConcurrency();puts("[case] TLS adopt");testTlsAdopt();puts("[case] TLS reject");testTlsRejectedVerifier();puts("[case] TLS framing faults/overload");testTlsFaults();puts("[case] TLS budgets");testTlsFactoryBudgets();puts("[case] TLS timer quota");testTlsTimerExhaustion();puts("[case] TLS metadata snapshot");testTlsMetadata();puts("[case] TLS original setup deadline");testTlsSetupDeadline();puts("[PASS] xrpc TLS: actual encrypted bidirectional framing/pin/reject/snapshot/ALPN/owned buffered adoption/native-alias/copy/bounds/batch/notify/cancel/producers/retained/shared-executor/physical/framing-faults/overload/final-before-EOF/OOM/timer-exhaustion/negotiated-network-extensions");return 0;}
