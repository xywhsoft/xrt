#include "support/ws_connect_peer.h"
static void wsConnectRoundtrip(bool TLS,xmqttversion V,unsigned Route,xnetportbackend B)
{
    printf("[connect] tls%u version%u route%u\n",TLS,V,Route);mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,V,B);P.Mode=Route;P.SplitResponse=Route==1;
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=V;if(Route==1)K.ReadBytes=K.WriteBytes=K.WriteChunkBytes=K.WorkPackets=1;
    char Authority[]="example.com",Target[]="/mqtt?x=%00",Credential[]="Bearer ephemeral";xhttpfield Fields[]={{XRT_STR_LITERAL("Authorization"),(xstrview){Credential,sizeof(Credential)-1}}};xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.Host=(xstrview){Authority,sizeof(Authority)-1};W.Target=(xstrview){Target,sizeof(Target)-1};W.Fields=Fields;W.FieldCount=1;xmqttclient* C=wsConnectStart(&P,Route,&K,&W);memset(Authority,'?',sizeof(Authority));memset(Target,'?',sizeof(Target));memset(Credential,'?',sizeof(Credential));memset(Fields,0,sizeof(Fields));
    xfuture* Ready=xmqttClientReady(C);const xmqttreadyresult* Result=mqttWsFuture(Ready);if(Result->Status!=XMQTT_STATUS_OK){xmqttclientwsconnectinfo I={.Size=sizeof(I)};xmqttClientWsConnectInfo(C,&I);fprintf(stderr,"WS Ready status=%d cause=%s http=%u request=%zu response=%zu copies=%zu interim=%zu upgraded=%u\n",Result->Status,Result->Cause?xrtErrorMessage(Result->Cause):"none",I.Status,I.RequestBytes,I.ResponseBytes,I.HeadCopyBytes,I.InformationalResponses,I.Upgraded);}testRequire(Result->Status==XMQTT_STATUS_OK,"HTTP101/mqtt then MQTT CONNACK");
    xmqttclientwsconnectinfo Info={.Size=sizeof(Info)};testRequire(xmqttClientWsConnectInfo(C,&Info)&&Info.Upgraded&&Info.Secure==TLS&&Info.Status==101&&Info.RequestBytes&&Info.ResponseBytes>=Info.HeaderBytes&&Info.ColdBytes<Info.PeakColdBytes&&Info.InformationalResponses==(Route==2?2u:Route==1?1u:0u),"owned post-cleanup HTTP setup facts");
    for(unsigned Qos=0;Qos<3;++Qos){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"WS factory publication");testRequire(((const xmqttwriteresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"WS factory QoS0/1/2 actual outcome");xrtFutureDestroy(F);}
    xfuture* Bye=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,10000,&Bye)==XMQTT_ADMISSION_ACCEPTED,"HTTP established graceful disconnect");testRequire(((const xmqttwriteresult*)mqttWsFuture(Bye))->Status==XMQTT_STATUS_OK,"MQTT/WS/TLS graceful completion");mqttWsCloseClient(C);mqttWsConnectStop(&P);xrtFutureDestroy(Bye);xrtFutureDestroy(Ready);
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(unsigned TLS=0;TLS<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        2
#else
        1
#endif
        ;++TLS)for(unsigned V=0;V<2;++V)for(unsigned Route=0;Route<3;++Route)wsConnectRoundtrip(TLS!=0,V?XMQTT_VERSION_311:XMQTT_VERSION_5,Route,B);
    puts("[PASS] native numeric/DNS/prepared WS/WSS HTTP setup and ordinary MQTT client");return 0;
}
