#include "test.h"
#include "support/tls_peer.h"
#include "../../../tests/fixtures/tls_host_inference.h"
static void mqttHostCase(xmqttversion V,unsigned Kind,unsigned Policy)
{
    static const xstrview Hosts[]={{"example.com",11},{"127.0.0.1",9},{"::1",3},{"very-long-broker-service-name.fixture.invalid",45}};
    xstrview Host=Hosts[Kind];bool Numeric=Kind==1||Kind==2;testhostobserver O;testHostInit(&O,Host,Policy,Numeric);mqttTlsPeer P;mqttTlsStart(&P,V,0);
    if(Kind==2){testRequire(xrtNetListenerClose(P.Listener),"replace IPv4 listener");mqttTlsWait(&P.ListenerClosed,1,"old listener close");xrtNetListenerDestroy(P.Listener);xrtAtomic32Store(&P.ListenerClosed,0,XMEMORY_RELEASE);
        xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV6,0);L.Distribution=XNET_ACCEPT_LOCAL;xnetlistenerevents E={mqttTlsAccept,NULL,mqttTlsListenerClosed};P.Listener=xrtNetListen(P.Engine,&L,&E,NULL,&P);testRequire(P.Listener&&xrtNetListenerLocal(P.Listener,&P.Address),"real IPv6 host listener");}
    testRequire(xrtNetResolverDestroy(P.Resolver),"replace inactive resolver");O.Address=P.Address;O.Identity=P.Identity;P.Resolver=testHostResolver(&O);P.Config.Select=testHostSelect;P.Config.SelectContext=&O;
    xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=P.Context;xtlsverifier* Verifier=testHostConfig(&O,Policy,&T.Session,&T.ServerNameFromHost);static const xstrview ALPN={"mqtt",4};T.Session.Protocols=&ALPN;T.Session.ProtocolCount=1;
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=V;xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("host-inference");
    char Counted[128];memcpy(Counted,Host.Data,Host.Size);xmqttclient* C=NULL;testHostAmbient(&O);
    bool OK=xmqttClientTlsDial(P.Engine,P.Resolver,(xstrview){Counted,Host.Size},P.Address.Port,1,&K,&T,&Q,&C);
    xrtSetErrorHandler(NULL,NULL);testRequire(OK&&C&&xrtGetError()==O.Ambient,"TLS factory preserves exact ambient slot");memset(Counted,'x',sizeof(Counted));xrtTlsVerifierRelease(Verifier);
    xfuture* Ready=xmqttClientReady(C);testRequire(Ready&&xrtFutureWaitFor(Ready,15000)==XWAIT_OK&&((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"TLS host plus MQTT readiness");
    testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,15000)==XWAIT_OK&&xmqttClientDestroy(C),"client physical host snapshot retirement");mqttTlsStop(&P);xrtFutureDestroy(Ready);
    testHostResult(&O);xrtClearError();
}
int main(void)
{for(unsigned V=0;V<2;++V)for(unsigned Kind=0;Kind<4;++Kind)for(unsigned Policy=0;Policy<4;++Policy)mqttHostCase(V?XMQTT_VERSION_311:XMQTT_VERSION_5,Kind,Policy);puts("[PASS]32 MQTT5/311 DNS/IPv4/IPv6/long-host cases, wire SNI, pinned identity, explicit overrides and quiet inference");return 0;}
