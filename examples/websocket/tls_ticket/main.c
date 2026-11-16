#include "../../../tests/fixtures/tls_server.h"
#include <xrt/websocket_stream.h>
#include <xrt/thread.h>
/* Local native attachment demo. A network-facing host must first validate its
 * HTTP Upgrade; HTTP/broker setup is intentionally outside this Core example. */
typedef struct ticketDemo ticketDemo;
typedef struct demoAttach {ticketDemo* Demo;xtlsstream* Native;xnetpost Post;} demoAttach;
struct ticketDemo {
    xnetengine* Engine;xnetlistener* Listener;xtlscontext* Context;
    xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig ServerConfig;
    xatomicptr Client,Server;xatomic32 Captured,ListenerClosed;
    demoAttach Attach[2];xtlsresume *ClientCredential,*ServerCredential;
};
static void demoAwait(xatomic32* A)
{double End=xrtTimer()+10;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"native demo deadline");xrtSleep(1);}}
static xtlsverifydecision demoVerify(const xtlspeer* P,ptr Data)
{ticketDemo* D=Data;xbytesview DER={0};return P&&P->Role==XTLS_SERVER&&P->CertificateCount&&P->Name.Size==14&&!memcmp(P->Name.Data,"ticket.fixture",14)&&xrtTlsIdentityCertificate(D->Identity,0,&DER)&&P->Certificates[0].Raw.Size==DER.Size&&!memcmp(P->Certificates[0].Raw.Data,DER.Data,DER.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
static void demoTicket(xwsstream* S,ptr Data)
{
    ticketDemo* D=Data;xtlsresume* R;
    while((R=xrtTlsClientTakeResume(xrtTlsStreamSession(xrtWsStreamTls(S))))!=NULL){xrtTlsResumeRelease(D->ClientCredential);D->ClientCredential=R;}
    testRequire(D->ClientCredential&&xrtWsStreamSetTlsTicket(S,NULL),"move credential and remove observer");xrtAtomic32Store(&D->Captured,1,XMEMORY_RELEASE);
}
static void demoAttachWs(xnetworker* W,ptr Data)
{
    (void)W;demoAttach* A=Data;ticketDemo* D=A->Demo;bool Client=xrtTlsSessionRole(xrtTlsStreamSession(A->Native))==XTLS_CLIENT;xwsstreamconfig K;xrtWsStreamConfigInit(&K);K.Role=Client?XWS_ROLE_CLIENT:XWS_ROLE_SERVER;
    xwsstream* S=xrtWsStreamAttachTls(A->Native,0,&K,NULL,D);testRequire(S!=NULL,"native WS attachment after verified TLS");
    if(Client)testRequire(xrtWsStreamSetTlsTicket(S,demoTicket),"observe actual TLS queue without replacing WS events");
    xrtAtomicPtrStore(Client?&D->Client:&D->Server,S,XMEMORY_RELEASE);
}
static void demoOpen(xtlsstream* T,ptr Data)
{
    ticketDemo* D=Data;demoAttach* A=&D->Attach[xrtTlsSessionRole(xrtTlsStreamSession(T))==XTLS_CLIENT?1:0];A->Demo=D;A->Native=T;
    testRequire(xrtNetPostInit(&A->Post)&&xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(T)),&A->Post,demoAttachWs,A),"leave native Open before WS handoff");
}
static const xtlsstreamevents DemoTlsEvents={.Open=demoOpen};
static bool demoAccept(xnetlistener* L,xnetstream* T,ptr Data)
{(void)L;ticketDemo* D=Data;xtlsstream* S=NULL;testRequire(xrtTlsStreamAccept(T,&D->ServerConfig,NULL,&DemoTlsEvents,D,&S),"accept genuine TLS1.3");return true;}
static void demoListenerClosed(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((ticketDemo*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static void demoIssue(xnetworker* W,ptr Data)
{(void)W;ticketDemo* D=Data;xtlsstream* T=xrtWsStreamTls(xrtAtomicPtrLoad(&D->Server,XMEMORY_ACQUIRE));testRequire(xrtTlsServerTicketNew(xrtTlsStreamSession(T),&D->ServerCredential)==XTLS_OK&&xrtTlsStreamFlush(T)==XTLS_OK,"issue and flush post-attachment TLS ticket without WS payload");}
int main(void)
{
    ticketDemo D={0};D.Context=testTlsServerContext();D.Identity=testTlsServerIdentity();testRequire(D.Context&&D.Identity,"local native TLS fixture");xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=demoVerify;V.Context=&D;D.Verifier=xrtTlsVerifierCreate(&V);testRequire(D.Verifier!=NULL,"pin exact local certificate and peer name");xrtTlsServerConfigInit(&D.ServerConfig);D.ServerConfig.Context=D.Context;D.ServerConfig.Identity=D.Identity;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;D.Engine=xrtNetEngineCreate(&E);testRequire(D.Engine&&xrtNetEngineStart(D.Engine),"borrowed native Engine");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents Events={demoAccept,NULL,demoListenerClosed};D.Listener=xrtNetListen(D.Engine,&L,&Events,NULL,&D);xnetaddr Remote;testRequire(D.Listener&&xrtNetListenerLocal(D.Listener,&Remote),"native loopback listener");
    xtlsclientconfig C;xrtTlsClientConfigInit(&C);C.Context=D.Context;C.Verifier=D.Verifier;C.ServerName=XRT_STR_LITERAL("ticket.fixture");testRequire(xrtTlsStreamConnect(D.Engine,&Remote,1,NULL,&C,NULL,&DemoTlsEvents,&D)!=NULL,"verified native TLS client");double End=xrtTimer()+10;
    while(!xrtAtomicPtrLoad(&D.Client,XMEMORY_ACQUIRE)||!xrtAtomicPtrLoad(&D.Server,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"both native WS peers attached");xrtSleep(1);}
    testRequire(xrtNetEnginePost(D.Engine,0,demoIssue,&D),"issue on actual server Worker");demoAwait(&D.Captured);xwsstream* Client=xrtAtomicPtrLoad(&D.Client,XMEMORY_ACQUIRE);xwsstream* Server=xrtAtomicPtrLoad(&D.Server,XMEMORY_ACQUIRE);
    testRequire(xrtWsStreamAbort(Client),"finish client");if(xrtWsStreamState(Server)!=XWS_STREAM_CLOSED)(void)xrtWsStreamAbort(Server);
    testRequire(xrtWsStreamWaitRetired(Client,5000)==XWAIT_OK&&xrtWsStreamWaitRetired(Server,5000)==XWAIT_OK,"actual WS/TLS/TCP retirement");xrtWsStreamDestroy(Client);xrtWsStreamDestroy(Server);xrtNetListenerClose(D.Listener);demoAwait(&D.ListenerClosed);xrtNetListenerDestroy(D.Listener);testRequire(xrtNetEngineDestroy(D.Engine),"Engine physically joined before context/code release");
    xtlsresumeinfo I;testRequire(xrtTlsResumeInfo(D.ClientCredential,&I)&&I.Ticket.Size&&I.Secret.Size,"credential independently survives all native owners");puts("[PASS] native WSS ticket notification, reference handoff and physical joins");xrtTlsResumeRelease(D.ClientCredential);xrtTlsResumeRelease(D.ServerCredential);xrtTlsVerifierRelease(D.Verifier);xrtTlsIdentityRelease(D.Identity);xrtTlsContextRelease(D.Context);return 0;
}
