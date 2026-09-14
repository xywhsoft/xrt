#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
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
#ifdef CANCEL_OWNERSHIP_THREADS_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include "../test_thread.h"

typedef struct work {
    volatile int32 refs;
    xcancel *parent, *child;
    xcancelwatch* watch;
    xvalue* value;
    xatomic32 entered, proceed, removing, removed, drop_entered, drop_proceed;
    unsigned calls, drops;
    bool block_notify, block_drop;
} work;
static void wait_for(xatomic32* value)
{ while (!xrtAtomic32Load(value, XMEMORY_ACQUIRE)) testThreadYield(); }
static bool count_work(const void* data, size_t* count)
{ const work* value = data; if (!value || !count || value->refs <= 0) return false; *count = (size_t)value->refs; return true; }
static bool trace_work(const void* data, xrtownershipvisitor visit, ptr context)
{ return visit(xrtValueOwnership(((const work*)data)->value), context); }
static const xrtownershipops work_ops = {count_work, trace_work};
static void notify_work(ptr data)
{
    work* value = data; xrtownershipscope mutation = {0}; ++value->calls;
    xrtAtomic32Store(&value->entered, 1, XMEMORY_RELEASE);
    if (value->block_notify) wait_for(&value->proceed);
    testRequire(xrtOwnershipMutationBegin(&mutation), "callback nested mutation after freeze");
    testRequire(xrtValueRetain(value->value) == value->value, "live callback data");
    xrtValueRelease(value->value); testRequire(xrtOwnershipScopeEnd(&mutation), "callback mutation end");
}
static void drop_work(const void* data)
{
    work* value = (work*)data; xrtownershipscope mutation = {0};
    xrtAtomic32Store(&value->drop_entered, 1, XMEMORY_RELEASE);
    if (value->block_drop) wait_for(&value->drop_proceed);
    testRequire(xrtOwnershipMutationBegin(&mutation), "Drop owns its transition");
    testRequire(xrtRefRelease(&value->refs) == 1, "watch returns one reference, host still owns Data"); ++value->drops;
    testRequire(xrtOwnershipScopeEnd(&mutation), "Drop mutation end");
}
static const xcancelwatchownershipv1 policy = {sizeof(policy), notify_work, drop_work, &work_ops};
static int request_parent(ptr data) { testRequire(xrtCancelRequest(((work*)data)->parent), "first parent request"); return 0; }
static int request_child(ptr data) { testRequire(xrtCancelRequest(((work*)data)->child), "first child request"); return 0; }
static int remove_watch(ptr data)
{
    work* value = data; xrtAtomic32Store(&value->removing, 1, XMEMORY_RELEASE);
    xrtCancelUnwatch(value->watch); xrtAtomic32Store(&value->removed, 1, XMEMORY_RELEASE); return 0;
}
static void init(work* value, bool owned)
{
    memset(value, 0, sizeof(*value)); value->refs = owned ? 2 : 1;
    value->parent = xrtCancelCreate(); value->child = xrtCancelChild(value->parent); value->value = xrtValueInt(49000);
    testRequire(value->parent && value->child && value->value, "thread fixture");
    xrtAtomic32Init(&value->entered, 0); xrtAtomic32Init(&value->proceed, 0);
    xrtAtomic32Init(&value->removing, 0); xrtAtomic32Init(&value->removed, 0);
    xrtAtomic32Init(&value->drop_entered, 0); xrtAtomic32Init(&value->drop_proceed, 0);
    value->watch = owned ? xrtCancelWatchOwnedV1(value->child, value, &policy) :
        xrtCancelWatch(value->child, notify_work, value);
    testRequire(value->watch != NULL, "thread registration");
}
static void finish(work* value, const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    testRequire(value->refs == 1, "only host reference remains");
    testRequire(xrtRefRelease(&value->refs) == 0, "actual final Data reference");
    xrtValueRelease(value->value); xrtCancelDestroy(value->child); xrtCancelDestroy(value->parent);
    testRequire(xrtGetError() == NULL, "no thread diagnostic"); xrtMemDebugSnapshot(&after);
    testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount,
        "concurrent cancellation exact allocation balance");
}
static void blocked(bool owned)
{
    work value; xmemdebugsnapshot before; testthread threads[2] = {{0}}; xrtownershipscope freeze = {0};
    const xcancelwatchownershipv1* policies[1] = {&policy};
    const xrtownershippreparationv1* prepare = (const xrtownershippreparationv1*)(uintptr_t)1;
    bool detached = false;
    xrtMemDebugSnapshot(&before); init(&value, owned); value.block_notify = true;
    threads[0].Proc = request_parent; threads[0].Data = &value;
    threads[1].Proc = remove_watch; threads[1].Data = &value;
    testThreadsStart(threads, 1); wait_for(&value.entered);
    if (owned) {
        testRequire(xrtOwnershipFreezeTryBegin(&freeze), "owned callback does not monopolize mutation domain");
        testRequire(!xrtCancelWatchOwnershipAdapterV1(xrtCancelWatchOwnership(value.watch), policies, 1, &prepare) &&
            prepare == (const xrtownershippreparationv1*)(uintptr_t)1, "active Watch is refused before tracing");
        testRequire(!xrtCancelOwnershipAdapterV2(xrtCancelOwnership(value.parent), policies, 1, &prepare), "active native request refused");
        testRequire(xrtOwnershipScopeEnd(&freeze), "active observation end");
    } else testRequire(!xrtOwnershipFreezeTryBegin(&freeze), "legacy callback keeps conservative isolation");
    testThreadsStart(&threads[1], 1); wait_for(&value.removing);
    if (owned) {
        /* Parent dispatch already removed its list. Child still had a list:
         * V1 admission proves Unwatch really unlinked it before blocking. */
        for (unsigned spin = 0; spin < 1000000 && !detached; ++spin) {
            if (xrtOwnershipFreezeTryBegin(&freeze)) {
                detached = xrtCancelOwnershipAdapterV1(xrtCancelOwnership(value.child)) != NULL;
                testRequire(!xrtAtomic32Load(&value.removed, XMEMORY_ACQUIRE), "Unwatch waits for the actual callback");
                testRequire(xrtOwnershipScopeEnd(&freeze), "Unwatch observation end");
            }
            if (!detached) testThreadYield();
        }
        testRequire(detached, "blocking Unwatch owns no mutation scope");
    } else {
        for (unsigned spin = 0; spin < 1000; ++spin) testThreadYield();
        testRequire(!xrtOwnershipFreezeTryBegin(&freeze), "legacy executing callback still excludes freeze");
    }
    xrtAtomic32Store(&value.proceed, 1, XMEMORY_RELEASE); testThreadsJoin(threads, 2);
    testRequire(value.calls == 1 && value.drops == (unsigned)owned && threads[0].Result == 0 && threads[1].Result == 0,
        "exact blocked callback/Drop result"); finish(&value, &before);
}
static void blocked_drop(void)
{
    work value; xmemdebugsnapshot before; testthread thread = {0}; xrtownershipscope freeze = {0};
    xrtMemDebugSnapshot(&before); init(&value, true); value.block_drop = true;
    thread.Proc = remove_watch; thread.Data = &value; testThreadsStart(&thread, 1); wait_for(&value.drop_entered);
    testRequire(xrtOwnershipFreezeTryBegin(&freeze), "owned Drop is outside API mutation");
    testRequire(!xrtAtomic32Load(&value.removed, XMEMORY_ACQUIRE) && value.refs == 2 && value.drops == 0,
        "Drop tail holds its actual Data reference until return");
    testRequire(xrtCancelOwnershipAdapterV1(xrtCancelOwnership(value.child)) != NULL, "real unwatch precedes Data release");
    testRequire(xrtOwnershipScopeEnd(&freeze), "Drop observation end");
    xrtAtomic32Store(&value.drop_proceed, 1, XMEMORY_RELEASE); testThreadsJoin(&thread, 1);
    testRequire(value.calls == 0 && value.drops == 1 && thread.Result == 0, "Drop does not invent Notify"); finish(&value, &before);
}
static void ancestor_race(bool remove)
{
    work value; xmemdebugsnapshot before; testthread threads[3] = {{0}};
    xrtMemDebugSnapshot(&before); init(&value, true);
    threads[0].Proc = request_parent; threads[0].Data = &value;
    threads[1].Proc = request_child; threads[1].Data = &value;
    threads[2].Proc = remove_watch; threads[2].Data = &value;
    testThreadsStart(threads, remove ? 3 : 2); testThreadsJoin(threads, remove ? 3 : 2);
    if (!remove) xrtCancelUnwatch(value.watch);
    testRequire(value.calls <= 1 && (remove || value.calls == 1) && value.drops == 1, "parent/child/unwatch linearization");
    for (unsigned i = 0; i < (remove ? 3u : 2u); ++i) testRequire(threads[i].Result == 0, "joined request/unwatch");
    finish(&value, &before);
}
int main(void)
{
    for (unsigned i = 0; i < 64; ++i) { blocked(true); blocked(false); blocked_drop(); }
    for (unsigned i = 0; i < 128; ++i) { ancestor_race(false); ancestor_race(true); }
    puts("Cancel phases: 64 owned callbacks, 64 legacy callbacks, 64 Drop tails, 256 ancestor/unwatch races");
    testMemoryDebugDrain("cancel phases drain"); return 0;
}
