/* Ordinary self-created literal loopback and lifecycle only. No DNS,
 * external peer, malformed traffic, fuzzing or security acceptance. */
#ifdef NET_STREAM_EVENTS_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include <xrt/detail/wait.h>
#include "../test.h"
#ifndef NET_STREAM_EVENTS_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_tcp.h"
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%s:%u: ",__FILE__,(unsigned)__LINE__);testRequire(false,#c);}}while(0)

typedef struct Context Context;
typedef struct Scene {
    xnetengine* engine;xnetlistener* listener;xnetstream* client;xnetstream* server;
    Context* first;Context* second;xatomic32 setup;
} Scene;
struct Context {
    volatile int32 refs;
    xnetstream* stream;
    Context* next; /* borrowed fixture alias, not an owning edge */
    xatomic32 active,tail,drops,dropEntered,dropGate,closes;
};
static unsigned cases,policy_refusals,allocation_failures;
static void waitSet(xatomic32* flag)
{
    double due=__xrtWaitAfter(10000);
    while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
}
static bool contextCount(const void* data,size_t* count)
{
    const Context* c=data;if(!c||!count)return false;int32 refs=__xrtAtomicRefLoad(&c->refs);
    if(refs<=0)return false;
    *count=(size_t)refs;return true;
}
static bool contextTrace(const void* data,xrtownershipvisitor visit,ptr arg)
{const Context* c=data;size_t count;return visit&&contextCount(c,&count)&&visit(xrtNetStreamOwnership(c->stream),arg);}
static const xrtownershipops contextOps={contextCount,contextTrace};
static Context* retain(Context* c)
{CHECK(xrtOwnershipRefRetain(&c->refs)>0);return c;}
static void release(Context* c)
{
    int32 refs=xrtOwnershipRefRelease(&c->refs);CHECK(refs>=0);
    if(!refs){xrtNetStreamDestroy(c->stream);xrtFree(c);}
}
static Context* createContext(xnetstream* stream)
{
    Context* c=xrtCalloc(1,sizeof(*c));CHECK(c);c->refs=1;c->stream=xrtNetStreamRef(stream);CHECK(c->stream);
    xrtAtomic32Init(&c->active,0);xrtAtomic32Init(&c->tail,0);xrtAtomic32Init(&c->drops,0);
    xrtAtomic32Init(&c->dropEntered,0);xrtAtomic32Init(&c->dropGate,0);xrtAtomic32Init(&c->closes,0);return c;
}
static void drop(const void* data)
{
    Context* c=(Context*)data;
    CHECK(!xrtAtomic32Load(&c->active,XMEMORY_ACQUIRE));
    /* A native Drop never monopolizes the mutation/freeze domain. */
    xrtownershipscope freeze={0};CHECK(xrtOwnershipFreezeTryBegin(&freeze));
    CHECK(xrtNetStreamOwnershipQuiescenceV1Get()->Busy(c->stream));
    CHECK(xrtOwnershipScopeEnd(&freeze));
    xrtAtomic32Store(&c->dropEntered,1,XMEMORY_RELEASE);waitSet(&c->dropGate);
    xrtAtomic32FetchAdd(&c->drops,1,XMEMORY_ACQ_REL);release(c);
}
static void closeEvent(xnetstream* stream,xnetresult result,const xerror* error,ptr data)
{
    (void)result;(void)error;Context* c=data;CHECK(c->stream==stream);
    xrtAtomic32Store(&c->active,1,XMEMORY_RELEASE);
    CHECK(!xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE));
    xrtAtomic32FetchAdd(&c->closes,1,XMEMORY_ACQ_REL);
    xrtAtomic32Store(&c->active,0,XMEMORY_RELEASE);
}
static const xnetstreameventsownershipv1 finalPolicy={sizeof(finalPolicy),
    {NULL,NULL,NULL,NULL,NULL,NULL,closeEvent},drop,&contextOps};
