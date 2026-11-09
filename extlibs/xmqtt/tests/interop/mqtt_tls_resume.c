#include <xmqtt.h>
#include "../../examples/tls/material.h"
#include <stdlib.h>
static void resumeRequire(bool OK,cstr Text)
{if(!OK){fprintf(stderr,"[FAIL] resume interop: %s cause=%s\n",Text,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static const void* resumeWait(xfuture* F)
{resumeRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"bounded typed outcome");return xrtFutureValue(F);}
static xtlsresume* resumeExpired(xtlsresume* Original)
{
    xtlsresumeinfo I;resumeRequire(xrtTlsResumeInfo(Original,&I),"credential metadata");xtlsresumeconfig K;xrtTlsResumeConfigInit(&K);K.Version=I.Version;K.Cipher=I.Cipher;K.Ticket=I.Ticket;K.Secret=I.Secret;K.ServerName=I.ServerName;K.Protocol=I.Protocol;K.PeerIdentity=I.PeerIdentity;K.AgeAdd=I.AgeAdd;K.Lifetime=1;K.IssuedAt=1;
    xtlsresume* R=xrtTlsResumeCreate(&K);resumeRequire(R!=NULL,"expired credential fixture");size_t Count=xrtTlsResumePeerCertificateCountV1(Original);resumeRequire(Count && Count<=8,"bounded retained chain");xbytesview Certificates[8];for(size_t At=0;At<Count;++At)Certificates[At]=xrtTlsResumePeerCertificateV1(Original,At)->Raw;
    xtlsverifylimits Limits;xrtTlsVerifyLimitsInit(&Limits);xtlsresume* WithPeer=xrtTlsResumeWithPeerCertificatesV1(R,Certificates,Count,&Limits);xrtTlsResumeRelease(R);resumeRequire(WithPeer!=NULL,"expired chain snapshot");return WithPeer;
}
int main(int Argc,char** Argv)
{
    /* PORT scenario(0..5) factory(0..2) MQTT(5|311) CA.pem */
    if(Argc!=6)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10);unsigned Mode=(unsigned)strtoul(Argv[2],NULL,10),Factory=(unsigned)strtoul(Argv[3],NULL,10),Version=(unsigned)strtoul(Argv[4],NULL,10);
    resumeRequire(Port && Port<=65535 && Mode<=5 && Factory<=2 && (Version==5 || Version==311),"arguments");size_t Size=0;bytes CA=xmqttExampleTlsRead(Argv[5],65536,&Size);xx509store* Store=xrtX509StoreCreate();resumeRequire(CA && Store && xrtX509StoreAddData(Store,(xbytesview){CA,Size},NULL),"strict runtime CA");xrtSecureZero(CA,Size);xrtFree(CA);
    xtlsverifierconfig VC;xrtTlsVerifierConfigInit(&VC);VC.Store=Store;xtlsverifylimits Limits;xrtTlsVerifyLimitsInit(&Limits);xtlsverifier* V=xrtTlsVerifierCreateLimited(&VC,&Limits);xrtX509StoreFree(Store);resumeRequire(V!=NULL,"strict verifier");
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=2;xnetengine* E=xrtNetEngineCreate(&EC);resumeRequire(E && xrtNetEngineStart(E),"host engine");xnetresolver* Resolver=xrtNetResolverCreate(NULL);resumeRequire(Resolver!=NULL,"host resolver");xtlsresume* Ticket=NULL;
    for(unsigned Round=0;Round<2;++Round){xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Verifier=V;T.Session.ServerName=T.Session.VerifyName=XRT_STR_LITERAL("localhost");static const xstrview Protocols[]={{"mqtt",4}};T.Session.Protocols=Protocols;T.Session.ProtocolCount=1;T.Session.Resume=Ticket;T.Session.ResumeOnly=Round && Mode!=2;
        xstrview Wrong=XRT_STR_LITERAL("wrong");if(Round && Mode==3)T.Session.ServerName=T.Session.VerifyName=XRT_STR_LITERAL("other.invalid");if(Round && Mode==4)T.Session.Protocols=&Wrong;
        if(Round && Mode==5){xtlsresume* Expired=resumeExpired(Ticket);xrtTlsResumeRelease(Ticket);Ticket=Expired;T.Session.Resume=Ticket;}
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=Version==5?XMQTT_VERSION_5:XMQTT_VERSION_311;xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("tls-resume-interop");xmqttclient* C=(xmqttclient*)(uintptr_t)1;xnetaddr Address;xrtNetAddrLoopback(&Address,XNET_FAMILY_IPV4,(uint16)Port);bool OK;
        if(!Factory)OK=xmqttClientTlsConnect(E,&Address,1,&K,&T,&Q,&C);
        else if(Factory==1)OK=xmqttClientTlsDial(E,Resolver,XRT_STR_LITERAL("localhost"),(uint16)Port,1,&K,&T,&Q,&C);
        else {xnetaddrlist* L=xrtNetAddrListCreate(&Address,1);resumeRequire(L!=NULL,"prepared endpoints");OK=xmqttClientTlsDialAddresses(E,L,1,&K,&T,&Q,&C);xrtNetAddrListDestroy(L);}
        xrtTlsResumeRelease(Ticket);Ticket=NULL;
        if(Round && Mode>=3){resumeRequire(!OK && C==(xmqttclient*)(uintptr_t)1 && xrtGetError(),"binding/expiry refusal preserves output before network");xrtClearError();break;}
        resumeRequire(OK,"factory admission");xfuture* Ready=xmqttClientReady(C);const xmqttreadyresult* R=resumeWait(Ready);
        if(Round && Mode==1)resumeRequire(R->Status==XMQTT_STATUS_TRANSPORT && R->Cause,"ResumeOnly refuses unaccepted ticket");
        else {resumeRequire(R->Status==XMQTT_STATUS_OK,"new MQTT readiness");xmqttclienttlsresumeinfo I;resumeRequire(xmqttClientTlsResumeInfo(C,&I) && I.Established && I.Resumed==(Round && Mode==0),"authenticated full/resumed observation");
            if(!Round){double End=xrtTimer()+10;while(!xmqttClientTlsTakeResume(C,&Ticket)){resumeRequire(xrtTimer()<End,"ticket handoff");xrtSleep(1);}}
            xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("resume/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;resumeRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)resumeWait(F))->Status==XMQTT_STATUS_OK,"fresh QoS1");xrtFutureDestroy(F);F=NULL;
            resumeRequire(xmqttClientDisconnectAsync(C,NULL,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)resumeWait(F))->Status==XMQTT_STATUS_OK,"authenticated shutdown");xrtFutureDestroy(F);
        }
        resumeRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,10000)==XWAIT_OK && xmqttClientDestroy(C),"actual native/client retirement");xrtFutureDestroy(Ready);
    }
    xrtTlsResumeRelease(Ticket);xrtTlsVerifierRelease(V);resumeRequire(xrtNetResolverDestroy(Resolver) && xrtNetEngineDestroy(E),"actual join");puts("[PASS] independent verified TLS resume/fallback/binding case");return 0;
}
