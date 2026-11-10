#include "../common/session_dial_peer.h"
static const void* result(xfuture* F)
{mqttExampleCheck(F && xrtFutureWaitFor(F,10000)==XWAIT_OK,"Dial Session result");return xrtFutureValue(F);}
int main(int Argc,char** Argv)
{
    bool Prepared=Argc==2 && !strcmp(Argv[1],"--prepared");if(Argc>2 || (Argc==2 && !Prepared))return 1;
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.ClientId=XRT_STR_LITERAL("session-dial-example");xmqttsession* State=NULL;xmqttclientinbox* Inbox=NULL;
    mqttExampleCheck(xmqttSessionCreate(&K,&State) && xmqttClientInboxCreate(NULL,&Inbox),"Dial passive owners");xfuture *Old=NULL,*Ready[2]={0};
    for(unsigned Generation=0;Generation<2;++Generation){mqttSessionDialPeer P;mqttSessionDialStart(&P,XMQTT_VERSION_5,Generation?0:MQTT_RESUME_HOLD_COMP,Generation!=0,false,false,0);
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=K.ClientId;xmqttproperty E=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);Q.Properties=(xmqttpropertylist){&E,1};xmqttclient* C=NULL;bool OK;
        if(Prepared){xnetaddr A[]={P.RefusedAddress,P.Peer.Address};xnetaddrlist* L=xrtNetAddrListCreate(A,2);mqttExampleCheck(L!=NULL,"prepared Dial list");OK=xmqttClientDialAddressesSession(P.Peer.Engine,L,0,NULL,NULL,State,Inbox,&Q,&C);xrtNetAddrListDestroy(L);}
        else {char Host[12];memcpy(Host,"mqtt.fixture",12);OK=xmqttClientDialSession(P.Peer.Engine,P.Resolver,(xstrview){Host,12},P.Peer.Address.Port,0,NULL,NULL,State,Inbox,&Q,&C);memset(Host,0,sizeof(Host));}
        mqttExampleCheck(OK,"Dial Session factory");Ready[Generation]=xmqttClientReady(C);mqttExampleCheck(((const xmqttreadyresult*)result(Ready[Generation]))->Status==XMQTT_STATUS_OK,"Dial CONNACK");
        if(!Generation){xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=4;Q.Body.Publish.Topic=XRT_STR_LITERAL("dial/example");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("publish once");mqttExampleCheck(xmqttClientPublishAsync(C,&Q,10000,&Old)==XMQTT_ADMISSION_ACCEPTED,"Dial QoS2 admission");mqttResumeWait(&P.Peer.Releases,1);}
        else {mqttResumeWait(&P.Peer.Releases,1);double End=xrtTimer()+10;xmqttsessioninfo I;do{mqttExampleCheck(xmqttSessionInfo(State,&I) && xrtTimer()<End,"Dial PUBCOMP recovery");if(I.ActiveOutgoing || I.Objects)xrtSleep(1);}while(I.ActiveOutgoing || I.Objects);mqttExampleCheck(!xrtAtomic32Load(&P.Peer.Publishes,XMEMORY_ACQUIRE),"PUBREL-only recovery");}
        mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,10000)==XWAIT_OK && xmqttClientDestroy(C),"physical Dial Client retirement");mqttSessionDialStop(&P);
    }
    mqttExampleCheck(((const xmqttwriteresult*)result(Old))->Status==XMQTT_STATUS_CLOSED,"original Future stays scoped");mqttExampleCheck(xmqttSessionDestroy(State) && xmqttClientInboxDestroy(Inbox),"passive retirement");xrtFutureDestroy(Old);xrtFutureDestroy(Ready[0]);xrtFutureDestroy(Ready[1]);puts("Dial Session recovered QoS2 across two Clients/Engines with PUBREL only");return 0;
}
