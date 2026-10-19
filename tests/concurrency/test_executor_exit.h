/* Included in both modular and single-header ownership acceptance. */
typedef struct ExecutorExitCase {
    size_t refs;
    xexecutor* executor;
    xatomic32 exits, drops, gate, jobs;
    unsigned counts, traces;
    bool probe;
} ExecutorExitCase;
static bool exit_count(const void* raw,size_t* count)
{ExecutorExitCase* c=(ExecutorExitCase*)raw;++c->counts;assert(c->refs);*count=c->refs;return true;}
static bool exit_trace(const void* raw,xrtownershipvisitor visit,ptr context)
{ExecutorExitCase* c=(ExecutorExitCase*)raw;++c->traces;(void)context;return visit!=NULL;}
static const xrtownershipops exit_ops={exit_count,exit_trace};
static void exit_worker(ptr raw)
{
    ExecutorExitCase* c=raw;
    if(c->probe){
        assert(xrtExecutorWaitFor(c->executor,0)==XWAIT_ERROR);
        assert(!xrtExecutorDestroy(c->executor));xrtClearError();
    }
    (void)xrtAtomic32FetchAdd(&c->exits,1,XMEMORY_RELEASE);
    signal_wait(&c->gate);
}
static void exit_drop(ptr raw,ptr context)
{
    ExecutorExitCase* c=raw;assert(!context&&c->refs==2);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));--c->refs;
    assert(xrtOwnershipScopeEnd(&mutation));
    (void)xrtAtomic32FetchAdd(&c->drops,1,XMEMORY_RELEASE);
}
static void exit_job(ptr raw)
{ExecutorExitCase* c=raw;(void)xrtAtomic32FetchAdd(&c->jobs,1,XMEMORY_RELEASE);}
static const xexecutorworkerexitownershipv1 exit_policy={sizeof(exit_policy),exit_worker,exit_drop,&exit_ops};
static const xexecutorworkerexitownershipv1 exit_copy={sizeof(exit_copy),exit_worker,exit_drop,&exit_ops};
static const xrtownershipadapterv1* exit_adapter(xrtownershipref ref,const xrtownershippreparationv1** prepare)
{const xexecutorworkerexitownershipv1* known[]={&exit_policy};return xrtExecutorOwnershipAdapterV2(ref,known,1,prepare);}
static void owned_exit_round(unsigned threads,bool restore)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    ExecutorExitCase c={.refs=2,.probe=true};xexecutorconfig config={threads,8,0};
    c.executor=xrtExecutorCreateOwnedExitV1(&config,&c,&exit_policy);assert(c.executor);
    for(unsigned job=0;job<4;++job){
        assert(xrtExecutorSubmit(c.executor,exit_job,&c,NULL,NULL));
        double end=__xrtWaitAfter(5000);
        while(xrtAtomic32Load(&c.jobs,XMEMORY_ACQUIRE)!=job+1){assert(!__xrtWaitExpired(end));xrtThreadYield();}
        assert(!xrtAtomic32Load(&c.exits,XMEMORY_ACQUIRE));
    }
    xrtownershipscope freeze={0};idle_freeze(c.executor,&freeze);xrtownershipref ref=xrtExecutorOwnership(c.executor);
    const xrtownershippreparationv1* preparation=(const void*)&c;
    assert(!xrtExecutorOwnershipAdapterV1(ref,&preparation)&&preparation==(const void*)&c);
    const xexecutorworkerexitownershipv1* wrong[]={&exit_copy};
    assert(!xrtExecutorOwnershipAdapterV2(ref,wrong,1,&preparation)&&preparation==(const void*)&c);
    assert(!c.counts&&!c.traces);
    const xrtownershipadapterv1* adapter=exit_adapter(ref,&preparation);assert(adapter&&preparation);
    Edges edges={0};assert(ref.Ops->Trace(ref.Data,edge,&edges));
    assert(edges.count==1&&edges.items[0].Data==&c&&edges.items[0].Ops==&exit_ops);
    size_t refs;assert(exit_count(&c,&refs)&&refs==2);
    assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&c));
    assert(xrtOwnershipScopeEnd(&freeze));
    const xrtownershippreparationv1* knownPrepare=preparation;
    double closing=__xrtWaitAfter(5000);
    while(!xrtAtomic32Load(&c.exits,XMEMORY_ACQUIRE)){
        assert(knownPrepare->Prepare(ref.Data,&c)==XRT_OWNERSHIP_PREPARE_BUSY);
        assert(!__xrtWaitExpired(closing));xrtThreadYield();
    }
    signal_wait(&c.exits);
    freeze_begin(&freeze);preparation=(const void*)&c;
    assert(!exit_adapter(ref,&preparation)&&preparation==(const void*)&c);
    assert(xrtExecutorOwnershipQuiescenceV1Get()->Busy(ref.Data));
    assert(xrtOwnershipScopeEnd(&freeze));
    if(restore){freeze_begin(&freeze);adapter->Restore(ref.Data,&c);assert(xrtOwnershipScopeEnd(&freeze));}
    /* Claim prevents API admission, but Restore must still not skip the tail. */
    if(restore)assert(xrtExecutorWaitFor(c.executor,0)==XWAIT_TIMEOUT);
    xrtAtomic32Store(&c.gate,1,XMEMORY_RELEASE);
    if(restore){assert(xrtExecutorClose(c.executor));assert(xrtExecutorWaitFor(c.executor,5000)==XWAIT_OK);}
    else {
        double end=__xrtWaitAfter(5000);
        for(;;){
            xrtownershipprepareresult result=knownPrepare->Prepare(ref.Data,&c);
            assert(result!=XRT_OWNERSHIP_PREPARE_FAILED);
            if(result==XRT_OWNERSHIP_PREPARE_READY)break;
            assert(!__xrtWaitExpired(end));xrtThreadYield();
        }
        freeze_begin(&freeze);adapter->Clear(ref.Data,&c);assert(xrtOwnershipScopeEnd(&freeze));
        assert(adapter->Finish(ref.Data,&c));
    }
    assert(xrtExecutorDestroy(c.executor));
    assert(xrtAtomic32Load(&c.exits,XMEMORY_ACQUIRE)==threads&&xrtAtomic32Load(&c.drops,XMEMORY_ACQUIRE)==1&&c.refs==1);
    adapter->Drop(ref.Data);balance(&before);
}
static void owned_exit_prefixes(void)
{
    bool complete=false;unsigned prefixes=0;
    for(unsigned budget=0;budget<64;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExecutorExitCase c={.refs=2};
        xrtAtomic32Store(&c.gate,1,XMEMORY_RELEASE);xexecutorconfig config={2,2,0};
        assert(xrtMemDebugFailAfter(budget));c.executor=xrtExecutorCreateOwnedExitV1(&config,&c,&exit_policy);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(c.executor){assert(xrtExecutorDestroy(c.executor));assert(c.refs==1);
            assert(xrtAtomic32Load(&c.exits,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&c.drops,XMEMORY_ACQUIRE)==1);}
        else assert(c.refs==2&&!xrtAtomic32Load(&c.exits,XMEMORY_ACQUIRE)&&!xrtAtomic32Load(&c.drops,XMEMORY_ACQUIRE));
        xrtClearError();balance(&before);++prefixes;if(c.executor&&!failed){complete=true;break;}
    }
    assert(complete);
    ExecutorExitCase c={.refs=2};xexecutorconfig config={1,1,0};
    xexecutorworkerexitownershipv1 invalid=exit_policy;invalid.size=0;
    assert(!xrtExecutorCreateOwnedExitV1(&config,&c,&invalid));
    assert(!xrtExecutorCreateOwnedExitV1(&config,NULL,&exit_policy));
    assert(!xrtExecutorCreateOwnedExitV1(&config,&c,NULL));xrtClearError();
    printf("[PASS] executor owned exit: 16 real-worker rounds, no per-job exit, exact graph edge, policy identity, BUSY/restore/finish, checked self calls, prefixes=%u\n",prefixes);
}
