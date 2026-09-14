#ifndef XRT_MODULE_FUTURE_COMBINE
#define XRT_MODULE_FUTURE_COMBINE
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifdef FUTURE_COMBINE_PHASES_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include "../test_thread.h"

typedef struct work {
    volatile int32 refs;
    xfuture *a, *b, *output;
    xpromise *pa, *pb;
    xvalue* value;
    xatomic32 entered, proceed, drop_entered, drop_proceed, start;
    unsigned maps, drops;
    bool block_map, block_drop;
} work;
static void wait_for(xatomic32* state)
{ while (!xrtAtomic32Load(state, XMEMORY_ACQUIRE)) testThreadYield(); }
static void freeze_begin(xrtownershipscope* freeze)
{
    for (unsigned i = 0; i < 1000000; ++i) { if (xrtOwnershipFreezeTryBegin(freeze)) return; testThreadYield(); }
    testRequire(false, "quiescent callback phase permits freeze");
}
static bool count_work(const void* data, size_t* count)
{ const work* value = data; if (!value || !count || value->refs <= 0) return false; *count = (size_t)value->refs; return true; }
static bool trace_work(const void* data, xrtownershipvisitor visit, ptr context)
{ return visit(xrtValueOwnership(((const work*)data)->value), context); }
static const xrtownershipops work_ops = {count_work, trace_work};
static void drop_work(const void* data)
{
    work* value = (work*)data; xrtownershipscope mutation = {0};
    xrtAtomic32Store(&value->drop_entered, 1, XMEMORY_RELEASE);
    if (value->block_drop) wait_for(&value->drop_proceed);
    testRequire(xrtOwnershipMutationBegin(&mutation), "mapper Drop owns its mutation transition");
    testRequire(xrtRefRelease(&value->refs) == 1, "group returns one physical context reference"); ++value->drops;
    testRequire(xrtOwnershipScopeEnd(&mutation), "mapper Drop transition end");
}
static void publish(xpromise* output, work* value)
{
    xrtownershipscope mutation = {0}; ++value->maps;
    xrtAtomic32Store(&value->entered, 1, XMEMORY_RELEASE);
    if (value->block_map) wait_for(&value->proceed);
    testRequire(xrtOwnershipMutationBegin(&mutation), "mapped callback may reenter after external freeze");
    testRequire(xrtValueRetain(value->value) == value->value, "mapper Data remains alive"); xrtValueRelease(value->value);
    testRequire(xrtOwnershipScopeEnd(&mutation), "mapper mutation end");
    testRequire(xrtPromiseResolve(output, NULL), "committed mapper owns completion despite later cancellation request");
}
static void map_all(const xfutureall* input, xpromise* output, ptr data)
{
    testRequire(input && input->Count == 3 && input->Futures[0] == input->Futures[1], "concurrent All duplicate inputs");
    for (size_t i = 0; i < input->Count; ++i) testRequire(xrtFutureDone(input->Futures[i]), "All callback only after every input terminal");
    publish(output, data);
}
static void map_pick(const xfuturepick* input, xpromise* output, ptr data)
{ testRequire(input && input->Index < 3 && xrtFutureDone(input->Future), "Pick actual terminal winner"); publish(output, data); }
static const xfuturecombineownershipv1 policy = {sizeof(policy), map_all, map_pick, drop_work, &work_ops};
typedef struct held_group { xrtownershipref ref; const xrtownershipadapterv1* adapter; } held_group;
static bool find_group(xrtownershipref ref, ptr data)
{
    held_group* result = data; const xfuturecombineownershipv1* policies[] = {&policy};
    const xrtownershippreparationv1* preparation = NULL;
    const xrtownershipadapterv1* adapter = xrtFutureCombineOwnershipAdapterV1(ref, policies, 1, &preparation);
    if (adapter) { testRequire(!result->ref.Data && adapter->Hold(ref.Data), "hold the actual producer, not callback storage"); result->ref = ref; result->adapter = adapter; }
    return true;
}
static work* create(unsigned mode)
{
    work* value = xrtCalloc(1, sizeof(*value)); xfuture* inputs[3]; testRequire(value != NULL, "concurrent mapper context");
    value->refs = 1; value->value = xrtValueInt(57001);
    value->pa = xrtPromiseCreate(&value->a, NULL); value->pb = xrtPromiseCreate(&value->b, NULL);
    testRequire(value->pa && value->pb && value->value, "concurrent fixture endpoints");
    inputs[0] = inputs[1] = value->a; inputs[2] = value->b;
    xrtAtomic32Init(&value->entered, 0); xrtAtomic32Init(&value->proceed, 0); xrtAtomic32Init(&value->drop_entered, 0);
    xrtAtomic32Init(&value->drop_proceed, 0); xrtAtomic32Init(&value->start, 0);
    testRequire(xrtRefRetain(&value->refs) == 2, "reserve actual transferred map reference");
    value->output = mode == 0 ? xrtFutureAllMapOwnedPolicyV1(inputs, 3, value, &policy) :
        mode == 1 ? xrtFutureAnyMapOwnedPolicyV1(inputs, 3, value, &policy) : xrtFutureRaceMapOwnedPolicyV1(inputs, 3, value, &policy);
    testRequire(value->output != NULL, "accepted concurrent aggregate"); return value;
}
static void finish(work* value, const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    testRequire(value->refs == 1 && value->drops == 1, "native group relinquished Data once");
    xrtPromiseDestroy(value->pa); xrtPromiseDestroy(value->pb); xrtFutureDestroy(value->a); xrtFutureDestroy(value->b); xrtFutureDestroy(value->output);
    testRequire(xrtRefRelease(&value->refs) == 0, "host returns final actual Data reference"); xrtValueRelease(value->value); xrtFree(value);
    testRequire(xrtGetError() == NULL, "no ambient thread diagnostic"); xrtMemDebugSnapshot(&after);
    testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount, "concurrent aggregate exact memory balance");
}
static int resolve_a(ptr data)
{ work* value = data; wait_for(&value->start); testRequire(xrtPromiseResolve(value->pa, NULL), "concurrent first input"); return 0; }
static int resolve_b(ptr data)
{ work* value = data; wait_for(&value->start); testRequire(xrtPromiseResolve(value->pb, NULL), "concurrent second input"); return 0; }
static int cancel_output(ptr data)
{
    work* value = data; bool accepted; wait_for(&value->start); accepted = xrtFutureCancel(value->output);
    /* Completion may win before FutureCancel takes its lock. That is a
     * refused request, not a product failure or an invented cancellation. */
    if (!accepted) testRequire(xrtFutureState(value->output) == XFUTURE_RESOLVED && !xrtGetError(), "late cancellation is refused only after real completion");
    else {
        xcancel* token = xrtFutureCancelToken(value->output);
        testRequire(token && xrtCancelRequested(token), "accepted cancel records a real token request"); xrtCancelDestroy(token);
    }
    return 0;
}
static void blocked_mapper(unsigned mode)
{
    work* value; xmemdebugsnapshot before; testthread thread = {0}; xrtownershipscope freeze = {0}; held_group group = {0};
    const xfuturecombineownershipv1* policies[] = {&policy};
    const xrtownershippreparationv1* prep = (const xrtownershippreparationv1*)(uintptr_t)1;
    xrtMemDebugSnapshot(&before); value = create(mode); value->block_map = true;
    freeze_begin(&freeze);
    { xrtownershipref output = xrtFutureOwnership(value->output); testRequire(output.Ops->Trace(output.Data, find_group, &group) && group.ref.Data, "actual output producer pin"); }
    testRequire(xrtOwnershipScopeEnd(&freeze), "producer pin scope end");
    if (!mode) testRequire(xrtPromiseResolve(value->pa, NULL), "All still waits for second input");
    thread.Proc = resolve_b; thread.Data = value; xrtAtomic32Store(&value->start, 1, XMEMORY_RELEASE);
    testThreadsStart(&thread, 1); wait_for(&value->entered); freeze_begin(&freeze);
    testRequire(!xrtFutureCombineOwnershipAdapterV1(group.ref, policies, 1, &prep) && prep == (const xrtownershippreparationv1*)(uintptr_t)1,
        "running mapped producer refuses inspection before user trace");
    testRequire(value->maps == 1 && !value->drops && value->refs == 2 && !xrtFutureDone(value->output), "callback owns live context and output completion");
    testRequire(xrtOwnershipScopeEnd(&freeze), "active mapped phase end");
    testRequire(xrtFutureCancel(value->output) && !xrtFutureDone(value->output), "later cancel cannot replace committed mapper");
    xrtAtomic32Store(&value->proceed, 1, XMEMORY_RELEASE); testThreadsJoin(&thread, 1);
    testRequire(thread.Result == 0 && xrtFutureState(value->output) == XFUTURE_RESOLVED && !value->drops,
        "actual group pin preserves mapper owner through callback release tail");
    group.adapter->Drop(group.ref.Data);
    testRequire(value->maps == 1 && value->drops == 1, "releasing final group pin returns Data once"); finish(value, &before);
}
static void blocked_drop(unsigned mode)
{
    work* value; xmemdebugsnapshot before; testthread thread = {0}; xrtownershipscope freeze = {0};
    xrtMemDebugSnapshot(&before); value = create(mode); value->block_drop = true;
    if (!mode) testRequire(xrtPromiseResolve(value->pa, NULL), "Drop case All first input");
    thread.Proc = resolve_b; thread.Data = value; xrtAtomic32Store(&value->start, 1, XMEMORY_RELEASE);
    testThreadsStart(&thread, 1); wait_for(&value->drop_entered); freeze_begin(&freeze);
    testRequire(value->refs == 2 && value->maps == 1 && !value->drops && xrtFutureState(value->output) == XFUTURE_RESOLVED,
        "Drop tail retains its real Data slot while native APIs permit freeze");
    testRequire(xrtOwnershipScopeEnd(&freeze), "Data Drop phase end");
    xrtAtomic32Store(&value->drop_proceed, 1, XMEMORY_RELEASE); testThreadsJoin(&thread, 1);
    testRequire(thread.Result == 0, "Data Drop thread joined"); finish(value, &before);
}
static void completion_race(unsigned mode, bool cancel)
{
    work* value; xmemdebugsnapshot before; testthread threads[3] = {{0}}; xfuturestate result;
    xrtMemDebugSnapshot(&before); value = create(mode);
    threads[0].Proc = resolve_a; threads[1].Proc = resolve_b; threads[2].Proc = cancel_output;
    for (unsigned i = 0; i < 3; ++i) threads[i].Data = value;
    testThreadsStart(threads, cancel ? 3 : 2); xrtAtomic32Store(&value->start, 1, XMEMORY_RELEASE); testThreadsJoin(threads, cancel ? 3 : 2);
    result = xrtFutureState(value->output);
    testRequire(result == XFUTURE_RESOLVED || (cancel && result == XFUTURE_CANCELLED), "source/cancel race produces a real terminal outcome");
    testRequire(value->maps == (unsigned)(result == XFUTURE_RESOLVED) && value->drops == 1, "one linearized mapper or cancellation and one context return");
    testRequire(xrtFutureState(value->a) == XFUTURE_RESOLVED && xrtFutureState(value->b) == XFUTURE_RESOLVED, "cancel never forges losing source terminal states");
    for (unsigned i = 0; i < (cancel ? 3u : 2u); ++i) testRequire(threads[i].Result == 0, "actual competing threads joined");
    finish(value, &before);
}
int main(void)
{
    for (unsigned i = 0; i < 32; ++i) for (unsigned mode = 0; mode < 3; ++mode) { blocked_mapper(mode); blocked_drop(mode); }
    for (unsigned i = 0; i < 64; ++i) for (unsigned mode = 0; mode < 3; ++mode) { completion_race(mode, false); completion_race(mode, true); }
    puts("Combine phases: 96 blocked mappers, 96 actual Drop tails, 384 two/three-thread source and cancellation races");
    testMemoryDebugDrain("combine phases drain"); return 0;
}
