#ifdef TASK_POOL_NATIVE_ENTRY_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>

/* Deliberately uses only the old public API. The identical regression can be
 * linked against the pre-ownership pool implementation to prove the lifetime
 * defect without substituting a mock pool or relying on the new adapter. */
typedef struct NativeEntryCase {xtaskpool* pool;unsigned runs,drops;} NativeEntryCase;
static xtaskoutcome entry_run(xcancel* cancel,ptr data,xtaskvalue* result)
{NativeEntryCase* test=data;(void)cancel;(void)result;++test->runs;return XTASK_SUCCESS;}
static void entry_drop(ptr value,ptr data)
{
    NativeEntryCase* test=value;assert(!data&&!test->runs&&!test->drops);
    /* The accepted pre-cancelled Job is being retired on the Submit caller's
     * stack, before that call updates pool statistics. Freeing here is UAF. */
    bool destroyed=xrtTaskPoolDestroy(test->pool);
    if(destroyed){fprintf(stderr,"REGRESSION: Destroy retired a pool inside its live Submit cleanup\n");exit(3);}
    xrtClearError();xtaskpoolstats stats={0};assert(xrtTaskPoolGet(test->pool,&stats)&&!stats.Closed);
    ++test->drops;
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"enable native entry accounting");
    for(unsigned round=0;round<100;++round){
        xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
        NativeEntryCase test={0};xtaskpoolconfig config={1,2,0};test.pool=xrtTaskPoolCreate(&config);assert(test.pool);
        xcancel* cancel=xrtCancelCreate();assert(cancel&&xrtCancelRequest(cancel));
        xtaskargs args={cancel,entry_drop,NULL};xfuture* cancelled=xrtTaskSubmit(test.pool,entry_run,&test,&args);
        assert(cancelled&&test.drops==1&&!test.runs&&xrtFutureState(cancelled)==XFUTURE_CANCELLED);
        xfuture* accepted=xrtTaskSubmit(test.pool,entry_run,&test,NULL);assert(accepted);
        assert(xrtTaskPoolDestroy(test.pool)&&test.runs==1);
        assert(xrtFutureState(accepted)==XFUTURE_RESOLVED);
        xrtFutureDestroy(accepted);xrtFutureDestroy(cancelled);xrtCancelDestroy(cancel);
        xrtClearError();xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
            before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
            before.UseAfterFreeCount==after.UseAfterFreeCount);
    }
    printf("Native pool entry regression: 100 pre-cancelled Submit cleanup reentries refused without closing; 100 subsequent tasks completed; exact allocation/count/bytes balance\n");
    return 0;
}
