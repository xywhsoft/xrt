#define XMQTT_MODULE_XMQTT_STORE_STATE
#define XRT_MODULE_VFS
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static xallocationbudget* RecoveryBudget;
static unsigned AllocationCall,FailAllocation;
static bool RecordAllocations;
static void sfBudgetFail(size_t N)
{if(RecordAllocations&&++AllocationCall==FailAllocation){xallocationbudgetinfo I={.Size=sizeof(I)};if(!xrtAllocationBudgetInfo(RecoveryBudget,&I)||!xrtAllocationBudgetSetLimit(RecoveryBudget,I.LiveBytes+N-1u))abort();}}
static ptr sfMalloc(size_t N){sfBudgetFail(N);return xrtMalloc(N);}
static ptr sfCalloc(size_t Count,size_t N){sfBudgetFail(Count*N);return xrtCalloc(Count,N);}
#define xrtMalloc sfMalloc
#define xrtCalloc sfCalloc
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtMalloc
#undef xrtCalloc
#define main sfNormalMain
#include "../test_store_state_faults.c"
#undef main
static void sfAllocationPrefixes(void)
{
    unsigned Total=0;
    for(unsigned Target=0;Target<=Total;++Target){memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(XMQTT_VERSION_5,&Source);xmqttjournal* J=stWrite(F,XMQTT_VERSION_5,Source.Ops,16,true);xmqttstorestateconfig C=stConfig(XMQTT_VERSION_5);
        xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(C.MaxMemoryBytes,&RecoveryBudget)&&xrtAllocationBudgetEnter(RecoveryBudget,&Scope),"actual prefix OOM budget");FailAllocation=Target;AllocationCall=0;RecordAllocations=true;xmqttstorestate* S=(xmqttstorestate*)(uintptr_t)1;xmqttstorestatus Result=xmqttStoreStateRecover(J,&C,&S);RecordAllocations=false;
        if(!Target){stStatus(Result,XMQTT_STORE_DONE,"allocation control");Total=AllocationCall;testRequire(Total>=8,"all real scratch/identity/payload allocation edges observed");xmqttStoreStateDestroy(S);}
        else {stStatus(Result,XMQTT_STORE_ERROR,"actual nth recovery allocation rejected");testRequire(S==(xmqttstorestate*)(uintptr_t)1&&AllocationCall==Target,"every actual allocation failure reached exactly, no fake NULL hook");}
        xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"prefix budget leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(RecoveryBudget,&I)&&!I.LiveBytes&&!I.LiveCount&&(!Target||I.Rejections),"every partial candidate physically refunded after real allocator rejection");xrtAllocationBudgetFree(RecoveryBudget);RecoveryBudget=NULL;testRequire(xmqttJournalDestroy(J),"OOM journal released");jpFileClose(F);}
    printf("[PASS] %u actual recovery allocation-prefix failures with complete refunds\n",Total);
}
int main(void){testRequire(!sfNormalMain(),"adversarial control");sfAllocationPrefixes();return 0;}
