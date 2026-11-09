#include "../tls/material.h"
#include <stdlib.h>
static bool resumeExampleWait(xfuture* F)
{return F && xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED;}
int main(int Count,char** Args)
{
    if(Count==1){puts("usage: xmqtt_tls_resume HOST PORT SERVER_CA.pem (TLS1.3 ticket-capable peer)");return 0;}if(Count!=4)return 2;
    unsigned long Port=strtoul(Args[2],NULL,10);if(!Port || Port>65535)return 2;int Code=1;size_t N=0;bytes CA=xmqttExampleTlsRead(Args[3],65536,&N);xx509store* Store=xrtX509StoreCreate();xtlsverifier* V=NULL;xtlsresume* Ticket=NULL;xnetengine* E=NULL;xnetresolver* Resolver=NULL;xmqttclient* C=NULL;xfuture* F=NULL;
    bool Added=CA && Store && xrtX509StoreAddData(Store,(xbytesview){CA,N},NULL);if(CA){xrtSecureZero(CA,N);xrtFree(CA);}if(!Added)goto Done;
    xtlsverifierconfig Verify;xrtTlsVerifierConfigInit(&Verify);Verify.Store=Store;xtlsverifylimits Limits;xrtTlsVerifyLimitsInit(&Limits);V=xrtTlsVerifierCreateLimited(&Verify,&Limits);if(!V)goto Done;
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=1;E=xrtNetEngineCreate(&EC);if(!E || !xrtNetEngineStart(E))goto Done;Resolver=xrtNetResolverCreate(NULL);if(!Resolver)goto Done;
    for(unsigned Round=0;Round<2;++Round){xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Verifier=V;T.Session.Resume=Ticket;T.Session.ResumeOnly=Round!=0;static const xstrview Protocols[]={{"mqtt",4}};T.Session.Protocols=Protocols;T.Session.ProtocolCount=1;
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("tls-resume-example");
        if(!xmqttClientTlsDial(E,Resolver,(xstrview){Args[1],strlen(Args[1])},(uint16)Port,0,NULL,&T,&Q,&C))goto Done;
        xrtTlsResumeRelease(Ticket);Ticket=NULL;
        F=xmqttClientReady(C);if(!resumeExampleWait(F) || ((const xmqttreadyresult*)xrtFutureValue(F))->Status!=XMQTT_STATUS_OK)goto Done;
        xrtFutureDestroy(F);F=NULL;
        xmqttclienttlsresumeinfo I;if(!xmqttClientTlsResumeInfo(C,&I) || !I.Established || I.Resumed!=(Round!=0))goto Done;
        if(!Round){double End=xrtTimer()+10;while(!xmqttClientTlsTakeResume(C,&Ticket)){if(xrtTimer()>=End)goto Done;xrtSleep(1);}}
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/resume");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("hello");
        if(xmqttClientPublish(C,&Q,5000,&F)!=XMQTT_ADMISSION_ACCEPTED || ((const xmqttwriteresult*)xrtFutureValue(F))->Status!=XMQTT_STATUS_OK)goto Done;
        xrtFutureDestroy(F);F=NULL;
        if(xmqttClientDisconnect(C,NULL,5000,&F)!=XMQTT_ADMISSION_ACCEPTED || ((const xmqttwriteresult*)xrtFutureValue(F))->Status!=XMQTT_STATUS_OK)goto Done;
        xrtFutureDestroy(F);F=NULL;
        if(xmqttClientWaitFor(C,10000)!=XWAIT_OK || !xmqttClientDestroy(C))goto Done;
        C=NULL;
    }
    puts("[PASS] trusted full/resumed TLS1.3, fresh CONNACK/PUBACK and authenticated shutdown on both connections");Code=0;
Done:
    xrtFutureDestroy(F);if(C && (!xmqttClientClose(C) || xmqttClientWaitFor(C,10000)!=XWAIT_OK || !xmqttClientDestroy(C)))Code=1;xrtTlsResumeRelease(Ticket);xrtTlsVerifierRelease(V);if(Store)xrtX509StoreFree(Store);
    if(Resolver && !xrtNetResolverDestroy(Resolver))Code=1;
    if(E && !xrtNetEngineDestroy(E))Code=1;
    return Code;
}
