#include "support/store_delta_peer.h"
#include "support/journal_peer.h"
static jpdrive Drive,Base;
static xmqttjournalconfig sjConfig(xmqttversion V)
{xmqttjournalconfig C=jpConfig();C.Version=V;C.Generation=9;C.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;C.Binding=XRT_BYTES_LITERAL("delta/broker/principal/slot");return C;}
static unsigned sjRead(xfile F,xmqttversion V)
{
    xmqttjournalconfig JCfg=sjConfig(V);xmqttstoredeltaconfig C=sdConfig(V);xmqttjournal* J=NULL;jpStatus(xmqttJournalOpen(F,&JCfg,&J),XMQTT_JOURNAL_DONE,"fresh typed journal open");unsigned Count=0;xmqttjournalrecord R;xmqttjournalstatus S;
    while((S=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE){testRequire(R.Sequence==1&&!Count,"exact single atomic group");sdCheck(&C,R.Data);Count=SD_COUNT;}
    jpStatus(S,XMQTT_JOURNAL_EMPTY,"entire committed prefix verified before recovered readiness");testRequire(xmqttJournalDestroy(J),"fresh typed reader released");return Count;
}
static void sjCuts(xmqttversion V)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);xmqttjournalconfig JCfg=sjConfig(V);xmqttstoredeltaconfig C=sdConfig(V);sdsource Source;sdSource(V,&Source);unsigned char B[SD_BUFFER];size_t N=sdWrite(&C,&Source,B,sizeof(B));xmqttjournal* J=jpCreate(F,&JCfg);testRequire(xmqttJournalDestroy(J),"empty durable envelope baseline");Base=Drive;
    const size_t Partial[]={0,1,7,47,48,63,4095,4096};unsigned Cases=0;
    for(unsigned Fail=1;Fail<=7;++Fail)for(unsigned P=0;P<8;++P)for(unsigned Persist=0;Persist<2;++Persist)for(unsigned Cache=0;Cache<2;++Cache){
        Drive=Base;J=jpReopen(F,&JCfg);testRequire(jpReplay(J)==0,"no earlier group");Drive.Calls=0;Drive.Fail=Fail;Drive.Partial=Partial[P];Drive.FlushOnFailure=Persist!=0;uint64 Seq=999;
        jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),Fail<=4?XMQTT_JOURNAL_IO:XMQTT_JOURNAL_UNCERTAIN,"group data/witness/sync failure");testRequire(Seq==999&&xmqttJournalDestroy(J),"failed group never acknowledges success");xrtClearError();Drive.Fail=0;if(!Cache){Drive.Size=Drive.StableSize;memcpy(Drive.Bytes,Drive.Stable,Drive.Size);}
        unsigned Count=sjRead(F.File,V);testRequire((Count==0||Count==SD_COUNT)&&(Fail>4||!Count),"recovery exposes zero or ALL16 typed operations, never an intermediate prefix");++Cases;
    }
    Drive=Base;J=jpReopen(F,&JCfg);testRequire(jpReplay(J)==0,"fully committed group baseline");uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),XMQTT_JOURNAL_DONE,"one16-operation group shares both native sync barriers");testRequire(Seq==1&&xmqttJournalDestroy(J),"group writer released");testRequire(sjRead(F.File,V)==SD_COUNT,"all group bytes and full-topic data exact");
    xmqttjournalconfig Wrong=JCfg;Wrong.PayloadSchema=1;J=(xmqttjournal*)(uintptr_t)1;jpStatus(xmqttJournalOpen(F.File,&Wrong,&J),XMQTT_JOURNAL_CORRUPT,"opaque journal schema cannot be confused with MQTT typed schema");testRequire(J==(xmqttjournal*)(uintptr_t)1,"wrong-schema output unchanged");xrtClearError();
    Drive=Base;J=jpReopen(F,&JCfg);testRequire(jpReplay(J)==0,"CRC-valid semantic failure baseline");B[35]=1;jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),XMQTT_JOURNAL_DONE,"journal can checksum opaque malformed typed body");testRequire(xmqttJournalDestroy(J),"bad typed writer released");J=jpReopen(F,&JCfg);xmqttjournalrecord R;jpStatus(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"journal CRC alone does not prove MQTT delta semantics");xmqttstoredelta O[SD_COUNT];xmqttproperty Props[4];xmqttstoredeltadocument D={(xmqttstoredelta*)(uintptr_t)1,777,{0}};
    sdStatus(xmqttStoreDeltaRead(&C,R.Data,O,SD_COUNT,Props,4,&D),XMQTT_STORE_DELTA_MALFORMED,"semantic framing rejected despite valid enclosing journal CRC");testRequire(D.Count==777&&xmqttJournalDestroy(J),"bad typed batch never publishes a document");xrtClearError();jpFileClose(F);printf("[GROUP-CUTS] version%d %u models, atomic0-or16 records; no live state/ACK application claim\n",V,Cases);
}
static int sjNative(int Argc,char** Argv)
{
    bool Read=!strcmp(Argv[1],"--read"),Crash=!strcmp(Argv[1],"--crash");testRequire(Argc==(Read||Crash?5:4),"native typed transaction CLI");xmqttversion V=!strcmp(Argv[Argc-1],"5")?XMQTT_VERSION_5:XMQTT_VERSION_311;
    xfile File=xrtOpen(Argv[2],XFILE_READ|XFILE_WRITE|(Read?0:XFILE_CREATE|XFILE_TRUNCATE));testRequire(File!=NULL,"actual native typed journal file");
    if(Read){unsigned Count=sjRead(File,V);testRequire(Count==(unsigned)atoi(Argv[3]),"fresh process group cardinality");printf("[RECOVERED] %u\n",Count);}
    else {testRequire(Crash||!strcmp(Argv[1],"--write"),"native writer/crash mode");xmqttjournalconfig JC=sjConfig(V);xmqttjournal* J=NULL;jpStatus(xmqttJournalCreate(File,&JC,&J),XMQTT_JOURNAL_DONE,"native empty typed store envelope");testRequire(xmqttJournalDestroy(J),"native initializer released");
        xmqttstoredeltaconfig C=sdConfig(V);sdsource Source;sdSource(V,&Source);unsigned char B[SD_BUFFER];size_t N=sdWrite(&C,&Source,B,sizeof(B));
        if(Crash){memset(&Drive,0,sizeof(Drive));Drive.Native=File;jpfile F=jpFile(&Drive);J=jpReopen(F,&JC);testRequire(jpReplay(J)==0,"native group pre-recovery");Drive.Calls=0;Drive.KillAt=(unsigned)atoi(Argv[3]);uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),XMQTT_JOURNAL_DONE,"native all-record group commit");testRequire(xmqttJournalDestroy(J),"normal crash-child finish");jpFileClose(F);}
        else {jpStatus(xmqttJournalOpen(File,&JC,&J),XMQTT_JOURNAL_DONE,"native writer reopened");testRequire(jpReplay(J)==0,"native writer ready");uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),XMQTT_JOURNAL_DONE,"native group synced");memset(&Source,0xcc,sizeof(Source));memset(B,0xcc,sizeof(B));testRequire(xmqttJournalDestroy(J)&&sjRead(File,V)==SD_COUNT,"durable batch owns bytes after source erasure");}}
    testRequire(xrtClose(File),"native borrowed file closed only by owner");puts("[PASS] native typed MQTT journal batch");return 0;
}
int main(int Argc,char** Argv)
{if(Argc>1)return sjNative(Argc,Argv);sjCuts(XMQTT_VERSION_5);sjCuts(XMQTT_VERSION_311);return 0;}
