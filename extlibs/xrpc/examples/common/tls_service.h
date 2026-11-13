#ifndef XRPC_EXAMPLE_TLS_SERVICE_H
#define XRPC_EXAMPLE_TLS_SERVICE_H
#include <xrpc/tls.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../../tests/fixtures/tls_server.h"
/* Local repository fixture only: historical public identity + explicit DER pin.
 * Early failures terminate; embedded applications unwind every successful owner. */
typedef struct rpcTlsService {
    xnetengine* Engine;xnetlistener* Listener;xrpcserver* Server;xrpctls* Accepted;
    xnetaddr Address;xrpctlsconfig Config;xatomic32 Ready,Calls,VerifyCalls,VerifyEntered,VerifyPermit;
    xtlscontext* TLSContext;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig Native;
    xstrview Protocol;bool Reject,HoldVerify;
} rpcTlsService;
static void rpcTlsServiceRequire(bool OK,cstr Message)
{if(!OK){fprintf(stderr,"%s: %s\n",Message,xrtErrorMessage(xrtGetError()));exit(1);}}
static bool rpcTlsServiceEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr Data)
{
    (void)C;rpcTlsService* S=Data;rpcTlsServiceRequire(!xrtNetEngineCurrent(S->Engine),"TLS business runs off network worker");
    xrtAtomic32FetchAdd(&S->Calls,1,XMEMORY_RELAXED);return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}
static xtlsverifydecision rpcTlsServicePin(const xtlspeer* Peer,ptr Data)
{
    rpcTlsService* S=Data;xrtAtomic32FetchAdd(&S->VerifyCalls,1,XMEMORY_RELAXED);xrtAtomic32Store(&S->VerifyEntered,1,XMEMORY_RELEASE);
    if(S->HoldVerify){double End=xrtTimer()+10;while(!xrtAtomic32Load(&S->VerifyPermit,XMEMORY_ACQUIRE)){rpcTlsServiceRequire(xrtTimer()<End,"release held test verifier");xrtSleep(1);}}
    xbytesview Pin={0};if(S->Reject || !Peer || !Peer->CertificateCount || Peer->Name.Size!=11 || memcmp(Peer->Name.Data,"rpc.fixture",11) || !xrtTlsIdentityCertificate(S->Identity,0,&Pin))return XTLS_VERIFY_REJECT;
    return Peer->Certificates[0].Raw.Size==Pin.Size && !memcmp(Peer->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;
}
static bool rpcTlsServiceAccept(xnetlistener* L,xnetstream* Stream,ptr Data)
{(void)L;rpcTlsService* S=Data;if(!xrpcTlsAccept(Stream,&S->Native,&S->Config,&S->Accepted))return false;xrtAtomic32Store(&S->Ready,1,XMEMORY_RELEASE);return true;}
static void rpcTlsServiceStart(rpcTlsService* S,unsigned Format,bool Fragment,bool Reject,bool HoldVerify)
{
    memset(S,0,sizeof(*S));S->Reject=Reject;S->HoldVerify=HoldVerify;xrtAtomic32Init(&S->Ready,0);xrtAtomic32Init(&S->Calls,0);xrtAtomic32Init(&S->VerifyCalls,0);xrtAtomic32Init(&S->VerifyEntered,0);xrtAtomic32Init(&S->VerifyPermit,0);
    S->TLSContext=testTlsServerContext();S->Identity=testTlsServerIdentity();rpcTlsServiceRequire(S->TLSContext && S->Identity,"local TLS fixture");
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=rpcTlsServicePin;V.Context=S;S->Verifier=xrtTlsVerifierCreate(&V);rpcTlsServiceRequire(S->Verifier!=NULL,"explicit test pin");
    S->Protocol=XRT_STR_LITERAL("rpc-test");xrtTlsServerConfigInit(&S->Native);S->Native.Context=S->TLSContext;S->Native.Identity=S->Identity;S->Native.Protocols=&S->Protocol;S->Native.ProtocolCount=1;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;S->Engine=xrtNetEngineCreate(&E);rpcTlsServiceRequire(S->Engine && xrtNetEngineStart(S->Engine),"TLS host Engine");
    xrpcserverconfig B;xrpcServerConfigInit(&B);B.Call=rpcTlsServiceEcho;B.User=S;rpcTlsServiceRequire(xrpcServerCreate(&B,&S->Server),"TLS shared Server");
    xrpcTlsConfigInit(&S->Config);S->Config.Duplex.Server=S->Server;S->Config.Frame.Format=(xrpcframeformat)Format;
    if(Fragment){S->Config.ReadChunkBytes=3;S->Config.ReadBytes=7;S->Config.WriteChunkBytes=1;S->Config.WriteBytes=5;S->Config.WorkFrames=2;S->Config.MaxCipherBytes=256;}
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents Events={rpcTlsServiceAccept,NULL,NULL};S->Listener=xrtNetListen(S->Engine,&L,&Events,NULL,S);
    rpcTlsServiceRequire(S->Listener && xrtNetListenerLocal(S->Listener,&S->Address),"TLS loopback listener");
}
static void rpcTlsServiceClient(rpcTlsService* S,xtlsclientconfig* C)
{xrtTlsClientConfigInit(C);C->Context=S->TLSContext;C->Verifier=S->Verifier;C->ServerName=XRT_STR_LITERAL("rpc.fixture");C->Protocols=&S->Protocol;C->ProtocolCount=1;}
static const xrpccallresult* rpcTlsServiceResult(xfuture* F)
{rpcTlsServiceRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK,"TLS call wait");const xrpccallresult* R=xrtFutureValue(F);rpcTlsServiceRequire(R!=NULL,"TLS typed result");return R;}
static void rpcTlsServicePeerClose(xrpctls* P)
{if(P)rpcTlsServiceRequire(xrpcTlsClose(P) && xrpcTlsWaitFor(P,10000)==XWAIT_OK && xrpcTlsDestroy(P),"TLS physical peer close");}
static void rpcTlsServiceCycleClose(rpcTlsService* S,xrpctls* P)
{rpcTlsServicePeerClose(P);if(xrtAtomic32Load(&S->Ready,XMEMORY_ACQUIRE))rpcTlsServicePeerClose(S->Accepted);S->Accepted=NULL;xrtAtomic32Store(&S->Ready,0,XMEMORY_RELEASE);}
static void rpcTlsServiceStop(rpcTlsService* S)
{
    rpcTlsServiceRequire(!S->Accepted && xrtNetListenerClose(S->Listener),"TLS listener close after peers");xrtNetListenerDestroy(S->Listener);rpcTlsServiceRequire(xrpcServerDestroy(S->Server),"TLS Server join");
    double End=xrtTimer()+10;xnetretireresult R;while((R=xrtNetEngineTryDestroy(S->Engine))==XNET_RETIRE_BUSY){rpcTlsServiceRequire(xrtTimer()<End,"TLS Engine resource/pin tail");xrtSleep(1);}rpcTlsServiceRequire(R==XNET_RETIRE_READY,"TLS Engine join");
    if(S->Verifier)xrtTlsVerifierRelease(S->Verifier);
    xrtTlsIdentityRelease(S->Identity);xrtTlsContextRelease(S->TLSContext);
}
#endif
