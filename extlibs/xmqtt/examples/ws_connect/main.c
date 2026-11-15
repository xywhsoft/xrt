#include <xmqtt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
#include <xrt/x509.h>
#endif
static bool wsExampleOutcome(xfuture* F)
{return F&&xrtFutureWaitFor(F,15000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED;}
int main(int Count,char** Args)
{
    if(Count==1){puts("Usage: xmqtt-ws HOST PORT [5|311] [SERVER_CA.pem]");return 0;}
    if(Count<3||Count>5)return 2;
    char* End=NULL;unsigned long Port=strtoul(Args[2],&End,10);if(!*Args[2]||*End||!Port||Port>65535)return 2;
    xmqttversion Version=Count>3&&!strcmp(Args[3],"311")?XMQTT_VERSION_311:XMQTT_VERSION_5;if(Count>3&&strcmp(Args[3],"311")&&strcmp(Args[3],"5"))return 2;
    bool Secure=Count==5,Joined=true;int Code=1;xnetengine* Engine=NULL;xnetresolver* Resolver=NULL;xmqttclient* Client=NULL;xfuture *Ready=NULL,*Sent=NULL,*Bye=NULL;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    xx509store* Store=NULL;xtlsverifier* Verify=NULL;xmqttclienttlsconfig TLS;xmqttClientTlsConfigInit(&TLS);TLS.Session.ResumeLimit=0;
    if(Secure){FILE* F=fopen(Args[4],"rb");if(!F)goto Done;unsigned char* Data=xrtMalloc(65537);if(!Data){fclose(F);goto Done;}size_t N=fread(Data,1,65537,F);bool Read=!ferror(F)&&N&&N<=65536;fclose(F);Store=xrtX509StoreCreate();bool Added=Read&&Store&&xrtX509StoreAddData(Store,(xbytesview){Data,N},NULL);xrtSecureZero(Data,N);xrtFree(Data);if(!Added)goto Done;xtlsverifierconfig K;xrtTlsVerifierConfigInit(&K);K.Store=Store;xtlsverifylimits Limits;xrtTlsVerifyLimitsInit(&Limits);Verify=xrtTlsVerifierCreateLimited(&K,&Limits);if(!Verify)goto Done;TLS.Session.Verifier=Verify;}
#else
    if(Secure){fputs("WSS requires xmqtt_client_ws_connect_tls\n",stderr);return 2;}
#endif
    xnetengineconfig Network;xrtNetEngineConfigInit(&Network);Network.Workers=2;Engine=xrtNetEngineCreate(&Network);if(!Engine||!xrtNetEngineStart(Engine))goto Done;Resolver=xrtNetResolverCreate(NULL);if(!Resolver)goto Done;
    char Authority[1200];int N=snprintf(Authority,sizeof(Authority),strchr(Args[1],':')?"[%s]:%lu":"%s:%lu",Args[1],Port);if(N<=0||(size_t)N>=sizeof(Authority))goto Done;
    xmqttclientconfig C;xmqttClientConfigInit(&C);C.Version=Version;xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.Host=(xstrview){Authority,(size_t)N};xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("xrt-websocket-example");bool Created;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(Secure)Created=xmqttClientWssDial(Engine,Resolver,(xstrview){Args[1],strlen(Args[1])},(uint16)Port,1,&C,&W,&TLS,&Q,&Client);else
#endif
    Created=xmqttClientWsDial(Engine,Resolver,(xstrview){Args[1],strlen(Args[1])},(uint16)Port,1,&C,&W,&Q,&Client);
    if(!Created)goto Done;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    xrtTlsVerifierRelease(Verify);Verify=NULL;xrtX509StoreFree(Store);Store=NULL;
#endif
    Ready=xmqttClientReady(Client);if(!wsExampleOutcome(Ready))goto Done;const xmqttreadyresult* R=xrtFutureValue(Ready);if(R->Status!=XMQTT_STATUS_OK){fprintf(stderr,"Ready status=%d cause=%s\n",R->Status,R->Cause?xrtErrorMessage(R->Cause):"none");goto Done;}
    xmqttclientwsconnectinfo Info={.Size=sizeof(Info)};if(!xmqttClientWsConnectInfo(Client,&Info)||!Info.Upgraded||Info.Status!=101||Info.Secure!=Secure)goto Done;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/ws");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");if(xmqttClientPublishAsync(Client,&Q,5000,&Sent)!=XMQTT_ADMISSION_ACCEPTED||!wsExampleOutcome(Sent)){goto Done;}
    const xmqttwriteresult* Write=xrtFutureValue(Sent);if(Write->Status!=XMQTT_STATUS_OK){fprintf(stderr,"PUBLISH status=%d reason=%u delivery=%d cause=%s\n",Write->Status,Write->Reason,Write->Delivery,Write->Cause?xrtErrorMessage(Write->Cause):"none");goto Done;}
    printf("[DATA] MQTT%s %s HTTP101/mqtt,CONNACK and PUBACK verified\n",Version==XMQTT_VERSION_311?"3.1.1":"5.0",Secure?"WSS":"WS");
    if(xmqttClientDisconnectAsync(Client,NULL,5000,&Bye)!=XMQTT_ADMISSION_ACCEPTED||!wsExampleOutcome(Bye)){goto Done;}
    const xmqttwriteresult* Closed=xrtFutureValue(Bye);if(Closed->Status!=XMQTT_STATUS_OK){fprintf(stderr,"DISCONNECT status=%d bytes=%zu delivery=%d cause=%s\n",Closed->Status,Closed->AcceptedBytes,Closed->Delivery,Closed->Cause?xrtErrorMessage(Closed->Cause):"none");goto Done;}
    printf("[PASS] MQTT%s %s HTTP101/mqtt,CONNACK,PUBACK,DISCONNECT and native shutdown\n",Version==XMQTT_VERSION_311?"3.1.1":"5.0",Secure?"WSS":"WS");Code=0;
Done:
    if(Client&&(!xmqttClientClose(Client)||xmqttClientWaitFor(Client,15000)!=XWAIT_OK||!xmqttClientDestroy(Client))){Code=1;Joined=false;}
    if(Resolver&&!xrtNetResolverDestroy(Resolver)){Code=1;Joined=false;}
    if(Engine&&!xrtNetEngineDestroy(Engine)){Code=1;Joined=false;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    xrtTlsVerifierRelease(Verify);xrtX509StoreFree(Store);
#endif
    if(Joined&&Client)puts("[JOIN] Client/Resolver/Engine physically retired");
    if(Code&&xrtGetError()){fprintf(stderr,"%s\n",xrtErrorMessage(xrtGetError()));}xrtFutureDestroy(Ready);xrtFutureDestroy(Sent);xrtFutureDestroy(Bye);return Code;
}
