#ifdef TASK_POLICY_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>
typedef struct PolicyJob { unsigned kind; bool destroyed; } PolicyJob;
static unsigned drops, mismatches;
static void drop_value(ptr value,ptr data)
{
    xrtownershipscope freeze={0}; xdeadline deadline=xrtDeadlineAfter(1000000);
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(xrtOwnershipScopeEnd(&freeze));
    if(data){assert(!((PolicyJob*)data)->destroyed);++mismatches;}
    ++drops;xrtFree(value);
}
static void wrong_drop(ptr value,ptr data)
{ assert(data && !((PolicyJob*)data)->destroyed);++mismatches;++drops;xrtFree(value); }
static bool trace_value(const void* value,const void* data,xrtownershipvisitor visit,ptr context)
{ (void)value;(void)context;return data==NULL && visit!=NULL; }
static const xfuturepayloadownershipv1 policy={sizeof(policy),drop_value,trace_value};
static const xfuturepayloadownershipv1* const allowed[]={&policy};
static xtaskoutcome run(xcancel* cancel,ptr data,xtaskvalue* result)
{
    PolicyJob* job=data;(void)cancel;assert(!job->destroyed);
    if(job->kind==3)return XTASK_SUCCESS; /* Native void: no owned result. */
    result->Value=xrtMalloc(8);assert(result->Value);result->Destroy=job->kind==1?wrong_drop:drop_value;
    result->DestroyData=job->kind==1 || job->kind==2?job:NULL;
    return XTASK_SUCCESS;
}
static void destroy_job(ptr data,ptr unused)
{ PolicyJob* job=data;(void)unused;assert(!job->destroyed);job->destroyed=true; }
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"enable task policy accounting");
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xtaskpoolconfig config={0};config.Threads=1;xtaskpool* pool=xrtTaskPoolCreate(&config);assert(pool);
    for(unsigned round=0;round<50;++round)for(unsigned kind=0;kind<4;++kind){
        PolicyJob job={kind,false};xtaskargs args={0};args.Destroy=destroy_job;
        xfuture* future=xrtTaskSubmitOwnedPolicyV1(pool,run,&job,&args,&policy);assert(future);
        assert(xrtFutureWaitFor(future,1000000)==XWAIT_OK && job.destroyed);
        xfutureresult result={0};assert(xrtFutureResult(future,&result));
        assert(result.State==(kind==1 || kind==2?XFUTURE_FAILED:XFUTURE_RESOLVED));
        if(kind==1 || kind==2)assert(xrtErrorKind(result.Error)==XERR_STATE);
        /* Worker/native job tail can briefly be active after publication.
         * Hold an actual owner until the test's own inspection freeze. */
        xrtownershipscope freeze={0};
        while(!xrtOwnershipFreezeTryBegin(&freeze))xrtThreadYield();
        assert(xrtFutureOwnershipAdapterV1(xrtFutureOwnership(future),allowed,1));
        assert(xrtOwnershipScopeEnd(&freeze));xrtFutureDestroy(future);
    }
    xrtTaskPoolDestroy(pool);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(drops==150 && mismatches==100 && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    puts("Task result policy: 200 native jobs, 100 pre-publication mismatches, 150 exactly-once drops; owned and void outcomes, terminal certification and zero live delta");
    return 0;
}
