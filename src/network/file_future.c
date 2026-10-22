#include <xrt/net_file_future.h>
#include "../internal/xrt_internal.h"
#include "../internal/xrt_net_engine.h"
#include "../internal/xrt_net_file.h"

#if defined(XRT_FEATURE_NET_FILE_FUTURE)
typedef struct xrt_net_file_operation xrt_net_file_operation;
struct xnetfile {
    volatile int32 References;
    xmutex Lock;
    size_t Active;
    bool Publishing, Closing, Closed, ControlQueued, Cleared;
    const void* Claim;
    xfile Native;
    uint32 Flags;
    xnetengine* Engine;
    xnetworker* Worker;
    __xrt_net_engine_runtime Registration;
    __xrt_net_engine_internal Control;
    /* Each registration is one real Op reference, not a shadow directory.
     * Next is a structural link; File traces each registration exactly once. */
    xrt_net_file_operation* Operations;
    xpromise* ClosePromise;
    xfuture* CloseFuture;
    xfuture* RetiredFuture;
};
struct xrt_net_file_operation {
    volatile int32 References;
    volatile int32 Active;
    bool Publishing, Done, Cleared, Write, CancelRequested, CancelIssued;
    const void* Claim;
    xnetfile* File;
    xrt_net_file_operation* Next;
    xpromise* Promise;
    xfuturebridge Bridge;
    xnetcompletion Completion;
    __xrt_net_engine_internal Start;
    uint64 Id, Offset;
    size_t Size;
    const void* Data;
    xbuffer* Buffer;
    ptr InputOwner;
    const xnetfiledataownershipv1* InputPolicy;
};

static void __xrtNetFileOwnedLock(xnetfile* f, xrtownershipscope* s)
{ if (!xrtOwnershipMutationBegin(s) || !xrtMutexLock(&f->Lock)) abort(); }
static void __xrtNetFileOwnedUnlock(xnetfile* f, xrtownershipscope* s)
{ if (!xrtMutexUnlock(&f->Lock) || !xrtOwnershipScopeEnd(s)) abort(); }
static bool __xrtNetFileOwnedSchedule(xnetfile* f);
static void __xrtNetFileOwnedControl(xnetworker* worker, ptr data);
static void __xrtNetFileOwnedStart(xnetworker* worker, ptr data);
static void __xrtNetFileOwnedComplete(xnetworker* worker, const xnetportevent* event, ptr data);
static const xrtownershipops __xrtNetFileOwnedOps, __xrtNetFileOwnedOperationOps;

/* Preserve a structured completion failure even when allocating its detailed
 * diagnostic failed. XRT's emergency allocation error is non-allocating. */
static xerror* __xrtNetFileOwnedTakeFailure(void)
{
    xerror* error=xrtTakeError();
    if (!error) {
        error=xrtErrorCreate(XERR_INTERNAL,"net-file",XNET_ERROR_PORT_SUBMIT,
            "native file failure did not provide a diagnostic");
        if (!error) error=xrtTakeError();
    }
    return error;
}

