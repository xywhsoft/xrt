/* Ordinary worker/ownership lifecycle only: no sockets or network traffic. */
#include <xrt.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Case Case;
typedef struct Context { volatile int32 refs; Case* test; } Context;
struct Case {
    xnetengine* engine;
    xatomic32 jobs,jobGate,exits,exitGate,drops;
    bool blockJob,blockExit,reenter;
    unsigned contextCounts,contextTraces,edges;
};
static void wait_count(xatomic32* value,uint32 expected)
{
    for(unsigned n=0;n<10000;++n){if(xrtAtomic32Load(value,XMEMORY_ACQUIRE)>=expected)return;xrtSleep(1);}
    assert(!"worker gate timed out");
}
static void freeze_begin(xrtownershipscope* scope)
{
    for(unsigned n=0;n<10000;++n){if(xrtOwnershipFreezeTryBegin(scope))return;xrtSleep(1);}
    assert(!"freeze timed out");
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool context_count(const void* raw,size_t* output)
{const Context* data=raw;++data->test->contextCounts;if(data->refs<=0)return false;*output=(size_t)data->refs;return true;}
static bool context_trace(const void* raw,xrtownershipvisitor visit,ptr user)
{(void)visit;(void)user;++((const Context*)raw)->test->contextTraces;return true;}
static const xrtownershipops contextOps={context_count,context_trace};
static void context_exit(ptr raw)
{
    Case* test=((Context*)raw)->test;
    assert(xrtNetEngineCurrent(test->engine)!=NULL);
    xrtAtomic32FetchAdd(&test->exits,1,XMEMORY_ACQ_REL);
    if(test->blockExit)wait_count(&test->exitGate,1);
}
static void context_drop(ptr raw,ptr user)
{
    Context* data=raw;Case* test=data->test;assert(!user&&data->refs==1);
    xerror* previous=xrtTakeError();
    xrtownershipscope freeze={0};freeze_begin(&freeze);assert(xrtOwnershipScopeEnd(&freeze));
    if(test->engine&&test->reenter){assert(xrtNetEngineTryDestroy(test->engine)==XNET_RETIRE_BUSY);}
    xrtAtomic32FetchAdd(&test->drops,1,XMEMORY_ACQ_REL);data->refs=0;xrtFree(data);
    xrtClearError();xrtSetErrorTake(previous);
}
static const xnetengineworkerexitownershipv1 exitPolicy={sizeof(exitPolicy),context_exit,context_drop,&contextOps};
static const xnetengineworkerexitownershipv1 otherPolicy={sizeof(otherPolicy),context_exit,context_drop,&contextOps};
static Context* make_context(Case* test)
{Context* data=xrtCalloc(1,sizeof(*data));assert(data);data->refs=1;data->test=test;return data;}
static bool edge(xrtownershipref ref,ptr raw)
{Case* test=raw;assert(ref.Ops==&contextOps);++test->edges;size_t count;assert(ref.Ops->Count(ref.Data,&count)&&count==1);return true;}
static bool admit(xrtownershipref ref,ptr raw)
{
    (void)raw;const xnetengineworkerexitownershipv1* policies[]={&exitPolicy};
    const xrtownershippreparationv1* preparation=NULL;
    return xrtNetEngineOwnershipAdapterV2(ref,NULL,0,NULL,0,policies,1,&preparation)||ref.Ops==&contextOps;
}
static void inspect(Case* test,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** prep)
{
    const xnetengineworkerexitownershipv1* policies[]={&exitPolicy};
    xrtownershipref ref=xrtNetEngineOwnership(test->engine);xrtownershipscope freeze={0};size_t count=0;
    for(unsigned n=0;;++n){assert(n<10000);freeze_begin(&freeze);
        if(ref.Ops->Count(ref.Data,&count))break;
        assert(xrtOwnershipScopeEnd(&freeze));xrtSleep(1);
    }
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)test;
    unsigned calls=test->contextCounts+test->contextTraces;
    assert(!xrtNetEngineOwnershipAdapterV1(ref,NULL,0,NULL,0,&sentinel)&&sentinel==(const xrtownershippreparationv1*)test);
    const xnetengineworkerexitownershipv1* wrong[]={&otherPolicy};
    assert(!xrtNetEngineOwnershipAdapterV2(ref,NULL,0,NULL,0,wrong,1,&sentinel)&&sentinel==(const xrtownershippreparationv1*)test);
    assert(test->contextCounts+test->contextTraces==calls);
    *adapter=xrtNetEngineOwnershipAdapterV2(ref,NULL,0,NULL,0,policies,1,prep);assert(*adapter&&*prep&&count==1);
    test->edges=0;assert(ref.Ops->Trace(ref.Data,edge,test)&&test->edges==1);
    xrtownershipsnapshot* snapshot=NULL;assert(xrtOwnershipSnapshotCreate(&ref,1,&ref,1,admit,test,&snapshot)&&snapshot);
    xrtOwnershipSnapshotDestroy(snapshot);assert(xrtOwnershipScopeEnd(&freeze));
}
static void post(xnetworker* worker,ptr raw)
{
    Case* test=raw;xnetportbackendinfo info;
    assert(xrtNetWorkerEngine(worker)==test->engine&&xrtNetEngineBackendInfo(test->engine,xrtNetWorkerIndex(worker),&info));
    assert(info.Backend==XNET_PORT_SELECT);xrtAtomic32FetchAdd(&test->jobs,1,XMEMORY_ACQ_REL);
    if(test->blockJob)wait_count(&test->jobGate,1);
}
static void stop_ready(xnetengine* engine)
{
    for(unsigned n=0;n<10000;++n){xnetretireresult result=xrtNetEngineTryStop(engine);
        if(result==XNET_RETIRE_READY)return;
        assert(result==XNET_RETIRE_BUSY);xrtSleep(1);}
    assert(!"stop timed out");
}
static void lifecycle(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Case test={0};test.reenter=true;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=XNET_PORT_SELECT;config.Workers=2;
    xnetbufpoolconfig buffer;xrtNetBufPoolConfigInit(&buffer);buffer.MaxCacheBytes=1048576;config.BufferPool=&buffer;
    test.engine=xrtNetEngineCreateOwnedExitV1(&config,make_context(&test),&exitPolicy);assert(test.engine);
    xnetengineconfig copied;xnetbufpoolconfig own;
    assert(xrtNetEngineGetConfig(test.engine,&copied,&own)&&copied.BufferPool==&own&&copied.Workers==2&&own.MaxCacheBytes==1048576);
    own.MaxCacheBytes=1;buffer.MaxCacheBytes=1;assert(xrtNetEngineGetConfig(test.engine,&copied,&own)&&own.MaxCacheBytes==1048576);
    xnetportbackendinfo sentinel={XNET_PORT_AUTO,"unchanged",123},info=sentinel;
    assert(!xrtNetEngineBackendInfo(test.engine,0,&info)&&info.Name==sentinel.Name);xrtClearError();
    const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prep;inspect(&test,&adapter,&prep);
    assert(xrtNetEngineStart(test.engine));
    assert(xrtNetEngineBackendInfo(test.engine,1,&info)&&info.Backend==XNET_PORT_SELECT&&!strcmp(info.Name,"select"));
    info=sentinel;assert(!xrtNetEngineBackendInfo(test.engine,2,&info)&&xrtErrorKind(xrtGetError())==XERR_RANGE&&info.Name==sentinel.Name);xrtClearError();
    /* Actual metadata reads must NOT publish the raw-port exposure duty. */
    inspect(&test,&adapter,&prep);
    assert(xrtNetEnginePin(test.engine));xrtSetErrorKind(XERR_ARGUMENT);const xerror* prior=xrtGetError();
    assert(xrtNetEngineTryStop(test.engine)==XNET_RETIRE_BUSY&&xrtGetError()==prior&&xrtNetEngineState(test.engine)==XNET_ENGINE_RUNNING);
    xrtClearError();assert(xrtNetEngineUnpin(test.engine));
    test.blockJob=true;test.blockExit=true;assert(xrtNetEnginePost(test.engine,0,post,&test));wait_count(&test.jobs,1);
    xrtSetErrorKind(XERR_ARGUMENT);prior=xrtGetError();
    assert(xrtNetEngineTryStop(test.engine)==XNET_RETIRE_BUSY&&xrtGetError()==prior);xrtClearError();
    assert(xrtNetEngineState(test.engine)==XNET_ENGINE_STOPPING);
    xrtAtomic32Store(&test.jobGate,1,XMEMORY_RELEASE);wait_count(&test.exits,1);
    xrtownershipscope freeze={0};freeze_begin(&freeze);size_t count=197;
    xrtownershipref ref=xrtNetEngineOwnership(test.engine);assert(!ref.Ops->Count(ref.Data,&count)&&count==197);
    assert(xrtOwnershipScopeEnd(&freeze));xrtAtomic32Store(&test.exitGate,1,XMEMORY_RELEASE);
    stop_ready(test.engine);assert(xrtAtomic32Load(&test.exits,XMEMORY_ACQUIRE)==2&&!xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE));
    xrtSetErrorKind(XERR_ARGUMENT);prior=xrtGetError();assert(xrtNetEngineTryStop(test.engine)==XNET_RETIRE_READY&&xrtGetError()==prior);xrtClearError();
    inspect(&test,&adapter,&prep);test.blockJob=false;test.blockExit=false;
    assert(xrtNetEngineStart(test.engine)&&xrtNetEngineStop(test.engine));
    assert(xrtAtomic32Load(&test.exits,XMEMORY_ACQUIRE)==4&&!xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE));
    assert(xrtNetEngineDestroy(test.engine));assert(xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==1);balanced(&before);
}
static void graph_retirement(bool pending)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Case test={0};test.reenter=true;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=XNET_PORT_SELECT;config.Workers=1;
    test.engine=xrtNetEngineCreateOwnedExitV1(&config,make_context(&test),&exitPolicy);assert(test.engine&&xrtNetEngineStart(test.engine));
    const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prep;inspect(&test,&adapter,&prep);
    if(pending){
        test.blockJob=true;assert(xrtNetEnginePost(test.engine,0,post,&test));wait_count(&test.jobs,1);
        assert(xrtNetEngineTryStop(test.engine)==XNET_RETIRE_BUSY);
        xrtAtomic32Store(&test.jobGate,1,XMEMORY_RELEASE);wait_count(&test.exits,1);
        /* No more TryStop: the native graph preparation must be able to adopt
         * the stable pending stop instead of waiting forever for its tag. */
        inspect(&test,&adapter,&prep);assert(xrtNetEngineState(test.engine)==XNET_ENGINE_STOPPING);
    }
    xrtownershipscope freeze={0};freeze_begin(&freeze);assert(adapter->Hold(test.engine)&&adapter->Claim(test.engine,&test));assert(xrtOwnershipScopeEnd(&freeze));
    for(unsigned n=0;;++n){assert(n<10000);xrtownershipprepareresult result=prep->Prepare(test.engine,&test);
        if(result==XRT_OWNERSHIP_PREPARE_READY)break;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY);xrtSleep(1);}
    assert(xrtAtomic32Load(&test.exits,XMEMORY_ACQUIRE)==1&&!xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE));
    freeze_begin(&freeze);assert(prep->Ready(test.engine));adapter->Clear(test.engine,&test);assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(test.engine,&test)&&adapter->Finish(test.engine,&test));assert(xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==1);
    assert(xrtNetEngineDestroy(test.engine));adapter->Drop(test.engine);balanced(&before);
}
static void start_oom(void)
{
    unsigned failures=0;
    for(size_t point=0;point<128;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Case test={0};test.reenter=true;
        xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=XNET_PORT_SELECT;
        test.engine=xrtNetEngineCreateOwnedExitV1(&config,make_context(&test),&exitPolicy);assert(test.engine);
        assert(xrtMemDebugFailAfter(point));bool started=xrtNetEngineStart(test.engine);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(!started&&!xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE));++failures;}
        else assert(started);
        assert(xrtNetEngineDestroy(test.engine)&&xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==1);
        balanced(&before);if(!hit)break;
    }
    assert(failures>0&&failures<128);printf("engine start: %u OOM prefixes, context retained until actual retirement\n",failures);
}
static void create_oom(void)
{
    unsigned failures=0;
    for(size_t point=0;point<32;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Case test={0};Context* data=make_context(&test);
        xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;assert(xrtMemDebugFailAfter(point));
        test.engine=xrtNetEngineCreateOwnedExitV1(&config,data,&exitPolicy);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(!test.engine&&!xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE));context_drop(data,NULL);++failures;}
        else {assert(test.engine&&xrtNetEngineDestroy(test.engine));}
        assert(xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==1);balanced(&before);if(!hit)break;
    }
    assert(failures>=2&&failures<32);printf("engine create: %u OOM prefixes, rejected contexts unconsumed\n",failures);
}
static void backend_selection(void)
{
    const xnetportbackend candidates[]={XNET_PORT_AUTO,XNET_PORT_SELECT,XNET_PORT_IOCP,XNET_PORT_EPOLL,XNET_PORT_KQUEUE,XNET_PORT_URING};
    for(size_t n=0;n<sizeof(candidates)/sizeof(candidates[0]);++n){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Case test={0};xnetengineconfig config;xrtNetEngineConfigInit(&config);
        config.Workers=1;config.Backend=candidates[n];test.engine=xrtNetEngineCreateOwnedExitV1(&config,make_context(&test),&exitPolicy);assert(test.engine);
        xnetportbackendinfo compiled,actual;bool built=xrtNetPortBackendInfo(config.Backend,&compiled);
        bool started=xrtNetEngineStart(test.engine);
        if(started){assert(built&&xrtNetEngineBackendInfo(test.engine,0,&actual)&&actual.Backend==compiled.Backend&&
            !strcmp(actual.Name,compiled.Name)&&!(actual.Capabilities&~compiled.Capabilities));
            printf("engine backend %d: actual %s capabilities 0x%08x\n",(int)config.Backend,actual.Name,(unsigned)actual.Capabilities);
        }else {assert(xrtGetError());assert(!built||config.Backend==XNET_PORT_URING);
            printf("engine backend %d: explicit unavailable (compiled=%d, kind=%d); no fallback\n",(int)config.Backend,built,(int)xrtErrorKind(xrtGetError()));xrtClearError();}
        assert(xrtNetEngineDestroy(test.engine)&&xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==1);balanced(&before);
    }
}
int main(void)
{
    lifecycle();graph_retirement(false);graph_retirement(true);create_oom();start_oom();backend_selection();
    puts("engine managed native: real exit tails, restart, exact context edge, guarded actual backend, stop/retire and balanced storage");
    return 0;
}
