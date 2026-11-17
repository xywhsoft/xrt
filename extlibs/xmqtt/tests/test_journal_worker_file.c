#include "support/journal_worker_peer.h"
#include "support/store_state_peer.h"
static jwdrive Drive;static jpdrive Baseline;
static unsigned wfRead(xfile F,xmqttversion V)
{xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=NULL;jpStatus(xmqttJournalOpen(F,&JC,&J),XMQTT_JOURNAL_DONE,"fresh worker-recovery journal");xmqttstorestateconfig C=stConfig(V);xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"actual committed typed state after async commit cut");unsigned N=(unsigned)stInfo(S).Operations;xmqttStoreStateDestroy(S);testRequire(xmqttJournalDestroy(J),"fresh typed reader retired");return N;}
static size_t wfBatch(xmqttversion V,unsigned char* Buffer,size_t Size)
{stsource Source;stSource(V,&Source);xmqttstoredeltaconfig C=sdConfig(V);xmqttproperty P[4];size_t N=0;sdStatus(xmqttStoreDeltaWrite(&C,Source.Ops,16,P,4,Buffer,Size,&N),XMQTT_STORE_DELTA_DONE,"legal MQTT-aware worker batch");return N;}
static void wfCuts(xmqttversion V)
{
    memset(&Drive,0,sizeof(Drive));jpfile F=jwFile(&Drive);xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=jpCreate(F,&JC);testRequire(xmqttJournalDestroy(J),"native effect baseline");Baseline=Drive.Base;unsigned char Buffer[SD_BUFFER];size_t N=wfBatch(V,Buffer,sizeof(Buffer));xtaskpool* P=jwPool(2);const size_t Partial[]={0,1,7,47,48,63,4095,4096};unsigned Cases=0;
    for(unsigned Point=1;Point<=7;++Point)for(unsigned K=0;K<8;++K)for(unsigned Persist=0;Persist<2;++Persist)for(unsigned Cache=0;Cache<2;++Cache){
        Drive.Base=Baseline;J=jpReopen(F,&JC);testRequire(!jpReplay(J),"writer original empty prefix proven");Drive.Base.Calls=0;Drive.Base.Fail=Point;Drive.Base.Partial=Partial[K];Drive.Base.FlushOnFailure=Persist!=0;xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);xmqttjournalworker* W=NULL;testRequire(xmqttJournalWorkerCreate(P,J,&C,&W),"fault worker takes Journal");xfuture* Future=NULL;
        testRequire(xmqttJournalWorkerAppendAsync(W,(xbytesview){Buffer,N},10000,&Future)==XMQTT_JWORK_ACCEPTED,"real async append before native failure");const xmqttcommitresult* R=jwOutcome(Future);
        testRequire(R->Status==XMQTT_COMMIT_STORAGE&&R->AppendStatus==(Point<=4?XMQTT_JOURNAL_IO:XMQTT_JOURNAL_UNCERTAIN)&&R->Effect==(Point<=4?XMQTT_JOURNAL_NOT_COMMITTED:XMQTT_JOURNAL_MAY_BE_COMMITTED)&&!R->Sequence&&R->AttemptSequence==1&&R->AppendCalled,"exact physical native effect retained independently from local result");testRequire(jwSnapshot(W).Failed&&xmqttJournalWorkerDestroy(W),"poisoned worker retires physically before source resolution");xrtFutureDestroy(Future);xrtClearError();Drive.Base.Fail=0;
        if(!Cache){Drive.Base.Size=Drive.Base.StableSize;memcpy(Drive.Base.Bytes,Drive.Base.Stable,Drive.Base.Size);}unsigned Count=wfRead(F.File,V);testRequire((Count==0||Count==16)&&(Point>4||!Count),"fresh replay resolves uncertainty,never partial MQTT state");++Cases;
    }
    jwPoolDone(P);jpFileClose(F);printf("[PASS] %u asynchronous native storage-failure models with fresh typed QoS/Inbox recovery,version%d\n",Cases,V);
}
static int wfNative(int Argc,char** Argv)
{
    bool Read=!strcmp(Argv[1],"--read"),Crash=!strcmp(Argv[1],"--crash");testRequire(Argc==(Read||Crash?5:4),"worker native CLI arguments");xmqttversion V=!strcmp(Argv[Argc-1],"5")?XMQTT_VERSION_5:XMQTT_VERSION_311;xfile File=xrtOpen(Argv[2],XFILE_READ|XFILE_WRITE|(Read?0:XFILE_CREATE|XFILE_TRUNCATE));testRequire(File!=NULL,"actual native commit/recovery file");
    if(Read){unsigned N=wfRead(File,V);testRequire(N==(unsigned)atoi(Argv[3]),"fresh-process MQTT state cardinality");printf("[RECOVERED] %u\n",N);}
    else {testRequire(Crash||!strcmp(Argv[1],"--write"),"worker native writer mode");xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=NULL;jpStatus(xmqttJournalCreate(File,&JC,&J),XMQTT_JOURNAL_DONE,"actual file initializer");testRequire(xmqttJournalDestroy(J),"initializer retired");memset(&Drive,0,sizeof(Drive));Drive.Base.Native=File;jpfile F=jwFile(&Drive);J=jpReopen(F,&JC);testRequire(!jpReplay(J),"native writer recovered");Drive.Base.Calls=0;if(Crash)Drive.Base.KillAt=(unsigned)atoi(Argv[3]);unsigned char Buffer[SD_BUFFER];size_t N=wfBatch(V,Buffer,sizeof(Buffer));xtaskpool* P=jwPool(2);xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);xmqttjournalworker* W=NULL;testRequire(xmqttJournalWorkerCreate(P,J,&C,&W),"native pool writer takes Journal");xfuture* Future=NULL;testRequire(xmqttJournalWorkerAppendAsync(W,(xbytesview){Buffer,N},10000,&Future)==XMQTT_JWORK_ACCEPTED,"actual asynchronous file write");const xmqttcommitresult* R=jwOutcome(Future);testRequire(R->Status==XMQTT_COMMIT_OK&&R->Effect==XMQTT_JOURNAL_COMMITTED&&R->Sequence==1,"actual file two sync barriers completed");testRequire(xmqttJournalWorkerDestroy(W),"native file writer physical retirement");jwPoolDone(P);xrtFutureDestroy(Future);jpFileClose(F);}
    testRequire(xrtClose(File),"actual native file owner closes");puts("[PASS] actual file async commit and fresh recovered state");return 0;
}
int main(int Argc,char** Argv){if(Argc>1)return wfNative(Argc,Argv);wfCuts(XMQTT_VERSION_5);wfCuts(XMQTT_VERSION_311);return 0;}
