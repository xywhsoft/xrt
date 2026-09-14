#include "../internal/xrt_future.h"

#if defined(XRT_FEATURE_FUTURE_COMBINE)
typedef enum xrt_future_combine_mode {
    XRT_FUTURE_COMBINE_ANY = 1,
    XRT_FUTURE_COMBINE_ALL = 2,
    XRT_FUTURE_COMBINE_RACE = 3
} xrt_future_combine_mode;
typedef struct xrt_future_combine xrt_future_combine;
/* One allocation still contains every callback address. Each registration
 * owns the GROUP itself, projected from this borrowed embedded item. */
typedef struct xrt_future_combine_item {
    xfuturewatch Watch;
    xrt_future_combine* Group;
    size_t Index;
    bool Owned;
} xrt_future_combine_item;
struct xrt_future_combine {
    volatile int32 RefCount;
    xmutex Lock;
    xrt_future_combine_mode Mode;
    size_t Count, Remaining, Active;
    bool Publishing, Completed, Cleared, MapAccepted, MapLive;
    const void* Claim;
    xpromise* Promise;
    xcancelwatch* Watch;
    xfuture** Sources;
    xrt_future_combine_item* Items;
    xfuturepick Pick;
    xfutureall All;
    xfutureallmapproc AllMap;
    xfuturepickmapproc PickMap;
    ptr MapData, MapDestroyData;
    xfuturefreeproc MapDestroy;
    xfutureownershiptrace MapTrace;
    const xfuturecombineownershipv1* MapPolicy;
};
static void __xrtFutureCombineBegin(xrt_future_combine* group, xrtownershipscope* scope)
{
    if (!xrtOwnershipMutationBegin(scope)) abort();
    if (!xrtMutexLock(&group->Lock)) abort();
}
static void __xrtFutureCombineEnd(xrt_future_combine* group, xrtownershipscope* scope)
{
    if (!xrtMutexUnlock(&group->Lock) || !xrtOwnershipScopeEnd(scope)) abort();
}
static bool __xrtFutureCombineOwnershipCount(const void* data, size_t* count)
{
    const xrt_future_combine* group = data; int32 references;
    if (!group || !count || group->Publishing || group->Active || group->Cleared) return false;
    references = __xrtAtomicRefLoad(&group->RefCount);
    if (references <= 0) return false;
    for (size_t i = 0; i < group->Count; ++i)
        if (__xrtFutureWatchImpl((xfuturewatch*)&group->Items[i].Watch)->Waiter.Calling) return false;
    *count = (size_t)references; return true;
}
static bool __xrtFutureCombineOwnershipTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
    const xrt_future_combine* group = data; size_t count;
    if (!visit || !__xrtFutureCombineOwnershipCount(data, &count)) return false;
    if (group->Promise && !visit(xrtPromiseOwnership(group->Promise), context)) return false;
    if (group->Watch && !visit(xrtCancelWatchOwnership(group->Watch), context)) return false;
    for (size_t i = 0; i < group->Count; ++i)
        if (!visit(xrtFutureOwnership(group->Sources[i]), context)) return false;
    if (!group->MapLive) return true;
    return group->MapPolicy ? visit((xrtownershipref){group->MapData, group->MapPolicy->Ops}, context) :
        group->MapTrace(group->MapData, group->MapDestroyData, visit, context);
}
static const xrtownershipops __xrtFutureCombineOwnershipOps = {
    __xrtFutureCombineOwnershipCount, __xrtFutureCombineOwnershipTrace
};
static xrtownershipref __xrtFutureCombineOwnership(const xrt_future_combine* group)
{ return (xrtownershipref){group, group ? &__xrtFutureCombineOwnershipOps : NULL}; }
static xrtownershipref __xrtFutureCombineItemOwner(const void* data)
{ return __xrtFutureCombineOwnership(((const xrt_future_combine_item*)data)->Group); }
static void __xrtFutureCombineDropContext(xrt_future_combine* group, ptr data, ptr other, bool live)
{
    if (!live) return;
    if (group->MapPolicy) {
        xerror* previous = xrtTakeError(); group->MapPolicy->Drop(data);
        xrtClearError(); xrtSetErrorTake(previous);
    } else {
        xrtownershipscope scope = {0};
        if (!xrtOwnershipMutationBegin(&scope)) abort();
        group->MapDestroy(data, other);
        if (!xrtOwnershipScopeEnd(&scope)) abort();
    }
}
/* Logical slots are cleared before their releases. A plan pin protects this
 * allocation through every Finish order; ordinary disposal has RefCount=0. */
