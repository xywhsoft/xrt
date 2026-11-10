/* Independent broker consumer. Uses public headers and API only. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static xnetaddr Address;
static void check(bool OK,const char* What)
{if(!OK){fprintf(stderr,"[FAIL] managed subscription broker: %s error=%s\n",What,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static xmqttclientstatus factory(xnetengine* E,uint64 A,const xmqttclientconfig* K,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,ptr Data,xmqttclient** C)
{(void)Data;return xmqttClientConnectSession(E,&Address,A,K,S,I,Q,C)?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;}
static xmqttsubscriptionsinfo online(xmqttreconnect* R,uint64 Connections)
{
    double End=xrtTimer()+15;xmqttreconnectinfo I;xmqttsubscriptionsinfo S;
    do {check(xmqttReconnectInfo(R,&I) && I.State<XMQTT_RECONNECT_STOPPED && xrtTimer()<End,"bounded ONLINE recovery");check(xmqttReconnectSubscriptionsInfo(R,&S),"registry snapshot");if(I.Connections>=Connections && I.State==XMQTT_RECONNECT_ONLINE && S.Synchronized)return S;xrtSleep(1);}while(true);
}
static const unsigned char Payload[]={0,255,'i','n','t','e','n','t',0};
static void publish(xmqttreconnect* R,xstrview Topic,xfuture** Future)
{
    xmqttclientlease L={0};check(xmqttReconnectAcquire(R,&L),"publication lease");xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=2;P.Body.Publish.Topic=Topic;P.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    check(xmqttClientPublishAsync(L.Client,&P,10000,Future)==XMQTT_ADMISSION_ACCEPTED && xmqttReconnectRelease(&L) && xrtFutureWaitFor(*Future,15000)==XWAIT_OK && xrtFutureState(*Future)==XFUTURE_RESOLVED && ((const xmqttwriteresult*)xrtFutureValue(*Future))->Status==XMQTT_STATUS_OK,"ordinary immutable QoS1 result");
}
static xmqttmessage* receive(xmqttreconnect* R,xstrview Topic,xmqttversion Version,uint32 Id)
{
    xmqttmessage* M=NULL;double End=xrtTimer()+10;while(!xmqttReconnectTakeMessage(R,&M)){check(xrtTimer()<End,"broker subscription routing");xrtSleep(1);}
    const xmqttpacket* P=xmqttMessagePacket(M);check(P && P->Body.Publish.Topic.Size==Topic.Size && !memcmp(P->Body.Publish.Topic.Data,Topic.Data,Topic.Size) && P->Body.Publish.Payload.Size==sizeof(Payload) && !memcmp(P->Body.Publish.Payload.Data,Payload,sizeof(Payload)),"routed binary topic/payload");
    if(Version==XMQTT_VERSION_5){bool Found=false;for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_SUBSCRIPTION_ID && P->Properties.Data[I].Value.Number==Id)Found=true;check(Found,"original/replaced subscription identifier");}return M;
}
static void closeGeneration(xmqttreconnect* R,xmqttclientlease* L)
{
    check(xmqttReconnectAcquire(R,L),"deliberate old-generation lease");xfuture* F=NULL;
    check(xmqttClientDisconnectAsync(L->Client,NULL,5000,&F)==XMQTT_ADMISSION_ACCEPTED && xrtFutureWaitFor(F,10000)==XWAIT_OK && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"graceful old-generation drain");xrtFutureDestroy(F);
    check(xmqttClientClose(L->Client) && xmqttClientWaitFor(L->Client,10000)==XWAIT_OK,"deliberate old-Client close under retained lease");
}
int main(int Argc,char** Argv)
{
    check(Argc==7,"host port version persistent fragmented cut arguments");xmqttversion Version=atoi(Argv[3])==5?XMQTT_VERSION_5:XMQTT_VERSION_311;bool Persist=atoi(Argv[4])!=0,Fragment=atoi(Argv[5])!=0,Cut=atoi(Argv[6])!=0;
    check(xrtNetAddrParse(&Address,Argv[1],(uint16)strtoul(Argv[2],NULL,10)),"numeric endpoint");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);check(Engine && xrtNetEngineStart(Engine),"Engine");xtaskpoolconfig T={1,8,0};xtaskpool* Pool=xrtTaskPoolCreate(&T);check(Pool!=NULL,"cleanup Pool");
    xmqttreconnectconfig K;check(xmqttReconnectConfigInit(&K),"controller config");K.Client.Version=Version;K.Factory=factory;K.InitialDelay=5;K.MaxDelay=20;K.JitterPermille=0;K.RecoveryTimeout=10000;K.CleanStartFirstOnly=Persist;K.MaxAttempts=6;K.RetryStatuses|=(UINT32_C(1)<<XMQTT_STATUS_CLOSED)|(UINT32_C(1)<<XMQTT_STATUS_OK);
    if(Fragment){K.Client.ReadBytes=37;K.Client.WriteBytes=41;K.Client.WriteChunkBytes=13;K.Client.WorkPackets=1;}
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.ClientId=XRT_STR_LITERAL("xrt-intent-broker");Connect.Body.Connect.CleanStart=Version!=XMQTT_VERSION_311 || !Persist;
    xmqttproperty Expiry={XMQTT_PROPERTY_SESSION_EXPIRY,XMQTT_PROPERTY_UINT32,{.Number=600}};if(Version==XMQTT_VERSION_5)Connect.Properties=(xmqttpropertylist){&Expiry,1};
    xmqttsubscriptionsconfig S;check(xmqttSubscriptionsConfigInit(&S),"subscription config");S.MaxFilters=8;S.MaxBatch=1;S.OperationTimeout=5000;
    xmqttsubscription Topics[]={{XRT_STR_LITERAL("intent/a/+"),Version==XMQTT_VERSION_5?0x21:1},{XRT_STR_LITERAL("intent/b/+"),2}};
    xmqttproperty Props[2]={{XMQTT_PROPERTY_SUBSCRIPTION_ID,XMQTT_PROPERTY_VARIABLE,{.Number=77}},{XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{.Pair={XRT_STR_LITERAL("source"),XRT_STR_LITERAL("snapshot")}}}};
    xmqttpacket Initial;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Initial);Initial.Body.Topics=(xmqttsubscriptionlist){Topics,2};if(Version==XMQTT_VERSION_5)Initial.Properties=(xmqttpropertylist){Props,2};
    xmqttreconnect* R=NULL;check(xmqttReconnectCreateSubscribed(Engine,Pool,1,&K,&S,&Connect,&Initial,1,&R),"controller with copied intents");uint64 First=Cut?2:1;xmqttsubscriptionsinfo Info=online(R,First);check(Info.Requests==(Cut?3u:2u) && Info.Desired==2,"initial ACK-loss reconciliation count");
    xfuture* Futures[5]={0};xmqttmessage* Messages[4]={0};publish(R,XRT_STR_LITERAL("intent/a/one"),Futures);Messages[0]=receive(R,XRT_STR_LITERAL("intent/a/one"),Version,77);
    xmqttclientlease L={0};closeGeneration(R,&L);check(xmqttReconnectRelease(&L),"release known old session");Info=online(R,First+1);uint64 Expected=(Cut?3u:2u)+(Persist?0:2);check(Info.Requests==Expected,"known resumed subscriptions are not repeated; lost sessions are rebuilt");
    publish(R,XRT_STR_LITERAL("intent/a/two"),Futures+1);Messages[1]=receive(R,XRT_STR_LITERAL("intent/a/two"),Version,77);
    closeGeneration(R,&L);xmqttsubscription Removed={XRT_STR_LITERAL("intent/a/+"),0};xmqttpacket Remove;xmqttPacketInit(XMQTT_PACKET_UNSUBSCRIBE,&Remove);Remove.Body.Topics=(xmqttsubscriptionlist){&Removed,1};
    uint64 Revision=0;check(xmqttReconnectUnsubscribeIntent(R,&Remove,&Revision)==XMQTT_ADMISSION_ACCEPTED,"offline removal");
    xmqttsubscription Added={XRT_STR_LITERAL("intent/c/+"),1};xmqttpacket Add;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Add);Add.Body.Topics=(xmqttsubscriptionlist){&Added,1};Props[0].Value.Number=99;if(Version==XMQTT_VERSION_5)Add.Properties=(xmqttpropertylist){Props,2};
    check(xmqttReconnectSubscribeIntent(R,&Add,&Revision)==XMQTT_ADMISSION_ACCEPTED && xmqttReconnectRelease(&L),"offline new group and old lease release");Info=online(R,First+2);check(Info.Requests==Expected+2 && Info.Filters==2 && Info.SettledRevision>=Revision,"offline tombstone and replacement reconciled");
    xmqttsubscriptionintentinfo Filter;check(!xmqttReconnectSubscriptionInfo(R,Removed.Filter,&Filter),"removed filter reclaimed");
    publish(R,XRT_STR_LITERAL("intent/a/removed"),Futures+2);publish(R,XRT_STR_LITERAL("intent/c/three"),Futures+3);Messages[2]=receive(R,XRT_STR_LITERAL("intent/c/three"),Version,99);
    publish(R,XRT_STR_LITERAL("intent/b/four"),Futures+4);Messages[3]=receive(R,XRT_STR_LITERAL("intent/b/four"),Version,77);xrtSleep(50);xmqttmessage* Extra=NULL;check(!xmqttReconnectTakeMessage(R,&Extra),"no extra removed-filter message in bounded observation");
    check(xmqttReconnectClose(R) && xmqttReconnectWaitFor(R,15000)==XWAIT_OK && xmqttReconnectDestroy(R),"controller/Client/native/watch physical retirement");check(xrtTaskPoolClose(Pool) && xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK && xrtTaskPoolDestroy(Pool),"cleanup Pool retirement");check(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"Engine retirement");
    for(size_t I=0;I<5;++I){check(((const xmqttwriteresult*)xrtFutureValue(Futures[I]))->Status==XMQTT_STATUS_OK,"original Future beyond all owners");xrtFutureDestroy(Futures[I]);}
    for(size_t I=0;I<4;++I){check(xmqttMessagePacket(Messages[I])->Body.Publish.Payload.Size==sizeof(Payload),"message beyond all owners");xmqttMessageRelease(Messages[I]);}
    printf("{\"passed\":true,\"version\":%u,\"persistent\":%u,\"fragmented\":%u,\"cut\":%u,\"connections\":%llu,\"requests\":%llu}\n",Version==XMQTT_VERSION_5?5u:311u,Persist?1u:0u,Fragment?1u:0u,Cut?1u:0u,(unsigned long long)(First+2),(unsigned long long)Info.Requests);return 0;
}
