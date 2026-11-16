#include "test.h"
#include "support/tls_peer.h"
#include "../../../tests/fixtures/tls_host_inference.h"
static void redisHostCase(unsigned Kind,unsigned Policy)
{
    static const xstrview Hosts[]={{"redis.fixture",13},{"127.0.0.1",9},{"::1",3},{"very-long-redis-service-name.fixture.invalid",44}};
    xstrview Host=Hosts[Kind];bool Numeric=Kind==1||Kind==2;testhostobserver O;testHostInit(&O,Host,Policy,Numeric);redistlspeer P;redisTlsPeerStart(&P,false,false,0);redispeer* Peer=&P.Dial.Init.Peer;
    if(Kind==2){testRequire(xrtNetListenerClose(Peer->Listener),"replace IPv4 listener");redisPeerWait(&Peer->ListenerClosed,1);xrtNetListenerDestroy(Peer->Listener);xrtAtomic32Store(&Peer->ListenerClosed,0,XMEMORY_RELEASE);
        xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV6,0);L.Affinity=1;L.Distribution=XNET_ACCEPT_LOCAL;Peer->Listener=xrtNetListen(Peer->Engine,&L,&redisPeerListenerEvents,NULL,Peer);testRequire(Peer->Listener&&xrtNetListenerLocal(Peer->Listener,&Peer->Address),"real IPv6 Redis listener");}
    testRequire(xrtNetResolverDestroy(P.Dial.Resolver),"replace inactive Redis resolver");O.Address=Peer->Address;O.Identity=P.Identity;P.Dial.Resolver=testHostResolver(&O);P.Options.Session.Select=testHostSelect;P.Options.Session.SelectContext=&O;
    xredisclienttlsconfig T;redisTlsPeerConfig(&P,&T);xtlsverifier* Verifier=testHostConfig(&O,Policy,&T.Session,&T.ServerNameFromHost);
    char Counted[128];memcpy(Counted,Host.Data,Host.Size);xredisclient* C=NULL;testHostAmbient(&O);
    bool OK=xredisClientTlsDial(Peer->Engine,P.Dial.Resolver,(xstrview){Counted,Host.Size},Peer->Address.Port,0,NULL,NULL,&T,&C);
    xrtSetErrorHandler(NULL,NULL);testRequire(OK&&C&&xrtGetError()==O.Ambient,"Redis factory preserves exact ambient slot");memset(Counted,'x',sizeof(Counted));xrtTlsVerifierRelease(Verifier);
    xfuture* Ready=redisDialReady(C);testRequire(((const xredisreadyresult*)xrtFutureValue(Ready))->Status==XREDIS_READY_OK,"pinned host TLS plus Redis readiness");
    redisDialClientClose(C);redisTlsPeerStop(&P);xrtFutureDestroy(Ready);testHostResult(&O);xrtClearError();
}
int main(void)
{for(unsigned Kind=0;Kind<4;++Kind)for(unsigned Policy=0;Policy<4;++Policy)redisHostCase(Kind,Policy);puts("[PASS]16 Redis DNS/IPv4/IPv6/long-host cases, wire SNI, pinned identity, explicit overrides and quiet inference");return 0;}