static bool __xrtNetFileOwnedHold(const void* data)
{
    xnetfile* f=(xnetfile*)data; xrtownershipscope s={0}; bool ok;
    if (!f) return false;
    __xrtNetFileOwnedLock(f,&s); ok=!f->Cleared && xrtRefRetain(&f->References)>0;
    __xrtNetFileOwnedUnlock(f,&s); return ok;
}
XRT_API xnetfile* xrtNetFileRef(xnetfile* f)
{
    xrtownershipscope s={0}; bool ok=false;
    if (f) { __xrtNetFileOwnedLock(f,&s); ok=!f->Claim && !f->Cleared && xrtRefRetain(&f->References)>0; __xrtNetFileOwnedUnlock(f,&s); }
    if (!ok) __xrtErrorSetInvalidState();
    return ok ? f : NULL;
}
static void __xrtNetFileOwnedDrop(const void* data)
{
    xnetfile* f=(xnetfile*)data; xrtownershipscope s={0}; int32 refs;
    if (!f) return;
    __xrtNetFileOwnedLock(f,&s); refs=xrtRefRelease(&f->References);
    if (refs<0 || (!refs && (!f->Closed || f->Active || f->Publishing ||
        f->Operations || f->Engine || f->Native || f->ClosePromise || f->ControlQueued ||
        f->Registration.Data || f->RetiredFuture))) abort();
    xfuture* future=!refs ? f->CloseFuture : NULL;
    if (!refs) f->CloseFuture=NULL;
    __xrtNetFileOwnedUnlock(f,&s);
    if (!refs) { xrtFutureDestroy(future); if (!xrtMutexUnit(&f->Lock)) abort(); xrtFree(f); }
}
XRT_API void xrtNetFileDestroy(xnetfile* f) { __xrtNetFileOwnedDrop(f); }
static void __xrtNetFileOwnedRuntimeDrop(ptr data) { __xrtNetFileOwnedDrop(data); }
static bool __xrtNetFileOwnedOperationHold(const void* data)
{
    xrt_net_file_operation* op=(xrt_net_file_operation*)data; xrtownershipscope s={0}; bool ok;
    if (!op || !xrtOwnershipMutationBegin(&s)) return false;
    ok=!op->Cleared && xrtRefRetain(&op->References)>0;
    if (!xrtOwnershipScopeEnd(&s)) abort();
    return ok;
}
static void __xrtNetFileOwnedOperationDrop(const void* data)
{
    xrt_net_file_operation* op=(xrt_net_file_operation*)data; xrtownershipscope s={0};
    if (!xrtOwnershipMutationBegin(&s)) abort();
    int32 refs=xrtRefRelease(&op->References);
    if (refs<0 || (!refs && (!op->Done || op->Publishing || op->Active || op->File ||
        op->Promise || op->Buffer || op->InputOwner))) abort();
    if (!xrtOwnershipScopeEnd(&s)) abort();
    if (!refs) xrtFree(op);
}
static bool __xrtNetFileOwnedBusy(const void* data)
{
    const xnetfile* f=data;
    return f && !f->Cleared && (f->Publishing || f->Active || f->ControlQueued ||
        (f->Engine && xrtNetEngineOwnershipQuiescenceV1Get()->Busy(f->Engine)));
}
static bool __xrtNetFileOwnedCount(const void* data, size_t* count)
{
    const xnetfile* f=data; int32 refs;
    if (!f || !count || f->Cleared || __xrtNetFileOwnedBusy(f) || (refs=__xrtAtomicRefLoad(&f->References))<=0) return false;
    *count=(size_t)refs; return true;
}
static bool __xrtNetFileOwnedTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
    const xnetfile* f=data; size_t count;
    if (!visit || !__xrtNetFileOwnedCount(data,&count)) return false;
    if (f->Engine && !visit(xrtNetEngineOwnership(f->Engine),context)) return false;
    if (f->ClosePromise && !visit(xrtPromiseOwnership(f->ClosePromise),context)) return false;
    if (f->CloseFuture && !visit(xrtFutureOwnership(f->CloseFuture),context)) return false;
    for (const xrt_net_file_operation* op=f->Operations;op;op=op->Next)
        if (!visit((xrtownershipref){op,&__xrtNetFileOwnedOperationOps},context)) return false;
    return true;
}
static const xrtownershipops __xrtNetFileOwnedOps={__xrtNetFileOwnedCount,__xrtNetFileOwnedTrace};
XRT_API xrtownershipref xrtNetFileOwnership(const xnetfile* f)
{ return (xrtownershipref){f,f?&__xrtNetFileOwnedOps:NULL}; }
static bool __xrtNetFileOwnedOperationBusy(const void* data)
{
    const xrt_net_file_operation* op=data; xrtownershipref watch;
    return op && !op->Cleared && (op->Publishing || op->Active ||
        (op->File && __xrtNetFileOwnedBusy(op->File)) || (!op->Done &&
        !xrtFutureBridgeWatchOwnershipV1(&op->Bridge,&watch)));
}
static bool __xrtNetFileOwnedOperationCount(const void* data, size_t* count)
{
    const xrt_net_file_operation* op=data; int32 refs;
    if (!op || !count || op->Cleared || __xrtNetFileOwnedOperationBusy(op) ||
        (refs=__xrtAtomicRefLoad(&op->References))<=0) return false;
    *count=(size_t)refs; return true;
}
static bool __xrtNetFileOwnedOperationTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
    const xrt_net_file_operation* op=data; size_t count; xrtownershipref watch;
    if (!visit || !__xrtNetFileOwnedOperationCount(data,&count)) return false;
    if (op->File && !visit(xrtNetFileOwnership(op->File),context)) return false;
    if (op->Promise && !visit(xrtPromiseOwnership(op->Promise),context)) return false;
    if (op->InputOwner && !visit((xrtownershipref){op->InputOwner,op->InputPolicy->Ops},context)) return false;
    if (op->Done) return true;
    if (!xrtFutureBridgeWatchOwnershipV1(&op->Bridge,&watch)) return false;
    return !watch.Data || visit(watch,context);
}
static const xrtownershipops __xrtNetFileOwnedOperationOps={__xrtNetFileOwnedOperationCount,__xrtNetFileOwnedOperationTrace};
static const xfutureproducerownershipv1 __xrtNetFileOwnedProducer={sizeof(__xrtNetFileOwnedProducer),__xrtNetFileOwnedOperationDrop};
static const xfutureproducerownershipv1 __xrtNetFileOwnedCloseProducer={sizeof(__xrtNetFileOwnedCloseProducer),__xrtNetFileOwnedDrop};
static void __xrtNetFileOwnedBufferDrop(ptr value, ptr unused) { (void)unused; xrtBufferDestroy(value); }
static bool __xrtNetFileOwnedBufferTrace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ (void)context; return value && !data && visit; } /* uniquely owned, no strong child */
static const xfuturepayloadownershipv1 __xrtNetFileOwnedBytesPayload={sizeof(__xrtNetFileOwnedBytesPayload),__xrtNetFileOwnedBufferDrop,__xrtNetFileOwnedBufferTrace};
static const xnetruntimeownershipv1 __xrtNetFileOwnedRuntime={sizeof(__xrtNetFileOwnedRuntime),&__xrtNetFileOwnedOps,__xrtNetFileOwnedRuntimeDrop,NULL,0};
XRT_API const xnetruntimeownershipv1* xrtNetFileRuntimeOwnershipV1Get(void) { return &__xrtNetFileOwnedRuntime; }
XRT_API const xfutureproducerownershipv1* xrtNetFileFutureProducerPolicyV1Get(void) { return &__xrtNetFileOwnedProducer; }
XRT_API const xfutureproducerownershipv1* xrtNetFileCloseProducerPolicyV1Get(void) { return &__xrtNetFileOwnedCloseProducer; }
XRT_API const xfuturepayloadownershipv1* xrtNetFileFutureBytesPayloadPolicyV1Get(void) { return &__xrtNetFileOwnedBytesPayload; }
XRT_API const xrtownershipquiescencev1* xrtNetFileOwnershipQuiescenceV1Get(void)
{ static const xrtownershipquiescencev1 p={sizeof(p),&__xrtNetFileOwnedOps,__xrtNetFileOwnedBusy}; return &p; }
XRT_API const xrtownershipquiescencev1* xrtNetFileFutureOwnershipQuiescenceV1Get(void)
{ static const xrtownershipquiescencev1 p={sizeof(p),&__xrtNetFileOwnedOperationOps,__xrtNetFileOwnedOperationBusy}; return &p; }

