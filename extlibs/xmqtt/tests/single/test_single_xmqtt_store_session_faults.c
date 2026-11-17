#define XMQTT_MODULE_XMQTT_STORE_SESSION
#define XRT_MODULE_VFS
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static xallocationbudget* ImportBudget;
static bool ImportArmed;
static unsigned ImportAllocation,FailImportAllocation;
static void siBudget(size_t Bytes)
{if(ImportArmed&&++ImportAllocation==FailImportAllocation){xallocationbudgetinfo I={.Size=sizeof(I)};if(!xrtAllocationBudgetInfo(ImportBudget,&I)||!xrtAllocationBudgetSetLimit(ImportBudget,I.LiveBytes+Bytes-1u))abort();}}
static ptr siMalloc(size_t N){siBudget(N);return xrtMalloc(N);}
static ptr siCalloc(size_t N,size_t S){siBudget(N*S);return xrtCalloc(N,S);}
#define xrtMalloc siMalloc
#define xrtCalloc siCalloc
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtMalloc
#undef xrtCalloc
#define main siFaultMain
#include "../test_store_session_faults.c"
#undef main
static void siOOM(xmqttversion V)
{
    xmqttstorestate* State=ssRecover(V,false,60);unsigned Total=0;
    for(unsigned Target=0;Target<=Total;++Target){xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1048576,&ImportBudget)&&xrtAllocationBudgetEnter(ImportBudget,&Scope),"actual import allocation ledger");xmqttsessionconfig K=ssSessionConfig(V);xmqttstoresessionconfig C=ssConfig(10,4000);xmqttsession* S=(xmqttsession*)(uintptr_t)1;
        ImportAllocation=0;FailImportAllocation=Target;ImportArmed=true;xmqttsessionstatus Status=xmqttStoreSessionRestore(State,&K,&C,&S);ImportArmed=false;
        if(!Target){ssStatus(Status,XMQTT_SESSION_DONE,"allocation control");Total=ImportAllocation;testRequire(Total==(V==XMQTT_VERSION_5?5u:4u),"one reusable scratch,one owner,exactly three retained packet allocations;311 needs no scratch");ssClose(S,C.MonotonicTime);}
        else {ssStatus(Status,XMQTT_SESSION_ERROR,"nth real allocator rejection aborts candidate");testRequire(S==(xmqttsession*)(uintptr_t)1&&ImportAllocation==Target,"every actual allocation prefix failed without fake malloc return");}
        xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"import budget leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(ImportBudget,&I)&&!I.LiveBytes&&!I.LiveCount&&(!Target||I.Rejections),"all new owner/scratch/partial packet owners physically refunded");xrtAllocationBudgetFree(ImportBudget);ImportBudget=NULL;
        testRequire(stInfo(State).Outgoing==3&&stInfo(State).Incoming==2&&stInfo(State).Inbox==2,"OOM never consumes recovered obligations");}
    xmqttStoreStateDestroy(State);printf("[PASS] %u actual import allocation-prefix failures with complete refunds,version%d\n",Total,V);
}
int main(void){testRequire(!siFaultMain(),"adversarial control");siOOM(XMQTT_VERSION_5);siOOM(XMQTT_VERSION_311);return 0;}
