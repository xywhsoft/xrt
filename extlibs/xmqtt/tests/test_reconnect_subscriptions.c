#include "test.h"
#include <xmqtt/reconnect_subscriptions.h>
struct rcConnection;
static bool subControl(struct rcConnection* C,const xmqttpacket* Packet);
static void subConnack(struct rcConnection* C,xmqttpacket* Packet);
#define RC_INPUT_BYTES 20000
#define RC_CONTROL subControl
#define RC_CONNACK subConnack
#define main xmqttReconnectSubscriptionBaseTests
#include "test_reconnect.c"
#undef main
#undef RC_CONTROL
#undef RC_CONNACK
#undef RC_INPUT_BYTES

typedef struct subRecord {unsigned Generation;bool Remove;uint16 Identifier;uint32 SubscriptionId;size_t Count,WireBytes,UserProperties;uint8 Options[8];size_t Lengths[8];char Filters[8][64];} subRecord;
typedef struct subPeer {
    rcPeer Base;xatomic32 Controls,Hold,Events;subRecord Records[256];xatomic32 PacketLimit,Blocked;xmqttproperty PeerProperties[5];const xmqttmessage* HeldAck;xallocationbudget* WorkerBudget;xallocationscope WorkerScope;bool ScopeActive,BudgetEntered;uint64 EventRevision[256];uint8 EventOption[256];size_t NarrowOutgoing;
} subPeer;
static void subConnack(rcConnection* C,xmqttpacket* Packet)
{
    subPeer* P=(subPeer*)C->Peer;
    /* Peer-owned descriptors remain valid through immediate rcSend encoding. */
    xmqttproperty* Props=P->PeerProperties;size_t Count=0;
    uint32 Limit=xrtAtomic32Load(&P->PacketLimit,XMEMORY_ACQUIRE),Blocked=xrtAtomic32Load(&P->Blocked,XMEMORY_ACQUIRE);
    if(P->Base.Version==XMQTT_VERSION_5){
        if(Limit)Props[Count++]=(xmqttproperty){XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,XMQTT_PROPERTY_UINT32,{.Number=Limit==1?48:Limit}};
        const uint32 Ids[]={XMQTT_PROPERTY_WILDCARD_AVAILABLE,XMQTT_PROPERTY_SHARED_AVAILABLE,XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE,XMQTT_PROPERTY_MAXIMUM_QOS};
        for(unsigned I=0;I<4;++I)if(Blocked&(1u<<I))Props[Count++]=(xmqttproperty){Ids[I],XMQTT_PROPERTY_BYTE,{.Number=0}};
        if(Count)Packet->Properties=(xmqttpropertylist){Props,Count};
    }
}
static void subAck(subPeer* P,unsigned Index)
{
    subRecord* R=P->Records+Index;unsigned char Reasons[8];
    for(size_t I=0;I<R->Count;++I)Reasons[I]=!strcmp(R->Filters[I],"bad")?(P->Base.Version==XMQTT_VERSION_5?0x87:0x80):R->Remove?0:(R->Options[I]&3u)>1?1:R->Options[I]&3u;
    xmqttpacket A;xmqttPacketInit(R->Remove?XMQTT_PACKET_UNSUBACK:XMQTT_PACKET_SUBACK,&A);A.Identifier=R->Identifier;
    if(!R->Remove || P->Base.Version==XMQTT_VERSION_5)A.Body.Reasons=(xbytesview){Reasons,R->Count};
    xmqttproperty Reason={XMQTT_PROPERTY_REASON_STRING,XMQTT_PROPERTY_STRING,{.Text=XRT_STR_LITERAL("managed-ack")}};if(P->Base.Version==XMQTT_VERSION_5)A.Properties=(xmqttpropertylist){&Reason,1};
    rcSend(P->Base.Connections+R->Generation,&A);
}
static bool subControl(rcConnection* C,const xmqttpacket* Packet)
{
    if(Packet->Type!=XMQTT_PACKET_SUBSCRIBE && Packet->Type!=XMQTT_PACKET_UNSUBSCRIBE)return false;
    subPeer* P=(subPeer*)C->Peer;unsigned N=xrtAtomic32Load(&P->Controls,XMEMORY_RELAXED);testRequire(N<256 && Packet->Body.Topics.Count<=8,"bounded subscription fixture");
    subRecord* R=P->Records+N;R->Generation=C->Index;R->Remove=Packet->Type==XMQTT_PACKET_UNSUBSCRIBE;R->Identifier=Packet->Identifier;R->Count=Packet->Body.Topics.Count;R->WireBytes=Packet->Wire.Size;
    for(size_t I=0;I<R->Count;++I){xmqttsubscription T=Packet->Body.Topics.Data[I];size_t Copy=T.Filter.Size<sizeof(R->Filters[I])-1?T.Filter.Size:sizeof(R->Filters[I])-1;memcpy(R->Filters[I],T.Filter.Data,Copy);R->Filters[I][Copy]=0;R->Lengths[I]=T.Filter.Size;R->Options[I]=T.Options;}
    for(size_t I=0;I<Packet->Properties.Count;++I){const xmqttproperty* Q=Packet->Properties.Data+I;if(Q->Id==XMQTT_PROPERTY_SUBSCRIPTION_ID)R->SubscriptionId=Q->Value.Number;
        if(Q->Id==XMQTT_PROPERTY_USER){testRequire(Q->Value.Pair.Name.Size==6 && !memcmp(Q->Value.Pair.Name.Data,"source",6) && Q->Value.Pair.Value.Size==8 && !memcmp(Q->Value.Pair.Value.Data,"snapshot",8),"copied user property bytes");++R->UserProperties;}}
    if(!xrtAtomic32Load(&P->Hold,XMEMORY_ACQUIRE))subAck(P,N);
    xrtAtomic32Store(&P->Controls,N+1,XMEMORY_RELEASE);return true;
}
static void subStart(subPeer* P,xmqttversion Version,unsigned Mode)
{memset(P,0,sizeof(*P));xrtAtomic32Init(&P->Controls,0);xrtAtomic32Init(&P->Hold,0);xrtAtomic32Init(&P->Events,0);xrtAtomic32Init(&P->PacketLimit,0);xrtAtomic32Init(&P->Blocked,0);rcStart(&P->Base,Version,Mode);}
static xmqttpacket subPacket(xmqttsubscription* Topics,size_t Count,bool Remove)
{xmqttpacket P;xmqttPacketInit(Remove?XMQTT_PACKET_UNSUBSCRIBE:XMQTT_PACKET_SUBSCRIBE,&P);P.Body.Topics=(xmqttsubscriptionlist){Topics,Count};return P;}
static void subResult(xmqttreconnect* R,uint64 Revision,const xmqttpacket* Request,const xmqttsubscriptionresult* Result,ptr Data)
{
    subPeer* P=Data;xmqttsubscriptionsinfo Info;
    testRequire(Revision && Request->Body.Topics.Count && !Request->Wire.Size && Result && xmqttReconnectSubscriptionsInfo(R,&Info),"ordered borrowed control callback and lock-free reentry");
    if(!P->HeldAck && Result->Ack){testRequire(xmqttMessageRetain(Result->Ack),"retained exact ACK snapshot");P->HeldAck=Result->Ack;}
    unsigned N=xrtAtomic32Load(&P->Events,XMEMORY_RELAXED);testRequire(N<256,"bounded result events");P->EventRevision[N]=Revision;P->EventOption[N]=Request->Body.Topics.Data[0].Options;
    xrtAtomic32FetchAdd(&P->Events,1,XMEMORY_RELEASE);
}
static void subStop(subPeer* P)
{
    rcStop(&P->Base);
    if(P->HeldAck){const xmqttpacket* Ack=xmqttMessagePacket(P->HeldAck);testRequire(Ack && (Ack->Type==XMQTT_PACKET_SUBACK || Ack->Type==XMQTT_PACKET_UNSUBACK),"ACK outlives controller and Engine");
        if(P->Base.Version==XMQTT_VERSION_5)testRequire(Ack->Properties.Count==1 && Ack->Properties.Data[0].Id==XMQTT_PROPERTY_REASON_STRING && Ack->Properties.Data[0].Value.Text.Size==11 && !memcmp(Ack->Properties.Data[0].Value.Text.Data,"managed-ack",11),"full broker ACK metadata preserved");
        xmqttMessageRelease(P->HeldAck);
    }
}
static xmqttsubscriptionsinfo subSynchronized(xmqttreconnect* R,uint64 Revision);
static xmqttclientstatus subNarrowFactory(xnetengine* E,uint64 Affinity,const xmqttclientconfig* K,xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,ptr Data,xmqttclient** C)
{
    subPeer* P=Data;xmqttclientconfig Copy=*K;if(P->NarrowOutgoing)Copy.MaxOutgoingBytes=P->NarrowOutgoing;
    return xmqttClientConnectSession(E,&P->Base.Address,Affinity,&Copy,Session,Inbox,Connect,C)?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;
}
static void subLocalPacketLimit(void)
{
    for(unsigned V=0;V<2;++V)for(unsigned Mode=0;Mode<3;++Mode){subPeer P;subStart(&P,V?XMQTT_VERSION_311:XMQTT_VERSION_5,0);xmqttreconnectconfig K;xmqttpacket Connect;xmqttproperty Expiry;rcConfig(&P.Base,&K,&Connect,&Expiry);
        K.Factory=subNarrowFactory;K.FactoryData=&P;K.Client.MaxOutgoingBytes=Mode?4096:40;P.NarrowOutgoing=Mode?40:0;
        xmqttsubscriptionsconfig S;xmqttSubscriptionsConfigInit(&S);S.MaxFilters=4;S.MaxBatch=3;
        xmqttsubscription Topics[]={{XRT_STR_LITERAL("aaaaaaaaaaaaaaaaaaaa"),1},{XRT_STR_LITERAL("bbbbbbbbbbbbbbbbbbbb"),1},{XRT_STR_LITERAL("cccccccccccccccccccc"),1}};
        if(Mode==2){Topics[0].Filter=XRT_STR_LITERAL("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");S.FailOnRejected=false;}
        xmqttpacket Packet=subPacket(Topics,Mode==2?1:3,false);xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreateSubscribed(P.Base.Engine,P.Base.Pool,1,&K,&S,&Connect,&Packet,1,&R),"local capacity controller");
        (void)rcOnline(R,1);xmqttsubscriptionsinfo I=subSynchronized(R,1);
        if(Mode==2){xmqttsubscriptionintentinfo E;testRequire(!I.Requests && I.Rejected==1 && xmqttReconnectSubscriptionInfo(R,Topics[0].Filter,&E) && E.Status==XMQTT_STATUS_RESTRICTED,"unfit single filter fails locally without quota retry");}
        else {rcWait(&P.Controls,3);testRequire(I.Requests==3 && !I.AdmissionRetries && I.Desired==3,"actual factory byte capacity splits before Client admission");for(unsigned N=0;N<3;++N)testRequire(P.Records[N].Count==1 && P.Records[N].WireBytes<=40,"every control fits local byte capacity");}
        rcClose(R);subStop(&P);
    }
}
static xmqttreconnect* subCreate(subPeer* P,const xmqttpacket* Initial,size_t Count,size_t Batch,bool Strict)
{
    xmqttreconnectconfig K;xmqttpacket Connect;xmqttproperty Expiry;rcConfig(&P->Base,&K,&Connect,&Expiry);
    xmqttsubscriptionsconfig S;testRequire(xmqttSubscriptionsConfigInit(&S),"subscription config");S.MaxFilters=16;S.MaxBatch=Batch;S.MaxBytes=65536;S.FailOnRejected=Strict;S.Notify=subResult;S.User=P;
    xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreateSubscribed(P->Base.Engine,P->Base.Pool,1,&K,&S,&Connect,Initial,Count,&R),"subscribed controller");return R;
}
static xmqttsubscriptionsinfo subSynchronized(xmqttreconnect* R,uint64 Revision)
{
    double End=xrtTimer()+5;xmqttsubscriptionsinfo S;
    do {testRequire(xmqttReconnectSubscriptionsInfo(R,&S) && xrtTimer()<End,"subscription synchronization deadline");if(S.Synchronized && S.SettledRevision>=Revision)return S;xrtSleep(1);}while(true);
}
static void subHappy(xmqttversion Version,bool Persist)
{
    subPeer P;subStart(&P,Version,Persist?RC_PERSIST:0);char A[]="alpha/#",B[]="beta/+";
    xmqttsubscription Topics[]={{{A,sizeof(A)-1},Version==XMQTT_VERSION_5?0x2a:2},{{B,sizeof(B)-1},1}};
    xmqttpacket Initial=subPacket(Topics,2,false);char Name[]="source",Value[]="snapshot";xmqttproperty Properties[2]={{XMQTT_PROPERTY_SUBSCRIPTION_ID,XMQTT_PROPERTY_VARIABLE,{.Number=42}},{XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{.Pair={{Name,6},{Value,8}}}}};
    if(Version==XMQTT_VERSION_5)Initial.Properties=(xmqttpropertylist){Properties,2};
    xmqttreconnect* R=subCreate(&P,&Initial,1,1,true);memset(A,'x',sizeof(A)-1);memset(B,'y',sizeof(B)-1);Properties[0].Value.Number=99;memset(Name,'z',6);memset(Value,'z',8);
    (void)rcOnline(R,1);xmqttsubscriptionsinfo S=subSynchronized(R,1);testRequire(S.Desired==2 && S.Requests==2 && S.Filters==2 && !S.Rejected,"initial intents restored before ONLINE");
    rcWait(&P.Controls,2);for(unsigned I=0;I<2;++I)testRequire(!P.Records[I].Remove && P.Records[I].Count==1 && (Version!=XMQTT_VERSION_5 || (P.Records[I].SubscriptionId==42 && P.Records[I].UserProperties==1)),"copied group properties on split batches");
    xmqttsubscriptionintentinfo Info;testRequire(xmqttReconnectSubscriptionInfo(R,XRT_STR_LITERAL("alpha/#"),&Info) && Info.Confirmed && Info.Reason==1 && Info.Options==(Version==XMQTT_VERSION_5?0x2a:2),"per-filter grant and original options");
    testRequire(xrtNetStreamAbort(P.Base.Connections[0].Stream),"subscription connection outage");(void)rcOnline(R,2);S=subSynchronized(R,1);
    testRequire(S.Requests==(Persist?2:4) && S.Recoveries==1 && xrtAtomic32Load(&P.Controls,XMEMORY_ACQUIRE)==(Persist?2u:4u),"retain known subscription state only on SessionPresent");
    xmqttsubscription Topic={XRT_STR_LITERAL("alpha/#"),0};xmqttpacket Remove=subPacket(&Topic,1,true);uint64 Revision=0;
    testRequire(xmqttReconnectUnsubscribeIntent(R,&Remove,&Revision)==XMQTT_ADMISSION_ACCEPTED,"managed removal");S=subSynchronized(R,Revision);testRequire(S.Filters==1 && S.Desired==1,"confirmed removal reclaimed");
    xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();memset(&Info,0x55,sizeof(Info));xmqttsubscriptionintentinfo Before=Info;
    testRequire(!xmqttReconnectSubscriptionInfo(R,Topic.Filter,&Info) && !memcmp(&Info,&Before,sizeof(Info)) && xrtGetError()==Error,"normal missing lookup preserves error/output");xrtClearError();rcClose(R);subStop(&P);
}
static void subSuperseded(void)
{
    subPeer P;subStart(&P,XMQTT_VERSION_5,0);xrtAtomic32Store(&P.Hold,1,XMEMORY_RELEASE);
    xmqttsubscription Topic={XRT_STR_LITERAL("replace"),0};xmqttpacket Initial=subPacket(&Topic,1,false);xmqttreconnect* R=subCreate(&P,&Initial,1,8,true);rcWait(&P.Controls,1);
    xmqttclientlease Lease={0};testRequire(!xmqttReconnectAcquire(R,&Lease),"initial recovery quarantines leases");
    Topic.Options=2;uint64 Revision=0;testRequire(xmqttReconnectSubscribeIntent(R,&Initial,&Revision)==XMQTT_ADMISSION_ACCEPTED,"replace in-flight desired options");
    subAck(&P,0);rcWait(&P.Controls,2);xmqttsubscriptionintentinfo I;testRequire(xmqttReconnectSubscriptionInfo(R,Topic.Filter,&I) && I.Pending && !I.Confirmed && I.AppliedRevision!=Revision,"old ACK never confirms replacement");
    subAck(&P,1);(void)subSynchronized(R,Revision);testRequire(xrtAtomic32Load(&P.Events,XMEMORY_ACQUIRE)==2 && P.EventRevision[0]==1 && P.EventRevision[1]==Revision && !P.EventOption[0] && P.EventOption[1]==2,"result callbacks retain original revision and ordered request");testRequire(xmqttReconnectSubscriptionInfo(R,Topic.Filter,&I) && I.Confirmed && I.Reason==1 && I.Options==2,"new ACK confirms replacement");
    Topic.Options=0;xmqttpacket Remove=subPacket(&Topic,1,true);testRequire(xmqttReconnectUnsubscribeIntent(R,&Remove,&Revision)==XMQTT_ADMISSION_ACCEPTED,"start removal");rcWait(&P.Controls,3);
    Topic.Options=1;testRequire(xmqttReconnectSubscribeIntent(R,&Initial,&Revision)==XMQTT_ADMISSION_ACCEPTED,"replace pending removal with subscription");subAck(&P,2);rcWait(&P.Controls,4);
    testRequire(xmqttReconnectSubscriptionInfo(R,Topic.Filter,&I) && I.Pending && I.Desired,"old removal does not erase new intent");subAck(&P,3);(void)subSynchronized(R,Revision);rcClose(R);subStop(&P);
}
static void subRejected(bool Strict,xmqttversion Version)
{
    subPeer P;subStart(&P,Version,0);xmqttsubscription Topics[]={{XRT_STR_LITERAL("good"),1},{XRT_STR_LITERAL("bad"),1}};xmqttpacket Initial=subPacket(Topics,2,false);xmqttreconnect* R=subCreate(&P,&Initial,1,8,Strict);
    if(Strict){testRequire(xmqttReconnectWaitFor(R,5000)==XWAIT_OK,"strict rejected restoration retires");xmqttreconnectinfo I;xmqttReconnectInfo(R,&I);testRequire(I.State==XMQTT_RECONNECT_FAILED && I.LastStatus==XMQTT_STATUS_REJECTED,"typed rejection terminal status");}
    else {(void)rcOnline(R,1);xmqttsubscriptionsinfo S=subSynchronized(R,1);testRequire(S.Rejected==1 && S.Status==XMQTT_STATUS_REJECTED,"explicit partial rejection");}
    xmqttsubscriptionintentinfo I;testRequire(xmqttReconnectSubscriptionInfo(R,XRT_STR_LITERAL("bad"),&I) && !I.Confirmed && I.Status==XMQTT_STATUS_REJECTED && I.Reason==(Version==XMQTT_VERSION_5?0x87:0x80),"exact negative filter outcome survives retirement");
    rcClose(R);subStop(&P);
}
static void subOffline(void)
{
    subPeer P;subStart(&P,XMQTT_VERSION_5,RC_PERSIST);xmqttreconnect* R=subCreate(&P,NULL,0,8,true);(void)rcOnline(R,1);xrtAtomic32Store(&P.Hold,1,XMEMORY_RELEASE);
    xmqttsubscription Old={XRT_STR_LITERAL("uncertain"),1};xmqttpacket Add=subPacket(&Old,1,false);uint64 V=0;testRequire(xmqttReconnectSubscribeIntent(R,&Add,&V)==XMQTT_ADMISSION_ACCEPTED,"uncertain control intent");rcWait(&P.Controls,1);
    xmqttclientlease L={0};testRequire(xmqttReconnectAcquire(R,&L) && xrtNetStreamAbort(P.Base.Connections[0].Stream) && xmqttClientWaitFor(L.Client,5000)==XWAIT_OK,"hold offline generation");
    Old.Options=0;xmqttpacket Remove=subPacket(&Old,1,true);testRequire(xmqttReconnectUnsubscribeIntent(R,&Remove,&V)==XMQTT_ADMISSION_ACCEPTED,"offline uncertain removal tombstone");
    xmqttsubscription New={XRT_STR_LITERAL("offline-new"),2};Add=subPacket(&New,1,false);testRequire(xmqttReconnectSubscribeIntent(R,&Add,&V)==XMQTT_ADMISSION_ACCEPTED,"offline replacement snapshot");
    xrtAtomic32Store(&P.Hold,0,XMEMORY_RELEASE);testRequire(xmqttReconnectRelease(&L),"release old generation");(void)rcOnline(R,2);xmqttsubscriptionsinfo S=subSynchronized(R,V);
    testRequire(S.Filters==1 && S.Desired==1 && S.Requests==3 && P.Records[1].Remove && !strcmp(P.Records[1].Filters[0],"uncertain"),"resumed session reconciles uncertain removal before new subscription");rcClose(R);subStop(&P);
}
static void subLargeSnapshot(void)
{
    subPeer P;subStart(&P,XMQTT_VERSION_5,0);xmqttreconnectconfig K;xmqttpacket Connect;xmqttproperty Expiry;rcConfig(&P.Base,&K,&Connect,&Expiry);
    xmqttsubscriptionsconfig S;xmqttSubscriptionsConfigInit(&S);S.MaxBatch=8;
    xmqttsubscription Topics[1024];char Names[1024][32];
    for(size_t I=0;I<1024;++I){int N=snprintf(Names[I],sizeof(Names[I]),"large/%04zu",1023-I);testRequire(N>0 && (size_t)N<sizeof(Names[I]),"large snapshot fixture filter");Topics[I]=(xmqttsubscription){{Names[I],(size_t)N},(uint8)(I%3)};}
    xmqttpacket Initial=subPacket(Topics,1024,false);xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreateSubscribed(P.Base.Engine,P.Base.Pool,1,&K,&S,&Connect,&Initial,1,&R),"large copied subscription group");memset(Topics,0x55,sizeof(Topics));memset(Names,'x',sizeof(Names));
    (void)rcOnline(R,1);xmqttsubscriptionsinfo Info=subSynchronized(R,1);rcWait(&P.Controls,128);testRequire(Info.Desired==1024 && Info.Requests==128 && !Info.Pending,"large bounded snapshot completely restored");
    for(size_t I=0;I<1024;++I){char Expected[32];snprintf(Expected,sizeof(Expected),"large/%04zu",1023-I);testRequire(P.Records[I/8].Count==8 && !strcmp(P.Records[I/8].Filters[I%8],Expected) && P.Records[I/8].Options[I%8]==I%3,"snapshot detection preserves caller filter/ACK order across all batches");}
    xmqttsubscriptionintentinfo Entry;testRequire(xmqttReconnectSubscriptionInfo(R,XRT_STR_LITERAL("large/0000"),&Entry) && Entry.Confirmed,"large exact-filter lookup");rcClose(R);subStop(&P);
}
static void subLimits(void)
{
    subPeer P;subStart(&P,XMQTT_VERSION_5,0);xmqttreconnectconfig K;xmqttpacket Connect;xmqttproperty Expiry;rcConfig(&P.Base,&K,&Connect,&Expiry);
    xmqttsubscriptionsconfig Config;xmqttSubscriptionsConfigInit(&Config);Config.MaxFilters=2;Config.MaxBatch=2;Config.MaxBytes=8192;
    xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreateSubscribed(P.Base.Engine,P.Base.Pool,1,&K,&Config,&Connect,NULL,0,&R),"small bounded registry");(void)rcOnline(R,1);
    xmqttsubscription Topics[]={{XRT_STR_LITERAL("one"),0},{XRT_STR_LITERAL("two"),1}};xmqttpacket Packet=subPacket(Topics,2,false);uint64 Revision=0;
    testRequire(xmqttReconnectSubscribeIntent(R,&Packet,&Revision)==XMQTT_ADMISSION_ACCEPTED,"fill filter slots");(void)subSynchronized(R,Revision);
    xmqttsubscription Extra={XRT_STR_LITERAL("three"),1};Packet=subPacket(&Extra,1,false);Revision=UINT64_MAX;xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();
    testRequire(xmqttReconnectSubscribeIntent(R,&Packet,&Revision)==XMQTT_ADMISSION_OVERLOADED && Revision==UINT64_MAX && xrtGetError()==Error,"slot overload preserves output/error");xrtClearError();
    Topics[1]=Topics[0];Packet=subPacket(Topics,2,false);testRequire(xmqttReconnectSubscribeIntent(R,&Packet,&Revision)==XMQTT_ADMISSION_ERROR && Revision==UINT64_MAX && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"duplicate intent rejected atomically");xrtClearError();
    xmqttsubscriptionsinfo S;xmqttReconnectSubscriptionsInfo(R,&S);testRequire(S.Revision==1 && S.Admitted==1 && S.Overloaded==1 && S.Bytes+S.Reserved<=Config.MaxBytes,"bounded counted admissions");
    testRequire(xmqttReconnectClose(R) && xmqttReconnectWaitFor(R,5000)==XWAIT_OK,"closed bounded registry");xrtSetErrorKind(XERR_MEMORY);Error=xrtGetError();
    testRequire(xmqttReconnectSubscribeIntent(R,&Packet,&Revision)==XMQTT_ADMISSION_CLOSED && Revision==UINT64_MAX && xrtGetError()==Error,"closed intent preserves output/error before descriptor work");xrtClearError();
    testRequire(xmqttReconnectDestroy(R),"small registry destroy");rcStop(&P.Base);
}
static void subPacketLimits(void)
{
    subPeer P;subStart(&P,XMQTT_VERSION_5,0);xrtAtomic32Store(&P.PacketLimit,48,XMEMORY_RELEASE);
    xmqttsubscription Topics[]={{XRT_STR_LITERAL("aaaaaaaaaaaaaaaaaaaa"),1},{XRT_STR_LITERAL("bbbbbbbbbbbbbbbbbbbb"),1},{XRT_STR_LITERAL("cccccccccccccccccccc"),1}};
    xmqttpacket Packet=subPacket(Topics,3,false);xmqttreconnect* R=subCreate(&P,&Packet,1,8,false);(void)rcOnline(R,1);xmqttsubscriptionsinfo S=subSynchronized(R,1);
    testRequire(S.Requests==3,"negotiated packet size splits a group");rcWait(&P.Controls,3);for(unsigned I=0;I<3;++I)testRequire(P.Records[I].Count==1 && P.Records[I].WireBytes<=48,"every batch fits peer limit");
    xmqttsubscription Long={XRT_STR_LITERAL("dddddddddddddddddddddddddddddddddddddddddddddddddd"),1};Packet=subPacket(&Long,1,false);uint64 V=0;
    testRequire(xmqttReconnectSubscribeIntent(R,&Packet,&V)==XMQTT_ADMISSION_ACCEPTED,"locally valid but peer-restricted intent");S=subSynchronized(R,V);xmqttsubscriptionintentinfo I;
    testRequire(S.Requests==3 && S.Rejected==1 && xmqttReconnectSubscriptionInfo(R,Long.Filter,&I) && I.Status==XMQTT_STATUS_RESTRICTED && !I.Confirmed,"explicit local peer restriction without a wire request");rcClose(R);subStop(&P);
}
/* Public codec bytes are the oracle: an exact cap admits the long filter,
 * one byte less rejects it locally, and the short following filter survives.
 * Exercise Remaining Length and Property Length encoding width transitions. */
