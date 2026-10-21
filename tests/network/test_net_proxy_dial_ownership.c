/* Only successful fixed proxy replies and controlled cancel/timeout. The
 * foreign peer has its OWN Engine/owner scope; TCP connectivity is not an RC
 * edge. No system DNS, malformed traffic, fuzzing or security acceptance. */
#ifdef NET_PROXY_DIAL_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include <xrt.h>
static xnetportbackend ownedLegacyBackend=XNET_PORT_SELECT;
#define TEST_PROXY_DIAL_BACKEND ownedLegacyBackend
#define main proxyLegacyMainNotExecuted
#include "test_net_proxy_dial.c"
#undef main
#ifndef NET_PROXY_DIAL_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_proxy.h"
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%s:%u: ",__FILE__,(unsigned)__LINE__);testRequire(false,#c);}}while(0)
typedef struct OwnedProxyScene {
    volatile int32 refs;
    xnetengine* engine;xnetresolver* resolver;xnetproxy* proxy;xnetproxydial* dial;
    xnetstream* stream;xerror* error;xatomic32 done,lookupProceed,lookupEntered;
    xnetresult result;bool busy;
} OwnedProxyScene;
static unsigned owned_cases,closed_graphs,policy_refusals,constructor_budgets,constructor_failures;
static bool ownedCount(const void* data,size_t* count)
{
    const OwnedProxyScene* s=data;if(!s||!count)return false;
    int32 refs=__xrtAtomicRefLoad(&s->refs);if(refs<=0)return false;
    *count=(size_t)refs;return true;
}
static bool ownedTrace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const OwnedProxyScene* s=data;size_t count;if(!visit||!ownedCount(s,&count))return false;
    if(s->engine&&!visit(xrtNetEngineOwnership(s->engine),context))return false;
    if(s->resolver&&!visit(xrtNetResolverOwnership(s->resolver),context))return false;
    if(s->proxy&&!visit(xrtNetProxyOwnership(s->proxy),context))return false;
    if(s->dial&&!visit(xrtNetProxyDialOwnership(s->dial),context))return false;
    if(s->stream&&!visit(xrtNetStreamOwnership(s->stream),context))return false;
    xrtownershipref error=xrtErrorOwnership(s->error);return !error.Data||visit(error,context);
}
static const xrtownershipops ownedOps={ownedCount,ownedTrace};
static OwnedProxyScene* ownedHold(OwnedProxyScene* s)
{CHECK(xrtOwnershipRefRetain(&s->refs)>0);return s;}
static void ownedDrop(const void* data)
{
    OwnedProxyScene* s=(OwnedProxyScene*)data;int32 refs=xrtOwnershipRefRelease(&s->refs);CHECK(refs>=0);
    if(!refs){CHECK(!s->engine&&!s->resolver&&!s->proxy&&!s->dial&&!s->stream&&!s->error);xrtFree(s);}
}
static xnetaddrlist* ownedLookup(cstr host,xnetfamily family,ptr data)
{
    (void)host;(void)family;OwnedProxyScene* s=data;double due=__xrtWaitAfter(10000);
    xrtAtomic32Store(&s->lookupEntered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&s->lookupProceed,XMEMORY_ACQUIRE)){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
    xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));return xrtNetAddrListCreate(&address,1);
}
static const xnetresolverlookupownershipv1 ownedLookupPolicy={sizeof(ownedLookupPolicy),ownedLookup,ownedDrop,&ownedOps};
static void ownedDone(xnetproxydial* dial,xnetresult result,xnetstream* stream,const xerror* error,ptr data)
{
    (void)dial;OwnedProxyScene* s=data;CHECK(!s->stream&&!s->error);
    s->stream=stream;s->error=xrtErrorRef(error);s->result=result;
    xrtAtomic32Store(&s->done,1,XMEMORY_RELEASE);
}
static const xnetproxydialownershipv1 ownedDonePolicy={sizeof(ownedDonePolicy),ownedDone,ownedDrop,&ownedOps};
static void ownedFreeze(xrtownershipscope* scope)
{double due=__xrtWaitAfter(10000);while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}}
static const xrtownershipadapterv1* ownedResolve(OwnedProxyScene* s,xrtownershipref ref,const xrtownershippreparationv1** prep)
{
    const xrtownershipquiescencev1* quiet[]={xrtNetEngineOwnershipQuiescenceV1Get(),xrtNetResolverOwnershipQuiescenceV1Get(),
        xrtNetResolveOpOwnershipQuiescenceV1Get(),xrtNetStreamOwnershipQuiescenceV1Get(),xrtNetBufPoolOwnershipQuiescenceV1Get(),
        xrtNetDialOwnershipQuiescenceV1Get(),xrtNetProxyDialOwnershipQuiescenceV1Get()};
    for(size_t i=0;i<sizeof(quiet)/sizeof(*quiet);++i)if(ref.Ops==quiet[i]->Ops&&quiet[i]->Busy(ref.Data)){s->busy=true;return NULL;}
    const xnetruntimeownershipv1* runtime[]={xrtNetStreamRuntimeOwnershipV1Get(),xrtNetDialRuntimeOwnershipV1Get(),xrtNetProxyDialRuntimeOwnershipV1Get()};
    const xnetresolverlookupownershipv1* lookup[]={&ownedLookupPolicy};
    const xnetresolveownershipv1* request[]={xrtNetDialResolverPolicyV1Get()};
    const xnetdialownershipv1* transport[]={xrtNetProxyDialTransportOwnershipV1Get()};
    const xnetstreameventsownershipv1* events[]={xrtNetProxyDialStreamOwnershipV1Get()};
    const xnetproxydialownershipv1* done[]={&ownedDonePolicy};
    const xrtownershipadapterv1* a=xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtime,3,prep);
    if(!a)a=xrtNetResolverOwnershipAdapterV1(ref,lookup,1,prep);
    if(!a)a=xrtNetResolveOpOwnershipAdapterV1(ref,request,1,prep);
    if(!a)a=xrtNetDialOwnershipAdapterV1(ref,transport,1,prep);
    if(!a)a=xrtNetStreamOwnershipAdapterV2(ref,events,1,prep);
    if(!a)a=xrtNetBufPoolOwnershipAdapterV1(ref,prep);
    if(!a)a=xrtNetProxyDialOwnershipAdapterV1(ref,done,1,prep);
    if(!a)a=xrtNetProxyOwnershipAdapterV1(ref);
    if(!a)a=xrtNetAddrListOwnershipAdapterV1(ref);
    if(!a)a=xrtErrorOwnershipAdapterV1(ref);
    return a;
}
static bool ownedAdmit(xrtownershipref ref,ptr data)
{
    OwnedProxyScene* s=data;if(ref.Ops==&ownedOps)return true;
    const xrtownershippreparationv1* prep=NULL;return ownedResolve(s,ref,&prep)!=NULL;
}
static void ownedSnapshot(OwnedProxyScene* s,bool pin)
{
    xrtownershipref root={s,&ownedOps},slots[2]={root,xrtNetProxyDialOwnership(s->dial)};
    double due=__xrtWaitAfter(10000);
    for(;;){xrtownershipscope freeze={0};ownedFreeze(&freeze);s->busy=false;
        if(xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->engine))s->busy=true;
        xrtownershipsnapshot* snapshot=NULL;
        bool ok=!s->busy&&xrtOwnershipSnapshotCreate(&root,1,slots,pin?2:1,ownedAdmit,s,&snapshot);
        if(ok){
            for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){xrtownershipnode node;
                CHECK(xrtOwnershipSnapshotNode(snapshot,i,&node));
                if(node.Reachable)fprintf(stderr,"rooted node data=%p count=%zu internal=%zu\n",node.Reference.Data,node.StrongCount,node.InternalCount);
                CHECK(!node.Reachable);
            }
            xrtOwnershipSnapshotDestroy(snapshot);CHECK(xrtOwnershipScopeEnd(&freeze));++closed_graphs;return;
        }
        if(!s->busy)fprintf(stderr,"unadmitted stable Proxy graph kind=%d\n",(int)xrtErrorKind(xrtGetError()));
        CHECK(s->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(&freeze));CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
}
static void ownedWaitReady(OwnedProxyScene* s)
{
    double due=__xrtWaitAfter(10000);
    while(!xrtAtomic32Load(&s->done,XMEMORY_ACQUIRE)){
        if(__xrtWaitExpired(due)){xnetproxydialstats stats;CHECK(xrtNetProxyDialStats(s->dial,&stats));
            fprintf(stderr,"owned pending state=%d transport=%d winner=%d\n",(int)stats.State,(int)stats.Transport.State,(int)stats.Transport.HasWinner);
            CHECK(false);
        }xrtThreadYield();
    }
    due=__xrtWaitAfter(10000);
    for(;;){xrtownershipscope freeze={0};ownedFreeze(&freeze);
        const xrtownershippreparationv1* prep=NULL;
        const xrtownershipadapterv1* a=xrtNetProxyDialOwnershipAdapterV1(xrtNetProxyDialOwnership(s->dial),NULL,0,&prep);
        bool ready=a&&prep->Ready(s->dial);CHECK(xrtOwnershipScopeEnd(&freeze));
        if(ready)return;
        CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
}
static const xrtownershipadapterv1* ownedRootFreeze(OwnedProxyScene* s,xrtownershipscope* freeze,
    const xrtownershippreparationv1** prep)
{
    const xnetproxydialownershipv1* known[]={&ownedDonePolicy};double due=__xrtWaitAfter(10000);
    for(;;){ownedFreeze(freeze);
        const xrtownershipadapterv1* a=xrtNetProxyDialOwnershipAdapterV1(xrtNetProxyDialOwnership(s->dial),known,1,prep);
        if(a)return a;
        CHECK(xrtNetProxyDialOwnershipQuiescenceV1Get()->Busy(s->dial));
        CHECK(xrtOwnershipScopeEnd(freeze)&&!__xrtWaitExpired(due));xrtThreadYield();
    }
}
typedef struct OwnedSelf {xrtownershipref root;unsigned slots;} OwnedSelf;
static bool ownedSelf(xrtownershipref ref,ptr data)
{OwnedSelf* self=data;if(ref.Data==self->root.Data&&ref.Ops==self->root.Ops)++self->slots;return true;}
typedef struct OwnedGate {xatomic32 entered,proceed;} OwnedGate;
static void ownedGate(xnetworker* worker,ptr data)
{
    (void)worker;OwnedGate* gate=data;xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    double due=__xrtWaitAfter(10000);while(!xrtAtomic32Load(&gate->proceed,XMEMORY_ACQUIRE)){
        CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
}
static bool ownedCase(xnetportbackend backend,unsigned mode,int64 budget)
{
    fprintf(stderr,"owned-case backend=%d mode=%u\n",(int)backend,mode);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);OwnedProxyScene* s=xrtCalloc(1,sizeof(*s));CHECK(s);
    s->refs=1;xrtAtomic32Init(&s->done,0);xrtAtomic32Init(&s->lookupProceed,budget<0&&mode!=5?1:0);xrtAtomic32Init(&s->lookupEntered,0);
    testproxycontext peer={0};peer.Mode=mode>=2?TEST_PROXY_STALL:TEST_PROXY_SUCCESS;peer.RequireAuth=mode==1;
    peer.Stage=TEST_PROXY_DIAL_HTTP_CONNECT?TEST_PROXY_CONNECT:TEST_PROXY_GREETING;
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Backend=backend;ec.Workers=1;ec.NodeCacheBytes=0;
    xnetengine* peerEngine=xrtNetEngineCreate(&ec);CHECK(peerEngine&&xrtNetEngineStart(peerEngine));
    s->engine=xrtNetEngineCreate(&ec);CHECK(s->engine&&xrtNetEngineStart(s->engine));
    xnetlistenconfig lc;xrtNetListenConfigInit(&lc);CHECK(xrtNetAddrLoopback(&lc.Address,XNET_FAMILY_IPV4,0));
    xnetlistenerevents le={testProxyDialAccept,NULL,testProxyDialListenerClose};
    xnetstreamevents se={NULL,testProxyDialServerRead,NULL,NULL,NULL,NULL,NULL};
    xnetlistener* listener=xrtNetListen(peerEngine,&lc,&le,&se,&peer);CHECK(listener);
    xnetaddr address;CHECK(xrtNetListenerLocal(listener,&address));
    xnetresolverconfig rc;xrtNetResolverConfigInit(&rc);rc.Workers=1;rc.CacheEntries=0;
    s->resolver=xrtNetResolverCreateOwnedV1(&rc,ownedHold(s),&ownedLookupPolicy);CHECK(s->resolver);
    xnetproxyconfig pc;xrtNetProxyConfigInit(&pc);pc.Type=TEST_PROXY_DIAL_HTTP_CONNECT?XNET_PROXY_HTTP_CONNECT:XNET_PROXY_SOCKS5;
    pc.Host=XRT_STR_LITERAL("proxy.test");pc.Port=address.Port;
    if(mode==1){pc.Username=XRT_BYTES_LITERAL("user");pc.Password=XRT_BYTES_LITERAL("password");}
    s->proxy=xrtNetProxyCreate(&pc);CHECK(s->proxy);
    xnetproxydialconfig dc;xrtNetProxyDialConfigInit(&dc);dc.ReceiveLimit=512;dc.Transport.Stream.ReadLimit=1024;
    dc.Transport.Stream.ReadSize=64;
    dc.Transport.Stream.WriteLimit=3;dc.Transport.Stream.WriteHighWater=2;dc.Transport.Stream.WriteLowWater=1;
    dc.Timeout=mode==4?100:10000;
    OwnedGate gate={0};xrtAtomic32Init(&gate.entered,0);xrtAtomic32Init(&gate.proceed,0);
    if(budget>=0){CHECK(xrtNetEnginePost(s->engine,0,ownedGate,&gate));testProxyDialWait(&gate.entered,1,"constructor Worker gate");}
    xrtownershipscope mutation={0};CHECK(xrtOwnershipMutationBegin(&mutation));
    if(budget>=0)CHECK(xrtMemDebugFailAfter((size_t)budget));
    s->dial=xrtNetProxyDialOwnedV1(s->engine,s->resolver,s->proxy,"origin.test",8443,&dc,NULL,NULL,ownedHold(s),&ownedDonePolicy);
    bool hit=budget>=0&&xrtMemDebugFailTriggered();
    if(budget>=0){xrtMemDebugFailClear();xrtAtomic32Store(&s->lookupProceed,1,XMEMORY_RELEASE);
        xrtAtomic32Store(&gate.proceed,1,XMEMORY_RELEASE);++constructor_budgets;
        if(!s->dial){CHECK(hit&&xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();ownedDrop(s);++constructor_failures;}
    }else if(!s->dial)fprintf(stderr,"owned submit error kind=%d\n",(int)xrtErrorKind(xrtGetError()));
    CHECK((s->dial||budget>=0)&&xrtOwnershipScopeEnd(&mutation));
    if(!s->dial)goto cleanup;
    if(mode==5){
        testProxyDialWait(&s->lookupEntered,1,"owned blocked lookup");
        xrtownershipscope freeze={0};const xrtownershippreparationv1* prep=NULL;
        (void)ownedRootFreeze(s,&freeze,&prep);
        CHECK(xrtNetResolverOwnershipQuiescenceV1Get()->Busy(s->resolver));
        OwnedSelf self={xrtNetProxyDialOwnership(s->dial),0};size_t refs=0;
        CHECK(self.root.Ops->Count(self.root.Data,&refs)&&refs==5);
        CHECK(self.root.Ops->Trace(self.root.Data,ownedSelf,&self)&&self.slots==1);
        CHECK(xrtOwnershipScopeEnd(&freeze));CHECK(xrtNetProxyDialCancel(s->dial));
        ownedWaitReady(s);xrtAtomic32Store(&s->lookupProceed,1,XMEMORY_RELEASE);
    }
    if(mode==2||mode==3){
        testProxyDialWait(&peer.Accepted,1,"owned peer acceptance");
        double due=__xrtWaitAfter(10000);while(xrtNetProxyDialState(s->dial)!=XNET_PROXY_DIAL_HANDSHAKE){CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
        ownedSnapshot(s,false);
        xrtownershipscope freeze={0};const xrtownershippreparationv1* prep=NULL;
        (void)ownedRootFreeze(s,&freeze,&prep);prep=(const xrtownershippreparationv1*)s;
        CHECK(!xrtNetProxyDialOwnershipAdapterV1(xrtNetProxyDialOwnership(s->dial),NULL,0,&prep)&&prep==(const xrtownershippreparationv1*)s);++policy_refusals;
        xnetproxydialownershipv1 copy=ownedDonePolicy;const xnetproxydialownershipv1* wrong[]={&copy};
        CHECK(!xrtNetProxyDialOwnershipAdapterV1(xrtNetProxyDialOwnership(s->dial),wrong,1,&prep)&&prep==(const xrtownershippreparationv1*)s);++policy_refusals;
        CHECK(xrtOwnershipScopeEnd(&freeze));
        if(mode==2)CHECK(xrtNetProxyDialCancel(s->dial)&&!xrtNetProxyDialCancel(s->dial));
        else{
            prep=NULL;const xrtownershipadapterv1* a=ownedRootFreeze(s,&freeze,&prep);
            CHECK(a&&a->Hold(s->dial)&&a->Claim(s->dial,s));CHECK(xrtOwnershipScopeEnd(&freeze));
            ownedSnapshot(s,true);double prepareDue=__xrtWaitAfter(10000);
            while(!xrtAtomic32Load(&s->done,XMEMORY_ACQUIRE)){
                CHECK(prep->Prepare(s->dial,s)!=XRT_OWNERSHIP_PREPARE_FAILED);
                CHECK(!__xrtWaitExpired(prepareDue));xrtThreadYield();
            }
            ownedWaitReady(s);
            ownedFreeze(&freeze);a->Restore(s->dial,s);CHECK(xrtOwnershipScopeEnd(&freeze));a->Drop(s->dial);
        }
    }
    ownedWaitReady(s);CHECK(s->result==(mode<=1?XNET_RESULT_OK:mode==4?XNET_RESULT_TIMEOUT:XNET_RESULT_CANCELLED));
    CHECK(!xrtNetProxyDialCancel(s->dial));
    if(mode<=1)CHECK(s->stream&&peer.AuthSeen==(mode==1)&&!strcmp(peer.TargetHost,"origin.test")&&peer.TargetPort==8443);
    else CHECK(!s->stream&&s->error&&xrtErrorKind(s->error)==(mode==4?XERR_TIMEOUT:XERR_CANCELLED));
    ownedSnapshot(s,false);
cleanup:
    if(s->stream){CHECK(xrtNetStreamAbort(s->stream));testProxyDialWaitClosed(s->stream,"owned result close");}
    if(peer.Server){CHECK(xrtNetStreamAbort(peer.Server));testProxyDialWaitClosed(peer.Server,"owned peer close");}
    CHECK(xrtNetListenerClose(listener));testProxyDialWait(&peer.ListenerClosed,1,"owned peer listener close");
    xrtNetStreamDestroy(peer.Server);xrtNetListenerDestroy(listener);CHECK(xrtNetEngineDestroy(peerEngine));
    xrtNetStreamDestroy(s->stream);s->stream=NULL;xrtNetProxyDialDestroy(s->dial);s->dial=NULL;
    xrtNetProxyRelease(s->proxy);s->proxy=NULL;CHECK(xrtNetResolverDestroy(s->resolver));s->resolver=NULL;
    CHECK(xrtNetEngineDestroy(s->engine));s->engine=NULL;xrtErrorFree(s->error);s->error=NULL;
    CHECK(__xrtAtomicRefLoad(&s->refs)==1);ownedDrop(s);xrtClearError();
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);++owned_cases;return hit;
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
    CHECK(xrtMemDebugEnable(true));
    ownedLegacyBackend=backend;
    for(unsigned mode=0;mode<6;++mode)(void)ownedCase(backend,mode,-1);
    unsigned point=0;for(;point<64;++point)if(!ownedCase(backend,0,(int64)point))break;
    CHECK(point<64&&constructor_failures>0&&constructor_failures<constructor_budgets);
    /* Explicit successful legacy compatibility paths only. Never call the
     * included old main, malformed/rejected modes or protocol-security tests. */
    testProxyDialRun(TEST_PROXY_SUCCESS,false,false);testProxyDialRun(TEST_PROXY_SUCCESS,true,false);
    testMemoryDebugDrain("owned Proxy graph allocations converge");
    printf("[PASS] owned Proxy backend=%d http=%d cases=%u closed-graphs=%u policy-refusals=%u constructor-budgets=%u failures=%u literal loopback balanced\n",
        (int)backend,TEST_PROXY_DIAL_HTTP_CONNECT,owned_cases,closed_graphs,policy_refusals,constructor_budgets,constructor_failures);return 0;
}
