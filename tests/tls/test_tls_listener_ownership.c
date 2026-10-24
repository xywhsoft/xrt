/* Ordinary fixed-key, self-created loopback, lifecycle and allocation tests.
 * No DNS, external targets, malformed records or security-style testing. */
#ifdef TLS_LISTENER_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../fixtures/tls_server.h"
#include <xrt/detail/wait.h>
#ifndef TLS_LISTENER_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_tls_stream.h"
#endif
#define CHECK(c) do { if(!(c)){fprintf(stderr,"line %u: ",(unsigned)__LINE__);testRequire(false,#c);} } while(0)
typedef struct Scene Scene;
typedef struct Context {
    volatile int32 refs;Scene* scene;xtlslistener* listener;xtlslistener* retired;
    const void* claim;bool client,cleared,finished;
} Context;
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prepare;} Node;
struct Scene {
    xnetengine* engine;xtlslistener* listener;xtlsstream* clients[2];size_t clientCount;
    xnetstream* plain;xtlsstream* accepted;xfuture* futures[2];size_t futureCount;
    xatomic32 clientOpen,serverOpen,clientClose,serverClose,listenerClose,drops,frees,read;
    Context* context;Node nodes[128];size_t count;bool pinned,failure;
};
static unsigned cases,proofs,refusals,pendingWaits,records,cleanupFailures,acceptBudgets,acceptFailures,startBudgets,startFailures;
static bool contextCount(const void* data,size_t* count)
{const Context* c=data;if(!c||!count||c->cleared)return false;int32 refs=__xrtAtomicRefLoad(&c->refs);if(refs<=0)return false;*count=(size_t)refs;return true;}
static bool contextTrace(const void* data,xrtownershipvisitor visit,ptr arg)
{const Context* c=data;size_t n;return visit&&contextCount(c,&n)&&(!c->listener||visit(xrtTlsListenerOwnership(c->listener),arg));}
static const xrtownershipops contextOps={contextCount,contextTrace};
static bool contextHold(const void* data)
{
    Context* c=(Context*)data;xrtownershipscope m={0};CHECK(xrtOwnershipMutationBegin(&m));
    bool held=!c->cleared&&xrtRefRetain(&c->refs)>=0;CHECK(xrtOwnershipScopeEnd(&m));return held;
}
static void contextDrop(const void* data)
{Context* c=(Context*)data;int32 left=xrtOwnershipRefRelease(&c->refs);CHECK(left>=0);if(!left){CHECK(!c->listener&&!c->retired);xrtAtomic32FetchAdd(&c->scene->frees,1,XMEMORY_ACQ_REL);xrtFree(c);}}
static bool contextClaim(const void* data,const void* token)
{Context* c=(Context*)data;size_t n;if(!token||(c->claim&&c->claim!=token)||!contextCount(c,&n))return false;c->claim=token;return true;}
static void contextRestore(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&!c->cleared);c->claim=NULL;}
static void contextClear(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&!c->cleared);c->retired=c->listener;c->listener=NULL;c->cleared=true;}
static bool contextFinish(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&c->cleared);if(!c->finished){xtlslistener* l=c->retired;c->retired=NULL;c->finished=true;xrtTlsListenerDestroy(l);}return true;}
static const xrtownershipadapterv1 contextAdapter={sizeof(contextAdapter),contextHold,contextDrop,contextClaim,contextRestore,NULL,contextClear,contextFinish};
static Context* context(Scene* s,bool client)
{Context* c=xrtCalloc(1,sizeof(*c));CHECK(c);c->refs=1;c->scene=s;c->client=client;return c;}
static void nativeDrop(const void* data)
{
    Context* c=(Context*)data;Scene* s=c->scene;bool fail=!c->client&&s->failure;
    xrtAtomic32FetchAdd(&s->drops,1,XMEMORY_ACQ_REL);contextDrop(c);if(fail)xrtSetErrorKind(XERR_STATE);
}
static void clientOpen(xtlsstream* stream,ptr data)
{(void)stream;Context* c=data;CHECK(c->client&&!c->cleared);xrtAtomic32FetchAdd(&c->scene->clientOpen,1,XMEMORY_ACQ_REL);}
static void clientClose(xtlsstream* stream,xnetresult result,const xerror* error,ptr data)
{(void)stream;(void)result;(void)error;Context* c=data;CHECK(c->client&&!c->cleared);xrtAtomic32FetchAdd(&c->scene->clientClose,1,XMEMORY_ACQ_REL);}
static void serverOpen(xtlsstream* stream,ptr data)
{(void)stream;Context* c=data;CHECK(!c->client&&!c->cleared);xrtAtomic32FetchAdd(&c->scene->serverOpen,1,XMEMORY_ACQ_REL);}
static void serverClose(xtlsstream* stream,xnetresult result,const xerror* error,ptr data)
{(void)stream;(void)result;(void)error;Context* c=data;CHECK(!c->client&&!c->cleared);xrtAtomic32FetchAdd(&c->scene->serverClose,1,XMEMORY_ACQ_REL);}
static void listenerClose(xtlslistener* listener,ptr data)
{Context* c=data;CHECK(!c->client&&!c->cleared&&xrtTlsListenerData(listener)==c);xrtAtomic32FetchAdd(&c->scene->listenerClose,1,XMEMORY_ACQ_REL);}
static bool pushAccept(xtlslistener* listener,xtlsstream* stream,ptr data)
{(void)listener;Context* c=data;CHECK(!c->scene->accepted);c->scene->accepted=stream;return true;}
static void listenerError(xtlslistener* listener,const xerror* error,ptr data)
{(void)listener;(void)data;fprintf(stderr,"listener transport error: %s op=%s kind=%d\n",xrtErrorMessage(error),xrtErrorOperation(error),(int)xrtErrorKind(error));}
static void serverRead(xtlsstream* stream,const xnetbuf* buffer,ptr data)
{
    Context* c=data;uint8 text[8];size_t read=0;(void)buffer;
    CHECK(xrtTlsStreamRead(stream,text,sizeof(text),&read)==XTLS_OK&&read==3&&!memcmp(text,"t\0x",3));
    xrtAtomic32FetchAdd(&c->scene->read,(uint32)read,XMEMORY_ACQ_REL);
}
static const xtlsstreamownershipv1 clientPolicy={sizeof(clientPolicy),{clientOpen,NULL,NULL,NULL,NULL,clientClose,NULL},nativeDrop,&contextOps};
static const xtlslistenerownershipv1 pullPolicy={sizeof(pullPolicy),{NULL,NULL,listenerError,listenerClose},
    {serverOpen,NULL,NULL,NULL,NULL,serverClose,NULL},nativeDrop,&contextOps};
