/* Fixed-key, explicit loopback only. No DNS, malformed traffic or security tests. */
#if defined(TLS_DIAL_OWNERSHIP_SINGLE) && !defined(TLS_DIAL_OWNERSHIP_PINNED)
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../fixtures/tls_server.h"
#include <xrt/detail/wait.h>
#if !defined(TLS_DIAL_OWNERSHIP_SINGLE) && !defined(TLS_DIAL_OWNERSHIP_PINNED)
#include "../../src/internal/xrt_tls_stream.h"
#endif
#define CHECK(c) do {if(!(c)){fprintf(stderr,"line %u: ",(unsigned)__LINE__);testRequire(false,#c);}}while(0)
#ifndef TLS_DIAL_TEST_DURATION
#define TLS_DIAL_TEST_DURATION(n) (n)
#endif
#ifndef TLS_DIAL_TEST_DEADLINE
#define TLS_DIAL_TEST_DEADLINE double
#endif
typedef struct Scene Scene;
typedef struct Context {volatile int32 refs;Scene* scene;xtlsdial* dial;xtlsdial* retired;const void* claim;bool client,cleared,finished;} Context;
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prepare;} Node;
struct Scene {
    xnetengine* engine;xnetresolver* resolver;xtlslistener* listener;xnetlistener* plainListener;
    xtlsdial* dial;xtlsstream* client;xtlsstream* server;xnetstream* plain;
    xatomic32 open,done,closed,drops,frees,read,accepted;
    Context* context;Node nodes[128];size_t count;bool pinned,failure,budget;
    xnetresult result;
};
static unsigned cases,proofs,refusals,failures,budgets,budgetFailures;
static bool contextCount(const void* data,size_t* count)
{const Context* c=data;if(!c||!count||c->cleared)return false;int32 n=__xrtAtomicRefLoad(&c->refs);if(n<=0)return false;*count=(size_t)n;return true;}
static bool contextTrace(const void* data,xrtownershipvisitor visit,ptr arg)
{const Context* c=data;size_t n;return visit&&contextCount(c,&n)&&(!c->dial||visit(xrtTlsDialOwnership(c->dial),arg));}
static const xrtownershipops contextOps={contextCount,contextTrace};
static bool contextHold(const void* data)
{Context* c=(Context*)data;xrtownershipscope m={0};CHECK(xrtOwnershipMutationBegin(&m));bool held=!c->cleared&&xrtRefRetain(&c->refs)>=0;CHECK(xrtOwnershipScopeEnd(&m));return held;}
static void contextDrop(const void* data)
{Context* c=(Context*)data;int32 n=xrtOwnershipRefRelease(&c->refs);CHECK(n>=0);if(!n){CHECK(!c->dial&&!c->retired);xrtAtomic32FetchAdd(&c->scene->frees,1,XMEMORY_ACQ_REL);xrtFree(c);}}
static bool contextClaim(const void* data,const void* token)
{Context* c=(Context*)data;size_t n;if(!token||(c->claim&&c->claim!=token)||!contextCount(c,&n))return false;c->claim=token;return true;}
static void contextRestore(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&!c->cleared);c->claim=NULL;}
static void contextClear(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&!c->cleared);c->retired=c->dial;c->dial=NULL;c->cleared=true;}
static bool contextFinish(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&c->cleared);if(!c->finished){xtlsdial* d=c->retired;c->retired=NULL;c->finished=true;xrtTlsDialDestroy(d);}return true;}
static const xrtownershipadapterv1 contextAdapter={sizeof(contextAdapter),contextHold,contextDrop,contextClaim,contextRestore,NULL,contextClear,contextFinish};
static Context* context(Scene* s,bool client)
{Context* c=xrtCalloc(1,sizeof(*c));CHECK(c);c->refs=1;c->scene=s;c->client=client;return c;}
static void dataDrop(const void* data)
{Context* c=(Context*)data;Scene* s=c->scene;bool fail=c->client&&s->failure;xrtAtomic32FetchAdd(&s->drops,1,XMEMORY_ACQ_REL);contextDrop(c);if(fail)xrtSetErrorKind(XERR_STATE);}
static void opened(xtlsstream* stream,ptr data)
{Context* c=data;CHECK(c->client&&!c->cleared&&xrtTlsStreamData(stream)!=c);xrtAtomic32FetchAdd(&c->scene->open,1,XMEMORY_ACQ_REL);}
static void closed(xtlsstream* stream,xnetresult result,const xerror* error,ptr data)
{(void)stream;(void)result;(void)error;Context* c=data;CHECK(!c->cleared);xrtAtomic32FetchAdd(&c->scene->closed,1,XMEMORY_ACQ_REL);}
static void readClient(xtlsstream* stream,const xnetbuf* buffer,ptr data)
{(void)buffer;Context* c=data;uint8 text[3];size_t n=0;CHECK(xrtTlsStreamRead(stream,text,3,&n)==XTLS_OK&&n==3&&!memcmp(text,"t\0x",3));xrtAtomic32Store(&c->scene->read,3,XMEMORY_RELEASE);}
static void done(xtlsdial* dial,xnetresult result,xtlsstream* stream,const xerror* error,ptr data)
{
    Context* c=data;Scene* s=c->scene;CHECK(!c->cleared&&!xrtAtomic32Load(&s->done,XMEMORY_ACQUIRE));
    if(result==XNET_RESULT_OK){CHECK(stream&&xrtAtomic32Load(&s->open,XMEMORY_ACQUIRE)==1&&!error);
        if(s->budget){CHECK(xrtTlsStreamAbort(stream));xrtTlsStreamDestroy(stream);}else s->client=stream;}
    else CHECK(!stream&&xrtTlsDialState(dial)!=XTLS_DIAL_CONNECTED);
    s->result=result;xrtAtomic32Store(&s->done,1,XMEMORY_RELEASE);
}
static bool acceptTls(xtlslistener* listener,xtlsstream* stream,ptr data)
{(void)listener;Context* c=data;CHECK(!c->scene->server);c->scene->server=stream;xrtAtomic32Store(&c->scene->accepted,1,XMEMORY_RELEASE);return true;}
static bool acceptPlain(xnetlistener* listener,xnetstream* stream,ptr data)
{(void)listener;Context* c=data;CHECK(!c->scene->plain);c->scene->plain=stream;xrtAtomic32Store(&c->scene->accepted,1,XMEMORY_RELEASE);return true;}
static const xtlsdialownershipv1 pullPolicy={sizeof(pullPolicy),{opened,NULL,NULL,NULL,NULL,closed,NULL},done,dataDrop,&contextOps};
static const xtlsdialownershipv1 pushPolicy={sizeof(pushPolicy),{opened,readClient,NULL,NULL,NULL,closed,NULL},done,dataDrop,&contextOps};
static const xtlslistenerownershipv1 listenerPolicy={sizeof(listenerPolicy),{acceptTls,NULL,NULL,NULL},{0},dataDrop,&contextOps};
static const xnetlistenereventsownershipv1 plainPolicy={sizeof(plainPolicy),{acceptPlain,NULL,NULL},dataDrop,&contextOps};
static const xrtownershipadapterv1* resolve(xrtownershipref ref,const xrtownershippreparationv1** prepare)
{
    *prepare=NULL;
    const xnetruntimeownershipv1* runtimes[]={xrtNetStreamRuntimeOwnershipV1Get(),xrtNetListenerRuntimeOwnershipV1Get(),xrtNetDialRuntimeOwnershipV1Get(),xrtTlsStreamRuntimeOwnershipV1Get(),xrtTlsListenerRuntimeOwnershipV1Get(),xrtTlsDialRuntimeOwnershipV1Get()};
    const xnettimerownershipv1* timers[]={xrtTlsDialTimerOwnershipV1Get()};
    const xnetdialownershipv1* dialData[]={xrtTlsDialTransportOwnershipV1Get()};
    const xtlsdialownershipv1* dials[]={&pullPolicy,&pushPolicy};
    const xtlsstreamownershipv1* streams[]={xrtTlsDialStreamOwnershipV1Get(),xrtTlsDialStreamPushOwnershipV1Get(),xrtTlsListenerStreamOwnershipV1Get()};
    const xtlslistenerownershipv1* listeners[]={&listenerPolicy};
    const xnetlistenereventsownershipv1* nativeListeners[]={&plainPolicy,xrtTlsListenerTransportOwnershipV1Get()};
    const xnetstreameventsownershipv1* nativeStreams[]={xrtTlsStreamTransportOwnershipV1Get()};
    const xnetresolveownershipv1* resolverCallbacks[]={xrtNetDialResolverPolicyV1Get()};
    if(ref.Ops==&contextOps)return &contextAdapter;
    if(ref.Ops==xrtNetEngineOwnershipQuiescenceV1Get()->Ops)return xrtNetEngineOwnershipAdapterV3(ref,NULL,0,timers,1,NULL,0,runtimes,6,prepare);
    if(ref.Ops==xrtNetDialOwnershipQuiescenceV1Get()->Ops)return xrtNetDialOwnershipAdapterV1(ref,dialData,1,prepare);
    if(ref.Ops==xrtTlsDialOwnershipQuiescenceV1Get()->Ops)return xrtTlsDialOwnershipAdapterV1(ref,dials,2,prepare);
    if(ref.Ops==xrtNetStreamRuntimeOwnershipV1Get()->Ops)return xrtNetStreamOwnershipAdapterV2(ref,nativeStreams,1,prepare);
    if(ref.Ops==xrtNetListenerRuntimeOwnershipV1Get()->Ops)return xrtNetListenerOwnershipAdapterV2(ref,nativeListeners,2,prepare);
    if(ref.Ops==xrtTlsStreamOwnershipQuiescenceV1Get()->Ops)return xrtTlsStreamOwnershipAdapterV1(ref,streams,3,prepare);
    if(ref.Ops==xrtTlsListenerOwnershipQuiescenceV1Get()->Ops)return xrtTlsListenerOwnershipAdapterV1(ref,listeners,1,prepare);
    if(ref.Ops==xrtTlsListenerStreamRecordOwnershipQuiescenceV1Get()->Ops)return xrtTlsListenerStreamRecordOwnershipAdapterV1(ref,prepare);
    if(ref.Ops==xrtNetResolverOwnershipQuiescenceV1Get()->Ops)return xrtNetResolverOwnershipAdapterV1(ref,NULL,0,prepare);
    if(ref.Ops==xrtNetResolveOpOwnershipQuiescenceV1Get()->Ops)return xrtNetResolveOpOwnershipAdapterV1(ref,resolverCallbacks,1,prepare);
    if(ref.Ops==xrtNetBufPoolOwnershipQuiescenceV1Get()->Ops)return xrtNetBufPoolOwnershipAdapterV1(ref,prepare);
    const xrtownershipadapterv1* a=xrtNetAddrListOwnershipAdapterV1(ref);
    if(!a)a=xrtErrorOwnershipAdapterV1(ref);
    return a;
}
static bool admit(xrtownershipref ref,ptr data)
{(void)data;const xrtownershippreparationv1* p=NULL;return resolve(ref,&p)!=NULL;}
static void waitValue(const xatomic32* value,uint32 expected)
{TLS_DIAL_TEST_DEADLINE due=__xrtWaitAfter(10000);while(xrtAtomic32Load(value,XMEMORY_ACQUIRE)<expected){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}}
static xrtownershipsnapshot* snapshot(Scene* s,xrtownershipscope* freeze)
{
    TLS_DIAL_TEST_DEADLINE due=__xrtWaitAfter(10000);
    for(;;){if(xrtOwnershipFreezeTryBegin(freeze)){
        if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->engine)){
            xrtownershipref anchors[140],slots[140];size_t n=0;
            anchors[n++]=xrtNetEngineOwnership(s->engine);anchors[n++]=xrtNetResolverOwnership(s->resolver);
            if(s->dial)anchors[n++]=xrtTlsDialOwnership(s->dial);
            if(s->listener)anchors[n++]=xrtTlsListenerOwnership(s->listener);
            if(s->plainListener)anchors[n++]=xrtNetListenerOwnership(s->plainListener);
            if(s->client)anchors[n++]=xrtTlsStreamOwnership(s->client);
            if(s->server)anchors[n++]=xrtTlsStreamOwnership(s->server);
            if(s->plain)anchors[n++]=xrtNetStreamOwnership(s->plain);
            if(s->pinned)for(size_t i=0;i<s->count;i++)anchors[n++]=s->nodes[i].ref;
            CHECK(n<=140);for(size_t i=0;i<n;i++)slots[i]=anchors[i];xrtownershipsnapshot* proof=NULL;
            if(xrtOwnershipSnapshotCreate(anchors,n,slots,n,admit,s,&proof))return proof;
            xrtClearError();
        }CHECK(xrtOwnershipScopeEnd(freeze));}
        CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
}
static void discover(Scene* s,xrtownershipsnapshot* proof)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);i++){
        xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);
        size_t at=0;while(at<s->count&&(s->nodes[at].ref.Data!=item.Reference.Data||s->nodes[at].ref.Ops!=item.Reference.Ops))at++;
        if(at<s->count)continue;
        CHECK(s->count<128);Node* n=&s->nodes[s->count++];n->ref=item.Reference;n->adapter=resolve(n->ref,&n->prepare);CHECK(n->adapter);
        CHECK(n->adapter->Hold(n->ref.Data)&&n->adapter->Claim(n->ref.Data,s));
    }s->pinned=true;
}
static void balanced(xmemdebugsnapshot before)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);CHECK(!xrtGetError());CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);}
static bool runCase(xnetportbackend backend,unsigned mode,bool failure,size_t allocationBudget)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};s.failure=failure;s.budget=allocationBudget!=SIZE_MAX;
    xnetengineconfig cfg;xrtNetEngineConfigInit(&cfg);cfg.Workers=2;cfg.Backend=backend;cfg.NodeCacheBytes=4096;
    s.engine=xrtNetEngineCreate(&cfg);CHECK(s.engine&&xrtNetEngineStart(s.engine));s.resolver=xrtNetResolverCreate(NULL);CHECK(s.resolver);
    xtlscontext* tls=testTlsServerContext();xtlsidentity* identity=testTlsServerIdentity();CHECK(tls&&identity);
    xtlsverifierconfig v;xrtTlsVerifierConfigInit(&v);v.Verify=testTlsServerAccept;xtlsverifier* verifier=xrtTlsVerifierCreate(&v);CHECK(verifier);
    xnetaddr address;Context* server=context(&s,false);
    if(mode<2){xtlslistenerconfig listen;xrtTlsListenerConfigInit(&listen);listen.Tls.Context=tls;listen.Tls.Identity=identity;
        CHECK(xrtNetAddrLoopback(&listen.Listen.Address,XNET_FAMILY_IPV4,0));s.listener=xrtTlsListenerStartOwnedV1(s.engine,&listen,server,&listenerPolicy);CHECK(s.listener&&xrtTlsListenerLocal(s.listener,&address));}
    else{xnetlistenconfig listen;xrtNetListenConfigInit(&listen);CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
        s.plainListener=xrtNetListenOwnedV1(s.engine,&listen,server,&plainPolicy);CHECK(s.plainListener&&xrtNetListenerLocal(s.plainListener,&address));}
    xtlsclientconfig client;xrtTlsClientConfigInit(&client);client.Context=tls;client.Verifier=verifier;client.ServerName=XRT_STR_LITERAL("example.com");
    xtlsdialconfig dial;xrtTlsDialConfigInit(&dial);dial.Timeout=TLS_DIAL_TEST_DURATION(mode==3?100:10000);
    s.context=context(&s,true);if(s.budget)CHECK(xrtMemDebugFailAfter(allocationBudget));
    s.dial=xrtTlsDialOwnedV1(s.engine,s.resolver,"127.0.0.1",address.Port,&client,&dial,s.context,mode==1?&pushPolicy:&pullPolicy);
    if(s.budget){xrtMemDebugFailClear();budgets++;}
    bool submitted=s.dial!=NULL;
    if(s.dial){s.context->dial=xrtTlsDialRef(s.dial);CHECK(s.context->dial);}
    else{CHECK(s.budget&&xrtGetError());xrtClearError();CHECK(!xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE)&&__xrtAtomicRefLoad(&s.context->refs)==1);contextDrop(s.context);s.context=NULL;budgetFailures++;}
    if(s.budget){if(s.dial){(void)xrtTlsDialCancel(s.dial);waitValue(&s.done,1);}}
    else{waitValue(&s.accepted,1);
    if(mode==4){CHECK(xrtTlsDialCancel(s.dial));waitValue(&s.done,1);CHECK(s.result==XNET_RESULT_CANCELLED);}
    else if(mode!=2){waitValue(&s.done,1);CHECK(s.result==(mode==3?XNET_RESULT_TIMEOUT:XNET_RESULT_OK));}
    else {TLS_DIAL_TEST_DEADLINE due=__xrtWaitAfter(10000);while(xrtTlsDialState(s.dial)!=XTLS_DIAL_HANDSHAKE){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}}
    if(mode==1){xfuture* sent=xrtTlsStreamSendAsync(s.server,"t\0x",3);CHECK(sent&&xrtFutureWaitFor(sent,5000)==XWAIT_OK&&xrtFutureState(sent)==XFUTURE_RESOLVED);xrtFutureDestroy(sent);waitValue(&s.read,3);}}
    xrtownershipscope freeze={0};xrtownershipsnapshot* proof=snapshot(&s,&freeze);proofs++;
    xtlsdialownershipv1 copy=pullPolicy;const xtlsdialownershipv1* unknown[]={&copy};
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&s;const xrtownershippreparationv1* out=sentinel;
    if(s.dial){CHECK(!xrtTlsDialOwnershipAdapterV1(xrtTlsDialOwnership(s.dial),unknown,1,&out)&&out==sentinel);refusals++;}
    discover(&s,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<s.count;i++)if(s.nodes[i].prepare){s.nodes[i].adapter->Restore(s.nodes[i].ref.Data,&s);CHECK(s.nodes[i].adapter->Claim(s.nodes[i].ref.Data,&s));}
    CHECK(xrtOwnershipScopeEnd(&freeze));if(s.dial){CHECK(!xrtTlsDialCancel(s.dial));xrtClearError();refusals++;}
    TLS_DIAL_TEST_DEADLINE due=__xrtWaitAfter(10000);
    for(;;){bool all=true;
        for(size_t step=0;step<s.count;step++){size_t i=(step+1)%s.count;Node* n=&s.nodes[i];if(!n->prepare)continue;
            proof=snapshot(&s,&freeze);discover(&s,proof);xrtOwnershipSnapshotDestroy(proof);bool ready=n->prepare->Ready(n->ref.Data);CHECK(xrtOwnershipScopeEnd(&freeze));
            if(!ready){CHECK(n->prepare->Prepare(n->ref.Data,&s)!=XRT_OWNERSHIP_PREPARE_FAILED);all=false;}}
        if(all)break;
        CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    proof=snapshot(&s,&freeze);discover(&s,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<s.count;i++){Node* n=&s.nodes[i];if(n->prepare)CHECK(n->prepare->Ready(n->ref.Data));n->adapter->Clear(n->ref.Data,&s);}CHECK(xrtOwnershipScopeEnd(&freeze));
    for(size_t i=0;i<s.count;i++){Node* n=&s.nodes[i];if(!n->adapter->Finish)continue;
        if(failure&&n->ref.Data==s.dial){xrtSetErrorKind(XERR_ARGUMENT);CHECK(!n->adapter->Finish(n->ref.Data,&s)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
            CHECK(!n->adapter->Finish(n->ref.Data,&s)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();failures++;}
        else CHECK(n->adapter->Finish(n->ref.Data,&s)&&n->adapter->Finish(n->ref.Data,&s));}
    CHECK(xrtAtomic32Load(&s.done,XMEMORY_ACQUIRE)==(submitted?1u:0u)&&xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE)==(submitted?2u:1u));
    if(mode==2)CHECK(s.result!=XNET_RESULT_OK);
    xrtTlsStreamDestroy(s.client);xrtTlsStreamDestroy(s.server);xrtNetStreamDestroy(s.plain);xrtTlsDialDestroy(s.dial);
    xrtTlsListenerDestroy(s.listener);xrtNetListenerDestroy(s.plainListener);xrtNetResolverDestroy(s.resolver);
    CHECK(xrtNetEngineTryDestroy(s.engine)==XNET_RETIRE_READY);for(size_t i=0;i<s.count;i++)s.nodes[i].adapter->Drop(s.nodes[i].ref.Data);
    CHECK(xrtAtomic32Load(&s.frees,XMEMORY_ACQUIRE)==2);xrtTlsVerifierRelease(verifier);xrtTlsIdentityRelease(identity);xrtTlsContextRelease(tls);balanced(before);cases++;return submitted;
}
int main(int argc,char** argv)
{
    xnetportbackend backend=XNET_PORT_SELECT;if(argc>1&&!strcmp(argv[1],"native")){
#ifdef _WIN32
        backend=XNET_PORT_IOCP;
#else
        backend=XNET_PORT_EPOLL;
#endif
    }
    CHECK(xrtMemDebugEnable(true));for(unsigned i=0;i<5;i++){runCase(backend,i,false,SIZE_MAX);runCase(backend,i,true,SIZE_MAX);}
    unsigned successes=0;for(size_t budget=0;budget<64&&successes<2;budget++){if(runCase(backend,0,false,budget))successes++;else successes=0;}CHECK(successes==2&&budgetFailures);
    CHECK(xrtMemDebugReset());
    printf("TLS Dial ownership: %u graphs; %u proofs; %u refusals; %u persistent failures; %u/%u budgets; balanced\n",cases,proofs,refusals,failures,budgets,budgetFailures);return 0;
}
