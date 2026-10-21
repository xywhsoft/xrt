/* Ordinary self-owned literal loopback, offline ownership/OOM only. No DNS,
 * external peers, invalid wire traffic, probes or security acceptance. */
#ifdef NET_LISTENER_EVENTS_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include <xrt/detail/wait.h>
#include "../test.h"
#ifndef NET_LISTENER_EVENTS_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_tcp.h"
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%s:%u: ",__FILE__,(unsigned)__LINE__);testRequire(false,#c);}}while(0)

typedef struct Context {
    volatile int32 refs;
    xatomicptr accepted; /* one transferred Stream reference, not a raw alias */
    xatomic32 active,closeOnAccept,acceptEntered,acceptGate,acceptTail;
    xatomic32 closes,closeTail,dropEntered,dropGate,drops;
} Context;
static unsigned cases,refusals,budgets,failures;
static void waitSet(xatomic32* flag)
{
    double due=__xrtWaitAfter(10000);
    while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
}
static void freezeBegin(xrtownershipscope* scope)
{
    double due=__xrtWaitAfter(10000);
    while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
}
static void idle(xnetengine* engine,xrtownershipscope* scope)
{
    double due=__xrtWaitAfter(10000);
    for(;;){freezeBegin(scope);if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(engine))return;
        CHECK(xrtOwnershipScopeEnd(scope));CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
}
static bool contextCount(const void* data,size_t* count)
{
    const Context* c=data;if(!c||!count)return false;int32 refs=__xrtAtomicRefLoad(&c->refs);
    if(refs<=0)return false;
    *count=(size_t)refs;return true;
}
static bool contextTrace(const void* data,xrtownershipvisitor visit,ptr arg)
{
    const Context* c=data;size_t count;if(!visit||!contextCount(c,&count))return false;
    xnetstream* stream=xrtAtomicPtrLoad(&c->accepted,XMEMORY_ACQUIRE);
    return !stream||visit(xrtNetStreamOwnership(stream),arg);
}
static const xrtownershipops contextOps={contextCount,contextTrace};
static Context* createContext(void)
{
    Context* c=xrtCalloc(1,sizeof(*c));CHECK(c);c->refs=1;xrtAtomicPtrInit(&c->accepted,NULL);
    xrtAtomic32Init(&c->active,0);xrtAtomic32Init(&c->closeOnAccept,0);
    xrtAtomic32Init(&c->acceptEntered,0);xrtAtomic32Init(&c->acceptGate,0);xrtAtomic32Init(&c->acceptTail,0);
    xrtAtomic32Init(&c->closes,0);xrtAtomic32Init(&c->closeTail,0);xrtAtomic32Init(&c->dropEntered,0);
    xrtAtomic32Init(&c->dropGate,0);xrtAtomic32Init(&c->drops,0);return c;
}
static Context* retain(Context* c){CHECK(xrtOwnershipRefRetain(&c->refs)>0);return c;}
static void release(Context* c)
{
    int32 refs=xrtOwnershipRefRelease(&c->refs);CHECK(refs>=0);
    if(!refs){CHECK(!xrtAtomicPtrLoad(&c->accepted,XMEMORY_ACQUIRE));xrtFree(c);}
}
static void drop(const void* data)
{
    Context* c=(Context*)data;CHECK(!xrtAtomic32Load(&c->active,XMEMORY_ACQUIRE));
    CHECK(xrtAtomic32Load(&c->closeTail,XMEMORY_ACQUIRE));
    /* No mutation/freeze lock is held across foreign Drop code. */
    xrtownershipscope scope={0};freezeBegin(&scope);CHECK(xrtOwnershipScopeEnd(&scope));
    xrtAtomic32Store(&c->dropEntered,1,XMEMORY_RELEASE);waitSet(&c->dropGate);
    CHECK(xrtAtomic32FetchAdd(&c->drops,1,XMEMORY_ACQ_REL)==0);release(c);
}
static void closeEvent(xnetlistener* listener,ptr data)
{
    Context* c=data;CHECK(xrtNetWorkerIsCurrent(xrtNetListenerWorker(listener)));
    CHECK(xrtNetListenerData(listener)==c&&xrtNetListenerState(listener)==XNET_LISTENER_CLOSED);
    CHECK(!xrtAtomic32Load(&c->active,XMEMORY_ACQUIRE));xrtAtomic32FetchAdd(&c->active,1,XMEMORY_ACQ_REL);
    CHECK(!xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE));CHECK(xrtNetListenerClose(listener));
    CHECK(xrtAtomic32FetchAdd(&c->closes,1,XMEMORY_ACQ_REL)==0);
    xrtAtomic32Store(&c->closeTail,1,XMEMORY_RELEASE);xrtAtomic32FetchSub(&c->active,1,XMEMORY_ACQ_REL);
}
static bool acceptEvent(xnetlistener* listener,xnetstream* stream,ptr data)
{
    Context* c=data;CHECK(xrtNetWorkerIsCurrent(xrtNetStreamWorker(stream)));
    CHECK(xrtNetListenerData(listener)==c&&!xrtNetStreamData(stream));
    CHECK(!stream->Events.Open&&!stream->Events.Read&&!stream->Events.Close);
    CHECK(!xrtAtomicPtrExchange(&c->accepted,stream,XMEMORY_ACQ_REL));
    if(xrtAtomic32Load(&c->closeOnAccept,XMEMORY_ACQUIRE)){
        /* Second round-robin dispatch runs on a different Worker. The actual
         * dispatch reference, not a fake context root, protects this borrow. */
        CHECK(xrtNetListenerWorker(listener)!=xrtNetStreamWorker(stream));
        xrtAtomic32FetchAdd(&c->active,1,XMEMORY_ACQ_REL);CHECK(xrtNetListenerClose(listener));
        CHECK(!xrtAtomic32Load(&c->closes,XMEMORY_ACQUIRE)&&!xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE));
        xrtAtomic32Store(&c->acceptEntered,1,XMEMORY_RELEASE);waitSet(&c->acceptGate);
        xrtAtomic32Store(&c->acceptTail,1,XMEMORY_RELEASE);xrtAtomic32FetchSub(&c->active,1,XMEMORY_ACQ_REL);
    }
    return true;
}
static const xnetlistenereventsownershipv1 policy={sizeof(policy),{acceptEvent,NULL,closeEvent},drop,&contextOps};
static const xnetlistenereventsownershipv1 pullPolicy={sizeof(pullPolicy),{NULL,NULL,closeEvent},drop,&contextOps};
static xnetengine* createEngine(xnetportbackend backend,unsigned workers)
{
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=backend;config.Workers=workers;config.NodeCacheBytes=0;
    xnetengine* engine=xrtNetEngineCreate(&config);CHECK(engine&&xrtNetEngineStart(engine));return engine;
}
static void destroyEngine(xnetengine* engine)
{
    double due=__xrtWaitAfter(10000);for(;;){xnetretireresult state=xrtNetEngineTryDestroy(engine);
        if(state==XNET_RETIRE_READY)return;
        CHECK(state==XNET_RETIRE_BUSY&&!__xrtWaitExpired(due));xrtThreadYield();}
}
static void balanced(xmemdebugsnapshot before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount&&before.OverflowCount==after.OverflowCount&&before.UnderflowCount==after.UnderflowCount);
}
typedef struct Admission {const xnetlistenereventsownershipv1* policy;} Admission;
static bool admit(xrtownershipref ref,ptr data)
{
    Admission* a=data;const xrtownershippreparationv1* prep=NULL;
    const xnetruntimeownershipv1* runtime[]={xrtNetListenerRuntimeOwnershipV1Get()};
    if(ref.Ops==&contextOps)return true;
    if(ref.Ops==xrtNetEngineOwnershipQuiescenceV1Get()->Ops)
        return xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtime,1,&prep)!=NULL;
    if(ref.Ops==xrtNetListenerOwnershipQuiescenceV1Get()->Ops)
        return xrtNetListenerOwnershipAdapterV2(ref,&a->policy,1,&prep)!=NULL;
    return xrtNetBufPoolOwnershipAdapterV1(ref,&prep)!=NULL;
}
static void inspect(xnetengine* engine,xnetlistener* listener,Context* c,unsigned workers,
    const xnetlistenereventsownershipv1* bound)
{
    xerror* error=xrtErrorCreate(XERR_STATE,"listener.ownership.fixture",1,"preserve diagnostic");CHECK(error);
    xrtSetError(error);xrtErrorFree(error);const xerror* diagnostic=xrtGetError();
    xrtownershipscope scope={0};idle(engine,&scope);
    xrtownershipref ref=xrtNetListenerOwnership(listener);size_t count=81;
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)c;const xrtownershippreparationv1* prep=sentinel;
    CHECK(!xrtNetListenerOwnershipAdapterV1(ref,&prep)&&prep==sentinel);++refusals;
    xnetlistenereventsownershipv1 copied=*bound;const xnetlistenereventsownershipv1* wrong[]={&copied};
    CHECK(!xrtNetListenerOwnershipAdapterV2(ref,wrong,1,&prep)&&prep==sentinel);++refusals;
    CHECK(!xrtNetListenerOwnershipAdapterV2(ref,NULL,1,&prep)&&prep==sentinel);++refusals;
    /* Unknown metadata must be refused by identity BEFORE dereferencing it. */
    listener->EventPolicy=(const xnetlistenereventsownershipv1*)(uintptr_t)1;
    CHECK(!xrtNetListenerOwnershipAdapterV2(ref,&bound,1,&prep)&&prep==sentinel);++refusals;
    CHECK(!xrtNetListenerOwnershipAdapterV1(ref,&prep)&&prep==sentinel);++refusals;
    listener->EventPolicy=bound;
    CHECK(!xrtNetListenerOwnershipAdapterV2((xrtownershipref){listener,&contextOps},&bound,1,&prep)&&prep==sentinel);++refusals;
    CHECK(xrtGetError()==diagnostic);xrtClearError();
    CHECK(xrtNetListenerOwnershipAdapterV2(ref,&bound,1,&prep)&&prep!=sentinel&&!prep->Ready(listener));
    CHECK(ref.Ops->Count(listener,&count)&&count==2&&contextCount(c,&count)&&count==2);
    xrtownershipref roots[3];roots[0]=xrtNetEngineOwnership(engine);roots[1]=ref;
    roots[2]=(xrtownershipref){c,&contextOps};
    Admission admission={bound};xrtownershipsnapshot* snapshot=NULL;
    CHECK(xrtOwnershipSnapshotCreate(roots,3,roots,3,admit,&admission,&snapshot));
    CHECK(xrtOwnershipSnapshotNodeCount(snapshot)==3+workers);
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){xrtownershipnode node;
        CHECK(xrtOwnershipSnapshotNode(snapshot,i,&node)&&!node.Reachable);}
    xrtOwnershipSnapshotDestroy(snapshot);CHECK(!xrtGetError());CHECK(xrtOwnershipScopeEnd(&scope));
}
static xnetstream* takeAccepted(Context* c)
{
    double due=__xrtWaitAfter(10000);xnetstream* stream;
    while(!(stream=xrtAtomicPtrExchange(&c->accepted,NULL,XMEMORY_ACQ_REL))){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    return stream;
}
static void waitDropped(xnetengine* engine,xnetlistener* listener,Context* c)
{
    double due=__xrtWaitAfter(10000);xrtownershipscope scope={0};
    for(;;){idle(engine,&scope);bool busy=xrtNetListenerOwnershipQuiescenceV1Get()->Busy(listener);
        if(!busy){const xrtownershippreparationv1* prep=NULL;
            CHECK(xrtNetListenerOwnershipAdapterV1(xrtNetListenerOwnership(listener),&prep)&&prep->Ready(listener));
            CHECK(!listener->EventPolicy&&!xrtNetListenerData(listener)&&__xrtAtomicRefLoad(&c->refs)==1);
            CHECK(xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE)==1);}
        CHECK(xrtOwnershipScopeEnd(&scope));if(!busy)return;CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
}
static void lifecycle(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Context* c=createContext();
    xnetengine* engine=createEngine(backend,2);xnetlistenconfig config;xrtNetListenConfigInit(&config);
    CHECK(xrtNetAddrLoopback(&config.Address,XNET_FAMILY_IPV4,0));config.Distribution=XNET_ACCEPT_ROUND_ROBIN;
    config.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    xnetlistener* listener=xrtNetListenOwnedV1(engine,&config,retain(c),&policy);CHECK(listener);inspect(engine,listener,c,2,&policy);
    xnetaddr address;CHECK(xrtNetListenerLocal(listener,&address));
    xnetstream* first=xrtNetStreamConnect(engine,&address,0,&config.Stream,NULL,NULL);CHECK(first);
    xnetstream* firstPeer=takeAccepted(c);CHECK(firstPeer);
    /* Ensure first dispatch completed before arming the second callback. */
    xrtownershipscope scope={0};idle(engine,&scope);CHECK(xrtOwnershipScopeEnd(&scope));
    xrtAtomic32Store(&c->closeOnAccept,1,XMEMORY_RELEASE);
    xnetstream* second=xrtNetStreamConnect(engine,&address,0,&config.Stream,NULL,NULL);CHECK(second);waitSet(&c->acceptEntered);
    freezeBegin(&scope);CHECK(xrtNetListenerOwnershipQuiescenceV1Get()->Busy(listener));
    size_t count=81;CHECK(!xrtNetListenerOwnership(listener).Ops->Count(listener,&count)&&count==81);
    CHECK(!xrtAtomic32Load(&c->closes,XMEMORY_ACQUIRE)&&!xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE));
    CHECK(xrtOwnershipScopeEnd(&scope));xrtAtomic32Store(&c->acceptGate,1,XMEMORY_RELEASE);
    waitSet(&c->dropEntered);CHECK(xrtAtomic32Load(&c->acceptTail,XMEMORY_ACQUIRE));
    CHECK(xrtNetListenerState(listener)==XNET_LISTENER_CLOSED);
    freezeBegin(&scope);CHECK(xrtNetListenerOwnershipQuiescenceV1Get()->Busy(listener));
    CHECK(!(xrtAtomic32Load(&listener->OwnershipState,XMEMORY_ACQUIRE)&XRT_NET_TCP_OWNERSHIP_READY));
    CHECK(listener->RuntimeHeld&&listener->EngineHeld&&listener->RuntimeOwner.Data==listener);
    const xrtownershippreparationv1* prep=(const xrtownershippreparationv1*)c;const xnetlistenereventsownershipv1* policies[]={&policy};
    CHECK(!xrtNetListenerOwnershipAdapterV2(xrtNetListenerOwnership(listener),policies,1,&prep)&&prep==(const xrtownershippreparationv1*)c);
    CHECK(xrtOwnershipScopeEnd(&scope));CHECK(xrtNetEngineTryDestroy(engine)==XNET_RETIRE_BUSY);
    xnetstream* secondPeer=takeAccepted(c);xrtAtomic32Store(&c->dropGate,1,XMEMORY_RELEASE);waitDropped(engine,listener,c);
    CHECK(xrtNetStreamAbort(first)&&xrtNetStreamAbort(firstPeer)&&xrtNetStreamAbort(second)&&xrtNetStreamAbort(secondPeer));
    xrtNetStreamDestroy(first);xrtNetStreamDestroy(firstPeer);xrtNetStreamDestroy(second);xrtNetStreamDestroy(secondPeer);
    xrtNetListenerDestroy(listener);release(c);destroyEngine(engine);balanced(before);++cases;
}
static void prepareLifecycle(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Context* c=createContext();xrtAtomic32Store(&c->dropGate,1,XMEMORY_RELEASE);
    xnetengine* engine=createEngine(backend,1);xnetlistenconfig config;xrtNetListenConfigInit(&config);
    CHECK(xrtNetAddrLoopback(&config.Address,XNET_FAMILY_IPV4,0));
    xnetlistener* listener=xrtNetListenOwnedV1(engine,&config,retain(c),&pullPolicy);CHECK(listener);inspect(engine,listener,c,1,&pullPolicy);
    xrtownershipscope scope={0};idle(engine,&scope);const xrtownershippreparationv1* prep=NULL;
    const xnetlistenereventsownershipv1* policies[]={&pullPolicy};xrtownershipref ref=xrtNetListenerOwnership(listener);
    const xrtownershipadapterv1* adapter=xrtNetListenerOwnershipAdapterV2(ref,policies,1,&prep);CHECK(adapter&&prep);
    CHECK(adapter->Hold(listener)&&adapter->Claim(listener,c));CHECK(xrtOwnershipScopeEnd(&scope));
    CHECK(!xrtNetListenerAccept(listener));xrtClearError();double due=__xrtWaitAfter(10000);
    for(;;){xrtownershipprepareresult state=prep->Prepare(listener,c);if(state==XRT_OWNERSHIP_PREPARE_READY)break;
        CHECK(state==XRT_OWNERSHIP_PREPARE_BUSY&&!__xrtWaitExpired(due));xrtThreadYield();}
    waitDropped(engine,listener,c);freezeBegin(&scope);CHECK(prep->Ready(listener));adapter->Clear(listener,c);
    CHECK(xrtOwnershipScopeEnd(&scope));xrtNetListenerDestroy(listener);adapter->Drop(listener);release(c);destroyEngine(engine);balanced(before);++cases;
}
typedef struct Gate {xatomic32 entered,open;} Gate;
static void gateTask(xnetworker* worker,ptr data)
{(void)worker;Gate* gate=data;xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);waitSet(&gate->open);}
static void allocations(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Context* c=createContext();xrtAtomic32Store(&c->dropGate,1,XMEMORY_RELEASE);
    xnetengine* engine=createEngine(backend,1);Gate gate;xrtAtomic32Init(&gate.entered,0);xrtAtomic32Init(&gate.open,0);
    CHECK(xrtNetEnginePost(engine,0,gateTask,&gate));waitSet(&gate.entered);
    xnetlistenconfig config;xrtNetListenConfigInit(&config);CHECK(xrtNetAddrLoopback(&config.Address,XNET_FAMILY_IPV4,0));
    xnetlistener* listener=NULL;
    for(size_t budget=0;budget<96;++budget){retain(c);CHECK(xrtMemDebugFailAfter(budget));
        listener=xrtNetListenOwnedV1(engine,&config,c,&pullPolicy);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        if(listener){CHECK(!hit);break;}
        CHECK(hit&&__xrtAtomicRefLoad(&c->refs)==2&&!xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE));++failures;release(c);xrtClearError();}
    CHECK(listener);CHECK(xrtNetListenerClose(listener));xrtAtomic32Store(&gate.open,1,XMEMORY_RELEASE);
    waitDropped(engine,listener,c);xrtNetListenerDestroy(listener);release(c);destroyEngine(engine);balanced(before);++cases;
}
static void invalidAndRaw(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Context* c=createContext();xnetengine* engine=createEngine(backend,1);
    xnetlistenconfig config;xrtNetListenConfigInit(&config);CHECK(xrtNetAddrLoopback(&config.Address,XNET_FAMILY_IPV4,0));
    xrtownershipops missing=contextOps;xnetlistenereventsownershipv1 bad=pullPolicy;
    for(unsigned mode=0;mode<5;++mode){bad=pullPolicy;missing=contextOps;
        if(mode==0)bad.size=0;
        if(mode==1)bad.Drop=NULL;
        if(mode==2)bad.Ops=NULL;
        if(mode==3){missing.Count=NULL;bad.Ops=&missing;}
        if(mode==4){missing.Trace=NULL;bad.Ops=&missing;}
        CHECK(!xrtNetListenOwnedV1(engine,&config,c,&bad));CHECK(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();++refusals;}
    CHECK(!xrtNetListenOwnedV1(engine,&config,NULL,&pullPolicy));xrtClearError();++refusals;
    CHECK(!xrtNetListenOwnedV1(NULL,&config,c,&pullPolicy));xrtClearError();++refusals;
    CHECK(__xrtAtomicRefLoad(&c->refs)==1&&!xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE));
    xnetlistener* listener=xrtNetListen(engine,&config,&pullPolicy.Events,NULL,c);CHECK(listener);
    xrtownershipscope scope={0};idle(engine,&scope);const xrtownershippreparationv1* prep=(const xrtownershippreparationv1*)c;
    const xnetlistenereventsownershipv1* policies[]={&pullPolicy};
    CHECK(!xrtNetListenerOwnershipAdapterV2(xrtNetListenerOwnership(listener),policies,1,&prep)&&prep==(const xrtownershippreparationv1*)c);
    CHECK(xrtOwnershipScopeEnd(&scope));CHECK(xrtNetListenerClose(listener));xrtNetListenerDestroy(listener);destroyEngine(engine);
    CHECK(__xrtAtomicRefLoad(&c->refs)==1&&!xrtAtomic32Load(&c->drops,XMEMORY_ACQUIRE));release(c);balanced(before);++cases;
}
int main(int argc,char** argv)
{
    xnetportbackend backend=XNET_PORT_SELECT;if(argc>1&&!strcmp(argv[1],"native")){
#if defined(_WIN32)
        backend=XNET_PORT_IOCP;
#elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
#endif
    }
    CHECK(xrtMemDebugEnable(true));invalidAndRaw(backend);allocations(backend);prepareLifecycle(backend);
    for(unsigned i=0;i<3;++i)lifecycle(backend);
    testMemoryDebugDrain("owned Listener event allocations converge");
    printf("[PASS] Listener owned events backend=%d cases=%u policy-refusals=%u OOM-budgets=%u OOM-failures=%u callback tails balanced\n",
        (int)backend,cases,refusals,budgets,failures);return 0;
}
