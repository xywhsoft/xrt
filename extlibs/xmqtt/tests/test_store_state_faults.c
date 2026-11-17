#include "support/store_state_peer.h"
static jpdrive Drive;
static void sfReject(xmqttversion V,xmqttstoredelta* Ops,size_t Count,xmqttstorestatus Want)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);xmqttjournal* J=stWrite(F,V,Ops,Count,false);xmqttstorestateconfig C=stConfig(V);xmqttstorestate* S=(xmqttstorestate*)(uintptr_t)1;
    stStatus(xmqttStoreStateRecover(J,&C,&S),Want,"invalid complete committed history fails closed");testRequire(S==(xmqttstorestate*)(uintptr_t)1,"no early state publication");xrtClearError();testRequire(xmqttJournalDestroy(J),"failed recovery journal released");jpFileClose(F);
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
static void sfLimits(xmqttversion V)
{
    for(unsigned Mode=0;Mode<13;++Mode){memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttjournal* J=stWrite(F,V,Source.Ops,16,true);xmqttstorestateconfig C=stConfig(V);xmqttstorestatus Want=XMQTT_STORE_LIMIT;
        switch(Mode){case 0:C.MaxOutgoing=0;break;case 1:C.MaxIncoming=0;break;case 2:C.MaxInbox=0;break;case 3:C.MaxMemoryBytes=0;break;
        case 4:C.MaxTransactions=15;break;case 5:C.MaxOperations=15;break;case 6:C.MaxReplayBytes=31;break;case 7:C.Delta.MaxOperations=0;break;case 8:C.Delta.Codec.MaxProperties=0;if(V==XMQTT_VERSION_311)C.Delta.Codec.MaxPayloadBytes=0;break;
        case 9:C.WallMilliseconds=1000;Want=XMQTT_STORE_STATE_ERROR;break;case 10:C.Generation=8;Want=XMQTT_STORE_STATE_ERROR;break;case 11:C.Binding=XRT_BYTES_LITERAL("wrong");Want=XMQTT_STORE_STATE_ERROR;break;default:C.ClientId=XRT_STR_LITERAL("wrong");Want=XMQTT_STORE_STATE_ERROR;break;}
        xmqttstorestate* S=(xmqttstorestate*)(uintptr_t)1;stStatus(xmqttStoreStateRecover(J,&C,&S),Want,"actual count/byte/work/property/identity limits");testRequire(S==(xmqttstorestate*)(uintptr_t)1,"refusal does not publish state");xrtClearError();testRequire(xmqttJournalDestroy(J),"refused journal released");jpFileClose(F);}
}
static void sfLateDamage(xmqttversion V)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttjournal* J=stWrite(F,V,Source.Ops,16,true);testRequire(jpReplay(J)==16,"consume full prefix only for writer setup");
    uint64 Seq=0;jpStatus(xmqttJournalAppend(J,XRT_BYTES_LITERAL("not a delta"),&Seq),XMQTT_JOURNAL_DONE,"late CRC-valid semantic corruption");testRequire(xmqttJournalDestroy(J),"late writer released");xmqttjournalconfig JC=stJournalConfig(V);J=jpReopen(F,&JC);xmqttstorestateconfig C=stConfig(V);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(C.MaxMemoryBytes,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"late-failure ledger");xmqttstorestate* S=(xmqttstorestate*)(uintptr_t)1;
    stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_CORRUPT,"late semantic failure never exposes previously valid recovered payloads");testRequire(S==(xmqttstorestate*)(uintptr_t)1,"complete prefix required");xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"late ledger leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.LiveBytes&&!I.LiveCount,"all staged earlier payloads freed after late failure");xrtAllocationBudgetFree(Budget);testRequire(xmqttJournalDestroy(J),"late recovery journal released");jpFileClose(F);
}
static void sfAliases(void)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(XMQTT_VERSION_5,&Source);xmqttjournal* J=stWrite(F,XMQTT_VERSION_5,Source.Ops,10,false);xmqttstorestateconfig C=stConfig(XMQTT_VERSION_5);
    stStatus(xmqttStoreStateRecover(J,&C,(xmqttstorestate**)&C),XMQTT_STORE_ERROR,"recover output cannot overwrite config");xrtClearError();xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"alias refusal did not consume journal");xmqttstoreview V=stView(S,XMQTT_STORE_INBOX,0);
    testRequire(!xmqttStoreStateInfo(S,(xmqttstorestateinfo*)(ptr)V.Wire.Data),"info cannot overwrite owned wire, even short wire");xrtClearError();stStatus(xmqttStoreStateView(S,XMQTT_STORE_INBOX,0,(xmqttstoreview*)(ptr)stInfo(S).ClientId.Data),XMQTT_STORE_ERROR,"view cannot overwrite identity");xrtClearError();
    xmqttstorestate* Sentinel=(xmqttstorestate*)(uintptr_t)1;stStatus(xmqttStoreStateRecover(J,&C,&Sentinel),XMQTT_STORE_STATE_ERROR,"already consumed cursor cannot publish false empty recovery");testRequire(Sentinel==(xmqttstorestate*)(uintptr_t)1,"consumed cursor output unchanged");xrtClearError();testRequire(xmqttJournalDestroy(J),"alias journal released");xmqttStoreStateDestroy(S);jpFileClose(F);
}
int main(void)
{for(unsigned I=0;I<2;++I){xmqttversion V=I?XMQTT_VERSION_5:XMQTT_VERSION_311;sfTransitions(V);sfLimits(V);sfLateDamage(V);}sfAliases();puts("[PASS]44 illegal histories,26 resource/identity limits,late-prefix atomicity and aliases");return 0;}
