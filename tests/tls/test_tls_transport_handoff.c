/* Owned callback publication and ordinary self-created loopback handoff only.
 * No DNS, external hosts, malformed records, probing or security acceptance. */
#ifdef TLS_TRANSPORT_HANDOFF_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <xrt/detail/wait.h>
#ifndef TLS_TRANSPORT_HANDOFF_SINGLE
#include "../../src/internal/xrt_tls_stream.h"
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%s:%u: ",__FILE__,(unsigned)__LINE__);testRequire(false,#c);}}while(0)
typedef struct Scene Scene;
typedef struct Context {volatile int32 refs;Scene* scene;} Context;
struct Scene {
    xnetengine* engine;xnetlistener* listener;xnetstream* client;xnetstream* server;
    xtlssession* session;xtlsstream* tls;
    xatomic32 opened,active,dropped,stage;
};
static unsigned cases,refusals,edges_seen;
static xnetportbackend backend=XNET_PORT_SELECT;
static bool contextCount(const void* data,size_t* count)
{const Context* c=data;if(!c||!count)return false;int32 n=__xrtAtomicRefLoad(&c->refs);if(n<=0)return false;*count=(size_t)n;return true;}
/* Scene is a borrowed, externally live C test scope, not an owning edge. */
static bool contextTrace(const void* data,xrtownershipvisitor visit,ptr arg)
{size_t n;(void)arg;return visit&&contextCount(data,&n);}
static const xrtownershipops contextOps={contextCount,contextTrace};
static void drop(const void* data)
{
    Context* c=(Context*)data;Scene* s=c->scene;
    CHECK(!xrtAtomic32Load(&s->active,XMEMORY_ACQUIRE));
    CHECK(xrtOwnershipRefRelease(&c->refs)==0);xrtFree(c);
    CHECK(xrtAtomic32FetchAdd(&s->dropped,1,XMEMORY_ACQ_REL)==0);
}
static void openEvent(xnetstream* stream,ptr data);
static const xnetstreameventsownershipv1 policy={sizeof(policy),{openEvent,NULL,NULL,NULL,NULL,NULL,NULL},drop,&contextOps};
static void openEvent(xnetstream* stream,ptr data)
{
    Context* c=data;Scene* s=c->scene;xrtAtomic32Store(&s->active,1,XMEMORY_RELEASE);
    CHECK(stream->EventOwner && stream->EventOwner->Data==data && stream->EventOwner->Policy==&policy);
    CHECK(xrtNetStreamData(stream)==data && !xrtAtomic32Load(&s->dropped,XMEMORY_ACQUIRE));
    xrtAtomic32Store(&s->active,0,XMEMORY_RELEASE);xrtAtomic32Store(&s->opened,1,XMEMORY_RELEASE);
}
static Context* context(Scene* s)
{Context* c=xrtCalloc(1,sizeof(*c));CHECK(c);c->refs=1;c->scene=s;return c;}
static void stage(Scene* s,unsigned want)
{double due=__xrtWaitAfter(10000);while(xrtAtomic32Load(&s->stage,XMEMORY_ACQUIRE)<want){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}}
static void idle(Scene* s,xrtownershipscope* freeze)
{
    double due=__xrtWaitAfter(10000);
    for(;;){
        if(xrtOwnershipFreezeTryBegin(freeze)){
            if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->engine))return;
            CHECK(xrtOwnershipScopeEnd(freeze));
        }CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
}
static bool edge(xrtownershipref ref,ptr data)
{if(ref.Ops==&contextOps){CHECK(ref.Data==data);++edges_seen;}return true;}
static xtlsverifydecision neverVerify(const xtlspeer* peer,ptr data)
{(void)peer;(void)data;CHECK(false);return XTLS_VERIFY_REJECT;}
static void failedAttach(xnetworker* worker,ptr data)
{
    (void)worker;Scene* s=data;xnetstream* tcp=s->client;
    xfuture* read=xrtNetStreamWaitAsync(tcp,XNET_STREAM_WAIT_READ);CHECK(read);
    xnetbufpool* previous[]={s->session->Feed.Pool,s->session->Send.Pool,s->session->Plain.Pool,s->session->Scratch.Pool};
    xtlsstream* output=(xtlsstream*)s;ptr owner=xrtNetStreamData(tcp);
    CHECK(!xrtTlsStreamAttach(tcp,s->session,NULL,NULL,NULL,&output) && !output);
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    CHECK(tcp->EventOwner && tcp->EventOwner->Policy==&policy && xrtNetStreamData(tcp)==owner);
    CHECK(s->session->Feed.Pool==previous[0] && s->session->Send.Pool==previous[1] && s->session->Plain.Pool==previous[2] && s->session->Scratch.Pool==previous[3]);
    CHECK(xrtTlsSessionState(s->session)==XTLS_STATE_HANDSHAKE && !xrtAtomic32Load(&s->dropped,XMEMORY_ACQUIRE));
    ++refusals;CHECK(xrtFutureCancel(read));xrtFutureDestroy(read);xrtAtomic32Store(&s->stage,1,XMEMORY_RELEASE);
}
static void attach(xnetworker* worker,ptr data)
{
    (void)worker;Scene* s=data;xrtAtomic32Store(&s->active,1,XMEMORY_RELEASE);
    CHECK(!s->client->ReadWaiters && xrtTlsStreamAttach(s->client,s->session,NULL,NULL,NULL,&s->tls));
    s->session=NULL; /* the successful Attach consumed exactly this reference */
    CHECK(s->tls && !s->client->EventOwner && s->client->RetiredEventOwners);
    CHECK(!xrtAtomic32Load(&s->dropped,XMEMORY_ACQUIRE));
    CHECK(xrtTlsStreamAbort(s->tls));
    xrtAtomic32Store(&s->active,0,XMEMORY_RELEASE);xrtAtomic32Store(&s->stage,2,XMEMORY_RELEASE);
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void lifecycle(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};
    xrtAtomic32Init(&s.opened,0);xrtAtomic32Init(&s.active,0);xrtAtomic32Init(&s.dropped,0);xrtAtomic32Init(&s.stage,0);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=backend;config.Workers=1;config.NodeCacheBytes=0;
    s.engine=xrtNetEngineCreate(&config);CHECK(s.engine&&xrtNetEngineStart(s.engine));
    xnetlistenconfig listener;xrtNetListenConfigInit(&listener);CHECK(xrtNetAddrLoopback(&listener.Address,XNET_FAMILY_IPV4,0));
    s.listener=xrtNetListen(s.engine,&listener,NULL,NULL,NULL);CHECK(s.listener);xnetaddr address;CHECK(xrtNetListenerLocal(s.listener,&address));
    Context* c=context(&s);s.client=xrtNetStreamConnectOwnedV1(s.engine,&address,0,&listener.Stream,c,&policy);CHECK(s.client);
    double due=__xrtWaitAfter(10000);while(!xrtAtomic32Load(&s.opened,XMEMORY_ACQUIRE)){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    while(!(s.server=xrtNetListenerAccept(s.listener))){CHECK(!xrtGetError()&&!__xrtWaitExpired(due));xrtThreadYield();}
    xrtownershipscope freeze={0};idle(&s,&freeze);
    const xnetstreameventsownershipv1* policies[]={&policy};const xrtownershippreparationv1* prep=NULL;
    xrtownershipref ref=xrtNetStreamOwnership(s.client);CHECK(xrtNetStreamOwnershipAdapterV2(ref,policies,1,&prep));
    CHECK(ref.Ops->Trace(ref.Data,edge,c));size_t count=0;CHECK(contextCount(c,&count)&&count==1);CHECK(xrtOwnershipScopeEnd(&freeze));
    xtlsverifierconfig verification;xrtTlsVerifierConfigInit(&verification);verification.Verify=neverVerify;
    xtlsverifier* verifier=xrtTlsVerifierCreate(&verification);CHECK(verifier);
    xtlsclientconfig tls;xrtTlsClientConfigInit(&tls);tls.Verifier=verifier;
    s.session=xrtTlsClientCreate(&tls,NULL);CHECK(s.session);xrtTlsVerifierRelease(verifier);
    CHECK(xrtNetEnginePost(s.engine,0,failedAttach,&s));stage(&s,1);
    for(;;){idle(&s,&freeze);bool ready=!s.client->ReadWaiters;CHECK(xrtOwnershipScopeEnd(&freeze));if(ready)break;CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    CHECK(xrtNetEnginePost(s.engine,0,attach,&s));stage(&s,2);
    due=__xrtWaitAfter(10000);
    /* Observe terminal worker fields only after its quiescence barrier. */
    for(;;){idle(&s,&freeze);bool closed=s.tls->CloseEmitted && xrtAtomic32Load(&s.dropped,XMEMORY_ACQUIRE)==1;
        CHECK(xrtOwnershipScopeEnd(&freeze));if(closed)break;CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    xrtTlsStreamDestroy(s.tls);CHECK(xrtNetStreamAbort(s.server));xrtNetStreamDestroy(s.server);
    CHECK(xrtNetListenerClose(s.listener));xrtNetListenerDestroy(s.listener);
    due=__xrtWaitAfter(10000);for(;;){xnetretireresult r=xrtNetEngineTryDestroy(s.engine);if(r==XNET_RETIRE_READY)break;CHECK(r==XNET_RETIRE_BUSY&&!__xrtWaitExpired(due));xrtThreadYield();}
    CHECK(!xrtGetError());balanced(before);++cases;
}
static void failedCreation(void)
{
    Scene s={0};xrtAtomic32Init(&s.dropped,0);Context* c=context(&s);xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<4;i++){
        CHECK(xrtMemDebugFailAfter(i));CHECK(!xrtNetStreamConnectOwnedV1(NULL,NULL,0,NULL,c,&policy));xrtMemDebugFailClear();
        CHECK(xrtGetError() && __xrtAtomicRefLoad(&c->refs)==1 && !xrtAtomic32Load(&s.dropped,XMEMORY_ACQUIRE));xrtClearError();balanced(before);++refusals;
    }
    CHECK(xrtOwnershipRefRelease(&c->refs)==0);xrtFree(c);
}
int main(int argc,char** argv)
{
    if(argc>1&&!strcmp(argv[1],"native")){
#if defined(_WIN32)
        backend=XNET_PORT_IOCP;
#else
        backend=XNET_PORT_EPOLL;
#endif
    }
    CHECK(xrtMemDebugEnable(true));failedCreation();for(unsigned i=0;i<3;i++)lifecycle();CHECK(xrtMemDebugReset());
    printf("TLS transport handoff: %u lifecycles; %u refusals; %u actual event edges; deferred drops; rollback; balanced\n",cases,refusals,edges_seen);return 0;
}
