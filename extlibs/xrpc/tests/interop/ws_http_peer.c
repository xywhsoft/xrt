#include <xrpc/ws_http.h>
#if defined(XRPC_FEATURE_XRPC_WS_DIAL)
#include <xrpc/ws_dial.h>
#if defined(XRPC_FEATURE_XRPC_WS_URI)
#include <xrpc/ws_uri.h>
#endif
#endif
#if defined(XRPC_FEATURE_XRPC_WS_HTTP_DEFLATE)
#include <xrpc/ws_deflate.h>
#endif
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(XRPC_WS_HTTP_OWNER_PROBE)
#include "../../src/ws/ws.c"
#include "../../../../src/internal/xrt_websocket_stream.h"
#include "../../../../src/internal/xrt_tls_stream.h"
typedef struct httpOwnerProbe {xnetpost Post;xatomic32 Done;xrpcws* Peer;} httpOwnerProbe;
static void httpOwnerInspect(xnetworker* W,ptr Data)
{
    (void)W;httpOwnerProbe* Q=Data;xwsstream* S=Q->Peer->Stream;
    if(S){fprintf(stderr,"[owner WS] pause%u post%u reading%d frame%d remaining%llu initial%zu\n",xrtAtomic32Load(&S->ReadPaused,XMEMORY_ACQUIRE),xrtAtomic32Load(&S->DrivePosted,XMEMORY_ACQUIRE),S->Reading,S->FrameActive,(unsigned long long)S->FrameRemaining,S->Initial.Size);
        if(S->TransportKind==__XRT_WS_TRANSPORT_TLS){xtlsstream* T=xrtAtomicPtrLoad(&S->Transport,XMEMORY_ACQUIRE);if(T){unsigned char Bytes[2]={0};(void)xrtNetBufPeek(xrtTlsStreamBuffer(T),0,Bytes,2);fprintf(stderr,"[owner TLS] driving%d notified%d more%d morebytes%zu transportpaused%d plain%zu feed%zu head%02x/%02x\n",T->Driving,T->ReadNotified,T->ReadMore,T->ReadMoreSize,T->TransportPaused,xrtTlsSessionPlainSize(T->Session),xrtTlsSessionFeedSize(T->Session),Bytes[0],Bytes[1]);}}
        if(getenv("XRPC_TEST_NUDGE")){fprintf(stderr,"[owner] one explicit WS Resume\n");(void)xrtWsStreamResume(S);}}
    xrtAtomic32Store(&Q->Done,1,XMEMORY_RELEASE);
}
static void httpOwnerDiagnose(xrpcws* P)
{httpOwnerProbe Q={0};Q.Peer=P;xrtAtomic32Init(&Q.Done,0);if(!xrtNetPostInit(&Q.Post)||!xrtNetPost(P->Worker,&Q.Post,httpOwnerInspect,&Q))return;while(!xrtAtomic32Load(&Q.Done,XMEMORY_ACQUIRE))xrtSleep(1);}
#endif
typedef struct httpInterop {
    xnetengine* Engine;xnetlistener* Listener;xrpcserver* Server;xrpcws* Peer;xfuture* Call;
    xrpcwsconfig RPC;xrpcwshttpconfig HTTP;xatomic32 Ready,Finished,Calls,NativeRefs;
    bool Client,Secure;const char* Mode;ptr Native;xnetpost Attach;
#if defined(XRPC_FEATURE_XRPC_WS_URI)
    xnetresolver* Resolver;
#endif
    xtlsserverconfig TLS;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsstream* Probe;xbytesview Pin;
} httpInterop;
static void httpInteropRequire(bool OK,cstr Text)
{if(!OK){fprintf(stderr,"[FAIL] %s: %s\n",Text,xrtErrorMessage(xrtGetError()));exit(1);}}
static bool httpInteropRead(cstr Name,bytes Data,size_t Capacity,size_t* Used)
{FILE* F=fopen(Name,"rb");if(!F)return false;size_t N=fread(Data,1,Capacity,F);bool OK=N&&N<Capacity&&!ferror(F);if(fclose(F))OK=false;if(OK)*Used=N;return OK;}
static bool httpInteropMethod(const xrpcmessage* M,cstr Name)
{size_t N=strlen(Name);return M->Method.Raw.Size==N+2&&!memcmp(M->Method.Raw.Data+1,Name,N);}
static bool httpInteropEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr Data)
{
    httpInterop* S=Data;httpInteropRequire(!xrtNetEngineCurrent(S->Engine),"business outside native Worker");xrtAtomic32FetchAdd(&S->Calls,1,XMEMORY_RELAXED);
    if(httpInteropMethod(M,"finish"))xrtAtomic32Store(&S->Finished,1,XMEMORY_RELEASE);
    if(httpInteropMethod(M,"who"))return xrpcReplyResult(R,C->Data.Size?(xstrview){(cstr)C->Data.Data,C->Data.Size}:XRT_STR_LITERAL("null"));
    return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}
