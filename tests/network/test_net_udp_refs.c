/* Typed UDP payloads: offline graphs/OOM and self-owned literal loopback only.
 * No DNS, external endpoint, error queue, malformed traffic or security tests. */
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
enum {NODES=64,ITEMS=5};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* prepare;bool ready;} Node;
typedef struct Case {xnetengine* engine;xnetudp* sender; xnetudp* receiver;xnetaddr address;
    xnetbytes *a,*b;xnetudppacket* packet;xnetbytesref parts[4];xnetdgramcontrol control;
    xnetudpownedmessage items[ITEMS];Node nodes[NODES];size_t count;bool busy,pinned;
    xatomic32 staged,blocked,go;} Case;
static unsigned graphCases,wireCases,partialCases,budgets,failures,refused,closeRaces;
static void freeze(xrtownershipscope* scope)
{double until=__xrtWaitAfter(5000);while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}}
static void state(xnetudp* udp,xnetudpstate wanted)
{double until=__xrtWaitAfter(5000);while(xrtNetUdpState(udp)!=wanted){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}}
static void signalWait(xatomic32* flag)
{double until=__xrtWaitAfter(5000);while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static const xrtownershipadapterv1* resolve(Case* c,xrtownershipref ref,const xrtownershippreparationv1** prep)
{
    const xrtownershipquiescencev1* quiet[]={xrtNetEngineOwnershipQuiescenceV1Get(),xrtNetUdpOwnershipQuiescenceV1Get(),
        xrtNetBufPoolOwnershipQuiescenceV1Get()};
    for(size_t i=0;i<3;++i)if(ref.Ops==quiet[i]->Ops&&quiet[i]->Busy(ref.Data)){c->busy=true;return NULL;}
    const xnetruntimeownershipv1* runtime[]={xrtNetUdpRuntimeOwnershipV1Get()};
    const xrtownershipadapterv1* a=xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtime,1,prep);
    if(!a)a=xrtNetUdpOwnershipAdapterV1(ref,prep);
    if(!a)a=xrtNetBufPoolOwnershipAdapterV1(ref,prep);
    if(!a)a=xrtNetBytesOwnershipAdapterV1(ref);
    if(!a)a=xrtNetUdpPacketOwnershipAdapterV1(ref);
    if(!a)a=xrtErrorOwnershipAdapterV1(ref);
    return a;
}
static bool admit(xrtownershipref ref,ptr raw)
{const xrtownershippreparationv1* prep=NULL;return resolve(raw,ref,&prep)!=NULL;}
static xrtownershipsnapshot* snapshot(Case* c,bool external,xrtownershipscope* scope)
{
    double until=__xrtWaitAfter(5000);
    for(;;){freeze(scope);c->busy=false;xrtownershipref roots[NODES+6],slots[NODES+6];size_t n=0,s=0;
        roots[n++]=xrtNetEngineOwnership(c->engine);roots[n++]=xrtNetUdpOwnership(c->sender);roots[n++]=xrtNetUdpOwnership(c->receiver);
        roots[n++]=xrtNetBytesOwnership(c->a);roots[n++]=xrtNetBytesOwnership(c->b);roots[n++]=xrtNetUdpPacketOwnership(c->packet);
        for(size_t i=0;i<n;++i)if(!external||i!=3)slots[s++]=roots[i];
        if(c->pinned)for(size_t i=0;i<c->count;++i)roots[n++]=slots[s++]=c->nodes[i].ref;
        xrtownershipsnapshot* proof=NULL;if(xrtOwnershipSnapshotCreate(roots,n,slots,s,admit,c,&proof))return proof;
        CHECK(c->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(scope));CHECK(!__xrtWaitExpired(until));xrtThreadYield();
    }
}
static void create(Case* c,xnetportbackend backend,unsigned concurrency,bool connected,size_t limit)
{
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=backend;config.Workers=1;config.NodeCacheBytes=0;
    c->engine=xrtNetEngineCreate(&config);CHECK(c->engine&&xrtNetEngineStart(c->engine));
    xnetudpconfig udp;xrtNetUdpConfigInit(&udp);udp.SendConcurrency=concurrency;udp.SendPacketLimit=limit;
    CHECK(xrtNetAddrLoopback(&c->address,XNET_FAMILY_IPV4,0));c->receiver=xrtNetUdpBind(c->engine,&c->address,0,&udp,NULL,NULL);
    CHECK(c->receiver);state(c->receiver,XNET_UDP_OPEN);CHECK(xrtNetUdpLocal(c->receiver,&c->address));
    if(connected)c->sender=xrtNetUdpConnect(c->engine,&c->address,0,&udp,NULL,NULL);
    else {xnetaddr local;CHECK(xrtNetAddrLoopback(&local,XNET_FAMILY_IPV4,0));c->sender=xrtNetUdpBind(c->engine,&local,0,&udp,NULL,NULL);}
    CHECK(c->sender);state(c->sender,XNET_UDP_OPEN);
    c->a=xrtNetBytesCopy("_a\0b_",5);c->b=xrtNetBytesCopy("XY",2);
    c->packet=__xrtNetUdpPacketCreate(&c->address,NULL,"_p\0q_",5,0);CHECK(c->a&&c->b&&c->packet);
    c->parts[0]=(xnetbytesref){c->a,1,3};c->parts[1]=(xnetbytesref){c->b,0,2};
    c->parts[2]=(xnetbytesref){c->a,0,1};c->parts[3]=(xnetbytesref){c->b,2,0};
    for(size_t i=0;i<ITEMS;++i)c->items[i].Remote=&c->address;
    c->items[0].Kind=XNET_UDP_OWNED_BYTES;c->items[0].Payload.Bytes=(xnetbytesref){c->a,1,3};
    c->items[1].Kind=XNET_UDP_OWNED_PACKET;c->items[1].Payload.Packet=(xnetudppacketref){c->packet,1,3};
    c->items[2].Kind=XNET_UDP_OWNED_VECTOR;c->items[2].Payload.Vector.Items=c->parts;c->items[2].Payload.Vector.Count=4;
    c->items[3].Kind=XNET_UDP_OWNED_BYTES;c->items[3].Payload.Bytes=(xnetbytesref){c->a,5,0};
    c->items[4].Kind=XNET_UDP_OWNED_VECTOR;c->items[4].Payload.Vector.Items=NULL;c->items[4].Payload.Vector.Count=0;
    xrtAtomic32Init(&c->staged,0);xrtAtomic32Init(&c->blocked,0);xrtAtomic32Init(&c->go,0);
}
static void dropInputs(Case* c)
{xrtNetBytesDestroy(c->a);xrtNetBytesDestroy(c->b);xrtNetUdpPacketDestroy(c->packet);c->a=c->b=NULL;c->packet=NULL;}
static void useControl(Case* c)
{
    uint32 available=xrtNetUdpSendControlAvailable(c->sender);
    if(available&XNET_DGRAM_CONTROL_HOP_LIMIT){c->control.Flags=XNET_DGRAM_CONTROL_HOP_LIMIT;c->control.HopLimit=64;}
    else {CHECK(available&XNET_DGRAM_CONTROL_SOURCE);c->control.Flags=XNET_DGRAM_CONTROL_SOURCE;
        CHECK(xrtNetAddrLoopback(&c->control.Source,XNET_FAMILY_IPV4,0));}
    c->items[2].Control=&c->control;
}
static void cleanup(Case* c)
{
    CHECK(xrtNetUdpAbort(c->sender)&&xrtNetUdpAbort(c->receiver));xrtNetUdpDestroy(c->sender);xrtNetUdpDestroy(c->receiver);
    double until=__xrtWaitAfter(5000);for(;;){xnetretireresult r=xrtNetEngineTryDestroy(c->engine);if(r==XNET_RETIRE_READY)break;
        CHECK(r==XNET_RETIRE_BUSY&&!__xrtWaitExpired(until));xrtThreadYield();}
}
static void blockWorker(xnetworker* worker,ptr raw)
{(void)worker;Case* c=raw;xrtAtomic32Store(&c->blocked,1,XMEMORY_RELEASE);signalWait(&c->go);}
static void stageWorker(xnetworker* worker,ptr raw)
{
    Case* c=raw;CHECK(worker==c->sender->Worker&&__xrtNetUdpBeginSend(c->sender));
    /* Stop at the genuine factory/list boundary, before the native driver.
     * No fake RC, mirrored allocation, callbacks, test hook or claimed busy
     * Worker is used to manufacture a collectable graph. */
    for(size_t i=0;i<ITEMS;++i){xnetresult result;__xrt_net_udp_send* send=__xrtNetUdpCreateOwnedSend(c->sender,&c->items[i],&result);
        if(!send)fprintf(stderr,"staging index=%zu kind=%d result=%d error-kind=%d\n",i,(int)c->items[i].Kind,(int)result,(int)xrtErrorKind(xrtGetError()));
        CHECK(send&&result==XNET_RESULT_OK&&__xrtNetUdpAttachSend(send));}
    __xrtNetUdpEndSend(c->sender);xrtAtomic32Store(&c->staged,1,XMEMORY_RELEASE);
}
static void pin(Case* c,xrtownershipsnapshot* proof)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);
        size_t j=0;while(j<c->count&&(c->nodes[j].ref.Data!=item.Reference.Data||c->nodes[j].ref.Ops!=item.Reference.Ops))++j;
        if(j<c->count)continue;
        CHECK(c->count<NODES);Node* n=&c->nodes[c->count++];n->ref=item.Reference;
        n->adapter=resolve(c,n->ref,&n->prepare);CHECK(n->adapter&&n->adapter->Hold(n->ref.Data)&&n->adapter->Claim(n->ref.Data,c));}
    c->pinned=true;
}
static void graph(xnetportbackend backend,unsigned order,bool controlled)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Case c={0};create(&c,backend,order&1?4:1,order&2,16);
    if(controlled)useControl(&c);
    CHECK(xrtNetEnginePost(c.engine,0,stageWorker,&c));signalWait(&c.staged);
    xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&c,true,&scope);bool external=false;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode n;CHECK(xrtOwnershipSnapshotNode(proof,i,&n));
        CHECK(n.Reachable==(n.Reference.Data==c.a));external=external||n.Reachable;}
    CHECK(external);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
    proof=snapshot(&c,false,&scope);bool a=false,b=false,packet=false;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode n;CHECK(xrtOwnershipSnapshotNode(proof,i,&n)&&!n.Reachable);
        if(n.Reference.Data==c.a){CHECK(n.StrongCount==4&&n.InternalCount==4);a=true;}
        if(n.Reference.Data==c.b){CHECK(n.StrongCount==2&&n.InternalCount==2);b=true;}
        if(n.Reference.Data==c.packet){CHECK(n.StrongCount==2&&n.InternalCount==2);packet=true;}}
    CHECK(a&&b&&packet);__xrt_net_udp_send* first=c.sender->SendHead;CHECK(first&&first->Data==xrtNetBytesView(c.a).Data+1);
    __xrt_net_udp_send* vec=first->Next->Next;CHECK(vec->PayloadOwnership==XRT_NET_UDP_SEND_VECTOR);
    __xrt_net_udp_vector* storage=__xrtNetUdpSendVector(vec);CHECK(storage->Count==3&&storage->Capacity==3);
    CHECK((uintptr_t)storage%_Alignof(__xrt_net_udp_vector)==0&&vec->Spans[0].Data==xrtNetBytesView(c.a).Data+1&&
        vec->Spans[1].Data==xrtNetBytesView(c.b).Data&&vec->Spans[2].Data==xrtNetBytesView(c.a).Data);
    uint32 tag=vec->PayloadOwnership;vec->PayloadOwnership=XRT_NET_UDP_SEND_OPAQUE;
    CHECK(!__xrtNetUdpSendsOwnership(c.sender,NULL,NULL));vec->PayloadOwnership=tag;++refused;
    size_t saved=storage->Capacity;storage->Capacity=XNET_UDP_VECTOR_MAX+1;
    CHECK(!__xrtNetUdpSendsOwnership(c.sender,NULL,NULL));storage->Capacity=saved;++refused;
    xnetspan span=storage->Spans[0];storage->Spans[0].Size=SIZE_MAX;
    CHECK(!__xrtNetUdpSendsOwnership(c.sender,NULL,NULL));storage->Spans[0]=span;++refused;
    pin(&c,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));double until=__xrtWaitAfter(5000);
    for(;;){bool ready=true;
        for(size_t step=0;step<c.count;++step){size_t i=order&1?c.count-1-step:step;Node* n=&c.nodes[i];if(!n->prepare||n->ready)continue;
            proof=snapshot(&c,false,&scope);pin(&c,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
            xrtownershipprepareresult r=n->prepare->Prepare(n->ref.Data,&c);CHECK(r!=XRT_OWNERSHIP_PREPARE_FAILED);
            n->ready=r==XRT_OWNERSHIP_PREPARE_READY;ready=ready&&n->ready;}
        if(ready)break;
        CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
    proof=snapshot(&c,false,&scope);pin(&c,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<c.count;++i){Node* n=&c.nodes[i];CHECK(!n->prepare||n->prepare->Ready(n->ref.Data));if(n->adapter->Clear)n->adapter->Clear(n->ref.Data,&c);}
    CHECK(xrtOwnershipScopeEnd(&scope));for(size_t i=0;i<c.count;++i){Node* n=&c.nodes[i];CHECK(!n->adapter->Finish||n->adapter->Finish(n->ref.Data,&c));}
    CHECK(xrtNetEngineTryDestroy(c.engine)==XNET_RETIRE_READY);xrtNetUdpDestroy(c.sender);xrtNetUdpDestroy(c.receiver);dropInputs(&c);
    for(size_t step=0;step<c.count;++step){size_t i=order&2?c.count-1-step:step;Node* n=&c.nodes[i];n->adapter->Drop(n->ref.Data);}
    balanced(&before);++graphCases;
}
static void receive(Case* c,size_t count)
{
    static const char* text[ITEMS]={"a\0b","p\0q","a\0bXY_","",""};static const size_t lengths[ITEMS]={3,3,6,0,0};
    bool seen[ITEMS]={false};
    double until=__xrtWaitAfter(5000);for(size_t i=0;i<count;++i){xnetudppacket* packet=NULL;
        while(!(packet=xrtNetUdpReceive(c->receiver))){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
        size_t match=0;while(match<count&&(seen[match]||xrtNetUdpPacketSize(packet)!=lengths[match]||
            (lengths[match]&&memcmp(xrtNetUdpPacketData(packet),text[match],lengths[match]))))++match;
        CHECK(match<count);seen[match]=true;
        xrtNetUdpPacketDestroy(packet);}
}
static void wire(xnetportbackend backend,unsigned concurrency,bool connected,bool controlled)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Case c={0};create(&c,backend,concurrency,connected,16);
    if(controlled)useControl(&c);
    CHECK(xrtNetEnginePost(c.engine,0,blockWorker,&c));signalWait(&c.blocked);size_t accepted=0;
    CHECK(xrtNetUdpSendOwnedBatch(c.sender,c.items,ITEMS,&accepted)==XNET_RESULT_OK&&accepted==ITEMS);
    /* Inputs really disappear before the queued native command runs. */
    memset(c.items,0,sizeof(c.items));memset(c.parts,0,sizeof(c.parts));memset(&c.control,0,sizeof(c.control));dropInputs(&c);
    xrtAtomic32Store(&c.go,1,XMEMORY_RELEASE);receive(&c,ITEMS);cleanup(&c);balanced(&before);++wireCases;
}
static void prefixAndOom(xnetportbackend backend)
{
    for(unsigned test=0;test<17;++test){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Case c={0};
        create(&c,backend,test==0?1:4,true,test==0?2:16);CHECK(xrtNetEnginePost(c.engine,0,blockWorker,&c));signalWait(&c.blocked);
        if(test==1)c.items[1].Payload.Packet.Offset=SIZE_MAX;
        if(test>=2){CHECK(xrtMemDebugFailAfter(test-2));++budgets;}
        size_t accepted=SIZE_MAX;xnetresult result=xrtNetUdpSendOwnedBatch(c.sender,c.items,ITEMS,&accepted);
        bool failed=test>=2&&xrtMemDebugFailTriggered();xrtMemDebugFailClear();CHECK(accepted<=ITEMS);
        if(test==0)CHECK(result==XNET_RESULT_AGAIN&&accepted==2);
        if(test==1)CHECK(result==XNET_RESULT_ERROR&&accepted==1);
        if(test>=2){if(failed){CHECK(result==XNET_RESULT_ERROR);++failures;}else CHECK(result==XNET_RESULT_OK&&accepted==ITEMS);}
        xrtownershipref a=xrtNetBytesOwnership(c.a),b=xrtNetBytesOwnership(c.b),p=xrtNetUdpPacketOwnership(c.packet);
        size_t refs=0;CHECK(a.Ops->Count(a.Data,&refs)&&refs==1u+(accepted>0)+(accepted>2?2u:0u));
        CHECK(b.Ops->Count(b.Data,&refs)&&refs==1+(accepted>2));CHECK(p.Ops->Count(p.Data,&refs)&&refs==1+(accepted>1));
        xrtClearError();dropInputs(&c);xrtAtomic32Store(&c.go,1,XMEMORY_RELEASE);receive(&c,accepted);cleanup(&c);balanced(&before);++partialCases;
    }
}
static void publicSingles(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Case c={0};create(&c,backend,4,true,16);
    /* Forward a genuinely received packet, not only a private result value. */
    CHECK(xrtNetUdpSend(c.sender,"_p\0q_",5)==XNET_RESULT_OK);xrtNetUdpPacketDestroy(c.packet);c.packet=NULL;
    double until=__xrtWaitAfter(5000);while(!(c.packet=xrtNetUdpReceive(c.receiver))){CHECK(!__xrtWaitExpired(until));xrtThreadYield();}
    CHECK(xrtNetUdpSendMsgBytes(c.sender,NULL,NULL,c.a,1,3)==XNET_RESULT_OK);
    CHECK(xrtNetUdpSendMsgPacket(c.sender,NULL,NULL,c.packet,1,3)==XNET_RESULT_OK);
    CHECK(xrtNetUdpSendMsgBytesVec(c.sender,NULL,NULL,c.parts,4)==XNET_RESULT_OK);
    CHECK(xrtNetUdpSendMsgBytes(c.sender,NULL,NULL,c.a,5,0)==XNET_RESULT_OK);
    CHECK(xrtNetUdpSendMsgBytesVec(c.sender,NULL,NULL,NULL,0)==XNET_RESULT_OK);
    CHECK(xrtNetUdpSendMsgBytes(c.sender,NULL,NULL,c.a,SIZE_MAX,1)==XNET_RESULT_ERROR);
    CHECK(xrtNetUdpSendMsgPacket(c.sender,NULL,NULL,c.packet,1,SIZE_MAX)==XNET_RESULT_ERROR);
    CHECK(xrtNetUdpSendMsgBytesVec(c.sender,NULL,NULL,c.parts,XNET_UDP_VECTOR_MAX+1)==XNET_RESULT_ERROR);xrtClearError();
    dropInputs(&c);receive(&c,ITEMS);cleanup(&c);balanced(&before);++wireCases;
}
typedef struct CloseRace {Case* c;xatomic32 go;} CloseRace;
static int32 sendRace(ptr raw)
{
    CloseRace* r=raw;signalWait(&r->go);
    for(unsigned i=0;i<16;++i){xnetresult result=xrtNetUdpSendMsgBytesVec(r->c->sender,NULL,NULL,r->c->parts,4);
        CHECK(result==XNET_RESULT_OK||result==XNET_RESULT_CLOSED);}
    return 0;
}
static void concurrentClose(xnetportbackend backend)
{
    for(unsigned i=0;i<24;++i){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Case c={0};create(&c,backend,4,true,128);
        CloseRace r={0};r.c=&c;xrtAtomic32Init(&r.go,0);xthread* thread=xrtThreadCreate(sendRace,&r,0);CHECK(thread);
        xrtAtomic32Store(&r.go,1,XMEMORY_RELEASE);CHECK(xrtNetUdpClose(c.sender));
        CHECK(xrtThreadWaitFor(thread,5000)==XWAIT_OK&&xrtThreadExitCode(thread)==0);xrtThreadDestroy(thread);
        dropInputs(&c);cleanup(&c);balanced(&before);++closeRaces;}
}
int main(int argc,char** argv)
{
    xnetportbackend backend=XNET_PORT_SELECT;if(argc>1&&!strcmp(argv[1],"native")){
        #if defined(_WIN32) || defined(_WIN64)
        backend=XNET_PORT_IOCP;
        #elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
        #endif
    }
    CHECK(xrtMemDebugEnable(true));for(unsigned order=0;order<4;++order)for(unsigned control=0;control<2;++control)graph(backend,order,control);
    for(unsigned concurrency=1;concurrency<=4;concurrency+=3)for(unsigned connected=0;connected<2;++connected)
        for(unsigned control=0;control<2;++control)wire(backend,concurrency,connected,control);
    prefixAndOom(backend);publicSingles(backend);concurrentClose(backend);CHECK(failures>0&&failures<budgets);
    printf("UDP typed refs: graphs=%u wire=%u prefixes=%u OOM=%u failed=%u guards=%u close-races=%u; no payload copies, balanced\n",
        graphCases,wireCases,partialCases,budgets,failures,refused,closeRaces);return 0;
}
