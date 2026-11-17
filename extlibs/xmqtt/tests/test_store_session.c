#include "support/store_session_peer.h"
static unsigned OldNotices;
static void ssNotice(const xerror* E,ptr P){if(E==P)++OldNotices;}
static bool ssWithin(xbytesview Wire,const void* Data,size_t N)
{return (const unsigned char*)Data>=Wire.Data&&(const unsigned char*)Data<=Wire.Data+Wire.Size&&N<=Wire.Size-(size_t)((const unsigned char*)Data-Wire.Data);}
static void ssPacketViews(const xmqttsessionpacket* P,xmqttversion V)
{
    const xmqttpacket* Q=xmqttSessionPacket(P);testRequire(Q&&Q->Type==XMQTT_PACKET_PUBLISH&&Q->Body.Publish.Topic.Size==11&&!memcmp(Q->Body.Publish.Topic.Data,"store/topic",11)&&Q->Body.Publish.Payload.Size==3&&!memcmp(Q->Body.Publish.Payload.Data,"x\0z",3),"owned canonical packet bytes after source/scratch destruction");
    testRequire(ssWithin(Q->Wire,Q->Body.Publish.Topic.Data,Q->Body.Publish.Topic.Size)&&ssWithin(Q->Wire,Q->Body.Publish.Payload.Data,Q->Body.Publish.Payload.Size),"topic/payload views rebased inside new owner");
    if(V==XMQTT_VERSION_5){testRequire(Q->Properties.Count==4,"exact owned property count");const xmqttproperty* R=Q->Properties.Data;
        testRequire(R[0].Value.Pair.Name.Size==0&&ssWithin(Q->Wire,R[0].Value.Pair.Name.Data,0)&&R[0].Value.Pair.Value.Size==4&&!memcmp(R[0].Value.Pair.Value.Data,"meta",4)&&ssWithin(Q->Wire,R[0].Value.Pair.Value.Data,4),"empty and nonempty pair views rebased");
        testRequire(R[1].Value.Binary.Size==3&&!memcmp(R[1].Value.Binary.Data,"a\0b",3)&&ssWithin(Q->Wire,R[1].Value.Binary.Data,3),"binary/NUL property view rebased");
        testRequire(R[2].Value.Text.Size==9&&!memcmp(R[2].Value.Text.Data,"type/utf8",9)&&ssWithin(Q->Wire,R[2].Value.Text.Data,9),"string view rebased");}
    else testRequire(!Q->Properties.Count,"311 has no properties");
}
static void ssReplay(xmqttversion V,bool Live,bool Assigned)
{
    xmqttstorestate* State=ssRecover(V,Live,60);xmqttsessionconfig K=ssSessionConfig(V);xmqttstoresessionconfig C=ssConfig(10,4000);if(Assigned)K.ClientId=(xstrview){0};xmqttsession* S=NULL;
    xrtSetErrorInfo(XERR_AGAIN,"import.ambient",1,"old caller");const xerror* Error=xrtGetError();OldNotices=0;xrtSetErrorHandler(ssNotice,(ptr)Error);
    ssStatus(Assigned?xmqttStoreSessionRestoreAssigned(State,&K,&C,&S):xmqttStoreSessionRestore(State,&K,&C,&S),XMQTT_SESSION_DONE,"direct owned Session import");testRequire(xrtGetError()==Error&&!OldNotices,"success restores caller error quietly");xrtSetErrorHandler(NULL,NULL);xrtClearError();
    xmqttsessioninfo I=ssInfo(S);testRequire(!I.Connected&&I.HasState&&I.ActiveOutgoing==3&&I.Incoming==2&&!I.Prepared&&!I.SendQuota&&!I.InitialSendQuota&&!I.Generation,"offline import excludes old connection state");
    xmqttsessionframe Refused;memset(&Refused,0x5a,sizeof(Refused));ssStatus(xmqttSessionNext(S,&Refused),XMQTT_SESSION_STATE_ERROR,"no replay before new authentication/CONNACK");xrtClearError();
    testRequire(stInfo(State).Inbox==2,"protocol import neither adopts nor acknowledges Inbox payloads");xmqttStoreStateDestroy(State);
    xstrview Id={0};testRequire(xmqttSessionClientId(S,&Id)&&Id.Size==12&&!memcmp(Id.Data,"delta-client",12),"independent bound identity survives State destruction");
    double OpenAt=Live?1000000:10;ssStatus(ssOpen(S,V,true,false,OpenAt),XMQTT_SESSION_DONE,"fresh accepted resume");
    const uint16 Ids[]={8,7,9};const xmqttsessionpacket* Held[3];
    for(unsigned N=0;N<3;++N){xmqttsessionframe F={0};ssStatus(xmqttSessionNext(S,&F),XMQTT_SESSION_DONE,"ordered restored wire selection");testRequire(F.Generation==1&&F.Identifier==Ids[N]&&F.Type==(!N?XMQTT_PACKET_PUBREL:XMQTT_PACKET_PUBLISH)&&((F.Prefix.Data[0]&8u)!=0)==(N==1),"new generation, PUBREL priority, attempted DUP1/unattempted DUP0");
        Held[N]=F.Packet;ssPacketViews(Held[N],V);ssStatus(xmqttSessionSent(S,&F),XMQTT_SESSION_DONE,"restored physical frame completed");xmqttpacket Ack;xmqttPacketInit(!N?XMQTT_PACKET_PUBCOMP:XMQTT_PACKET_PUBACK,&Ack);Ack.Identifier=F.Identifier;ssStatus(xmqttSessionAcknowledge(S,&Ack),XMQTT_SESSION_DONE,"new actual protocol acknowledgement");}
    uint8 Reason=255;ssStatus(xmqttSessionReceivePrepare(S,17),XMQTT_SESSION_DUPLICATE,"old accepted receive identity deduplicates");ssStatus(xmqttSessionReceiveComplete(S,17),XMQTT_SESSION_DONE,"old received-PUBREL stage completes directly");
    ssStatus(xmqttSessionReceiveComplete(S,18),XMQTT_SESSION_STATE_ERROR,"PUBLISH-only identity cannot complete before PUBREL");xrtClearError();ssStatus(xmqttSessionReceiveRelease(S,18,&Reason),XMQTT_SESSION_DONE,"fresh PUBREL for old identity");testRequire(!Reason,"known old identity");ssStatus(xmqttSessionReceiveComplete(S,18),XMQTT_SESSION_DONE,"complete old receive flow");
    testRequire(!ssInfo(S).ActiveOutgoing&&!ssInfo(S).Incoming&&ssInfo(S).Objects==3,"held packet references keep physical credits after ACK");ssClose(S,OpenAt);
    for(unsigned N=0;N<3;++N){ssPacketViews(Held[N],V);xmqttSessionPacketRelease(Held[N]);}
}
static void ssClocks(void)
{
    xmqttstorestate* State=ssRecover(XMQTT_VERSION_5,false,60);xmqttsessionconfig K=ssSessionConfig(XMQTT_VERSION_5);
    for(unsigned I=0;I<3;++I){uint64 Wall=I==0?61999:62000;xmqttstoresessionconfig C=ssConfig(I==2?1e200:10,Wall);xmqttsession* S=NULL;
        ssStatus(xmqttStoreSessionRestore(State,&K,&C,&S),I?XMQTT_SESSION_RESET:XMQTT_SESSION_DONE,"import charges additional downtime with exact final millisecond");xmqttsessioninfo Info=ssInfo(S);
        if(!I){testRequire(Info.HasState&&Info.ActiveOutgoing==3,"last millisecond retained");ssStatus(xmqttSessionExpire(S,nextafter(10.001,INFINITY)),XMQTT_SESSION_DONE,"no extra lifetime from ms-to-binary rounding");testRequire(!ssInfo(S).HasState,"expiry physically clears new protocol owner");}
        else testRequire(!Info.HasState&&!Info.ActiveOutgoing&&!Info.Incoming,"expired import publishes only an empty protocol owner");
        ssClose(S,I?C.MonotonicTime:nextafter(10.001,INFINITY));}
    testRequire(stInfo(State).Inbox==2&&stInfo(State).Outgoing==3,"import expiry never mutates source/Inbox");xmqttStoreStateDestroy(State);
}
int main(void)
{for(unsigned V=0;V<2;++V)for(unsigned Live=0;Live<2;++Live)ssReplay(V?XMQTT_VERSION_5:XMQTT_VERSION_311,Live!=0,false);ssReplay(XMQTT_VERSION_5,true,true);ssClocks();puts("[PASS]5 normal/assigned MQTT5/311 imports,property/view/packet lifetime,QoS/receive stages,fresh readiness and3 expiry cuts");return 0;}
