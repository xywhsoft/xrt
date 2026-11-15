#include "support/ws_peer.h"
static void wsReady(xmqttclient* C)
{xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"fault control readiness");xrtFutureDestroy(F);}
static void wsFailures(bool TLS,xnetportbackend B)
{
    for(unsigned Mode=0;Mode<3;++Mode){mqttWsPeer P;mqttWsInit(&P,TLS,XMQTT_VERSION_5,B);P.Validate=true;P.Text=Mode==0;P.Silent=Mode==1;P.HoldAlias=Mode==2;P.ClientConfig.SetupTimeout=Mode==1?40:5000;
        xmqttclient* C=mqttWsStart(&P);xfuture* F=xmqttClientReady(C);const xmqttreadyresult* R=mqttWsFuture(F);
        testRequire(R->Status==(Mode==0?XMQTT_STATUS_PROTOCOL:Mode==1?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_OK),"Text rejection / setup deadline / healthy alias control");
        if(P.HoldAlias){testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,30)==XWAIT_TIMEOUT,"logical close cannot bypass held native alias");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
            if(TLS)xrtTlsStreamDestroy(P.Alias);else
#endif
            {xrtNetStreamDestroy(P.Alias);}P.Alias=NULL;}
        mqttWsCloseClient(C);mqttWsStop(&P);testRequire(R->Status==(Mode==0?XMQTT_STATUS_PROTOCOL:Mode==1?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_OK),"Ready result survives all transport roots");xrtFutureDestroy(F);
    }
    bool Failed=false,Passed=false;
    for(unsigned I=0;I<19&&!Passed;++I){mqttWsPeer P;mqttWsInit(&P,TLS,XMQTT_VERSION_5,B);P.UseBudget=true;P.AllocationLimit=(size_t)1<<(I+1);xmqttclient* C=mqttWsStart(&P);wsReady(C);Passed=P.BudgetPassed;Failed|=!Passed;mqttWsCloseClient(C);mqttWsStop(&P);}
    testRequire(Failed&&Passed,"real failed allocation controls followed by successful constructor");
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static void wsAuthDecide(xmqttclient* C,xfuture* F)
{
    double End=xrtTimer()+10;while(!xrtFutureDone(F)){xmqttauthticket* T=NULL;if(xmqttClientTakeAuth(C,&T)){
        if(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE){xmqttproperty Props[2]={{0}};Props[0].Id=XMQTT_PROPERTY_AUTH_METHOD;Props[0].Type=XMQTT_PROPERTY_STRING;Props[0].Value.Text=XRT_STR_LITERAL("m");Props[1].Id=XMQTT_PROPERTY_AUTH_DATA;Props[1].Type=XMQTT_PROPERTY_BINARY;Props[1].Value.Binary=XRT_BYTES_LITERAL("D");xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x18;Q.Properties=(xmqttpropertylist){Props,2};testRequire(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ACCEPTED,"WS continuation");}
        else {testRequire(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"WS final proof gate");}xmqttAuthRelease(T);
    }else{testRequire(xrtTimer()<End,"bounded WS auth decision");xrtSleep(1);}}
}
static void wsAuth(bool TLS,xnetportbackend B)
{
    mqttWsPeer P;mqttWsInit(&P,TLS,XMQTT_VERSION_5,B);P.Auth=true;xmqttauthconfig A;xmqttAuthConfigInit(&A);P.WsConfig.Auth=&A;xmqttproperty Method={0};Method.Id=XMQTT_PROPERTY_AUTH_METHOD;Method.Type=XMQTT_PROPERTY_STRING;Method.Value.Text=XRT_STR_LITERAL("m");P.Connect.Properties=(xmqttpropertylist){&Method,1};xmqttclient* C=mqttWsStart(&P);xfuture* F=xmqttClientReady(C);wsAuthDecide(C,F);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"WS AUTH/CONNACK local verification");xrtFutureDestroy(F);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x19;Q.Properties=(xmqttpropertylist){&Method,1};testRequire(xmqttClientReauthenticateAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"WS reauth");wsAuthDecide(C,F);testRequire(((const xmqttauthresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"WS reauth final proof");mqttWsCloseClient(C);mqttWsStop(&P);xrtFutureDestroy(F);
}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
static void wsSession(bool TLS,xmqttversion V,xnetportbackend B)
{
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.Version=V;K.ClientId=XRT_STR_LITERAL("ws-fixture");xmqttsession* S=NULL;xmqttclientinbox* I=NULL;xmqttclientinboxconfig IK;xmqttClientInboxConfigInit(&IK);IK.Version=V;testRequire(xmqttSessionCreate(&K,&S)&&xmqttClientInboxCreate(&IK,&I),"WS passive protocol state");
    xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;xfuture* Old=NULL;
    for(unsigned Round=0;Round<2;++Round){mqttWsPeer P;mqttWsInit(&P,TLS,V,B);P.HoldAck=Round==0;P.SessionPresent=Round!=0;P.WsConfig.Session=S;P.WsConfig.Inbox=I;P.Connect.Body.Connect.CleanStart=false;if(V==XMQTT_VERSION_5)P.Connect.Properties=(xmqttpropertylist){&Expiry,1};xmqttclient* C=mqttWsStart(&P);wsReady(C);
        if(!Round){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=23;Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");testRequire(xmqttClientPublishAsync(C,&Q,10000,&Old)==XMQTT_ADMISSION_ACCEPTED,"WS Session publication");}
        mqttWsWait(&P.Publishes,1,"WS original or canonical replay publication");xmqttsessioninfo Info;double End=xrtTimer()+5;
        do{testRequire(xmqttSessionInfo(S,&Info),"WS Session info");if(Round&&Info.ActiveOutgoing){testRequire(xrtTimer()<End,"WS replay acknowledgement");xrtSleep(1);}}while(Round&&Info.ActiveOutgoing);
        testRequire(Round?Info.ActiveOutgoing==0:Info.ActiveOutgoing==1,"WS state preserved then replayed");mqttWsCloseClient(C);mqttWsStop(&P);
        if(!Round)testRequire(((const xmqttwriteresult*)mqttWsFuture(Old))->Status==XMQTT_STATUS_CLOSED,"old connection Future settles separately from passive replay");
    }
    testRequire(xmqttClientInboxDestroy(I)&&xmqttSessionDestroy(S),"WS passive state real lease release");xrtFutureDestroy(Old);
}
#endif
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(unsigned TLS=0;TLS<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
        2
#else
        1
#endif
        ;++TLS){printf("[faults] TLS%u\n",TLS);wsFailures(TLS!=0,B);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        wsAuth(TLS!=0,B);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        wsSession(TLS!=0,XMQTT_VERSION_5,B);wsSession(TLS!=0,XMQTT_VERSION_311,B);
#endif
    }puts("[PASS] WS/WSS policy, Text, timeout, native alias, constructor OOM and selected Auth/Session");return 0;
}
