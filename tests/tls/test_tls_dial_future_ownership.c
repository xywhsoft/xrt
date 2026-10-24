/* Fixed-key, explicit loopback only. No DNS, malformed traffic or security tests. */
#if defined(TLS_DIAL_FUTURE_OWNERSHIP_SINGLE) && !defined(TLS_DIAL_OWNERSHIP_PINNED)
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../fixtures/tls_server.h"
#include <xrt/detail/wait.h>
#if !defined(TLS_DIAL_FUTURE_OWNERSHIP_SINGLE) && !defined(TLS_DIAL_OWNERSHIP_PINNED)
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
typedef struct Context {volatile int32 refs;Scene* scene;xfuture* future;xfuture* retired;const void* claim;bool client,cleared,finished;} Context;
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prepare;} Node;
struct Scene {
    xnetengine* engine;xnetresolver* resolver;xtlslistener* listener;xnetlistener* plainListener;
    xfuture* future;xtlsstream* client;xtlsstream* server;xnetstream* plain;
    xnetengine* peerEngine;xnetlistener* peerListener;xnetstream* peerTransport;xtlsstream* peerTls;
    xtlsserverconfig peerConfig;xatomic32 peerAccepted,peerClosed,peerListenerClosed;
    xatomic32 open,done,closed,drops,frees,read,accepted;
    Context* context;Node nodes[128];size_t count;bool pinned,failure,budget;
    xnetresult result;
};
static unsigned cases,proofs,refusals,failures,budgets,budgetFailures;
static bool contextCount(const void* data,size_t* count)
{const Context* c=data;if(!c||!count||c->cleared)return false;int32 n=__xrtAtomicRefLoad(&c->refs);if(n<=0)return false;*count=(size_t)n;return true;}
static bool contextTrace(const void* data,xrtownershipvisitor visit,ptr arg)
{const Context* c=data;size_t n;return visit&&contextCount(c,&n)&&(!c->future||visit(xrtFutureOwnership(c->future),arg));}
static const xrtownershipops contextOps={contextCount,contextTrace};
static bool contextHold(const void* data)
{Context* c=(Context*)data;xrtownershipscope m={0};CHECK(xrtOwnershipMutationBegin(&m));bool held=!c->cleared&&xrtRefRetain(&c->refs)>=0;CHECK(xrtOwnershipScopeEnd(&m));return held;}
static void contextDrop(const void* data)
{Context* c=(Context*)data;int32 n=xrtOwnershipRefRelease(&c->refs);CHECK(n>=0);if(!n){CHECK(!c->future&&!c->retired);xrtAtomic32FetchAdd(&c->scene->frees,1,XMEMORY_ACQ_REL);xrtFree(c);}}
static bool contextClaim(const void* data,const void* token)
{Context* c=(Context*)data;size_t n;if(!token||(c->claim&&c->claim!=token)||!contextCount(c,&n))return false;c->claim=token;return true;}
static void contextRestore(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&!c->cleared);c->claim=NULL;}
static void contextClear(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&!c->cleared);c->retired=c->future;c->future=NULL;c->cleared=true;}
static bool contextFinish(const void* data,const void* token)
{Context* c=(Context*)data;CHECK(token&&c->claim==token&&c->cleared);if(!c->finished){xfuture* f=c->retired;c->retired=NULL;c->finished=true;xrtFutureDestroy(f);}return true;}
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
static bool acceptTls(xtlslistener* listener,xtlsstream* stream,ptr data)
{(void)listener;Context* c=data;CHECK(!c->scene->server);c->scene->server=stream;xrtAtomic32Store(&c->scene->accepted,1,XMEMORY_RELEASE);return true;}
static bool acceptPlain(xnetlistener* listener,xnetstream* stream,ptr data)
{(void)listener;Context* c=data;CHECK(!c->scene->plain);c->scene->plain=stream;xrtAtomic32Store(&c->scene->accepted,1,XMEMORY_RELEASE);return true;}
static const xtlsstreamownershipv1 pullPolicy={sizeof(pullPolicy),{opened,NULL,NULL,NULL,NULL,closed,NULL},dataDrop,&contextOps};
static const xtlsstreamownershipv1 pushPolicy={sizeof(pushPolicy),{opened,readClient,NULL,NULL,NULL,closed,NULL},dataDrop,&contextOps};
static const xtlslistenerownershipv1 listenerPolicy={sizeof(listenerPolicy),{acceptTls,NULL,NULL,NULL},{0},dataDrop,&contextOps};
static const xnetlistenereventsownershipv1 plainPolicy={sizeof(plainPolicy),{acceptPlain,NULL,NULL},dataDrop,&contextOps};
/* Separate peer scope: a normal CONNECT response followed by TLS on the same
 * explicitly local connection, not an external proxy or protocol-failure test. */
