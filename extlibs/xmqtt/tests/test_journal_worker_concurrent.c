#include "support/journal_worker_peer.h"
#include <xrt/thread.h>
typedef struct wcproducer {xmqttjournalworker* W;unsigned Id;xatomic32* Start;} wcproducer;
static int32 wcRun(ptr Data)
{
    wcproducer* P=Data;jwAwait(P->Start);
    for(unsigned N=0;N<32;++N){unsigned char B[2]={(unsigned char)P->Id,(unsigned char)N};xfuture* F=NULL;double End=xrtTimer()+15;
        for(;;){xmqttjournaladmission A=xmqttJournalWorkerAppendAsync(P->W,(xbytesview){B,2},10000,&F);if(A==XMQTT_JWORK_ACCEPTED)break;testRequire(A==XMQTT_JWORK_OVERLOADED&&xrtTimer()<End&&!F,"bounded contention retries before successful admission");xrtSleep(1);}
        memset(B,0xcc,sizeof(B));const xmqttcommitresult* R=jwOutcome(F);testRequire(R->Status==XMQTT_COMMIT_OK&&R->Effect==XMQTT_JOURNAL_COMMITTED&&R->Data.Data[0]==P->Id&&R->Data.Data[1]==N,"parallel producer immutable input/result");xrtFutureDestroy(F);}
    return 0;
}
int main(void)
{
    jwdrive D={0};jpfile F=jwFile(&D);xtaskpool* P=jwPool(4);xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);C.MaxRequests=16;C.MaxObjects=16;xmqttjournalworker* W=jwCreate(F,&D,P,&C);xatomic32 Start={0};wcproducer Producers[8];xthread* Threads[8];
    for(unsigned N=0;N<8;++N){Producers[N]=(wcproducer){W,N,&Start};Threads[N]=xrtThreadCreate(wcRun,Producers+N,0);testRequire(Threads[N]!=NULL,"native producer thread");}xrtAtomic32Store(&Start,1,XMEMORY_RELEASE);
    for(unsigned N=0;N<8;++N){testRequire(xrtThreadWaitFor(Threads[N],15000)==XWAIT_OK&&!xrtThreadExitCode(Threads[N]),"actual producer join");xrtThreadDestroy(Threads[N]);}
    testRequire(xmqttJournalWorkerClose(W)&&xmqttJournalWorkerWaitFor(W,15000)==XWAIT_OK,"all native task tails joined");xmqttjournalworkerinfo I=jwSnapshot(W);testRequire(I.Accepted==256&&I.Completed==256&&!I.Requests&&!I.Objects&&!I.Tasks&&I.Bytes==I.BaseBytes&&I.Journal.Sequence==256&&xrtAtomic32Load(&D.MaxActive,XMEMORY_ACQUIRE)==1,"bounded owner ledger fully refunds despite concurrent submissions/results/reapers");testRequire(xmqttJournalWorkerDestroy(W),"parallel owner retired");jwPoolDone(P);
    xmqttjournalconfig JC=jpConfig();xmqttjournal* J=jpReopen(F,&JC);bool Seen[8][32]={0};xmqttjournalrecord R;unsigned Count=0;xmqttjournalstatus S;
    while((S=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE){testRequire(R.Data.Size==2&&R.Data.Data[0]<8&&R.Data.Data[1]<32&&!Seen[R.Data.Data[0]][R.Data.Data[1]],"fresh replay has every producer operation once");Seen[R.Data.Data[0]][R.Data.Data[1]]=true;++Count;}testRequire(S==XMQTT_JOURNAL_EMPTY&&Count==256&&xmqttJournalDestroy(J),"no lost/duplicated/mis-sequenced commits");jpFileClose(F);puts("[PASS]8 real producer threads,4 host workers,256 exact commits,concurrent retirement reapers and full credit refund");return 0;
}
