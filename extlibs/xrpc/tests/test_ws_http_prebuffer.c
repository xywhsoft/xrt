#define main wsHttpExistingMain
#include "test_ws_http.c"
#undef main
typedef struct prefixScene {
    rpcWsHttpService S;xatomicptr Client,Server;xatomic32 Opened,Factory,Response;
    xnetpost Bind,Rest;xrpcws* Peer;xallocationbudget* Budget;
} prefixScene;
static const char PrefixRequest[]="GET /rpc HTTP/1.1\r\nHost: rpc.fixture\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: jsonrpc\r\n\r\n";
static void prefixSend(xtlsstream* S,const char* B,size_t N)
{size_t Written=0;testRequire(xrtTlsStreamSend(S,B,N,&Written)==XTLS_OK&&Written==N,"prefix exact native TLS offer");}
static void prefixOpen(xtlsstream* S,ptr Data)
{prefixScene* P=Data;bool Server=xrtTlsSessionRole(xrtTlsStreamSession(S))==XTLS_SERVER;xrtAtomicPtrStore(Server?&P->Server:&P->Client,S,XMEMORY_RELEASE);if(!Server)prefixSend(S,PrefixRequest,9);xrtAtomic32FetchAdd(&P->Opened,1,XMEMORY_RELEASE);}
static void prefixRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{
    (void)B;prefixScene* P=Data;xhttpfield Fields[16];xhttp1head H={.Fields=Fields,.FieldCapacity=16};xhttp1status R=xrtHttp1ResponseParseTls(S,&H,NULL,NULL);
    if(R==XHTTP1_MORE){testRequire(xrtTlsStreamReadMore(S),"partial prefix response");return;}
    xwsupgradeclientconfig C;xrtWsUpgradeClientConfigInit(&C);C.Protocols=XRT_STR_LITERAL("jsonrpc");xwsupgrade U;
    if(R!=XHTTP1_READY||H.Status!=101){printf("[prefix response] parse%d status%u error%s\n",R,H.Status,xrtErrorMessage(xrtGetError()));xrtAtomic32Store(&P->Response,2,XMEMORY_RELEASE);return;}
    testRequire(xrtWsUpgradeResponseCheck(&H,XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),&C,&U),"split prebuffer request produces checked101");testRequire(xrtTlsStreamConsume(S,H.Bytes),"prefix response consume");xrtAtomic32Store(&P->Response,1,XMEMORY_RELEASE);
}
static const xtlsstreamownershipv1 PrefixClient={sizeof(xtlsstreamownershipv1),{.Open=prefixOpen,.Read=prefixRead},rpcWsHttpServiceNativeDrop,&RpcWsHttpServiceNativeOps};
static const xtlsstreamownershipv1 PrefixServer={sizeof(xtlsstreamownershipv1),{.Open=prefixOpen},rpcWsHttpServiceNativeDrop,&RpcWsHttpServiceNativeOps};
static bool prefixAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;prefixScene* P=Data;xtlsstream* T=NULL;xrtAtomic32FetchAdd(&P->S.NativeRefs,1,XMEMORY_RELAXED);testRequire(xrtTlsStreamAcceptOwnedV1(S,&P->S.NativeServer,NULL,P,&PrefixServer,&T),"prefix TLS accept");return true;}
static void prefixBind(xnetworker* W,ptr Data)
{
    (void)W;prefixScene* P=Data;xtlsstream* Native=xrtAtomicPtrLoad(&P->Server,XMEMORY_ACQUIRE);xrpcwsconfig C;xrpcWsConfigInit(&C);C.Duplex.Server=P->S.Server;xrpcwshttpconfig H;xrpcWsHttpConfigInit(&H);H.Host=XRT_STR_LITERAL("rpc.fixture");
    xrpcws* Sentinel=(xrpcws*)(uintptr_t)17;xhttpfield Managed={XRT_STR_LITERAL("Content-Length"),XRT_STR_LITERAL("0")};xrpcwshttpconfig Bad=H;Bad.Fields=&Managed;Bad.FieldCount=1;
    testRequire(!xrpcWsHttpServerTls(Native,&C,&Bad,&Sentinel)&&Sentinel==(xrpcws*)(uintptr_t)17&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT&&xrtTlsStreamAvailable(Native)==9,"managed header override leaves native/prefix/output unchanged");xrtClearError();
    Managed=(xhttpfield){XRT_STR_LITERAL("X-Test"),XRT_STR_LITERAL("a\r\nInjected: bad")};
    testRequire(!xrpcWsHttpServerTls(Native,&C,&Bad,&Sentinel)&&Sentinel==(xrpcws*)(uintptr_t)17&&xrtTlsStreamAvailable(Native)==9,"header injection rejected before native transfer");xrtClearError();
    Bad=H;Bad.UnauthorizedChallenge=XRT_STR_LITERAL("Bearer\r\nInjected: bad");
    testRequire(!xrpcWsHttpServerTls(Native,&C,&Bad,&Sentinel)&&Sentinel==(xrpcws*)(uintptr_t)17&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT&&xrtTlsStreamAvailable(Native)==9,"challenge injection rejected before native transfer");xrtClearError();
    static const size_t Limits[]={1,256,4096,32768,49152,65536,131072,262144,1048576,16777216};unsigned Rejected=0;
    for(size_t I=0;I<sizeof(Limits)/sizeof(*Limits);++I){
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[I],&B)&&xrtAllocationBudgetEnter(B,&Scope),"original HTTP constructor budget");
        bool OK=xrpcWsHttpServerTls(Native,&C,&H,&Sentinel);xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"constructor budget leave");xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(B,&Info),"constructor ledger");
        if(OK){P->Peer=Sentinel;P->Budget=B;break;}
        testRequire(!Info.LiveBytes&&!Info.LiveCount&&Info.Rejections&&Sentinel==(xrpcws*)(uintptr_t)17&&xrtTlsStreamAvailable(Native)==9&&xrtAtomic32Load(&P->S.NativeRefs,XMEMORY_ACQUIRE)==3,"all failed constructor credits/allocations/native prefix/output roll back");++Rejected;xrtAllocationBudgetFree(B);
    }
    testRequire(P->Peer&&Rejected>=4,"constructor succeeds under one original sufficient budget");xrtAtomic32Store(&P->Factory,1,XMEMORY_RELEASE);
}
static void prefixRest(xnetworker* W,ptr Data)
{(void)W;prefixScene* P=Data;prefixSend(xrtAtomicPtrLoad(&P->Client,XMEMORY_ACQUIRE),PrefixRequest+9,sizeof(PrefixRequest)-1-9);}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);prefixScene P={0};xrtAtomicPtrInit(&P.Client,NULL);xrtAtomicPtrInit(&P.Server,NULL);xrtAtomic32Init(&P.Opened,0);xrtAtomic32Init(&P.Factory,0);xrtAtomic32Init(&P.Response,0);xrtAtomic32Init(&P.S.NativeRefs,2);testRequire(xrtNetPostInit(&P.Bind)&&xrtNetPostInit(&P.Rest),"prefix embedded nodes");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;P.S.Engine=xrtNetEngineCreate(&E);testRequire(P.S.Engine&&xrtNetEngineStart(P.S.Engine),"prefix Engine");xrpcserverconfig C;xrpcServerConfigInit(&C);C.Call=rpcWsHttpServiceEcho;C.User=&P.S;testRequire(xrpcServerCreate(&C,&P.S.Server),"prefix shared Server");P.S.Context=testTlsServerContext();P.S.Identity=testTlsServerIdentity();xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=rpcWsHttpServiceVerify;V.Context=&P.S;P.S.Verifier=xrtTlsVerifierCreate(&V);xrtTlsServerConfigInit(&P.S.NativeServer);P.S.NativeServer.Context=P.S.Context;P.S.NativeServer.Identity=P.S.Identity;
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents A={prefixAccept,NULL,NULL};P.S.Listener=xrtNetListen(P.S.Engine,&L,&A,NULL,&P);xnetaddr Address;testRequire(P.S.Listener&&xrtNetListenerLocal(P.S.Listener,&Address),"prefix listener");xtlsclientconfig T;xrtTlsClientConfigInit(&T);T.Context=P.S.Context;T.Verifier=P.S.Verifier;T.ServerName=XRT_STR_LITERAL("rpc.fixture");testRequire(xrtTlsStreamConnectOwnedV1(P.S.Engine,&Address,1,NULL,&T,NULL,&P,&PrefixClient),"prefix TLS connect");
    double End=xrtTimer()+10;while(xrtAtomic32Load(&P.Opened,XMEMORY_ACQUIRE)<2){testRequire(xrtTimer()<End,"prefix opened");xrtSleep(1);}xtlsstream* Server=xrtAtomicPtrLoad(&P.Server,XMEMORY_ACQUIRE);while(xrtTlsStreamAvailable(Server)<9){testRequire(xrtTimer()<End,"first9 bytes buffered with no original Read callback");xrtSleep(1);}testRequire(xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(Server)),&P.Bind,prefixBind,&P),"post native HTTP bind");rpcWsHttpServiceAwait(&P.Factory);
    testRequire(xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(xrtAtomicPtrLoad(&P.Client,XMEMORY_ACQUIRE))),&P.Rest,prefixRest,&P),"release remaining header");rpcWsHttpServiceAwait(&P.Response);testRequire(xrtAtomic32Load(&P.Response,XMEMORY_ACQUIRE)==1,"prebuffered prefix produces101");xrpcwshttpinfo I={.Size=sizeof(I)};do{testRequire(xrpcWsHttpInfo(P.Peer,&I)&&xrtTimer()<End,"cold prefix storage retires");if(I.ColdBytes)xrtSleep(1);}while(I.ColdBytes);testRequire(I.Outcome==XRPC_WS_HTTP_OPEN&&I.Status==101&&I.HeaderCopyBytes>=9&&I.HeaderCopyBytes<=16384,"bounded exceptional prefix copy and cold release");puts("[PASS] valid prebuffered prefix advances to101 with bounded cold copy and no Core change");
    rpcWsHttpServicePeerClose(P.Peer);xallocationbudgetinfo Budget={.Size=sizeof(Budget)};testRequire(xrtAllocationBudgetInfo(P.Budget,&Budget)&&!Budget.LiveBytes&&!Budget.LiveCount,"successful original constructor budget returns to zero after physical peer destruction");xrtAllocationBudgetFree(P.Budget);xtlsstream* Client=xrtAtomicPtrLoad(&P.Client,XMEMORY_ACQUIRE);(void)xrtTlsStreamAbort(Client);testRequire(xrtTlsStreamWaitRetired(Client,5000)==XWAIT_OK,"prefix raw client tail");xrtTlsStreamDestroy(Client);testRequire(xrtNetListenerClose(P.S.Listener),"prefix listener close");xrtNetListenerDestroy(P.S.Listener);testRequire(xrpcServerDestroy(P.S.Server),"prefix Server join");End=xrtTimer()+5;xnetretireresult R;while((R=xrtNetEngineTryDestroy(P.S.Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"prefix Engine join");xrtSleep(1);}testRequire(R==XNET_RETIRE_READY&&xrtAtomic32Load(&P.S.NativeRefs,XMEMORY_ACQUIRE)==1,"prefix real owner credits");xrtTlsVerifierRelease(P.S.Verifier);xrtTlsIdentityRelease(P.S.Identity);xrtTlsContextRelease(P.S.Context);return 0;
}
