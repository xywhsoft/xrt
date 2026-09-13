#ifdef FUTURE_WAITER_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

_Static_assert(sizeof(xfuturewatch) == 64, "public Watch ABI remains fixed");
typedef struct watch_data { xvalue* first; xvalue* second; xfuture* source; } watch_data;
static unsigned notifications, releases, traces;
static void notify_watch(ptr data) { (void)data; ++notifications; }
static void release_watch(ptr data)
{
    watch_data* owned = data; ++releases;
    xrtValueRelease(owned->first); xrtValueRelease(owned->second);
    xrtFutureDestroy(owned->source); xrtFree(owned);
}
static bool trace_watch(const void* data, xrtownershipvisitor visit, ptr context)
{
    const watch_data* owned = data; ++traces;
    return visit(xrtValueOwnership(owned->first), context) &&
        visit(xrtValueOwnership(owned->second), context) && visit(xrtFutureOwnership(owned->source), context);
}
static void drop_value(ptr value, ptr data) { (void)data; xrtValueRelease(value); }
static bool trace_value(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return data == NULL && visit(xrtValueOwnership(value), context); }
static void reject_graph(xfuture* future)
{
    xrtownershipref root = xrtFutureOwnership(future); xrtownershipresult old = {11,22,33,44}, graph = old; bool live = true;
    testRequire(!xrtOwnershipInspectReachable(&root, 1, NULL, 0, &live, &graph, NULL, NULL), "opaque waiter rejected");
    testRequire(live && memcmp(&old, &graph, sizeof(old)) == 0, "graph rejection atomic"); xrtClearError();
}
static void inspect(xfuture* future, xrtownershipref* slots, size_t count, size_t nodes, size_t edges, size_t roots, bool reachable)
{
    xrtownershipref anchor = xrtFutureOwnership(future); xrtownershipresult graph = {0}; bool live = !reachable;
    testRequire(xrtOwnershipInspectReachable(&anchor, 1, slots, count, &live, &graph, NULL, NULL), "inspect complete waiter/group graph");
    if (graph.NodeCount != nodes || graph.EdgeCount != edges || graph.ExternalRootCount != roots || live != reachable)
        fprintf(stderr, "graph %zu/%zu/%zu live=%u expected %zu/%zu/%zu live=%u\n", graph.NodeCount, graph.EdgeCount, graph.ExternalRootCount, live, nodes, edges, roots, reachable);
    testRequire(graph.NodeCount == nodes && graph.EdgeCount == edges && graph.ExternalRootCount == roots && live == reachable, "exact graph");
}
static void watcher_round(unsigned mode)
{
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
    xfuturewatch watch, untouched; xrtownershipref slots[2]; watch_data* owned = xrtMalloc(sizeof(*owned));
    unsigned before_notify = notifications, before_release = releases, before_trace = traces;
    testRequire(promise != NULL && owned != NULL, "watch fixture");
    owned->first = xrtValueInt(42000); owned->second = xrtValueRetain(owned->first);
    owned->source = xrtFutureRef(future); testRequire(owned->first && owned->second && owned->source, "watch owned slots");
    memset(&watch, 0x5A, sizeof(watch)); untouched = watch;
    testRequire(!xrtFutureWatchInitTraced(&watch, notify_watch, release_watch, owned, NULL), "trace required");
    testRequire(memcmp(&watch, &untouched, sizeof(watch)) == 0, "invalid init does not mutate storage"); xrtClearError();
    testRequire(xrtFutureWatchInitTraced(&watch, notify_watch, release_watch, owned, trace_watch), "traced Watch init");
    testRequire(traces == before_trace && releases == before_release, "init does not trace/drop");
    if (mode == 2) testRequire(xrtPromiseResolve(promise, NULL), "ready before registration");
    testRequire(xrtFutureWatchAdd(future, &watch) == (mode == 2 ? XFUTURE_WATCH_READY : XFUTURE_WATCH_PENDING), "registration ownership result");
    slots[0] = xrtFutureOwnership(future); slots[1] = xrtPromiseOwnership(promise);
    if (mode != 2) {
        unsigned failures = 0;
        inspect(future, slots, 2, 3, 6, 0, false); /* cancel + two Value slots + self + two enclosing slots */
        for (unsigned point = 0; point < 10; ++point) {
            xrtownershipresult old = {11,22,33,44}, graph = old; bool live = true, ok;
            testRequire(xrtMemDebugFailAfter(point), "arm pending graph OOM");
            ok = xrtOwnershipInspectReachable(slots, 1, slots, 2, &live, &graph, NULL, NULL);
            if (xrtMemDebugFailTriggered()) { ++failures; testRequire(!ok && live && memcmp(&old, &graph, sizeof(old)) == 0, "pending graph OOM atomic"); }
            else testRequire(ok && !live, "graph after allocation frontier");
            xrtMemDebugFailClear(); xrtClearError();
        }
        testRequire(failures != 0, "actual pending graph allocation failures exercised");
        testRequire(xrtValueRetain(owned->first) == owned->first, "external Value alias");
        inspect(future, slots, 2, 3, 6, 1, false); xrtValueRelease(owned->first);
        if (mode == 0) testRequire(xrtFutureWatchDetach(future, &watch), "detach and release");
        else testRequire(xrtPromiseResolve(promise, NULL), "complete and release");
    } else {
        testRequire(releases == before_release && notifications == before_notify, "READY does not transfer registration");
        release_watch(owned);
    }
    testRequire(releases == before_release + 1 && notifications == before_notify + (mode == 1), "exact notification/release");
    inspect(future, slots, 2, 2, 3, 0, false);
    if (mode == 0) {
        testRequire(xrtFutureWatchInit(&watch, notify_watch, NULL, NULL), "old opaque Watch");
        testRequire(xrtFutureWatchAdd(future, &watch) == XFUTURE_WATCH_PENDING, "opaque registration"); reject_graph(future);
        testRequire(xrtFutureWatchDetach(future, &watch), "opaque detach");
    }
    xrtPromiseDestroy(promise); xrtFutureDestroy(future);
}
static void resolve_value(xpromise* promise, int64 value)
{
    xvalue* result = xrtValueInt(value); testRequire(result != NULL, "owned source payload");
    testRequire(xrtPromiseResolveOwnedTraced(promise, result, drop_value, NULL, trace_value), "resolve source");
}
static void combine_round(unsigned mode)
{
    xfuture *a = NULL, *b = NULL, *output; xpromise *pa = xrtPromiseCreate(&a, NULL), *pb = xrtPromiseCreate(&b, NULL);
    xfuture* inputs[3] = {a,a,b}; xrtownershipref slots[5];
    testRequire(pa && pb, "aggregate sources");
    output = mode == 1 ? xrtFutureAny(inputs, 3) : mode == 2 ? xrtFutureRace(inputs, 3) : xrtFutureAll(inputs, 3);
    testRequire(output != NULL, "aggregate");
    slots[0] = xrtFutureOwnership(a); slots[1] = xrtFutureOwnership(b);
    slots[2] = xrtPromiseOwnership(pa); slots[3] = xrtPromiseOwnership(pb); slots[4] = xrtFutureOwnership(output);
    /* Exactly one pending operation base reference, not an omitted slot. */
    inspect(a, slots, 5, 8, 17, 1, true);
    if (mode == 3) {
        testRequire(xrtFutureCancel(output), "cancel aggregate");
        testRequire(xrtFutureState(output) == XFUTURE_CANCELLED, "aggregate terminal cancellation");
        { xcancel* token = xrtPromiseCancelToken(pa);
          testRequire(token != NULL && xrtCancelRequested(token), "cancellation propagated");
          xrtCancelDestroy(token); }
    } else {
        resolve_value(pa, 42); resolve_value(pb, 84);
        testRequire(xrtFutureState(output) == XFUTURE_RESOLVED, "aggregate resolved");
        if (mode == 0) {
            const xfutureall* all = xrtFutureValue(output);
            testRequire(all && all->Count == 3 && all->Futures[0] == all->Futures[1] && all->Futures[0] == a, "duplicate source identity preserved");
        } else {
            const xfuturepick* pick = xrtFutureValue(output);
            testRequire(pick && pick->Index == 0 && pick->Future == a, "borrowed winner aliases real source slot");
        }
    }
    xrtPromiseDestroy(pa); xrtPromiseDestroy(pb); xrtFutureDestroy(a); xrtFutureDestroy(b);
    if (mode == 3) inspect(output, &slots[4], 1, 2, 2, 0, false);
    else {
        inspect(output, &slots[4], 1, 10, 12, 0, false);
        testRequire(xrtFutureRef(output) == output, "native aggregate alias");
        inspect(output, &slots[4], 1, 10, 12, 1, true); xrtFutureDestroy(output);
    }
    xrtFutureDestroy(output);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true), "memory debug");
    for (unsigned round = 0; round < 100; ++round) {
        xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
        for (unsigned mode = 0; mode < 3; ++mode) watcher_round(mode);
        for (unsigned mode = 0; mode < 4; ++mode) combine_round(mode);
        {
            xfuture* empty = xrtFutureAll(NULL, 0); xrtownershipref slot = xrtFutureOwnership(empty);
            testRequire(empty != NULL, "empty all"); inspect(empty, &slot, 1, 3, 3, 0, false); xrtFutureDestroy(empty);
        }
        testRequire(xrtGetError() == NULL, "clean error slot"); xrtMemDebugSnapshot(&after);
        testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount, "exact graph transaction memory");
    }
    puts("Future waiter ownership: 300 Watch transfers, 500 aggregate graphs, 2000 pending OOM probes, exact nodes/edges/roots and memory");
    return 0;
}
