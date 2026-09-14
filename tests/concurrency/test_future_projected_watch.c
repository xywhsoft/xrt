#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef FUTURE_PROJECTED_WATCH_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

_Static_assert(sizeof(xfuturewatch) == 64, "projected waiter preserves public storage");
typedef struct counts { unsigned notify, release, project; } counts;
typedef struct owner owner;
typedef struct slot { xfuturewatch watch; owner* parent; } slot;
struct owner { volatile int32 refs; slot item; xfuture* source; xvalue* value; counts* events; };
static bool owner_count(const void* data, size_t* count)
{ const owner* value = data; if (!value || !count || value->refs <= 0) return false; *count = (size_t)value->refs; return true; }
static bool owner_trace(const void* data, xrtownershipvisitor visit, ptr context)
{ const owner* value = data; return visit(xrtFutureOwnership(value->source), context) && visit(xrtValueOwnership(value->value), context); }
static const xrtownershipops owner_ops = {owner_count, owner_trace};
static xrtownershipref project(const void* data)
{ owner* value = ((const slot*)data)->parent; ++value->events->project; return (xrtownershipref){value, &owner_ops}; }
static void outside(void)
{ xrtownershipscope freeze = {0}; testRequire(xrtOwnershipFreezeTryBegin(&freeze), "certified callback owns no API scope"); testRequire(xrtOwnershipScopeEnd(&freeze), "probe end"); }
static void notify(ptr data)
{ owner* value = ((slot*)data)->parent; outside(); ++value->events->notify; testRequire(xrtFutureDone(value->source), "source remains live through notify"); }
static void release(ptr data)
{
    owner* value = ((slot*)data)->parent; xrtownershipscope mutation = {0}; outside();
    testRequire(xrtOwnershipMutationBegin(&mutation), "actual owner release transition");
    ++value->events->release; testRequire(xrtRefRelease(&value->refs) == 0, "registration returns the physical owner reference");
    xrtFutureDestroy(value->source); xrtValueRelease(value->value); xrtFree(value);
    testRequire(xrtOwnershipScopeEnd(&mutation), "actual owner release end");
}
static const xfuturewatchownershipv2 policy = {sizeof(policy), notify, release, project};
static const xfuturewatchownershipv2 foreign = {sizeof(foreign), notify, release, project};
static owner* create(xfuture* source, counts* events)
{
    owner* value = xrtCalloc(1, sizeof(*value)); testRequire(value != NULL, "physical owner allocation");
    value->refs = 1; value->item.parent = value; value->events = events;
    value->source = xrtFutureRef(source); value->value = xrtValueInt(55101);
    testRequire(value->source && value->value, "real owner children"); return value;
}
static void graph(xfuture* future, xpromise* promise, owner* value, bool rooted)
{
    xrtownershipscope freeze = {0}; xrtownershipresult result = {0}; bool reachable = !rooted;
    xrtownershipref anchor = xrtFutureOwnership(future), slots[2] = {anchor, xrtPromiseOwnership(promise)};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze), "projected graph freeze");
    testRequire(xrtOwnershipInspectReachable(&anchor, 1, slots, 2, &reachable, &result, NULL, NULL), "projected graph");
    testRequire(result.NodeCount == 4 && result.EdgeCount == 6 && result.ExternalRootCount == (size_t)rooted && reachable == rooted,
        "Future, Cancel, owner and value only; callback address is not a node");
    testRequire(value->refs == (rooted ? 2 : 1), "inspection never retains projected owner");
    testRequire(xrtOwnershipScopeEnd(&freeze), "projected graph end");
}
static void admission(xfuture* future, counts* events, bool known)
{
    const xfuturewatchownershipv2* policies[] = {&policy};
    xfutureownershipadmissionv1 allowed = {0}; const xrtownershippreparationv1* unchanged = (const xrtownershippreparationv1*)(uintptr_t)1;
    const xrtownershippreparationv1* prep = unchanged; xrtownershipref ref = xrtFutureOwnership(future);
    unsigned projections = events->project; xrtownershipscope freeze = {0};
    allowed.size = sizeof(allowed); allowed.ProjectedWatchPolicies = policies; allowed.ProjectedWatchPolicyCount = 1;
    testRequire(xrtOwnershipFreezeTryBegin(&freeze), "admission freeze");
    testRequire(!xrtFutureOwnershipAdapterV3(ref, NULL, 0, NULL, 0, NULL, 0, &prep) && prep == unchanged, "old admission does not silently expand");
    testRequire((xrtFutureOwnershipAdapterV4(ref, &allowed, &prep) != NULL) == known, "exact projected policy identity admission");
    testRequire(known ? prep != unchanged : prep == unchanged, "refused output unchanged");
    testRequire(events->project == projections, "admission never invokes projection");
    for (unsigned i = 0; i < 6; ++i) {
        xfutureownershipadmissionv1 invalid = allowed; prep = unchanged;
        switch (i) {
        case 0: invalid.size--; break;
        case 1: invalid.PayloadPolicyCount = 1; break;
        case 2: invalid.ProducerPolicyCount = 1; break;
        case 3: invalid.WatchPolicyCount = 1; break;
        case 4: invalid.ProjectedWatchPolicies = NULL; break;
        default: invalid.CancelWatchPolicyCount = 1; break;
        }
        testRequire(!xrtFutureOwnershipAdapterV4(ref, &invalid, &prep) && prep == unchanged && events->project == projections, "malformed table fails before projection");
    }
    testRequire(xrtOwnershipScopeEnd(&freeze), "admission end");
}
static void round_trip(unsigned mode)
{
    xmemdebugsnapshot before, after; counts events = {0}; xfuture* future; xpromise* promise; owner* value;
    xrtMemDebugSnapshot(&before); promise = xrtPromiseCreate(&future, NULL); testRequire(promise != NULL, "future fixture");
    value = create(future, &events);
    for (unsigned i = 0; i < 4; ++i) {
        xfuturewatch invalid, old; xfuturewatchownershipv2 bad = policy; memset(&invalid, 0x5a, sizeof(invalid)); old = invalid;
        if (i == 0) bad.size--; else if (i == 1) bad.Notify = NULL; else if (i == 2) bad.Release = NULL; else bad.Reference = NULL;
        testRequire(!xrtFutureWatchInitOwnershipV2(&invalid, &value->item, &bad) && !memcmp(&invalid, &old, sizeof(old)), "invalid init preserves caller storage");
        xrtClearError();
    }
    testRequire(xrtFutureWatchInitOwnershipV2(&value->item.watch, &value->item, mode == 3 ? &foreign : &policy), "projected initialization");
    testRequire(events.project == 0 && events.notify == 0 && events.release == 0 && value->refs == 1, "initialization has no ownership effects");
    testRequire(xrtFutureWatchAdd(NULL, &value->item.watch) == XFUTURE_WATCH_ERROR && !events.release, "ERROR leaves caller ownership"); xrtClearError();
    if (mode == 2) testRequire(xrtPromiseResolve(promise, NULL), "precompleted registration");
    testRequire(xrtFutureWatchAdd(future, &value->item.watch) == (mode == 2 ? XFUTURE_WATCH_READY : XFUTURE_WATCH_PENDING), "actual transfer frontier");
    if (mode != 2) {
        admission(future, &events, mode != 3);
        graph(future, promise, value, false);
        { xrtownershipscope mutation = {0}; testRequire(xrtOwnershipMutationBegin(&mutation), "external owner alias"); testRequire(xrtRefRetain(&value->refs) == 2, "real alias retain"); testRequire(xrtOwnershipScopeEnd(&mutation), "alias transition end"); }
        graph(future, promise, value, true);
        { xrtownershipscope mutation = {0}; testRequire(xrtOwnershipMutationBegin(&mutation), "return alias"); testRequire(xrtRefRelease(&value->refs) == 1, "return actual alias"); testRequire(xrtOwnershipScopeEnd(&mutation), "return alias end"); }
        if (mode == 1) testRequire(xrtPromiseResolve(promise, NULL), "notify and release");
        else testRequire(xrtFutureWatchDetach(future, &value->item.watch), "detach and release owner containing Watch storage");
    } else { testRequire(!events.notify && !events.release, "READY never transfers"); release(&value->item); }
    testRequire(events.release == 1 && events.notify == (unsigned)(mode == 1), "exact notify/release");
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); xrtClearError(); xrtMemDebugSnapshot(&after);
    testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount, "projected owner exact memory balance");
}
int main(void)
{
    for (unsigned i = 0; i < 64; ++i) for (unsigned mode = 0; mode < 4; ++mode) round_trip(mode);
    puts("Projected Watch: 256 transfers; 64 unknown-policy refusals; real embedded storage owner, aliases and exact graphs");
    testMemoryDebugDrain("projected watch drain"); return 0;
}
