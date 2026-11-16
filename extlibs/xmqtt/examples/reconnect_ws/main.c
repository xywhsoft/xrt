#include "endpoint.h"
#include <stdlib.h>
static bool managedWsOutcome(xfuture* F)
{return F&&xrtFutureWaitFor(F,15000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED;}
static bool managedWsOnline(xmqttreconnect* R,uint64 Count,xmqttclientlease* L)
{double End=xrtTimer()+15;for(;;){xmqttreconnectinfo I;if(!xmqttReconnectInfo(R,&I)||I.State>=XMQTT_RECONNECT_STOPPED||xrtTimer()>=End)return false;if(I.State==XMQTT_RECONNECT_ONLINE&&I.Connections==Count&&xmqttReconnectAcquire(R,L))return true;xrtSleep(1);}}
int main(int Count,char** Args)
{
    if(Count==1){puts("Usage: xmqtt-reconnect-ws URI [5|311] [SERVER_CA.pem]");return 0;}if(Count<2||Count>4)return 2;xmqttversion Version=Count>2&&!strcmp(Args[2],"311")?XMQTT_VERSION_311:XMQTT_VERSION_5;if(Count>2&&strcmp(Args[2],"5")&&strcmp(Args[2],"311"))return 2;
    int Code=1;bool Joined=true;mqttWsEndpoint Endpoint={0};xnetengine* Engine=NULL;xtaskpool* Pool=NULL;xmqttreconnect* R=NULL;xmqttclientlease Lease={0};xfuture* Results[5]={0};
    if(!mqttWsEndpointInit(&Endpoint,(xstrview){Args[1],strlen(Args[1])},Count==4?Args[3]:NULL))goto Done;
    xnetengineconfig N;xrtNetEngineConfigInit(&N);N.Workers=2;Engine=xrtNetEngineCreate(&N);if(!Engine||!xrtNetEngineStart(Engine))goto Done;xtaskpoolconfig P={1,8,0};Pool=xrtTaskPoolCreate(&P);if(!Pool)goto Done;
    xmqttreconnectconfig K;xmqttReconnectConfigInit(&K);K.Client.Version=Version;K.Factory=mqttWsEndpointFactory;K.FactoryData=&Endpoint;K.InitialDelay=5;K.MaxDelay=20;K.JitterPermille=0;K.RetryStatuses|=UINT32_C(1)<<XMQTT_STATUS_CLOSED;
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("xrt-ws-reconnect-example");Q.Body.Connect.CleanStart=false;xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;if(Version==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&Expiry,1};if(!xmqttReconnectCreate(Engine,Pool,1,&K,&Q,&R))goto Done;
    for(unsigned Round=0;Round<2;++Round){if(!managedWsOnline(R,Round+1,&Lease))goto Done;Results[Round*2]=xmqttClientReady(Lease.Client);if(!managedWsOutcome(Results[Round*2]))goto Done;const xmqttreadyresult* Ready=xrtFutureValue(Results[Round*2]);if(Ready->Status!=XMQTT_STATUS_OK||(Round&&!Ready->Negotiated.SessionPresent))goto Done;
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/ws");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");if(xmqttClientPublishAsync(Lease.Client,&Q,10000,&Results[Round*2+1])!=XMQTT_ADMISSION_ACCEPTED||!managedWsOutcome(Results[Round*2+1])||((const xmqttwriteresult*)xrtFutureValue(Results[Round*2+1]))->Status!=XMQTT_STATUS_OK)goto Done;
        printf("[DATA] automatic generation%u HTTP/mqtt,CONNACK,PUBACK and session continuity\n",Round+1);
        if(!Round){if(!xmqttClientClose(Lease.Client)||xmqttClientWaitFor(Lease.Client,15000)!=XWAIT_OK||!xmqttReconnectRelease(&Lease))goto Done;}
    }
    if(xmqttClientDisconnectAsync(Lease.Client,NULL,10000,&Results[4])!=XMQTT_ADMISSION_ACCEPTED||!managedWsOutcome(Results[4])||((const xmqttwriteresult*)xrtFutureValue(Results[4]))->Status!=XMQTT_STATUS_OK)goto Done;
    if(!xmqttReconnectRelease(&Lease)){goto Done;}Code=0;
Done:
    if(Lease.Owner&&!xmqttReconnectRelease(&Lease)){Joined=false;Code=1;}
    if(R&&(!xmqttReconnectClose(R)||xmqttReconnectWaitFor(R,15000)!=XWAIT_OK||!xmqttReconnectDestroy(R))){Joined=false;Code=1;}
    if(!mqttWsEndpointClear(&Endpoint)){Joined=false;Code=1;}
    if(Pool&&(!xrtTaskPoolClose(Pool)||xrtTaskPoolWaitFor(Pool,15000)!=XWAIT_OK||!xrtTaskPoolDestroy(Pool))){Joined=false;Code=1;}
    if(Engine&&!xrtNetEngineDestroy(Engine)){Joined=false;Code=1;}
    if(Joined&&R){puts("[JOIN] controller, generations, Resolver, Pool and Engine physically retired");}if(Code&&xrtGetError()){fprintf(stderr,"%s\n",xrtErrorMessage(xrtGetError()));}for(unsigned I=0;I<5;++I)xrtFutureDestroy(Results[I]);return Code;
}
