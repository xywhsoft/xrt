/* Exact native result owners, ordinary literal loopback and offline allocation
 * checks only. No external network, ICMP/error-queue probing or protocol tests. */
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
#define WAIT INT64_C(5000)
static unsigned graphChecks,attempts,winners,allocationFailures,freezeChecks;
static void complete(xfuture* future)
{CHECK(future&&xrtFutureWaitFor(future,WAIT)==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);}
static bool admit(xrtownershipref ref,ptr unused)
{
    (void)unused;
    return xrtNetUdpPacketOwnershipAdapterV1(ref)||xrtNetUdpErrorPacketOwnershipAdapterV1(ref)||xrtNetUdpBatchOwnershipAdapterV1(ref);
}
static void freeze(xrtownershipscope* frozen)
{
    double until=__xrtWaitAfter(WAIT);
    while(!xrtOwnershipFreezeTryBegin(frozen)){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void graph(xnetudpbatch* batch,size_t nodes,const xnetudppacket* external,bool accounted)
{
    xrtownershipscope frozen={0};freeze(&frozen);
    xrtownershipref roots[2]={xrtNetUdpBatchOwnership(batch),xrtNetUdpPacketOwnership(external)};
    xrtownershipref slots[2]={roots[0],roots[1]};xrtownershipsnapshot* proof=NULL;
    CHECK(xrtOwnershipSnapshotCreate(roots,external?2:1,slots,external&&accounted?2:1,admit,NULL,&proof));
    CHECK(xrtOwnershipSnapshotNodeCount(proof)==nodes);
    bool found=false;
    for(size_t i=0;i<nodes;++i){xrtownershipnode node;CHECK(xrtOwnershipSnapshotNode(proof,i,&node));
        bool isExternal=node.Reference.Data==external;
        CHECK(node.Reachable==(isExternal&&!accounted));if(isExternal)found=true;
    }
    CHECK(!external||found);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&frozen));++graphChecks;
}
static void leaf(xrtownershipref ref)
{
    const xrtownershipadapterv1* a=xrtNetUdpPacketOwnershipAdapterV1(ref);
    if(!a)a=xrtNetUdpErrorPacketOwnershipAdapterV1(ref);
    CHECK(a&&a->Hold(ref.Data));
    size_t count=0;CHECK(ref.Ops->Count(ref.Data,&count)&&count==2);a->Drop(ref.Data);
    xrtownershipscope frozen={0};freeze(&frozen);xrtownershipsnapshot* proof=NULL;
    CHECK(xrtOwnershipSnapshotCreate(&ref,1,&ref,1,admit,NULL,&proof)&&xrtOwnershipSnapshotNodeCount(proof)==1);
    xrtownershipnode node;CHECK(xrtOwnershipSnapshotNode(proof,0,&node)&&!node.Reachable);
    CHECK(a->Claim(ref.Data,&frozen));a->Restore(ref.Data,&frozen);xrtOwnershipSnapshotDestroy(proof);
    CHECK(xrtOwnershipScopeEnd(&frozen));++graphChecks;
}
static void offline(void)
{
    xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,1234));
    xnetdgrammeta meta={0};xnetdgramerror error={0};
    for(unsigned budget=0;budget<3;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);CHECK(xrtMemDebugFailAfter(budget));
        xnetudppacket* packet=__xrtNetUdpPacketCreate(&address,&meta,"a\0b",3,0);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        CHECK(failed==!packet);if(failed)++allocationFailures;
        if(packet){CHECK(xrtNetUdpPacketSize(packet)==3&&!memcmp(xrtNetUdpPacketData(packet),"a\0b",3));leaf(xrtNetUdpPacketOwnership(packet));}
        xrtNetUdpPacketDestroy(packet);balanced(&before);
        xrtMemDebugSnapshot(&before);CHECK(xrtMemDebugFailAfter(budget));
        xnetudperrorpacket* diagnostic=__xrtNetUdpErrorPacketCreate(&error,"e\0r",3);
        failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();CHECK(failed==!diagnostic);if(failed)++allocationFailures;
        if(diagnostic){CHECK(xrtNetUdpErrorPacketSize(diagnostic)==3&&!memcmp(xrtNetUdpErrorPacketData(diagnostic),"e\0r",3));
            leaf(xrtNetUdpErrorPacketOwnership(diagnostic));}
        xrtNetUdpErrorPacketDestroy(diagnostic);balanced(&before);
        xrtMemDebugSnapshot(&before);CHECK(xrtMemDebugFailAfter(budget));xnetudpbatch* empty=__xrtNetUdpBatchCreate(4);
        failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();CHECK(failed==!empty);if(failed)++allocationFailures;
        if(empty)graph(empty,1,NULL,false);
        xrtNetUdpBatchDestroy(empty);balanced(&before);
    }
    CHECK(!xrtNetUdpPacketOwnershipAdapterV1((xrtownershipref){NULL,NULL}));
    CHECK(!xrtNetUdpErrorPacketOwnershipAdapterV1((xrtownershipref){NULL,NULL}));
    CHECK(!xrtNetUdpBatchOwnershipAdapterV1((xrtownershipref){NULL,NULL}));
}
typedef struct Take {xnetudpbatch* batch;xnetudppacket* packet;xatomic32 *start,*ready,*attempted,*finished;} Take;
static int32 takeThread(ptr raw)
{
    Take* task=raw;xrtAtomic32FetchAdd(task->ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(task->start,XMEMORY_ACQUIRE))xrtThreadYield();
    xrtAtomic32FetchAdd(task->attempted,1,XMEMORY_RELEASE);
    task->packet=xrtNetUdpBatchTake(task->batch,1);
    xrtAtomic32FetchAdd(task->finished,1,XMEMORY_RELEASE);return 0;
}
static void run(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.NodeCacheBytes=0;
    xnetengine* engine=xrtNetEngineCreate(&config);CHECK(engine&&xrtNetEngineStart(engine));
    xnetudpconfig options;xrtNetUdpConfigInit(&options);xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    xnetudp* receiver=xrtNetUdpBind(engine,&address,0,&options,NULL,NULL);CHECK(receiver);
    xfuture* open=xrtNetUdpWaitAsync(receiver,XNET_UDP_WAIT_OPEN);complete(open);xrtFutureDestroy(open);
    CHECK(xrtNetUdpLocal(receiver,&address));xnetudp* sender=xrtNetUdpConnect(engine,&address,0,&options,NULL,NULL);CHECK(sender);
    open=xrtNetUdpWaitAsync(sender,XNET_UDP_WAIT_OPEN);complete(open);xrtFutureDestroy(open);
    xnetudppacket* late=NULL;const uint8* lateData=NULL;
    for(unsigned round=0;round<4;++round){
        for(unsigned i=0;i<4;++i)CHECK(xrtNetUdpSend(sender,"a\0b",i)==XNET_RESULT_OK);
        double until=__xrtWaitAfter(WAIT);
        while(xrtNetUdpQueued(receiver)<4){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
        xfuture* future=xrtNetUdpReceiveBatchAsync(receiver,4);complete(future);
        xnetudpbatch* batch=xrtNetUdpBatchRef(xrtFutureValue(future));CHECK(batch);xrtFutureDestroy(future);
        CHECK(xrtNetUdpBatchCount(batch)==4);graph(batch,5,NULL,false);
        xnetudppacket* kept=xrtNetUdpPacketRef(xrtNetUdpBatchPacket(batch,0));CHECK(kept);
        CHECK(!xrtNetUdpPacketOwnershipAdapterV1(xrtNetUdpBatchOwnership(batch))&&
            !xrtNetUdpBatchOwnershipAdapterV1(xrtNetUdpPacketOwnership(kept)));
        graph(batch,5,kept,false);graph(batch,5,kept,true);xrtNetUdpPacketDestroy(kept);
        xrtownershipscope frozen={0};freeze(&frozen);
        /* The Freeze holder may make balanced controlled cursor holds; it
         * must not confuse those with a foreign thread's mutation admission. */
        xnetudpbatch* pinned=xrtNetUdpBatchRef(batch);CHECK(pinned==batch);
        kept=xrtNetUdpPacketRef(xrtNetUdpBatchPacket(batch,0));CHECK(kept);
        xrtNetUdpBatchDestroy(pinned);xrtNetUdpPacketDestroy(kept);
        CHECK(xrtOwnershipScopeEnd(&frozen));xrtClearError();graph(batch,5,NULL,false);
        xatomic32 start,ready,attempted,finished;xrtAtomic32Init(&start,0);xrtAtomic32Init(&ready,0);
        xrtAtomic32Init(&attempted,0);xrtAtomic32Init(&finished,0);Take take[8]={0};xthread* threads[8]={0};
        for(unsigned i=0;i<8;++i){take[i]=(Take){batch,NULL,&start,&ready,&attempted,&finished};
            threads[i]=xrtThreadCreate(takeThread,&take[i],0);CHECK(threads[i]);}
        until=__xrtWaitAfter(WAIT);
        while(xrtAtomic32Load(&ready,XMEMORY_ACQUIRE)!=8){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
        freeze(&frozen);xrtAtomic32Store(&start,1,XMEMORY_RELEASE);
        /* Fixed read-only inspection work, not a wait/join on a frozen mutator.
         * No participant must run before the holder can release Freeze. */
        for(unsigned inspection=0;inspection<1024;++inspection){
            xrtownershipref root=xrtNetUdpBatchOwnership(batch);xrtownershipsnapshot* proof=NULL;
            CHECK(xrtOwnershipSnapshotCreate(&root,1,&root,1,admit,NULL,&proof));
            CHECK(xrtOwnershipSnapshotNodeCount(proof)==5);xrtOwnershipSnapshotDestroy(proof);
            CHECK(xrtAtomic32Load(&finished,XMEMORY_ACQUIRE)==0);++freezeChecks;
        }
        unsigned observed=xrtAtomic32Load(&attempted,XMEMORY_ACQUIRE);
        CHECK(xrtOwnershipScopeEnd(&frozen));CHECK(observed>0);
        unsigned won=0;
        for(unsigned i=0;i<8;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK);xrtThreadDestroy(threads[i]);
            if(take[i].packet){CHECK(xrtNetUdpPacketSize(take[i].packet)==1&&xrtNetUdpPacketData(take[i].packet)[0]=='a');++won;}
        }
        CHECK(won==1&&!xrtNetUdpBatchPacket(batch,1)&&!xrtNetUdpBatchTake(batch,1));attempts+=8;winners+=won;
        graph(batch,4,NULL,false);
        for(unsigned i=0;i<8;++i)xrtNetUdpPacketDestroy(take[i].packet);
        CHECK(xrtMemDebugFailAfter(0));xnetudppacket* last=xrtNetUdpBatchTake(batch,3);
        CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();CHECK(last&&xrtNetUdpPacketSize(last)==3);
        const uint8* original=xrtNetUdpPacketData(last);xrtNetUdpBatchDestroy(batch);
        CHECK(xrtNetUdpPacketData(last)==original&&!memcmp(original,"a\0b",3));
        if(round==3){late=last;lateData=original;}else xrtNetUdpPacketDestroy(last);
    }
    for(unsigned batch=0;batch<2;++batch){
        xfuture* receive=batch?xrtNetUdpReceiveBatchAsync(receiver,4):xrtNetUdpReceiveAsync(receiver);
        CHECK(receive&&xrtFutureCancel(receive));
        CHECK(xrtFutureWaitFor(receive,WAIT)==XWAIT_OK&&xrtFutureState(receive)==XFUTURE_CANCELLED);xrtFutureDestroy(receive);
    }
    CHECK(xrtNetUdpClose(sender)&&xrtNetUdpClose(receiver));
    open=xrtNetUdpWaitAsync(sender,XNET_UDP_WAIT_CLOSE);complete(open);xrtFutureDestroy(open);
    open=xrtNetUdpWaitAsync(receiver,XNET_UDP_WAIT_CLOSE);complete(open);xrtFutureDestroy(open);
    CHECK(xrtNetEngineDestroy(engine));xrtNetUdpDestroy(sender);xrtNetUdpDestroy(receiver);
    CHECK(late&&xrtNetUdpPacketData(late)==lateData&&!memcmp(lateData,"a\0b",3));
    leaf(xrtNetUdpPacketOwnership(late));xrtNetUdpPacketDestroy(late);balanced(&before);
}
int main(void)
{
    CHECK(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_ARGUMENT);xrtClearError();offline();
    run(XNET_PORT_SELECT);
    #ifdef _WIN32
    run(XNET_PORT_IOCP);
    #else
    run(XNET_PORT_EPOLL);
    #endif
    CHECK(attempts==64&&winners==8&&allocationFailures==3&&freezeChecks==8192&&xrtMemDebugReset());
    printf("UDP native value ownership: graphs=%u OOM=9 failures=%u concurrent-takes=%u winners=%u freeze-inspections=%u, allocation-free take, producer-independent payload, holes/Freeze/NUL/empty/batch-Future cancellation, balanced\n",
        graphChecks,allocationFailures,attempts,winners,freezeChecks);return 0;
}