/* Real command reference keeps File and its operational Engine lease alive.
 * Submission/wakeup happens outside mutation/locks. The consumer keeps the
 * queue gate until its final decision, so a racing cancel is never lost. */
static bool __xrtNetFileOwnedSchedule(xnetfile* f)
{
    xrtownershipscope s={0}; xnetworker* worker;
    __xrtNetFileOwnedLock(f,&s);
    if (f->Closed || f->ControlQueued) { __xrtNetFileOwnedUnlock(f,&s); return true; }
    worker=f->Worker; f->ControlQueued=true;
    if (xrtRefRetain(&f->References)<=0) abort();
    __xrtNetFileOwnedUnlock(f,&s);
    if (__xrtNetEnginePostInternal(worker,&f->Control,__xrtNetFileOwnedControl,f)) return true;
    __xrtNetFileOwnedLock(f,&s); f->ControlQueued=false; __xrtNetFileOwnedUnlock(f,&s); __xrtNetFileOwnedDrop(f);
    __xrtErrorSetInvalidState(); return false;
}
static void __xrtNetFileOwnedLeave(xnetfile* f)
{
    xrtownershipscope s={0}; bool closing;
    __xrtNetFileOwnedLock(f,&s); if (!f->Active) abort(); --f->Active; closing=f->Closing && !f->Closed;
    __xrtNetFileOwnedUnlock(f,&s);
    if (closing) (void)__xrtNetFileOwnedSchedule(f); /* explicit Close may retry scheduling */
}
static void __xrtNetFileOwnedCancel(ptr data)
{
    xrt_net_file_operation* op=data; xnetfile* f=op->File;
    xerror* previous=xrtTakeError(); xrtownershipscope s={0};
    /* Unwatch joins this callback before removing Op.File or its registration. */
    __xrtNetFileOwnedLock(f,&s); ++f->Active; op->CancelRequested=true; __xrtNetFileOwnedUnlock(f,&s);
    (void)__xrtNetFileOwnedSchedule(f); __xrtNetFileOwnedLeave(f);
    xrtClearError(); xrtSetErrorTake(previous);
}
static const xcancelwatchownershipv1 __xrtNetFileOwnedCancelPolicy={sizeof(__xrtNetFileOwnedCancelPolicy),__xrtNetFileOwnedCancel,__xrtNetFileOwnedOperationDrop,&__xrtNetFileOwnedOperationOps};
XRT_API const xcancelwatchownershipv1* xrtNetFileFutureCancelPolicyV1Get(void) { return &__xrtNetFileOwnedCancelPolicy; }

