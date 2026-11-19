#define XREDIS_MODULE_XREDIS_CLUSTER_COMMAND
#define XREDIS_MODULE_XREDIS_CLUSTER_CLIENT_SYNC
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"

static xallocationbudget* AppCatalogBudget;
static xrediskeysstatus appCatalogAllocate(xredisreply* Reply, const xrediscommandkeylimits* Limits,
    xrediscommandcatalog** Catalogue)
{
    if (!AppCatalogBudget) return xredisCommandCatalogCreate(Reply, Limits, Catalogue);
    xallocationscope Scope = {0};
    if (!xrtAllocationBudgetEnter(AppCatalogBudget, &Scope)) abort();
    AppCatalogBudget = NULL;
    xrediskeysstatus Status = xredisCommandCatalogCreate(Reply, Limits, Catalogue);
    if (!xrtAllocationBudgetLeave(&Scope)) abort();
    return Status;
}

#define XREDIS_TEST_APP_CATALOG_PARSE
#define xredisCommandCatalogCreate appCatalogAllocate
#include "../test_cluster_command_cache.c"
