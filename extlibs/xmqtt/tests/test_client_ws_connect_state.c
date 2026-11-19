#include "support/ws_connect_peer.h"
static xmqttclientwsconnectconfig wsStatePolicy(void)
{xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.Host=XRT_STR_LITERAL("example.com");W.Target=XRT_STR_LITERAL("/mqtt?x=%00");return W;}
static void wsConnectInvalid(xnetportbackend B)
{
    mqttWsConnectPeer P;mqttWsConnectInit(&P,false,XMQTT_VERSION_5,B);xmqttclientwsconnectconfig Base=wsStatePolicy();
    for(unsigned Mode=0;Mode<13;++Mode){xmqttclientwsconnectconfig W=Base;xmqttclient* C=(xmqttclient*)(uintptr_t)1;xhttpfield F[2]={{XRT_STR_LITERAL("Authorization"),XRT_STR_LITERAL("Bearer test")},{XRT_STR_LITERAL("Authorization"),XRT_STR_LITERAL("Bearer test2")}};
        if(Mode==0)W.Host=(xstrview){0};else if(Mode==1)W.Target=XRT_STR_LITERAL("relative");else if(Mode==2)W.Origin=XRT_STR_LITERAL("x\r\nInjected: y");else if(Mode==3){F[0].Name=XRT_STR_LITERAL("hOsT");W.Fields=F;W.FieldCount=1;}else if(Mode==4)W.MaxWriteBytes=1;else if(Mode==5)W.Head.MaxHead=0;else if(Mode==6)W.Head.MaxFields=129;else if(Mode==7){W.Fields=F;W.FieldCount=2;}else if(Mode==8)W.WebSocket.Stream.Role=XWS_ROLE_SERVER;else if(Mode==9)W.WebSocket.MaxStagedBytes=0;else if(Mode==10)W.MaxInformationalResponses=17;else if(Mode==11)W.Size=0;
        if(Mode==12){xmqttclientwsconnectconfig Before=W;testRequire(!xmqttClientWsConnect(P.Base.Engine,&P.Base.Address,1,NULL,&W,&P.Base.Connect,(xmqttclient**)&W.Host.Data)&&!memcmp(&W,&Before,sizeof(W)),"overlapping output cannot mutate config");}
        else {testRequire(!xmqttClientWsConnect(P.Base.Engine,&P.Base.Address,1,NULL,&W,&P.Base.Connect,&C)&&C==(xmqttclient*)(uintptr_t)1,"invalid policy preserves output with real Engine/address");}xrtClearError();
    }
#if !defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    {xmqttclientwsconnectconfig W=Base;W.WebSocket.Auth=(struct xmqttauthconfig*)(uintptr_t)1;xmqttclient* C=(xmqttclient*)(uintptr_t)1;testRequire(!xmqttClientWsConnect(P.Base.Engine,&P.Base.Address,1,NULL,&W,&P.Base.Connect,&C)&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED&&C==(xmqttclient*)(uintptr_t)1,"feature-excluded Auth refuses without dereferencing opaque descriptor");xrtClearError();}
#endif
    xmqttclient* C=wsConnectStart(&P,0,NULL,&Base);xfuture* Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"same valid Engine/policy succeeds after refusals");xmqttclientwsconnectinfo I={.Size=0,.Status=77},Before=I;testRequire(!xmqttClientWsConnectInfo(C,&I)&&!memcmp(&I,&Before,sizeof(I)),"bad info Size is atomic");mqttWsCloseClient(C);mqttWsConnectStop(&P);xrtFutureDestroy(Ready);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static void wsConnectAuthDecide(xmqttclient* C,xfuture* F)
{
    double End=xrtTimer()+10;while(!xrtFutureDone(F)){xmqttauthticket* T=NULL;if(xmqttClientTakeAuth(C,&T)){
        if(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE){xmqttproperty P[2]={{0}};P[0].Id=XMQTT_PROPERTY_AUTH_METHOD;P[0].Type=XMQTT_PROPERTY_STRING;P[0].Value.Text=XRT_STR_LITERAL("m");P[1].Id=XMQTT_PROPERTY_AUTH_DATA;P[1].Type=XMQTT_PROPERTY_BINARY;P[1].Value.Binary=XRT_BYTES_LITERAL("D");xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x18;Q.Properties=(xmqttpropertylist){P,2};testRequire(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ACCEPTED,"HTTP-established MQTT AUTH continuation");}
        else {testRequire(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"HTTP-established local final proof");}xmqttAuthRelease(T);
    }else{testRequire(xrtTimer()<End,"bounded HTTP-established AUTH exchange");xrtSleep(1);}}
}
static void wsConnectAuth(bool TLS,xnetportbackend B)
{
    mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);P.Base.Auth=true;xmqttclientwsconnectconfig W=wsStatePolicy();xmqttauthconfig A;xmqttAuthConfigInit(&A);W.WebSocket.Auth=&A;xmqttproperty Method={0};Method.Id=XMQTT_PROPERTY_AUTH_METHOD;Method.Type=XMQTT_PROPERTY_STRING;Method.Value.Text=XRT_STR_LITERAL("m");P.Base.Connect.Properties=(xmqttpropertylist){&Method,1};xmqttclient* C=wsConnectStart(&P,1,NULL,&W);xfuture* F=xmqttClientReady(C);wsConnectAuthDecide(C,F);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"HTTP/TLS plus MQTT initial proof gates");xrtFutureDestroy(F);F=NULL;xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x19;Q.Properties=(xmqttpropertylist){&Method,1};testRequire(xmqttClientReauthenticateAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"HTTP-established reauthentication");wsConnectAuthDecide(C,F);testRequire(((const xmqttauthresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"HTTP-established reauth proof");mqttWsCloseClient(C);mqttWsConnectStop(&P);xrtFutureDestroy(F);
}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
static void wsConnectSession(bool TLS,xmqttversion V,xnetportbackend B)
{
    xmqttsessionconfig SK;xmqttSessionConfigInit(&SK);SK.Version=V;SK.ClientId=XRT_STR_LITERAL("ws-fixture");xmqttsession* S=NULL;testRequire(xmqttSessionCreate(&SK,&S),"passive HTTP WS Session");xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;xfuture* Old=NULL;
    for(unsigned Round=0;Round<2;++Round){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,V,B);P.Base.HoldAck=!Round;P.Base.SessionPresent=Round!=0;P.Base.Connect.Body.Connect.CleanStart=false;if(V==XMQTT_VERSION_5)P.Base.Connect.Properties=(xmqttpropertylist){&Expiry,1};xmqttclientwsconnectconfig W=wsStatePolicy();W.WebSocket.Session=S;xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=V;xmqttclient* C=wsConnectStart(&P,Round?2:0,&K,&W);xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"HTTP-established new or resumed Session");
        if(!Round){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Identifier=23;Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");testRequire(xmqttClientPublishAsync(C,&Q,5000,&Old)==XMQTT_ADMISSION_ACCEPTED,"HTTP Session unacknowledged QoS1");}
        mqttWsWait(&P.Base.Publishes,1,"HTTP Session original/canonical replay");if(Round){double End=xrtTimer()+5;for(;;){xmqttsessioninfo I;xmqttSessionInfo(S,&I);if(!I.ActiveOutgoing)break;testRequire(xrtTimer()<End,"HTTP Session replay acknowledgement");xrtSleep(1);}}
        mqttWsCloseClient(C);mqttWsConnectStop(&P);xrtFutureDestroy(F);if(!Round)testRequire(((const xmqttwriteresult*)mqttWsFuture(Old))->Status==XMQTT_STATUS_CLOSED,"old Session Future keeps original connection outcome");
    }testRequire(xmqttSessionDestroy(S),"HTTP Session claim fully released");xrtFutureDestroy(Old);
}
#endif
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;wsConnectInvalid(B);
    for(unsigned TLS=0;TLS<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        2
#else
        1
#endif
        ;++TLS){(void)TLS;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        wsConnectAuth(TLS!=0,B);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        wsConnectSession(TLS!=0,XMQTT_VERSION_5,B);wsConnectSession(TLS!=0,XMQTT_VERSION_311,B);
#endif
    }puts("[PASS] native WS HTTP policy atomicity and selected Auth/Session/Inbox recovery");return 0;
}