static bool peerAccept(xnetlistener* listener,xnetstream* stream,ptr data)
{(void)listener;Scene* s=data;CHECK(!s->peerTransport&&xrtNetStreamSetData(stream,s));s->peerTransport=stream;return true;}
static void peerClose(xtlsstream* stream,xnetresult result,const xerror* error,ptr data)
{(void)stream;(void)result;(void)error;Scene* s=data;xrtAtomic32Store(&s->peerClosed,1,XMEMORY_RELEASE);}
static void peerListenerClose(xnetlistener* listener,ptr data)
{(void)listener;Scene* s=data;xrtAtomic32Store(&s->peerListenerClosed,1,XMEMORY_RELEASE);}
static void peerRead(xnetstream* stream,xnetbuf* buffer,ptr data)
{
    Scene* s=data;char request[2048];size_t n=xrtNetBufSize(buffer);CHECK(n<sizeof(request));
    CHECK(xrtNetBufPeek(buffer,0,request,n)==n);size_t header=0;
    for(size_t i=3;i<n;i++)if(!memcmp(request+i-3,"\r\n\r\n",4)){header=i+1;break;}
    if(!header)return;
    static const char start[]="CONNECT 127.0.0.1:443 HTTP/1.1\r\n";
    CHECK(header>=sizeof(start)-1&&!memcmp(request,start,sizeof(start)-1));
    CHECK(xrtNetStreamConsume(stream,header)==header);
    static const char response[]="HTTP/1.1 200 Connection Established\r\n\r\n";
    CHECK(xrtNetStreamSend(stream,response,sizeof(response)-1)==XNET_RESULT_OK);
    xtlsstreamevents events={0};events.Close=peerClose;
    xtlssession* session=xrtTlsServerCreate(&s->peerConfig,NULL);CHECK(session);
    bool attached=xrtTlsStreamAttach(stream,session,NULL,&events,s,&s->peerTls);
    if(!attached)fprintf(stderr,"peer TLS attach: %s op=%s kind=%d\n",xrtErrorMessage(xrtGetError()),xrtErrorOperation(xrtGetError()),(int)xrtErrorKind(xrtGetError()));
    CHECK(attached);s->peerTransport=NULL;
    xrtAtomic32Store(&s->peerAccepted,1,XMEMORY_RELEASE);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,const xrtownershippreparationv1** prepare)
{
    *prepare=NULL;
    const xnetruntimeownershipv1* runtimes[]={xrtNetStreamRuntimeOwnershipV1Get(),xrtNetListenerRuntimeOwnershipV1Get(),xrtNetDialRuntimeOwnershipV1Get(),xrtTlsStreamRuntimeOwnershipV1Get(),xrtTlsListenerRuntimeOwnershipV1Get(),xrtTlsDialRuntimeOwnershipV1Get(),xrtNetProxyDialRuntimeOwnershipV1Get()};
    const xnettimerownershipv1* timers[]={xrtTlsDialTimerOwnershipV1Get()};
    const xnetdialownershipv1* dialData[]={xrtTlsDialTransportOwnershipV1Get(),xrtNetProxyDialTransportOwnershipV1Get()};
    const xnetproxydialownershipv1* proxyData[]={xrtTlsDialProxyTransportOwnershipV1Get()};
    const xtlsdialownershipv1* dials[]={xrtTlsDialFutureDialPolicyV1Get(),xrtTlsDialFutureDialPushPolicyV1Get()};
    const xtlsstreamownershipv1* dataPolicies[]={&pullPolicy,&pushPolicy};
    const xtlsstreamownershipv1* streams[]={xrtTlsDialStreamOwnershipV1Get(),xrtTlsDialStreamPushOwnershipV1Get(),xrtTlsListenerStreamOwnershipV1Get()};
    const xtlslistenerownershipv1* listeners[]={&listenerPolicy};
    const xnetlistenereventsownershipv1* nativeListeners[]={&plainPolicy,xrtTlsListenerTransportOwnershipV1Get()};
    const xnetstreameventsownershipv1* nativeStreams[]={xrtTlsStreamTransportOwnershipV1Get(),xrtNetProxyDialStreamOwnershipV1Get()};
    const xnetresolveownershipv1* resolverCallbacks[]={xrtNetDialResolverPolicyV1Get()};
    if(ref.Ops==&contextOps)return &contextAdapter;
    if(ref.Ops==xrtTlsDialFutureContextOwnershipQuiescenceV1Get()->Ops)return xrtTlsDialFutureContextOwnershipAdapterV1(ref,dataPolicies,2,prepare);
    if(ref.Ops==xrtNetEngineOwnershipQuiescenceV1Get()->Ops)return xrtNetEngineOwnershipAdapterV3(ref,NULL,0,timers,1,NULL,0,runtimes,7,prepare);
    if(ref.Ops==xrtNetDialOwnershipQuiescenceV1Get()->Ops)return xrtNetDialOwnershipAdapterV1(ref,dialData,2,prepare);
    if(ref.Ops==xrtNetProxyDialOwnershipQuiescenceV1Get()->Ops)return xrtNetProxyDialOwnershipAdapterV1(ref,proxyData,1,prepare);
    if(ref.Ops==xrtTlsDialOwnershipQuiescenceV1Get()->Ops)return xrtTlsDialOwnershipAdapterV1(ref,dials,2,prepare);
    if(ref.Ops==xrtNetStreamRuntimeOwnershipV1Get()->Ops)return xrtNetStreamOwnershipAdapterV2(ref,nativeStreams,2,prepare);
    if(ref.Ops==xrtNetListenerRuntimeOwnershipV1Get()->Ops)return xrtNetListenerOwnershipAdapterV2(ref,nativeListeners,2,prepare);
    if(ref.Ops==xrtTlsStreamOwnershipQuiescenceV1Get()->Ops)return xrtTlsStreamOwnershipAdapterV1(ref,streams,3,prepare);
    if(ref.Ops==xrtTlsListenerOwnershipQuiescenceV1Get()->Ops)return xrtTlsListenerOwnershipAdapterV1(ref,listeners,1,prepare);
    if(ref.Ops==xrtTlsListenerStreamRecordOwnershipQuiescenceV1Get()->Ops)return xrtTlsListenerStreamRecordOwnershipAdapterV1(ref,prepare);
    if(ref.Ops==xrtNetResolverOwnershipQuiescenceV1Get()->Ops)return xrtNetResolverOwnershipAdapterV1(ref,NULL,0,prepare);
    if(ref.Ops==xrtNetResolveOpOwnershipQuiescenceV1Get()->Ops)return xrtNetResolveOpOwnershipAdapterV1(ref,resolverCallbacks,1,prepare);
    if(ref.Ops==xrtNetBufPoolOwnershipQuiescenceV1Get()->Ops)return xrtNetBufPoolOwnershipAdapterV1(ref,prepare);
    const xfutureproducerownershipv1* producers[]={xrtTlsDialFutureProducerPolicyV1Get(),xrtTlsStreamFutureProducerPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[]={xrtTlsDialFutureCancelPolicyV1Get(),xrtTlsStreamFutureCancelPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[]={xrtTlsDialFuturePayloadPolicyV1Get(),xrtTlsStreamFutureBytesPayloadPolicyV1Get()};
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);admission.ProducerPolicies=producers;admission.ProducerPolicyCount=2;
    admission.CancelWatchPolicies=cancels;admission.CancelWatchPolicyCount=2;admission.PayloadPolicies=payloads;admission.PayloadPolicyCount=2;
    const xrtownershipadapterv1* a=xrtTlsDialFutureOwnershipAdapterV1(ref,prepare);
    if(!a)a=xrtTlsStreamFutureOwnershipAdapterV1(ref,prepare);
    if(!a)a=xrtFutureOwnershipAdapterV4(ref,&admission,prepare);
    if(!a)a=xrtCancelOwnershipAdapterV2(ref,cancels,2,prepare);
    if(!a)a=xrtCancelWatchOwnershipAdapterV1(ref,cancels,2,prepare);
    if(!a)a=xrtNetBytesOwnershipAdapterV1(ref);
    if(!a)a=xrtNetAddrListOwnershipAdapterV1(ref);
    if(!a)a=xrtNetProxyOwnershipAdapterV1(ref);
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
            if(s->future)anchors[n++]=xrtFutureOwnership(s->future);
            if(s->context)anchors[n++]=(xrtownershipref){s->context,&contextOps};
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
    if(mode>=5){
        contextDrop(server);s.peerEngine=xrtNetEngineCreate(&cfg);CHECK(s.peerEngine&&xrtNetEngineStart(s.peerEngine));
        xrtTlsServerConfigInit(&s.peerConfig);s.peerConfig.Context=tls;s.peerConfig.Identity=identity;
        xnetlistenconfig listen;xrtNetListenConfigInit(&listen);CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
        xnetlistenerevents le={0};le.Accept=peerAccept;le.Close=peerListenerClose;xnetstreamevents se={0};se.Read=peerRead;
        s.peerListener=xrtNetListen(s.peerEngine,&listen,&le,&se,&s);CHECK(s.peerListener&&xrtNetListenerLocal(s.peerListener,&address));
    }else if(mode<2){xtlslistenerconfig listen;xrtTlsListenerConfigInit(&listen);listen.Tls.Context=tls;listen.Tls.Identity=identity;
        CHECK(xrtNetAddrLoopback(&listen.Listen.Address,XNET_FAMILY_IPV4,0));s.listener=xrtTlsListenerStartOwnedV1(s.engine,&listen,server,&listenerPolicy);CHECK(s.listener&&xrtTlsListenerLocal(s.listener,&address));}
    else{xnetlistenconfig listen;xrtNetListenConfigInit(&listen);CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
        s.plainListener=xrtNetListenOwnedV1(s.engine,&listen,server,&plainPolicy);CHECK(s.plainListener&&xrtNetListenerLocal(s.plainListener,&address));}
    xtlsclientconfig client;xrtTlsClientConfigInit(&client);client.Context=tls;client.Verifier=verifier;client.ServerName=XRT_STR_LITERAL("example.com");
    xtlsdialconfig dial;xrtTlsDialConfigInit(&dial);dial.Timeout=TLS_DIAL_TEST_DURATION(mode==3?100:10000);
    s.context=context(&s,true);CHECK(contextHold(s.context)); /* Scene observer + one provisional Data credit */
    if(s.budget)CHECK(xrtMemDebugFailAfter(allocationBudget));
    if(mode>=5){xnetproxyconfig pc;xrtNetProxyConfigInit(&pc);pc.Type=XNET_PROXY_HTTP_CONNECT;pc.Host=XRT_STR_LITERAL("127.0.0.1");pc.Port=address.Port;
        xnetproxy* proxy=xrtNetProxyCreate(&pc);CHECK(proxy);
        s.future=xrtTlsDialProxyAsyncOwnedV1(s.engine,s.resolver,proxy,"127.0.0.1",443,&client,&dial,s.context,mode==6?&pushPolicy:&pullPolicy);xrtNetProxyRelease(proxy);
    }else s.future=xrtTlsDialAsyncOwnedV1(s.engine,s.resolver,"127.0.0.1",address.Port,&client,&dial,s.context,mode==1?&pushPolicy:&pullPolicy);
    if(s.budget){xrtMemDebugFailClear();budgets++;}
    bool submitted=s.future!=NULL;
    if(s.future){s.context->future=xrtFutureRef(s.future);CHECK(s.context->future);}
    else{if(!s.budget)fprintf(stderr,"mode=%u create error: %s op=%s kind=%d\n",mode,xrtErrorMessage(xrtGetError()),xrtErrorOperation(xrtGetError()),(int)xrtErrorKind(xrtGetError()));
        CHECK(s.budget&&xrtGetError());xrtClearError();CHECK(!xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE)&&__xrtAtomicRefLoad(&s.context->refs)==2);contextDrop(s.context);budgetFailures++;}
    if(s.budget){if(s.future){(void)xrtFutureCancel(s.future);CHECK(xrtFutureWaitFor(s.future,5000)==XWAIT_OK);}}
    else{
        waitValue(mode>=5?&s.peerAccepted:&s.accepted,1);
        if(mode==4){CHECK(xrtFutureCancel(s.future));CHECK(xrtFutureWaitFor(s.future,5000)==XWAIT_OK&&xrtFutureState(s.future)==XFUTURE_CANCELLED);}
        else if(mode!=2){CHECK(xrtFutureWaitFor(s.future,5000)==XWAIT_OK);
            if(mode==3)CHECK(xrtFutureState(s.future)==XFUTURE_FAILED&&xrtErrorKind(xrtFutureError(s.future))==XERR_TIMEOUT);
            else CHECK(xrtFutureState(s.future)==XFUTURE_RESOLVED&&xrtAtomic32Load(&s.open,XMEMORY_ACQUIRE)==1);}
        else CHECK(xrtFutureState(s.future)==XFUTURE_PENDING);
    }
    if(s.future&&xrtFutureState(s.future)==XFUTURE_RESOLVED){s.client=xrtTlsStreamRef(xrtFutureValue(s.future));CHECK(s.client);if(s.budget)CHECK(xrtTlsStreamAbort(s.client));}
    if(mode==1||mode==6){xfuture* sent=xrtTlsStreamSendAsync(mode==6?s.peerTls:s.server,"t\0x",3);CHECK(sent&&xrtFutureWaitFor(sent,5000)==XWAIT_OK&&xrtFutureState(sent)==XFUTURE_RESOLVED);xrtFutureDestroy(sent);waitValue(&s.read,3);}
    xrtownershipscope freeze={0};xrtownershipsnapshot* proof=snapshot(&s,&freeze);proofs++;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);i++){
        xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);
        if(item.Reference.Ops==xrtTlsDialFutureContextOwnershipQuiescenceV1Get()->Ops){
            xtlsstreamownershipv1 copy=pullPolicy;const xtlsstreamownershipv1* unknown[]={&copy};
            const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&s;const xrtownershippreparationv1* out=sentinel;
            CHECK(!xrtTlsDialFutureContextOwnershipAdapterV1(item.Reference,unknown,1,&out)&&out==sentinel);refusals++;
        }
    }
    discover(&s,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<s.count;i++)if(s.nodes[i].prepare){s.nodes[i].adapter->Restore(s.nodes[i].ref.Data,&s);CHECK(s.nodes[i].adapter->Claim(s.nodes[i].ref.Data,&s));}
    CHECK(xrtOwnershipScopeEnd(&freeze));
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
        if(failure&&n->ref.Ops==xrtTlsDialFutureContextOwnershipQuiescenceV1Get()->Ops){
            xrtSetErrorKind(XERR_ARGUMENT);CHECK(!n->adapter->Finish(n->ref.Data,&s)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
            CHECK(!n->adapter->Finish(n->ref.Data,&s)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();failures++;
        }else CHECK(n->adapter->Finish(n->ref.Data,&s)&&n->adapter->Finish(n->ref.Data,&s));}
    CHECK(xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE)==(mode>=5?0u:1u)+(submitted?1u:0u));
    if(mode==2)CHECK(xrtFutureState(s.future)!=XFUTURE_RESOLVED&&xrtFutureState(s.future)!=XFUTURE_PENDING);
    xrtTlsStreamDestroy(s.client);xrtTlsStreamDestroy(s.server);xrtNetStreamDestroy(s.plain);xrtFutureDestroy(s.future);contextDrop(s.context);
    xrtTlsListenerDestroy(s.listener);xrtNetListenerDestroy(s.plainListener);xrtNetResolverDestroy(s.resolver);
    CHECK(xrtNetEngineTryDestroy(s.engine)==XNET_RETIRE_READY);for(size_t i=0;i<s.count;i++)s.nodes[i].adapter->Drop(s.nodes[i].ref.Data);
    if(mode>=5){CHECK(xrtTlsStreamAbort(s.peerTls));waitValue(&s.peerClosed,1);CHECK(xrtNetListenerClose(s.peerListener));waitValue(&s.peerListenerClosed,1);
        xrtTlsStreamDestroy(s.peerTls);xrtNetListenerDestroy(s.peerListener);
        TLS_DIAL_TEST_DEADLINE until=__xrtWaitAfter(10000);for(;;){xnetretireresult state=xrtNetEngineTryDestroy(s.peerEngine);if(state==XNET_RETIRE_READY)break;CHECK(state==XNET_RETIRE_BUSY&&!__xrtWaitExpired(until));xrtThreadYield();}}
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
    CHECK(xrtMemDebugEnable(true));for(unsigned i=0;i<7;i++){runCase(backend,i,false,SIZE_MAX);if(i<3||i>=5)runCase(backend,i,true,SIZE_MAX);}
    unsigned successes=0;for(size_t budget=0;budget<64&&successes<2;budget++){if(runCase(backend,0,false,budget))successes++;else successes=0;}CHECK(successes==2&&budgetFailures);
    CHECK(xrtMemDebugReset());
    printf("TLS Dial Future ownership: %u graphs; %u proofs; %u refusals; %u persistent failures; %u/%u budgets; balanced\n",cases,proofs,refusals,failures,budgets,budgetFailures);return 0;
}
