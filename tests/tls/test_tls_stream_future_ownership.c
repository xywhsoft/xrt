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
    xtlsclientconfig clientConfig;xtlssession* pendingSession;xnetstream* pendingTcp;unsigned factoryMode;
    xatomic32 opened,closed,drops,frees;
    bool failCleanup;
    xrtownershipref refs[64];const xrtownershipadapterv1* adapters[64];
    const xrtownershippreparationv1* preparations[64];size_t nodes;
    xfuture* futures[4];size_t futureCount;
};
static unsigned cases,refusals,finishedFailures,pendingNodes,ownedResults,budgets,oomFailures,attachRollbacks;
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
static void attachClient(xnetworker* worker,ptr data)
{
    (void)worker;Scene* s=data;
    xnetstream* tcp=s->pendingTcp;
    CHECK(!tcp->ReadWaiters);
    CHECK(xrtTlsStreamAttachOwnedV1(tcp,s->pendingSession,NULL,s->clientOwner,&streamPolicy,&s->client));
    s->pendingSession=NULL;s->pendingTcp=NULL;
}
static void openedTcp(xnetstream* tcp,ptr data)
{
    Scene* s=data;
    if(s->factoryMode==2){
        CHECK(xrtTlsStreamClientOwnedV1(tcp,&s->clientConfig,NULL,s->clientOwner,&streamPolicy,&s->client));
        return;
    }
    s->pendingSession=xrtTlsClientCreate(&s->clientConfig,NULL);CHECK(s->pendingSession);
    xfuture* read=xrtNetStreamWaitAsync(tcp,XNET_STREAM_WAIT_READ);CHECK(read);
    xnetbufpool* pools[]={s->pendingSession->Feed.Pool,s->pendingSession->Send.Pool,s->pendingSession->Plain.Pool,s->pendingSession->Scratch.Pool};
    xtlsstream* output=(xtlsstream*)s;
    CHECK(!xrtTlsStreamAttachOwnedV1(tcp,s->pendingSession,NULL,s->clientOwner,&streamPolicy,&output)&&!output);
    CHECK(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    CHECK(__xrtAtomicRefLoad(&s->clientOwner->refs)==1&&!tcp->EventOwner);
    CHECK(s->pendingSession->Feed.Pool==pools[0]&&s->pendingSession->Send.Pool==pools[1]&&
        s->pendingSession->Plain.Pool==pools[2]&&s->pendingSession->Scratch.Pool==pools[3]);attachRollbacks++;
    CHECK(xrtFutureCancel(read));xrtFutureDestroy(read);
    /* Keep a distinct typed TCP slot while delayed handoff is unpublished. */
    s->pendingTcp=tcp;
    CHECK(xrtNetEnginePost(s->engine,0,attachClient,s));
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
    const xfutureproducerownershipv1* producers[]={xrtTlsStreamFutureProducerPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[]={xrtTlsStreamFutureCancelPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[]={xrtTlsStreamFutureBytesPayloadPolicyV1Get()};
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=producers;admission.ProducerPolicyCount=1;
    admission.CancelWatchPolicies=cancels;admission.CancelWatchPolicyCount=1;
    admission.PayloadPolicies=payloads;admission.PayloadPolicyCount=1;
    const xrtownershipadapterv1* async=xrtTlsStreamFutureOwnershipAdapterV1(ref,prepare);
    if(!async)async=xrtFutureOwnershipAdapterV4(ref,&admission,prepare);
    if(!async)async=xrtCancelOwnershipAdapterV2(ref,cancels,1,prepare);
    if(!async)async=xrtCancelWatchOwnershipAdapterV1(ref,cancels,1,prepare);
    if(async)return async;
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
                xrtownershipref creators[7];
                creators[0]=xrtNetEngineOwnership(s->engine);
                creators[1]=xrtTlsStreamOwnership(s->client);
                creators[2]=xrtTlsStreamOwnership(s->server);
                size_t roots=3+s->futureCount;
                for(size_t i=0;i<s->futureCount;i++)creators[3+i]=xrtFutureOwnership(s->futures[i]);
                xrtownershipref slots[71];
                for(size_t i=0;i<roots;i++)slots[i]=creators[i];
                if(pinned)for(size_t i=0;i<s->nodes;i++)slots[roots+i]=s->refs[i];
                xrtownershipref anchors[71];
                size_t anchorCount=pinned?roots+s->nodes:roots;
                for(size_t i=0;i<anchorCount;i++)anchors[i]=slots[i];
                xrtownershipsnapshot* graph=NULL;
                if(xrtOwnershipSnapshotCreate(anchors,anchorCount,slots,anchorCount,admit,s,&graph)){
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
static void idleClient(Scene* s,xrtownershipscope* freeze)
{
    double due=__xrtWaitAfter(10000);
    for(;;){if(xrtOwnershipFreezeTryBegin(freeze)){
        if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->engine)&&!xrtTlsStreamAsyncCount(s->client))return;
        CHECK(xrtOwnershipScopeEnd(freeze));}
        CHECK(!__xrtWaitExpired(due));xrtThreadYield();}
}
static void constructionBudgets(Scene* s)
{
    xrtownershipscope freeze={0};idleClient(s,&freeze);size_t baseline=0;
    xrtownershipref ref=xrtTlsStreamOwnership(s->client);CHECK(ref.Ops->Count(ref.Data,&baseline));CHECK(xrtOwnershipScopeEnd(&freeze));
    unsigned success=0,failed=0;
    for(size_t budget=0;budget<24&&success<2;budget++){
        CHECK(xrtMemDebugFailAfter(budget));xfuture* f=xrtTlsStreamWaitAsync(s->client,XTLS_STREAM_WAIT_END);xrtMemDebugFailClear();budgets++;
        if(!f){CHECK(xrtGetError());xrtClearError();failed++;oomFailures++;success=0;}
        else {CHECK(xrtFutureCancel(f));xrtFutureDestroy(f);success++;}
        idleClient(s,&freeze);size_t after=0;
        CHECK(ref.Ops->Count(ref.Data,&after)&&after==baseline&&__xrtAtomicRefLoad(&s->clientOwner->refs)==1);
        CHECK(!xrtAtomic32Load(&s->drops,XMEMORY_ACQUIRE));CHECK(xrtOwnershipScopeEnd(&freeze));
    }
    CHECK(failed&&success==2);
}
static void lifecycle(xnetportbackend backend,bool failure,unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};s.failCleanup=failure;s.factoryMode=mode;
    xrtAtomic32Init(&s.opened,0);xrtAtomic32Init(&s.closed,0);xrtAtomic32Init(&s.drops,0);xrtAtomic32Init(&s.frees,0);
    xtlscontext* context=testTlsServerContext();xtlsidentity* identity=testTlsServerIdentity();CHECK(context&&identity);
    xtlsverifierconfig v;xrtTlsVerifierConfigInit(&v);v.Verify=testTlsServerAccept;xtlsverifier* verifier=xrtTlsVerifierCreate(&v);CHECK(verifier);
    xrtTlsClientConfigInit(&s.clientConfig);s.clientConfig.Context=context;s.clientConfig.Verifier=verifier;s.clientConfig.ServerName=XRT_STR_LITERAL("example.com");
    xrtTlsServerConfigInit(&s.serverConfig);s.serverConfig.Context=context;s.serverConfig.Identity=identity;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.NodeCacheBytes=4096;config.Backend=backend;
    s.engine=xrtNetEngineCreate(&config);CHECK(s.engine&&xrtNetEngineStart(s.engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    const xnetlistenerevents events={acceptStream,NULL,NULL};s.listener=xrtNetListen(s.engine,&listen,&events,NULL,&s);CHECK(s.listener);
    xnetaddr address;CHECK(xrtNetListenerLocal(s.listener,&address));s.clientOwner=owner(&s,false);
    if(!mode){s.client=xrtTlsStreamConnectOwnedV1(s.engine,&address,0,&listen.Stream,&s.clientConfig,NULL,s.clientOwner,&streamPolicy);CHECK(s.client);}
    else {const xnetstreamevents event={openedTcp,NULL,NULL,NULL,NULL,NULL,NULL};
        CHECK(xrtNetStreamConnect(s.engine,&address,0,&listen.Stream,&event,&s));}
    double due=__xrtWaitAfter(10000);
    while(xrtAtomic32Load(&s.opened,XMEMORY_ACQUIRE)!=2||xrtTlsStreamAvailable(s.client)!=3){
        if(__xrtWaitExpired(due)){fprintf(stderr,"handshake open=%u client=%d server=%d avail=%zu closed=%u error=%d\n",
            xrtAtomic32Load(&s.opened,XMEMORY_ACQUIRE),(int)xrtTlsStreamState(s.client),s.server?(int)xrtTlsStreamState(s.server):-1,
            xrtTlsStreamAvailable(s.client),xrtAtomic32Load(&s.closed,XMEMORY_ACQUIRE),xrtGetError()?(int)xrtErrorKind(xrtGetError()):0);CHECK(false);}
        xrtThreadYield();}
    CHECK(xrtNetListenerClose(s.listener));xrtNetListenerDestroy(s.listener);s.listener=NULL;
    constructionBudgets(&s);
    s.futures[s.futureCount++]=xrtTlsStreamRecvAsync(s.client,3);CHECK(s.futures[0]);
    CHECK(xrtFutureWaitFor(s.futures[0],5000)==XWAIT_OK&&xrtFutureState(s.futures[0])==XFUTURE_RESOLVED);
    xnetbytes* result=xrtFutureValue(s.futures[0]);CHECK(result);
    xbytesview view=xrtNetBytesView(result);CHECK(view.Size==3&&!memcmp(view.Data,"a\0b",3));ownedResults++;
    const xnetspan pieces[]={{(cbytes)"x",1},{(cbytes)"\0",1},{(cbytes)"y",1}};
    xfuture* sent=xrtTlsStreamSendVecAsync(s.client,pieces,3);CHECK(sent);
    s.futures[s.futureCount++]=xrtTlsStreamRecvAsync(s.server,3);CHECK(s.futures[1]);
    CHECK(xrtFutureWaitFor(sent,5000)==XWAIT_OK&&xrtFutureState(sent)==XFUTURE_RESOLVED);xrtFutureDestroy(sent);
    CHECK(xrtFutureWaitFor(s.futures[1],5000)==XWAIT_OK&&xrtFutureState(s.futures[1])==XFUTURE_RESOLVED);
    result=xrtFutureValue(s.futures[1]);CHECK(result);view=xrtNetBytesView(result);
    CHECK(view.Size==3&&!memcmp(view.Data,"x\0y",3));ownedResults++;
    s.futures[s.futureCount++]=xrtTlsStreamWaitAsync(s.client,XTLS_STREAM_WAIT_END);
    s.futures[s.futureCount++]=xrtTlsStreamRecvAsync(s.server,3);
    CHECK(s.futures[2]&&s.futures[3]);
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            bool ready=!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s.engine)&&s.engine->RuntimeOwnerCount==4;
            CHECK(xrtOwnershipScopeEnd(&freeze));if(ready)break;
        }CHECK(!__xrtWaitExpired(due));xrtThreadYield();
    }
    xrtownershipscope freeze={0};snapshot(&s,false,&freeze);
    CHECK(s.nodes>=8);
    unsigned pending=0;
    for(size_t i=0;i<s.nodes;i++)if(s.refs[i].Ops==xrtTlsStreamFutureOwnershipQuiescenceV1Get()->Ops){
        size_t count=0;CHECK(s.refs[i].Ops->Count(s.refs[i].Data,&count)&&count==3);pending++;
    }
    CHECK(pending==2);pendingNodes+=pending;
    const xfutureproducerownershipv1* realProducer=xrtTlsStreamFutureProducerPolicyV1Get();
    xfutureproducerownershipv1 fakeProducer=*realProducer;const xfutureproducerownershipv1* unknown[]={&fakeProducer};
    xrtownershipref futureRef=xrtFutureOwnership(s.futures[2]);
    CHECK(!xrtFutureOwnershipAdapterV2(futureRef,NULL,0,unknown,1));refusals++;
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&s;const xrtownershippreparationv1* prepare=sentinel;
    xtlsstreamownershipv1 copied=streamPolicy;const xtlsstreamownershipv1* wrong[]={&copied};
    CHECK(!xrtTlsStreamOwnershipAdapterV1(xrtTlsStreamOwnership(s.client),wrong,1,&prepare)&&prepare==sentinel);refusals++;
    for(size_t i=0;i<s.nodes;i++){
        CHECK(s.adapters[i]->Hold(s.refs[i].Data)&&s.adapters[i]->Claim(s.refs[i].Data,&s));
        if(s.preparations[i]||s.refs[i].Ops==&ownerOps)
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
    for(size_t i=0;i<s.futureCount;i++)xrtFutureDestroy(s.futures[i]);
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
    CHECK(xrtMemDebugEnable(true));for(unsigned mode=0;mode<3;mode++){lifecycle(backend,false,mode);lifecycle(backend,true,mode);}CHECK(xrtMemDebugReset());
    printf("TLS Future ownership: %u cyclic graphs; %u pending activations; %u owned results; %u refusals; %u persistent cleanup failures; %u budgets/%u failures; %u attach rollbacks; balanced\n",cases,pendingNodes,ownedResults,refusals,finishedFailures,budgets,oomFailures,attachRollbacks);return 0;
}