static void __xrtFutureCombineDisposeSlots(xrt_future_combine* group)
{
    xrtownershipscope scope = {0}; ptr data, other; bool live;
    __xrtFutureCombineBegin(group, &scope);
    data = group->MapData; other = group->MapDestroyData; live = group->MapLive;
    group->MapData = NULL; group->MapDestroyData = NULL; group->MapLive = false;
    __xrtFutureCombineEnd(group, &scope);
    __xrtFutureCombineDropContext(group, data, other, live);
    for (size_t i = 0; i < group->Count; ++i) {
        xfuture* source;
        __xrtFutureCombineBegin(group, &scope); source = group->Sources[i]; group->Sources[i] = NULL;
        __xrtFutureCombineEnd(group, &scope); xrtFutureDestroy(source);
    }
}
static void __xrtFutureCombineFree(xrt_future_combine* group)
{
    xrtownershipscope scope = {0};
    if (group->Promise || group->Watch || group->Active) abort();
    __xrtFutureCombineDisposeSlots(group);
    if (!xrtOwnershipMutationBegin(&scope)) abort();
    (void)xrtMutexUnit(&group->Lock); xrtFree(group);
    if (!xrtOwnershipScopeEnd(&scope)) abort();
}
static void __xrtFutureCombineRelease(xrt_future_combine* group)
{
    xrtownershipscope scope = {0}; int32 left;
    __xrtFutureCombineBegin(group, &scope); left = xrtRefRelease(&group->RefCount);
    __xrtFutureCombineEnd(group, &scope);
    if (left < 0) abort();
    if (!left) __xrtFutureCombineFree(group);
}
static void __xrtFutureCombineProducerDrop(const void* data)
{ __xrtFutureCombineRelease((xrt_future_combine*)data); }
static bool __xrtFutureCombineHold(const void* data)
{
    xrt_future_combine* group = (xrt_future_combine*)data; xrtownershipscope scope = {0}; bool held;
    __xrtFutureCombineBegin(group, &scope);
    held = !group->Cleared && xrtRefRetain(&group->RefCount) > 0;
    __xrtFutureCombineEnd(group, &scope); return held;
}
static const xfutureproducerownershipv1 __xrtFutureCombineProducerPolicy = {
    sizeof(xfutureproducerownershipv1), __xrtFutureCombineProducerDrop
};
XRT_API const xfutureproducerownershipv1* xrtFutureCombineProducerPolicyV1Get(void)
{ return &__xrtFutureCombineProducerPolicy; }
static void __xrtFutureCombineWaiterRelease(ptr data)
{
    xrt_future_combine_item* item = data; xrt_future_combine* group = item->Group;
    xrtownershipscope scope = {0}; int32 left;
    __xrtFutureCombineBegin(group, &scope);
    if (!item->Owned) abort();
    item->Owned = false; left = xrtRefRelease(&group->RefCount);
    __xrtFutureCombineEnd(group, &scope);
    if (left < 0) abort();
    if (!left) __xrtFutureCombineFree(group);
}
static void __xrtFutureCombineSourceDone(ptr data);
static void __xrtFutureCombineCancelled(ptr data);
static const xfuturewatchownershipv2 __xrtFutureCombineWatchPolicy = {
    sizeof(xfuturewatchownershipv2), __xrtFutureCombineSourceDone,
    __xrtFutureCombineWaiterRelease, __xrtFutureCombineItemOwner
};
static const xcancelwatchownershipv1 __xrtFutureCombineCancelPolicy = {
    sizeof(xcancelwatchownershipv1), __xrtFutureCombineCancelled,
    __xrtFutureCombineProducerDrop, &__xrtFutureCombineOwnershipOps
};
XRT_API const xfuturewatchownershipv2* xrtFutureCombineWatchPolicyV2Get(void)
{ return &__xrtFutureCombineWatchPolicy; }
XRT_API const xcancelwatchownershipv1* xrtFutureCombineCancelPolicyV1Get(void)
{ return &__xrtFutureCombineCancelPolicy; }
static xrt_future_combine* __xrtFutureCombineAllGroup(const void* value)
{ return (xrt_future_combine*)((unsigned char*)value - offsetof(xrt_future_combine, All)); }
static xrt_future_combine* __xrtFutureCombinePickGroup(const void* value)
{ return (xrt_future_combine*)((unsigned char*)value - offsetof(xrt_future_combine, Pick)); }
static void __xrtFutureCombineAllDrop(ptr value, ptr data)
{ if (data) abort(); __xrtFutureCombineRelease(__xrtFutureCombineAllGroup(value)); }
static void __xrtFutureCombinePickDrop(ptr value, ptr data)
{ if (data) abort(); __xrtFutureCombineRelease(__xrtFutureCombinePickGroup(value)); }
static bool __xrtFutureCombineAllTrace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return value && !data && visit && visit(__xrtFutureCombineOwnership(__xrtFutureCombineAllGroup(value)), context); }
static bool __xrtFutureCombinePickTrace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return value && !data && visit && visit(__xrtFutureCombineOwnership(__xrtFutureCombinePickGroup(value)), context); }
static const xfuturepayloadownershipv1 __xrtFutureCombineAllPolicy = {
    sizeof(xfuturepayloadownershipv1), __xrtFutureCombineAllDrop, __xrtFutureCombineAllTrace
};
static const xfuturepayloadownershipv1 __xrtFutureCombinePickPolicy = {
    sizeof(xfuturepayloadownershipv1), __xrtFutureCombinePickDrop, __xrtFutureCombinePickTrace
};
XRT_API const xfuturepayloadownershipv1* xrtFutureCombineAllPayloadPolicyV1Get(void)
{ return &__xrtFutureCombineAllPolicy; }
XRT_API const xfuturepayloadownershipv1* xrtFutureCombinePickPayloadPolicyV1Get(void)
{ return &__xrtFutureCombinePickPolicy; }
static void __xrtFutureCombineDetach(xrt_future_combine* group, xrt_future_combine_item* current)
{
    for (size_t i = 0; i < group->Count; ++i)
        if (&group->Items[i] != current) (void)xrtFutureWatchDetach(group->Sources[i], &group->Items[i].Watch);
}
static void __xrtFutureCombineUnwatch(xrt_future_combine* group)
{
    xrtownershipscope scope = {0}; xcancelwatch* watch;
    __xrtFutureCombineBegin(group, &scope); watch = group->Watch; group->Watch = NULL;
    __xrtFutureCombineEnd(group, &scope); xrtCancelUnwatch(watch);
}
static void __xrtFutureCombineCancelSources(xrt_future_combine* group, size_t except)
{
    for (size_t i = 0; i < group->Count; ++i) if (i != except) (void)xrtFutureCancel(group->Sources[i]);
}
static void __xrtFutureCombineComplete(xrt_future_combine* group, xpromise* promise)
{
    if (group->MapAccepted) {
        xrtownershipscope callback = {0};
        if (!group->MapPolicy && !xrtOwnershipMutationBegin(&callback)) abort();
        if (group->AllMap) group->AllMap(&group->All, promise, group->MapData);
        else group->PickMap(&group->Pick, promise, group->MapData);
        if (!group->MapPolicy && !xrtOwnershipScopeEnd(&callback)) abort();
        if (!xrtPromiseDone(promise)) (void)xrtPromiseClose(promise);
    } else {
        const xfuturepayloadownershipv1* policy = group->Mode == XRT_FUTURE_COMBINE_ALL ?
            &__xrtFutureCombineAllPolicy : &__xrtFutureCombinePickPolicy;
        ptr value = group->Mode == XRT_FUTURE_COMBINE_ALL ? (ptr)&group->All : (ptr)&group->Pick;
        if (!__xrtFutureCombineHold(group)) abort();
        if (!xrtPromiseResolveOwnedPolicyV1(promise, value, policy)) __xrtFutureCombineRelease(group);
    }
    xrtPromiseDestroy(promise);
    __xrtFutureCombineUnwatch(group);
}
static void __xrtFutureCombineCancelled(ptr data)
{
    xrt_future_combine* group = data; xrtownershipscope scope = {0}; xpromise* promise;
    __xrtFutureCombineBegin(group, &scope);
    if (group->Completed) { __xrtFutureCombineEnd(group, &scope); return; }
    ++group->Active; group->Completed = true; promise = group->Promise; group->Promise = NULL;
    __xrtFutureCombineEnd(group, &scope);
    __xrtFutureCombineDetach(group, NULL);
    (void)xrtPromiseCancel(promise); xrtPromiseDestroy(promise);
    __xrtFutureCombineCancelSources(group, SIZE_MAX); __xrtFutureCombineUnwatch(group);
    __xrtFutureCombineBegin(group, &scope); --group->Active; __xrtFutureCombineEnd(group, &scope);
}
static void __xrtFutureCombineSourceDone(ptr data)
{
    xrt_future_combine_item* item = data; xrt_future_combine* group = item->Group;
    xrtownershipscope scope = {0}; xpromise* promise = NULL; bool race = false;
    __xrtFutureCombineBegin(group, &scope); ++group->Active;
    if (!group->Completed) {
        if (group->Mode == XRT_FUTURE_COMBINE_ALL) {
            if (!group->Remaining) abort();
            if (--group->Remaining == 0) group->Completed = true;
        } else {
            group->Completed = true; group->Pick.Index = item->Index;
            group->Pick.Future = group->Sources[item->Index]; race = group->Mode == XRT_FUTURE_COMBINE_RACE;
        }
        if (group->Completed) { promise = group->Promise; group->Promise = NULL; }
    }
    __xrtFutureCombineEnd(group, &scope);
    if (promise) {
        __xrtFutureCombineDetach(group, item); __xrtFutureCombineComplete(group, promise);
        if (race) __xrtFutureCombineCancelSources(group, item->Index);
    }
    __xrtFutureCombineBegin(group, &scope); --group->Active; __xrtFutureCombineEnd(group, &scope);
}
static bool __xrtFutureCombineDone(xrt_future_combine* group)
{
    xrtownershipscope scope = {0}; bool done;
    __xrtFutureCombineBegin(group, &scope); done = group->Completed; __xrtFutureCombineEnd(group, &scope); return done;
}
static void __xrtFutureCombineAttach(xrt_future_combine* group, size_t index)
{
    xrt_future_combine_item* item = &group->Items[index]; xrtownershipscope scope = {0}; xfuturewatchresult added;
    __xrtFutureCombineBegin(group, &scope);
    if (group->Completed) { __xrtFutureCombineEnd(group, &scope); return; }
    if (item->Owned || xrtRefRetain(&group->RefCount) < 0) abort();
    item->Owned = true; __xrtFutureCombineEnd(group, &scope);
    added = xrtFutureWatchAdd(group->Sources[index], &item->Watch);
    if (added == XFUTURE_WATCH_READY) {
        __xrtFutureCombineSourceDone(item); __xrtFutureCombineWaiterRelease(item);
    } else if (added == XFUTURE_WATCH_PENDING) {
        if (__xrtFutureCombineDone(group)) (void)xrtFutureWatchDetach(group->Sources[index], &item->Watch);
    } else abort(); /* Valid preinitialized private storage and retained source; no fallible allocation. */
}
static xfuture* __xrtFutureCombineCreate(xfuture* const* futures, size_t count, xrt_future_combine_mode mode,
    xfutureallmapproc allMap, xfuturepickmapproc pickMap, ptr data, xfuturefreeproc destroy, ptr other,
    xfutureownershiptrace trace, const xfuturecombineownershipv1* policy)
{
    xrt_future_combine* group; xfuture* future = NULL; xcancel* cancel; xrtownershipscope scope = {0};
    xpromise* promise; xcancelwatch* watch; size_t bytes;
    if ((mode != XRT_FUTURE_COMBINE_ALL && !count) || (count && !futures)) { __xrtErrorSetInvalidArgument(); return NULL; }
    if (count > (size_t)(INT32_MAX - 3) ||
        count > (SIZE_MAX - sizeof(*group)) / (sizeof(xfuture*) + sizeof(xrt_future_combine_item))) {
        __xrtErrorSetSizeOverflow(); return NULL;
    }
    bytes = sizeof(*group) + count * (sizeof(xfuture*) + sizeof(xrt_future_combine_item));
    if (!xrtOwnershipMutationBegin(&scope)) return NULL;
    group = xrtCalloc(1, bytes);
    if (!group) { if (!xrtOwnershipScopeEnd(&scope)) abort(); return NULL; }
    group->RefCount = 1; group->Publishing = true; group->Mode = mode; group->Count = count; group->Remaining = count;
    group->Sources = count ? (xfuture**)(group + 1) : NULL;
    group->Items = count ? (xrt_future_combine_item*)(group->Sources + count) : NULL;
    group->All.Count = count; group->All.Futures = group->Sources;
    group->AllMap = allMap; group->PickMap = pickMap; group->MapData = data;
    group->MapDestroy = destroy; group->MapDestroyData = other; group->MapTrace = trace; group->MapPolicy = policy;
    if (!xrtMutexInit(&group->Lock)) {
        xrtFree(group); if (!xrtOwnershipScopeEnd(&scope)) abort(); return NULL;
    }
    if (!xrtOwnershipScopeEnd(&scope)) abort();
    for (size_t i = 0; i < count; ++i) {
        xfuture* source = xrtFutureRef(futures[i]);
        if (!source) goto failed;
        __xrtFutureCombineBegin(group, &scope); group->Sources[i] = source;
        group->Items[i].Group = group; group->Items[i].Index = i;
        __xrtFutureCombineEnd(group, &scope);
        if (!xrtFutureWatchInitOwnershipV2(&group->Items[i].Watch, &group->Items[i], &__xrtFutureCombineWatchPolicy)) goto failed;
    }
    promise = xrtPromiseCreate(&future, NULL);
    if (!promise) goto failed;
    __xrtFutureCombineBegin(group, &scope); group->Promise = promise; __xrtFutureCombineEnd(group, &scope);
    if (!__xrtFutureCombineHold(group)) abort();
    if (!xrtPromiseProducerBindTakeV1(group->Promise, __xrtFutureCombineOwnership(group), &__xrtFutureCombineProducerPolicy)) {
        __xrtFutureCombineRelease(group); goto failed;
    }
    if (count) {
        cancel = xrtPromiseCancelToken(group->Promise);
        if (!cancel) goto failed;
        if (!__xrtFutureCombineHold(group)) abort();
        watch = xrtCancelWatchOwnedV1(cancel, group, &__xrtFutureCombineCancelPolicy);
        xrtCancelDestroy(cancel);
        if (!watch) { __xrtFutureCombineRelease(group); goto failed; }
        __xrtFutureCombineBegin(group, &scope); group->Watch = watch; __xrtFutureCombineEnd(group, &scope);
    }
    /* All fallible preparation is finished before accepting context or
     * observing any source. Private publication remains an inspection refusal. */
    __xrtFutureCombineBegin(group, &scope);
    group->MapLive = group->MapAccepted = allMap != NULL || pickMap != NULL;
    if (!count) { group->Completed = true; ++group->Active; promise = group->Promise; group->Promise = NULL; }
    else promise = NULL;
    __xrtFutureCombineEnd(group, &scope);
    if (!count) {
        __xrtFutureCombineComplete(group, promise);
        __xrtFutureCombineBegin(group, &scope); --group->Active; __xrtFutureCombineEnd(group, &scope);
    } else {
        if (mode != XRT_FUTURE_COMBINE_ALL)
            for (size_t i = 0; i < count; ++i) if (xrtFutureDone(group->Sources[i])) { __xrtFutureCombineAttach(group, i); break; }
        for (size_t i = 0; i < count && !__xrtFutureCombineDone(group); ++i) __xrtFutureCombineAttach(group, i);
    }
    __xrtFutureCombineBegin(group, &scope); group->Publishing = false; __xrtFutureCombineEnd(group, &scope);
    __xrtFutureCombineRelease(group); return future;
failed:
    __xrtFutureCombineUnwatch(group);
    __xrtFutureCombineBegin(group, &scope); promise = group->Promise; group->Promise = NULL; __xrtFutureCombineEnd(group, &scope);
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); __xrtFutureCombineRelease(group); return NULL;
}
XRT_API xfuture* xrtFutureAll(xfuture* const* futures, size_t count)
{ return __xrtFutureCombineCreate(futures, count, XRT_FUTURE_COMBINE_ALL, NULL, NULL, NULL, NULL, NULL, NULL, NULL); }
XRT_API xfuture* xrtFutureAny(xfuture* const* futures, size_t count)
{ return __xrtFutureCombineCreate(futures, count, XRT_FUTURE_COMBINE_ANY, NULL, NULL, NULL, NULL, NULL, NULL, NULL); }
XRT_API xfuture* xrtFutureRace(xfuture* const* futures, size_t count)
{ return __xrtFutureCombineCreate(futures, count, XRT_FUTURE_COMBINE_RACE, NULL, NULL, NULL, NULL, NULL, NULL, NULL); }
static xfuture* __xrtFutureCombineMapped(xfuture* const* futures, size_t count, xrt_future_combine_mode mode,
    xfutureallmapproc allMap, xfuturepickmapproc pickMap, ptr data, xfuturefreeproc destroy, ptr other, xfutureownershiptrace trace)
{
    if ((!allMap && !pickMap) || !destroy || !trace) { __xrtErrorSetInvalidArgument(); return NULL; }
    return __xrtFutureCombineCreate(futures, count, mode, allMap, pickMap, data, destroy, other, trace, NULL);
}
XRT_API xfuture* xrtFutureAllMapOwnedTraced(xfuture* const* futures, size_t count,
    xfutureallmapproc map, ptr data, xfuturefreeproc destroy, ptr other, xfutureownershiptrace trace)
{ return __xrtFutureCombineMapped(futures, count, XRT_FUTURE_COMBINE_ALL, map, NULL, data, destroy, other, trace); }
XRT_API xfuture* xrtFutureAnyMapOwnedTraced(xfuture* const* futures, size_t count,
    xfuturepickmapproc map, ptr data, xfuturefreeproc destroy, ptr other, xfutureownershiptrace trace)
{ return __xrtFutureCombineMapped(futures, count, XRT_FUTURE_COMBINE_ANY, NULL, map, data, destroy, other, trace); }
XRT_API xfuture* xrtFutureRaceMapOwnedTraced(xfuture* const* futures, size_t count,
    xfuturepickmapproc map, ptr data, xfuturefreeproc destroy, ptr other, xfutureownershiptrace trace)
{ return __xrtFutureCombineMapped(futures, count, XRT_FUTURE_COMBINE_RACE, NULL, map, data, destroy, other, trace); }
static xfuture* __xrtFutureCombinePolicy(xfuture* const* futures, size_t count, xrt_future_combine_mode mode,
    ptr data, const xfuturecombineownershipv1* policy)
{
    if (!data || !policy || policy->size != sizeof(*policy) || !policy->Drop || !policy->Ops ||
        !policy->Ops->Count || !policy->Ops->Trace || (mode == XRT_FUTURE_COMBINE_ALL ? !policy->AllMap : !policy->PickMap)) {
        __xrtErrorSetInvalidArgument(); return NULL;
    }
    return __xrtFutureCombineCreate(futures, count, mode, mode == XRT_FUTURE_COMBINE_ALL ? policy->AllMap : NULL,
        mode == XRT_FUTURE_COMBINE_ALL ? NULL : policy->PickMap, data, NULL, NULL, NULL, policy);
}
XRT_API xfuture* xrtFutureAllMapOwnedPolicyV1(xfuture* const* futures, size_t count, ptr data, const xfuturecombineownershipv1* policy)
{ return __xrtFutureCombinePolicy(futures, count, XRT_FUTURE_COMBINE_ALL, data, policy); }
XRT_API xfuture* xrtFutureAnyMapOwnedPolicyV1(xfuture* const* futures, size_t count, ptr data, const xfuturecombineownershipv1* policy)
{ return __xrtFutureCombinePolicy(futures, count, XRT_FUTURE_COMBINE_ANY, data, policy); }
XRT_API xfuture* xrtFutureRaceMapOwnedPolicyV1(xfuture* const* futures, size_t count, ptr data, const xfuturecombineownershipv1* policy)
{ return __xrtFutureCombinePolicy(futures, count, XRT_FUTURE_COMBINE_RACE, data, policy); }

