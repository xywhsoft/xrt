/* Native base UDP graphs and lifecycle; only owned literal loopback traffic.
 * No DNS, public endpoint, ICMP/error-queue or network-security tests. */
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
#define WAIT INT64_C(5000)
enum {MAX_NODES=64};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* prepare;bool ready;} Node;
typedef struct Graph {xnetengine* engine;xnetudp* a; xnetudp* b;const xnetudppacket* queued;
    Node nodes[MAX_NODES];size_t count;bool pinned,missingRuntime;} Graph;
static unsigned graphs,ordinary,retirements,borrows,ooms,oomFailures,observations,rejections;
static unsigned offlineErrors,unsupportedErrors;
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void state(xnetudp* u,xnetudpstate wanted)
{double until=__xrtWaitAfter(WAIT);while(xrtNetUdpState(u)!=wanted){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}}
static void freeze(xrtownershipscope* f)
{double until=__xrtWaitAfter(WAIT);while(!xrtOwnershipFreezeTryBegin(f)){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,Graph* g,const xrtownershippreparationv1** prep)
{
    const xnetruntimeownershipv1* runtime[]={xrtNetUdpRuntimeOwnershipV1Get()};
    if(ref.Ops==xrtNetEngineOwnership(g->engine).Ops)
        return xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtime,g->missingRuntime?0:1,prep);
    if(ref.Ops==runtime[0]->Ops)return xrtNetUdpOwnershipAdapterV1(ref,prep);
    if(ref.Ops==xrtNetBufPoolOwnershipQuiescenceV1Get()->Ops)return xrtNetBufPoolOwnershipAdapterV1(ref,prep);
    const xrtownershipadapterv1* result=xrtNetUdpPacketOwnershipAdapterV1(ref);
    if(!result)result=xrtNetUdpErrorPacketOwnershipAdapterV1(ref);
    if(!result)result=xrtErrorOwnershipAdapterV1(ref);
    return result;
}
static bool admit(xrtownershipref ref,ptr raw)
{const xrtownershippreparationv1* prep=NULL;return resolve(ref,raw,&prep)!=NULL;}
static bool busy(Graph* g)
{return xrtNetEngineOwnershipQuiescenceV1Get()->Busy(g->engine)||
    xrtNetUdpOwnershipQuiescenceV1Get()->Busy(g->a)||xrtNetUdpOwnershipQuiescenceV1Get()->Busy(g->b);}
