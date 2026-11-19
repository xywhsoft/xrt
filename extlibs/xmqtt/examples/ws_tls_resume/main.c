#include "../reconnect_ws/endpoint.h"
#include <stdlib.h>
static bool wssResumeWait(xfuture* F)
{return F&&xrtFutureWaitFor(F,15000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED;}
int main(int Argc,char** Argv)
{
    if(Argc==1){puts("Usage: xmqtt-wss-resume WSS_URI SERVER_CA.pem [5|311] [strict|fallback]");return 0;}
    if(Argc<3||Argc>5||(Argc>3&&strcmp(Argv[3],"5")&&strcmp(Argv[3],"311"))||(Argc>4&&strcmp(Argv[4],"strict")&&strcmp(Argv[4],"fallback")))return 2;
    xmqttversion Version=Argc>3&&!strcmp(Argv[3],"311")?XMQTT_VERSION_311:XMQTT_VERSION_5;bool Fallback=Argc>4&&!strcmp(Argv[4],"fallback");int Code=1;bool Joined=true;mqttWsEndpoint E={0};xnetengine* Engine=NULL;xmqttclient* C=NULL;xmqttsession* Session=NULL;xtlsresume* Credential=NULL;xfuture* Results[6]={0};
    if(!mqttWsEndpointInit(&E,(xstrview){Argv[1],strlen(Argv[1])},Argv[2])||!E.Secure)goto Done;
    E.TLS.Session.ResumeLimit=4;xnetengineconfig N;xrtNetEngineConfigInit(&N);N.Workers=2;Engine=xrtNetEngineCreate(&N);if(!Engine||!xrtNetEngineStart(Engine))goto Done;
    xmqttsessionconfig SC;xmqttSessionConfigInit(&SC);SC.Version=Version;SC.ClientId=XRT_STR_LITERAL("xrt-wss-resume-example");if(!xmqttSessionCreate(&SC,&Session))goto Done;E.Policy.WebSocket.Session=Session;
    for(unsigned Round=0;Round<2;++Round){xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=Version;E.TLS.Session.Resume=Credential;E.TLS.Session.ResumeOnly=Round&&!Fallback;
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("xrt-wss-resume-example");Q.Body.Connect.CleanStart=false;xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;if(Version==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&Expiry,1};
        if(!xmqttClientWssConnectUri(Engine,E.Resolver,E.URI,1,&K,&E.Policy,&E.TLS,&Q,&C))goto Done;
        /* Admission has snapshotted the native credential before DNS. */
        xrtTlsResumeRelease(Credential);Credential=NULL;E.TLS.Session.Resume=NULL;Results[Round*3]=xmqttClientReady(C);if(!wssResumeWait(Results[Round*3]))goto Done;const xmqttreadyresult* Ready=xrtFutureValue(Results[Round*3]);xmqttclienttlsresumeinfo I;
        if(Ready->Status!=XMQTT_STATUS_OK){fprintf(stderr,"generation%u MQTT Ready status=%u\n",Round+1,(unsigned)Ready->Status);goto Done;}
        if(!xmqttClientTlsResumeInfo(C,&I)||!I.Established||(!Round&&I.Resumed)||(Round&&!Fallback&&!I.Resumed)||(Round&&!Ready->Negotiated.SessionPresent))goto Done;
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/ws-resume");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");if(xmqttClientPublishAsync(C,&Q,10000,&Results[Round*3+1])!=XMQTT_ADMISSION_ACCEPTED||!wssResumeWait(Results[Round*3+1])||((const xmqttwriteresult*)xrtFutureValue(Results[Round*3+1]))->Status!=XMQTT_STATUS_OK)goto Done;
        if(!Round){double End=xrtTimer()+10;while(!xmqttClientTlsTakeResume(C,&Credential)){if(xrtTimer()>=End)goto Done;xrtSleep(1);}}
        printf("[DATA] generation%u TLS resumed=%u MQTT SessionPresent=%u CONNACK/PUBACK\n",Round+1,I.Resumed,Ready->Negotiated.SessionPresent);
        if(xmqttClientDisconnectAsync(C,NULL,10000,&Results[Round*3+2])!=XMQTT_ADMISSION_ACCEPTED||!wssResumeWait(Results[Round*3+2])||((const xmqttwriteresult*)xrtFutureValue(Results[Round*3+2]))->Status!=XMQTT_STATUS_OK||xmqttClientWaitFor(C,15000)!=XWAIT_OK||!xmqttClientDestroy(C)){goto Done;}C=NULL;
    }
    Code=0;
Done:
    if(C&&(!xmqttClientClose(C)||xmqttClientWaitFor(C,15000)!=XWAIT_OK||!xmqttClientDestroy(C))){Joined=false;Code=1;}
    if(!Joined)return 1; /* Still-owned callback contexts cannot be freed. */
    xrtTlsResumeRelease(Credential);if(Session&&!xmqttSessionDestroy(Session)){Joined=false;Code=1;}if(!mqttWsEndpointClear(&E)){Joined=false;Code=1;}if(Engine&&!xrtNetEngineDestroy(Engine)){Joined=false;Code=1;}
    if(Joined&&Engine){puts("[JOIN] Client/WS/TLS/TCP/Resolver/Engine physically retired");}for(unsigned I=0;I<6;++I)xrtFutureDestroy(Results[I]);if(Code&&xrtGetError())fprintf(stderr,"%s\n",xrtErrorMessage(xrtGetError()));return Code;
}
