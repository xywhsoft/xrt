#include "support/store_write_peer.h"
static void swBasic(xmqttversion Version)
{
    jpdrive D={0};jpfile File=jpFile(&D);xmqttjournalconfig JC=stJournalConfig(Version);xmqttjournal* J=jpCreate(File,&JC);
    xmqttstorestateconfig SC=stConfig(Version);xmqttstorestate* State=NULL;stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"empty typed READY recovery");
    xtaskpool* Pool=jwPool(2);xmqttstorewriteconfig C;testRequire(xmqttStoreWriteConfigInit(&C),"writer defaults");xmqttstorewrite* W=NULL;testRequire(xmqttStoreWriteCreate(Pool,J,State,&C,&W),"consume matching recovered state and journal");
    xmqttstorestateinfo SI;testRequire(!xmqttStoreStateInfo(State,&SI),"claimed State alias refuses Info");xmqttStoreStateDestroy(State);xrtClearError();
    sdsource S;sdSource(Version,&S);for(size_t I=0;I<SD_COUNT;++I)if(S.Ops[I].WallMilliseconds)S.Ops[I].WallMilliseconds+=100000;
    xmqttstoredeltaconfig DC=sdConfig(Version);
    xmqttstoredelta Bad=S.Ops[0];Bad.Data=XRT_BYTES_LITERAL("wrong-client");unsigned Calls=D.Calls;xfuture* F=swOps(W,&DC,&Bad,1);const xmqttcommitresult* R=jwOutcome(F);
    testRequire(R->Status==XMQTT_COMMIT_INVALID&&!R->AppendCalled&&R->Effect==XMQTT_JOURNAL_NOT_COMMITTED&&D.Calls==Calls&&!swInfo(W).Worker.Failed&&!swInfo(W).State.Transactions,"invalid identity batch has no IO/model change and owner remains usable");xrtFutureDestroy(F);
    for(size_t I=0;I<SD_COUNT;++I){
        if(I==2){xmqttstoredelta B[2]={S.Ops[2],S.Ops[2]};B[1].Key=12;Calls=D.Calls;F=swOps(W,&DC,B,2);R=jwOutcome(F);xmqttstorewriteinfo Info=swInfo(W);
            testRequire(R->Status==XMQTT_COMMIT_INVALID&&!R->AppendCalled&&D.Calls==Calls&&!Info.State.LastKey&&!Info.State.Outgoing&&Info.State.Transactions==2&&!Info.Worker.Failed,"late invalid operation rolls back earlier staged payload/id/key");xrtFutureDestroy(F);}
        F=swOps(W,&DC,S.Ops+I,1);R=jwOutcome(F);xmqttstorewriteinfo Info=swInfo(W);
        testRequire(R->Status==XMQTT_COMMIT_OK&&R->Effect==XMQTT_JOURNAL_COMMITTED&&R->Sequence==I+1&&Info.State.Transactions==I+1&&Info.State.Operations==I+1,"model installed before resolved commit Future");
        if(I==8){unsigned char Buffer[SD_WIRE];xmqttstoreview V={.Key=999};size_t Need=999;stStatus(xmqttStoreWriteRead(W,21,NULL,0,&V,&Need),XMQTT_STORE_LIMIT,"committed size query");testRequire(V.Key==999&&Need==S.Sizes[2],"limit exposes only Required");stStatus(xmqttStoreWriteRead(W,21,Buffer,sizeof(Buffer),&V,&Need),XMQTT_STORE_DONE,"copy committed Inbox");testRequire(V.Key==21&&V.InboxPending&&V.Wire.Data==Buffer&&Need==S.Sizes[2]&&!memcmp(Buffer,S.Wire[2],Need),"read uses only caller storage");}
        if(I+1<SD_COUNT)xrtFutureDestroy(F);
    }
    testRequire(!swInfo(W).State.Outgoing&&!swInfo(W).State.Incoming&&!swInfo(W).State.Inbox,"all legal transitions applied and Inbox ACK remains independent");
    testRequire(xmqttStoreWriteClose(W)&&xmqttStoreWriteWaitFor(W,15000)==XWAIT_OK&&xmqttStoreWriteDestroy(W),"physical writer destruction");jwPoolDone(Pool);
    J=jpReopen(File,&JC);SC.WallMilliseconds=200000;State=NULL;stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"fresh persisted model recovery");SI=stInfo(State);testRequire(SI.Transactions==SD_COUNT&&SI.Operations==SD_COUNT&&SI.LastKey==22&&!SI.Outgoing&&!SI.Incoming&&!SI.Inbox,"complete persisted model matches online result");xmqttStoreStateDestroy(State);testRequire(xmqttJournalDestroy(J),"fresh reader journal");jpFileClose(File);
    testRequire(jwOutcome(F)==R&&R->Sequence==SD_COUNT,"immutable result outlives model,worker,Journal,file and pool");xrtFutureDestroy(F);
}
int main(void){swBasic(XMQTT_VERSION_5);swBasic(XMQTT_VERSION_311);puts("[PASS] real semantic preflight,late invalid-batch rollback,all12 transitions,model-before-Future and fresh typed recovery on both versions");return 0;}