static void readEvent(xnetstream* stream,xnetbuf* buffer,ptr data)
{
    Context* c=data;if(xrtNetBufSize(buffer)<3)return;
    CHECK(c->stream==stream&&xrtNetBufConsume(buffer,3)==3);
    xrtAtomic32Store(&c->active,1,XMEMORY_RELEASE);
    CHECK(xrtNetStreamSetEventsOwnedV1(stream,retain(c->next),&finalPolicy));
    CHECK(!xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE));
    CHECK(xrtNetStreamAbort(stream));
    xrtAtomic32Store(&c->tail,1,XMEMORY_RELEASE);
    xrtAtomic32Store(&c->active,0,XMEMORY_RELEASE);
}
static const xnetstreameventsownershipv1 readPolicy={sizeof(readPolicy),
    {NULL,readEvent,NULL,NULL,NULL,NULL,closeEvent},drop,&contextOps};
static void expectError(xerrkind kind)
{CHECK(xrtGetError()&&xrtErrorKind(xrtGetError())==kind);xrtClearError();}
static void install(xnetworker* worker,ptr data)
{
    (void)worker;Scene* scene=data;Context* c=scene->first;
    /* Invalid/failed submissions consume no actual callback-data credit. */
    xnetstreameventsownershipv1 invalid=readPolicy;invalid.size=0;
    CHECK(!xrtNetStreamSetEventsOwnedV1(scene->client,c,&invalid));expectError(XERR_ARGUMENT);
    invalid=readPolicy;invalid.Drop=NULL;
    CHECK(!xrtNetStreamSetEventsOwnedV1(scene->client,c,&invalid));expectError(XERR_ARGUMENT);
    CHECK(!xrtNetStreamSetEventsOwnedV1(scene->client,NULL,&readPolicy));expectError(XERR_ARGUMENT);
    CHECK(__xrtAtomicRefLoad(&c->refs)==1);
    CHECK(xrtMemDebugFailAfter(0));retain(c);
    CHECK(!xrtNetStreamSetEventsOwnedV1(scene->client,c,&readPolicy));
    CHECK(xrtMemDebugFailTriggered());xrtMemDebugFailClear();expectError(XERR_MEMORY);++allocation_failures;
    CHECK(__xrtAtomicRefLoad(&c->refs)==2&&!scene->client->EventOwner);release(c);
    xfuture* pending=xrtNetStreamWaitAsync(scene->client,XNET_STREAM_WAIT_READ);CHECK(pending);
    retain(c);CHECK(!xrtNetStreamSetEventsOwnedV1(scene->client,c,&readPolicy));expectError(XERR_STATE);
    CHECK(!scene->client->EventOwner&&__xrtAtomicRefLoad(&c->refs)==2);release(c);
    CHECK(xrtFutureCancel(pending));xrtFutureDestroy(pending);
    /* Cancellation's actual wait-list unlink is worker-local. */
    xrtAtomic32Store(&scene->setup,1,XMEMORY_RELEASE);
}
static void installAfterWait(xnetworker* worker,ptr data)
{
    (void)worker;Scene* scene=data;Context* c=scene->first;
    CHECK(xrtNetStreamSetEventsOwnedV1(scene->client,retain(c),&readPolicy));
    CHECK(__xrtAtomicRefLoad(&c->refs)==2);
    CHECK(!xrtNetStreamSetData(scene->client,scene->second));expectError(XERR_STATE);
    CHECK(xrtNetStreamData(scene->client)==c);
    xrtAtomic32Store(&scene->setup,2,XMEMORY_RELEASE);
}
static void waitSetup(Scene* scene,uint32 stage)
{double due=__xrtWaitAfter(10000);while(xrtAtomic32Load(&scene->setup,XMEMORY_ACQUIRE)<stage){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}}
static void freezeBegin(xrtownershipscope* freeze)
{double due=__xrtWaitAfter(10000);while(!xrtOwnershipFreezeTryBegin(freeze)){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}}
typedef struct Edges {Context* context;unsigned visits;} Edges;
static bool edges(xrtownershipref ref,ptr data)
{Edges* observed=data;if(ref.Ops==&contextOps){CHECK(ref.Data==observed->context);++observed->visits;}return true;}
static void inspect(Scene* scene)
{
    xrtownershipscope freeze={0};double due=__xrtWaitAfter(10000);
    for(;;){freezeBegin(&freeze);if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(scene->engine))break;
        CHECK(xrtOwnershipScopeEnd(&freeze));CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    xrtownershipref ref=xrtNetStreamOwnership(scene->client);
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)scene;
    const xrtownershippreparationv1* prep=sentinel;
    CHECK(!xrtNetStreamOwnershipAdapterV1(ref,&prep)&&prep==sentinel);++policy_refusals;
    xnetstreameventsownershipv1 copy=readPolicy;
    const xnetstreameventsownershipv1* forged[]={&copy};
    CHECK(!xrtNetStreamOwnershipAdapterV2(ref,forged,1,&prep)&&prep==sentinel);++policy_refusals;
    CHECK(!xrtNetStreamOwnershipAdapterV2(ref,NULL,1,&prep)&&prep==sentinel);++policy_refusals;
    const xnetstreameventsownershipv1* policies[]={&readPolicy,&finalPolicy};
    CHECK(xrtNetStreamOwnershipAdapterV2(ref,policies,2,&prep)&&prep!=sentinel&&!prep->Ready(ref.Data));
    size_t count=0;CHECK(ref.Ops->Count(ref.Data,&count)&&count==4);
    Edges observed={scene->first,0};CHECK(ref.Ops->Trace(ref.Data,edges,&observed)&&observed.visits==1);
    CHECK(contextCount(scene->first,&count)&&count==2);
    CHECK(xrtOwnershipScopeEnd(&freeze));
}
static void blockedDrop(Scene* scene,Context* c,bool closed)
{
    waitSet(&c->dropEntered);CHECK(xrtAtomic32Load(&scene->first->tail,XMEMORY_ACQUIRE));
    if(closed)CHECK(xrtNetStreamState(scene->client)==XNET_STREAM_CLOSED);
    xrtownershipscope freeze={0};freezeBegin(&freeze);
    CHECK(xrtNetStreamOwnershipQuiescenceV1Get()->Busy(scene->client));
    CHECK(!(xrtAtomic32Load(&scene->client->OwnershipState,XMEMORY_ACQUIRE)&XRT_NET_TCP_OWNERSHIP_READY));
    size_t count=91;xrtownershipref ref=xrtNetStreamOwnership(scene->client);
    CHECK(!ref.Ops->Count(ref.Data,&count)&&count==91);
    const xrtownershippreparationv1* prep=(const xrtownershippreparationv1*)scene;
    const xnetstreameventsownershipv1* policies[]={&readPolicy,&finalPolicy};
    CHECK(!xrtNetStreamOwnershipAdapterV2(ref,policies,2,&prep)&&prep==(const xrtownershippreparationv1*)scene);
    CHECK(xrtOwnershipScopeEnd(&freeze));
    if(closed)CHECK(xrtNetEngineTryDestroy(scene->engine)==XNET_RETIRE_BUSY);
    xrtAtomic32Store(&c->dropGate,1,XMEMORY_RELEASE);
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;CHECK(!xrtGetError());xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount&&before.OverflowCount==after.OverflowCount&&before.UnderflowCount==after.UnderflowCount);
}
static void lifecycle(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene scene={0};xrtAtomic32Init(&scene.setup,0);
    xnetengineconfig engine;xrtNetEngineConfigInit(&engine);engine.Backend=backend;engine.Workers=1;engine.NodeCacheBytes=0;
    scene.engine=xrtNetEngineCreate(&engine);CHECK(scene.engine&&xrtNetEngineStart(scene.engine));
    xnetlistenconfig config;xrtNetListenConfigInit(&config);CHECK(xrtNetAddrLoopback(&config.Address,XNET_FAMILY_IPV4,0));
    scene.listener=xrtNetListen(scene.engine,&config,NULL,NULL,NULL);CHECK(scene.listener);
    xnetaddr address;CHECK(xrtNetListenerLocal(scene.listener,&address));
    scene.client=xrtNetStreamConnect(scene.engine,&address,0,&config.Stream,NULL,NULL);CHECK(scene.client);
    double due=__xrtWaitAfter(10000);
    while(xrtNetStreamState(scene.client)!=XNET_STREAM_OPEN){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    while(!(scene.server=xrtNetListenerAccept(scene.listener))){CHECK(!xrtGetError()&&!__xrtWaitExpired(due));xrtThreadYield();}
    scene.first=createContext(scene.client);scene.second=createContext(scene.client);scene.first->next=scene.second;
    CHECK(!xrtNetStreamSetEventsOwnedV1(scene.client,scene.first,&readPolicy));expectError(XERR_STATE);
    CHECK(__xrtAtomicRefLoad(&scene.first->refs)==1);
    CHECK(xrtNetEnginePost(scene.engine,0,install,&scene));waitSetup(&scene,1);
    /* Observe real wait cleanup before installing a read callback. */
    xrtownershipscope freeze={0};for(;;){freezeBegin(&freeze);
        if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(scene.engine))break;
        CHECK(xrtOwnershipScopeEnd(&freeze));CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    CHECK(!scene.client->ReadWaiters);CHECK(xrtOwnershipScopeEnd(&freeze));
    CHECK(xrtNetEnginePost(scene.engine,0,installAfterWait,&scene));waitSetup(&scene,2);inspect(&scene);
    CHECK(xrtNetStreamSend(scene.server,"a\0b",3)==XNET_RESULT_OK);
    blockedDrop(&scene,scene.first,false);blockedDrop(&scene,scene.second,true);
    due=__xrtWaitAfter(10000);for(;;){freezeBegin(&freeze);
        bool ready=!xrtNetStreamOwnershipQuiescenceV1Get()->Busy(scene.client);
        if(ready){const xrtownershippreparationv1* prep=NULL;
            CHECK(xrtNetStreamOwnershipAdapterV1(xrtNetStreamOwnership(scene.client),&prep)&&prep->Ready(scene.client));
            CHECK(!scene.client->EventOwner&&!scene.client->RetiredEventOwners);
        }CHECK(xrtOwnershipScopeEnd(&freeze));if(ready)break;CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    CHECK(xrtAtomic32Load(&scene.first->drops,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&scene.second->drops,XMEMORY_ACQUIRE)==1);
    CHECK(xrtAtomic32Load(&scene.second->closes,XMEMORY_ACQUIRE)==1);
    CHECK(__xrtAtomicRefLoad(&scene.first->refs)==1&&__xrtAtomicRefLoad(&scene.second->refs)==1);
    release(scene.first);release(scene.second);CHECK(xrtNetStreamAbort(scene.server));
    xrtNetStreamDestroy(scene.client);xrtNetStreamDestroy(scene.server);CHECK(xrtNetListenerClose(scene.listener));xrtNetListenerDestroy(scene.listener);
    due=__xrtWaitAfter(10000);for(;;){xnetretireresult result=xrtNetEngineTryDestroy(scene.engine);if(result==XNET_RETIRE_READY)break;
        CHECK(result==XNET_RETIRE_BUSY&&!__xrtWaitExpired(due));xrtThreadYield();}
    balanced(before);++cases;
}
int main(int argc,char** argv)
{
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
#if defined(_WIN32)
        backend=XNET_PORT_IOCP;
#elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
#endif
    }
    CHECK(xrtMemDebugEnable(true));for(unsigned i=0;i<3;++i)lifecycle(backend);
    testMemoryDebugDrain("owned Stream events allocations converge");
    printf("[PASS] Stream owned events backend=%d cases=%u policy-refusals=%u allocation-failures=%u real callback tails balanced\n",
        (int)backend,cases,policy_refusals,allocation_failures);return 0;
}
