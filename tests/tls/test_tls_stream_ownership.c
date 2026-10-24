/* Normal fixed-key, self-created loopback and real frozen ownership graphs.
 * No external addresses, DNS, malformed records, probing or security tests. */
#ifdef TLS_STREAM_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../fixtures/tls_server.h"
#include <xrt/detail/wait.h>
#ifndef TLS_STREAM_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_tls_stream.h"
#endif
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%s:%u: ",__FILE__,(unsigned)__LINE__);testRequire(false,#c);}}while(0)
typedef struct Scene Scene;
typedef struct Owner {
    volatile int32 refs;Scene* scene;xtlsstream* stream;xtlsstream* retired;
    const void* claim;bool cleared,finished,server;
} Owner;
struct Scene {
    xnetengine* engine;xnetlistener* listener;xtlsstream* client;xtlsstream* server;
    xtlsserverconfig serverConfig;Owner* clientOwner;Owner* serverOwner;
    xatomic32 opened,closed,drops,frees;
    bool failCleanup;
    xrtownershipref refs[64];const xrtownershipadapterv1* adapters[64];
    const xrtownershippreparationv1* preparations[64];size_t nodes;
};
static unsigned cases,refusals,finishedFailures;
static bool ownerCount(const void* data,size_t* count)
{const Owner* o=data;if(!o||!count||o->cleared)return false;int32 n=__xrtAtomicRefLoad(&o->refs);if(n<=0)return false;*count=(size_t)n;return true;}
static bool ownerTrace(const void* data,xrtownershipvisitor visit,ptr context)
{const Owner* o=data;size_t n;return visit&&ownerCount(data,&n)&&(!o->stream||visit(xrtTlsStreamOwnership(o->stream),context));}
static const xrtownershipops ownerOps={ownerCount,ownerTrace};
static bool ownerHold(const void* data)
{Owner* o=(Owner*)data;xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))return false;bool ok=!o->cleared&&xrtRefRetain(&o->refs)>=0;CHECK(xrtOwnershipScopeEnd(&m));return ok;}
static void ownerDrop(const void* data)
{
    Owner* o=(Owner*)data;int32 n=xrtOwnershipRefRelease(&o->refs);CHECK(n>=0);
    if(!n){CHECK(!o->stream&&!o->retired);xrtAtomic32FetchAdd(&o->scene->frees,1,XMEMORY_ACQ_REL);xrtFree(o);}
}
static bool ownerClaim(const void* data,const void* token)
{Owner* o=(Owner*)data;size_t n;if(!token||(o->claim&&o->claim!=token)||!ownerCount(o,&n))return false;o->claim=token;return true;}
static void ownerRestore(const void* data,const void* token)
{Owner* o=(Owner*)data;CHECK(token&&o->claim==token&&!o->cleared);o->claim=NULL;}
static void ownerClear(const void* data,const void* token)
{Owner* o=(Owner*)data;CHECK(token&&o->claim==token&&!o->cleared);o->retired=o->stream;o->stream=NULL;o->cleared=true;}
static bool ownerFinish(const void* data,const void* token)
{Owner* o=(Owner*)data;CHECK(token&&o->claim==token&&o->cleared);if(!o->finished){xtlsstream* retired=o->retired;o->retired=NULL;o->finished=true;xrtTlsStreamDestroy(retired);}return true;}
static const xrtownershipadapterv1 ownerAdapter={sizeof(ownerAdapter),ownerHold,ownerDrop,ownerClaim,ownerRestore,NULL,ownerClear,ownerFinish};
static void nativeOwnerDrop(const void* data)
{
    Owner* o=(Owner*)data;Scene* s=o->scene;bool fail=s->failCleanup&&!o->server;
    xrtAtomic32FetchAdd(&s->drops,1,XMEMORY_ACQ_REL);ownerDrop(data);
    if(fail)xrtSetErrorKind(XERR_STATE);
}
static void opened(xtlsstream* stream,ptr data)
{
    Owner* o=data;xrtownershipscope m={0};CHECK(xrtOwnershipMutationBegin(&m));
    CHECK(!o->stream);o->stream=xrtTlsStreamRef(stream);CHECK(o->stream);CHECK(xrtOwnershipScopeEnd(&m));
    if(o->server){size_t written=0;CHECK(xrtTlsStreamSend(stream,"a\0b",3,&written)==XTLS_OK&&written==3);}
    xrtAtomic32FetchAdd(&o->scene->opened,1,XMEMORY_ACQ_REL);
}
static void closed(xtlsstream* stream,xnetresult result,const xerror* error,ptr data)
{(void)stream;(void)result;(void)error;Owner* o=data;xrtAtomic32FetchAdd(&o->scene->closed,1,XMEMORY_ACQ_REL);}
static const xtlsstreamownershipv1 streamPolicy={sizeof(streamPolicy),{opened,NULL,NULL,NULL,NULL,closed,NULL},nativeOwnerDrop,&ownerOps};
static Owner* owner(Scene* s,bool server)
{Owner* o=xrtCalloc(1,sizeof(*o));CHECK(o);o->refs=1;o->scene=s;o->server=server;return o;}
static bool acceptStream(xnetlistener* listener,xnetstream* stream,ptr data)
{
    (void)listener;Scene* s=data;s->serverOwner=owner(s,true);
    bool ok=xrtTlsStreamAcceptOwnedV1(stream,&s->serverConfig,NULL,s->serverOwner,&streamPolicy,&s->server);
    if(!ok)ownerDrop(s->serverOwner);
    return ok;
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,const xrtownershippreparationv1** prepare)
{
    const xnetruntimeownershipv1* runtimes[]={xrtNetStreamRuntimeOwnershipV1Get(),xrtNetListenerRuntimeOwnershipV1Get(),xrtTlsStreamRuntimeOwnershipV1Get()};
    const xtlsstreamownershipv1* owners[]={&streamPolicy};
    const xnetstreameventsownershipv1* events[]={xrtTlsStreamTransportOwnershipV1Get()};
    *prepare=NULL;
    if(ref.Ops==xrtNetEngineOwnershipQuiescenceV1Get()->Ops)
        return xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtimes,3,prepare);
    if(ref.Ops==xrtNetStreamRuntimeOwnershipV1Get()->Ops)return xrtNetStreamOwnershipAdapterV2(ref,events,1,prepare);
    if(ref.Ops==xrtTlsStreamOwnershipQuiescenceV1Get()->Ops)return xrtTlsStreamOwnershipAdapterV1(ref,owners,1,prepare);
    if(ref.Ops==xrtNetBufPoolOwnershipQuiescenceV1Get()->Ops)return xrtNetBufPoolOwnershipAdapterV1(ref,prepare);
    if(ref.Ops==&ownerOps)return &ownerAdapter;
    const xrtownershipadapterv1* adapter=xrtNetBytesOwnershipAdapterV1(ref);
    return adapter?adapter:xrtErrorOwnershipAdapterV1(ref);
}
static bool admit(xrtownershipref ref,ptr data)
{(void)data;const xrtownershippreparationv1* prepare=NULL;return resolve(ref,&prepare)!=NULL;}
static void snapshot(Scene* s,bool pinned,xrtownershipscope* freeze)
{
    double due=__xrtWaitAfter(10000);
    for(;;){
        if(xrtOwnershipFreezeTryBegin(freeze)){
            if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->engine)){
                xrtownershipref creators[3];
                creators[0]=xrtNetEngineOwnership(s->engine);
                creators[1]=xrtTlsStreamOwnership(s->client);
                creators[2]=xrtTlsStreamOwnership(s->server);
                xrtownershipref slots[67];
                for(size_t i=0;i<3;i++)slots[i]=creators[i];
                if(pinned)for(size_t i=0;i<s->nodes;i++)slots[3+i]=s->refs[i];
                xrtownershipsnapshot* graph=NULL;
                if(xrtOwnershipSnapshotCreate(creators,3,slots,pinned?3+s->nodes:3,admit,s,&graph)){
                    size_t discovered=xrtOwnershipSnapshotNodeCount(graph);
                    for(size_t i=0;i<discovered;i++){
                        xrtownershipnode node;CHECK(xrtOwnershipSnapshotNode(graph,i,&node)&&!node.Reachable);
                        size_t at=0;while(at<s->nodes&&(s->refs[at].Data!=node.Reference.Data||s->refs[at].Ops!=node.Reference.Ops))at++;
                        if(at==s->nodes){
                            CHECK(s->nodes<64);s->refs[at]=node.Reference;s->adapters[at]=resolve(node.Reference,&s->preparations[at]);CHECK(s->adapters[at]);s->nodes++;
                            if(pinned)CHECK(s->adapters[at]->Hold(node.Reference.Data)&&s->adapters[at]->Claim(node.Reference.Data,s));
                        }
                    }
                    xrtOwnershipSnapshotDestroy(graph);return;
                }xrtClearError();
            }CHECK(xrtOwnershipScopeEnd(freeze));
        }
        CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);CHECK(!xrtGetError());
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void lifecycle(xnetportbackend backend,bool failure)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};s.failCleanup=failure;
    xrtAtomic32Init(&s.opened,0);xrtAtomic32Init(&s.closed,0);xrtAtomic32Init(&s.drops,0);xrtAtomic32Init(&s.frees,0);
    xtlscontext* context=testTlsServerContext();xtlsidentity* identity=testTlsServerIdentity();CHECK(context&&identity);
    xtlsverifierconfig v;xrtTlsVerifierConfigInit(&v);v.Verify=testTlsServerAccept;xtlsverifier* verifier=xrtTlsVerifierCreate(&v);CHECK(verifier);
    xtlsclientconfig client;xrtTlsClientConfigInit(&client);client.Context=context;client.Verifier=verifier;client.ServerName=XRT_STR_LITERAL("example.com");
    xrtTlsServerConfigInit(&s.serverConfig);s.serverConfig.Context=context;s.serverConfig.Identity=identity;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.NodeCacheBytes=0;config.Backend=backend;
    s.engine=xrtNetEngineCreate(&config);CHECK(s.engine&&xrtNetEngineStart(s.engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    const xnetlistenerevents events={acceptStream,NULL,NULL};s.listener=xrtNetListen(s.engine,&listen,&events,NULL,&s);CHECK(s.listener);
    xnetaddr address;CHECK(xrtNetListenerLocal(s.listener,&address));s.clientOwner=owner(&s,false);
    s.client=xrtTlsStreamConnectOwnedV1(s.engine,&address,0,&listen.Stream,&client,NULL,s.clientOwner,&streamPolicy);CHECK(s.client);
    double due=__xrtWaitAfter(10000);
    while(xrtAtomic32Load(&s.opened,XMEMORY_ACQUIRE)!=2||xrtTlsStreamAvailable(s.client)!=3){
        if(__xrtWaitExpired(due)){fprintf(stderr,"handshake open=%u client=%d server=%d avail=%zu closed=%u error=%d\n",
            xrtAtomic32Load(&s.opened,XMEMORY_ACQUIRE),(int)xrtTlsStreamState(s.client),s.server?(int)xrtTlsStreamState(s.server):-1,
            xrtTlsStreamAvailable(s.client),xrtAtomic32Load(&s.closed,XMEMORY_ACQUIRE),xrtGetError()?(int)xrtErrorKind(xrtGetError()):0);CHECK(false);}
        xrtThreadYield();}
    CHECK(xrtNetListenerClose(s.listener));xrtNetListenerDestroy(s.listener);s.listener=NULL;
    /* A legacy async node is not a certified participant yet. Refuse the
     * graph, then exercise ordinary cancellation before testing retirement. */
    xfuture* pending=xrtTlsStreamWaitAsync(s.client,XTLS_STREAM_WAIT_END);CHECK(pending);
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            bool idle=!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s.engine);
            if(idle){size_t untouched=SIZE_MAX;xrtownershipref ref=xrtTlsStreamOwnership(s.client);
                CHECK(!ref.Ops->Count(ref.Data,&untouched)&&untouched==SIZE_MAX);refusals++;}
            CHECK(xrtOwnershipScopeEnd(&freeze));if(idle)break;
        }CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
    CHECK(xrtFutureCancel(pending));
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            bool idle=!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s.engine)&&!xrtTlsStreamAsyncCount(s.client);
            CHECK(xrtOwnershipScopeEnd(&freeze));if(idle)break;
        }CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
    xrtFutureDestroy(pending);
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            bool ready=!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s.engine)&&s.engine->RuntimeOwnerCount==4;
            CHECK(xrtOwnershipScopeEnd(&freeze));if(ready)break;
        }CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
    xrtownershipscope freeze={0};snapshot(&s,false,&freeze);
    CHECK(s.nodes>=8);
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&s;const xrtownershippreparationv1* prepare=sentinel;
    xtlsstreamownershipv1 copied=streamPolicy;const xtlsstreamownershipv1* wrong[]={&copied};
    CHECK(!xrtTlsStreamOwnershipAdapterV1(xrtTlsStreamOwnership(s.client),wrong,1,&prepare)&&prepare==sentinel);refusals++;
    for(size_t i=0;i<s.nodes;i++){
        CHECK(s.adapters[i]->Hold(s.refs[i].Data)&&s.adapters[i]->Claim(s.refs[i].Data,&s));
        CHECK(!s.adapters[i]->Claim(s.refs[i].Data,&before));
        s.adapters[i]->Restore(s.refs[i].Data,&s);CHECK(s.adapters[i]->Claim(s.refs[i].Data,&s));
    }
    CHECK(xrtOwnershipScopeEnd(&freeze));
    CHECK(!xrtTlsStreamClose(s.client));xrtClearError();CHECK(!xrtTlsStreamWaitAsync(s.client,XTLS_STREAM_WAIT_READ));xrtClearError();refusals+=2;
    due=__xrtWaitAfter(10000);
    for(;;){bool all=true;
        for(size_t step=0;step<s.nodes;step++){
            size_t i=(step+1)%s.nodes;if(!s.preparations[i])continue;
            snapshot(&s,true,&freeze);bool ready=s.preparations[i]->Ready(s.refs[i].Data);CHECK(xrtOwnershipScopeEnd(&freeze));
            if(ready)continue;
            xrtownershipprepareresult state=s.preparations[i]->Prepare(s.refs[i].Data,&s);CHECK(state!=XRT_OWNERSHIP_PREPARE_FAILED);all=false;
        }if(all)break;
        if(__xrtWaitExpired(due)){
            snapshot(&s,true,&freeze);
            for(size_t i=0;i<s.nodes;i++)if(s.preparations[i])fprintf(stderr,"prepare node=%zu tls=%d tcp=%d engine=%d pool=%d ready=%d\n",i,
                s.refs[i].Ops==xrtTlsStreamOwnershipQuiescenceV1Get()->Ops,s.refs[i].Ops==xrtNetStreamRuntimeOwnershipV1Get()->Ops,
                s.refs[i].Ops==xrtNetEngineOwnershipQuiescenceV1Get()->Ops,s.refs[i].Ops==xrtNetBufPoolOwnershipQuiescenceV1Get()->Ops,
                s.preparations[i]->Ready(s.refs[i].Data));
            fprintf(stderr,"closed=%u runtime=%d/%d timers=%llu,%llu/%llu,%llu engineState=%d\n",xrtAtomic32Load(&s.closed,XMEMORY_ACQUIRE),s.client->RuntimeHeld,s.server->RuntimeHeld,
                (unsigned long long)s.client->HandshakeTimer,(unsigned long long)s.client->CloseTimer,(unsigned long long)s.server->HandshakeTimer,(unsigned long long)s.server->CloseTimer,(int)xrtNetEngineState(s.engine));
            CHECK(xrtOwnershipScopeEnd(&freeze));CHECK(false);
        }xrtThreadYield();
    }
    snapshot(&s,true,&freeze);
    for(size_t i=0;i<s.nodes;i++){if(s.preparations[i])CHECK(s.preparations[i]->Ready(s.refs[i].Data));s.adapters[i]->Clear(s.refs[i].Data,&s);}
    CHECK(xrtOwnershipScopeEnd(&freeze));
    bool sawFailure=false;
    for(size_t i=0;i<s.nodes;i++)if(s.adapters[i]->Finish){
        bool expect=failure&&s.refs[i].Data==s.client;
        if(expect)xrtSetErrorKind(XERR_ARGUMENT);
        bool ok=s.adapters[i]->Finish(s.refs[i].Data,&s);
        if(expect){CHECK(!ok&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();CHECK(!s.adapters[i]->Finish(s.refs[i].Data,&s)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();sawFailure=true;}
        else CHECK(ok&&s.adapters[i]->Finish(s.refs[i].Data,&s));
    }
    CHECK(sawFailure==failure);if(sawFailure)finishedFailures++;
    CHECK(xrtAtomic32Load(&s.closed,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&s.drops,XMEMORY_ACQUIRE)==2);
    xrtTlsStreamDestroy(s.client);xrtTlsStreamDestroy(s.server);CHECK(xrtNetEngineTryDestroy(s.engine)==XNET_RETIRE_READY);
    for(size_t i=0;i<s.nodes;i++)s.adapters[i]->Drop(s.refs[i].Data);
    CHECK(xrtAtomic32Load(&s.frees,XMEMORY_ACQUIRE)==2);
    xrtTlsVerifierRelease(verifier);xrtTlsIdentityRelease(identity);xrtTlsContextRelease(context);
    balanced(before);cases++;
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
    CHECK(xrtMemDebugEnable(true));lifecycle(backend,false);lifecycle(backend,true);CHECK(xrtMemDebugReset());
    printf("TLS stream ownership: %u actual cyclic graphs; %u refusals; %u persistent cleanup failures; balanced\n",cases,refusals,finishedFailures);return 0;
}
