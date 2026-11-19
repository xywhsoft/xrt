#include "support/ws_reconnect_peer.h"
static void wrRecovery(bool TLS,xmqttversion V,unsigned Route,unsigned Cut,xnetportbackend Backend)
{
    printf("[case] tls%u v%u route%u cut%u\n",TLS,V,Route,Cut);wrService S;wrStart(&S,TLS,V,Route,Backend);xrtAtomic32Store(&S.Cut,Cut,XMEMORY_RELEASE);xmqttreconnect* R=wrCreate(&S,10000,false);xmqttclientlease L={0};wrOnline(R,1,&L);testRequire(L.Generation==1,"first WS generation");xfuture* Ready1=xmqttClientReady(L.Client);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready1))->Status==XMQTT_STATUS_OK,"first WS Ready result");
    xmqttclientinfo Receive;double End=xrtTimer()+10;
    for(;;){
        testRequire(xmqttClientInfo(L.Client,&Receive)&&xrtTimer()<End,"first-generation receive queue ready");
        if(Receive.Messages==1)break;
        xrtSleep(1);
    }
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=Cut?4:2;Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* Old=NULL;testRequire(xmqttClientPublishAsync(L.Client,&Q,10000,&Old)==XMQTT_ADMISSION_ACCEPTED,"ordinary leased WS submission");wrWait(Cut==2?&S.Pubrels:&S.Publishes,1,"real first-generation acknowledgement cut");
    xwsstream* Server=xrtAtomicPtrLoad(&S.Connections[0].Peer.Base.Server,XMEMORY_ACQUIRE);testRequire(Server&&xrtWsStreamAbort(Server)&&xmqttClientWaitFor(L.Client,15000)==XWAIT_OK,"outage physically closes old Client under its lease");const xmqttwriteresult* Outcome=mqttWsFuture(Old);testRequire(Outcome->Status!=XMQTT_STATUS_OK&&Outcome->AcceptedBytes&&Outcome->Identifier&&Outcome->Delivery==XMQTT_MAY_HAVE_REACHED_PEER&&Outcome->PubrecReceived==(Cut==2),"old WS result is conservative and records exact ACK progress");
    xmqttmessage* Held=NULL;
    testRequire(xmqttClientTakeMessage(L.Client,&Held),"drain the retired Client while it is still leased");
    const xbytesview Payload=xmqttMessagePacket(Held)->Body.Publish.Payload;
    testRequire(Payload.Size==3&&!memcmp(Payload.Data,"x\0z",3),"old incoming payload remains exact");
    xrtSleep(25);testRequire(xrtAtomic32Load(&S.Factories,XMEMORY_ACQUIRE)==1,"no new WS factory while old lease is live");testRequire(xmqttReconnectRelease(&L),"release old WS generation");wrOnline(R,2,&L);testRequire(L.Generation==2,"automatic replacement generation");xfuture* Ready2=xmqttClientReady(L.Client);const xmqttreadyresult* New=mqttWsFuture(Ready2);testRequire(New->Status==XMQTT_STATUS_OK&&New->Negotiated.SessionPresent,"new HTTP/WS/TLS plus accepted retained Session");wrWait(Cut==2?&S.Pubrels:&S.Replays,Cut==2?2:1,"canonical automatic QoS replay");
    xmqttmessage* Extra=NULL;
    testRequire(!xmqttClientTakeMessage(L.Client,&Extra)&&!Extra,"the replacement has no old receive queue");
    wrWait(&S.Connacks,2,"both CONNECT observations published");testRequire(S.Clean[0]==(V==XMQTT_VERSION_5)&&!S.Clean[1],"MQTT5 clean-first versus persistent311 factory policy");
    testRequire(xmqttReconnectClose(R)&&xmqttReconnectWaitFor(R,0)==XWAIT_TIMEOUT,"last live lease blocks controller destruction");testRequire(xmqttReconnectRelease(&L),"last WS generation lease returned");wrStop(&S,R);testRequire(Outcome==xrtFutureValue(Old)&&Outcome->Status!=XMQTT_STATUS_OK&&((const xmqttreadyresult*)xrtFutureValue(Ready2))->Negotiated.SessionPresent&&xmqttMessagePacket(Held)->Body.Publish.Payload.Size==3,"old outcomes and message outlive controller/Pool/Engine");xmqttMessageRelease(Held);xrtFutureDestroy(Old);xrtFutureDestroy(Ready1);xrtFutureDestroy(Ready2);printf("[PASS] auto WS tls%u v%u route%u ACKcut%u leases/replay/APP delivery/physical joins\n",TLS,V,Route,Cut);
}
static void wrSubscriptions(bool TLS,xmqttversion V,bool Present,xnetportbackend B)
{
    wrService S;wrStart(&S,TLS,V,4,B);xrtAtomic32Store(&S.Present,Present,XMEMORY_RELEASE);xmqttreconnect* R=wrCreate(&S,10000,true);xmqttclientlease L={0};wrOnline(R,1,&L);xmqttsubscriptionsinfo I;testRequire(xmqttReconnectSubscriptionsInfo(R,&I)&&I.Synchronized&&I.Requests==1,"WS subscriptions settle before first ONLINE");xwsstream* W=xrtAtomicPtrLoad(&S.Connections[0].Peer.Base.Server,XMEMORY_ACQUIRE);testRequire(xrtWsStreamAbort(W)&&xmqttClientWaitFor(L.Client,15000)==XWAIT_OK&&xmqttReconnectRelease(&L),"subscription WS outage");wrOnline(R,2,&L);testRequire(xmqttReconnectSubscriptionsInfo(R,&I)&&I.Synchronized&&I.Recoveries==1&&I.Requests==(Present?1u:2u),"WS registry preserves only known retained-session state");
    xmqttsubscription Topic={XRT_STR_LITERAL("managed/#"),0};xmqttpacket Remove;xmqttPacketInit(XMQTT_PACKET_UNSUBSCRIBE,&Remove);Remove.Body.Topics=(xmqttsubscriptionlist){&Topic,1};uint64 Revision=0;testRequire(xmqttReconnectUnsubscribeIntent(R,&Remove,&Revision)==XMQTT_ADMISSION_ACCEPTED,"WS managed removal after recovery");double End=xrtTimer()+10;do{testRequire(xmqttReconnectSubscriptionsInfo(R,&I)&&xrtTimer()<End,"WS removal synchronization");if(I.Synchronized&&I.SettledRevision>=Revision)break;xrtSleep(1);}while(true);testRequire(!I.Filters&&!I.Desired&&xmqttReconnectRelease(&L),"WS removal reclaims desired filter");wrStop(&S,R);
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
        ;++TLS)for(unsigned V=0;V<2;++V){xmqttversion Version=V?XMQTT_VERSION_311:XMQTT_VERSION_5;for(unsigned Route=0;Route<6;++Route)for(unsigned Cut=0;Cut<3;++Cut)wrRecovery(TLS!=0,Version,Route,Cut,B);wrSubscriptions(TLS!=0,Version,false,B);wrSubscriptions(TLS!=0,Version,true,B);}
    puts("[PASS] automatic native WS/WSS QoS, generation/owner barriers, APP delivery and subscriptions");return 0;
}
