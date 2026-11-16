#include "support/journal_peer.h"
static jpdrive Drive,Base;
static void jfPut(unsigned char* P,uint64 V,unsigned N)
{for(unsigned I=N;I;--I){P[I-1]=(unsigned char)V;V>>=8;}}
static void jfSeal(unsigned char* P){jfPut(P+4092,xrtCrc32(P,4092),4);}
static xmqttjournalstatus jfRecover(jpfile F,const xmqttjournalconfig* C,unsigned* Count)
{
    xmqttjournal* J=(xmqttjournal*)(uintptr_t)1;xmqttjournalstatus S=xmqttJournalOpen(F.File,C,&J);*Count=0;
    if(S!=XMQTT_JOURNAL_DONE){testRequire(J==(xmqttjournal*)(uintptr_t)1,"failed open output atomic");xrtClearError();return S;}
    xmqttjournalrecord R={UINT64_MAX,{0}};
    while((S=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE){++*Count;testRequire(R.Sequence==*Count,"fault replay monotonic identity");}
    if(S<0){xmqttjournalrecord Before=R;jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_STATE,"failed recovery poisons owner");testRequire(!memcmp(&Before,&R,sizeof(R)),"poisoned replay output untouched");}
    testRequire(xmqttJournalDestroy(J),"fault recovery owner cleanup");xrtClearError();return S;
}
static void jfBaseline(jpfile F,xmqttjournalconfig* C,bool Two)
{memset(&Drive,0,sizeof(Drive));xmqttjournal* J=jpCreate(F,C);jpAppend(J,"first",1);if(Two)jpAppend(J,"second",2);testRequire(xmqttJournalDestroy(J),"baseline owner stop");Base=Drive;}
static void jfCut(jpfile F,xmqttjournalconfig* C)
{
    jfBaseline(F,C,false);unsigned Cases=0;
    const size_t Partials[]={0,1,7,47,48,63,4095,4096};
    for(unsigned Fail=1;Fail<=7;++Fail)for(unsigned P=0;P<sizeof(Partials)/sizeof(*Partials);++P)
    for(unsigned Persist=0;Persist<2;++Persist)for(unsigned Cache=0;Cache<2;++Cache){
        Drive=Base;xmqttjournal* J=jpReopen(F,C);testRequire(jpReplay(J)==1,"failure old prefix validated");Drive.Calls=0;Drive.Fail=Fail;Drive.Partial=Partials[P];Drive.FlushOnFailure=Persist!=0;
        uint64 Seq=777;xrtSetErrorInfo(XERR_STATE,"old.ambient",91,"not a backend cause");
        jpStatus(xmqttJournalAppend(J,XRT_BYTES_LITERAL("second"),&Seq),Fail<=4?XMQTT_JOURNAL_IO:XMQTT_JOURNAL_UNCERTAIN,"every append write/sync boundary");
        xmqttjournalinfo I=jpInfo(J);testRequire(Seq==777&&I.Sequence==1&&I.State==XMQTT_JOURNAL_POISONED&&I.AttemptSequence==2&&I.Effect==(Fail<=4?XMQTT_JOURNAL_NOT_COMMITTED:XMQTT_JOURNAL_MAY_BE_COMMITTED),"failure uncertainty and output/cache duties exact");
        testRequire(!xrtErrorFind(xrtGetError(),"old.ambient",91),"unrelated ambient error not used as I/O cause");
        jpStatus(xmqttJournalAppend(J,XRT_BYTES_LITERAL("retry"),&Seq),XMQTT_JOURNAL_STATE,"cannot retry an uncertain physical write");testRequire(xmqttJournalDestroy(J),"failed owner released without closing file");xrtClearError();Drive.Fail=0;Drive.Calls=0;
        if(!Cache){memcpy(Drive.Bytes,Drive.Stable,Drive.StableSize);Drive.Size=Drive.StableSize;}
        unsigned Count=0;jpStatus(jfRecover(F,C,&Count),XMQTT_JOURNAL_EMPTY,"cut recovers only verified committed prefix");testRequire(Count>=1&&Count<=2&&(Fail>4||Count==1),"no acknowledged transaction loss; no definite-failure publication");
        ++Cases;
    }
    printf("[CUTS] %u partial/failure/sync/cache persistence models\n",Cases);
    Drive=Base;xmqttjournal* J=jpReopen(F,C);testRequire(jpReplay(J)==1,"zero-write prefix");Drive.Zero=true;uint64 Seq=33;jpStatus(xmqttJournalAppend(J,XRT_BYTES_LITERAL("x"),&Seq),XMQTT_JOURNAL_IO,"successful zero write cannot loop/hang");testRequire(Seq==33&&xmqttJournalDestroy(J),"zero-write owner cleanup");Drive.Zero=false;xrtClearError();
}
static void jfCorruption(jpfile F,xmqttjournalconfig* C)
{
    jfBaseline(F,C,true);size_t N=Base.Size;unsigned Count=0;
    for(size_t At=0;At<N;++At){Drive=Base;Drive.Bytes[At]^=1;
        xmqttjournalstatus S=jfRecover(F,C,&Count);
        if(At>=4096&&At<12288){jpStatus(S,XMQTT_JOURNAL_EMPTY,"one damaged witness retains latest acknowledged commit");testRequire(Count==2,"dual latest witnesses never downgrade acknowledged history");}
        else jpStatus(S,XMQTT_JOURNAL_CORRUPT,"every committed bit fault is fatal outside redundant witness");
    }
    for(size_t At=0;At<N;++At){Drive=Base;Drive.Size=At;jpStatus(jfRecover(F,C,&Count),XMQTT_JOURNAL_CORRUPT,"every truncated committed file is fatal; no older-root fallback");}
    Drive=Base;Drive.Bytes[4096]^=1;Drive.Bytes[8192]^=1;jpStatus(jfRecover(F,C,&Count),XMQTT_JOURNAL_CORRUPT,"both witness corrupt");
    Drive=Base;jfPut(Drive.Bytes+4096+24,N-1,8);jfSeal(Drive.Bytes+4096);jpStatus(jfRecover(F,C,&Count),XMQTT_JOURNAL_CORRUPT,"same-sequence conflicting end even with valid CRC");
    Drive=Base;jfPut(Drive.Bytes+4096+16,0,8);jfPut(Drive.Bytes+4096+24,12288,8);jfPut(Drive.Bytes+4096+32,0,4);jfSeal(Drive.Bytes+4096);jpStatus(jfRecover(F,C,&Count),XMQTT_JOURNAL_CORRUPT,"nonadjacent publication generations");
    for(unsigned Field=0;Field<8;++Field){Drive=Base;unsigned char* H=Drive.Bytes+12288;
        switch(Field){case 0:jfPut(H+8,UINT64_MAX,8);break;case 1:jfPut(H+24,UINT64_MAX,8);break;
            case 2:jfPut(H+16,0,8);break;case 3:jfPut(H+32,1,4);break;case 4:H[40]=1;break;
            case 5:H[36]^=1;break;case 6:Drive.Bytes[12288+69-4]=1;break;
            default:for(unsigned Page=1;Page<=2;++Page){Drive.Bytes[Page*4096+32]^=1;jfSeal(Drive.Bytes+Page*4096);}break;}
        jfPut(H+44,xrtCrc32(H,44),4);jpStatus(jfRecover(F,C,&Count),XMQTT_JOURNAL_CORRUPT,"CRC-valid arithmetic/sequence/chain/reserved-field faults never accepted");
    }
    Drive=Base;memset(Drive.Bytes+Drive.Size,0xee,100);Drive.Size+=100;unsigned Calls=Drive.Calls;
    xmqttjournal* J=jpReopen(F,C);xmqttjournalrecord R;for(unsigned I=0;I<2;++I)jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"committed prefix before suffix removal");testRequire(Drive.Size==N+100,"suffix remains until ALL committed data validated");jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_EMPTY,"proven uncommitted suffix retired");testRequire(Drive.Size==N&&Drive.Calls==Calls+2&&Drive.StableSize==N,"suffix truncate followed by native barrier");testRequire(xmqttJournalDestroy(J),"suffix owner cleanup");
    for(unsigned Fail=1;Fail<=2;++Fail){Drive=Base;Drive.Size+=100;J=jpReopen(F,C);for(unsigned I=0;I<2;++I)jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"retirement failure prefix");Drive.Calls=0;Drive.Fail=Fail;jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_IO,"suffix resize/flush failure does not advertise readiness");testRequire(jpInfo(J).State==XMQTT_JOURNAL_POISONED&&xmqttJournalDestroy(J),"failed suffix retirement poisoned");xrtClearError();}
    printf("[CORRUPTION] %zu bit faults plus %zu truncations and CRC-valid structural/witness faults\n",N,N);
}
static void jfLimits(jpfile F,xmqttjournalconfig C)
{
    Drive=Base;unsigned Count=0;xmqttjournalconfig X=C;X.Generation++;jpStatus(jfRecover(F,&X,&Count),XMQTT_JOURNAL_CORRUPT,"wrong generation");X=C;X.Version=XMQTT_VERSION_311;jpStatus(jfRecover(F,&X,&Count),XMQTT_JOURNAL_CORRUPT,"wrong MQTT level");X=C;X.Binding=XRT_BYTES_LITERAL("other");jpStatus(jfRecover(F,&X,&Count),XMQTT_JOURNAL_CORRUPT,"wrong storage identity");X=C;X.PayloadSchema++;jpStatus(jfRecover(F,&X,&Count),XMQTT_JOURNAL_CORRUPT,"wrong transaction schema");X=C;X.MaxFileBytes=12288;jpStatus(jfRecover(F,&X,&Count),XMQTT_JOURNAL_LIMIT,"bounded physical log including suffix");
    X=C;X.MaxTransactionBytes=4;xmqttjournal* J=jpReopen(F,&X);xmqttjournalrecord R={99,{0}};unsigned Calls=Drive.Calls;jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_LIMIT,"bounded replay buffer");testRequire(R.Sequence==99&&jpInfo(J).State==XMQTT_JOURNAL_RECOVERING&&Drive.Calls==Calls,"replay LIMIT atomic cursor/output, no writes");testRequire(xmqttJournalDestroy(J),"small replay buffer released");
    J=jpReopen(F,&C);testRequire(jpReplay(J)==2,"quota baseline validated");size_t Memory=jpInfo(J).MemoryBytes;testRequire(xmqttJournalDestroy(J),"quota baseline owner freed");
    size_t Limits[]={1,Memory-1,Memory,Memory+1};
    for(unsigned I=0;I<4;++I){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[I],&B)&&xrtAllocationBudgetEnter(B,&Scope),"constructor allocation failure scope");J=(xmqttjournal*)(uintptr_t)1;xmqttjournalstatus S=xmqttJournalOpen(F.File,&C,&J);jpStatus(S,I<2?XMQTT_JOURNAL_ERROR:XMQTT_JOURNAL_DONE,"one exact upfront allocation boundary");if(I<2)testRequire(J==(xmqttjournal*)(uintptr_t)1,"OOM constructor output unchanged");else {testRequire(jpReplay(J)==2,"allocation-bounded replay");testRequire(xmqttJournalDestroy(J),"OOM control owner released");}xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"OOM scope leave");xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(B,&BI)&&!BI.LiveCount&&!BI.LiveBytes,"all physical allocations refunded after every constructor path");xrtAllocationBudgetFree(B);}
    X=C;X.MaxMemoryBytes=Memory-1;J=(xmqttjournal*)(uintptr_t)1;jpStatus(xmqttJournalOpen(F.File,&X,&J),XMQTT_JOURNAL_LIMIT,"memory preflight is typed normal limit");testRequire(J==(xmqttjournal*)(uintptr_t)1,"preflight unchanged output");
    for(unsigned Fail=1;Fail<=5;++Fail){memset(&Drive,0,sizeof(Drive));Drive.Fail=Fail;J=(xmqttjournal*)(uintptr_t)1;jpStatus(xmqttJournalCreate(F.File,&C,&J),XMQTT_JOURNAL_IO,"each create write/resize/flush failure");testRequire(J==(xmqttjournal*)(uintptr_t)1,"failed initialization atomic owner publication");xrtClearError();}
}
int main(void)
{jpfile F=jpFile(&Drive);xmqttjournalconfig C=jpConfig();jfCut(F,&C);jfCorruption(F,&C);jfLimits(F,C);jpFileClose(F);puts("[PASS] journal partial I/O/sync uncertainty, atomic replay, strict committed corruption, bounded/OOM/cleanup");return 0;}
