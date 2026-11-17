#include "support/journal_peer.h"
#include <xmqtt/journal_worker.h>
static const xmqttcommitresult* jwResult(xfuture* F)
{testRequire(F&&xrtFutureWaitFor(F,10000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED,"physical typed journal completion");return xrtFutureValue(F);}
static xmqttjournalworkerinfo jwInfo(xmqttjournalworker* W)
{xmqttjournalworkerinfo I={0};testRequire(xmqttJournalWorkerInfo(W,&I),"worker info snapshot");return I;}
static void jwBasic(xmqttversion V)
{
    jpdrive Drive={0};jpfile File=jpFile(&Drive);xmqttjournalconfig JC=jpConfig();JC.Version=V;xmqttjournal* J=jpCreate(File,&JC);xtaskpoolconfig PC={.Threads=2,.QueueLimit=8};xtaskpool* Pool=xrtTaskPoolCreate(&PC);testRequire(Pool!=NULL,"borrowed real host pool");
    xmqttjournalworkerconfig C;testRequire(xmqttJournalWorkerConfigInit(&C),"journal worker defaults");C.MaxObjects=4;C.MaxRequests=4;C.MaxTasks=8;xmqttjournalworker* W=NULL;testRequire(xmqttJournalWorkerCreate(Pool,J,&C,&W),"exclusive Journal transfer");
    xmqttjournalinfo I;uint64 Seq=99;xmqttjournalrecord Record;testRequire(!xmqttJournalInfo(J,&I)&&!xmqttJournalDestroy(J),"transferred alias cannot inspect or destroy Journal");jpStatus(xmqttJournalNext(J,&Record),XMQTT_JOURNAL_STATE,"transferred alias cannot replay");jpStatus(xmqttJournalAppend(J,XRT_BYTES_LITERAL("x"),&Seq),XMQTT_JOURNAL_STATE,"transferred alias cannot append");testRequire(Seq==99,"alias output unchanged");xrtClearError();
    xfuture* F[3]={0};for(unsigned N=0;N<3;++N){unsigned char B[3]={(unsigned char)N,0,127};testRequire(xmqttJournalWorkerAppendAsync(W,(xbytesview){B,3},10000,&F[N])==XMQTT_JWORK_ACCEPTED,"bounded immutable admission");memset(B,0xcc,sizeof(B));}
    const xmqttcommitresult* R[3];for(unsigned N=0;N<3;++N){R[N]=jwResult(F[N]);testRequire(R[N]->Status==XMQTT_COMMIT_OK&&R[N]->AppendStatus==XMQTT_JOURNAL_DONE&&R[N]->Effect==XMQTT_JOURNAL_COMMITTED&&R[N]->Sequence==N+1&&R[N]->AttemptSequence==N+1&&R[N]->Data.Size==3&&R[N]->Data.Data[0]==N&&!R[N]->Cause,"ordered native append and exact source-independent result");}
    testRequire(!jwInfo(W).Requests&&jwInfo(W).Objects==3&&jwInfo(W).Accepted==3&&jwInfo(W).Completed==3,"physical retained results remain charged");testRequire(xmqttJournalWorkerClose(W)&&xmqttJournalWorkerWaitFor(W,10000)==XWAIT_OK&&xmqttJournalWorkerDestroy(W),"actual extension task tails retire");
    testRequire(xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK&&xrtTaskPoolDestroy(Pool),"borrowed pool remains host owned");J=jpReopen(File,&JC);testRequire(jpReplay(J)==3&&xmqttJournalDestroy(J),"recover exact committed sequence after worker/pool retirement");jpFileClose(File);
    for(unsigned N=0;N<3;++N){testRequire(R[N]==xrtFutureValue(F[N])&&R[N]->Data.Data[0]==N&&R[N]->Sequence==N+1,"result survives creator,Journal,file and host pool");xrtFutureDestroy(F[N]);}
}
int main(void){jwBasic(XMQTT_VERSION_5);jwBasic(XMQTT_VERSION_311);puts("[PASS] real host pool serialized commit,exclusive Journal,immutable retained results and physical joins on both versions");return 0;}
