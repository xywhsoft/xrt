#ifndef XRT_TEST_NET_FILE_BOUNDARY_CASES_H
#define XRT_TEST_NET_FILE_BOUNDARY_CASES_H
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Real regular files and Engine completion events only. All buffers/contexts
 * stay alive until native completion; no sockets, artificial backend or
 * attacker-shaped input. The same cases run against the pinned products. */
typedef struct nf_boundary_case {
    xnetengine* engine;
    xfile file;
    xnetcompletion completion;
    xatomic32 submitted,done,terminals;
    uint64 id;
    unsigned kind;
    xnetresult result;
    int systemCode;
    size_t bytes;
    char data[8];
} nf_boundary_case;
static unsigned nf_boundary_rejections,nf_boundary_successes,nf_boundary_eofs,nf_boundary_opaque;
#if defined(NET_FILE_GAP_PROBE)
static xnetengine* nf_boundary_active_engine;
static unsigned nf_boundary_gap_probes;
static int32 nf_boundary_gap_thread(ptr raw)
{
    xnetengine* engine=raw;xrtownershipscope freeze={0};double end=xrtTimer()+10.0;
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(xrtTimer()<end);xrtThreadYield();}
    /* The backend really accepted I/O but its opaque marker is not published
     * yet. Worker activity must refuse this graph without preventing freeze
     * admission itself. No fake port or speculative reference count. */
    assert(xrtNetEngineOwnershipQuiescenceV1Get()->Busy(engine));
    xrtownershipref ref=xrtNetEngineOwnership(engine);size_t count=197;
    assert(!ref.Ops->Count(ref.Data,&count)&&count==197);
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
void nfBoundaryGapProbe(void)
{
    xthread* thread=xrtThreadCreate(nf_boundary_gap_thread,nf_boundary_active_engine,0);assert(thread);
    /* IsWaiting uses only the stable double timer across both clock ABIs. */
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);++nf_boundary_gap_probes;
}
#endif
#if defined(XRT_FEATURE_MEMORY_DEBUG)
static void nf_boundary_balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
#endif
static void nf_boundary_wait(xatomic32* flag)
{
    double end=xrtTimer()+10.0;
    while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){assert(xrtTimer()<end);xrtThreadYield();}
}
static unsigned nf_boundary_visits;
static bool nf_boundary_visit(xrtownershipref ref,ptr unused)
{(void)ref;(void)unused;++nf_boundary_visits;return true;}
static void nf_boundary_graph(xnetengine* engine,bool opaque)
{
    xrtownershipref ref=xrtNetEngineOwnership(engine);
    const xrtownershipquiescencev1* idle=xrtNetEngineOwnershipQuiescenceV1Get();
    double end=xrtTimer()+10.0;xrtownershipscope freeze={0};
    for(;;){
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            if(!idle->Busy(engine))break;
            assert(xrtOwnershipScopeEnd(&freeze));
        }
        assert(xrtTimer()<end);xrtThreadYield();
    }
    size_t count=197;nf_boundary_visits=0;
    const xrtownershippreparationv1* preparation=(const xrtownershippreparationv1*)engine;
    if(opaque){
        assert(!ref.Ops->Count(ref.Data,&count)&&count==197);
        assert(!ref.Ops->Trace(ref.Data,nf_boundary_visit,NULL)&&nf_boundary_visits==0);
        assert(!xrtNetEngineOwnershipAdapterV1(ref,NULL,0,NULL,0,&preparation)&&
            preparation==(const xrtownershippreparationv1*)engine);
        ++nf_boundary_opaque;
    }else{
        assert(ref.Ops->Count(ref.Data,&count)&&count==1);
        assert(xrtNetEngineOwnershipAdapterV1(ref,NULL,0,NULL,0,&preparation)&&preparation);
    }
    assert(xrtOwnershipScopeEnd(&freeze));
}
static void nf_boundary_complete(xnetworker* worker,const xnetportevent* event,ptr raw)
{
    nf_boundary_case* c=raw;assert(worker&&event->Id==c->id);
    assert(event->Type==((c->kind==1||c->kind==2||c->kind==4||c->kind==8)?XNET_PORT_EVENT_FILE_WRITE:XNET_PORT_EVENT_FILE_READ));
    c->result=event->Result;c->systemCode=event->SystemCode;c->bytes=event->Bytes;
    assert(xrtAtomic32FetchAdd(&c->terminals,1,XMEMORY_ACQ_REL)==0);
    xrtAtomic32Store(&c->done,1,XMEMORY_RELEASE);
}
static void nf_boundary_submit(xnetworker* worker,ptr raw)
{
    nf_boundary_case* c=raw;
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    if(c->kind==8)assert(xrtMemDebugFailAfter(0));
#endif
    if(c->kind==1||c->kind==2||c->kind==4||c->kind==8)c->id=xrtNetFileWrite(worker,c->file,0,"X\0Y",c->kind==4?0:3,&c->completion);
    else c->id=xrtNetFileRead(worker,c->file,c->kind==5?3:c->kind==6?17:0,c->data,c->kind==3?0:8,&c->completion);
    bool rejected=c->kind<2||c->kind==3||c->kind==4||c->kind==8;
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    if(c->kind==8){assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();}
#endif
    if(rejected){
        assert(!c->id&&xrtGetError());
        assert(xrtErrorKind(xrtGetError())==((c->kind<2)?XERR_UNSUPPORTED:c->kind==8?XERR_MEMORY:XERR_ARGUMENT));
        xrtClearError();xrtAtomic32Store(&c->done,1,XMEMORY_RELEASE);
    }else assert(c->id);
    xrtAtomic32Store(&c->submitted,1,XMEMORY_RELEASE);
}
static bool nf_boundary_case_run(xnetportbackend backend,unsigned kind)
{
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
#endif
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.NodeCacheBytes=0;
    xnetengine* engine=xrtNetEngineCreate(&config);
    if(!engine||!xrtNetEngineStart(engine)){
        /* Capability absence is explicit, never a successful native I/O test. */
        const xerror* error=xrtGetError();assert(error);
        if(backend==XNET_PORT_SELECT)assert(!"required select backend failed");
#if defined(_WIN32)
        assert(!"required Windows IOCP backend failed");
#else
        assert(xrtErrorKind(error)==XERR_UNSUPPORTED||xrtErrorSystemCode(error)==EPERM||
            xrtErrorSystemCode(error)==EACCES||xrtErrorSystemCode(error)==ENOSYS);
#endif
        printf("NetFile native backend unavailable: kind=%d system=%d\n",(int)xrtErrorKind(error),xrtErrorSystemCode(error));
        xrtClearError();assert(!engine||xrtNetEngineDestroy(engine));
#if defined(XRT_FEATURE_MEMORY_DEBUG)
        nf_boundary_balanced(&before);
#endif
        return false;
    }
    const char* path="net_file_boundary_cases.tmp";xfileoptions options;xrtFileOptionsInit(&options);
    options.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    xfile seed=xrtFileOpen(path,&options);assert(seed);size_t written=0;
    assert(xrtWrite(seed,"a\0b",3,&written)&&written==3&&xrtClose(seed));
    options.Flags=XFILE_READ|XFILE_WRITE;
    nf_boundary_case c={0};c.engine=engine;c.kind=kind;c.file=xrtNetFileOpen(path,&options);assert(c.file);
#if defined(NET_FILE_GAP_PROBE)
    nf_boundary_active_engine=engine;
#endif
    xrtNetCompletionInit(&c.completion,nf_boundary_complete,&c);
    assert(xrtNetEnginePost(engine,0,nf_boundary_submit,&c));nf_boundary_wait(&c.submitted);nf_boundary_wait(&c.done);
    bool rejected=kind<2||kind==3||kind==4||kind==8;
    nf_boundary_graph(engine,!rejected);
    assert(xrtNetEngineStop(engine));
    assert(xrtAtomic32Load(&c.terminals,XMEMORY_ACQUIRE)==(rejected?0:1));
    /* Opaque capability lasts for the actual port run, not the Engine shell. */
    nf_boundary_graph(engine,false);
    if(rejected)++nf_boundary_rejections;
    else{
        assert(c.result==XNET_RESULT_OK&&c.systemCode==0);
        if(kind==5||kind==6){assert(!c.bytes);++nf_boundary_eofs;}
        else{assert(c.bytes==3);if(kind==7)assert(!memcmp(c.data,"a\0b",3));}
        ++nf_boundary_successes;
    }
    assert(xrtClose(c.file));
    if(kind==2){char value[3];size_t read=0;options.Flags=XFILE_READ;
        xfile result=xrtFileOpen(path,&options);assert(result&&xrtRead(result,value,3,&read)&&read==3&&!memcmp(value,"X\0Y",3)&&xrtClose(result));}
    assert(xrtNetEngineDestroy(engine)&&xrtFileDelete(path));
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    nf_boundary_balanced(&before);
#endif
    return true;
}
static void testNetFileBoundary(void)
{
    nf_boundary_rejections=nf_boundary_successes=nf_boundary_eofs=nf_boundary_opaque=0;
    assert(nf_boundary_case_run(XNET_PORT_SELECT,0));
    assert(nf_boundary_case_run(XNET_PORT_SELECT,1));
    /* Invalid read/write stay closed to OS submission on every backend. */
    assert(nf_boundary_case_run(XNET_PORT_SELECT,3));
    assert(nf_boundary_case_run(XNET_PORT_SELECT,4));
#if defined(_WIN32) || (defined(__linux__)&&!defined(__ANDROID__))
#if defined(_WIN32)
    xnetportbackend backend=XNET_PORT_IOCP;
#else
    xnetportbackend backend=XNET_PORT_URING;
#endif
    if(nf_boundary_case_run(backend,2)){
        assert(nf_boundary_case_run(backend,5)&&nf_boundary_case_run(backend,6)&&nf_boundary_case_run(backend,7));
#if defined(XRT_FEATURE_MEMORY_DEBUG)
        assert(nf_boundary_case_run(backend,8));
#endif
        assert(nf_boundary_successes==4&&nf_boundary_eofs==2&&nf_boundary_opaque==4);
    }
#endif
    assert(nf_boundary_rejections==(nf_boundary_successes?
#if defined(XRT_FEATURE_MEMORY_DEBUG)
        5
#else
        4
#endif
        :4));
    printf("NetFile boundary: rejected=%u actual-I/O=%u EOF=%u opaque=%u; unique terminals, exact retirement\n",
        nf_boundary_rejections,nf_boundary_successes,nf_boundary_eofs,nf_boundary_opaque);
#if defined(NET_FILE_GAP_PROBE)
    assert(nf_boundary_gap_probes==nf_boundary_successes);
    printf("NetFile gap: %u independent freezes admitted while accepted I/O remains BUSY\n",nf_boundary_gap_probes);
#endif
}
#endif
