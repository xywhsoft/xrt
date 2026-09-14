#ifdef FUTURE_MAP_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

typedef struct map_counts { unsigned calls, drops, traces; } map_counts;
typedef struct map_data { xvalue* value; map_counts* counts; bool leave_pending; } map_data;
static void drop_result(ptr value, ptr data) { (void)data; xrtValueRelease(value); }
static bool trace_result(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return data == NULL && visit(xrtValueOwnership(value), context); }
static void drop_context(ptr data, ptr other)
{
    map_data* owned = data; ++owned->counts->drops;
    xrtValueRelease(owned->value); xrtValueRelease(other); xrtFree(owned);
}
static bool trace_context(const void* data, const void* other, xrtownershipvisitor visit, ptr context)
{
    const map_data* owned = data; ++owned->counts->traces;
    return visit(xrtValueOwnership(owned->value), context) && visit(xrtValueOwnership(other), context);
}
static map_data* context_new(map_counts* counts, xvalue** other, bool leave_pending)
{
    map_data* owned = xrtCalloc(1, sizeof(*owned)); testRequire(owned != NULL, "map context");
    owned->value = xrtValueInt(42000); owned->counts = counts; owned->leave_pending = leave_pending;
    *other = xrtValueRetain(owned->value); testRequire(owned->value && *other, "two context owning slots");
    return owned;
}
static void map_publish(xpromise* output, map_data* data)
{
    xvalue* result; ++data->counts->calls;
    if (data->leave_pending) return;
    result = xrtValueRetain(data->value); testRequire(result != NULL, "mapper result retain");
    testRequire(xrtPromiseResolveOwnedTraced(output, result, drop_result, NULL, trace_result), "mapper result publish");
}
static void map_all(const xfutureall* input, xpromise* output, ptr data)
{
    testRequire(input && (input->Count == 0 || (input->Count == 3 && input->Futures[0] == input->Futures[1])), "mapped All order and duplicates");
    for (size_t i = 0; i < input->Count; ++i) testRequire(xrtFutureDone(input->Futures[i]), "All sees terminal inputs");
    map_publish(output, data);
}
static void map_pick(const xfuturepick* input, xpromise* output, ptr data)
{
    testRequire(input && input->Index < 3 && xrtFutureDone(input->Future), "mapped Pick terminal source");
    map_publish(output, data);
}
static xfuture* start(unsigned mode, xfuture** inputs, size_t count, map_data* data, xvalue* other)
{
    if (mode == 0) return xrtFutureAllMapOwnedTraced(inputs, count, map_all, data, drop_context, other, trace_context);
    if (mode == 1) return xrtFutureAnyMapOwnedTraced(inputs, count, map_pick, data, drop_context, other, trace_context);
    return xrtFutureRaceMapOwnedTraced(inputs, count, map_pick, data, drop_context, other, trace_context);
}
static bool cancel_requested(xfuture* future)
{
    xcancel* cancel = xrtFutureCancelToken(future); bool requested;
    testRequire(cancel != NULL, "cancel observation"); requested = xrtCancelRequested(cancel); xrtCancelDestroy(cancel); return requested;
}
static void graph_check(xrtownershipref* anchors, size_t anchor_count, xrtownershipref* slots, size_t count,
    size_t nodes, size_t edges, size_t roots)
{
    xrtownershipresult graph = {0};
    testRequire(xrtOwnershipInspect(anchors, anchor_count, slots, count, &graph), "mapped complete graph");
    if (graph.NodeCount != nodes || graph.EdgeCount != edges || graph.ExternalRootCount != roots)
        fprintf(stderr, "map graph %zu/%zu/%zu expected %zu/%zu/%zu\n", graph.NodeCount, graph.EdgeCount, graph.ExternalRootCount, nodes, edges, roots);
    testRequire(graph.NodeCount == nodes && graph.EdgeCount == edges && graph.ExternalRootCount == roots, "exact mapped nodes/edges/roots");
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount, "exact mapped memory balance");
}
static void functional_round(unsigned mode, bool cancel, bool leave_pending)
{
    xmemdebugsnapshot before; xfuture *a, *b, *output; xpromise *pa, *pb; xfuture* inputs[3];
    xrtownershipref slots[5]; map_counts counts = {0}; map_data* data; xvalue* other;
    xrtMemDebugSnapshot(&before); pa = xrtPromiseCreate(&a, NULL); pb = xrtPromiseCreate(&b, NULL);
    testRequire(pa && pb, "mapped fixture"); inputs[0] = a; inputs[1] = a; inputs[2] = b;
    data = context_new(&counts, &other, leave_pending); output = start(mode, inputs, 3, data, other);
    testRequire(output && counts.calls == 0 && counts.drops == 0, "pending accepted context");
    slots[0] = xrtFutureOwnership(a); slots[1] = xrtFutureOwnership(b);
    slots[2] = xrtPromiseOwnership(pa); slots[3] = xrtPromiseOwnership(pb); slots[4] = xrtFutureOwnership(output);
    /* Output producer and owned CancelWatch each report their actual Group
     * reference. No external operation-base reference remains. */
    graph_check(slots, 1, slots, 5, 9, 21, 0);
    if (cancel) {
        testRequire(xrtFutureCancel(output), "cancel mapped output");
        testRequire(xrtFutureState(output) == XFUTURE_CANCELLED && counts.calls == 0 && counts.drops == 1, "cancel suppresses mapper and frees once");
        testRequire(cancel_requested(a) && cancel_requested(b), "accepted cancellation reaches sources");
    } else {
        testRequire(xrtPromiseResolve(pb, NULL), "second source first");
        if (mode == 0) testRequire(counts.calls == 0, "All still waiting");
        else testRequire(counts.calls == 1, "Pick ran exactly once");
        testRequire(xrtPromiseResolve(pa, NULL), "first source last");
        testRequire(counts.calls == 1 && counts.drops == 1, "map/drop exactly once");
        testRequire(cancel_requested(a) == (mode == 2), "only Race cancels losing source");
        testRequire(!cancel_requested(b), "winner is not cancelled");
        testRequire(xrtFutureState(output) == (leave_pending ? XFUTURE_CLOSED : XFUTURE_RESOLVED), "synchronous map output contract");
    }
    xrtPromiseDestroy(pa); xrtPromiseDestroy(pb); xrtFutureDestroy(a); xrtFutureDestroy(b);
    slots[0] = xrtFutureOwnership(output);
    graph_check(slots, 1, slots, 1, cancel || leave_pending ? 2 : 3, cancel || leave_pending ? 2 : 3, 0);
    { xfuture* alias = xrtFutureRef(output); graph_check(slots, 1, slots, 1, cancel || leave_pending ? 2 : 3, cancel || leave_pending ? 2 : 3, 1); xrtFutureDestroy(alias); }
    xrtFutureDestroy(output); balanced(&before);
}
static unsigned failure_round(unsigned mode, bool ready)
{
    unsigned failures = 0, successes = 0;
    for (unsigned point = 0; point < 16; ++point) {
        xmemdebugsnapshot before; xfuture *a, *b, *output; xpromise *pa, *pb; xfuture* inputs[3];
        xrtownershipref anchors[2], slots[4]; map_counts counts = {0}; map_data* data; xvalue* other; bool triggered;
        xrtMemDebugSnapshot(&before); pa = xrtPromiseCreate(&a, NULL); pb = xrtPromiseCreate(&b, NULL);
        testRequire(pa && pb, "failure fixture"); inputs[0] = a; inputs[1] = a; inputs[2] = b;
        data = context_new(&counts, &other, false);
        if (ready) testRequire(xrtPromiseResolve(pa, NULL) && xrtPromiseResolve(pb, NULL), "precompleted inputs");
        testRequire(xrtMemDebugFailAfter(point), "arm launch allocation failure");
        output = start(mode, inputs, 3, data, other); triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear(); xrtClearError();
        if (!output) {
            ++failures; testRequire(triggered, "NULL must have real injected failure");
            testRequire(counts.calls == 0 && counts.drops == 0 && counts.traces == 0, "failed launch consumed no callback rights");
            testRequire(!cancel_requested(a) && !cancel_requested(b), "failed launch never cancels inputs");
            testRequire(xrtFutureDone(a) == ready && xrtFutureDone(b) == ready, "failed launch preserves source state");
            anchors[0] = slots[0] = xrtFutureOwnership(a); anchors[1] = slots[1] = xrtFutureOwnership(b);
            slots[2] = xrtPromiseOwnership(pa); slots[3] = xrtPromiseOwnership(pb);
            graph_check(anchors, 2, slots, 4, 4, 6, 0); /* no hidden group, waiter, or retained source */
            drop_context(data, other);
        } else {
            ++successes;
            if (!ready) testRequire(xrtFutureCancel(output), "accepted pending launch can cancel");
            testRequire(counts.calls == (unsigned)ready && counts.drops == 1, "accepted callback ownership");
            xrtFutureDestroy(output);
        }
        xrtPromiseDestroy(pa); xrtPromiseDestroy(pb); xrtFutureDestroy(a); xrtFutureDestroy(b); balanced(&before);
    }
    testRequire(failures && successes, "cover both preparation failures and acceptance frontier"); return failures;
}
static void empty_and_invalid(void)
{
    xmemdebugsnapshot before; map_counts counts = {0}; map_data* data; xvalue* other; xfuture* output;
    xrtMemDebugSnapshot(&before); data = context_new(&counts, &other, false);
    testRequire(!xrtFutureAllMapOwnedTraced(NULL, 0, map_all, data, drop_context, other, NULL), "trace required"); xrtClearError();
    testRequire(!xrtFutureAnyMapOwnedTraced(NULL, 0, map_pick, data, drop_context, other, trace_context), "Any nonempty"); xrtClearError();
    testRequire(!xrtFutureRaceMapOwnedTraced(NULL, 1, map_pick, data, drop_context, other, trace_context), "Race valid input"); xrtClearError();
    testRequire(counts.calls == 0 && counts.drops == 0 && counts.traces == 0, "invalid request leaves context");
    output = start(0, NULL, 0, data, other);
    testRequire(output && xrtFutureState(output) == XFUTURE_RESOLVED && counts.calls == 1 && counts.drops == 1, "empty All maps once");
    xrtFutureDestroy(output); balanced(&before);
}
int main(void)
{
    unsigned actual_failures = 0; testRequire(xrtMemDebugEnable(true), "memory debug");
    for (unsigned round = 0; round < 100; ++round) {
        for (unsigned mode = 0; mode < 3; ++mode) {
            functional_round(mode, false, false); functional_round(mode, true, false); functional_round(mode, false, true);
            actual_failures += failure_round(mode, false); actual_failures += failure_round(mode, true);
        }
        empty_and_invalid();
    }
    printf("Future mapped ownership: 1000 lifecycle cases, 9600 launch probes, %u actual preparation failures, exact graphs and memory\n", actual_failures);
    return 0;
}
