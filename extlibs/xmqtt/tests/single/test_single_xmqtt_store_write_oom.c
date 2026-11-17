#define XMQTT_MODULE_XMQTT_STORE_WRITE
#define XRT_MODULE_VFS
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../test.h"
static xtaskpool* soPool;static xallocationbudget* soBudget;static bool soArmed,soConstructor;static unsigned soTarget,soCount;
static ptr soAllocate(size_t N,size_t Size,bool Zero)
{
    bool Track=soArmed&&(soConstructor||xrtTaskPoolIsCurrentWorkerV1(soPool));xallocationscope Scope={0};
    if(Track){testRequire(xrtAllocationBudgetEnter(soBudget,&Scope),"actual per-call native allocation scope");++soCount;
        if(soTarget&&soCount==soTarget){xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(N&&Size&&xrtAllocationBudgetInfo(soBudget,&I)&&xrtAllocationBudgetSetLimit(soBudget,I.LiveBytes+N*Size-1),"real prefix capacity rejection");}}
    ptr P=Zero?xrtCalloc(N,Size):xrtMalloc(N);
    if(Track){testRequire(xrtAllocationBudgetSetLimit(soBudget,1048576)&&xrtAllocationBudgetLeave(&Scope),"restore actual native allocation scope");}return P;
}
static ptr soMalloc(size_t N){return soAllocate(N,1,false);}
static ptr soCalloc(size_t N,size_t Size){return soAllocate(N,Size,true);}
#define xrtMalloc soMalloc
#define xrtCalloc soCalloc
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtMalloc
#undef xrtCalloc
#include "../support/store_write_peer.h"
static void soPrefixes(bool Constructor)
{
    unsigned Total=0;
    for(unsigned Target=0;Target<=Total;++Target){
        jpdrive Drive={0};jpfile F=jpFile(&Drive);xmqttjournalconfig JC=stJournalConfig(XMQTT_VERSION_5);xmqttjournal* J=jpCreate(F,&JC);xmqttstorestateconfig SC=stConfig(XMQTT_VERSION_5);SC.WallMilliseconds=0;xmqttstorestate* State=NULL;stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"empty OOM prefix state");
        soPool=jwPool(1);soTarget=Target;soCount=0;soConstructor=Constructor;testRequire(xrtAllocationBudgetCreate(1048576,&soBudget),"real allocation ledger");xmqttstorewriteconfig WC;xmqttStoreWriteConfigInit(&WC);xmqttstorewrite* W=(xmqttstorewrite*)(uintptr_t)1;unsigned Calls=Drive.Calls;soArmed=Constructor;
        bool Created=xmqttStoreWriteCreate(soPool,J,State,&WC,&W);xfuture* Future=NULL;
        if(Constructor){soArmed=false;
            if(!Target){testRequire(Created,"constructor control");Total=soCount;testRequire(Total>=3&&Total<16,"all constructor extension allocation edges observed");}
            else {testRequire(!Created&&W==(xmqttstorewrite*)(uintptr_t)1&&soCount==Target,"failed constructor publishes nothing");testRequire(stInfo(State).Transactions==0&&jpInfo(J).State==XMQTT_JOURNAL_READY,"both source owners retained on every constructor prefix");xmqttStoreStateDestroy(State);testRequire(xmqttJournalDestroy(J),"unconsumed journal released");}
        }else {
            testRequire(Created,"native preflight owner");stsource Source;stSource(XMQTT_VERSION_5,&Source);unsigned char Buffer[SD_BUFFER];xmqttproperty Props[4];size_t N=0;sdStatus(xmqttStoreDeltaWrite(&SC.Delta,Source.Ops,16,Props,4,Buffer,sizeof(Buffer),&N),XMQTT_STORE_DELTA_DONE,"typed OOM transaction");soArmed=true;Future=swAppend(W,(xbytesview){Buffer,N});const xmqttcommitresult* R=jwOutcome(Future);soArmed=false;
            if(!Target){testRequire(R->Status==XMQTT_COMMIT_OK,"native preflight control");Total=soCount;testRequire(Total>=6&&Total<16,"all metadata/identity/new-payload allocation edges observed");}
            else {xmqttstorewriteinfo I=swInfo(W);testRequire(soCount==Target&&R->Status==XMQTT_COMMIT_INITIALIZATION&&!R->AppendCalled&&R->Effect==XMQTT_JOURNAL_NOT_COMMITTED&&Drive.Calls==Calls&&!I.State.Transactions&&I.Worker.Failed,"every real native allocation failure aborts staged model before IO");}
        }
        if(Created)testRequire(xmqttStoreWriteDestroy(W),"OOM owner physical retirement");
        jwPoolDone(soPool);soPool=NULL;xrtFutureDestroy(Future);xrtClearError();jpFileClose(F);
        xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(soBudget,&I)&&!I.LiveBytes&&!I.LiveCount&&(!Target||I.Rejections),"all actual prefix allocations and diagnostics physically refunded");xrtAllocationBudgetFree(soBudget);soBudget=NULL;
    }
    printf("[PASS] %u actual %s allocation-prefix rejections,unchanged sources/no native IO and physical refunds\n",Total,Constructor?"constructor":"host-worker semantic preparation");
}
int main(void){soPrefixes(true);soPrefixes(false);return 0;}
