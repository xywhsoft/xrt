#include "../tls/material.h"
#include <stdlib.h>
static bool ready(xfuture* F)
{return F && xrtFutureWaitFor(F,10000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK;}
int main(int Count,char** Args)
{
    if(Count==1){puts("Usage: xmqtt-session-tls HOST PORT SERVER_CA.pem CLIENT_ID");return 0;}if(Count!=5)return 2;
    char* End=NULL;unsigned long Port=strtoul(Args[2],&End,10);if(*End || !Port || Port>UINT16_MAX)return 2;
    xmqttsession *S=NULL;xmqttclientinbox* Inbox=NULL;xmqttclient* C=NULL;xnetengine* E=NULL;xnetresolver* Resolver=NULL;xtlsverifier* Verifier=NULL;xx509store* Store=NULL;
    xfuture *Ready=NULL,*Sent=NULL,*Bye=NULL;int Code=1;size_t N=0;bytes CA=xmqttExampleTlsRead(Args[3],65536,&N);if(!CA)goto Done;
    Store=xrtX509StoreCreate();bool Added=Store && xrtX509StoreAddData(Store,(xbytesview){CA,N},NULL);xrtSecureZero(CA,N);xrtFree(CA);if(!Added)goto Done;
    xtlsverifierconfig VC;xrtTlsVerifierConfigInit(&VC);VC.Store=Store;xtlsverifylimits VL;xrtTlsVerifyLimitsInit(&VL);Verifier=xrtTlsVerifierCreateLimited(&VC,&VL);if(!Verifier)goto Done;
    xmqttsessionconfig SK;xmqttSessionConfigInit(&SK);SK.ClientId=(xstrview){Args[4],strlen(Args[4])};if(!xmqttSessionCreate(&SK,&S) || !xmqttClientInboxCreate(NULL,&Inbox))goto Done;
    for(unsigned Generation=0;Generation<2;++Generation){
        xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=1;E=xrtNetEngineCreate(&EC);if(!E || !xrtNetEngineStart(E))goto Done;Resolver=xrtNetResolverCreate(NULL);if(!Resolver)goto Done;
        xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Verifier=Verifier;static const xstrview ALPN[]={{"mqtt",4}};T.Session.Protocols=ALPN;T.Session.ProtocolCount=1;
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=SK.ClientId;Q.Body.Connect.CleanStart=false;
        xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;Q.Properties=(xmqttpropertylist){&Expiry,1};
        if(!xmqttClientTlsDialSession(E,Resolver,(xstrview){Args[1],strlen(Args[1])},(uint16)Port,0,NULL,&T,S,Inbox,&Q,&C))goto Done;
        Ready=xmqttClientReady(C);if(!ready(Ready))goto Done;
        const xmqttreadyresult* R=xrtFutureValue(Ready);if((!Generation && R->Negotiated.SessionPresent) || (Generation && !R->Negotiated.SessionPresent))goto Done;
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=Generation?4:2;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/session/tls");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("hello\0TLS");
        if(xmqttClientPublish(C,&Q,5000,&Sent)!=XMQTT_ADMISSION_ACCEPTED || ((const xmqttwriteresult*)xrtFutureValue(Sent))->Status!=XMQTT_STATUS_OK)goto Done;
        if(Generation && (xmqttClientDisconnect(C,NULL,5000,&Bye)!=XMQTT_ADMISSION_ACCEPTED || ((const xmqttwriteresult*)xrtFutureValue(Bye))->Status!=XMQTT_STATUS_OK))goto Done;
        if(!xmqttClientClose(C) || xmqttClientWaitFor(C,10000)!=XWAIT_OK || !xmqttClientDestroy(C))goto Done;
        C=NULL;
        if(!xrtNetResolverDestroy(Resolver))goto Done;
        Resolver=NULL;if(!xrtNetEngineDestroy(E))goto Done;
        E=NULL;
        xrtFutureDestroy(Ready);Ready=NULL;xrtFutureDestroy(Sent);Sent=NULL;xrtFutureDestroy(Bye);Bye=NULL;
    }
    puts("[PASS] verified TLS, two Engines, retained MQTT session, QoS1/2 and physical close");Code=0;
Done:
    if(C && (!xmqttClientClose(C) || xmqttClientWaitFor(C,10000)!=XWAIT_OK || !xmqttClientDestroy(C)))Code=1;
    if(Resolver && !xrtNetResolverDestroy(Resolver))Code=1;
    if(E && !xrtNetEngineDestroy(E))Code=1;
    if(!xmqttSessionDestroy(S) || !xmqttClientInboxDestroy(Inbox))Code=1;
    xrtFutureDestroy(Ready);xrtFutureDestroy(Sent);xrtFutureDestroy(Bye);xrtTlsVerifierRelease(Verifier);xrtX509StoreFree(Store);
    if(Code)fputs("TLS Session example failed; use a fresh unique ClientId and inspect typed outcomes.\n",stderr);
    return Code;
}