static void snapshot(Graph* g,xrtownershipscope* f)
{
    double until=__xrtWaitAfter(WAIT);
    for(;;){freeze(f);
        if(!busy(g)){
            xrtownershipref roots[MAX_NODES+3]={xrtNetEngineOwnership(g->engine),xrtNetUdpOwnership(g->a),xrtNetUdpOwnership(g->b)};
            xrtownershipref slots[MAX_NODES+3]={roots[0],roots[1],roots[2]};size_t count=3;
            if(g->pinned)for(size_t i=0;i<g->count;++i)roots[count]=slots[count]=g->nodes[i].ref,++count;
            xrtownershipsnapshot* proof=NULL;
            CHECK(xrtOwnershipSnapshotCreate(roots,count,slots,count,admit,g,&proof));
            bool packet=false;
            for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode item;
                CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);
                if(item.Reference.Data==g->queued)packet=true;
                size_t n=0;while(n<g->count&&(g->nodes[n].ref.Data!=item.Reference.Data||g->nodes[n].ref.Ops!=item.Reference.Ops))++n;
                if(n==g->count){CHECK(n<MAX_NODES);Node* node=&g->nodes[g->count++];node->ref=item.Reference;
                    node->adapter=resolve(node->ref,g,&node->prepare);CHECK(node->adapter);
                    if(g->pinned)CHECK(node->adapter->Hold(node->ref.Data)&&node->adapter->Claim(node->ref.Data,g));
                }
            }
            CHECK(!g->queued||packet);xrtOwnershipSnapshotDestroy(proof);++graphs;return;
        }
        CHECK(xrtOwnershipScopeEnd(f));CHECK(!__xrtWaitExpired(until));xrtThreadYield();
    }
}
static void setup(Graph* g,xnetportbackend backend)
{
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.NodeCacheBytes=0;
    g->engine=xrtNetEngineCreate(&config);CHECK(g->engine&&xrtNetEngineStart(g->engine));
    xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));xnetudpconfig c;xrtNetUdpConfigInit(&c);
    g->b=xrtNetUdpBind(g->engine,&address,0,&c,NULL,NULL);CHECK(g->b);state(g->b,XNET_UDP_OPEN);
    CHECK(xrtNetUdpLocal(g->b,&address));g->a=xrtNetUdpConnect(g->engine,&address,0,&c,NULL,NULL);CHECK(g->a);state(g->a,XNET_UDP_OPEN);
    CHECK(xrtNetUdpSend(g->a,"a\0b",3)==XNET_RESULT_OK);double until=__xrtWaitAfter(WAIT);
    while(xrtNetUdpQueued(g->b)!=1){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
    g->queued=g->b->ReceiveHead;CHECK(g->queued);
}
static void run(xnetportbackend backend,bool retire,bool borrow,bool missing)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph g={0};setup(&g,backend);g.missingRuntime=missing;
    #if defined(XRT_FEATURE_NET_UDP_FUTURE)
    xfuture* pending=xrtNetUdpReceiveAsync(g.a);CHECK(pending&&xrtFutureState(pending)==XFUTURE_PENDING);
    xrtownershipscope pendingFreeze={0};double pendingUntil=__xrtWaitAfter(WAIT);
    for(;;){freeze(&pendingFreeze);if(!busy(&g))break;
        CHECK(xrtOwnershipScopeEnd(&pendingFreeze)&&!__xrtWaitExpired(pendingUntil));xrtThreadYield();}
    const xrtownershippreparationv1* supported=NULL;
    CHECK(xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(g.a),&supported));
    CHECK(xrtOwnershipScopeEnd(&pendingFreeze));CHECK(xrtFutureCancel(pending));
    CHECK(xrtFutureWaitFor(pending,WAIT)==XWAIT_OK&&xrtFutureState(pending)==XFUTURE_CANCELLED);xrtFutureDestroy(pending);
    #endif
    xrtownershipscope f={0};snapshot(&g,&f);
    const xrtownershippreparationv1* guard=NULL;
    xrtAtomicPtrStore(&g.a->Data,&g,XMEMORY_RELEASE);
    CHECK(!xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(g.a),&guard));++rejections;
    xrtAtomicPtrStore(&g.a->Data,NULL,XMEMORY_RELEASE);
    xnetcompletionproc saved=g.b->Receives[0].Completion.Proc;g.b->Receives[0].Completion.Proc=NULL;
    CHECK(!xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(g.b),&guard));++rejections;
    g.b->Receives[0].Completion.Proc=saved;
    xnetudppacket* link=(xnetudppacket*)g.queued;CHECK(!link->Next);
    link->Next=link;xrtAtomic64Store(&g.b->ReceiveQueued,UINT64_MAX,XMEMORY_RELEASE);
    CHECK(!xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(g.b),&guard));++rejections;
    link->Next=NULL;xrtAtomic64Store(&g.b->ReceiveQueued,1,XMEMORY_RELEASE);
    CHECK(__xrtNetUdpOwnershipEnter(g.a,false));CHECK(xrtNetUdpOwnershipQuiescenceV1Get()->Busy(g.a));
    CHECK(!xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(g.a),&guard));++rejections;__xrtNetUdpOwnershipLeave(g.a);
    if(!retire){CHECK(xrtOwnershipScopeEnd(&f));
        if(borrow)CHECK(__xrtNetUdpOwnershipEnter(g.a,false));
        CHECK(xrtNetUdpClose(g.a)&&xrtNetUdpClose(g.b));state(g.a,XNET_UDP_CLOSED);state(g.b,XNET_UDP_CLOSED);
        xnetudppacket* late=xrtNetUdpReceive(g.b);CHECK(late&&xrtNetUdpPacketSize(late)==3);
        const uint8* original=xrtNetUdpPacketData(late);CHECK(xrtNetEngineDestroy(g.engine));
        if(borrow){CHECK(g.a->EngineStorageHeld&&g.a->RuntimeHeld);__xrtNetUdpOwnershipLeave(g.a);++borrows;}
        CHECK(!g.a->EngineStorageHeld&&!g.a->RuntimeHeld&&xrtNetUdpSend(g.a,NULL,0)==XNET_RESULT_CLOSED);
        CHECK(xrtNetUdpClose(g.a)&&xrtNetUdpAbort(g.b));
        CHECK(xrtNetUdpPacketData(late)==original&&!memcmp(original,"a\0b",3));xrtNetUdpPacketDestroy(late);
        xrtNetUdpDestroy(g.a);xrtNetUdpDestroy(g.b);balanced(&before);++ordinary;return;
    }
    for(size_t i=0;i<g.count;++i){Node* node=&g.nodes[i];CHECK(node->adapter->Hold(node->ref.Data)&&node->adapter->Claim(node->ref.Data,&g));}
    g.pinned=true;CHECK(xrtOwnershipScopeEnd(&f));
    CHECK(xrtNetUdpSend(g.a,NULL,0)==XNET_RESULT_CLOSED&&!xrtNetUdpReceive(g.b));xrtClearError();
    double until=__xrtWaitAfter(WAIT);
    for(;;){bool all=true;
        for(size_t i=0;i<g.count;++i){Node* node=&g.nodes[i];if(node->ready)continue;
            snapshot(&g,&f);CHECK(xrtOwnershipScopeEnd(&f));
            xrtownershipprepareresult r=node->prepare?node->prepare->Prepare(node->ref.Data,&g):XRT_OWNERSHIP_PREPARE_READY;
            CHECK(r!=XRT_OWNERSHIP_PREPARE_FAILED);node->ready=r==XRT_OWNERSHIP_PREPARE_READY;all=all&&node->ready;
        }
        if(all)break;
        CHECK(!__xrtWaitExpired(until));xrtThreadYield();
    }
    snapshot(&g,&f);for(size_t i=0;i<g.count;++i){Node* n=&g.nodes[i];CHECK(!n->prepare||n->prepare->Ready(n->ref.Data));
        if(n->adapter->Clear)n->adapter->Clear(n->ref.Data,&g);}
    CHECK(xrtOwnershipScopeEnd(&f));for(size_t i=0;i<g.count;++i){Node* n=&g.nodes[i];CHECK(!n->adapter->Finish||n->adapter->Finish(n->ref.Data,&g));}
    CHECK(xrtNetEngineTryDestroy(g.engine)==XNET_RETIRE_READY);xrtNetUdpDestroy(g.a);xrtNetUdpDestroy(g.b);
    for(size_t i=1;i<g.count;++i)g.nodes[i].adapter->Drop(g.nodes[i].ref.Data);
    g.nodes[0].adapter->Drop(g.nodes[0].ref.Data);balanced(&before);++retirements;
}
typedef struct Observation {xatomic32 closed,seen,released;unsigned edges;bool opaque;unsigned kind;} Observation;
static bool ownEdge(xrtownershipref ref,ptr raw)
{Observation* o=raw;CHECK(ref.Ops==xrtNetUdpRuntimeOwnershipV1Get()->Ops);++o->edges;return true;}
static void high(xnetudp* u,size_t bytes,size_t packets,ptr raw)
{
    Observation* o=raw;CHECK(bytes==4&&packets==1);
    if(o->opaque)CHECK(!__xrtNetUdpSendsOwnership(u,ownEdge,o));
    else {CHECK(__xrtNetUdpSendsOwnership(u,ownEdge,o)&&o->edges==1);
        uint32 tag=u->SendHead->PayloadOwnership;u->SendHead->PayloadOwnership=0;
        CHECK(!__xrtNetUdpSendsOwnership(u,NULL,NULL));u->SendHead->PayloadOwnership=tag;}
    CHECK(xrtNetUdpAbort(u));xrtNetUdpDestroy(u);xrtAtomic32Store(&o->seen,1,XMEMORY_RELEASE);
}
static void closed(xnetudp* u,xnetresult r,const xerror* e,ptr raw)
{(void)u;(void)r;(void)e;Observation* o=raw;xrtAtomic32Store(&o->closed,1,XMEMORY_RELEASE);}
static void release(ptr raw,cbytes data,size_t size)
{(void)size;Observation* o=raw;xrtFree((ptr)data);xrtAtomic32FetchAdd(&o->released,1,XMEMORY_RELEASE);}
static void reenter(xnetportbackend backend,unsigned kind)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Observation o={0};o.opaque=kind==2;o.kind=kind;
    xnetengineconfig c;xrtNetEngineConfigInit(&c);c.Workers=1;c.Backend=backend;c.NodeCacheBytes=0;
    xnetengine* engine=xrtNetEngineCreate(&c);CHECK(engine&&xrtNetEngineStart(engine));
    xnetudpconfig config;xrtNetUdpConfigInit(&config);config.SendHighWater=4;config.SendLowWater=0;config.SendLimit=8;
    xnetudpevents events={0};events.HighWater=high;events.Close=closed;xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    xnetudp* u=xrtNetUdpBind(engine,&address,0,&config,&events,&o);CHECK(u);state(u,XNET_UDP_OPEN);CHECK(xrtNetUdpLocal(u,&address));
    const xrtownershippreparationv1* prep=NULL;xrtownershipscope frozen={0};
    double until=__xrtWaitAfter(WAIT);for(;;){freeze(&frozen);if(!xrtNetUdpOwnershipQuiescenceV1Get()->Busy(u))break;
        CHECK(xrtOwnershipScopeEnd(&frozen)&&!__xrtWaitExpired(until));xrtThreadYield();}
    CHECK(!xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(u),&prep));++rejections;CHECK(xrtOwnershipScopeEnd(&frozen));
    xnetresult r;
    if(!kind)r=xrtNetUdpSendTo(u,&address,"a\0bc",4);
    else {void* data=xrtMalloc(4);CHECK(data);memcpy(data,"a\0bc",4);
        r=kind==1?xrtNetUdpSendTakeTo(u,&address,data,4):xrtNetUdpSendRefTo(u,&address,data,4,release,&o);}
    CHECK(r==XNET_RESULT_OK);until=__xrtWaitAfter(WAIT);
    while(!xrtAtomic32Load(&o.closed,XMEMORY_ACQUIRE)){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
    CHECK(xrtNetEngineDestroy(engine)&&xrtAtomic32Load(&o.seen,XMEMORY_ACQUIRE)&&
        xrtAtomic32Load(&o.released,XMEMORY_ACQUIRE)==(kind==2));balanced(&before);++observations;
}
static void construct(xnetportbackend backend)
{
    for(unsigned budget=0;budget<8;++budget){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        xnetengineconfig c;xrtNetEngineConfigInit(&c);c.Workers=1;c.Backend=backend;c.NodeCacheBytes=0;
        xnetengine* engine=xrtNetEngineCreate(&c);CHECK(engine&&xrtNetEngineStart(engine));xnetaddr address;
        CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));CHECK(xrtMemDebugFailAfter(budget));
        xnetudp* u=xrtNetUdpBind(engine,&address,0,NULL,NULL,NULL);bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!u){CHECK(failed);++oomFailures;}else {CHECK(xrtNetUdpAbort(u));state(u,XNET_UDP_CLOSED);xrtNetUdpDestroy(u);}
        CHECK(xrtNetEngineDestroy(engine));balanced(&before);++ooms;
    }
}
typedef struct ErrorView {xnetudperrorpacket* packet;unsigned edges;} ErrorView;
static bool errorEdge(xrtownershipref ref,ptr raw)
{
    ErrorView* view=raw;xrtownershipref expected=xrtNetUdpErrorPacketOwnership(view->packet);
    CHECK(ref.Data==expected.Data&&ref.Ops==expected.Ops);++view->edges;return true;
}
static void offlineErrorQueue(xnetportbackend backend)
{
    /* The genuine unpublished constructor and queue, not a fabricated UDP
     * record or a network error probe. No start command is ever submitted. */
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;
    xnetengine* engine=xrtNetEngineCreate(&config);CHECK(engine&&xrtNetEngineStart(engine));
    xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    xnetsocket socket=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);
    CHECK(socket&&xrtNetSocketBind(socket,&address)&&xrtNetSocketLocal(socket,&address));
    xnetudpconfig options;xrtNetUdpConfigInit(&options);options.ReceiveErrors=true;
    xnetudp* u=__xrtNetUdpCreate(engine,xrtNetEngineWorker(engine,0),socket,&address,NULL,&options,NULL,NULL);
    if(!u){
        uint32 caps=xrtNetPortCapabilities(__xrtNetWorkerPortBorrow(xrtNetEngineWorker(engine,0)));
        CHECK((caps&XNET_PORT_CAP_COMPLETION)&&!(caps&XNET_PORT_CAP_DGRAM_ERROR)&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);
        CHECK(xrtNetSocketClose(socket)&&xrtNetEngineDestroy(engine));balanced(&before);++unsupportedErrors;return;
    }
    xnetdgramerror description={0};xnetudperrorpacket* packet=__xrtNetUdpErrorPacketCreate(&description,"e\0r",3);CHECK(packet);
    CHECK(__xrtNetUdpQueueError(u,packet));ErrorView view={packet,0};
    CHECK(__xrtNetUdpQueues(u,errorEdge,&view)&&view.edges==1);
    xrtAtomic64Store(&u->Errors->QueuedBytes,4,XMEMORY_RELEASE);CHECK(!__xrtNetUdpQueues(u,NULL,NULL));
    xrtAtomic64Store(&u->Errors->QueuedBytes,3,XMEMORY_RELEASE);
    xnetudperrorpacket* taken=xrtNetUdpReceiveError(u);CHECK(taken==packet&&xrtNetUdpQueuedErrors(u)==0);
    CHECK(xrtNetUdpErrorPacketSize(taken)==3&&!memcmp(xrtNetUdpErrorPacketData(taken),"e\0r",3));
    CHECK(xrtNetSocketClose(socket));u->Socket=NULL;__xrtNetUdpRollbackCreated(u);xrtNetUdpDestroy(u);
    CHECK(xrtNetEngineDestroy(engine));xrtNetUdpErrorPacketDestroy(taken);balanced(&before);++offlineErrors;
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_ARGUMENT);xrtClearError();
    bool missing=argc>1&&!strcmp(argv[1],"missing-runtime");
    for(unsigned native=0;native<2;++native){xnetportbackend backend=XNET_PORT_SELECT;
        if(native){
            #ifdef _WIN32
            backend=XNET_PORT_IOCP;
            #else
            backend=XNET_PORT_EPOLL;
            #endif
        }
        run(backend,false,false,missing);run(backend,false,true,false);run(backend,true,false,false);
        for(unsigned kind=0;kind<3;++kind)reenter(backend,kind);
        construct(backend);offlineErrorQueue(backend);
    }
    CHECK(ordinary==4&&retirements==2&&borrows==2&&observations==6&&ooms==16&&oomFailures>0&&offlineErrors>0&&
        offlineErrors+unsupportedErrors==2&&xrtMemDebugReset());
    printf("UDP native base ownership: graphs=%u ordinary=%u retired=%u closed-call-borrows=%u observed-sends=%u rejected-opaque=%u OOM=%u failures=%u offline-errors=%u unsupported-errors=%u; exact Engine/pool/packet/send graph, stable CLOSED, checked preparation, balanced\n",
        graphs,ordinary,retirements,borrows,observations,rejections,ooms,oomFailures,offlineErrors,unsupportedErrors);return 0;
}