static bool __xrtFutureCombineClaim(const void* data, const void* token)
{
    xrt_future_combine* group = (xrt_future_combine*)data; size_t count;
    if (!token || !__xrtFutureCombineOwnershipCount(group, &count) || (group->Claim && group->Claim != token)) return false;
    group->Claim = token; return true;
}
static void __xrtFutureCombineRestore(const void* data, const void* token)
{
    xrt_future_combine* group = (xrt_future_combine*)data;
    if (!token || group->Claim != token || group->Cleared) abort();
    group->Claim = NULL;
}
static bool __xrtFutureCombinePrepared(const void* data)
{
    const xrt_future_combine* group = data; size_t count;
    if (!__xrtFutureCombineOwnershipCount(data, &count) || !group->Completed || group->Promise || group->Watch) return false;
    for (size_t i = 0; i < group->Count; ++i) if (group->Items[i].Owned) return false;
    return true;
}
static xrtownershipprepareresult __xrtFutureCombinePrepare(const void* data, const void* token)
{
    xrt_future_combine* group = (xrt_future_combine*)data; xrtownershipscope freeze = {0}; bool ready;
    if (!xrtOwnershipFreezeTryBegin(&freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || group->Claim != token || group->Cleared) abort();
    ready = __xrtFutureCombinePrepared(data);
    if (!xrtOwnershipScopeEnd(&freeze)) abort();
    return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtFutureCombineClear(const void* data, const void* token)
{
    xrt_future_combine* group = (xrt_future_combine*)data;
    if (!token || group->Claim != token || group->Cleared || !__xrtFutureCombinePrepared(data)) abort();
    group->Cleared = true;
}
static bool __xrtFutureCombineFinish(const void* data, const void* token)
{
    xrt_future_combine* group = (xrt_future_combine*)data;
    if (!token || group->Claim != token || !group->Cleared) abort();
    __xrtFutureCombineDisposeSlots(group); return true;
}
XRT_API const xrtownershipadapterv1* xrtFutureCombineOwnershipAdapterV1(xrtownershipref ref,
    const xfuturecombineownershipv1* const* policies, size_t policyCount, const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), __xrtFutureCombineHold, __xrtFutureCombineProducerDrop,
        __xrtFutureCombineClaim, __xrtFutureCombineRestore, NULL, __xrtFutureCombineClear, __xrtFutureCombineFinish};
    static const xrtownershippreparationv1 prepare = {sizeof(prepare), &adapter, __xrtFutureCombinePrepared, __xrtFutureCombinePrepare};
    const xrt_future_combine* group; size_t count; bool known = false;
    if (ref.Ops != &__xrtFutureCombineOwnershipOps || !ref.Data || !preparation || (policyCount && !policies)) return NULL;
    group = ref.Data;
    if (!__xrtFutureCombineOwnershipCount(group, &count)) return NULL;
    if (group->MapAccepted) {
        for (size_t i = 0; i < policyCount; ++i) if (policies[i] && policies[i] == group->MapPolicy) { known = true; break; }
        if (!known || group->MapPolicy->size != sizeof(xfuturecombineownershipv1) || !group->MapPolicy->Drop ||
            !group->MapPolicy->Ops || !group->MapPolicy->Ops->Count || !group->MapPolicy->Ops->Trace ||
            (group->Mode == XRT_FUTURE_COMBINE_ALL ? group->AllMap != group->MapPolicy->AllMap : group->PickMap != group->MapPolicy->PickMap)) return NULL;
    }
    *preparation = &prepare; return &adapter;
}
XRT_API bool xrtFutureCombineWaitV1(xrtownershipref ref, const void* token, xfuturecombinewaitv1* wait)
{
    const xrt_future_combine* group; size_t references, pending = 0;
    const xcancelwatchownershipv1* policy = &__xrtFutureCombineCancelPolicy;
    const xrtownershippreparationv1* preparation = NULL;
    if (ref.Ops != &__xrtFutureCombineOwnershipOps || !ref.Data || !token || !wait) return false;
    group = ref.Data;
    if (group->Claim != token || !__xrtFutureCombineOwnershipCount(group, &references) || group->Completed ||
        !group->Promise || !group->Watch || (group->MapAccepted && !group->MapPolicy) ||
        xrtPromiseDone(group->Promise) || xrtCancelTriggered(group->Watch) ||
        !xrtCancelWatchOwnershipAdapterV1(xrtCancelWatchOwnership(group->Watch), &policy, 1, &preparation)) return false;
    for (size_t i = 0; i < group->Count; ++i) {
        const xrt_future_combine_item* item = &group->Items[i];
        const xrt_future_waiter* watcher = &__xrtFutureWatchImpl((xfuturewatch*)&item->Watch)->Waiter;
        if (xrtFutureState(group->Sources[i]) == XFUTURE_PENDING) {
            if (!item->Owned || !watcher->Linked || watcher->Calling) return false;
            ++pending;
        } else if (item->Owned || watcher->Linked || watcher->Calling) return false;
    }
    if (!pending || (group->Mode == XRT_FUTURE_COMBINE_ALL && pending != group->Remaining) ||
        (group->Mode != XRT_FUTURE_COMBINE_ALL && pending != group->Count)) return false;
    *wait = (xfuturecombinewaitv1){sizeof(*wait), group->Promise, group->Sources, group->Count,
        group->Mode == XRT_FUTURE_COMBINE_ALL ? XFUTURE_WAIT_ALL_TERMINAL : XFUTURE_WAIT_ANY_TERMINAL,
        group->Mode == XRT_FUTURE_COMBINE_RACE};
    return true;
}
#endif
