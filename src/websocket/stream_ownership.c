#include "../internal/xrt_websocket_stream.h"
#include "../internal/xrt_tcp.h"
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
#include "../internal/xrt_tls_stream.h"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_STREAM)

bool __xrtWsStreamOwnershipEnter(xwsstream* s,bool submit)
{
    xrtownershipscope mutation={0};
    if(!s||!xrtOwnershipMutationBegin(&mutation))return false;
    bool ok=false;
    uint32 active=xrtAtomic32Load(&s->OwnershipActive,XMEMORY_ACQUIRE);
    if(!(xrtAtomic32Load(&s->OwnershipState,XMEMORY_ACQUIRE)&__XRT_WS_OWN_CLEARED)&&
       (!submit||!xrtAtomicPtrLoad(&s->OwnershipClaim,XMEMORY_ACQUIRE))&&
       active<UINT32_MAX&&xrtRefRetain(&s->References)>=0){
        xrtAtomic32Store(&s->OwnershipActive,active+1,XMEMORY_RELEASE);ok=true;
    }
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    if(!ok)__xrtErrorSetInvalidState();
    return ok;
}
void __xrtWsStreamOwnershipLeave(xwsstream* s)
{
    xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!xrtAtomic32FetchSub(&s->OwnershipActive,1,XMEMORY_ACQ_REL))abort();
    bool last=xrtRefRelease(&s->References)==0;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    if(last)__xrtWsStreamFree(s);
}
static bool __xrtWsOwnedBusy(const void* data)
{
    const xwsstream* s=data;
    return s&&(xrtAtomic32Load(&s->OwnershipActive,XMEMORY_ACQUIRE)||
        xrtAtomic32Load(&s->DrivePosted,XMEMORY_ACQUIRE)||
        (s->OwnershipEngine&&xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->OwnershipEngine)));
}
static bool __xrtWsOwnedCount(const void* data,size_t* result)
{
    const xwsstream* s=data;
    if(!s||!result||__xrtWsOwnedBusy(s)||!s->OwnershipPolicy||!s->OwnershipDataHeld||!s->Data||
       !s->OwnershipStorageHeld||!s->OwnershipEngine||s->Reading||
       (xrtAtomic32Load(&s->OwnershipState,XMEMORY_ACQUIRE)&__XRT_WS_OWN_CLEARED))return false;
    if(s->RuntimeHeld){
        if(!s->OwnershipEngineHeld||s->RuntimeOwner.Data!=s||!s->RuntimeOwner.Previous||
           s->RuntimeOwner.Engine!=s->OwnershipEngine||s->RuntimeOwner.Policy!=xrtWsStreamRuntimeOwnershipV1Get())return false;
    }else if(s->OwnershipEngineHeld||s->RuntimeOwner.Data||s->RuntimeOwner.Previous||
        !(xrtAtomic32Load(&s->OwnershipState,XMEMORY_ACQUIRE)&__XRT_WS_OWN_RUNTIME_RELEASED))return false;
    int32 references=__xrtAtomicRefLoad(&s->References);
    if(references<=0)return false;
    *result=(size_t)references;return true;
}
static bool __xrtWsOwnedTrace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const xwsstream* s=data;size_t references;
    if(!visit||!__xrtWsOwnedCount(s,&references)||
       !visit(xrtNetEngineOwnership(s->OwnershipEngine),context)||
       (s->OwnershipEngineHeld&&!visit(xrtNetEngineOwnership(s->OwnershipEngine),context))||
       !visit((xrtownershipref){s->Data,s->OwnershipPolicy->Ops},context)||
       !xrtNetBufOwnershipTrace(&s->Initial,visit,context))return false;
    ptr transport=xrtAtomicPtrLoad(&s->Transport,XMEMORY_ACQUIRE);
    if(transport){
        #if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
        if(s->TransportKind==__XRT_WS_TRANSPORT_TLS){
            if(!visit(xrtTlsStreamOwnership(transport),context))return false;
        }else
        #endif
        if(!visit(xrtNetStreamOwnership(transport),context))return false;
    }
    xerror* error=xrtAtomicPtrLoad(&s->Error,XMEMORY_ACQUIRE);
    return !error||visit(xrtErrorOwnership(error),context);
}
const xrtownershipops __xrtWsStreamOwnershipOps={__xrtWsOwnedCount,__xrtWsOwnedTrace};
XRT_API xrtownershipref xrtWsStreamOwnership(const xwsstream* s)
{return (xrtownershipref){s,s&&s->OwnershipPolicy?&__xrtWsStreamOwnershipOps:NULL};}
XRT_API const xrtownershipquiescencev1* xrtWsStreamOwnershipQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 policy={sizeof(policy),&__xrtWsStreamOwnershipOps,__xrtWsOwnedBusy};
    return &policy;
}
static void __xrtWsOwnedRuntimeDrop(ptr data){xrtWsStreamDestroy(data);}
XRT_API const xnetruntimeownershipv1* xrtWsStreamRuntimeOwnershipV1Get(void)
{
    static const xnettimerproc timers[]={__xrtWsStreamCloseTimer};
    static const xnetruntimeownershipv1 policy={sizeof(policy),&__xrtWsStreamOwnershipOps,__xrtWsOwnedRuntimeDrop,timers,1};
    return &policy;
}
void __xrtWsOwnedRuntimeRelease(xwsstream* s)
{
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    const xnetruntimeownershipv1* policy=NULL;
    if(!s->RuntimeHeld||!s->OwnershipEngineHeld||
       __xrtNetEngineRuntimeTake(s->OwnershipEngine,&s->RuntimeOwner,&policy)!=s||
       policy!=xrtWsStreamRuntimeOwnershipV1Get())abort();
    s->RuntimeHeld=false;s->OwnershipEngineHeld=false;
    xrtAtomic32Store(&s->OwnershipState,__XRT_WS_OWN_RUNTIME_RELEASED,XMEMORY_RELEASE);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    __xrtNetEngineObjectRelease(s->OwnershipEngine);
    xrtWsStreamDestroy(s);
}
static bool __xrtWsOwnedHold(const void* data)
{
    xwsstream* s=(xwsstream*)data;xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))return false;
    bool ok=!(xrtAtomic32Load(&s->OwnershipState,XMEMORY_ACQUIRE)&__XRT_WS_OWN_CLEARED)&&xrtRefRetain(&s->References)>=0;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return ok;
}
static void __xrtWsOwnedDrop(const void* data){xrtWsStreamDestroy((xwsstream*)data);}
static bool __xrtWsOwnedClaim(const void* data,const void* token)
{
    xwsstream* s=(xwsstream*)data;size_t references;
    ptr old=xrtAtomicPtrLoad(&s->OwnershipClaim,XMEMORY_ACQUIRE);
    if(!token||(old&&old!=token)||!__xrtWsOwnedCount(s,&references))return false;
    xrtAtomicPtrStore(&s->OwnershipClaim,(ptr)token,XMEMORY_RELEASE);return true;
}
static void __xrtWsOwnedRestore(const void* data,const void* token)
{
    xwsstream* s=(xwsstream*)data;
    if(!token||xrtAtomicPtrLoad(&s->OwnershipClaim,XMEMORY_ACQUIRE)!=token||
       (xrtAtomic32Load(&s->OwnershipState,XMEMORY_ACQUIRE)&__XRT_WS_OWN_CLEARED))abort();
    xrtAtomicPtrStore(&s->OwnershipClaim,NULL,XMEMORY_RELEASE);
}
static bool __xrtWsOwnedStopped(const xwsstream* s)
{
    return !__xrtWsOwnedBusy(s)&&!s->RuntimeHeld&&!s->OwnershipEngineHeld&&
        !s->Reading&&!s->CloseTimer&&!s->DriveCommand.Task&&
        xrtWsStreamState(s)==XWS_STREAM_CLOSED&&s->CloseEmitted&&
        !xrtAtomicPtrLoad(&s->Transport,XMEMORY_ACQUIRE)&&!s->OutputHead;
}
static bool __xrtWsOwnedReady(const void* data)
{const xwsstream* s=data;return s&&s->OwnershipBuffersSettled&&__xrtWsOwnedStopped(s);}
static xrtownershipprepareresult __xrtWsOwnedPrepare(const void* data,const void* token)
{
    xwsstream* s=(xwsstream*)data;xrtownershipscope freeze={0};
    if(!xrtOwnershipFreezeTryBegin(&freeze))return XRT_OWNERSHIP_PREPARE_BUSY;
    if(!token||xrtAtomicPtrLoad(&s->OwnershipClaim,XMEMORY_ACQUIRE)!=token||
       (xrtAtomic32Load(&s->OwnershipState,XMEMORY_ACQUIRE)&__XRT_WS_OWN_CLEARED))abort();
    if(__xrtWsOwnedReady(s)){
        if(!xrtOwnershipScopeEnd(&freeze))abort();
        return XRT_OWNERSHIP_PREPARE_READY;
    }
    if(__xrtWsOwnedStopped(s)){
        xnetbuf initial=s->Initial;s->Initial=(xnetbuf){0};
        xrtAtomic32FetchAdd(&s->OwnershipActive,1,XMEMORY_ACQ_REL);
        if(!xrtOwnershipScopeEnd(&freeze))abort();
        xrtNetBufClear(&initial);
        xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
        s->OwnershipBuffersSettled=true;
        if(xrtAtomic32FetchSub(&s->OwnershipActive,1,XMEMORY_ACQ_REL)!=1)abort();
        if(!xrtOwnershipScopeEnd(&mutation))abort();
        return XRT_OWNERSHIP_PREPARE_READY;
    }
    if(__xrtWsOwnedBusy(s)){
        if(!xrtOwnershipScopeEnd(&freeze))abort();
        return XRT_OWNERSHIP_PREPARE_BUSY;
    }
    if(s->OwnershipPolicy->Retire)s->OwnershipPolicy->Retire(s->Data);
    xnetstream* tcp=NULL;ptr transport=xrtAtomicPtrLoad(&s->Transport,XMEMORY_ACQUIRE);
    #if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
    if(transport&&s->TransportKind==__XRT_WS_TRANSPORT_TLS)
        tcp=xrtAtomicPtrLoad(&((xtlsstream*)transport)->Transport,XMEMORY_ACQUIRE);
    else
    #endif
    tcp=transport;
    /* Borrow the child's real runtime stop slot, including when that child
     * is already claimed by this graph. No new owning edge or user callback
     * is published by a BUSY preparation. */
    bool ok=!tcp||!tcp->RuntimeHeld||__xrtNetStreamOwnershipStop(tcp);
    if(!xrtOwnershipScopeEnd(&freeze))abort();
    return ok?XRT_OWNERSHIP_PREPARE_BUSY:XRT_OWNERSHIP_PREPARE_FAILED;
}
static void __xrtWsOwnedClear(const void* data,const void* token)
{
    xwsstream* s=(xwsstream*)data;
    if(!token||xrtAtomicPtrLoad(&s->OwnershipClaim,XMEMORY_ACQUIRE)!=token||!__xrtWsOwnedReady(s)||
       (xrtAtomic32Load(&s->OwnershipState,XMEMORY_ACQUIRE)&__XRT_WS_OWN_CLEARED))abort();
    xrtAtomic32Store(&s->OwnershipState,__XRT_WS_OWN_RUNTIME_RELEASED|__XRT_WS_OWN_CLEARED,XMEMORY_RELEASE);
}
static bool __xrtWsOwnedFinish(const void* data,const void* token)
{
    xwsstream* s=(xwsstream*)data;xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))return false;
    if(!token||xrtAtomicPtrLoad(&s->OwnershipClaim,XMEMORY_ACQUIRE)!=token||
       !(xrtAtomic32Load(&s->OwnershipState,XMEMORY_ACQUIRE)&__XRT_WS_OWN_CLEARED))abort();
    if(s->OwnershipFinishing){if(!xrtOwnershipScopeEnd(&mutation))abort();return false;}
    if(s->OwnershipFinished){
        xerror* failure=s->OwnershipFinishError;
        if(!xrtOwnershipScopeEnd(&mutation))abort();
        if(failure&&!xrtGetError())xrtSetError(failure);
        return !failure;
    }
    s->OwnershipFinishing=true;
    ptr owner=s->OwnershipDataHeld?s->Data:NULL;s->Data=NULL;s->OwnershipDataHeld=false;
    xnetengine* engine=s->OwnershipEngine;bool storage=s->OwnershipStorageHeld;
    s->OwnershipEngine=NULL;s->OwnershipStorageHeld=false;
    xerror* error=xrtAtomicPtrExchange(&s->Error,NULL,XMEMORY_ACQ_REL);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xerror* prior=xrtTakeError();
    xrtErrorFree(error);
    if(owner)s->OwnershipPolicy->Drop(owner);
    if(storage)__xrtNetEngineStorageRelease(engine);
    xerror* failure=xrtTakeError();
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    s->OwnershipFinishError=failure;s->OwnershipFinishing=false;s->OwnershipFinished=true;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    if(prior)xrtSetErrorTake(prior);else if(failure)xrtSetError(failure);
    return !failure;
}
XRT_API const xrtownershipadapterv1* xrtWsStreamOwnershipAdapterV1(xrtownershipref ref,
    const xwsstreamownershipv1* const* policies,size_t policyCount,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),__xrtWsOwnedHold,__xrtWsOwnedDrop,__xrtWsOwnedClaim,__xrtWsOwnedRestore,NULL,__xrtWsOwnedClear,__xrtWsOwnedFinish};
    static const xrtownershippreparationv1 preparing={sizeof(preparing),&adapter,__xrtWsOwnedReady,__xrtWsOwnedPrepare};
    if(!preparation||ref.Ops!=&__xrtWsStreamOwnershipOps||!ref.Data||
       (policyCount&&!policies)||__xrtWsOwnedBusy(ref.Data))return NULL;
    const xwsstream* s=ref.Data;bool known=false;
    for(size_t i=0;i<policyCount;++i)if(policies[i]==s->OwnershipPolicy){known=true;break;}
    if(!known)return NULL;
    size_t references;if(!__xrtWsOwnedCount(s,&references))return NULL;
    *preparation=&preparing;return &adapter;
}

