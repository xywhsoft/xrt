#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef CANCEL_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

typedef struct counts { unsigned calls, drops, traces, scopes; } counts;
typedef struct owned {
    volatile int32 refs;
    counts* events;
    xvalue *a, *b;
    xcancelwatch* watch; /* A real owning slot only when assigned. */
    bool self, outer;
} owned;
static void mutation(xrtownershipscope* scope) { testRequire(xrtOwnershipMutationBegin(scope), "owned mutation"); }
static void end(xrtownershipscope* scope) { testRequire(xrtOwnershipScopeEnd(scope), "owned scope end"); }
static void callback_scope(owned* value)
{
    xrtownershipscope freeze = {0}; bool held = xrtOwnershipFreezeTryBegin(&freeze);
    testRequire(held != value->outer, "owned callback has no API scope and never suspends caller scope");
    if (held) end(&freeze);
    ++value->events->scopes;
}
static bool count_owned(const void* data, size_t* count)
{ const owned* value = data; if (!value || !count || value->refs <= 0) return false; *count = (size_t)value->refs; return true; }
static bool trace_owned(const void* data, xrtownershipvisitor visit, ptr context)
{
    const owned* value = data; ++value->events->traces;
    return visit(xrtValueOwnership(value->a), context) && visit(xrtValueOwnership(value->b), context) &&
        (!value->watch || visit(xrtCancelWatchOwnership(value->watch), context));
}
static const xrtownershipops owned_ops = {count_owned, trace_owned};
static void drop_owned(const void* data)
{
    owned* value = (owned*)data; xrtownershipscope scope = {0}; callback_scope(value);
    mutation(&scope);
    if (xrtRefRelease(&value->refs) == 0) {
        testRequire(value->watch == NULL, "real watch ownership released before context");
        ++value->events->drops; xrtValueRelease(value->a); xrtValueRelease(value->b); xrtFree(value);
    }
    end(&scope);
}
static void notify_owned(ptr data)
{
    owned* value = data; callback_scope(value); ++value->events->calls;
    if (value->self) {
        xrtownershipscope scope = {0}; xcancelwatch* watch;
        mutation(&scope); watch = value->watch; value->watch = NULL; end(&scope);
        testRequire(watch != NULL, "self owns registration"); xrtCancelUnwatch(watch);
        testRequire(value->events->drops == 0, "Data lives through self-unwatch callback tail");
    }
}
static const xcancelwatchownershipv1 policy = {sizeof(policy), notify_owned, drop_owned, &owned_ops};
static owned* make_owned(counts* events)
{
    owned* value = xrtCalloc(1, sizeof(*value)); testRequire(value != NULL, "context allocation");
    value->refs = 1; value->events = events; value->a = xrtValueInt(42000); value->b = xrtValueRetain(value->a);
    testRequire(value->a && value->b, "duplicate actual owning slots"); return value;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    testRequire(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount,
        "owned cancellation exact allocation balance");
}
static void ordinary(unsigned mode)
{
    counts events = {0}; xmemdebugsnapshot before; xcancel *parent, *child; xcancelwatch* watch; owned* value;
    xrtownershipscope outer = {0}; xrtMemDebugSnapshot(&before);
    parent = xrtCancelCreate(); child = xrtCancelChild(parent); value = make_owned(&events);
    testRequire(parent && child, "tokens");
    if (mode == 2) testRequire(xrtCancelRequest(parent), "already cancelled ancestor");
    value->outer = mode == 4;
    if (value->outer) mutation(&outer);
    watch = xrtCancelWatchOwnedV1(child, value, &policy); testRequire(watch != NULL, "owned registration");
    if (mode == 3) { value->self = true; value->watch = watch; }
    if (mode != 0 && mode != 2) {
        testRequire(xrtCancelRequest(parent), "request parent");
        testRequire(!xrtCancelRequest(parent), "one local request");
        testRequire(xrtCancelRequest(child), "independent child request");
    }
    if (mode != 3) xrtCancelUnwatch(watch);
    if (mode == 4) end(&outer);
    testRequire(events.calls == (mode != 0) && events.drops == 1 && events.scopes == events.calls + 1,
        "exact Notify/Drop/scope counts");
    xrtCancelDestroy(child); xrtCancelDestroy(parent); balanced(&before);
}
static unsigned oom(bool ready)
{
    unsigned failures = 0;
    for (unsigned point = 0; point < 16; ++point) {
        counts events = {0}; xmemdebugsnapshot before; xcancel* token; xcancelwatch* watch; owned* value;
        xrtMemDebugSnapshot(&before); token = xrtCancelCreate(); value = make_owned(&events);
        testRequire(token != NULL, "OOM token");
        if (ready) testRequire(xrtCancelRequest(token), "OOM late registration");
        testRequire(xrtMemDebugFailAfter(point), "arm registration OOM");
        watch = xrtCancelWatchOwnedV1(token, value, &policy);
        if (xrtMemDebugFailTriggered()) {
            ++failures; testRequire(!watch && events.calls == 0 && events.drops == 0 && value->refs == 1,
                "failed registration never accepts Data or notifies");
        } else testRequire(watch != NULL, "success after allocation frontier");
        xrtMemDebugFailClear(); xrtClearError();
        if (watch) xrtCancelUnwatch(watch); else drop_owned(value);
        testRequire(events.drops == 1, "one OOM-context release");
        xrtCancelDestroy(token); balanced(&before);
    }
    testRequire(failures > 0, "actual registration failures"); return failures;
}
static void retirement(bool watch_first, bool restore)
{
    counts events = {0}; xmemdebugsnapshot before; xcancel *parent, *child; owned* value; xcancelwatch* watch;
    const xcancelwatchownershipv1* policies[1] = {&policy};
    const xcancelwatchownershipv1* unknown[1] = {(const xcancelwatchownershipv1*)(uintptr_t)1};
    const xrtownershipadapterv1* adapters[3]; const xrtownershippreparationv1* prepare[3];
    xrtownershipref refs[3]; xrtownershipscope freeze = {0}; xrtownershipresult graph = {0}; unsigned token = 0, wrong = 1;
    xrtMemDebugSnapshot(&before); parent = xrtCancelCreate(); child = xrtCancelChild(parent); value = make_owned(&events);
    watch = xrtCancelWatchOwnedV1(child, value, &policy); testRequire(watch != NULL, "cycle registration");
    value->watch = watch; /* Context takes the actual registration returned above. */
    refs[0] = xrtCancelWatchOwnership(watch); refs[1] = xrtCancelOwnership(child); refs[2] = xrtCancelOwnership(parent);
    xrtCancelDestroy(child); xrtCancelDestroy(parent);
    testRequire(xrtOwnershipFreezeTryBegin(&freeze), "freeze complete native owning cycle");
    testRequire(xrtOwnershipInspect(refs, 1, NULL, 0, &graph) && graph.NodeCount == 5 &&
        graph.EdgeCount == 6 && graph.ExternalRootCount == 0, "real Data/Watch cycle, duplicate slots, no synthetic token edge");
    testRequire(xrtCancelOwnershipAdapterV1(refs[1]) == NULL, "V1 never admits observed token");
    for (unsigned i = 0; i < 3; ++i) {
        prepare[i] = (const xrtownershippreparationv1*)(uintptr_t)1;
        testRequire((i ? xrtCancelOwnershipAdapterV2(refs[i], unknown, 1, &prepare[i]) :
            xrtCancelWatchOwnershipAdapterV1(refs[i], unknown, 1, &prepare[i])) == NULL &&
            prepare[i] == (const xrtownershippreparationv1*)(uintptr_t)1, "unknown policy rejection is atomic");
        adapters[i] = i ? xrtCancelOwnershipAdapterV2(refs[i], policies, 1, &prepare[i]) :
            xrtCancelWatchOwnershipAdapterV1(refs[i], policies, 1, &prepare[i]);
        testRequire(adapters[i] && prepare[i] && prepare[i]->Adapter == adapters[i] &&
            adapters[i]->Hold(refs[i].Data) && adapters[i]->Claim(refs[i].Data, &token), "actual node pin and claim");
        testRequire(!adapters[i]->Claim(refs[i].Data, &wrong), "foreign claim refused");
        testRequire(!prepare[i]->Ready(refs[i].Data), "accepted observer is not prepared");
    }
    end(&freeze);
    for (unsigned i = 0; i < 3; ++i)
        testRequire(prepare[i]->Prepare(refs[i].Data, &token) == XRT_OWNERSHIP_PREPARE_BUSY,
            "collector does not cancel or detach accepted observer");
    testRequire(events.calls == 0 && !xrtCancelRequested(child), "BUSY has no semantic effects");
    if (restore) {
        testRequire(xrtOwnershipFreezeTryBegin(&freeze), "restore freeze");
        for (unsigned i = 0; i < 3; ++i) adapters[i]->Restore(refs[i].Data, &token);
        end(&freeze);
        testRequire(xrtCancelRequest(parent), "restored observer still receives cancellation");
        for (unsigned i = 0; i < 3; ++i) adapters[i]->Drop(refs[i].Data);
        value->watch = NULL; xrtCancelUnwatch(watch);
        testRequire(events.calls == 1 && events.drops == 1, "restore preserves normal lifetime");
    } else {
        xrtownershipscope change = {0}; mutation(&change); value->watch = NULL; end(&change);
        xrtCancelUnwatch(watch); testRequire(events.calls == 0 && events.drops == 0, "owner Unwatch leaves plan-owned Data intact");
        for (unsigned i = 0; i < 3; ++i)
            testRequire(prepare[i]->Prepare(refs[i].Data, &token) == XRT_OWNERSHIP_PREPARE_READY, "real unwatch preparation");
        testRequire(xrtOwnershipFreezeTryBegin(&freeze), "clear freeze");
        for (unsigned i = 0; i < 3; ++i) adapters[i]->Clear(refs[i].Data, &token);
        end(&freeze);
        for (unsigned j = 0; j < 3; ++j) {
            unsigned i = watch_first ? j : 2 - j;
            testRequire(adapters[i]->Finish(refs[i].Data, &token) && adapters[i]->Finish(refs[i].Data, &token), "both finish orders and idempotence");
        }
        for (unsigned i = 0; i < 3; ++i) adapters[i]->Drop(refs[i].Data);
        testRequire(events.calls == 0 && events.drops == 1, "retirement is not cancellation");
    }
    balanced(&before);
}
static void future_route(unsigned route)
{
    counts events = {0}; xmemdebugsnapshot before; xfuture* future; xpromise* promise;
    xcancel *parent, *token; xcancelwatch* watch; owned* value;
    xrtMemDebugSnapshot(&before); parent = xrtCancelCreate(); promise = xrtPromiseCreate(&future, parent);
    testRequire(parent && promise, "Future route endpoints"); token = xrtFutureCancelToken(future);
    value = make_owned(&events); watch = xrtCancelWatchOwnedV1(token, value, &policy);
    testRequire(watch != NULL, "Future owns actual cancellation token"); xrtCancelDestroy(token);
    switch (route) {
        case 0: testRequire(xrtFutureCancel(future), "Future cancellation route"); break;
        case 1: testRequire(xrtPromiseClose(promise), "Promise close route"); break;
        case 2: testRequire(xrtPromiseCancel(promise), "Promise cancellation route"); break;
        case 3: xrtPromiseDestroy(promise); promise = NULL; break;
        default: testRequire(xrtCancelRequest(parent), "Future parent route"); break;
    }
    testRequire(events.calls == 1 && events.drops == 0, "one phased Future observer notification");
    testRequire(xrtFutureState(future) == (route == 1 || route == 3 ? XFUTURE_CLOSED :
        route == 2 ? XFUTURE_CANCELLED : XFUTURE_PENDING), "request never forges producer terminal state");
    xrtCancelUnwatch(watch); testRequire(events.drops == 1, "Future observer drops once");
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); xrtCancelDestroy(parent); balanced(&before);
}
static void noop(ptr data) { (void)data; }
static void invalid_admission(void)
{
    counts events = {0}; xmemdebugsnapshot before; xcancel* token; owned* value;
    xcancelwatch *watch, *legacy; xrtownershipscope freeze = {0};
    const xcancelwatchownershipv1* policies[1] = {&policy};
    const xrtownershippreparationv1* prepare;
    xrtMemDebugSnapshot(&before); token = xrtCancelCreate(); value = make_owned(&events);
    for (unsigned i = 0; i < 7; ++i) {
        xcancelwatchownershipv1 bad = policy; xrtownershipops bad_ops = owned_ops;
        if (i == 0) bad.size = 0;
        if (i == 1) bad.Notify = NULL;
        if (i == 2) bad.Drop = NULL;
        if (i == 3) bad.Ops = NULL;
        if (i == 4) { bad_ops.Count = NULL; bad.Ops = &bad_ops; }
        if (i == 5) { bad_ops.Trace = NULL; bad.Ops = &bad_ops; }
        testRequire(!xrtCancelWatchOwnedV1(token, value, i == 6 ? NULL : &bad), "invalid policy is not accepted");
        testRequire(value->refs == 1 && !events.calls && !events.drops && !events.traces, "invalid policy has no owning effects");
        xrtClearError();
    }
    testRequire(!xrtCancelWatchOwnedV1(NULL, value, &policy), "invalid token retains Data"); xrtClearError();
    testRequire(!xrtCancelWatchOwnedV1(token, NULL, &policy), "invalid Data rejected"); xrtClearError();
    watch = xrtCancelWatchOwnedV1(token, value, &policy); legacy = xrtCancelWatch(token, noop, NULL);
    testRequire(watch && legacy && xrtOwnershipFreezeTryBegin(&freeze), "mixed observer admission fixture");
    prepare = (const xrtownershippreparationv1*)(uintptr_t)1;
    testRequire(!xrtCancelOwnershipAdapterV2(xrtCancelOwnership(token), policies, 1, &prepare) &&
        prepare == (const xrtownershippreparationv1*)(uintptr_t)1, "one legacy observer refuses whole token");
    testRequire(!xrtCancelWatchOwnershipAdapterV1(xrtCancelWatchOwnership(legacy), policies, 1, &prepare), "legacy never certified");
    testRequire(!xrtCancelWatchOwnershipAdapterV1(xrtCancelWatchOwnership(watch), NULL, 1, &prepare), "invalid allowlist rejected");
    testRequire(!xrtCancelWatchOwnershipAdapterV1(xrtCancelWatchOwnership(watch), policies, 1, NULL), "preparation is mandatory");
    testRequire(!events.traces && !events.calls && !events.drops, "admission cannot invoke callback or trace");
    end(&freeze); xrtCancelUnwatch(legacy);
    testRequire(xrtOwnershipFreezeTryBegin(&freeze), "admission after real legacy removal");
    testRequire(xrtCancelOwnershipAdapterV2(xrtCancelOwnership(token), policies, 1, &prepare) != NULL, "known observer independently admitted");
    end(&freeze); xrtCancelUnwatch(watch); xrtCancelDestroy(token);
    testRequire(events.drops == 1 && events.calls == 0, "admission never cancels observer"); balanced(&before);
}
int main(void)
{
    unsigned failures = 0;
    for (unsigned round = 0; round < 64; ++round) {
        for (unsigned mode = 0; mode < 5; ++mode) ordinary(mode);
        retirement(true, false); retirement(false, false); retirement(true, true);
        for (unsigned route = 0; route < 5; ++route) future_route(route);
    }
    failures += oom(false); failures += oom(true);
    invalid_admission();
    printf("Cancel ownership: 320 lifetimes, 192 cycle transactions, 320 Future routes, 32 OOM budgets (%u actual failures)\n", failures);
    testMemoryDebugDrain("cancel ownership memory drain"); return 0;
}