static void __xrtNetFileOwnedControl(xnetworker* worker, ptr data)
{
    xnetfile* f=data; xrtownershipscope s={0}; xerror* previous=xrtTakeError();
    for (;;) {
        uint64 id=0; xfile native=NULL; xnetengine* engine=NULL; xpromise* promise=NULL;
        __xrtNetFileOwnedLock(f,&s);
        for (xrt_net_file_operation* op=f->Operations;op;op=op->Next) {
            if ((f->Closing || op->CancelRequested) && op->Id && !op->CancelIssued) {
                op->CancelIssued=true; id=op->Id; break;
            }
        }
        if (!id) {
            f->ControlQueued=false;
            if (f->Closing && !f->Active && !f->Operations) {
                ++f->Active; f->Closed=true;
                native=f->Native; f->Native=NULL;
                promise=f->ClosePromise; f->ClosePromise=NULL;
                engine=f->Engine; f->Engine=NULL; f->Worker=NULL;
            }
        }
        __xrtNetFileOwnedUnlock(f,&s);
        if (id) { (void)xrtNetFileCancel(worker,id); xrtClearError(); continue; }
        if (native) {
            const xnetruntimeownershipv1* policy=NULL;
            ptr owner=__xrtNetEngineRuntimeTake(engine,&f->Registration,&policy);
            if (owner!=f || policy!=&__xrtNetFileOwnedRuntime) abort();
            bool closed=xrtClose(native); xerror* failure=closed?NULL:__xrtNetFileOwnedTakeFailure();
            /* Close completion is also the operational release barrier. A
             * caller may Stop immediately after observing this terminal. */
            policy->Release(owner); __xrtNetEngineObjectRelease(engine);
            if (closed) (void)xrtPromiseResolve(promise,NULL);
            else (void)xrtPromiseReject(promise,failure);
            xrtErrorFree(failure); xrtPromiseDestroy(promise);
            __xrtNetFileOwnedLeave(f);
        }
        break;
    }
    xrtClearError(); xrtSetErrorTake(previous); __xrtNetFileOwnedDrop(f); /* actual command slot */
}
XRT_API xfuture* xrtNetFileCloseFuture(xnetfile* f)
{
    xrtownershipscope s={0}; xfuture* future;
    if (!f) { __xrtErrorSetInvalidArgument(); return NULL; }
    __xrtNetFileOwnedLock(f,&s);
    if (f->Cleared) { __xrtNetFileOwnedUnlock(f,&s); __xrtErrorSetInvalidState(); return NULL; }
    f->Closing=true; future=xrtFutureRef(f->CloseFuture);
    __xrtNetFileOwnedUnlock(f,&s);
    if (!__xrtNetFileOwnedSchedule(f)) { xrtFutureDestroy(future); return NULL; }
    return future;
}