/* Rejecting a prepared, unpublished attachment drains its queued native task
 * without any application callbacks or Data credit. Real __xrtWsOwnedReady work keeps
 * its own reference until the Worker actually retires it. */
static void __xrtWsOwnedDiscard(xwsstream* s)
{
    if(s->RuntimeHeld)__xrtWsOwnedRuntimeRelease(s);
    memset(&s->Events,0,sizeof(s->Events));s->Data=NULL;
    xrtAtomic32Store(&s->State,XWS_STREAM_CLOSED,XMEMORY_RELEASE);
    xrtAtomicPtrStore(&s->Transport,NULL,XMEMORY_RELEASE);
    xrtWsStreamDestroy(s);xrtWsStreamDestroy(s);
}
static xwsstream* __xrtWsOwnedAttach(ptr transport,__xrt_ws_transport kind,xnetbuf* initial,
    const xwsstreamconfig* config,ptr data,const xwsstreamownershipv1* policy)
{
    if(!transport||!data||!policy||policy->size!=sizeof(*policy)||!policy->Drop||
       !policy->Ops||!policy->Ops->Count||!policy->Ops->Trace){__xrtErrorSetInvalidArgument();return NULL;}
    xnetworker* worker=NULL;const xnetbuf* buffered=NULL;
    #if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
    if(kind==__XRT_WS_TRANSPORT_TLS){worker=xrtNetStreamWorker(xrtTlsStreamTransport(transport));}else
    #endif
    {worker=xrtNetStreamWorker(transport);}
    if(!worker||!xrtNetWorkerIsCurrent(worker)){__xrtErrorSetInvalidState();return NULL;}
    #if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
    if(kind==__XRT_WS_TRANSPORT_TLS){
        if(xrtTlsStreamState(transport)!=XTLS_STREAM_OPEN){__xrtErrorSetInvalidState();return NULL;}
        buffered=xrtTlsStreamBuffer(transport);
    }else
    #endif
    {
        if(xrtNetStreamState(transport)!=XNET_STREAM_OPEN){__xrtErrorSetInvalidState();return NULL;}
        buffered=xrtNetStreamBuffer(transport);
    }
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))return NULL;
    xnetbuf draft={0};xwsstream* s=NULL;bool moved=false;
    if(initial){if(!__xrtWsStreamInitialTake(initial,buffered,&draft))goto fail;moved=true;}
    s=__xrtWsStreamCreate(worker,kind,transport,config,&policy->Events,data);
    if(!s)goto fail;
    s->OwnershipPolicy=policy;s->OwnershipEngine=xrtNetWorkerEngine(worker);
    if(!__xrtNetEngineObjectHold(s->OwnershipEngine))goto fail;
    s->OwnershipEngineHeld=true;
    if(!__xrtNetEngineStorageRetain(s->OwnershipEngine))goto fail;
    s->OwnershipStorageHeld=true;
    if(!xrtWsStreamRef(s))goto fail;
    if(!__xrtNetEngineRuntimeAdopt(s->OwnershipEngine,&s->RuntimeOwner,s,xrtWsStreamRuntimeOwnershipV1Get())){
        xrtWsStreamDestroy(s);goto fail;
    }
    s->RuntimeHeld=true;
    if(!__xrtWsStreamAttachBudget(s,transport)){
        (void)__xrtWsStreamReject(XERR_RANGE,XWS_STREAM_ERROR_CONFIG,
            "__xrtWsOwnedAttach-owned-websocket","WebSocket send budget is incompatible with transport",xrtGetError());goto fail;
    }
    if(!__xrtWsStreamDrivePrepare(s))goto fail;
    bool bound;
    #if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
    if(kind==__XRT_WS_TRANSPORT_TLS)bound=xrtTlsStreamBindEventsOwnedV1(transport,s,xrtWsStreamTlsEventsOwnershipV1Get());else
    #endif
    bound=xrtNetStreamSetEventsOwnedV1(transport,s,xrtWsStreamTcpEventsOwnershipV1Get());
    if(!bound)goto fail;
    s->OwnershipDataHeld=true;s->Initial=draft;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return s;
fail:
    if(s)__xrtWsOwnedDiscard(s);
    if(moved&&!xrtNetBufMove(initial,&draft))abort();
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return NULL;
}
XRT_API xwsstream* xrtWsStreamAttachOwnedV1(xnetstream* transport,xnetbuf* initial,
    const xwsstreamconfig* config,ptr data,const xwsstreamownershipv1* policy)
{return __xrtWsOwnedAttach(transport,__XRT_WS_TRANSPORT_TCP,initial,config,data,policy);}
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
XRT_API xwsstream* xrtWsStreamAttachTlsOwnedV1(xtlsstream* transport,xnetbuf* initial,
    const xwsstreamconfig* config,ptr data,const xwsstreamownershipv1* policy)
{return __xrtWsOwnedAttach(transport,__XRT_WS_TRANSPORT_TLS,initial,config,data,policy);}
#endif
#endif