static const xtlslistenerownershipv1 pushPolicy={sizeof(pushPolicy),{pushAccept,NULL,listenerError,listenerClose},
    {serverOpen,serverRead,NULL,NULL,NULL,serverClose,NULL},nativeDrop,&contextOps};
static const xrtownershipadapterv1* resolve(xrtownershipref ref,const xrtownershippreparationv1** prepare)
{
    *prepare=NULL;
    const xnetruntimeownershipv1* runtimes[]={xrtNetStreamRuntimeOwnershipV1Get(),xrtNetListenerRuntimeOwnershipV1Get(),xrtTlsStreamRuntimeOwnershipV1Get(),xrtTlsListenerRuntimeOwnershipV1Get()};
    const xnetstreameventsownershipv1* streamEvents[]={xrtTlsStreamTransportOwnershipV1Get()};
    const xnetlistenereventsownershipv1* listenerEvents[]={xrtTlsListenerTransportOwnershipV1Get()};
    const xtlsstreamownershipv1* streams[]={&clientPolicy,xrtTlsListenerStreamOwnershipV1Get(),xrtTlsListenerStreamPushOwnershipV1Get()};
    const xtlslistenerownershipv1* listeners[]={&pullPolicy,&pushPolicy};
    if(ref.Ops==xrtNetEngineOwnershipQuiescenceV1Get()->Ops)return xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtimes,4,prepare);
    if(ref.Ops==xrtNetStreamRuntimeOwnershipV1Get()->Ops)return xrtNetStreamOwnershipAdapterV2(ref,streamEvents,1,prepare);
    if(ref.Ops==xrtNetListenerRuntimeOwnershipV1Get()->Ops)return xrtNetListenerOwnershipAdapterV2(ref,listenerEvents,1,prepare);
    if(ref.Ops==xrtTlsStreamOwnershipQuiescenceV1Get()->Ops)return xrtTlsStreamOwnershipAdapterV1(ref,streams,3,prepare);
    if(ref.Ops==xrtTlsListenerOwnershipQuiescenceV1Get()->Ops)return xrtTlsListenerOwnershipAdapterV1(ref,listeners,2,prepare);
    if(ref.Ops==xrtTlsListenerStreamRecordOwnershipQuiescenceV1Get()->Ops)return xrtTlsListenerStreamRecordOwnershipAdapterV1(ref,prepare);
    if(ref.Ops==xrtNetBufPoolOwnershipQuiescenceV1Get()->Ops)return xrtNetBufPoolOwnershipAdapterV1(ref,prepare);
    if(ref.Ops==&contextOps)return &contextAdapter;
    const xfutureproducerownershipv1* producers[]={xrtTlsListenerFutureProducerPolicyV1Get(),xrtTlsStreamFutureProducerPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[]={xrtTlsListenerFutureCancelPolicyV1Get(),xrtTlsStreamFutureCancelPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[]={xrtTlsListenerFutureStreamPayloadPolicyV1Get(),xrtTlsStreamFutureBytesPayloadPolicyV1Get()};
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=producers;admission.ProducerPolicyCount=2;
    admission.CancelWatchPolicies=cancels;admission.CancelWatchPolicyCount=2;
    admission.PayloadPolicies=payloads;admission.PayloadPolicyCount=2;
    const xrtownershipadapterv1* a=xrtTlsListenerFutureOwnershipAdapterV1(ref,prepare);
    if(!a)a=xrtTlsStreamFutureOwnershipAdapterV1(ref,prepare);
    if(!a)a=xrtFutureOwnershipAdapterV4(ref,&admission,prepare);
    if(!a)a=xrtCancelOwnershipAdapterV2(ref,cancels,2,prepare);
    if(!a)a=xrtCancelWatchOwnershipAdapterV1(ref,cancels,2,prepare);
    if(!a)a=xrtNetBytesOwnershipAdapterV1(ref);
    if(!a)a=xrtErrorOwnershipAdapterV1(ref);
    return a;
}
static bool admit(xrtownershipref ref,ptr data)
{
    (void)data;const xrtownershippreparationv1* p=NULL;
    const xrtownershipadapterv1* a=resolve(ref,&p);
    static const void* last;
    if(!a&&last!=ref.Data){last=ref.Data;fprintf(stderr,"unadmitted data=%p engine=%d tcp=%d nativeListener=%d tls=%d listener=%d record=%d future=%d wait=%d context=%d\n",ref.Data,
        ref.Ops==xrtNetEngineOwnershipQuiescenceV1Get()->Ops,ref.Ops==xrtNetStreamRuntimeOwnershipV1Get()->Ops,
        ref.Ops==xrtNetListenerRuntimeOwnershipV1Get()->Ops,ref.Ops==xrtTlsStreamOwnershipQuiescenceV1Get()->Ops,
        ref.Ops==xrtTlsListenerOwnershipQuiescenceV1Get()->Ops,ref.Ops==xrtTlsListenerStreamRecordOwnershipQuiescenceV1Get()->Ops,
        ref.Ops==xrtFutureOwnershipQuiescenceV1Get()->Ops,ref.Ops==xrtTlsListenerFutureOwnershipQuiescenceV1Get()->Ops,ref.Ops==&contextOps);}
    return a!=NULL;
}
static xrtownershipsnapshot* snapshot(Scene* s,xrtownershipscope* freeze)
{
    double due=__xrtWaitAfter(10000);
    size_t acquired=0,busy=0;
    for(;;){if(xrtOwnershipFreezeTryBegin(freeze)){
        acquired++;
        if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->engine)){
            xrtownershipref anchors[136],slots[136];size_t count=0;
            anchors[count++]=xrtNetEngineOwnership(s->engine);anchors[count++]=xrtTlsListenerOwnership(s->listener);
            for(size_t i=0;i<s->clientCount;i++)anchors[count++]=xrtTlsStreamOwnership(s->clients[i]);
            if(s->plain)anchors[count++]=xrtNetStreamOwnership(s->plain);
            if(s->accepted)anchors[count++]=xrtTlsStreamOwnership(s->accepted);
            for(size_t i=0;i<s->futureCount;i++)anchors[count++]=xrtFutureOwnership(s->futures[i]);
            if(s->pinned)for(size_t i=0;i<s->count;i++)anchors[count++]=s->nodes[i].ref;
            CHECK(count<=136);for(size_t i=0;i<count;i++)slots[i]=anchors[i];
            xrtownershipsnapshot* proof=NULL;
            if(xrtOwnershipSnapshotCreate(anchors,count,slots,count,admit,s,&proof))return proof;
            static bool reported;
            if(!reported){reported=true;size_t e=0,l=0;fprintf(stderr,"snapshot miss engineCount=%d listenerCount=%d active=%u closed=%d runtime=%d refs=%d hs=%u queued=%u error=%d\n",
                xrtNetEngineOwnership(s->engine).Ops->Count(s->engine,&e),xrtTlsListenerOwnership(s->listener).Ops->Count(s->listener,&l),
                xrtAtomic32Load(&s->listener->OwnershipActive,XMEMORY_ACQUIRE),s->listener->Closed,s->listener->RuntimeHeld,__xrtAtomicRefLoad(&s->listener->References),
                xrtAtomic32Load(&s->listener->ActiveHandshakes,XMEMORY_ACQUIRE),xrtAtomic32Load(&s->listener->QueuedAccepts,XMEMORY_ACQUIRE),xrtGetError()?(int)xrtErrorKind(xrtGetError()):0);}
            xrtClearError();
        }else busy++;CHECK(xrtOwnershipScopeEnd(freeze));}
        if(__xrtWaitExpired(due)){fprintf(stderr,"snapshot timeout freezes=%zu engineBusy=%zu ownActive=%u engineState=%d\n",acquired,busy,
            xrtAtomic32Load(&s->listener->OwnershipActive,XMEMORY_ACQUIRE),(int)xrtNetEngineState(s->engine));CHECK(false);}
        xrtThreadYield();}
}
static void discover(Scene* s,xrtownershipsnapshot* proof)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);i++){
        xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);
        size_t at=0;while(at<s->count&&(s->nodes[at].ref.Data!=item.Reference.Data||s->nodes[at].ref.Ops!=item.Reference.Ops))at++;
        if(at<s->count)continue;
        CHECK(s->count<128);Node* n=&s->nodes[s->count++];n->ref=item.Reference;n->adapter=resolve(n->ref,&n->prepare);CHECK(n->adapter);
        CHECK(n->adapter->Hold(n->ref.Data)&&n->adapter->Claim(n->ref.Data,s));
        if(n->prepare)CHECK(!n->adapter->Claim(n->ref.Data,&item));
    }s->pinned=true;
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);CHECK(!xrtGetError());
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void idle(Scene* s,xrtownershipscope* freeze)
{
    double due=__xrtWaitAfter(10000);
    for(;;){if(xrtOwnershipFreezeTryBegin(freeze)){
        if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->engine)&&!xrtAtomic32Load(&s->listener->AcceptWaiters,XMEMORY_ACQUIRE))return;
        CHECK(xrtOwnershipScopeEnd(freeze));}
        CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
}
static void acceptConstructionBudgets(Scene* s)
{
    xrtownershipscope freeze={0};idle(s,&freeze);size_t baseline=0;xrtownershipref ref=xrtTlsListenerOwnership(s->listener);
    CHECK(ref.Ops->Count(ref.Data,&baseline));CHECK(xrtOwnershipScopeEnd(&freeze));unsigned success=0,failed=0;
    for(size_t budget=0;budget<24&&success<2;budget++){
        CHECK(xrtMemDebugFailAfter(budget));xfuture* f=xrtTlsListenerAcceptAsync(s->listener);xrtMemDebugFailClear();acceptBudgets++;
        if(!f){CHECK(xrtGetError());xrtClearError();failed++;acceptFailures++;success=0;}
        else{CHECK(xrtFutureCancel(f));xrtFutureDestroy(f);success++;}
        idle(s,&freeze);size_t after=0;CHECK(ref.Ops->Count(ref.Data,&after)&&after==baseline);
        CHECK(__xrtAtomicRefLoad(&s->context->refs)==1&&!xrtAtomic32Load(&s->drops,XMEMORY_ACQUIRE));CHECK(xrtOwnershipScopeEnd(&freeze));
    }CHECK(failed&&success==2);
}
static void startConstructionBudgets(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};
    xrtAtomic32Init(&s.listenerClose,0);xrtAtomic32Init(&s.drops,0);xrtAtomic32Init(&s.frees,0);
    xtlscontext* tls=testTlsServerContext();xtlsidentity* identity=testTlsServerIdentity();CHECK(tls&&identity);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.NodeCacheBytes=0;
    s.engine=xrtNetEngineCreate(&config);CHECK(s.engine&&xrtNetEngineStart(s.engine));
    xtlslistenerconfig listen;xrtTlsListenerConfigInit(&listen);listen.Tls.Context=tls;listen.Tls.Identity=identity;
    CHECK(xrtNetAddrLoopback(&listen.Listen.Address,XNET_FAMILY_IPV4,0));unsigned success=0,failed=0;
    for(size_t budget=0;budget<32&&success<2;budget++){
        uint32 drops=xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE),frees=xrtAtomic32Load(&s.frees,XMEMORY_ACQUIRE);Context* c=context(&s,false);
        CHECK(xrtMemDebugFailAfter(budget));xtlslistener* l=xrtTlsListenerStartOwnedV1(s.engine,&listen,c,&pullPolicy);xrtMemDebugFailClear();startBudgets++;
        if(!l){
            CHECK(xrtGetError());xrtClearError();CHECK(xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE)==drops&&xrtAtomic32Load(&s.frees,XMEMORY_ACQUIRE)==frees);
            CHECK(__xrtAtomicRefLoad(&c->refs)==1);contextDrop(c);failed++;startFailures++;success=0;
        }else{
            CHECK(xrtTlsListenerClose(l));xrtTlsListenerDestroy(l);success++;
            double due=__xrtWaitAfter(10000);
            while(xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE)!=drops+1||xrtAtomic32Load(&s.frees,XMEMORY_ACQUIRE)!=frees+1){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
        }
        double due=__xrtWaitAfter(10000);
        for(;;){xrtownershipscope freeze={0};if(xrtOwnershipFreezeTryBegin(&freeze)){
            bool ready=!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s.engine)&&!s.engine->RuntimeOwnerCount;
            CHECK(xrtOwnershipScopeEnd(&freeze));if(ready)break;}
            CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    }
    CHECK(failed&&success==2);double due=__xrtWaitAfter(10000);
    for(;;){xnetretireresult state=xrtNetEngineTryDestroy(s.engine);if(state==XNET_RETIRE_READY)break;
        CHECK(state==XNET_RETIRE_BUSY&&!__xrtWaitExpired(due));xrtThreadYield();}
    xrtTlsIdentityRelease(identity);xrtTlsContextRelease(tls);xrtClearError();balanced(before);
}
static void runCase(xnetportbackend backend,unsigned mode,bool failure)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};s.failure=failure;
    xrtAtomic32Init(&s.clientOpen,0);xrtAtomic32Init(&s.serverOpen,0);xrtAtomic32Init(&s.clientClose,0);xrtAtomic32Init(&s.serverClose,0);
    xrtAtomic32Init(&s.listenerClose,0);xrtAtomic32Init(&s.drops,0);xrtAtomic32Init(&s.frees,0);xrtAtomic32Init(&s.read,0);
    xtlscontext* tls=testTlsServerContext();xtlsidentity* identity=testTlsServerIdentity();CHECK(tls&&identity);
    xtlsverifierconfig v;xrtTlsVerifierConfigInit(&v);v.Verify=testTlsServerAccept;xtlsverifier* verifier=xrtTlsVerifierCreate(&v);CHECK(verifier);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.NodeCacheBytes=4096;
    s.engine=xrtNetEngineCreate(&config);CHECK(s.engine&&xrtNetEngineStart(s.engine));
    xtlslistenerconfig listen;xrtTlsListenerConfigInit(&listen);listen.Tls.Context=tls;listen.Tls.Identity=identity;
    CHECK(xrtNetAddrLoopback(&listen.Listen.Address,XNET_FAMILY_IPV4,0));
    s.context=context(&s,false);s.listener=xrtTlsListenerStartOwnedV1(s.engine,&listen,s.context,mode==3?&pushPolicy:&pullPolicy);CHECK(s.listener);
    s.context->listener=xrtTlsListenerRef(s.listener);CHECK(s.context->listener);
    xnetaddr address;CHECK(xrtTlsListenerLocal(s.listener,&address));
    if(mode!=3)acceptConstructionBudgets(&s);
    double due=__xrtWaitAfter(10000);
    if(mode==2){
        s.plain=xrtNetStreamConnect(s.engine,&address,0,&listen.Listen.Stream,NULL,NULL);CHECK(s.plain);
        while(xrtNetStreamState(s.plain)!=XNET_STREAM_OPEN||xrtAtomic32Load(&s.listener->ActiveHandshakes,XMEMORY_ACQUIRE)!=1){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    }else{
        xtlsclientconfig client;xrtTlsClientConfigInit(&client);client.Context=tls;client.Verifier=verifier;client.ServerName=XRT_STR_LITERAL("example.com");
        s.clientCount=mode==0?2:1;
        for(size_t i=0;i<s.clientCount;i++){Context* c=context(&s,true);s.clients[i]=xrtTlsStreamConnectOwnedV1(s.engine,&address,0,&listen.Listen.Stream,&client,NULL,c,&clientPolicy);CHECK(s.clients[i]);}
        while(xrtAtomic32Load(&s.clientOpen,XMEMORY_ACQUIRE)!=s.clientCount||xrtAtomic32Load(&s.serverOpen,XMEMORY_ACQUIRE)!=s.clientCount){
            if(__xrtWaitExpired(due)){
                fprintf(stderr,"open timeout mode=%u clients=%u servers=%u hs=%u queued=%u rejected=%llu hsErrors=%llu rootActive=%u rootState=%d\n",mode,
                    xrtAtomic32Load(&s.clientOpen,XMEMORY_ACQUIRE),xrtAtomic32Load(&s.serverOpen,XMEMORY_ACQUIRE),
                    xrtAtomic32Load(&s.listener->ActiveHandshakes,XMEMORY_ACQUIRE),xrtAtomic32Load(&s.listener->QueuedAccepts,XMEMORY_ACQUIRE),
                    (unsigned long long)xrtAtomic64Load(&s.listener->Rejected,XMEMORY_ACQUIRE),(unsigned long long)xrtAtomic64Load(&s.listener->HandshakeErrors,XMEMORY_ACQUIRE),
                    xrtAtomic32Load(&s.listener->OwnershipActive,XMEMORY_ACQUIRE),(int)xrtTlsListenerState(s.listener));
                for(size_t i=0;i<s.clientCount;i++)fprintf(stderr,"client[%zu] state=%d err=%s\n",i,(int)xrtTlsStreamState(s.clients[i]),xrtTlsStreamError(s.clients[i])?xrtErrorMessage(xrtTlsStreamError(s.clients[i])):"none");
                fprintf(stderr,"native state=%d accepted=%llu rejected=%llu dataMatch=%d policyMatch=%d eventsMatch=%d published=%d start=%d refs=%d rootErr=%s\n",
                    (int)xrtNetListenerState(s.listener->Native),(unsigned long long)xrtAtomic64Load(&s.listener->Native->Accepted,XMEMORY_ACQUIRE),
                    (unsigned long long)xrtAtomic64Load(&s.listener->Native->Rejected,XMEMORY_ACQUIRE),xrtNetListenerData(s.listener->Native)==s.listener,
                    s.listener->Native->EventPolicy==xrtTlsListenerTransportOwnershipV1Get(),s.listener->Native->Events.Accept==xrtTlsListenerTransportOwnershipV1Get()->Events.Accept,
                    s.listener->Published,s.listener->StartDone,__xrtAtomicRefLoad(&s.listener->References),s.listener->StartupError?xrtErrorMessage(s.listener->StartupError):"none");
                CHECK(false);
            }xrtThreadYield();}
        if(mode==0){s.accepted=xrtTlsListenerAccept(s.listener);CHECK(s.accepted);}
        if(mode==1){
            s.futures[s.futureCount++]=xrtTlsListenerAcceptAsync(s.listener);CHECK(s.futures[0]);
            CHECK(xrtFutureWaitFor(s.futures[0],5000)==XWAIT_OK&&xrtFutureState(s.futures[0])==XFUTURE_RESOLVED&&xrtFutureValue(s.futures[0]));
            s.futures[s.futureCount++]=xrtTlsListenerAcceptAsync(s.listener);CHECK(s.futures[1]);
        }
        if(mode==3){
            CHECK(s.accepted);xfuture* sent=xrtTlsStreamSendAsync(s.clients[0],"t\0x",3);CHECK(sent);
            CHECK(xrtFutureWaitFor(sent,5000)==XWAIT_OK&&xrtFutureState(sent)==XFUTURE_RESOLVED);xrtFutureDestroy(sent);
            while(xrtAtomic32Load(&s.read,XMEMORY_ACQUIRE)!=3){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
        }
    }
    xrtownershipscope freeze={0};xrtownershipsnapshot* proof=snapshot(&s,&freeze);unsigned recordCount=0,waitCount=0;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);i++){
        xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);
        if(item.Reference.Ops==xrtTlsListenerStreamRecordOwnershipQuiescenceV1Get()->Ops){recordCount++;CHECK(item.StrongCount==item.InternalCount);}
        if(item.Reference.Ops==xrtTlsListenerFutureOwnershipQuiescenceV1Get()->Ops){waitCount++;CHECK(item.StrongCount==3&&item.InternalCount==3);}
    }
    CHECK(recordCount==(mode==0?2u:1u)&&waitCount==(mode==1?1u:0u));records+=recordCount;pendingWaits+=waitCount;proofs++;
    xtlslistenerownershipv1 copy=pullPolicy;const xtlslistenerownershipv1* unknown[]={&copy};
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&s;const xrtownershippreparationv1* out=sentinel;
    CHECK(!xrtTlsListenerOwnershipAdapterV1(xrtTlsListenerOwnership(s.listener),unknown,1,&out)&&out==sentinel);refusals++;
    xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&freeze));
    if(mode==0||mode==3){
        CHECK(xrtTlsListenerClose(s.listener));
        while(xrtTlsListenerState(s.listener)!=XTLS_LISTENER_CLOSED){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
        CHECK(xrtTlsStreamState(s.accepted)==XTLS_STREAM_OPEN&&!xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE));
    }
    proof=snapshot(&s,&freeze);discover(&s,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<s.count;i++)if(s.nodes[i].prepare){s.nodes[i].adapter->Restore(s.nodes[i].ref.Data,&s);CHECK(s.nodes[i].adapter->Claim(s.nodes[i].ref.Data,&s));}
    CHECK(xrtOwnershipScopeEnd(&freeze));
    CHECK(!xrtTlsListenerAccept(s.listener));xrtClearError();CHECK(!xrtTlsListenerAcceptAsync(s.listener));xrtClearError();CHECK(!xrtTlsListenerClose(s.listener));xrtClearError();refusals+=3;
    due=__xrtWaitAfter(10000);
    for(;;){bool all=true;
        for(size_t step=0;step<s.count;step++){
            size_t i=(step+1)%s.count;Node* n=&s.nodes[i];if(!n->prepare)continue;
            proof=snapshot(&s,&freeze);discover(&s,proof);xrtOwnershipSnapshotDestroy(proof);bool ready=n->prepare->Ready(n->ref.Data);CHECK(xrtOwnershipScopeEnd(&freeze));
            if(ready)continue;
            CHECK(n->prepare->Prepare(n->ref.Data,&s)!=XRT_OWNERSHIP_PREPARE_FAILED);all=false;
        }if(all)break;CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    proof=snapshot(&s,&freeze);discover(&s,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<s.count;i++){Node* n=&s.nodes[i];if(n->prepare)CHECK(n->prepare->Ready(n->ref.Data));n->adapter->Clear(n->ref.Data,&s);}
    CHECK(xrtOwnershipScopeEnd(&freeze));
    for(size_t i=0;i<s.count;i++){Node* n=&s.nodes[i];if(!n->adapter->Finish)continue;
        if(failure&&n->ref.Data==s.listener){
            xrtSetErrorKind(XERR_ARGUMENT);CHECK(!n->adapter->Finish(n->ref.Data,&s)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
            CHECK(!n->adapter->Finish(n->ref.Data,&s)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();cleanupFailures++;
        }else CHECK(n->adapter->Finish(n->ref.Data,&s)&&n->adapter->Finish(n->ref.Data,&s));
    }
    CHECK(xrtAtomic32Load(&s.listenerClose,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE)==1+s.clientCount);
    for(size_t i=0;i<s.futureCount;i++)xrtFutureDestroy(s.futures[i]);
    for(size_t i=0;i<s.clientCount;i++)xrtTlsStreamDestroy(s.clients[i]);
    xrtTlsStreamDestroy(s.accepted);xrtNetStreamDestroy(s.plain);xrtTlsListenerDestroy(s.listener);
    CHECK(xrtNetEngineTryDestroy(s.engine)==XNET_RETIRE_READY);
    for(size_t i=0;i<s.count;i++)s.nodes[i].adapter->Drop(s.nodes[i].ref.Data);
    CHECK(xrtAtomic32Load(&s.frees,XMEMORY_ACQUIRE)==1+s.clientCount);
    xrtTlsVerifierRelease(verifier);xrtTlsIdentityRelease(identity);xrtTlsContextRelease(tls);balanced(before);cases++;
}
int main(int argc,char** argv)
{
    xnetportbackend backend=XNET_PORT_SELECT;if(argc>1&&!strcmp(argv[1],"native")){
#if defined(_WIN32)
        backend=XNET_PORT_IOCP;
#else
        backend=XNET_PORT_EPOLL;
#endif
    }
    CHECK(xrtMemDebugEnable(true));startConstructionBudgets(backend);for(unsigned mode=0;mode<4;mode++){runCase(backend,mode,false);runCase(backend,mode,true);}CHECK(xrtMemDebugReset());
    printf("TLS Listener ownership: %u graphs; %u proofs; %u records; %u pending accepts; %u refusals; %u persistent failures; %u/%u start budgets; %u/%u accept budgets; balanced\n",cases,proofs,records,pendingWaits,refusals,cleanupFailures,startBudgets,startFailures,acceptBudgets,acceptFailures);return 0;
}