XRT_API xnetfile* xrtNetFileCreate(xnetengine* engine, uint32 worker,
    cstr path, const xfileoptions* options)
{
    xnetportbackendinfo info; xnetfile* f; xerror* failure;
    if (!engine || !path) { __xrtErrorSetInvalidArgument(); return NULL; }
    if (!__xrtNetEngineObjectHold(engine)) return NULL;
    if (!xrtNetEngineBackendInfo(engine,worker,&info)) goto early;
    if (!(info.Capabilities&XNET_PORT_CAP_FILE_IO)) {
        __xrtNetSetError(XERR_UNSUPPORTED,XNET_ERROR_PORT_SUBMIT,"open-file",
            "selected network worker has no native file I/O capability",0); goto early;
    }
    f=xrtCalloc(1,sizeof(*f)); if (!f) goto early;
    f->References=1; f->Publishing=true; f->Engine=engine;
    f->Worker=xrtNetEngineWorker(engine,worker);
    if (!xrtMutexInit(&f->Lock)) { xrtFree(f); goto early; }
    f->ClosePromise=xrtPromiseCreate(&f->CloseFuture,NULL);
    if (!f->ClosePromise) goto failed;
    if (!__xrtNetFileOwnedHold(f)) abort();
    if (!xrtPromiseProducerBindTakeV1(f->ClosePromise,xrtNetFileOwnership(f),&__xrtNetFileOwnedCloseProducer)) {
        __xrtNetFileOwnedDrop(f); goto failed;
    }
    if (!__xrtNetFileOwnedHold(f)) abort();
    if (!__xrtNetEngineRuntimeAdopt(engine,&f->Registration,f,&__xrtNetFileOwnedRuntime)) { __xrtNetFileOwnedDrop(f); goto failed; }
    /* All fallible control storage/directory preparation precedes OS open. */
    f->Native=xrtNetFileOpen(path,options);
    if (!f->Native) goto failed;
    f->Flags=xrtFileFlags(f->Native);
    xrtownershipscope s={0}; __xrtNetFileOwnedLock(f,&s); f->Publishing=false; __xrtNetFileOwnedUnlock(f,&s);
    return f;
failed:
    failure=xrtTakeError();
    if (f->Registration.Data) {
        const xnetruntimeownershipv1* policy=NULL;
        ptr owner=__xrtNetEngineRuntimeTake(engine,&f->Registration,&policy);
        if (owner!=f || policy!=&__xrtNetFileOwnedRuntime) abort();
        __xrtNetFileOwnedDrop(owner);
    }
    { xrtownershipscope scope={0}; __xrtNetFileOwnedLock(f,&scope);
      f->Publishing=false; f->Closed=true; f->Engine=NULL; f->Worker=NULL;
      xpromise* promise=f->ClosePromise; f->ClosePromise=NULL;
      __xrtNetFileOwnedUnlock(f,&scope);
      if (promise) { (void)xrtPromiseCancel(promise); xrtPromiseDestroy(promise); } }
    __xrtNetFileOwnedDrop(f); __xrtNetEngineObjectRelease(engine);
    xrtClearError(); xrtSetErrorTake(failure); return NULL;
early:
    failure=xrtTakeError(); __xrtNetEngineObjectRelease(engine);
    xrtClearError(); xrtSetErrorTake(failure); return NULL;
}

/* Exactly one callback/registration owns retirement. A requested cancellation
 * never frees an accepted buffer or File; only the real completion does that. */
static void __xrtNetFileOwnedFinish(xrt_net_file_operation* op, xnetresult result, size_t bytes, xerror* error)
{
    xnetfile* f=op->File; xrtownershipscope s={0};
    __xrtNetFileOwnedLock(f,&s); ++f->Active; ++op->Active; __xrtNetFileOwnedUnlock(f,&s);
    xrtFutureBridgeUnwatch(&op->Bridge); /* waits outside mutation/locks */
    __xrtNetFileOwnedLock(f,&s);
    xrt_net_file_operation** slot=&f->Operations;
    while (*slot && *slot!=op) slot=&(*slot)->Next;
    if (*slot!=op || op->Done) abort();
    *slot=op->Next; op->Next=NULL;
    xpromise* promise=op->Promise; op->Promise=NULL;
    xbuffer* buffer=op->Buffer; op->Buffer=NULL;
    ptr input=op->InputOwner; op->InputOwner=NULL;
    op->File=NULL; op->Done=true; op->Data=NULL; op->Id=0;
    __xrtNetFileOwnedUnlock(f,&s);
    if (input) op->InputPolicy->Drop(input);
    if (result==XNET_RESULT_CANCELLED) (void)xrtPromiseCancel(promise);
    else if (result!=XNET_RESULT_OK) (void)xrtPromiseReject(promise,error);
    else if (op->Write) (void)xrtPromiseResolve(promise,(ptr)(uintptr_t)bytes);
    else {
        buffer->Size=bytes;
        if (xrtPromiseResolveOwnedPolicyV1(promise,buffer,&__xrtNetFileOwnedBytesPayload)) buffer=NULL;
    }
    xrtBufferDestroy(buffer); xrtPromiseDestroy(promise);
    __xrtNetFileOwnedLock(f,&s); --op->Active; __xrtNetFileOwnedUnlock(f,&s);
    __xrtNetFileOwnedLeave(f); __xrtNetFileOwnedDrop(f); /* Op.File reference transferred to this tail */
    __xrtNetFileOwnedOperationDrop(op); /* actual File registration reference */
}
static void __xrtNetFileOwnedComplete(xnetworker* worker, const xnetportevent* event, ptr data)
{
    xrt_net_file_operation* op=data; xerror* previous=xrtTakeError(); xerror* error=NULL;
    if (worker!=op->File->Worker || event->Id!=op->Id || event->Bytes>op->Size) abort();
    if (event->Result!=XNET_RESULT_OK && event->Result!=XNET_RESULT_CANCELLED) {
        xerrordesc desc={0}; desc.Kind=XERR_IO; desc.Domain="net-file";
        desc.Code=op->Write?XFILE_ERROR_WRITE:XFILE_ERROR_READ;
        desc.SystemCode=event->SystemCode; desc.Operation=op->Write?"write-file":"read-file";
        desc.Message="native file completion failed"; error=xrtErrorBuild(&desc);
        if (!error) error=__xrtNetFileOwnedTakeFailure();
    }
    __xrtNetFileOwnedFinish(op,event->Result,event->Bytes,error);
    xrtErrorFree(error); xrtClearError(); xrtSetErrorTake(previous);
}
static void __xrtNetFileOwnedStart(xnetworker* worker, ptr data)
{
    xrt_net_file_operation* op=data; xnetfile* f=op->File;
    xrtownershipscope s={0}; xerror* previous=xrtTakeError(); bool cancelled;
    if (!xrtFutureBridgeWait(&op->Bridge)) abort();
    /* Start consumes the independent active File lease prepared by Submit. */
    __xrtNetFileOwnedLock(f,&s); cancelled=f->Closing || op->CancelRequested; __xrtNetFileOwnedUnlock(f,&s);
    uint64 id=0; xerror* error=NULL; bool failed=false;
    if (!cancelled && op->Size) {
        id=__xrtNetFileSubmit(worker,f->Native,op->Offset,(void*)op->Data,
            op->Size,&op->Completion,op->Write);
        if (!id) { failed=true; error=__xrtNetFileOwnedTakeFailure(); }
    }
    __xrtNetFileOwnedLock(f,&s); op->Id=id; cancelled=cancelled || f->Closing || op->CancelRequested; __xrtNetFileOwnedUnlock(f,&s);
    if (id) {
        if (cancelled) (void)__xrtNetFileOwnedSchedule(f);
    } else __xrtNetFileOwnedFinish(op,cancelled?XNET_RESULT_CANCELLED:failed?XNET_RESULT_ERROR:XNET_RESULT_OK,0,error);
    __xrtNetFileOwnedLeave(f); /* caller still owns f through this separate active lease below */
    __xrtNetFileOwnedDrop(f);
    xrtErrorFree(error); xrtClearError(); xrtSetErrorTake(previous);
}

