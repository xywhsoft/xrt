#include "material.h"
#include <stdlib.h>
int main(int Count,char** Args)
{
    if(Count==1){puts("Usage: xmqtt-tls HOST PORT SERVER_CA.pem [CLIENT_CHAIN.pem RSA_KEY.pem]");return 0;}
    if(Count!=4 && Count!=6)return 2;
    char* End=NULL;unsigned long Port=strtoul(Args[2],&End,10);if(*End || !Port || Port>UINT16_MAX)return 2;
    xnetengine* E=NULL;xnetresolver* Resolver=NULL;xx509store* Store=NULL;xtlsverifier* V=NULL;xtlsidentity* Identity=NULL;xmqttclient* C=NULL;xfuture *Ready=NULL,*Sent=NULL,*Bye=NULL;int Code=1;
    size_t N=0;bytes CA=xmqttExampleTlsRead(Args[3],65536,&N);if(!CA)goto Done;Store=xrtX509StoreCreate();
    bool Added=Store && xrtX509StoreAddData(Store,(xbytesview){CA,N},NULL);xrtSecureZero(CA,N);xrtFree(CA);if(!Added)goto Done;
    xtlsverifierconfig Verify;xrtTlsVerifierConfigInit(&Verify);Verify.Store=Store;xtlsverifylimits Limits;xrtTlsVerifyLimitsInit(&Limits);V=xrtTlsVerifierCreateLimited(&Verify,&Limits);if(!V)goto Done;
    xnetengineconfig Network;xrtNetEngineConfigInit(&Network);Network.Workers=1;E=xrtNetEngineCreate(&Network);if(!E || !xrtNetEngineStart(E))goto Done;Resolver=xrtNetResolverCreate(NULL);if(!Resolver)goto Done;
    xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Verifier=V;xtlsclientauthconfigv1 Auth;
    static const xstrview Protocols[]={{"mqtt",4}};T.Session.Protocols=Protocols;T.Session.ProtocolCount=1;
    if(Count==6){Identity=xmqttExampleTlsIdentity(Args[4],Args[5]);if(!Identity)goto Done;xrtTlsClientAuthConfigInitV1(&Auth);Auth.Identity=Identity;T.ClientAuth=&Auth;}
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("xrt-tls-example");
    if(!xmqttClientTlsDial(E,Resolver,(xstrview){Args[1],strlen(Args[1])},(uint16)Port,0,NULL,&T,&Q,&C))goto Done;
    xrtTlsVerifierRelease(V);V=NULL;xrtTlsIdentityRelease(Identity);Identity=NULL;
    Ready=xmqttClientReady(C);if(!Ready || xrtFutureWaitFor(Ready,10000)!=XWAIT_OK || ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status!=XMQTT_STATUS_OK)goto Done;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/xrt");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("hello");
    if(xmqttClientPublish(C,&Q,5000,&Sent)!=XMQTT_ADMISSION_ACCEPTED || ((const xmqttwriteresult*)xrtFutureValue(Sent))->Status!=XMQTT_STATUS_OK)goto Done;
    if(xmqttClientDisconnect(C,NULL,5000,&Bye)!=XMQTT_ADMISSION_ACCEPTED || ((const xmqttwriteresult*)xrtFutureValue(Bye))->Status!=XMQTT_STATUS_OK)goto Done;
    puts("[PASS] trusted TLS, optional client proof, CONNACK/PUBACK and authenticated shutdown");Code=0;
Done:
    if(C && (!xmqttClientClose(C) || xmqttClientWaitFor(C,10000)!=XWAIT_OK || !xmqttClientDestroy(C)))Code=1;
    if(Resolver && !xrtNetResolverDestroy(Resolver))Code=1;
    if(E && !xrtNetEngineDestroy(E))Code=1;
    xrtFutureDestroy(Ready);xrtFutureDestroy(Sent);xrtFutureDestroy(Bye);xrtTlsIdentityRelease(Identity);xrtTlsVerifierRelease(V);xrtX509StoreFree(Store);
    if(Code)fputs("TLS example failed; inspect typed readiness and explicit trust/identity configuration. Select tls_identity_rsa for RSA key files.\n",stderr);
    return Code;
}
