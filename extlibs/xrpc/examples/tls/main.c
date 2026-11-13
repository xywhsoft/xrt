#include <xrpc/tls.h>
#include <xrt/thread.h>
#include "../../../../tests/fixtures/tls_server.h"
static xtlsidentity* Identity;static xtlsserverconfig NativeServer;static xrpctlsconfig Config;static xatomicptr Accepted;
static xtlsverifydecision tlsDemoPin(const xtlspeer* Peer,ptr Data)
{(void)Data;xbytesview Pin={0};if(!Peer || !Peer->CertificateCount || !xrtTlsIdentityCertificate(Identity,0,&Pin))return XTLS_VERIFY_REJECT;return Peer->Certificates[0].Raw.Size==Pin.Size && !memcmp(Peer->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
static bool tlsDemoEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr Data)
{(void)C;(void)Data;return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));}
static bool tlsDemoAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;(void)Data;xrpctls* P=NULL;if(!xrpcTlsAccept(S,&NativeServer,&Config,&P))return false;xrtAtomicPtrStore(&Accepted,P,XMEMORY_RELEASE);return true;}
int main(void)
{
    xrtAtomicPtrInit(&Accepted,NULL);xtlscontext* Context=testTlsServerContext();Identity=testTlsServerIdentity();testRequire(Context && Identity,"TLS demo fixture");
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=tlsDemoPin;xtlsverifier* Verifier=xrtTlsVerifierCreate(&V);testRequire(Verifier!=NULL,"TLS demo explicit certificate pin");
    xrtTlsServerConfigInit(&NativeServer);NativeServer.Context=Context;NativeServer.Identity=Identity;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);testRequire(Engine && xrtNetEngineStart(Engine),"TLS demo Engine");
    xrpcserverconfig Business;xrpcServerConfigInit(&Business);Business.Call=tlsDemoEcho;xrpcserver* Server=NULL;testRequire(xrpcServerCreate(&Business,&Server),"TLS demo shared business executor");xrpcTlsConfigInit(&Config);Config.Duplex.Server=Server;
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents LE={tlsDemoAccept,NULL,NULL};xnetlistener* Listener=xrtNetListen(Engine,&L,&LE,NULL,NULL);xnetaddr Address;testRequire(Listener && xrtNetListenerLocal(Listener,&Address),"TLS demo listener");
    xtlsclientconfig C;xrtTlsClientConfigInit(&C);C.Context=Context;C.Verifier=Verifier;C.ServerName=XRT_STR_LITERAL("rpc.fixture");xrpctls* Left=NULL;testRequire(xrpcTlsConnect(Engine,&Address,1,&C,&Config,&Left),"TLS demo client");
    double End=xrtTimer()+10;xrpctls* Right;while(!(Right=xrtAtomicPtrLoad(&Accepted,XMEMORY_ACQUIRE))){testRequire(xrtTimer()<End,"TLS demo accept");xrtSleep(1);}
    xfuture* F[2];testRequire(xrpcTlsCall(Left,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),5000,F)==XRPC_DUPLEX_ACCEPTED && xrpcTlsCall(Right,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[9]"),5000,F+1)==XRPC_DUPLEX_ACCEPTED,"TLS demo bidirectional admission");
    for(unsigned I=0;I<2;++I){testRequire(xrtFutureWaitFor(F[I],10000)==XWAIT_OK,"TLS demo response");const xrpccallresult* R=xrtFutureValue(F[I]);testRequire(R && R->Status==XRPC_CALL_OK && R->Delivery==XRPC_REPLIED,"TLS demo exact native result");xrpcresponseinfo Info={0};Info.Size=sizeof(Info);testRequire(xrpcResponsesInfo(R->Responses,&Info),"TLS demo retained response view");printf("%.*s\n",(int)Info.Messages[R->Index].Result.Raw.Size,Info.Messages[R->Index].Result.Raw.Data);}
    testRequire(xrpcTlsClose(Left) && xrpcTlsClose(Right) && xrpcTlsWaitFor(Left,10000)==XWAIT_OK && xrpcTlsWaitFor(Right,10000)==XWAIT_OK && xrpcTlsDestroy(Left) && xrpcTlsDestroy(Right),"TLS demo physical peers");
    for(unsigned I=0;I<2;++I)xrtFutureDestroy(F[I]);
    xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);testRequire(xrpcServerDestroy(Server),"TLS demo executor join");End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"TLS demo Engine resource tail");xrtSleep(1);}testRequire(Q==XNET_RETIRE_READY,"TLS demo Engine join");xrtTlsVerifierRelease(Verifier);xrtTlsIdentityRelease(Identity);xrtTlsContextRelease(Context);return 0;
}
