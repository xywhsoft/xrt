#include "support/store_write_peer.h"
static jpdrive Drive;
static void sfReject(xmqttversion V,xmqttstoredelta* Ops,size_t Count,xmqttstorestatus Want)
{
    (void)Want;for(size_t I=0;I<Count;++I)if(Ops[I].WallMilliseconds)Ops[I].WallMilliseconds+=100000;
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=jpCreate(F,&JC);xmqttstorestateconfig C=stConfig(V);xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"empty semantic refusal prefix");xtaskpool* Pool=jwPool(2);xmqttstorewriteconfig WC;xmqttStoreWriteConfigInit(&WC);xmqttstorewrite* W=NULL;testRequire(xmqttStoreWriteCreate(Pool,J,S,&WC,&W),"illegal-history model owner");unsigned Calls=Drive.Calls;xfuture* Future=swOps(W,&C.Delta,Ops,Count);const xmqttcommitresult* R=jwOutcome(Future);xmqttstorewriteinfo Info=swInfo(W);
    testRequire(R->Status==XMQTT_COMMIT_INVALID&&!R->AppendCalled&&R->Effect==XMQTT_JOURNAL_NOT_COMMITTED&&Drive.Calls==Calls&&!Info.State.Transactions&&!Info.State.LastKey&&!Info.State.Outgoing&&!Info.State.Incoming&&!Info.State.Inbox&&!Info.Worker.Failed,"every whole illegal history rejected before IO with original model intact");xrtFutureDestroy(Future);xrtClearError();sdsource Valid;sdSource(V,&Valid);Valid.Ops[0].WallMilliseconds+=100000;Future=swOps(W,&C.Delta,Valid.Ops,1);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"invalid history does not poison later valid BIND");xrtFutureDestroy(Future);testRequire(xmqttStoreWriteDestroy(W),"rejected prefix physical retirement");jwPoolDone(Pool);jpFileClose(F);
}
static void sfTransitions(xmqttversion V)
{
    for(unsigned Mode=0;Mode<22;++Mode){stsource Source;stSource(V,&Source);xmqttstoredelta* O=Source.Ops;size_t Count=ST_COUNT;
        switch(Mode){
        case 0:memmove(O,O+1,2*sizeof(*O));Count=1;break; /* no BIND */
        case 1:O[1]=O[0];Count=2;break;
        case 2:O[1].Flags=2;Count=2;break;
        case 3:O[1].Epoch=2;Count=2;break;
        case 4:O[2]=O[1];O[2].Flags=1;O[2].Expiry=V==XMQTT_VERSION_5?60:0;Count=3;break;
        case 5:O[2]=O[1];O[2].Flags=2;O[2].Epoch=2;Count=3;break;
        case 6:O[8]=O[2];O[8].Key=11;Count=9;break;
        case 7:O[7].Identifier=8;Count=8;break;
        case 8:O[5]=O[6];Count=6;break;
        case 9:O[3]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_PUBREL,.Epoch=1,.Key=11,.Identifier=7};Count=4;break;
        case 10:O[9]=O[8];O[9].Key=22;Count=10;break;
        case 11:O[10]=O[11];Count=11;break;
        case 12:O[13]=O[3];Count=14;break;
        case 13:O[12]=O[13];Count=13;break;
        case 14:O[13].Reason=1;Count=14;break;
        case 15:O[16].Epoch=2;Count=17;break;
        case 16:O[17]=O[16];Count=18;break;
        case 17:O[2].Flags=0;O[3]=O[7];Count=4;break;
        case 18:O[12].WallMilliseconds=999;Count=13;break;
        case 19:O[12].Expiry=V==XMQTT_VERSION_5?61:0;Count=13;break;
        case 20:O[0].Epoch=UINT64_MAX;O[1].Epoch=UINT64_MAX;O[2]=O[1];O[2].Flags=1;O[2].Expiry=V==XMQTT_VERSION_5?60:0;O[2].Epoch=1;Count=3;break;
        default:O[2].Key=UINT64_MAX;O[3].Key=UINT64_MAX;Count=5;break;
        }
        sfReject(V,O,Count,XMQTT_STORE_CORRUPT);
    }
}
int main(void){sfTransitions(XMQTT_VERSION_5);sfTransitions(XMQTT_VERSION_311);puts("[PASS]44 syntactically valid illegal histories rejected before native IO,full rollback and later valid commit");return 0;}
