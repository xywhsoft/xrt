#ifdef NET_ENGINE_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>

typedef struct EngineCase EngineCase;
typedef struct EngineData {
    volatile int32 refs;
    EngineCase* test;
    xnetengine* engine; /* ONE actual independent hold, not LiveObjects. */
} EngineData;
struct EngineCase {
    xnetengine* engine;
    const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* preparation;
    xatomic32 tasks, timers, closed, drops, freed, probes, tail_entered, tail_release;
    xthreadkey* key;
    unsigned counts, traces;
    bool wrong_task, wrong_timer, resurrect, escaped, probe, tail;
};
static unsigned idle_cases, queued_cases, opaque_cases, exit_cases, oom_budgets, oom_failures;
static void wait_one(xatomic32* value)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtAtomic32Load(value,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void freeze_begin(xrtownershipscope* freeze)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(freeze)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void stable_freeze(xnetengine* engine,xrtownershipscope* freeze)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);xrtownershipref ref=xrtNetEngineOwnership(engine);size_t count;
    for(;;){freeze_begin(freeze);if(ref.Ops->Count(ref.Data,&count))return;
        assert(xrtOwnershipScopeEnd(freeze));assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&
        before->UnderflowCount==after.UnderflowCount);
}
static bool data_count(const void* data,size_t* count)
{
    const EngineData* item=data;++item->test->counts;
    if(item->refs<=0)return false;
    *count=(size_t)item->refs;return true;
}
static bool data_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const EngineData* item=data;++item->test->traces;
    return visit(xrtNetEngineOwnership(item->engine),context);
}
static const xrtownershipops data_ops={data_count,data_trace};
static void data_drop(ptr data)
{
    EngineData* item=data;EngineCase* test=item->test;xrtownershipscope mutation={0};
    assert(xrtOwnershipMutationBegin(&mutation));
    xrtAtomic32FetchAdd(&test->drops,1,XMEMORY_ACQ_REL);
    xnetengine* engine=NULL;
    if(xrtRefRelease(&item->refs)==0){engine=item->engine;item->engine=NULL;
        xrtAtomic32FetchAdd(&test->freed,1,XMEMORY_ACQ_REL);xrtFree(item);}
    assert(xrtOwnershipScopeEnd(&mutation));
    if(engine)test->adapter->Drop(engine);
}
static int32 probe_thread(ptr data)
{
    EngineCase* test=data;xrtownershipscope freeze={0};freeze_begin(&freeze);
    xrtownershipref ref=xrtNetEngineOwnership(test->engine);size_t count=197;
    const xrtownershippreparationv1* prep=(const xrtownershippreparationv1*)test;
    assert(!ref.Ops->Count(ref.Data,&count)&&count==197);
    assert(!xrtNetEngineOwnershipAdapterV1(ref,NULL,0,NULL,0,&prep)&&prep==(const xrtownershippreparationv1*)test);
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void callback_probe(EngineCase* test)
{
    if(!test->probe)return;
    xthread* thread=xrtThreadCreate(probe_thread,test,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);
    xrtAtomic32FetchAdd(&test->probes,1,XMEMORY_ACQ_REL);
}
static void task(xnetworker* worker,ptr data)
{
    EngineData* item=data;EngineCase* test=item->test;assert(xrtNetWorkerEngine(worker)==test->engine);
    callback_probe(test);
    if(test->tail)assert(xrtThreadKeySet(test->key,test));
    if(test->resurrect&&!test->escaped){assert(test->adapter->Hold(test->engine));test->escaped=true;}
    if(test->probe||test->tail){
        xerror* marker=xrtErrorCreate(XERR_STATE,"engine.ownership",17,"callback marker");assert(marker);xrtSetError(marker);
        assert(xrtNetEngineTryDestroy(test->engine)==XNET_RETIRE_BUSY&&xrtGetError()==marker);
        xrtClearError();xrtErrorFree(marker);
    }
    xrtAtomic32FetchAdd(&test->tasks,1,XMEMORY_ACQ_REL);
}
static void timer(xnetworker* worker,uint64 id,xnetresult result,ptr data)
{
    EngineData* item=data;EngineCase* test=item->test;assert(worker&&id);
    assert(result==XNET_RESULT_OK||result==XNET_RESULT_CANCELLED||result==XNET_RESULT_CLOSED||result==XNET_RESULT_ERROR);
    callback_probe(test);
    xrtAtomic32FetchAdd(&test->timers,1,XMEMORY_ACQ_REL);
    if(result==XNET_RESULT_CLOSED)xrtAtomic32FetchAdd(&test->closed,1,XMEMORY_ACQ_REL);
}
static const xnettaskownershipv1 task_policy={sizeof(task_policy),task,data_drop,&data_ops};
static const xnettaskownershipv1 wrong_task={sizeof(wrong_task),task,data_drop,&data_ops};
static const xnettimerownershipv1 timer_policy={sizeof(timer_policy),timer,data_drop,&data_ops};
static const xnettimerownershipv1 wrong_timer={sizeof(wrong_timer),timer,data_drop,&data_ops};
static bool admit(xrtownershipref ref,ptr data)
{
    EngineCase* test=data;const xrtownershippreparationv1* prep=NULL;
    const xnettaskownershipv1* tasks[]={test->wrong_task?&wrong_task:&task_policy};
    const xnettimerownershipv1* timers[]={test->wrong_timer?&wrong_timer:&timer_policy};
    return xrtNetEngineOwnershipAdapterV1(ref,tasks,1,timers,1,&prep)||ref.Ops==&data_ops;
}
static void setup(EngineCase* test,unsigned workers,size_t capacity)
{
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=workers;
    config.CommandCapacity=capacity;config.NodeCacheBytes=0;
    test->engine=xrtNetEngineCreate(&config);assert(test->engine&&xrtNetEngineStart(test->engine));
    xrtownershipscope freeze={0};stable_freeze(test->engine,&freeze);
    test->adapter=xrtNetEngineOwnershipAdapterV1(xrtNetEngineOwnership(test->engine),NULL,0,NULL,0,&test->preparation);
    assert(test->adapter&&test->preparation);assert(xrtOwnershipScopeEnd(&freeze));
}
static EngineData* item_new(EngineCase* test,int32 refs)
{
    EngineData* item=xrtCalloc(1,sizeof(*item));assert(item);item->refs=refs;item->test=test;
    assert(test->adapter->Hold(test->engine));item->engine=test->engine;return item;
}
static void prepare(EngineCase* test)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){xrtownershipprepareresult result=test->preparation->Prepare(test->engine,test);
        if(result==XRT_OWNERSHIP_PREPARE_READY)return;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void idle(unsigned workers,bool destroy_first)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);EngineCase test={0};setup(&test,workers,8);
    xrtownershipscope freeze={0};stable_freeze(test.engine,&freeze);xrtownershipref ref=xrtNetEngineOwnership(test.engine);size_t count;
    assert(ref.Ops->Count(ref.Data,&count)&&count==1);
    assert(xrtNetEnginePin(test.engine)&&test.adapter->Hold(test.engine));
    assert(ref.Ops->Count(ref.Data,&count)&&count==3);assert(xrtNetEngineUnpin(test.engine));
    assert(ref.Ops->Count(ref.Data,&count)&&count==2);assert(xrtOwnershipScopeEnd(&freeze));
    if(destroy_first)assert(xrtNetEngineDestroy(test.engine));else{assert(xrtNetEngineStop(test.engine));assert(xrtNetEngineStart(test.engine));}
    stable_freeze(test.engine,&freeze);assert(ref.Ops->Count(ref.Data,&count)&&count==(destroy_first?1u:2u));
    assert(test.adapter->Claim(test.engine,&test)&&!test.adapter->Claim(test.engine,&count));test.adapter->Restore(test.engine,&test);
    assert(test.adapter->Claim(test.engine,&test));assert(!xrtNetEnginePin(test.engine));xrtClearError();
    assert(xrtOwnershipScopeEnd(&freeze));prepare(&test);stable_freeze(test.engine,&freeze);
    assert(test.preparation->Ready(test.engine));test.adapter->Clear(test.engine,&test);assert(!test.adapter->Hold(test.engine));
    assert(xrtOwnershipScopeEnd(&freeze));assert(test.adapter->Finish(test.engine,&test)&&test.adapter->Finish(test.engine,&test));
    if(!destroy_first)assert(xrtNetEngineTryDestroy(test.engine)==XNET_RETIRE_READY);
    test.adapter->Drop(test.engine);balanced(&before);++idle_cases;
}
static void queued(bool resurrect,bool cancel)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);EngineCase test={0};test.resurrect=resurrect;test.probe=true;setup(&test,1,8);
    xrtownershipscope freeze={0};stable_freeze(test.engine,&freeze);EngineData* item=item_new(&test,2);
    assert(xrtNetEnginePostOwnedV1(test.engine,0,item,&task_policy));
    uint64 id=xrtNetEngineScheduleOwnedV1(test.engine,0,xrtDeadlineAfter(3600000000ULL),item,&timer_policy);assert(id);
    xrtownershipref ref=xrtNetEngineOwnership(test.engine);xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&ref,1,&ref,1,&graph));
    assert(graph.NodeCount==2&&graph.EdgeCount==4&&graph.ExternalRootCount==0);
    xrtownershipsnapshot* snapshot=NULL;unsigned calls=test.counts+test.traces;
    test.wrong_task=true;assert(!xrtOwnershipSnapshotCreate(&ref,1,&ref,1,admit,&test,&snapshot)&&!snapshot);
    assert(calls==test.counts+test.traces);test.wrong_task=false;xrtClearError();
    test.wrong_timer=true;assert(!xrtOwnershipSnapshotCreate(&ref,1,&ref,1,admit,&test,&snapshot)&&!snapshot);
    assert(calls==test.counts+test.traces);test.wrong_timer=false;xrtClearError();
    assert(xrtOwnershipSnapshotCreate(&ref,1,&ref,1,admit,&test,&snapshot));xrtOwnershipSnapshotDestroy(snapshot);
    if(cancel)assert(xrtNetEngineTimerCancel(test.engine,id));
    assert(test.adapter->Hold(test.engine)&&test.adapter->Claim(test.engine,&test));
    /* Rejected publication consumes no context reference. */
    assert(!xrtNetEnginePostOwnedV1(test.engine,0,item,&task_policy));xrtClearError();
    assert(item->refs==2);assert(xrtOwnershipScopeEnd(&freeze));prepare(&test);
    assert(xrtAtomic32Load(&test.tasks,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&test.timers,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&test.freed,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&test.probes,XMEMORY_ACQUIRE)==2);
    stable_freeze(test.engine,&freeze);xrtownershipref slots[2];slots[0]=ref;slots[1]=ref;
    assert(xrtOwnershipInspect(&ref,1,slots,2,&graph)&&graph.NodeCount==1&&graph.ExternalRootCount==(resurrect?1u:0u));
    if(resurrect){assert(test.escaped);test.adapter->Restore(test.engine,&test);assert(xrtOwnershipScopeEnd(&freeze));test.adapter->Drop(test.engine);}
    else {test.adapter->Clear(test.engine,&test);assert(xrtOwnershipScopeEnd(&freeze));assert(test.adapter->Finish(test.engine,&test));}
    assert(xrtNetEngineDestroy(test.engine));test.adapter->Drop(test.engine);balanced(&before);++queued_cases;
}
static void borrowed_task(xnetworker* worker,ptr data)
{ EngineCase* test=data;if(test->tail)assert(xrtNetWorkerPort(worker));xrtAtomic32FetchAdd(&test->tasks,1,XMEMORY_ACQ_REL); }
static void borrowed_timer(xnetworker* worker,uint64 id,xnetresult result,ptr data)
{ (void)worker;(void)id;assert(result==XNET_RESULT_CLOSED);xrtAtomic32FetchAdd(&((EngineCase*)data)->timers,1,XMEMORY_ACQ_REL); }
static void opaque(unsigned kind)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);EngineCase test={0};setup(&test,1,8);
    xrtownershipscope freeze={0};stable_freeze(test.engine,&freeze);xrtownershipref ref=xrtNetEngineOwnership(test.engine);size_t count=197;
    if(kind==0)assert(xrtNetEnginePost(test.engine,0,borrowed_task,&test));
    else if(kind==1)assert(xrtNetEngineSchedule(test.engine,0,xrtDeadlineAfter(3600000000ULL),borrowed_timer,&test));
    else{test.tail=true;assert(xrtNetEnginePost(test.engine,0,borrowed_task,&test));assert(xrtOwnershipScopeEnd(&freeze));wait_one(&test.tasks);freeze_begin(&freeze);}
    assert(!ref.Ops->Count(ref.Data,&count)&&count==197);assert(xrtOwnershipScopeEnd(&freeze));
    assert(xrtNetEngineStop(test.engine));stable_freeze(test.engine,&freeze);assert(ref.Ops->Count(ref.Data,&count)&&count==1);
    assert(xrtOwnershipScopeEnd(&freeze));assert(xrtNetEngineDestroy(test.engine));balanced(&before);++opaque_cases;
}
static void thread_tail(ptr data)
{ EngineCase* test=data;xrtAtomic32Store(&test->tail_entered,1,XMEMORY_RELEASE);wait_one(&test->tail_release); }
static void exit_window(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);EngineCase test={0};test.tail=true;
    test.key=xrtThreadKeyCreate(thread_tail);assert(test.key);setup(&test,1,8);
    xrtownershipscope freeze={0};stable_freeze(test.engine,&freeze);
    EngineData* item=item_new(&test,1);assert(xrtNetEnginePostOwnedV1(test.engine,0,item,&task_policy));
    assert(test.adapter->Hold(test.engine)&&test.adapter->Claim(test.engine,&test));assert(xrtOwnershipScopeEnd(&freeze));
    assert(test.preparation->Prepare(test.engine,&test)==XRT_OWNERSHIP_PREPARE_BUSY);wait_one(&test.tail_entered);
    freeze_begin(&freeze);xrtownershipref ref=xrtNetEngineOwnership(test.engine);size_t count=197;
    assert(!ref.Ops->Count(ref.Data,&count)&&count==197&&!test.preparation->Ready(test.engine));assert(xrtOwnershipScopeEnd(&freeze));
    xrtAtomic32Store(&test.tail_release,1,XMEMORY_RELEASE);
    /* No intervening Prepare: read-only completion must unlock revalidation. */
    stable_freeze(test.engine,&freeze);assert(!test.preparation->Ready(test.engine));assert(xrtOwnershipScopeEnd(&freeze));prepare(&test);
    stable_freeze(test.engine,&freeze);test.adapter->Clear(test.engine,&test);assert(xrtOwnershipScopeEnd(&freeze));
    assert(test.adapter->Finish(test.engine,&test)&&xrtNetEngineDestroy(test.engine));test.adapter->Drop(test.engine);
    assert(xrtThreadKeyDestroy(test.key));balanced(&before);++exit_cases;
}
static void submit_oom(bool is_timer)
{
    for(unsigned budget=0;budget<12;++budget){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);EngineCase test={0};setup(&test,1,2);
        xrtownershipscope freeze={0};stable_freeze(test.engine,&freeze);EngineData* item=item_new(&test,1);
        assert(xrtMemDebugFailAfter(budget));++oom_budgets;
        bool accepted=is_timer?xrtNetEngineScheduleOwnedV1(test.engine,0,xrtDeadlineAfter(3600000000ULL),item,&timer_policy)!=0:
            xrtNetEnginePostOwnedV1(test.engine,0,item,&task_policy);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!accepted){assert(failed&&item->refs==1&&xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==0);++oom_failures;data_drop(item);}
        xrtClearError();assert(xrtOwnershipScopeEnd(&freeze));assert(xrtNetEngineDestroy(test.engine));
        assert(xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&test.freed,XMEMORY_ACQUIRE)==1);balanced(&before);}
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned i=0;i<6;++i){idle(1,false);idle(2,true);queued(false,false);queued(false,true);queued(true,false);
        opaque(0);opaque(1);opaque(2);exit_window();}
    submit_oom(false);submit_oom(true);assert(oom_failures>0);
    testMemoryDebugDrain("engine ownership balance");
    printf("engine graph: idle=%u queued=%u opaque=%u exit=%u allocation-budgets=%u failures=%u\n",
        idle_cases,queued_cases,opaque_cases,exit_cases,oom_budgets,oom_failures);
    return 0;
}
