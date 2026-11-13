/* Local lifecycle demo with an explicit pin to the historical public fixture.
 * This verifier is not a general remote-service trust policy. */
#include "../../../tests/fixtures/tls_server.h"
static xtlsidentity* Identity;static xtlsserverconfig ServerConfig;static xatomicptr Accepted;static xatomic32 Opened;
static void retireDemoOpen(xtlsstream* S,ptr Data)
{(void)S;(void)Data;xrtAtomic32FetchAdd(&Opened,1,XMEMORY_RELEASE);}
static const xtlsstreamevents Events={.Open=retireDemoOpen};
static xtlsverifydecision retireDemoPin(const xtlspeer* Peer,ptr Data)
{
    (void)Data;xbytesview Pin={0};if(!Peer || !Peer->CertificateCount || !xrtTlsIdentityCertificate(Identity,0,&Pin))return XTLS_VERIFY_REJECT;
    return Peer->Certificates[0].Raw.Size==Pin.Size && !memcmp(Peer->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;
}
static bool retireDemoAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;(void)Data;xtlsstream* TLS=NULL;if(!xrtTlsStreamAccept(S,&ServerConfig,NULL,&Events,NULL,&TLS))return false;xrtAtomicPtrStore(&Accepted,TLS,XMEMORY_RELEASE);return true;}
int main(void)
{
    xrtAtomicPtrInit(&Accepted,NULL);xrtAtomic32Init(&Opened,0);xtlscontext* Context=testTlsServerContext();Identity=testTlsServerIdentity();testRequire(Context && Identity,"demo fixture identity");
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=retireDemoPin;xtlsverifier* Verifier=xrtTlsVerifierCreate(&V);testRequire(Verifier!=NULL,"demo explicit certificate pin");
    xrtTlsServerConfigInit(&ServerConfig);ServerConfig.Context=Context;ServerConfig.Identity=Identity;xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);testRequire(Engine && xrtNetEngineStart(Engine),"demo Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents LE={retireDemoAccept,NULL,NULL};xnetlistener* Listener=xrtNetListen(Engine,&L,&LE,NULL,NULL);xnetaddr Remote;testRequire(Listener && xrtNetListenerLocal(Listener,&Remote),"demo listener");
    xtlsclientconfig C;xrtTlsClientConfigInit(&C);C.Context=Context;C.Verifier=Verifier;C.ServerName=XRT_STR_LITERAL("retire.fixture");xtlsstream* Client=xrtTlsStreamConnect(Engine,&Remote,1,NULL,&C,NULL,&Events,NULL);testRequire(Client!=NULL,"demo TLS connect");
    double End=xrtTimer()+10;while(xrtAtomic32Load(&Opened,XMEMORY_ACQUIRE)!=2 || !xrtAtomicPtrLoad(&Accepted,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"demo handshake timeout");xrtSleep(1);}
    xtlsstream* Server=xrtAtomicPtrLoad(&Accepted,XMEMORY_ACQUIRE);testRequire(xrtTlsStreamAbort(Client) && xrtTlsStreamWaitRetired(Client,5000)==XWAIT_OK && xrtTlsStreamWaitRetired(Server,5000)==XWAIT_OK,"demo physical TLS/TCP retirement");
    xrtTlsStreamDestroy(Client);xrtTlsStreamDestroy(Server);xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);End=xrtTimer()+5;xnetretireresult R;
    while((R=xrtNetEngineTryDestroy(Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<End,"demo Engine tail");xrtSleep(1);}testRequire(R==XNET_RETIRE_READY,"demo Engine join");xrtTlsVerifierRelease(Verifier);xrtTlsIdentityRelease(Identity);xrtTlsContextRelease(Context);puts("TLS and TCP callback tails retired; stream owners and Engine destroyed");return 0;
}
