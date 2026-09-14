#ifdef TASK_POOL_RESOURCE_TAIL_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>

typedef struct ResourceTailCase {xtaskpool* pool;xfile native;xfuture* closed;} ResourceTailCase;
static xtaskoutcome resource_tail(xcancel* cancel,ptr data,xtaskvalue* result)
{
    ResourceTailCase* test=data;(void)cancel;(void)result;
    xdeadline deadline=xrtDeadlineAfter(5000000);xtaskpoolstats stats={0};
    do{assert(xrtTaskPoolGet(test->pool,&stats));if(stats.Closed)break;
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}while(true);
    /* Pool Destroy stops ordinary task admission, but accepted task code and
     * its existing native-resource cleanup are still allowed to finish. */
    xasyncfile* file=xrtAsyncFileAdopt(test->pool,test->native);
    if(!file){fprintf(stderr,"REGRESSION: pool retirement rejected a native resource in accepted task cleanup\n");exit(3);}
    test->native=NULL;test->closed=xrtAsyncFileClose(file);assert(test->closed);
    return XTASK_SUCCESS;
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"enable accepted resource tail accounting");
    for(unsigned i=0;i<40;++i){
        xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
        ResourceTailCase test={0};xtaskpoolconfig config={2,2,0};test.pool=xrtTaskPoolCreate(&config);assert(test.pool);
        str path=NULL;test.native=xrtFileTemp(NULL,"xlir_pool_tail_",".tmp",&path);assert(test.native&&path);
        xfuture* task=xrtTaskSubmit(test.pool,resource_tail,&test,NULL);assert(task);
        xdeadline deadline=xrtDeadlineAfter(5000000);
        while(!xrtTaskPoolDestroy(test.pool)){
            /* An overlapping native stats entry may conservatively refuse
             * Destroy before it closes admission; the owner remains intact. */
            xrtClearError();assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
        }
        assert(!test.native&&test.closed&&xrtFutureState(task)==XFUTURE_RESOLVED&&xrtFutureState(test.closed)==XFUTURE_RESOLVED);
        xrtFutureDestroy(task);xrtFutureDestroy(test.closed);assert(xrtFileDelete(path));xrtFree(path);
        xrtClearError();xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
            before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
            before.UseAfterFreeCount==after.UseAfterFreeCount);
    }
    printf("Native pool accepted resource tail: 40 real files adopted and asynchronously finalized after task admission closed, all original tasks completed and worker joins balanced\n");
    return 0;
}