static xrpcwshttpauthstatus httpInteropAuthorize(const xrpcwshttpauthrequest* R,bytes Context,size_t Capacity,size_t* Used,ptr Data)
{
    httpInterop* S=Data;httpInteropRequire(!xrtNetEngineCurrent(S->Engine)&&R->Secure==S->Secure&&R->Head&&R->Cancel,"authorization shared executor/security/snapshot");
    const xhttpfield* A=xrtHttp1Field(R->Head,XRT_STR_LITERAL("Authorization"));
    if(!A||A->Value.Size!=14||memcmp(A->Value.Data,"Bearer fixture",14))return XRPC_WS_HTTP_UNAUTHORIZED;
    if(!strcmp(S->Mode,"auth-timeout")){while(xrtTimer()<R->Deadline&&!xrtCancelRequested(R->Cancel))xrtSleep(1);return XRPC_WS_HTTP_AUTH_ERROR;}
    httpInteropRequire(Capacity>=4,"context capacity");memcpy(Context,"[42]",4);*Used=4;return XRPC_WS_HTTP_ALLOW;
}
#if defined(XRPC_FEATURE_XRPC_WS_HTTP_DEFLATE)
static bool httpInteropDeflate(const xwsdeflate* Offer,xwsdeflate* Response,ptr Data)
{
    (void)Data;if(!xrtWsDeflateAccept(Offer,Response))return false;
    if(Offer->Flags&XWS_DEFLATE_CLIENT_NO_CONTEXT)Response->Flags|=XWS_DEFLATE_CLIENT_NO_CONTEXT;
    if(Offer->Flags&XWS_DEFLATE_CLIENT_MAX_WINDOW){Response->Flags|=XWS_DEFLATE_CLIENT_MAX_WINDOW;Response->ClientMaxWindowBits=Offer->ClientMaxWindowBits;}
    return true;
}
#endif
static void httpInteropAttach(httpInterop* S,ptr Native)
{
    bool OK;if(S->Secure)OK=S->Client?xrpcWsHttpClientTls(Native,&S->RPC,&S->HTTP,&S->Peer):xrpcWsHttpServerTls(Native,&S->RPC,&S->HTTP,&S->Peer);else OK=S->Client?xrpcWsHttpClientTcp(Native,&S->RPC,&S->HTTP,&S->Peer):xrpcWsHttpServerTcp(Native,&S->RPC,&S->HTTP,&S->Peer);
    httpInteropRequire(OK,"owned HTTP factory");httpInteropRequire(xrpcWsCall(S->Peer,XRT_STR_LITERAL("python_echo"),XRT_STR_LITERAL("[42]"),10000,&S->Call)==XRPC_DUPLEX_ACCEPTED,"queue call before HTTP owner runs");xrtAtomic32Store(&S->Ready,1,XMEMORY_RELEASE);
}
static void httpInteropAttachPost(xnetworker* W,ptr Data)
{(void)W;httpInterop* S=Data;httpInteropAttach(S,S->Native);}
static void httpInteropTLSOpen(xtlsstream* Native,ptr Data)
{httpInterop* S=Data;S->Native=Native;const char* Probe=getenv("XRPC_TEST_TLS_PROBE");if(Probe&&Probe[0]=='1')S->Probe=xrtTlsStreamRef(Native);httpInteropRequire((!Probe||Probe[0]!='1'||S->Probe)&&xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(Native)),&S->Attach,httpInteropAttachPost,S),"post TLS bind outside Open driver");}
static void httpInteropTLSClosed(xtlsstream* Native,xnetresult Result,const xerror* Error,ptr Data)
{(void)Result;(void)Error;httpInterop* S=Data;if(!S->Peer){S->Native=Native;xrtAtomic32Store(&S->Ready,2,XMEMORY_RELEASE);}}
static bool httpInteropCount(const void* Data,size_t* N)
{*N=xrtAtomic32Load(&((const httpInterop*)Data)->NativeRefs,XMEMORY_ACQUIRE);return true;}
static bool httpInteropTrace(const void* D,xrtownershipvisitor V,ptr U)
{(void)D;(void)V;(void)U;return false;}
static void httpInteropDrop(const void* Data)
{xrtAtomic32FetchSub(&((httpInterop*)Data)->NativeRefs,1,XMEMORY_RELEASE);}
static const xrtownershipops HttpInteropOps={httpInteropCount,httpInteropTrace};
static const xtlsstreamownershipv1 HttpInteropNative={sizeof(xtlsstreamownershipv1),{.Open=httpInteropTLSOpen,.Close=httpInteropTLSClosed},httpInteropDrop,&HttpInteropOps};
static xtlsverifydecision httpInteropPin(const xtlspeer* P,ptr Data)
{httpInterop* S=Data;return P&&P->CertificateCount&&P->Name.Size==8&&!memcmp(P->Name.Data,"rpc.test",8)&&P->Certificates[0].Raw.Size==S->Pin.Size&&!memcmp(P->Certificates[0].Raw.Data,S->Pin.Data,S->Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
static void httpInteropOpen(xnetstream* Native,ptr Data)
{httpInteropAttach(Data,Native);}
static bool httpInteropAccept(xnetlistener* L,xnetstream* Native,ptr Data)
{
    (void)L;httpInterop* S=Data;if(S->Secure){xtlsstream* TLS=NULL;xrtAtomic32FetchAdd(&S->NativeRefs,1,XMEMORY_RELAXED);httpInteropRequire(xrtTlsStreamAcceptOwnedV1(Native,&S->TLS,NULL,S,&HttpInteropNative,&TLS),"native TLS accept");}else httpInteropAttach(S,Native);return true;
}
#if defined(XRPC_FEATURE_XRPC_WS_URI)
static xnetaddrlist* httpInteropUriLookup(cstr Name,xnetfamily Family,ptr Data)
{(void)Data;httpInteropRequire(!strcmp(Name,"rpc.test")&&(Family==XNET_FAMILY_UNSPEC||Family==XNET_FAMILY_IPV4),"URI decoded routing name");xnetaddr A;xrtNetAddrLoopback(&A,XNET_FAMILY_IPV4,0);return xrtNetAddrListCreate(&A,1);}
#endif
int main(int Argc,char** Argv)
{
    if(Argc!=7&&Argc!=8)return 2;
    #if !defined(XRPC_FEATURE_XRPC_WS_HTTP_DEFLATE)
    if(Argc==8)return 2;
    #endif
    httpInterop S={0};S.Client=!strcmp(Argv[1],"client");S.Secure=!strcmp(Argv[2],"tls");S.Mode=Argv[3];unsigned Port=(unsigned)strtoul(Argv[4],NULL,10);if(Port>65535||(S.Client&&!Port))return 3;setbuf(stdout,NULL);
    xrtAtomic32Init(&S.Ready,0);xrtAtomic32Init(&S.Finished,0);xrtAtomic32Init(&S.Calls,0);xrtAtomic32Init(&S.NativeRefs,1);httpInteropRequire(xrtNetPostInit(&S.Attach),"embedded TLS handoff");
    unsigned char Certificate[65536],Key[8192];size_t CertificateBytes=0,KeyBytes=0;
    if(S.Secure){httpInteropRequire(httpInteropRead(Argv[5],Certificate,sizeof(Certificate),&CertificateBytes),"runtime public certificate");S.Pin=(xbytesview){Certificate,CertificateBytes};if(!S.Client){httpInteropRequire(httpInteropRead(Argv[6],Key,sizeof(Key),&KeyBytes),"runtime private key");S.Identity=xrtTlsIdentityRsa(&S.Pin,1,(xbytesview){Key,KeyBytes});xrtSecureZero(Key,sizeof(Key));httpInteropRequire(S.Identity!=NULL,"owned RSA identity");}else{xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=httpInteropPin;V.Context=&S;S.Verifier=xrtTlsVerifierCreate(&V);httpInteropRequire(S.Verifier!=NULL,"explicit DER pin and host");}}
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;S.Engine=xrtNetEngineCreate(&E);httpInteropRequire(S.Engine&&xrtNetEngineStart(S.Engine),"native Engine");xrpcserverconfig Server;xrpcServerConfigInit(&Server);Server.Call=httpInteropEcho;Server.User=&S;httpInteropRequire(xrpcServerCreate(&Server,&S.Server),"shared Server");xrpcWsConfigInit(&S.RPC);S.RPC.Duplex.Server=S.Server;xrpcWsHttpConfigInit(&S.HTTP);S.HTTP.Host=XRT_STR_LITERAL("rpc.test");S.HTTP.SetupTimeout=700;
    xhttpfield Authorization={XRT_STR_LITERAL("Authorization"),XRT_STR_LITERAL("Bearer fixture")};if(S.Client){S.HTTP.Fields=&Authorization;S.HTTP.FieldCount=1;}else{S.HTTP.Authorize=httpInteropAuthorize;S.HTTP.AuthorizationUser=&S;S.HTTP.MaxAuthorizationContextBytes=16;}
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_DEFLATE)
    if(Argc==8){S.RPC.CompressOutput=true;S.RPC.CompressMinBytes=0;S.HTTP.Client.EnableDeflate=true;S.HTTP.Client.RequireDeflate=!strcmp(Argv[7],"missing");S.HTTP.Server.EnableDeflate=strcmp(Argv[7],"decline")!=0;S.HTTP.Server.AcceptDeflate=httpInteropDeflate;
        if(!strcmp(Argv[7],"no-context"))S.HTTP.Client.Deflate.Flags=XWS_DEFLATE_SERVER_NO_CONTEXT|XWS_DEFLATE_CLIENT_NO_CONTEXT;
        if(!strcmp(Argv[7],"bomb"))S.RPC.MaxMessageBytes=4096;
        if(!strcmp(Argv[7],"threshold"))S.RPC.CompressMinBytes=1024;
        if(!strncmp(Argv[7],"window",6)){unsigned Bits=(unsigned)strtoul(Argv[7]+6,NULL,10);httpInteropRequire(Bits>=8&&Bits<=15,"fixture window");S.HTTP.Client.Deflate.Flags=XWS_DEFLATE_SERVER_MAX_WINDOW|XWS_DEFLATE_CLIENT_MAX_WINDOW;S.HTTP.Client.Deflate.ServerMaxWindowBits=S.HTTP.Client.Deflate.ClientMaxWindowBits=(uint8)Bits;}
    }
    #endif
    if(S.Client){xnetaddr A;httpInteropRequire(xrtNetAddrParse(&A,"127.0.0.1",(uint16)Port),"numeric loopback");
        const char* Uri=getenv("XRPC_TEST_WS_URI");const char* Dial=getenv("XRPC_TEST_WS_DIAL");if(Uri&&Uri[0]=='1'){
            #if defined(XRPC_FEATURE_XRPC_WS_URI_TLS)
            xnetresolverconfig RC;xrtNetResolverConfigInit(&RC);RC.Workers=1;RC.CacheEntries=0;RC.Lookup=httpInteropUriLookup;S.Resolver=xrtNetResolverCreate(&RC);httpInteropRequire(S.Resolver!=NULL,"host URI Resolver");char URL[100];int N=snprintf(URL,sizeof(URL),"%s://rpc.test:%u/rpc",S.Secure?"wss":"ws",Port);bool OK;
            if(S.Secure){xtlsclientconfig T;xrtTlsClientConfigInit(&T);T.Verifier=S.Verifier;OK=xrpcWsConnectUriTls(S.Engine,S.Resolver,(xstrview){URL,(size_t)N},1,&T,NULL,&S.RPC,&S.HTTP,NULL,&S.Peer);}else OK=xrpcWsConnectUri(S.Engine,S.Resolver,(xstrview){URL,(size_t)N},1,&S.RPC,&S.HTTP,NULL,&S.Peer);
            httpInteropRequire(OK&&xrpcWsCall(S.Peer,XRT_STR_LITERAL("python_echo"),XRT_STR_LITERAL("[42]"),10000,&S.Call)==XRPC_DUPLEX_ACCEPTED,"URI factory, derived native identity and queued call");xrtAtomic32Store(&S.Ready,1,XMEMORY_RELEASE);
            #else
            return 2;
            #endif
        }else if(Dial&&Dial[0]=='1'){
            #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
            bool OK;if(S.Secure){xtlsclientconfig TLS;xrtTlsClientConfigInit(&TLS);TLS.Verifier=S.Verifier;TLS.ServerName=XRT_STR_LITERAL("rpc.test");OK=xrpcWsConnectTls(S.Engine,&A,1,&TLS,NULL,&S.RPC,&S.HTTP,NULL,&S.Peer);}else OK=xrpcWsConnect(S.Engine,&A,1,&S.RPC,&S.HTTP,NULL,&S.Peer);
            httpInteropRequire(OK&&xrpcWsCall(S.Peer,XRT_STR_LITERAL("python_echo"),XRT_STR_LITERAL("[42]"),10000,&S.Call)==XRPC_DUPLEX_ACCEPTED,"full numeric WS/WSS dial and queued call");xrtAtomic32Store(&S.Ready,1,XMEMORY_RELEASE);
            #else
            return 2;
            #endif
        }else if(S.Secure){xtlsclientconfig TLS;xrtTlsClientConfigInit(&TLS);TLS.Verifier=S.Verifier;TLS.ServerName=XRT_STR_LITERAL("rpc.test");xrtAtomic32FetchAdd(&S.NativeRefs,1,XMEMORY_RELAXED);httpInteropRequire(xrtTlsStreamConnectOwnedV1(S.Engine,&A,1,NULL,&TLS,NULL,&S,&HttpInteropNative)!=NULL,"native TLS connect");}else{xnetstreamevents Events={.Open=httpInteropOpen};httpInteropRequire(xrtNetStreamConnect(S.Engine,&A,1,NULL,&Events,&S)!=NULL,"native TCP connect");}puts("READY");}
    else{xrtTlsServerConfigInit(&S.TLS);S.TLS.Identity=S.Identity;xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents Events={httpInteropAccept,NULL,NULL};S.Listener=xrtNetListen(S.Engine,&L,&Events,NULL,&S);xnetaddr A;httpInteropRequire(S.Listener&&xrtNetListenerLocal(S.Listener,&A),"native listener");printf("PORT %u\n",(unsigned)A.Port);}
    double End=xrtTimer()+15;while(!xrtAtomic32Load(&S.Ready,XMEMORY_ACQUIRE)){httpInteropRequire(xrtTimer()<End,"factory readiness");xrtSleep(1);}httpInteropRequire(xrtAtomic32Load(&S.Ready,XMEMORY_ACQUIRE)==1,"native TLS handshake precedes factory");xwaitresult Wait=xrtFutureWaitFor(S.Call,5000);
    if(Wait!=XWAIT_OK){xrpcwsinfo D={.Size=sizeof(D)};(void)xrpcWsInfo(S.Peer,&D);fprintf(stderr,"[wait diagnostic] state%u reason%u messages%llu/%llu bytes%llu/%llu buffered%zu pending%zu native%d/%d\n",(unsigned)D.State,(unsigned)D.Reason,(unsigned long long)D.ReadMessages,(unsigned long long)D.WriteMessages,(unsigned long long)D.ReadBytes,(unsigned long long)D.WriteBytes,D.ReceiveBytes,D.Duplex.PendingCalls,(int)D.NativeErrorKind,(int)D.NativeErrorCode);if(S.Probe){xnetstreamstats N={0};(void)xrtNetStreamStats(xrtTlsStreamTransport(S.Probe),&N);fprintf(stderr,"[TLS diagnostic] state%u plain%zu pending%zu readpaused%d recv%llu\n",(unsigned)xrtTlsStreamState(S.Probe),xrtTlsStreamAvailable(S.Probe),xrtTlsStreamPending(S.Probe),(int)N.ReadPaused,(unsigned long long)N.ReceivedBytes);}}
    #if defined(XRPC_WS_HTTP_OWNER_PROBE)
    if(Wait!=XWAIT_OK){httpOwnerDiagnose(S.Peer);if(getenv("XRPC_TEST_NUDGE"))Wait=xrtFutureWaitFor(S.Call,1500);}
    #endif
    httpInteropRequire(Wait==XWAIT_OK,"queued call terminal result");const xrpccallresult* R=xrtFutureValue(S.Call);httpInteropRequire(R!=NULL,"typed queued result");
    bool Normal=!strcmp(S.Mode,"normal");if(Normal){if(R->Status!=XRPC_CALL_OK){xrpcwsinfo D={.Size=sizeof(D)};(void)xrpcWsInfo(S.Peer,&D);fprintf(stderr,"[diagnostic] status%u delivery%u state%u reason%u messages%llu/%llu native%d/%d\n",(unsigned)R->Status,(unsigned)R->Delivery,(unsigned)D.State,(unsigned)D.Reason,(unsigned long long)D.ReadMessages,(unsigned long long)D.WriteMessages,(int)D.NativeErrorKind,(int)D.NativeErrorCode);}httpInteropRequire(R->Status==XRPC_CALL_OK&&R->Delivery==XRPC_REPLIED,"bidirectional reply");xrtTlsStreamDestroy(S.Probe);S.Probe=NULL;httpInteropRequire(xrpcWsWaitFor(S.Peer,15000)==XWAIT_OK&&xrtAtomic32Load(&S.Finished,XMEMORY_ACQUIRE),"independent acknowledged barrier and remote WS close");}
    else{
        xrpccallstatus Expected=XRPC_CALL_PROTOCOL;
        if(!strcmp(S.Mode,"deflate-bomb"))Expected=XRPC_CALL_LIMIT;
        else if(!strcmp(S.Mode,"timeout")||!strcmp(S.Mode,"auth-timeout"))Expected=XRPC_CALL_TIMEOUT;
        else if(!strcmp(S.Mode,"reject")||!strcmp(S.Mode,"route")||!strcmp(S.Mode,"method")||!strcmp(S.Mode,"unauthorized"))Expected=XRPC_CALL_HTTP_ERROR;
        httpInteropRequire((R->Delivery==XRPC_NOT_SENT||!strcmp(S.Mode,"deflate-bomb"))&&R->Status==Expected,"exact typed handshake/decode failure and delivery");
        /* A terminal unsent call does not mean the rejection response has
         * drained. Let the peer finish its orderly HTTP close before issuing
         * the host's explicit Close, which deliberately permits abort. */
        xrtTlsStreamDestroy(S.Probe);S.Probe=NULL;
        httpInteropRequire(xrpcWsWaitFor(S.Peer,10000)==XWAIT_OK,"rejected HTTP response and native physical tail precede explicit abort");
    }
    xrpcwshttpinfo H={.Size=sizeof(H)};xrpcwsinfo I={.Size=sizeof(I)};httpInteropRequire(xrpcWsHttpInfo(S.Peer,&H)&&xrpcWsInfo(S.Peer,&I),"HTTP/native snapshots");if(Normal)httpInteropRequire(H.Outcome==XRPC_WS_HTTP_OPEN&&H.Status==101&&I.ProtocolBytes==7&&!memcmp(I.Protocol,"jsonrpc",7)&&I.Encrypted==S.Secure&&(!S.Secure||I.Version),"checked upgrade role/security/protocol");else httpInteropRequire(!I.ReadMessages&&(!I.WriteMessages||!strcmp(S.Mode,"deflate-bomb"))&&!xrtAtomic32Load(&S.Calls,XMEMORY_ACQUIRE),"failed upgrade/decode has no JSON/business");
    #if defined(XRPC_FEATURE_XRPC_WS_HTTP_DEFLATE)
    if(Argc==8&&Normal){xrpcwsdeflateinfo D={.Size=sizeof(D)};httpInteropRequire(xrpcWsDeflateInfo(S.Peer,&D)&&D.Enabled==(strcmp(Argv[7],"decline")!=0)&&D.CompressOutput,"actual negotiated compression snapshot after physical close");}
    #endif
    unsigned ResultStatus=(unsigned)R->Status,Delivery=(unsigned)R->Delivery;xrtTlsStreamDestroy(S.Probe);S.Probe=NULL;
    httpInteropRequire(xrpcWsClose(S.Peer)&&xrpcWsWaitFor(S.Peer,10000)==XWAIT_OK&&xrpcWsDestroy(S.Peer),"physical peer tail");xrtFutureDestroy(S.Call);if(S.Listener){httpInteropRequire(xrtNetListenerClose(S.Listener),"listener close");xrtNetListenerDestroy(S.Listener);}httpInteropRequire(xrpcServerDestroy(S.Server),"shared Server join");
    #if defined(XRPC_FEATURE_XRPC_WS_URI)
    if(S.Resolver)httpInteropRequire(xrtNetResolverDestroy(S.Resolver),"host URI Resolver physical join");
    #endif
    End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(S.Engine))==XNET_RETIRE_BUSY){httpInteropRequire(xrtTimer()<End,"host tail");xrtSleep(1);}httpInteropRequire(Q==XNET_RETIRE_READY&&xrtAtomic32Load(&S.NativeRefs,XMEMORY_ACQUIRE)==1,"Engine/native original owners");xrtTlsVerifierRelease(S.Verifier);xrtTlsIdentityRelease(S.Identity);
    printf("PASS role=%s transport=%s mode=%s outcome=%u status=%u call=%u delivery=%u\n",S.Client?"client":"server",S.Secure?"tls":"tcp",S.Mode,(unsigned)H.Outcome,(unsigned)H.Status,ResultStatus,Delivery);return 0;
}
