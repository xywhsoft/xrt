/* Actual UDP producer/Watch/result graphs; only own literal loopback normal
 * traffic. Error payloads use the real private constructor entirely offline. */
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
enum {CAPACITY=80};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* prepare;bool ready;} Node;
typedef struct Graph {xnetengine* engine;xnetudp* sender; xnetudp* receiver;
    xfuture* futures[5];size_t futureCount;Node nodes[CAPACITY];size_t count;
    bool pinned,busy,wrong;Node* race;} Graph;
typedef struct Race {Node* node;xfuture* future;xatomic32 ready,go;} Race;
static unsigned graphs,roots,dropRaces,budgets,failures,deliveries,cancelRaces,closedShells,offlineErrors,unsupportedErrors,lateCommands;
static void freeze(xrtownershipscope* scope)
{double until=__xrtWaitAfter(5000);while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}}
static void state(xnetudp* udp,xnetudpstate wanted)
{double until=__xrtWaitAfter(5000);while(xrtNetUdpState(udp)!=wanted){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static const xrtownershipadapterv1* resolve(Graph* g,xrtownershipref ref,const xrtownershippreparationv1** prep)
{
    const xrtownershipquiescencev1* quiet[]={xrtNetEngineOwnershipQuiescenceV1Get(),xrtNetUdpOwnershipQuiescenceV1Get(),
        xrtNetBufPoolOwnershipQuiescenceV1Get(),xrtNetUdpFutureOwnershipQuiescenceV1Get(),xrtFutureOwnershipQuiescenceV1Get(),
        xrtCancelOwnershipQuiescenceV1Get(),xrtCancelWatchOwnershipQuiescenceV1Get()};
    for(size_t i=0;i<sizeof(quiet)/sizeof(*quiet);++i)if(ref.Ops==quiet[i]->Ops&&quiet[i]->Busy(ref.Data)){g->busy=true;return NULL;}
    const xnetruntimeownershipv1* runtime[]={xrtNetUdpRuntimeOwnershipV1Get()};
    const xfutureproducerownershipv1* producers[]={xrtNetUdpFutureProducerPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[]={xrtNetUdpFutureCancelPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[]={xrtNetUdpFuturePacketPayloadPolicyV1Get(),
        xrtNetUdpFutureErrorPayloadPolicyV1Get(),xrtNetUdpFutureBatchPayloadPolicyV1Get()};
    xfutureproducerownershipv1 copied=*producers[0];if(g->wrong)producers[0]=&copied;
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.PayloadPolicies=payloads;admission.PayloadPolicyCount=3;
    admission.ProducerPolicies=producers;admission.ProducerPolicyCount=1;
    admission.CancelWatchPolicies=cancels;admission.CancelWatchPolicyCount=1;
    const xrtownershipadapterv1* a=xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtime,1,prep);
    if(!a)a=xrtNetUdpOwnershipAdapterV1(ref,prep);
    if(!a)a=xrtNetBufPoolOwnershipAdapterV1(ref,prep);
    if(!a)a=xrtNetUdpFutureOwnershipAdapterV1(ref,prep);
    if(!a)a=xrtFutureOwnershipAdapterV4(ref,&admission,prep);
    if(!a)a=xrtCancelOwnershipAdapterV2(ref,cancels,1,prep);
    if(!a)a=xrtCancelWatchOwnershipAdapterV1(ref,cancels,1,prep);
    if(!a)a=xrtNetUdpPacketOwnershipAdapterV1(ref);
    if(!a)a=xrtNetUdpErrorPacketOwnershipAdapterV1(ref);
    if(!a)a=xrtNetUdpBatchOwnershipAdapterV1(ref);
    if(!a)a=xrtErrorOwnershipAdapterV1(ref);
    return a;
}
static bool admit(xrtownershipref ref,ptr raw)
{const xrtownershippreparationv1* prep=NULL;return resolve(raw,ref,&prep)!=NULL;}
static xrtownershipsnapshot* snapshot(Graph* g,bool external,xrtownershipscope* scope)
{
    double until=__xrtWaitAfter(5000);
    for(;;){freeze(scope);g->busy=false;
        xrtownershipref anchors[CAPACITY+8],slots[CAPACITY+10];size_t a=0,s=0;
        anchors[a++]=xrtNetEngineOwnership(g->engine);anchors[a++]=xrtNetUdpOwnership(g->sender);
        anchors[a++]=xrtNetUdpOwnership(g->receiver);
        for(size_t i=0;i<g->futureCount;++i)anchors[a++]=xrtFutureOwnership(g->futures[i]);
        for(size_t i=0;i<a;++i)if(!external||i+1<a)slots[s++]=anchors[i];
        if(g->pinned)for(size_t i=0;i<g->count;++i)anchors[a++]=slots[s++]=g->nodes[i].ref;
        if(g->race){slots[s++]=g->race->ref;slots[s++]=g->race->ref;}
        xrtownershipsnapshot* result=NULL;
        if(xrtOwnershipSnapshotCreate(anchors,a,slots,s,admit,g,&result))return result;
        CHECK(g->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(scope));CHECK(!__xrtWaitExpired(until));xrtThreadYield();
    }
}
static void discover(Graph* g,xrtownershipsnapshot* proof)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode item;
        CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);size_t n=0;
        while(n<g->count&&(g->nodes[n].ref.Data!=item.Reference.Data||g->nodes[n].ref.Ops!=item.Reference.Ops))++n;
        if(n<g->count)continue;
        CHECK(g->count<CAPACITY);Node* node=&g->nodes[g->count++];node->ref=item.Reference;
        node->adapter=resolve(g,node->ref,&node->prepare);CHECK(node->adapter);
        CHECK(node->adapter->Hold(node->ref.Data)&&node->adapter->Claim(node->ref.Data,g));
        if(node->prepare)CHECK(!node->adapter->Claim(node->ref.Data,&item));
    }
    g->pinned=true;
}
static void create(Graph* g,xnetportbackend backend)
{
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=backend;config.Workers=1;config.NodeCacheBytes=4096;
    g->engine=xrtNetEngineCreate(&config);CHECK(g->engine&&xrtNetEngineStart(g->engine));
    xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));xnetudpconfig udp;xrtNetUdpConfigInit(&udp);
    g->receiver=xrtNetUdpBind(g->engine,&address,0,&udp,NULL,NULL);CHECK(g->receiver);state(g->receiver,XNET_UDP_OPEN);
    CHECK(xrtNetUdpLocal(g->receiver,&address));g->sender=xrtNetUdpConnect(g->engine,&address,0,&udp,NULL,NULL);
    CHECK(g->sender);state(g->sender,XNET_UDP_OPEN);
}
static void closeOrdinary(Graph* g)
{
    CHECK(xrtNetUdpAbort(g->sender)&&xrtNetUdpAbort(g->receiver));
    xrtNetUdpDestroy(g->sender);xrtNetUdpDestroy(g->receiver);
    double until=__xrtWaitAfter(5000);
    for(;;){xnetretireresult r=xrtNetEngineTryDestroy(g->engine);if(r==XNET_RETIRE_READY)break;
        CHECK(r==XNET_RETIRE_BUSY&&!__xrtWaitExpired(until));xrtThreadYield();}
}
static int32 concurrentDrop(ptr raw)
{
    Race* race=raw;(void)xrtAtomic32FetchAdd(&race->ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&race->go,XMEMORY_ACQUIRE))xrtThreadYield();
    race->node->adapter->Drop(race->node->ref.Data);return 0;
}
static void lifecycle(xnetportbackend backend,unsigned order,unsigned outcome)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph g={0};create(&g,backend);
    g.futures[g.futureCount++]=xrtNetUdpWaitAsync(g.sender,XNET_UDP_WAIT_CLOSE);
    g.futures[g.futureCount++]=xrtNetUdpReceiveAsync(g.sender);
    g.futures[g.futureCount++]=xrtNetUdpReceiveBatchAsync(g.receiver,4);
    for(size_t i=0;i<g.futureCount;++i)CHECK(g.futures[i]);
    if(outcome==1){CHECK(xrtNetUdpSend(g.sender,"abc\0def",7)==XNET_RESULT_OK);
        CHECK(xrtFutureWaitFor(g.futures[2],5000)==XWAIT_OK&&xrtFutureState(g.futures[2])==XFUTURE_RESOLVED);
        xnetudpbatch* batch=xrtFutureValue(g.futures[2]);CHECK(batch&&xrtNetUdpBatchCount(batch)==1);
        xnetudppacket* packet=xrtNetUdpBatchPacket(batch,0);CHECK(packet&&xrtNetUdpPacketSize(packet)==7);
        CHECK(!memcmp(xrtNetUdpPacketData(packet),"abc\0def",7));}
    if(outcome==2){CHECK(xrtFutureCancel(g.futures[2])&&xrtFutureState(g.futures[2])==XFUTURE_CANCELLED);
        CHECK(xrtNetUdpState(g.receiver)==XNET_UDP_OPEN);}
    xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&g,true,&scope);bool rooted=false;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode n;CHECK(xrtOwnershipSnapshotNode(proof,i,&n));
        rooted=rooted||n.Reachable;if(n.Reference.Data==g.engine)CHECK(n.Reachable==(outcome==0));}
    CHECK(rooted);++roots;xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
    proof=snapshot(&g,false,&scope);unsigned producers=0,emptyBatches=0;bool resultBatch=false,resultPacket=false;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode n;CHECK(xrtOwnershipSnapshotNode(proof,i,&n));
        if(n.Reference.Ops==xrtNetUdpFutureOwnershipQuiescenceV1Get()->Ops){CHECK(n.StrongCount==3&&n.InternalCount==3);++producers;}
        const xrtownershipadapterv1* batch=xrtNetUdpBatchOwnershipAdapterV1(n.Reference);
        if(batch&&xrtNetUdpBatchCount((const xnetudpbatch*)n.Reference.Data)==0)++emptyBatches;
        if(outcome==1){xnetudpbatch* value=xrtFutureValue(g.futures[2]);
            if(n.Reference.Data==value){CHECK(n.StrongCount==1&&n.InternalCount==1);resultBatch=true;}
            if(n.Reference.Data==xrtNetUdpBatchPacket(value,0)){CHECK(n.StrongCount==1&&n.InternalCount==1);resultPacket=true;}}}
    CHECK(producers==(outcome?2u:3u)&&emptyBatches==(outcome?0u:1u));
    CHECK(outcome!=1||(resultBatch&&resultPacket));
    g.wrong=true;xrtownershipsnapshot* refused=NULL;xrtownershipref root=xrtFutureOwnership(g.futures[1]);
    CHECK(!xrtOwnershipSnapshotCreate(&root,1,&root,1,admit,&g,&refused)&&!refused);g.wrong=false;xrtClearError();
    const xrtownershippreparationv1* invalid=NULL;uint64 count=xrtAtomic64Load(&g.sender->ReceiveWaiters,XMEMORY_RELAXED);
    xrtAtomic64Store(&g.sender->ReceiveWaiters,count+1,XMEMORY_RELAXED);
    CHECK(!xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(g.sender),&invalid));
    xrtAtomic64Store(&g.sender->ReceiveWaiters,count,XMEMORY_RELAXED);
    discover(&g,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
    CHECK(!xrtNetUdpReceiveAsync(g.sender));xrtClearError();double until=__xrtWaitAfter(5000);
    for(;;){bool all=true;
        for(size_t step=0;step<g.count;++step){size_t i=order&1?g.count-1-step:step;Node* n=&g.nodes[i];
            if(!n->prepare||n->ready)continue;
            proof=snapshot(&g,false,&scope);discover(&g,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
            xrtownershipprepareresult r=n->prepare->Prepare(n->ref.Data,&g);CHECK(r!=XRT_OWNERSHIP_PREPARE_FAILED);
            n->ready=r==XRT_OWNERSHIP_PREPARE_READY;all=all&&n->ready;}
        if(all)break;
        CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
    proof=snapshot(&g,false,&scope);discover(&g,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<g.count&&!g.race;++i)if(g.nodes[i].ref.Ops==xrtNetUdpFutureOwnershipQuiescenceV1Get()->Ops){
        g.race=&g.nodes[i];CHECK(g.race->adapter->Hold(g.race->ref.Data)&&g.race->adapter->Hold(g.race->ref.Data));}
    CHECK(g.race&&xrtOwnershipScopeEnd(&scope));proof=snapshot(&g,false,&scope);discover(&g,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<g.count;++i){Node* n=&g.nodes[i];CHECK(!n->prepare||n->prepare->Ready(n->ref.Data));
        if(n->adapter->Clear)n->adapter->Clear(n->ref.Data,&g);}
    CHECK(xrtOwnershipScopeEnd(&scope));
    for(size_t step=0;step<g.count;++step){size_t i=order&2?g.count-1-step:step;Node* n=&g.nodes[i];
        CHECK(!n->adapter->Finish||n->adapter->Finish(n->ref.Data,&g));}
    CHECK(xrtNetEngineTryDestroy(g.engine)==XNET_RETIRE_READY);xrtNetUdpDestroy(g.sender);xrtNetUdpDestroy(g.receiver);
    for(size_t i=0;i<g.futureCount;++i)xrtFutureDestroy(g.futures[i]);
    for(size_t step=0;step<g.count;++step){size_t i=order&4?g.count-1-step:step;
        if(&g.nodes[i]!=g.race)g.nodes[i].adapter->Drop(g.nodes[i].ref.Data);}
    Race race={0};race.node=g.race;xrtAtomic32Init(&race.ready,0);xrtAtomic32Init(&race.go,0);
    xthread* t[2]={xrtThreadCreate(concurrentDrop,&race,0),xrtThreadCreate(concurrentDrop,&race,0)};CHECK(t[0]&&t[1]);
    until=__xrtWaitAfter(5000);while(xrtAtomic32Load(&race.ready,XMEMORY_ACQUIRE)!=2){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
    xrtAtomic32Store(&race.go,1,XMEMORY_RELEASE);g.race->adapter->Drop(g.race->ref.Data);
    for(size_t i=0;i<2;++i){CHECK(xrtThreadWaitFor(t[i],5000)==XWAIT_OK&&xrtThreadExitCode(t[i])==0);xrtThreadDestroy(t[i]);}
    ++dropRaces;balanced(&before);++graphs;
}
static void construction(xnetportbackend backend)
{
    for(unsigned kind=0;kind<3;++kind)for(unsigned budget=0;budget<12;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph g={0};create(&g,backend);
        xfuture* warm=xrtNetUdpReceiveAsync(g.receiver);CHECK(warm&&xrtFutureCancel(warm));xrtFutureDestroy(warm);
        xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&g,false,&scope);xrtOwnershipSnapshotDestroy(proof);
        CHECK(xrtMemDebugFailAfter(budget));++budgets;
        xfuture* future=kind==0?xrtNetUdpReceiveAsync(g.receiver):kind==1?xrtNetUdpReceiveBatchAsync(g.receiver,4):
            xrtNetUdpWaitAsync(g.sender,XNET_UDP_WAIT_CLOSE);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!future){CHECK(failed);++failures;}else CHECK(!failed);
        xrtClearError();CHECK(xrtOwnershipScopeEnd(&scope));if(future){CHECK(xrtFutureCancel(future));xrtFutureDestroy(future);}
        closeOrdinary(&g);balanced(&before);
    }
}
static int32 cancelWorker(ptr raw)
{
    Race* race=raw;(void)xrtAtomic32FetchAdd(&race->ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&race->go,XMEMORY_ACQUIRE))xrtThreadYield();
    bool requested=xrtFutureCancel(race->future);
    CHECK(requested||xrtFutureState(race->future)!=XFUTURE_PENDING);return 0;
}
static void cancellation(xnetportbackend backend)
{
    for(unsigned i=0;i<48;++i){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph g={0};create(&g,backend);
        Race race={0};race.future=i&1?xrtNetUdpReceiveBatchAsync(g.receiver,2):xrtNetUdpReceiveAsync(g.receiver);CHECK(race.future);
        xrtAtomic32Init(&race.ready,0);xrtAtomic32Init(&race.go,0);xthread* t=xrtThreadCreate(cancelWorker,&race,0);CHECK(t);
        double until=__xrtWaitAfter(5000);
        while(!xrtAtomic32Load(&race.ready,XMEMORY_ACQUIRE)){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
        xrtAtomic32Store(&race.go,1,XMEMORY_RELEASE);
        if(i&2)CHECK(xrtNetUdpClose(g.receiver));else CHECK(xrtNetUdpSend(g.sender,"x\0y",3)==XNET_RESULT_OK);
        CHECK(xrtThreadWaitFor(t,5000)==XWAIT_OK&&xrtThreadExitCode(t)==0);xrtThreadDestroy(t);
        CHECK(xrtFutureWaitFor(race.future,5000)==XWAIT_OK);xfuturestate terminal=xrtFutureState(race.future);
        CHECK(terminal==XFUTURE_RESOLVED||terminal==XFUTURE_CANCELLED||terminal==XFUTURE_CLOSED);
        closeOrdinary(&g);if(terminal==XFUTURE_RESOLVED){xnetudppacket* packet=i&1?
            xrtNetUdpBatchPacket(xrtFutureValue(race.future),0):xrtFutureValue(race.future);
            CHECK(packet&&xrtNetUdpPacketSize(packet)==3&&!memcmp(xrtNetUdpPacketData(packet),"x\0y",3));}
        xrtFutureDestroy(race.future);balanced(&before);++cancelRaces;
    }
}
static void lateClosed(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph g={0};create(&g,backend);
    CHECK(xrtNetUdpClose(g.sender)&&xrtNetUdpClose(g.receiver));state(g.sender,XNET_UDP_CLOSED);state(g.receiver,XNET_UDP_CLOSED);
    CHECK(xrtNetEngineDestroy(g.engine));
    xfuture* futures[]={xrtNetUdpReceiveAsync(g.receiver),xrtNetUdpReceiveBatchAsync(g.receiver,4),
        xrtNetUdpWaitAsync(g.sender,XNET_UDP_WAIT_CLOSE),xrtNetUdpWritableAsync(g.sender,1)};
    for(size_t i=0;i<4;++i){CHECK(futures[i]&&xrtFutureWaitFor(futures[i],5000)==XWAIT_OK);
        CHECK(xrtFutureState(futures[i])==(i==2?XFUTURE_RESOLVED:XFUTURE_CLOSED));xrtFutureDestroy(futures[i]);++closedShells;}
    xrtNetUdpDestroy(g.sender);xrtNetUdpDestroy(g.receiver);balanced(&before);
}
static void lateControl(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph g={0};create(&g,backend);
    CHECK(xrtNetUdpClose(g.sender)&&xrtNetUdpClose(g.receiver));state(g.sender,XNET_UDP_CLOSED);state(g.receiver,XNET_UDP_CLOSED);
    xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&g,false,&scope);xrtOwnershipSnapshotDestroy(proof);
    CHECK(!g.sender->RuntimeHeld&&!g.sender->ActiveDepth);
    /* The same actual coalesced command used by a late lifecycle wake owns
     * the ONLY remaining sender reference. Do not add a test pin. */
    __xrtNetUdpControlRequest(g.sender,XRT_NET_UDP_CONTROL_FINISH);xrtNetUdpDestroy(g.sender);xrtNetUdpDestroy(g.receiver);
    CHECK(xrtOwnershipScopeEnd(&scope));double until=__xrtWaitAfter(5000);
    for(;;){xnetretireresult r=xrtNetEngineTryDestroy(g.engine);if(r==XNET_RETIRE_READY)break;
        CHECK(r==XNET_RETIRE_BUSY&&!__xrtWaitExpired(until));xrtThreadYield();}
    balanced(&before);++lateCommands;
}
static void conditions(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph g={0};create(&g,backend);
    xfuture* immediate[]={xrtNetUdpWaitAsync(g.sender,XNET_UDP_WAIT_OPEN),xrtNetUdpWaitAsync(g.sender,XNET_UDP_WAIT_DRAIN),
        xrtNetUdpWritableAsync(g.sender,0)};
    for(size_t i=0;i<3;++i){CHECK(immediate[i]&&xrtFutureWaitFor(immediate[i],5000)==XWAIT_OK&&
        xrtFutureState(immediate[i])==XFUTURE_RESOLVED);xrtFutureDestroy(immediate[i]);}
    xfuture* ready=xrtNetUdpWaitAsync(g.receiver,XNET_UDP_WAIT_RECEIVE);CHECK(ready);
    CHECK(!xrtNetUdpReceiveAsync(g.receiver)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    CHECK(xrtNetUdpSend(g.sender,"a\0b",3)==XNET_RESULT_OK);
    CHECK(xrtFutureWaitFor(ready,5000)==XWAIT_OK&&xrtFutureState(ready)==XFUTURE_RESOLVED);xrtFutureDestroy(ready);
    xnetudppacket* packet=xrtNetUdpReceive(g.receiver);CHECK(packet&&xrtNetUdpPacketSize(packet)==3);xrtNetUdpPacketDestroy(packet);
    xfuture* consume=xrtNetUdpReceiveAsync(g.receiver);CHECK(consume);
    CHECK(!xrtNetUdpWaitAsync(g.receiver,XNET_UDP_WAIT_RECEIVE)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    CHECK(xrtFutureCancel(consume));xrtFutureDestroy(consume);closeOrdinary(&g);balanced(&before);
}
/* Actual unlink transfers the source-list owner to this local completion.
 * The source Promise is independently made terminal to cover refused delivery
 * without malformed traffic, a fake object or a production test hook. */
static void rejectedDelivery(xnetportbackend backend)
{
    for(unsigned batch=0;batch<2;++batch){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph g={0};create(&g,backend);
        xfuture* future=batch?xrtNetUdpReceiveBatchAsync(g.receiver,2):xrtNetUdpReceiveAsync(g.receiver);CHECK(future);
        CHECK(__xrtNetUdpOwnershipEnter(g.receiver,false));xrtownershipscope scope={0};CHECK(xrtOwnershipMutationBegin(&scope));
        __xrtSpinLock(&g.receiver->ReceiveLock);__xrt_net_udp_wait* waiter=g.receiver->ReceiveFutureHead;
        CHECK(waiter&&__xrtNetUdpWaitRemove(g.receiver,waiter));__xrtSpinUnlock(&g.receiver->ReceiveLock);
        xnetaddr address;CHECK(xrtNetUdpLocal(g.receiver,&address));
        xnetudppacket* packet=__xrtNetUdpPacketCreate(&address,NULL,"a\0b",3,0);CHECK(packet);
        if(batch){xrtAtomicPtrStore(&waiter->Batch->Packets[0],packet,XMEMORY_RELEASE);waiter->Batch->Count=1;}
        else waiter->Packet=packet;
        waiter->Result=__XRT_NET_UDP_WAIT_RECEIVE;CHECK(xrtOwnershipScopeEnd(&scope));
        CHECK(xrtFutureCancel(future)&&xrtFutureState(future)==XFUTURE_PENDING);
        CHECK(xrtPromiseCancel(waiter->Promise)&&xrtFutureState(future)==XFUTURE_CANCELLED);
        __xrtNetUdpFutureFinishList(waiter);__xrtNetUdpOwnershipLeave(g.receiver);
        xrtFutureDestroy(future);closeOrdinary(&g);balanced(&before);++deliveries;
    }
}
static void offlinePayloads(void)
{
    for(unsigned kind=0;kind<3;++kind){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);xnetaddr address;
        CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));xnetdgramerror error={0};
        xnetudppacket* packet=kind==1?NULL:__xrtNetUdpPacketCreate(&address,NULL,"a\0b",3,0);
        xnetudperrorpacket* err=kind==1?__xrtNetUdpErrorPacketCreate(&error,"a\0b",3):NULL;
        xnetudpbatch* batch=kind==2?__xrtNetUdpBatchCreate(1):NULL;
        CHECK(kind==1?err!=NULL:packet!=NULL);
        if(kind==2){CHECK(batch);xrtAtomicPtrStore(&batch->Packets[0],packet,XMEMORY_RELEASE);batch->Count=1;}
        ptr payload=kind==0?(ptr)packet:kind==1?(ptr)err:(ptr)batch;
        const xfuturepayloadownershipv1* policy=kind==0?xrtNetUdpFuturePacketPayloadPolicyV1Get():
            kind==1?xrtNetUdpFutureErrorPayloadPolicyV1Get():xrtNetUdpFutureBatchPayloadPolicyV1Get();
        xfuture* future=NULL;xpromise* promise=xrtPromiseCreate(&future,NULL);CHECK(promise&&future);
        CHECK(xrtPromiseResolveOwnedPolicyV1(promise,payload,policy));xrtPromiseDestroy(promise);
        xrtownershipscope scope={0};freeze(&scope);Graph g={0};xrtownershipref root=xrtFutureOwnership(future);
        xrtownershipsnapshot* proof=NULL;CHECK(xrtOwnershipSnapshotCreate(&root,1,&root,1,admit,&g,&proof));bool found=false;
        for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode n;CHECK(xrtOwnershipSnapshotNode(proof,i,&n)&&!n.Reachable);
            if(n.Reference.Data==payload){CHECK(n.StrongCount==1&&n.InternalCount==1);found=true;}}
        CHECK(found);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
        if(err)CHECK(xrtNetUdpErrorPacketRef(err));else CHECK(xrtNetUdpPacketRef(packet));
        xrtFutureDestroy(future);
        CHECK(err?!memcmp(xrtNetUdpErrorPacketData(err),"a\0b",3):!memcmp(xrtNetUdpPacketData(packet),"a\0b",3));
        if(err)xrtNetUdpErrorPacketDestroy(err);else xrtNetUdpPacketDestroy(packet);
        balanced(&before);++deliveries;
    }
}
typedef struct ErrorWaitView {const __xrt_net_udp_wait* wait;unsigned sources,engines,promises,watches;} ErrorWaitView;
static bool errorWaitEdge(xrtownershipref ref,ptr raw)
{
    ErrorWaitView* view=raw;const __xrt_net_udp_wait* wait=view->wait;
    xrtownershipref expected[4];
    expected[0]=xrtNetUdpOwnership(wait->Udp);expected[1]=xrtNetEngineOwnership(wait->Udp->Engine);
    expected[2]=xrtPromiseOwnership(wait->Promise);expected[3]=xrtCancelWatchOwnership(wait->Watch);
    if(ref.Data==expected[0].Data&&ref.Ops==expected[0].Ops)++view->sources;
    else if(ref.Data==expected[1].Data&&ref.Ops==expected[1].Ops)++view->engines;
    else if(ref.Data==expected[2].Data&&ref.Ops==expected[2].Ops)++view->promises;
    else if(ref.Data==expected[3].Data&&ref.Ops==expected[3].Ops)++view->watches;
    else CHECK(false);
    return true;
}
static bool errorWaitNode(xrtownershipref ref,ptr raw)
{
    ErrorWaitView* view=raw;size_t count=0;CHECK(ref.Data==view->wait&&ref.Ops->Count(ref.Data,&count)&&count==3);
    CHECK(ref.Ops->Trace(ref.Data,errorWaitEdge,view));return true;
}
static void offlineErrorWait(xnetportbackend backend)
{
    /* Real unpublished constructor: no start command, socket receive, error
     * traffic or OS error-queue operation is performed. Only native queues,
     * condition subscriptions and owned delivery are exercised. */
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);xnetengineconfig config;
    xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;
    xnetengine* engine=xrtNetEngineCreate(&config);CHECK(engine&&xrtNetEngineStart(engine));
    xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    xnetsocket socket=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);
    CHECK(socket&&xrtNetSocketBind(socket,&address)&&xrtNetSocketLocal(socket,&address));
    xnetudpconfig options;xrtNetUdpConfigInit(&options);options.ReceiveErrors=true;
    xnetudp* udp=__xrtNetUdpCreate(engine,xrtNetEngineWorker(engine,0),socket,&address,NULL,&options,NULL,NULL);
    if(!udp){uint32 caps=xrtNetPortCapabilities(__xrtNetWorkerPortBorrow(xrtNetEngineWorker(engine,0)));
        CHECK((caps&XNET_PORT_CAP_COMPLETION)&&!(caps&XNET_PORT_CAP_DGRAM_ERROR)&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);
        CHECK(xrtNetSocketClose(socket)&&xrtNetEngineDestroy(engine));balanced(&before);++unsupportedErrors;return;}
    __xrtNetUdpOwnershipLeave(udp); /* end the real unpublished constructor borrow */
    xfuture* future=xrtNetUdpReceiveErrorAsync(udp);CHECK(future&&xrtFutureState(future)==XFUTURE_PENDING);
    xrtownershipscope scope={0};double until=__xrtWaitAfter(5000);
    for(;;){freeze(&scope);if(!xrtNetUdpOwnershipQuiescenceV1Get()->Busy(udp))break;
        CHECK(xrtOwnershipScopeEnd(&scope)&&!__xrtWaitExpired(until));xrtThreadYield();}
    /* The constructor still has a genuine unpublished START request. Its
     * whole graph must NOT be certified; inspect only the actual error-list
     * storage and exact contained edges, without faking a started source. */
    const xrtownershippreparationv1* unsupported=NULL;
    CHECK(!xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(udp),&unsupported));
    ErrorWaitView view={udp->Errors->WaitHead,0,0,0,0};CHECK(view.wait);
    CHECK(__xrtNetUdpWaitOwnershipWalk(udp,errorWaitNode,&view));
    CHECK(view.sources==1&&view.engines==1&&view.promises==1&&view.watches==1);
    CHECK(xrtOwnershipScopeEnd(&scope));
    CHECK(__xrtNetUdpOwnershipEnter(udp,false));xnetdgramerror description={0};
    xnetudperrorpacket* packet=__xrtNetUdpErrorPacketCreate(&description,"e\0r",3);CHECK(packet&&__xrtNetUdpQueueError(udp,packet));
    __xrtNetUdpOwnershipLeave(udp);CHECK(xrtFutureWaitFor(future,5000)==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
    CHECK(xrtFutureValue(future)==packet&&!memcmp(xrtNetUdpErrorPacketData(packet),"e\0r",3));xrtFutureDestroy(future);
    future=xrtNetUdpWaitAsync(udp,XNET_UDP_WAIT_ERROR);CHECK(future&&xrtFutureCancel(future));xrtFutureDestroy(future);
    CHECK(__xrtNetUdpOwnershipEnter(udp,false)&&xrtNetSocketClose(socket));udp->Socket=NULL;
    __xrtNetUdpRollbackCreated(udp);xrtNetUdpDestroy(udp);CHECK(xrtNetEngineDestroy(engine));balanced(&before);++offlineErrors;
}
int main(int argc,char** argv)
{
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #if defined(_WIN32) || defined(_WIN64)
        backend=XNET_PORT_IOCP;
        #elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
        #endif
    }
    CHECK(xrtMemDebugEnable(true));
    if(argc>1&&!strcmp(argv[1],"late-only")){
        puts("late command tail regression");fflush(stdout);lateControl(backend);return 0;
    }
    for(unsigned order=0;order<8;++order)for(unsigned outcome=0;outcome<3;++outcome)lifecycle(backend,order,outcome);
    construction(backend);CHECK(failures>0&&failures<budgets);cancellation(backend);lateClosed(backend);
    rejectedDelivery(backend);offlinePayloads();offlineErrorWait(backend);lateControl(backend);conditions(backend);
    CHECK(offlineErrors+unsupportedErrors==1);
    printf("UDP Future ownership: graphs=%u roots=%u drop-races=%u OOM=%u failed=%u deliveries=%u cancel-races=%u closed-shells=%u offline-errors=%u unsupported-errors=%u late-commands=%u; balanced\n",
        graphs,roots,dropRaces,budgets,failures,deliveries,cancelRaces,closedShells,offlineErrors,unsupportedErrors,lateCommands);return 0;
}
