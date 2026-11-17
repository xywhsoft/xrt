#include "support/store_write_peer.h"
static void swBatch(xmqttversion V,unsigned Prefix)
{
    jpdrive D={0};jpfile F=jpFile(&D);stsource O;stSource(V,&O);xmqttjournalconfig JC=stJournalConfig(V);
    xmqttjournal* J=Prefix?stWrite(F,V,O.Ops,Prefix,false):jpCreate(F,&JC);xmqttstorestateconfig SC=stConfig(V);xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"transaction starting prefix");xtaskpool* Pool=jwPool(2);xmqttstorewriteconfig WC;xmqttStoreWriteConfigInit(&WC);xmqttstorewrite* W=NULL;testRequire(xmqttStoreWriteCreate(Pool,J,State,&WC,&W),"online transaction owner");xmqttstoredeltaconfig DC=sdConfig(V);xfuture* Future;
    if(!Prefix){
        for(size_t I=0;I<ST_COUNT;++I)if(O.Ops[I].WallMilliseconds)O.Ops[I].WallMilliseconds+=100000;
        Future=swOps(W,&DC,O.Ops,ST_COUNT);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"one transaction creates,mutates,completes,resets and acknowledges new payloads");xrtFutureDestroy(Future);
        xmqttstorewriteinfo I=swInfo(W);testRequire(I.State.Transactions==1&&I.State.Operations==ST_COUNT&&I.State.Epoch==2&&I.State.LastKey==23&&!I.State.Outgoing&&!I.State.Incoming&&!I.State.Inbox,"full-batch overlay key/id/accounting after repeated reset");
    }else {
        xmqttstoredelta Flow={.Type=Prefix==5?XMQTT_STORE_OUT_ATTEMPT:XMQTT_STORE_IN_PUBREL,.Epoch=1,.Key=Prefix==5?11:21,.Identifier=Prefix==5?7:17};unsigned Calls=D.Calls;
        Future=swOps(W,&DC,&Flow,1);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_INVALID&&D.Calls==Calls,"recovery live cut requires fresh OPEN before protocol flow");xrtFutureDestroy(Future);
        xmqttstoredelta Open={.Type=XMQTT_STORE_OPEN,.Flags=2,.Epoch=1,.Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX,.WallMilliseconds=100000};Future=swOps(W,&DC,&Open,1);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"actual persisted resumed OPEN");xrtFutureDestroy(Future);
        xmqttstorewriteinfo Before=swInfo(W);
        if(Prefix==5){
            xmqttstoredelta Ops[5]={{.Type=XMQTT_STORE_END,.Epoch=1,.WallMilliseconds=100001,.Expiry=Open.Expiry},{.Type=XMQTT_STORE_CLEAR,.Epoch=1,.WallMilliseconds=100002},{.Type=XMQTT_STORE_OPEN,.Epoch=2,.WallMilliseconds=100003,.Expiry=Open.Expiry},O.Ops[2],O.Ops[2]};Ops[3].Epoch=Ops[4].Epoch=2;Ops[3].Key=13;Ops[4].Key=14;
            Calls=D.Calls;Future=swOps(W,&DC,Ops,5);const xmqttcommitresult* R=jwOutcome(Future);xmqttstorewriteinfo After=swInfo(W);testRequire(R->Status==XMQTT_COMMIT_INVALID&&!R->AppendCalled&&D.Calls==Calls&&After.State.Epoch==Before.State.Epoch&&After.State.LastKey==Before.State.LastKey&&After.State.Outgoing==2&&After.State.Transactions==Before.State.Transactions,"late invalid batch preserves original two outgoing records after staged reset and id reuse");xrtFutureDestroy(Future);
            Future=swOps(W,&DC,Ops,4);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"reset clears base ids before reusing same id in new epoch");xrtFutureDestroy(Future);After=swInfo(W);testRequire(After.State.Epoch==2&&After.State.LastKey==13&&After.State.Outgoing==1,"atomic replacement model");
            unsigned char B[SD_WIRE];xmqttstoreview View;size_t N=0;stStatus(xmqttStoreWriteRead(W,11,B,sizeof(B),&View,&N),XMQTT_STORE_EMPTY,"old base key removed");stStatus(xmqttStoreWriteRead(W,13,B,sizeof(B),&View,&N),XMQTT_STORE_DONE,"new reused identifier owned wire");testRequire(View.Epoch==2&&View.Identifier==7&&!memcmp(B,O.Packets.Wire[0],N),"new-key payload preserved across metadata overlay");
        }else {
            xmqttstoredelta Bad={.Type=XMQTT_STORE_IN_COMPLETE,.Epoch=1,.Key=21,.Identifier=17};Calls=D.Calls;Future=swOps(W,&DC,&Bad,1);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_INVALID&&D.Calls==Calls&&swInfo(W).State.Incoming==1,"invalid touched existing receive state unchanged");xrtFutureDestroy(Future);
            xmqttstoredelta Ops[3]={{.Type=XMQTT_STORE_IN_PUBREL,.Epoch=1,.Key=21,.Identifier=17},Bad,{.Type=XMQTT_STORE_INBOX_ACK,.Epoch=1,.Key=21}};Future=swOps(W,&DC,Ops,3);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"complete and ACK existing receive in one transaction");xrtFutureDestroy(Future);testRequire(!swInfo(W).State.Incoming&&swInfo(W).State.Inbox==1,"independent second Inbox remains");
        }
    }
    xmqttstorestateinfo Expected=swInfo(W).State;testRequire(xmqttStoreWriteDestroy(W),"transaction writer physically retired");jwPoolDone(Pool);J=jpReopen(F,&JC);State=NULL;SC.WallMilliseconds=Expected.LastWallMilliseconds;stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"fresh entire typed prefix");xmqttstorestateinfo Actual=stInfo(State);
    testRequire(Actual.Transactions==Expected.Transactions&&Actual.Operations==Expected.Operations&&Actual.ReplayBytes==Expected.ReplayBytes&&Actual.Epoch==Expected.Epoch&&Actual.LastKey==Expected.LastKey&&Actual.Outgoing==Expected.Outgoing&&Actual.Incoming==Expected.Incoming&&Actual.Inbox==Expected.Inbox,"online committed state agrees with independent full replay");xmqttStoreStateDestroy(State);testRequire(xmqttJournalDestroy(J),"reader retirement");jpFileClose(F);
}
int main(void){for(unsigned I=0;I<2;++I){xmqttversion V=I?XMQTT_VERSION_311:XMQTT_VERSION_5;swBatch(V,0);swBatch(V,5);swBatch(V,10);}puts("[PASS] six complete/partial-prefix transaction cases,recovery OPEN gate,late-reset rollback,existing/new id reuse and fresh full replay equivalence");return 0;}
