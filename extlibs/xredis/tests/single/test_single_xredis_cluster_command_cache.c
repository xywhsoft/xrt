#define XREDIS_TEST_CATALOG_WAIT
#define XREDIS_MODULE_XREDIS_CLUSTER_COMMAND_CACHE
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"

static _Thread_local xallocationbudget* CatalogBudget;
static _Thread_local xallocationscope CatalogScope;
static _Thread_local bool CatalogWaitError;
static xwaitresult cacheWait(xfuture* Future)
{
    if(CatalogWaitError)return XWAIT_ERROR;
    xwaitresult Wait=xrtFutureWait(Future);
    if(CatalogBudget && Wait==XWAIT_OK){
        if(!xrtAllocationBudgetEnter(CatalogBudget,&CatalogScope))abort();
        CatalogBudget=NULL;
    }
    return Wait;
}

#define xrtFutureWait cacheWait
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"
#undef xrtFutureWait
#include "../test_cluster_command_cache.c"