static xfuture* __xrtNetFileOwnedSubmit(xnetfile* f, uint64 offset, size_t size, const void* data,
    ptr owner, const xnetfiledataownershipv1* policy, bool write, xcancel* cancel)
{
    xrtownershipscope s={0}; xfuture* future=NULL; xerror* error;
    xrt_net_file_operation* op=NULL; bool linked=false, ready=false;
    if (!f || (write && size && !data) || (policy && (!owner || policy->size!=sizeof(*policy) ||
        !policy->Drop || !policy->Ops || !policy->Ops->Count || !policy->Ops->Trace))) {
        __xrtErrorSetInvalidArgument(); return NULL;
    }
    if (size>(size_t)INT_MAX || offset>(uint64)INT64_MAX ||
        (size && (uint64)(size-1u)>(uint64)INT64_MAX-offset)) {
        __xrtErrorSetSizeOverflow(); return NULL;
    }
    __xrtNetFileOwnedLock(f,&s);
    if (f->Closing || f->Claim || f->Cleared || !(f->Flags&(write?XFILE_WRITE:XFILE_READ))) {
        __xrtNetFileOwnedUnlock(f,&s); __xrtErrorSetInvalidState(); return NULL;
    }
    ++f->Active; if (xrtRefRetain(&f->References)<=0) abort();
    __xrtNetFileOwnedUnlock(f,&s);
    op=xrtCalloc(1,sizeof(*op)); if (!op) goto failed;
    op->References=1; op->Active=1; op->Publishing=true;
    op->Write=write; op->Size=size; op->Offset=offset; op->InputPolicy=policy;
    /* op gets a distinct File reference; the API/start active tail keeps the
     * existing entry lease independently until its last access. */
    if (!__xrtNetFileOwnedHold(f)) abort();
    op->File=f;
    if (!write || !policy) {
        op->Buffer=xrtBufferCreate(); if (!op->Buffer) goto failed;
        if (size && !xrtBufferReserve(op->Buffer,size)) goto failed;
        if (write && size) memcpy(op->Buffer->Data,data,size);
        op->Data=op->Buffer->Data;
    } else op->Data=data;
    op->Promise=xrtPromiseCreate(&future,cancel); if (!op->Promise) goto failed;
    if (!xrtFutureBridgeInit(&op->Bridge,op->Promise)) abort();
    ready=true;
    if (!__xrtNetFileOwnedOperationHold(op)) abort();
    if (!xrtPromiseProducerBindTakeV1(op->Promise,(xrtownershipref){op,&__xrtNetFileOwnedOperationOps},&__xrtNetFileOwnedProducer)) {
        __xrtNetFileOwnedOperationDrop(op); goto failed;
    }
    if (!__xrtNetFileOwnedOperationHold(op)) abort();
    if (!xrtFutureBridgeWatchOwnedV1(&op->Bridge,op,&__xrtNetFileOwnedCancelPolicy)) {
        __xrtNetFileOwnedOperationDrop(op); goto failed;
    }
    xrtNetCompletionInit(&op->Completion,__xrtNetFileOwnedComplete,op);
    __xrtNetFileOwnedLock(f,&s);
    if (f->Closing || f->Claim) { __xrtNetFileOwnedUnlock(f,&s); __xrtErrorSetInvalidState(); goto failed; }
    op->Next=f->Operations; f->Operations=op; linked=true;
    if (!__xrtNetFileOwnedOperationHold(op)) abort(); /* real File registration */
    __xrtNetFileOwnedUnlock(f,&s);
    if (!__xrtNetEnginePostInternal(f->Worker,&op->Start,__xrtNetFileOwnedStart,op)) {
        __xrtErrorSetInvalidState(); goto failed;
    }
    __xrtNetFileOwnedLock(f,&s); op->InputOwner=owner; op->Publishing=false; --op->Active;
    __xrtNetFileOwnedUnlock(f,&s);
    if (!xrtFutureBridgeReady(&op->Bridge)) abort();
    __xrtNetFileOwnedOperationDrop(op); /* constructor; active entry lease transferred to Start */
    return future;
failed:
    error=xrtTakeError();
    if (op) {
        if (ready) { if (!xrtFutureBridgeFail(&op->Bridge)) abort(); xrtFutureBridgeUnwatch(&op->Bridge); }
        __xrtNetFileOwnedLock(f,&s);
        if (linked) {
            xrt_net_file_operation** slot=&f->Operations;
            while (*slot && *slot!=op) slot=&(*slot)->Next;
            if (*slot!=op) abort();
            *slot=op->Next; op->Next=NULL;
        }
        xpromise* promise=op->Promise; op->Promise=NULL;
        xbuffer* buffer=op->Buffer; op->Buffer=NULL;
        xnetfile* held=op->File; op->File=NULL;
        op->Publishing=false; op->Done=true; --op->Active;
        __xrtNetFileOwnedUnlock(f,&s);
        if (promise) { (void)xrtPromiseCancel(promise); xrtPromiseDestroy(promise); }
        xrtFutureDestroy(future); xrtBufferDestroy(buffer); __xrtNetFileOwnedDrop(held);
        if (linked) __xrtNetFileOwnedOperationDrop(op);
        __xrtNetFileOwnedOperationDrop(op);
    }
    __xrtNetFileOwnedLeave(f); __xrtNetFileOwnedDrop(f);
    xrtClearError(); xrtSetErrorTake(error); return NULL;
}
XRT_API xfuture* xrtNetFileReadFuture(xnetfile* f, uint64 offset, size_t size, xcancel* cancel)
{ return __xrtNetFileOwnedSubmit(f,offset,size,NULL,NULL,NULL,false,cancel); }
XRT_API xfuture* xrtNetFileWriteFuture(xnetfile* f, uint64 offset, xbytesview input, xcancel* cancel)
{ return __xrtNetFileOwnedSubmit(f,offset,input.Size,input.Data,NULL,NULL,true,cancel); }
XRT_API xfuture* xrtNetFileWriteRefPolicyV1(xnetfile* f, uint64 offset, xbytesview input,
    ptr owner, const xnetfiledataownershipv1* policy, xcancel* cancel)
{
    if (!policy) { __xrtErrorSetInvalidArgument(); return NULL; }
    return __xrtNetFileOwnedSubmit(f,offset,input.Size,input.Data,owner,policy,true,cancel);
}

