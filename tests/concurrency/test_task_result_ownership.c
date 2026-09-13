#ifdef TASK_RESULT_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

/* Match the old public layouts, including offsets, rather than adding a field
 * that an old caller's stack allocation cannot contain. */
typedef struct old_task_value { ptr Value; xfuturefreeproc Destroy; ptr DestroyData; } old_task_value;
typedef struct old_task_args { xcancel* Cancel; xfuturefreeproc Destroy; ptr DestroyData; } old_task_args;
_Static_assert(sizeof(xtaskvalue) == sizeof(old_task_value), "task value ABI");
_Static_assert(offsetof(xtaskvalue, DestroyData) == offsetof(old_task_value, DestroyData), "task value offset");
_Static_assert(sizeof(xtaskargs) == sizeof(old_task_args), "task args ABI");
_Static_assert(offsetof(xtaskargs, DestroyData) == offsetof(old_task_args, DestroyData), "task args offset");

typedef struct task_context { unsigned mode, hits, data_drops; } task_context;
static unsigned payload_drops, trace_calls;
static void drop_payload(ptr value, ptr data)
{ ++payload_drops; xrtValueRelease(value); xrtValueRelease(data); }
static bool trace_payload(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{
    ++trace_calls;
    return visit(xrtValueOwnership(value), context) && visit(xrtValueOwnership(data), context);
}
static bool reject_trace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ (void)value; (void)data; (void)visit; (void)context; return false; }
static void drop_data(ptr value, ptr data)
{ (void)data; ++((task_context*)value)->data_drops; }
static xtaskoutcome run_task(xcancel* cancel, ptr data, xtaskvalue* result)
{
    task_context* context = data; xvalue* value;
    (void)cancel; ++context->hits;
    if (context->mode == 4) { result->Value = context; return XTASK_SUCCESS; }
    value = xrtValueInt(42000); testRequire(value != NULL, "task payload allocation");
    result->Value = value; result->Destroy = drop_payload;
    result->DestroyData = xrtValueRetain(value); testRequire(result->DestroyData == value, "separate context owner");
    if (context->mode == 2) { xrtSetErrorKind(XERR_INTERNAL); return XTASK_FAILED; }
    if (context->mode == 3) return XTASK_CANCELLED;
    return XTASK_SUCCESS;
}
static void rejected_graph(xfuture* future)
{
    xrtownershipref root = xrtFutureOwnership(future);
    xrtownershipresult old = {11,22,33,44}, graph = old; bool live = true;
    testRequire(!xrtOwnershipInspectReachable(&root, 1, &root, 1, &live, &graph, NULL, NULL), "opaque result rejected");
    testRequire(live && memcmp(&old, &graph, sizeof(old)) == 0, "rejection output atomic");
    xrtClearError();
}
static void round_trip(void)
{
    xtaskpoolconfig config = {1,32,0}; xtaskpool* pool = xrtTaskPoolCreate(&config);
    task_context contexts[7] = {{0}}, rejected = {0}; xfuture* futures[7];
    xtaskargs args = {0}; xcancel* cancel = xrtCancelCreate();
    unsigned mode, drops_before = payload_drops, traces_before = trace_calls;
    testRequire(pool != NULL && cancel != NULL, "pool and parent");
    args.Destroy = drop_data;
    testRequire(!xrtTaskSubmitTraced(pool, run_task, &rejected, &args, NULL), "trace required before acceptance");
    xrtClearError();
    testRequire(xrtCancelRequest(cancel), "pre-cancel parent");
    for (mode = 0; mode < 7; ++mode) {
        contexts[mode].mode = mode; args.Cancel = mode == 5 ? cancel : NULL;
        futures[mode] = mode == 1 ? xrtTaskSubmit(pool, run_task, &contexts[mode], &args) :
            xrtTaskSubmitTraced(pool, run_task, &contexts[mode], &args, mode == 6 ? reject_trace : trace_payload);
        testRequire(futures[mode] != NULL, "submit");
    }
    /* Join is stronger than Future-ready or idle: all worker releases returned. */
    testRequire(xrtTaskPoolDestroy(pool), "join workers before graph inspection");
    xrtCancelDestroy(cancel);
    testRequire(trace_calls == traces_before, "submission/publication never invokes result trace");
    testRequire(payload_drops == drops_before + 2, "failed/cancelled staged results released");
    testRequire(rejected.hits == 0 && rejected.data_drops == 0, "rejected data not consumed");
    for (mode = 0; mode < 7; ++mode) {
        xrtownershipref root = xrtFutureOwnership(futures[mode]);
        xrtownershipresult graph = {0}; bool live = true;
        testRequire(contexts[mode].data_drops == 1 && contexts[mode].hits == (mode == 5 ? 0u : 1u), "exact task execution/data cleanup");
        testRequire(xrtFutureState(futures[mode]) == (mode == 2 ? XFUTURE_FAILED :
            (mode == 3 || mode == 5) ? XFUTURE_CANCELLED : XFUTURE_RESOLVED), "terminal outcome preserved");
        if (mode == 1 || mode == 6) rejected_graph(futures[mode]);
        else {
            testRequire(xrtOwnershipInspectReachable(&root, 1, &root, 1, &live, &graph, NULL, NULL), "inspect settled task result");
            testRequire(!live && graph.ExternalRootCount == 0, "no hidden producer/task roots after join");
            if (mode == 0) testRequire(graph.NodeCount == 3 && graph.EdgeCount == 4, "payload/context duplicate edges");
            testRequire(xrtFutureRef(futures[mode]) == futures[mode], "native Future alias");
            testRequire(xrtOwnershipInspectReachable(&root, 1, &root, 1, &live, &graph, NULL, NULL) && live && graph.ExternalRootCount == 1,
                "native Retain remains an external root");
            xrtFutureDestroy(futures[mode]);
        }
        xrtFutureDestroy(futures[mode]);
    }
    testRequire(payload_drops == drops_before + 5, "all five owned results dropped exactly once");
    pool = xrtTaskPoolCreate(&config); testRequire(pool != NULL, "rejection pool");
    args.Cancel = NULL;
    testRequire(xrtTaskPoolClose(pool), "close rejection pool");
    testRequire(!xrtTaskSubmitTraced(pool, run_task, &rejected, &args, trace_payload), "closed submission rejected");
    xrtClearError();
    testRequire(xrtMemDebugFailAfter(0), "arm allocation failure");
    testRequire(!xrtTaskSubmitTraced(pool, run_task, &rejected, &args, trace_payload), "job allocation failure");
    testRequire(xrtMemDebugFailTriggered(), "actual allocation failure"); xrtMemDebugFailClear(); xrtClearError();
    testRequire(xrtTaskPoolDestroy(pool), "destroy closed pool");
    testRequire(rejected.hits == 0 && rejected.data_drops == 0, "all submit failures preserve caller data");
}
int main(void)
{
    unsigned round; testRequire(xrtMemDebugEnable(true), "memory debug");
    for (round = 0; round < 100; ++round) {
        xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before); round_trip();
        testRequire(xrtGetError() == NULL, "clean host error"); xrtMemDebugSnapshot(&after);
        testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount,
            "exact task graph memory balance");
    }
    puts("task result ownership: 700 outcomes, 300 rejected submissions including 100 actual OOMs, aliases and exact memory balance");
    return 0;
}
