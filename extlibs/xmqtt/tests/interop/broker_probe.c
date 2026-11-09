#include <xmqtt.h>
#include "../../examples/tls/material.h"
#include <stdlib.h>
/* Public API only. Broker/client sources are not an implementation reference. */
typedef struct brokerProbe {
    xnetengine* Engine;xnetresolver* Resolver;xtlsverifier* Verifier;
    xmqttclient* Clients[8];unsigned Serial,Factory,Scenario;xmqttversion Version;bool TLS;
    uint16 Port;char Username[128],Password[256];size_t UserBytes,PasswordBytes;
} brokerProbe;
static brokerProbe Probe;
static void brokerCheck(bool OK,cstr Text)
{if(!OK){fprintf(stderr,"[FAIL] broker probe: %s cause=%s\n",Text,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static const void* brokerFuture(xfuture* F)
{brokerCheck(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"bounded typed outcome");return xrtFutureValue(F);}
static void brokerRetire(xmqttclient* C,bool Graceful)
{
    if(Graceful){xfuture* F=NULL;brokerCheck(xmqttClientDisconnectAsync(C,NULL,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)brokerFuture(F))->Status==XMQTT_STATUS_OK,"MQTT and selected-transport graceful close");xrtFutureDestroy(F);}
    brokerCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,15000)==XWAIT_OK && xmqttClientDestroy(C),"physical Client/native retirement");
    for(size_t I=0;I<8;++I)if(Probe.Clients[I]==C)Probe.Clients[I]=NULL;
}
static xmqttclient* brokerConnect(bool Will,bool Reject)
{
    unsigned Slot=Probe.Serial++;brokerCheck(Slot<8,"bounded probe clients");char Id[64];snprintf(Id,sizeof(Id),"xmqtt-%u-%u-%u",(unsigned)Probe.Version,Probe.Factory,Slot);
    if(Probe.Scenario==3)memcpy(Id,"xrt-session-guard",18);
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=Probe.Version;K.SetupTimeout=10000;
    if(Probe.Scenario==1){K.ReadBytes=37;K.WriteBytes=41;K.WriteChunkBytes=13;K.WorkPackets=4;}
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=Probe.Scenario!=3;Q.Body.Connect.KeepAlive=1;Q.Body.Connect.ClientId=(xstrview){Id,strlen(Id)};
    xmqttproperty Props[2]={{0}};if(Probe.Version==XMQTT_VERSION_5){Props[0].Id=XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM;Props[0].Type=XMQTT_PROPERTY_UINT16;Props[0].Value.Number=4;Q.Properties=(xmqttpropertylist){Props,1};
        if(Probe.Scenario==3){Props[1].Id=XMQTT_PROPERTY_SESSION_EXPIRY;Props[1].Type=XMQTT_PROPERTY_UINT32;Props[1].Value.Number=30;Q.Properties.Count=2;}}
    if(Probe.UserBytes){Q.Body.Connect.HasUsername=Q.Body.Connect.HasPassword=true;Q.Body.Connect.Username=(xstrview){Probe.Username,Probe.UserBytes};Q.Body.Connect.Password=Reject?XRT_BYTES_LITERAL("deliberately-wrong"):(xbytesview){(cbytes)Probe.Password,Probe.PasswordBytes};}
    if(Will){Q.Body.Connect.HasWill=true;Q.Body.Connect.WillQos=2;Q.Body.Connect.WillTopic=XRT_STR_LITERAL("xrt/qual/will");Q.Body.Connect.WillPayload=XRT_BYTES_LITERAL("w\0z");}
    xmqttclient* C=NULL;xnetaddr Address;xrtNetAddrLoopback(&Address,XNET_FAMILY_IPV4,Probe.Port);bool OK;
    if(Probe.TLS){xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Verifier=Probe.Verifier;T.Session.ServerName=T.Session.VerifyName=XRT_STR_LITERAL("localhost");static const xstrview Protocols[]={{"mqtt",4}};T.Session.Protocols=Protocols;T.Session.ProtocolCount=1;
        if(!Probe.Factory)OK=xmqttClientTlsConnect(Probe.Engine,&Address,1,&K,&T,&Q,&C);
        else if(Probe.Factory==1)OK=xmqttClientTlsDial(Probe.Engine,Probe.Resolver,XRT_STR_LITERAL("localhost"),Probe.Port,1,&K,&T,&Q,&C);
        else{xnetaddrlist* L=xrtNetAddrListCreate(&Address,1);brokerCheck(L!=NULL,"prepared endpoints");OK=xmqttClientTlsDialAddresses(Probe.Engine,L,1,&K,&T,&Q,&C);xrtNetAddrListDestroy(L);}
    }else if(!Probe.Factory)OK=xmqttClientConnect(Probe.Engine,&Address,1,&K,&Q,&C);
    else if(Probe.Factory==1)OK=xmqttClientDial(Probe.Engine,Probe.Resolver,XRT_STR_LITERAL("localhost"),Probe.Port,1,&K,NULL,&Q,&C);
    else{xnetaddrlist* L=xrtNetAddrListCreate(&Address,1);brokerCheck(L!=NULL,"prepared endpoints");OK=xmqttClientDialAddresses(Probe.Engine,L,1,&K,NULL,&Q,&C);xrtNetAddrListDestroy(L);}
    brokerCheck(OK,"connection factory");Probe.Clients[Slot]=C;xfuture* F=xmqttClientReady(C);const xmqttreadyresult* R=brokerFuture(F);
    if(Reject)brokerCheck(R->Status==XMQTT_STATUS_REJECTED,"broker authentication refusal");
    else if(Probe.Scenario==3)brokerCheck(R->Status==XMQTT_STATUS_SESSION,"unsupported retained broker session fails closed");
    else brokerCheck(R->Status==XMQTT_STATUS_OK && R->Negotiated.Accepted && !R->Negotiated.SessionPresent,"fresh broker CONNACK");
    xrtFutureDestroy(F);return C;
}
static void brokerSubscribe(xmqttclient* C,xstrview Topic,uint8 Options,bool Unsubscribe)
{
    xmqttsubscription S={Topic,Unsubscribe?0:Options};xmqttpacket Q;xmqttPacketInit(Unsubscribe?XMQTT_PACKET_UNSUBSCRIBE:XMQTT_PACKET_SUBSCRIBE,&Q);Q.Body.Topics=(xmqttsubscriptionlist){&S,1};xfuture* F=NULL;
    brokerCheck((Unsubscribe?xmqttClientUnsubscribeAsync(C,&Q,5000,&F):xmqttClientSubscribeAsync(C,&Q,5000,&F))==XMQTT_ADMISSION_ACCEPTED,"subscription admission");
    const xmqttsubscriptionresult* R=brokerFuture(F);brokerCheck(R->Status==XMQTT_STATUS_OK && R->Acknowledged && R->Requested==1 && R->Granted==1 && !R->Rejected,"per-filter broker subscription decision");xrtFutureDestroy(F);
}
static void brokerPublish(xmqttclient* C,xstrview Topic,xbytesview Payload,unsigned Qos,bool Retain,xmqttpropertylist Properties)
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)((Qos<<1)|(Retain?1:0));Q.Body.Publish.Topic=Topic;Q.Body.Publish.Payload=Payload;Q.Properties=Properties;xfuture* F=NULL;
    brokerCheck(xmqttClientPublishAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"binary publish admission");const xmqttwriteresult* R=brokerFuture(F);
    brokerCheck(R->Status==XMQTT_STATUS_OK && R->AcceptedBytes && (!Qos || R->Delivery==XMQTT_PEER_ACCEPTED),"broker QoS0/1/2 completion");
    if(Qos==2)brokerCheck(R->PubrecReceived && R->PubcompReceived && R->PubrecReason<0x80,"both broker QoS2 acknowledgements");
    xrtFutureDestroy(F);
}
static xmqttmessage* brokerMessage(xmqttclient* C,xstrview Topic,xbytesview Payload,unsigned Qos,int Retain)
{
    xmqttmessage* M=NULL;double End=xrtTimer()+15;while(!xmqttClientTakeMessage(C,&M)){brokerCheck(xrtTimer()<End,"bounded broker delivery");xrtSleep(1);}
    const xmqttpacket* P=xmqttMessagePacket(M);brokerCheck(P && P->Type==XMQTT_PACKET_PUBLISH && P->Body.Publish.Topic.Size==Topic.Size && !memcmp(P->Body.Publish.Topic.Data,Topic.Data,Topic.Size) && P->Body.Publish.Payload.Size==Payload.Size && (!Payload.Size || !memcmp(P->Body.Publish.Payload.Data,Payload.Data,Payload.Size)) && ((P->Flags>>1)&3u)==Qos,"exact broker-delivered topic/binary/QoS");
    if(Retain>=0)brokerCheck((P->Flags&1u)==(unsigned)Retain,"broker retained-delivery flag");
    return M;
}
int main(int Argc,char** Argv)
{
    /* PORT MQTT(5|311) TLS(0|1) factory(0..2) CA.pem|- auth-file|- scenario(0..3) */
    if(Argc!=8)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10);unsigned Version=(unsigned)strtoul(Argv[2],NULL,10);Probe.TLS=atoi(Argv[3])!=0;Probe.Factory=(unsigned)strtoul(Argv[4],NULL,10);Probe.Scenario=(unsigned)strtoul(Argv[7],NULL,10);Probe.Version=Version==5?XMQTT_VERSION_5:XMQTT_VERSION_311;Probe.Port=(uint16)Port;
    brokerCheck(Port && Port<=65535 && (Version==5 || Version==311) && Probe.Factory<3 && Probe.Scenario<=3,"probe arguments");
    if(strcmp(Argv[6],"-")){FILE* F=fopen(Argv[6],"rb");brokerCheck(F && fgets(Probe.Username,sizeof(Probe.Username),F) && fgets(Probe.Password,sizeof(Probe.Password),F),"runtime credentials");fclose(F);Probe.UserBytes=strcspn(Probe.Username,"\r\n");Probe.PasswordBytes=strcspn(Probe.Password,"\r\n");}
    if(Probe.TLS){size_t N=0;bytes CA=xmqttExampleTlsRead(Argv[5],65536,&N);xx509store* Store=xrtX509StoreCreate();brokerCheck(CA && Store && xrtX509StoreAddData(Store,(xbytesview){CA,N},NULL),"runtime broker trust");xrtSecureZero(CA,N);xrtFree(CA);xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Store=Store;xtlsverifylimits L;xrtTlsVerifyLimitsInit(&L);Probe.Verifier=xrtTlsVerifierCreateLimited(&V,&L);xrtX509StoreFree(Store);brokerCheck(Probe.Verifier!=NULL,"strict broker verifier");}
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=2;Probe.Engine=xrtNetEngineCreate(&EC);brokerCheck(Probe.Engine && xrtNetEngineStart(Probe.Engine),"host engine");Probe.Resolver=xrtNetResolverCreate(NULL);brokerCheck(Probe.Resolver!=NULL,"host resolver");xmqttmessage* Held=NULL;
    if(Probe.Scenario==2){xmqttclient* C=brokerConnect(false,true);brokerRetire(C,false);}
    else if(Probe.Scenario==3){xmqttclient* C=brokerConnect(false,false);brokerRetire(C,false);}
    else {xmqttclient *S=brokerConnect(false,false),*P=brokerConnect(false,false);xstrview Topic=XRT_STR_LITERAL("xrt/qual/data");brokerSubscribe(S,Topic,2,false);
        unsigned char Payload[65537];size_t N=Probe.Scenario==1?257:sizeof(Payload);for(size_t I=0;I<N;++I)Payload[I]=(unsigned char)(I*29u);
        for(unsigned Qos=0;Qos<3;++Qos){brokerPublish(P,Topic,(xbytesview){Payload,N},Qos,false,(xmqttpropertylist){0});xmqttmessage* M=brokerMessage(S,Topic,(xbytesview){Payload,N},Qos,0);xmqttMessageRelease(M);}
        if(Version==5){xmqttproperty A={0};A.Id=XMQTT_PROPERTY_TOPIC_ALIAS;A.Type=XMQTT_PROPERTY_UINT16;A.Value.Number=1;for(unsigned I=0;I<3;++I){brokerPublish(P,I==2?(xstrview){0}:Topic,XRT_BYTES_LITERAL("a\0z"),1,false,(xmqttpropertylist){&A,1});xmqttMessageRelease(brokerMessage(S,Topic,XRT_BYTES_LITERAL("a\0z"),1,0));}
            brokerSubscribe(S,Topic,2|4,false);brokerPublish(S,Topic,XRT_BYTES_LITERAL("self"),1,false,(xmqttpropertylist){0});brokerPublish(P,Topic,XRT_BYTES_LITERAL("after"),1,false,(xmqttpropertylist){0});xmqttMessageRelease(brokerMessage(S,Topic,XRT_BYTES_LITERAL("after"),1,0));brokerSubscribe(S,Topic,2,false);}
        brokerPublish(P,Topic,XRT_BYTES_LITERAL("r\0z"),1,true,(xmqttpropertylist){0});xmqttMessageRelease(brokerMessage(S,Topic,XRT_BYTES_LITERAL("r\0z"),1,0));xmqttclient* Late=brokerConnect(false,false);brokerSubscribe(Late,Topic,1,false);Held=brokerMessage(Late,Topic,XRT_BYTES_LITERAL("r\0z"),1,1);
        brokerPublish(P,Topic,(xbytesview){0},1,true,(xmqttpropertylist){0});xmqttMessageRelease(brokerMessage(S,Topic,(xbytesview){0},1,0));xmqttMessageRelease(brokerMessage(Late,Topic,(xbytesview){0},1,0));brokerSubscribe(Late,Topic,0,true);brokerRetire(Late,true);brokerSubscribe(S,Topic,0,true);
        xstrview Will=XRT_STR_LITERAL("xrt/qual/will");brokerSubscribe(S,Will,2,false);xmqttclient* Victim=brokerConnect(true,false);brokerRetire(Victim,false);xmqttMessageRelease(brokerMessage(S,Will,XRT_BYTES_LITERAL("w\0z"),2,0));brokerSubscribe(S,Will,0,true);
        brokerRetire(P,true);brokerRetire(S,true);
    }
    brokerCheck(xrtNetResolverDestroy(Probe.Resolver) && xrtNetEngineDestroy(Probe.Engine),"actual host join");xrtTlsVerifierRelease(Probe.Verifier);xrtSecureZero(Probe.Username,sizeof(Probe.Username));xrtSecureZero(Probe.Password,sizeof(Probe.Password));
    if(Held){const xmqttpacket* P=xmqttMessagePacket(Held);brokerCheck(P->Body.Publish.Payload.Size==3 && !memcmp(P->Body.Publish.Payload.Data,"r\0z",3),"retained metadata survives all owners and Engine");xmqttMessageRelease(Held);}
    puts(Probe.Scenario==2?"[PASS] real broker authentication refusal and physical lifetime":Probe.Scenario==3?"[PASS] real broker retained-session guard and physical lifetime":"[PASS] real broker: binary QoS0/1/2, aliases, No Local, retained, Will, subscription and physical lifetime");return 0;
}