/* Native semantic duties settle through explicit Close/real I/O, never an
 * adapter's synthetic cancellation or inferred reference-count correction. */
static bool __xrtNetFileOwnedClaim(const void* data, const void* token)
{
    xnetfile* f=(xnetfile*)data; size_t count;
    if (!token || !__xrtNetFileOwnedCount(f,&count) || (f->Claim && f->Claim!=token)) return false;
    f->Claim=token; return true;
}
static void __xrtNetFileOwnedRestore(const void* data, const void* token)
{ xnetfile* f=(xnetfile*)data; if (!token || f->Claim!=token || f->Cleared) abort(); f->Claim=NULL; }
static bool __xrtNetFileOwnedReady(const void* data)
{ const xnetfile* f=data; size_t count; return __xrtNetFileOwnedCount(f,&count) && f->Closed; }
static xrtownershipprepareresult __xrtNetFileOwnedPrepare(const void* data, const void* token)
{
    const xnetfile* f=data; xrtownershipscope s={0}; bool ready;
    if (!xrtOwnershipFreezeTryBegin(&s)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || f->Claim!=token || f->Cleared) abort();
    ready=__xrtNetFileOwnedReady(f);
    if (!xrtOwnershipScopeEnd(&s)) abort();
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetFileOwnedClear(const void* data, const void* token)
{
    xnetfile* f=(xnetfile*)data;
    if (!token || f->Claim!=token || !__xrtNetFileOwnedReady(f)) abort();
    f->Cleared=true; f->RetiredFuture=f->CloseFuture; f->CloseFuture=NULL;
}
static bool __xrtNetFileOwnedFinishClear(const void* data, const void* token)
{
    xnetfile* f=(xnetfile*)data; xrtownershipscope s={0};
    __xrtNetFileOwnedLock(f,&s); if (!token || f->Claim!=token || !f->Cleared) abort();
    xfuture* future=f->RetiredFuture; f->RetiredFuture=NULL; __xrtNetFileOwnedUnlock(f,&s);
    xrtFutureDestroy(future); return true;
}
XRT_API const xrtownershipadapterv1* xrtNetFileOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 a={sizeof(a),__xrtNetFileOwnedHold,__xrtNetFileOwnedDrop,__xrtNetFileOwnedClaim,__xrtNetFileOwnedRestore,NULL,__xrtNetFileOwnedClear,__xrtNetFileOwnedFinishClear};
    static const xrtownershippreparationv1 p={sizeof(p),&a,__xrtNetFileOwnedReady,__xrtNetFileOwnedPrepare}; size_t count;
    if (!preparation || ref.Ops!=&__xrtNetFileOwnedOps || !__xrtNetFileOwnedCount(ref.Data,&count)) return NULL;
    *preparation=&p; return &a;
}
static bool __xrtNetFileOwnedOperationClaim(const void* data, const void* token)
{
    xrt_net_file_operation* op=(xrt_net_file_operation*)data; size_t count;
    if (!token || !__xrtNetFileOwnedOperationCount(op,&count) || (op->Claim && op->Claim!=token)) return false;
    op->Claim=token; return true;
}
static void __xrtNetFileOwnedOperationRestore(const void* data, const void* token)
{ xrt_net_file_operation* op=(xrt_net_file_operation*)data; if (!token || op->Claim!=token || op->Cleared) abort(); op->Claim=NULL; }
static bool __xrtNetFileOwnedOperationReady(const void* data)
{ const xrt_net_file_operation* op=data; size_t count; return __xrtNetFileOwnedOperationCount(op,&count) && op->Done; }
static xrtownershipprepareresult __xrtNetFileOwnedOperationPrepare(const void* data, const void* token)
{
    const xrt_net_file_operation* op=data; xrtownershipscope s={0}; bool ready;
    if (!xrtOwnershipFreezeTryBegin(&s)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || op->Claim!=token || op->Cleared) abort();
    ready=__xrtNetFileOwnedOperationReady(op);
    if (!xrtOwnershipScopeEnd(&s)) abort();
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetFileOwnedOperationClear(const void* data, const void* token)
{ xrt_net_file_operation* op=(xrt_net_file_operation*)data; if (!token || op->Claim!=token || !__xrtNetFileOwnedOperationReady(op)) abort(); op->Cleared=true; }
XRT_API const xrtownershipadapterv1* xrtNetFileFutureOwnershipAdapterV1(xrtownershipref ref,
    const xnetfiledataownershipv1* const* policies, size_t count,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 a={sizeof(a),__xrtNetFileOwnedOperationHold,__xrtNetFileOwnedOperationDrop,
        __xrtNetFileOwnedOperationClaim,__xrtNetFileOwnedOperationRestore,NULL,__xrtNetFileOwnedOperationClear,NULL};
    static const xrtownershippreparationv1 p={sizeof(p),&a,__xrtNetFileOwnedOperationReady,__xrtNetFileOwnedOperationPrepare};
    const xrt_net_file_operation* op; size_t refs;
    if (!preparation || ref.Ops!=&__xrtNetFileOwnedOperationOps || !ref.Data || (count && !policies)) return NULL;
    op=ref.Data;
    if (op->InputOwner) {
        bool known=false; for (size_t i=0;i<count;++i) if (policies[i] && policies[i]==op->InputPolicy) { known=true; break; }
        if (!known) return NULL;
    }
    if (!__xrtNetFileOwnedOperationCount(op,&refs)) return NULL;
    *preparation=&p; return &a;
}
#endif