static void subSizingBoundaries(void)
{
    const uint32 Boundaries[]={127,128,16383,16384};
    for(unsigned Version=0;Version<2;++Version)for(unsigned Remove=0;Remove<2;++Remove)for(size_t B=0;B<4;++B)for(unsigned Below=0;Below<2;++Below){
        subPeer* P=calloc(1,sizeof(*P));testRequire(P!=NULL,"large boundary fixture");subStart(P,Version?XMQTT_VERSION_311:XMQTT_VERSION_5,0);
        xmqttreconnectconfig K;xmqttpacket Connect;xmqttproperty Expiry;rcConfig(&P->Base,&K,&Connect,&Expiry);
        xmqttproperty Props[8];for(size_t I=0;I<8;++I)Props[I]=(xmqttproperty){XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{.Pair={XRT_STR_LITERAL("source"),XRT_STR_LITERAL("snapshot")}}};
        char* Filter=malloc(16384);testRequire(Filter!=NULL,"boundary filter");memset(Filter,'a',16384);
        xmqttsubscription Topics[]={{{Filter,1},Remove?0:2},{XRT_STR_LITERAL("tail"),Remove?0:1}};
        xmqttpacket Packet=subPacket(Topics,1,Remove!=0);Packet.Identifier=1;
        if(!Version && B>=2)Packet.Properties=(xmqttpropertylist){Props,8};
        unsigned char Wire[20000];size_t Bytes=0;xmqttfixedheader Header;xmqttwireconfig W;xmqttWireConfigInit(&W);W.Version=P->Base.Version;
        testRequire((Version?xmqttPacketWrite311(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Bytes):xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Bytes)) && xmqttHeaderRead((xbytesview){Wire,Bytes},&W,&Header)==XMQTT_WIRE_DONE,"public codec boundary base");
        Topics[0].Filter.Size+=Boundaries[B]-Header.RemainingLength;
        testRequire((Version?xmqttPacketWrite311(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Bytes):xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Bytes)) && xmqttHeaderRead((xbytesview){Wire,Bytes},&W,&Header)==XMQTT_WIRE_DONE && Header.RemainingLength==Boundaries[B],"independent exact encoded length oracle");
        K.Client.MaxOutgoingBytes=Bytes-Below;
        /* MQTT5 also exercises the peer cap independently of local capacity. */
        if(!Version){xrtAtomic32Store(&P->PacketLimit,(uint32)(Bytes-Below),XMEMORY_RELEASE);K.Client.MaxOutgoingBytes=65536;}
        xmqttsubscriptionsconfig S;xmqttSubscriptionsConfigInit(&S);S.MaxFilters=2;S.MaxBatch=2;S.FailOnRejected=false;
        xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreateSubscribed(P->Base.Engine,P->Base.Pool,1,&K,&S,&Connect,NULL,0,&R),"boundary controller");(void)rcOnline(R,1);
        Packet.Identifier=0;Packet.Body.Topics.Count=2;uint64 Revision=0;
        testRequire((Remove?xmqttReconnectUnsubscribeIntent(R,&Packet,&Revision):xmqttReconnectSubscribeIntent(R,&Packet,&Revision))==XMQTT_ADMISSION_ACCEPTED,"boundary intent admission");
        xmqttsubscriptionsinfo Info=subSynchronized(R,Revision);rcWait(&P->Controls,Below?1:2);
        testRequire(Info.Requests==(Below?1u:2u) && Info.Rejected==Below && !Info.AdmissionRetries,"exact boundary split/restriction without admission retries");
        if(!Below)testRequire(P->Records[0].Count==1 && P->Records[0].Lengths[0]==Topics[0].Filter.Size && P->Records[0].WireBytes==Bytes,"wire bytes equal public codec at width boundary");
        testRequire(P->Records[Below?0:1].Count==1 && !strcmp(P->Records[Below?0:1].Filters[0],"tail"),"later filter remains ordered after split/restriction");
        rcClose(R);subStop(P);free(Filter);free(P);
    }
}
static void subCapabilities(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){subPeer P;subStart(&P,XMQTT_VERSION_5,0);
        xrtAtomic32Store(&P.Blocked,Mode==3?4u:Mode==4?8u:1u<<Mode,XMEMORY_RELEASE);
        xmqttsubscription Topics[]={{XRT_STR_LITERAL("allowed"),2},{Mode==1?XRT_STR_LITERAL("$share/g/topic"):XRT_STR_LITERAL("wild/+"),1},{XRT_STR_LITERAL("later"),1}};
        xmqttproperty Id={XMQTT_PROPERTY_SUBSCRIPTION_ID,XMQTT_PROPERTY_VARIABLE,{.Number=42}};xmqttpacket Packet=subPacket(Topics,3,false);if(Mode==2)Packet.Properties=(xmqttpropertylist){&Id,1};
        xmqttreconnect* R=subCreate(&P,&Packet,1,8,false);(void)rcOnline(R,1);xmqttsubscriptionsinfo I=subSynchronized(R,1);
        unsigned Rejected=Mode==2?3:Mode<2?1:0,Requests=Mode==2?0:Mode<2?2:1;
        testRequire(I.Rejected==Rejected && I.Requests==Requests && !I.AdmissionRetries,"cached capabilities preserve per-filter restrictions and requested QoS above peer publish maximum");
        Packet=subPacket(Topics,3,true);for(unsigned N=0;N<3;++N)Topics[N].Options=0;uint64 Revision=0;
        testRequire(xmqttReconnectUnsubscribeIntent(R,&Packet,&Revision)==XMQTT_ADMISSION_ACCEPTED,"removal ignores subscription-only capabilities");I=subSynchronized(R,Revision);
        testRequire(!I.Filters && !I.Rejected && I.Requests==Requests+1,"all removals allowed and reclaimed");rcClose(R);subStop(&P);
    }
}
static void subDeadlines(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){subPeer P;subStart(&P,XMQTT_VERSION_5,0);xrtAtomic32Store(&P.Hold,1,XMEMORY_RELEASE);
        xmqttreconnectconfig K;xmqttpacket Connect;xmqttproperty Expiry;rcConfig(&P.Base,&K,&Connect,&Expiry);K.Client.MaxRequests=1;
        xmqttsubscriptionsconfig S;xmqttSubscriptionsConfigInit(&S);S.MaxFilters=4;S.MaxBatch=2;S.OperationTimeout=Mode==1?40:1000;
        xmqttsubscription Topic={XRT_STR_LITERAL("deadline"),1};xmqttpacket Packet=subPacket(&Topic,1,false);xmqttreconnect* R=NULL;
        if(!Mode)K.RecoveryTimeout=40;
        double Start=xrtTimer();testRequire(xmqttReconnectCreateSubscribed(P.Base.Engine,P.Base.Pool,1,&K,&S,&Connect,Mode?NULL:&Packet,Mode?0:1,&R),"deadline controller");
        xfuture* Block=NULL;xmqttclientlease L={0};
        if(Mode){(void)rcOnline(R,1);if(Mode==1){
                testRequire(xmqttReconnectAcquire(R,&L) && xmqttClientSubscribeAsync(L.Client,&Packet,5000,&Block)==XMQTT_ADMISSION_ACCEPTED,"fill ordinary Client control admission");rcWait(&P.Controls,1);testRequire(xmqttReconnectRelease(&L),"release ordinary blocker lease");
            }
            uint64 V=0;testRequire(xmqttReconnectSubscribeIntent(R,&Packet,&V)==XMQTT_ADMISSION_ACCEPTED,"online deadline intent");if(Mode==2){rcWait(&P.Controls,1);testRequire(xmqttReconnectClose(R),"stop with real pending watch");}
        }
        testRequire(xmqttReconnectWaitFor(R,5000)==XWAIT_OK && xrtTimer()-Start<2,"restoration/admission/stop have finite physical completion");xmqttreconnectinfo I;xmqttReconnectInfo(R,&I);
        if(Mode==2)testRequire(I.State==XMQTT_RECONNECT_STOPPED,"stop retires active control and watch");
        else {testRequire(I.State==XMQTT_RECONNECT_FAILED && I.LastStatus==XMQTT_STATUS_TIMEOUT,"outage includes restoration and online quota wait is bounded");if(Mode==1){xmqttsubscriptionsinfo Info;xmqttReconnectSubscriptionsInfo(R,&Info);testRequire(Info.AdmissionRetries && !Info.Requests,"quota retries are counted and never accepted another control");}}
        testRequire(xmqttReconnectDestroy(R),"deadline physical destroy");if(Block){testRequire(xrtFutureDone(Block),"old ordinary Future settles without controller ownership");xrtFutureDestroy(Block);}rcStop(&P.Base);
    }
}
static void subOOM(void)
{
    const size_t Limits[]={1,64,256,512,2048,8192,65536};
    for(unsigned Constructor=0;Constructor<2;++Constructor)for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){
        subPeer P;subStart(&P,XMQTT_VERSION_5,0);xmqttreconnectconfig K;xmqttpacket Connect;xmqttproperty Expiry;rcConfig(&P.Base,&K,&Connect,&Expiry);
        xmqttsubscriptionsconfig S;xmqttSubscriptionsConfigInit(&S);S.MaxFilters=4;S.MaxBatch=2;S.MaxBytes=16384;
        xmqttsubscription Topic={XRT_STR_LITERAL("oom/snapshot"),1};xmqttpacket Packet=subPacket(&Topic,1,false);xmqttreconnect* R=NULL;
        if(!Constructor){testRequire(xmqttReconnectCreateSubscribed(P.Base.Engine,P.Base.Pool,1,&K,&S,&Connect,NULL,0,&R),"OOM online controller");(void)rcOnline(R,1);}
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&B) && xrtAllocationBudgetEnter(B,&Scope),"snapshot allocation budget");
        bool OK;uint64 V=UINT64_MAX;
        if(Constructor){R=(xmqttreconnect*)(uintptr_t)1;OK=xmqttReconnectCreateSubscribed(P.Base.Engine,P.Base.Pool,1,&K,&S,&Connect,&Packet,1,&R);}
        else OK=xmqttReconnectSubscribeIntent(R,&Packet,&V)==XMQTT_ADMISSION_ACCEPTED;
        testRequire(xrtAllocationBudgetLeave(&Scope),"snapshot scope leave");xrtClearError();
        if(Constructor && !OK){testRequire(R==(xmqttreconnect*)(uintptr_t)1,"constructor output atomic on OOM");R=NULL;}
        if(R){if(OK)(void)subSynchronized(R,Constructor?1:V);else {xmqttsubscriptionsinfo Info;xmqttReconnectSubscriptionsInfo(R,&Info);testRequire(!Info.Revision && V==UINT64_MAX,"snapshot failure publishes no revision");}rcClose(R);}
        rcStop(&P.Base);xrtClearError();xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveCount && !Info.LiveBytes,"all constructor/snapshot quota owners retired");xrtAllocationBudgetFree(B);
    }
}
static void subBudgetResult(xmqttreconnect* R,uint64 Revision,const xmqttpacket* Request,const xmqttsubscriptionresult* Result,ptr Data)
{
    (void)R;(void)Revision;(void)Request;(void)Result;subPeer* P=Data;
    if(!P->BudgetEntered){testRequire(xrtAllocationBudgetEnter(P->WorkerBudget,&P->WorkerScope),"worker budget after original ACK and before replacement watch");P->BudgetEntered=true;P->ScopeActive=true;}
}
static void subBudgetLifecycle(xmqttreconnect* R,ptr Data)
{
    subPeer* P=Data;xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I),"worker budget lifecycle");
    if(P->ScopeActive && (I.State==XMQTT_RECONNECT_ONLINE || I.State==XMQTT_RECONNECT_RETIRING || I.State>=XMQTT_RECONNECT_STOPPED)){
        testRequire(xrtAllocationBudgetLeave(&P->WorkerScope),"worker budget leaves on the same Engine worker");P->ScopeActive=false;xrtClearError();
    }
}
static void subWorkerOOM(void)
{
    const size_t Limits[]={1,64,128,512,2048,8192,65536};
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){subPeer P;subStart(&P,XMQTT_VERSION_5,0);xrtAtomic32Store(&P.Hold,1,XMEMORY_RELEASE);
        testRequire(xrtAllocationBudgetCreate(Limits[J],&P.WorkerBudget),"worker control budget");xmqttreconnectconfig K;xmqttpacket Connect;xmqttproperty Expiry;rcConfig(&P.Base,&K,&Connect,&Expiry);K.Notify=subBudgetLifecycle;K.User=&P;K.MaxAttempts=1;
        xmqttsubscriptionsconfig S;xmqttSubscriptionsConfigInit(&S);S.MaxFilters=4;S.MaxBatch=2;S.Notify=subBudgetResult;S.User=&P;
        xmqttsubscription Topic={XRT_STR_LITERAL("worker-oom"),0};xmqttpacket Packet=subPacket(&Topic,1,false);xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreateSubscribed(P.Base.Engine,P.Base.Pool,1,&K,&S,&Connect,&Packet,1,&R),"worker quota controller");rcWait(&P.Controls,1);
        Topic.Options=1;uint64 Revision=0;testRequire(xmqttReconnectSubscribeIntent(R,&Packet,&Revision)==XMQTT_ADMISSION_ACCEPTED,"replacement snapshot precedes worker quota");xrtAtomic32Store(&P.Hold,0,XMEMORY_RELEASE);subAck(&P,0);
        double End=xrtTimer()+5;xmqttreconnectinfo I;do {testRequire(xmqttReconnectInfo(R,&I) && xrtTimer()<End,"worker quota terminal/online deadline");if(I.State!=XMQTT_RECONNECT_ONLINE && I.State<XMQTT_RECONNECT_STOPPED)xrtSleep(1);}while(I.State!=XMQTT_RECONNECT_ONLINE && I.State<XMQTT_RECONNECT_STOPPED);
        if(Limits[J]==1){xmqttsubscriptionsinfo Info;testRequire(xmqttReconnectSubscriptionsInfo(R,&Info) && I.State==XMQTT_RECONNECT_FAILED && I.LastStatus==XMQTT_STATUS_MEMORY && Info.Requests==1 && Info.Status==XMQTT_STATUS_MEMORY,"first replacement watch allocation fails with no second Client admission");}
        rcClose(R);subStop(&P);testRequire(P.BudgetEntered && !P.ScopeActive,"worker scope physically retired");xrtClearError();xallocationbudgetinfo B={0};B.Size=sizeof(B);testRequire(xrtAllocationBudgetInfo(P.WorkerBudget,&B) && !B.LiveCount && !B.LiveBytes,"watch/control worker OOM releases all owners");xrtAllocationBudgetFree(P.WorkerBudget);
    }
}
typedef struct subRace {xmqttreconnect* Controller;xatomic32 Accepted,Done;} subRace;
static int32 subProducer(ptr Data)
{
    subRace* Race=Data;
    for(unsigned N=0;N<32;++N){xmqttsubscription Topic={XRT_STR_LITERAL("race"),(uint8)(N%3)};xmqttpacket Packet=subPacket(&Topic,1,false);uint64 Revision=UINT64_MAX;
        xmqttclientadmission A=xmqttReconnectSubscribeIntent(Race->Controller,&Packet,&Revision);testRequire(A==XMQTT_ADMISSION_ACCEPTED || A==XMQTT_ADMISSION_OVERLOADED,"concurrent intent admission");
        if(A==XMQTT_ADMISSION_ACCEPTED)xrtAtomic32FetchAdd(&Race->Accepted,1,XMEMORY_RELAXED);else testRequire(Revision==UINT64_MAX,"concurrent overload output atomicity");
        xmqttsubscriptionsinfo I;testRequire(xmqttReconnectSubscriptionsInfo(Race->Controller,&I) && I.Bytes+I.Reserved<=65536 && I.Watchers<=2,"live and reserved storage stay bounded");
    }xrtAtomic32FetchAdd(&Race->Done,1,XMEMORY_RELEASE);return 0;
}
static void subConcurrent(void)
{
    subPeer P;subStart(&P,XMQTT_VERSION_5,0);xmqttreconnect* R=subCreate(&P,NULL,0,8,true);(void)rcOnline(R,1);subRace Race={R,{0},{0}};xrtAtomic32Init(&Race.Accepted,0);xrtAtomic32Init(&Race.Done,0);
    xthread* Threads[4];for(unsigned N=0;N<4;++N){Threads[N]=xrtThreadCreate(subProducer,&Race,0);testRequire(Threads[N]!=NULL,"concurrent snapshot producer");}rcWait(&Race.Done,4);
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(Threads[N],5000)==XWAIT_OK,"intent producer join");xrtThreadDestroy(Threads[N]);}
    xmqttsubscriptionsinfo I=subSynchronized(R,0);testRequire(I.Admitted==xrtAtomic32Load(&Race.Accepted,XMEMORY_ACQUIRE) && I.Revision==I.Admitted && I.Filters==1 && I.SettledRevision==I.Revision,"latest concurrent revisions all reconciled");rcClose(R);subStop(&P);
}
int main(void)
{
    subHappy(XMQTT_VERSION_5,false);subHappy(XMQTT_VERSION_5,true);subHappy(XMQTT_VERSION_311,false);subHappy(XMQTT_VERSION_311,true);
    subSuperseded();subRejected(true,XMQTT_VERSION_5);subRejected(false,XMQTT_VERSION_5);subRejected(true,XMQTT_VERSION_311);subRejected(false,XMQTT_VERSION_311);subOffline();
    subLocalPacketLimit();subLargeSnapshot();subLimits();subPacketLimits();subSizingBoundaries();subCapabilities();subDeadlines();subOOM();subWorkerOOM();subConcurrent();
    puts("[PASS] automatic subscription intent recovery: bounded snapshots, split properties, session loss/resumption, supersession, per-filter rejection and offline tombstones");return 0;
}
