#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(bool OK,const char* Message)
{if(!OK){fprintf(stderr,"[FAIL] diagnostic probe: %s (%s)\n",Message,xrtErrorMessage(xrtGetError()));exit(1);}}
static const void* result(xfuture* F)
{check(F && xrtFutureWaitFor(F,5000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed result");return xrtFutureValue(F);}
typedef struct closingProbe {bool Delay,Abort,Done;} closingProbe;
static void closingNotify(xmqttclient* C,ptr User)
{
    closingProbe* P=User;xmqttclientinfo I;xmqttClientInfo(C,&I);
    if(I.State==XMQTT_CLIENT_CLOSING && !P->Done){P->Done=true;
        /* Deliberately adversarial callback stall: verify expired absolute
         * deadlines on next owner progress, not production callback guidance. */
        if(P->Delay)xrtSleep(20);
        if(P->Abort)check(xmqttClientClose(C),"reentrant abort of diagnostic drain");}
}
int main(int Argc,char** Argv)
{
    check(Argc==4,"usage: PORT VERSION CASE");const char* Case=Argv[3];unsigned Version=(unsigned)atoi(Argv[2]);
    bool Before=!strcmp(Case,"before_ready"),Explicit=!strcmp(Case,"explicit_close"),Server=!strcmp(Case,"server_close"),Partial=!strcmp(Case,"partial_publish"),PartialAck=!strcmp(Case,"partial_ack"),Timeout=!strcmp(Case,"deadline"),Native=!strcmp(Case,"native_queue"),Abort=!strcmp(Case,"abort_drain"),PeerClose=!strcmp(Case,"peer_close");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);check(Engine && xrtNetEngineStart(Engine),"Engine");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;K.InitialInputBytes=256;K.CloseTimeout=Timeout?1:Native?50:1000;closingProbe Closing={Timeout,Abort,false};if(Timeout || Abort){K.Notify=closingNotify;K.User=&Closing;}
    if(!strcmp(Case,"one_byte") || Partial || PartialAck || PeerClose){K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=K.WriteChunkBytes=1;}
    if(PartialAck)K.WorkPackets=1;
    if(Partial){K.Codec.MaxPacketBytes=K.Codec.MaxPayloadBytes=2u*1024u*1024u;}
    if(Native){K.Codec.MaxPacketBytes=K.Codec.MaxPayloadBytes=K.MaxOutgoingBytes=32u*1024u*1024u;K.Stream.WriteLimit=K.WriteBytes=K.WriteChunkBytes=32u*1024u*1024u;K.Stream.WriteHighWater=16u*1024u*1024u;K.Stream.WriteLowWater=8u*1024u*1024u;}
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("diag-probe");
    xmqttproperty Prop={0};if(!strcmp(Case,"oversize")){Prop.Id=XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE;Prop.Type=XMQTT_PROPERTY_UINT32;Prop.Value.Number=4096;Connect.Properties=(xmqttpropertylist){&Prop,1};}
    if(!strcmp(Case,"window")){Prop.Id=XMQTT_PROPERTY_RECEIVE_MAXIMUM;Prop.Type=XMQTT_PROPERTY_UINT16;Prop.Value.Number=1;Connect.Properties=(xmqttpropertylist){&Prop,1};}
    xnetaddr Address;xmqttclient* C=NULL;check(xrtNetAddrParse(&Address,"127.0.0.1",(uint16)atoi(Argv[1])) && xmqttClientConnect(Engine,&Address,1,&K,&Connect,&C),"factory");xfuture* Ready=xmqttClientReady(C);
    check(((const xmqttreadyresult*)result(Ready))->Status==(Before?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_OK),"readiness classification");xfuture* Written=NULL;const xmqttwriteresult* W=NULL;
    if(Partial || Native){size_t N=Partial?1024u*1024u:16u*1024u*1024u;unsigned char* Data=malloc(N);check(Data!=NULL,"payload");memset(Data,'p',N);xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Body.Publish.Topic=XRT_STR_LITERAL("outgoing");Pub.Body.Publish.Payload=(xbytesview){Data,N};
        check(xmqttClientPublishAsync(C,&Pub,5000,&Written)==XMQTT_ADMISSION_ACCEPTED,"publication");free(Data);W=result(Written);check((W->Status==XMQTT_STATUS_PROTOCOL || (Native && W->Status==XMQTT_STATUS_OK)) && W->Delivery==XMQTT_MAY_HAVE_REACHED_PEER && W->AcceptedBytes,"error or prior native retirement; delivery remains uncertain");
        if(Partial)check(W->AcceptedBytes<N,"partial frame fallback");
    }
    if(Explicit)check(xmqttClientClose(C),"explicit abortive Close");
    double Start=xrtTimer();check(xmqttClientWaitFor(C,5000)==XWAIT_OK,"bounded physical retirement");double WaitMs=(xrtTimer()-Start)*1000;
    xmqttclientinfo I;check(xmqttClientInfo(C,&I),"snapshot");uint8 Reason=Explicit || Server || Before || Version==4?0:!strcmp(Case,"alias")?0x94:!strcmp(Case,"oversize")?0x95:!strcmp(Case,"window")?0x93:!strcmp(Case,"unexpected")?0x82:0x81;
    bool Drain=Reason && !Partial && !PartialAck && strcmp(Case,"cap_two");
    if(I.LocalReason!=Reason || I.DrainStarted!=Drain || (!Native && I.DrainTimedOut!=Timeout))fprintf(stderr,"actual reason=%u started=%u bytes=%zu timeout=%u expected=%u/%u/%u\n",I.LocalReason,I.DrainStarted?1u:0u,I.DisconnectBytes,I.DrainTimedOut?1u:0u,Reason,Drain?1u:0u,Timeout?1u:0u);
    check(I.CloseStatus==(Explicit?XMQTT_STATUS_CLOSED:Server?XMQTT_STATUS_SERVER_DISCONNECT:XMQTT_STATUS_PROTOCOL) && I.LocalReason==Reason && I.DrainStarted==Drain && (Native || I.DrainTimedOut==Timeout),"first outcome and diagnostic state");
    check(PeerClose?(I.DisconnectBytes>=1 && I.DisconnectBytes<=3):I.DisconnectBytes==(Drain && !Timeout && !Abort?3u:0u),"native accepted diagnostic prefix");check(!I.Requests && !I.OutgoingBytes && !I.StartupBytes && I.Accepted==I.Completed,"physical request accounting");
    xmqttmessage* Saved=NULL;if(PartialAck || !strcmp(Case,"window"))check(xmqttClientTakeMessage(C,&Saved),"accepted message survives diagnostic close");
    check(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"Client/Engine destruction");
    if(Saved){check(xmqttMessagePacket(Saved)->Identifier==1,"retained receive metadata");xmqttMessageRelease(Saved);}if(W)check((W->Status==XMQTT_STATUS_PROTOCOL || (Native && W->Status==XMQTT_STATUS_OK)) && W->AcceptedBytes,"retained typed outcome");xrtFutureDestroy(Written);xrtFutureDestroy(Ready);
    printf("{\"case\":\"%s\",\"version\":%u,\"reason\":%u,\"bytes\":%zu,\"started\":%u,\"timed_out\":%u,\"physical_wait_ms\":%.3f}\n",Case,Version,Reason,I.DisconnectBytes,I.DrainStarted?1u:0u,I.DrainTimedOut?1u:0u,WaitMs);return 0;
}
