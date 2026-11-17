#define XMQTT_MODULE_XMQTT_STORE_WRITE
#define XRT_MODULE_VFS
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../support/store_write_peer.h"
/* Exercise the public backing allocator, including its real native frees.
 * The private single implementation identifies exactly the retained payload's
 * backing allocation; it does not replace an allocation or force a NULL. */
static xmqttstorewrite* Tracked;
static xtaskpool* TrackedPool;
static ptr Target;
static size_t Retained;
static uint64 ExpectedTransaction;
static xatomic32 Tracking,Active,Calls,TargetCalls;
static _Thread_local bool Inside;
static ptr allocfn(ptr C,size_t N){(void)C;return malloc(N);}
static ptr reallocfn(ptr C,ptr P,size_t N){(void)C;return realloc(P,N);}
static void freefn(ptr C,ptr P)
{
    (void)C;
    if(!Inside&&xrtAtomic32Load(&Tracking,XMEMORY_SEQ_CST)){
        xrtAtomic32FetchAdd(&Active,1,XMEMORY_SEQ_CST);
        if(xrtAtomic32Load(&Tracking,XMEMORY_SEQ_CST)&&xrtTaskPoolIsCurrentWorkerV1(TrackedPool)){
            Inside=true;
            testRequire(xrtMutexTryLock(&Tracked->Gate),"actual backing Free occurs outside nonrecursive model gate");
            testRequire(xrtMutexUnlock(&Tracked->Gate),"model gate probe unlock");
            xmqttstorewriteinfo Info=swInfo(Tracked);
            xrtAtomic32FetchAdd(&Calls,1,XMEMORY_SEQ_CST);
            if(P==Target){
                testRequire(Info.State.Transactions==ExpectedTransaction&&Info.State.Inbox==0,
                    "free callback reads the complete newly committed metadata");
                testRequire(Info.State.MemoryBytes>=Retained,"payload credits remain charged until backing Free returns");
                testRequire(xmqttStoreWriteClose(Tracked),"late Close from physical retirement callback");
                xrtAtomic32FetchAdd(&TargetCalls,1,XMEMORY_SEQ_CST);
            }
            Inside=false;
        }
        xrtAtomic32FetchSub(&Active,1,XMEMORY_SEQ_CST);
    }
    free(P);
}
static void arm(void){xrtAtomic32Store(&Tracking,1,XMEMORY_SEQ_CST);}
static void disarm(void)
{
    xrtAtomic32Store(&Tracking,0,XMEMORY_SEQ_CST);
    while(xrtAtomic32Load(&Active,XMEMORY_SEQ_CST))xrtSleep(1);
}
static void run(xmqttversion V)
{
    jpdrive D={0};jpfile F=jpFile(&D);xmqttjournalconfig JC=stJournalConfig(V);
    xmqttjournal* J=jpCreate(F,&JC);xmqttstorestateconfig SC=stConfig(V);SC.WallMilliseconds=0;
    xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&SC,&S),XMQTT_STORE_DONE,"empty source");
    TrackedPool=jwPool(1);xmqttstorewriteconfig C;xmqttStoreWriteConfigInit(&C);
    testRequire(xmqttStoreWriteCreate(TrackedPool,J,S,&C,&Tracked),"semantic owner");
    sdsource Source;sdSource(V,&Source);Target=NULL;arm();
    xfuture* Future=swOps(Tracked,&SC.Delta,Source.Ops,16);
    testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"full atomic batch");disarm();xrtFutureDestroy(Future);
    testRequire(xrtAtomic32Load(&Calls,XMEMORY_SEQ_CST)>0,"metadata plan invokes an actual backing Free");
    xmqttstoredelta Open={.Type=XMQTT_STORE_OPEN,.Epoch=2,.WallMilliseconds=2002,.Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX};
    Future=swOps(Tracked,&SC.Delta,&Open,1);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"new live epoch");xrtFutureDestroy(Future);
    unsigned char Payload[2048]={0},Wire[2200];size_t N=0;xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);
    Q.Body.Publish.Topic=XRT_STR_LITERAL("allocator");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    testRequire(V==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&Q,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),&Q,XMQTT_SERVER_TO_CLIENT,NULL,&N),"large canonical retained payload");
    xmqttstoredelta Accept={.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=2,.Key=23,.Data={Wire,N}};
    Future=swOps(Tracked,&SC.Delta,&Accept,1);testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"retain actual large entry");xrtFutureDestroy(Future);
    double End=xrtTimer()+10;while(swInfo(Tracked).Worker.Tasks){testRequire(xrtTimer()<End,"retire extension native tail before identifying retained allocation");xrtSleep(1);}
    xmqttstoreentry* E=xmqttSFindBase(Tracked->State,23);testRequire(E&&E->Allocation>2048,"large retained entry located");
    Target=__xrtHeapHeader(E)->Allocation;Retained=swInfo(Tracked).State.MemoryBytes;ExpectedTransaction=4;
    unsigned Before=xrtAtomic32Load(&TargetCalls,XMEMORY_SEQ_CST);arm();
    xmqttstoredelta Ack={.Type=XMQTT_STORE_INBOX_ACK,.Epoch=2,.Key=23};Future=swOps(Tracked,&SC.Delta,&Ack,1);
    const xmqttcommitresult* Result=jwOutcome(Future);
    testRequire(Result->Status==XMQTT_COMMIT_CLOSED&&Result->Effect==XMQTT_JOURNAL_COMMITTED&&Result->Sequence==ExpectedTransaction,"late retirement Close sampled before resolution without losing COMMITTED");disarm();
    testRequire(xrtAtomic32Load(&TargetCalls,XMEMORY_SEQ_CST)==Before+1&&swInfo(Tracked).State.MemoryBytes<Retained,
        "one actual payload backing Free precedes credit refund and result publication");
    xrtFutureDestroy(Future);testRequire(xmqttStoreWriteDestroy(Tracked),"serialized owner destruction");
    jwPoolDone(TrackedPool);jpFileClose(F);Tracked=NULL;TrackedPool=NULL;
}
int main(void)
{
    xallocator A={NULL,allocfn,reallocfn,freefn};testRequire(xrtSetAllocator(&A),"install public allocator before first raw allocation");
    run(XMQTT_VERSION_5);run(XMQTT_VERSION_311);
    puts("[PASS] real public allocator Free reentry reads complete committed state outside model gate; retired payload charged until physical free, both MQTT versions");return 0;
}
