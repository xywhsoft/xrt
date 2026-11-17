#include "support/store_state_peer.h"
static jpdrive Drive,Base;
static unsigned scRead(xfile File,xmqttversion V)
{
    xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=NULL;jpStatus(xmqttJournalOpen(File,&JC,&J),XMQTT_JOURNAL_DONE,"fresh complete-prefix reader");xmqttstorestateconfig C=stConfig(V);xmqttstorestate* S=NULL;
    stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"actual protocol/Inbox state application after committed-prefix verification");xmqttstorestateinfo I=stInfo(S);testRequire((!I.Operations&&!I.Inbox&&!I.Outgoing&&!I.Incoming)||
        (I.Operations==16&&I.Inbox==3&&I.Epoch==2&&!I.Outgoing&&!I.Incoming),"zero or entire legal16-operation history, never intermediate state");
    for(size_t N=0;N<I.Inbox;++N){xmqttstoreview W=stView(S,XMQTT_STORE_INBOX,N);testRequire(W.Key==21+N&&W.Epoch==(N==2?2u:1u)&&W.Wire.Size,"old accepted Inbox survives protocol reset at a crash cut");}
    unsigned Count=(unsigned)I.Operations;testRequire(xmqttJournalDestroy(J),"fresh reader released");xmqttStoreStateDestroy(S);return Count;
}
static void scCuts(xmqttversion V)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=jpCreate(F,&JC);testRequire(xmqttJournalDestroy(J),"durable empty baseline");Base=Drive;
    stsource Source;stSource(V,&Source);xmqttstoredeltaconfig C=sdConfig(V);xmqttproperty P[4];unsigned char B[SD_BUFFER];size_t N=0;sdStatus(xmqttStoreDeltaWrite(&C,Source.Ops,16,P,4,B,sizeof(B),&N),XMQTT_STORE_DELTA_DONE,"legal full recovery group");
    const size_t Partial[]={0,1,7,47,48,63,4095,4096};unsigned Cases=0;
    for(unsigned Fail=1;Fail<=7;++Fail)for(unsigned K=0;K<8;++K)for(unsigned Persist=0;Persist<2;++Persist)for(unsigned Cache=0;Cache<2;++Cache){
        Drive=Base;J=jpReopen(F,&JC);testRequire(!jpReplay(J),"empty writer pre-recovery");Drive.Calls=0;Drive.Fail=Fail;Drive.Partial=Partial[K];Drive.FlushOnFailure=Persist!=0;uint64 Seq=999;
        jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),Fail<=4?XMQTT_JOURNAL_IO:XMQTT_JOURNAL_UNCERTAIN,"real partial-write/sync commit cut");testRequire(Seq==999&&xmqttJournalDestroy(J),"failed caller has no commit success");xrtClearError();Drive.Fail=0;
        if(!Cache){Drive.Size=Drive.StableSize;memcpy(Drive.Bytes,Drive.Stable,Drive.Size);}unsigned Count=scRead(F.File,V);testRequire((Count==0||Count==16)&&(Fail>4||!Count),"actual state applier respects journal commit uncertainty");++Cases;
    }
    jpFileClose(F);printf("[PASS] %u storage cuts with actual QoS/Inbox recovery,version%d\n",Cases,V);
}
static int scNative(int Argc,char** Argv)
{
    bool Read=!strcmp(Argv[1],"--read"),Crash=!strcmp(Argv[1],"--crash");testRequire(Argc==(Read||Crash?5:4),"state integration CLI");xmqttversion V=!strcmp(Argv[Argc-1],"5")?XMQTT_VERSION_5:XMQTT_VERSION_311;
    xfile File=xrtOpen(Argv[2],XFILE_READ|XFILE_WRITE|(Read?0:XFILE_CREATE|XFILE_TRUNCATE));testRequire(File!=NULL,"actual native recovery file");
    if(Read){unsigned Count=scRead(File,V);testRequire(Count==(unsigned)atoi(Argv[3]),"fresh process state cardinality");printf("[RECOVERED] %u\n",Count);}
    else {testRequire(Crash||!strcmp(Argv[1],"--write"),"native writer mode");xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=NULL;jpStatus(xmqttJournalCreate(File,&JC,&J),XMQTT_JOURNAL_DONE,"native empty envelope");testRequire(xmqttJournalDestroy(J),"native initializer released");
        stsource Source;stSource(V,&Source);xmqttstoredeltaconfig C=sdConfig(V);xmqttproperty P[4];unsigned char B[SD_BUFFER];size_t N=0;sdStatus(xmqttStoreDeltaWrite(&C,Source.Ops,16,P,4,B,sizeof(B),&N),XMQTT_STORE_DELTA_DONE,"native legal recovery batch");
        memset(&Drive,0,sizeof(Drive));Drive.Native=File;jpfile F=jpFile(&Drive);J=jpReopen(F,&JC);testRequire(!jpReplay(J),"native writer ready");Drive.Calls=0;if(Crash)Drive.KillAt=(unsigned)atoi(Argv[3]);uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),XMQTT_JOURNAL_DONE,"native legal prefix committed");testRequire(xmqttJournalDestroy(J),"native writer released");jpFileClose(F);}
    testRequire(xrtClose(File),"native owner closes file");puts("[PASS] native recovered protocol and pending Inbox state");return 0;
}
int main(int Argc,char** Argv)
{if(Argc>1)return scNative(Argc,Argv);scCuts(XMQTT_VERSION_5);scCuts(XMQTT_VERSION_311);return 0;}
