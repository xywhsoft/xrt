#include "support/store_state_peer.h"
static jpdrive Drive;
static void siAssigned(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){xmqttversion V=Mode==1?XMQTT_VERSION_311:XMQTT_VERSION_5;memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttjournal* J=stWrite(F,V,Source.Ops,10,false);xmqttstorestateconfig C=stConfig(V);xmqttstorestate* S=(xmqttstorestate*)(uintptr_t)1;
        if(Mode<2)C.ClientId=(xstrview){0};else C.Delta.Version=XMQTT_VERSION_311;
        stStatus(xmqttStoreStateRecover(J,&C,&S),Mode==1?XMQTT_STORE_ERROR:Mode==2?XMQTT_STORE_STATE_ERROR:XMQTT_STORE_DONE,"explicit assigned identity/version recovery");
        if(!Mode){testRequire(stInfo(S).ClientId.Size==12&&stInfo(S).Inbox==2,"assigned MQTT5 identity copied");xmqttStoreStateDestroy(S);}
        else {testRequire(S==(xmqttstorestate*)(uintptr_t)1,"identity refusal leaves output unchanged");xrtClearError();C=stConfig(V);S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"identity/version refusal precedes journal consumption");xmqttStoreStateDestroy(S);}
        testRequire(xmqttJournalDestroy(J),"identity journal released");jpFileClose(F);}
}
static void siResume(xmqttversion V,bool Live)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttstoredelta Ops[8];memcpy(Ops,Source.Ops,6*sizeof(*Ops));size_t N=6;uint32 Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX;
    if(!Live)Ops[N++]=(xmqttstoredelta){.Type=XMQTT_STORE_END,.Epoch=1,.Expiry=Expiry,.WallMilliseconds=2000};
    Ops[N++]=(xmqttstoredelta){.Type=XMQTT_STORE_OPEN,.Flags=2,.Epoch=1,.Expiry=Expiry,.WallMilliseconds=Live?1000000:3000};
    xmqttjournal* J=stWrite(F,V,Ops,N,true);xmqttstorestateconfig C=stConfig(V);C.WallMilliseconds=1000001;xmqttstorestate* S=NULL;
    stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"authenticated resumed OPEN preserves logical epoch and old QoS stage");xmqttstorestateinfo I=stInfo(S);testRequire(I.Epoch==1&&I.LiveAtCut&&I.Outgoing==2&&I.RemainingMilliseconds==UINT64_MAX,"live resumed candidate does not guess its next disconnect time");
    testRequire(stView(S,XMQTT_STORE_OUTGOING,0).Key==11&&stView(S,XMQTT_STORE_OUTGOING,1).Key==12&&stView(S,XMQTT_STORE_OUTGOING,1).Pubrel,"resume preserves send order and PUBREL obligation");testRequire(xmqttJournalDestroy(J),"resume journal released");xmqttStoreStateDestroy(S);jpFileClose(F);
}
static void siEarlyAck(xmqttversion V)
{
    size_t PendingBytes=0;
    for(unsigned Tail=0;Tail<3;++Tail){memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttstoredelta Ops[12];memcpy(Ops,Source.Ops,9*sizeof(*Ops));size_t N=9;
        if(Tail)Ops[N++]=(xmqttstoredelta){.Type=XMQTT_STORE_INBOX_ACK,.Epoch=1,.Key=21};
        if(Tail==2){Ops[N++]=Source.Ops[10];Ops[N++]=Source.Ops[11];}
        xmqttjournal* J=stWrite(F,V,Ops,N,true);xmqttstorestateconfig C=stConfig(V);xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"early application ACK independent of receive protocol completion");xmqttstorestateinfo I=stInfo(S);
        testRequire(I.Inbox==(!Tail?1u:0u)&&I.Incoming==(Tail<2?1u:0u),"application and receive obligations counted independently");
        if(!Tail)PendingBytes=I.MemoryBytes;else if(Tail==1)testRequire(I.MemoryBytes==PendingBytes&&!stView(S,XMQTT_STORE_INCOMING,0).InboxPending,"early ACK cannot physically release active receive identity/payload allocation");
        else testRequire(I.MemoryBytes==I.BaseBytes+I.ClientId.Size,"both obligations completed: retained entry physically freed");
        testRequire(xmqttJournalDestroy(J),"early ACK journal released");xmqttStoreStateDestroy(S);jpFileClose(F);}
}
static void siQos1(void)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(XMQTT_VERSION_5,&Source);xmqttstoredelta Ops[4]={Source.Ops[0],Source.Ops[1]};
    for(unsigned I=2;I<4;++I)Ops[I]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=I,.Identifier=7,.Data={Source.Packets.Wire[0],Source.Packets.Sizes[0]}};
    xmqttjournal* J=stWrite(F,XMQTT_VERSION_5,Ops,4,true);xmqttstorestateconfig C=stConfig(XMQTT_VERSION_5);xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"QoS1 reused receive id creates distinct accepted deliveries");testRequire(stInfo(S).Inbox==2&&!stInfo(S).Incoming&&stView(S,XMQTT_STORE_INBOX,0).Key==2&&stView(S,XMQTT_STORE_INBOX,1).Key==3,"QoS1 has no false QoS2 dedup identity");testRequire(xmqttJournalDestroy(J),"QoS1 journal released");xmqttStoreStateDestroy(S);jpFileClose(F);
}
int main(void)
{siAssigned();for(unsigned I=0;I<2;++I){xmqttversion V=I?XMQTT_VERSION_5:XMQTT_VERSION_311;siResume(V,false);siResume(V,true);siEarlyAck(V);}siQos1();puts("[PASS] assigned identity/version refusal,4 resumed epochs,6 early application-ACK cuts and independent QoS1 deliveries");return 0;}
