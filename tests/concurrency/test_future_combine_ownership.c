#ifndef XRT_MODULE_FUTURE_COMBINE
#define XRT_MODULE_FUTURE_COMBINE
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef FUTURE_COMBINE_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

_Static_assert(sizeof(xfuturewatch) == 64, "projected Watch preserves storage ABI");
typedef struct events { unsigned maps, drops, projections; } events;
typedef struct context { volatile int32 refs; xvalue* value; events* counts; const void* claim; } context;
static void outside_scope(void)
{ xrtownershipscope freeze = {0}; testRequire(xrtOwnershipFreezeTryBegin(&freeze), "certified map/Drop has no API scope"); testRequire(xrtOwnershipScopeEnd(&freeze), "map/Drop scope end"); }
static bool context_count(const void* data, size_t* count)
{ const context* ctx = data; if (!ctx || !count || ctx->refs <= 0) return false; *count = (size_t)ctx->refs; return true; }
static bool context_trace(const void* data, xrtownershipvisitor visit, ptr opaque)
{ return visit(xrtValueOwnership(((const context*)data)->value), opaque); }
static const xrtownershipops context_ops = {context_count, context_trace};
static bool context_hold(const void* data) { return xrtRefRetain(&((context*)data)->refs) > 0; }
static void context_drop(const void* data)
{
    context* ctx = (context*)data; xrtownershipscope scope = {0};
    outside_scope(); testRequire(xrtOwnershipMutationBegin(&scope), "context drop transition");
    if (!xrtRefRelease(&ctx->refs)) { ++ctx->counts->drops; xrtValueRelease(ctx->value); xrtFree(ctx); }
    testRequire(xrtOwnershipScopeEnd(&scope), "context drop end");
}
static bool context_claim(const void* data, const void* token)
{ context* ctx = (context*)data; if (!token || (ctx->claim && ctx->claim != token)) return false; ctx->claim = token; return true; }
static void context_restore(const void* data, const void* token)
{ context* ctx = (context*)data; testRequire(ctx->claim == token, "context claim identity"); ctx->claim = NULL; }
static void context_clear(const void* data, const void* token) { (void)data; (void)token; testRequire(false, "test context is restored, never cleared"); }
static const xrtownershipadapterv1 context_adapter = {sizeof(context_adapter), context_hold, context_drop, context_claim, context_restore, NULL, context_clear, NULL};
static context* create_context(events* counts)
{
    context* ctx = xrtCalloc(1, sizeof(*ctx)); testRequire(ctx != NULL, "mapper context");
    ctx->refs = 1; ctx->counts = counts; ctx->value = xrtValueInt(44000); testRequire(ctx->value != NULL, "mapper owned value"); return ctx;
}
static void map_all(const xfutureall* input, xpromise* output, ptr data)
{
    context* ctx = data; outside_scope(); ++ctx->counts->maps;
    testRequire(input && (input->Count == 0 || (input->Count == 3 && input->Futures[0] == input->Futures[1])), "ordered duplicate All inputs");
    for (size_t i = 0; i < input->Count; ++i) testRequire(xrtFutureDone(input->Futures[i]), "ALL really requires every terminal input");
    testRequire(xrtPromiseResolve(output, NULL), "mapped output");
}
static void map_pick(const xfuturepick* input, xpromise* output, ptr data)
{
    context* ctx = data; outside_scope(); ++ctx->counts->maps;
    testRequire(input && input->Index == 2 && xrtFutureDone(input->Future), "ANY preserves the actual winning slot");
    testRequire(xrtPromiseResolve(output, NULL), "mapped pick");
}
static const xfuturecombineownershipv1 map_policy = {sizeof(map_policy), map_all, map_pick, context_drop, &context_ops};
static void drop_future(ptr value, ptr data) { testRequire(data == NULL, "owned Future payload has no extra Data"); xrtFutureDestroy(value); }
static bool trace_future(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return data == NULL && visit(xrtFutureOwnership(value), context); }
static const xfuturepayloadownershipv1 future_policy = {sizeof(future_policy), drop_future, trace_future};
static const xrtownershipadapterv1* resolve(xrtownershipref ref, const xrtownershippreparationv1** preparation)
{
    const xfuturepayloadownershipv1* payloads[] = {xrtFutureCombineAllPayloadPolicyV1Get(), xrtFutureCombinePickPayloadPolicyV1Get(), &future_policy};
    const xfutureproducerownershipv1* producers[] = {xrtFutureCombineProducerPolicyV1Get()};
    const xfuturewatchownershipv2* watches[] = {xrtFutureCombineWatchPolicyV2Get()};
    const xcancelwatchownershipv1* cancels[] = {xrtFutureCombineCancelPolicyV1Get()};
    const xfuturecombineownershipv1* maps[] = {&map_policy};
    const xfutureownershipadmissionv1 admission = {sizeof(admission), payloads, 3, producers, 1, NULL, 0, watches, 1, cancels, 1};
    const xrtownershipadapterv1* adapter; *preparation = NULL;
    if (ref.Ops == &context_ops) return &context_adapter;
    if ((adapter = xrtFutureCombineOwnershipAdapterV1(ref, maps, 1, preparation)) != NULL) return adapter;
    if ((adapter = xrtCancelWatchOwnershipAdapterV1(ref, cancels, 1, preparation)) != NULL) return adapter;
    if ((adapter = xrtCancelOwnershipAdapterV2(ref, cancels, 1, preparation)) != NULL) return adapter;
    if ((adapter = xrtFutureOwnershipAdapterV4(ref, &admission, preparation)) != NULL) return adapter;
    return xrtValueOwnershipAdapterV1(ref);
}
static bool admit(xrtownershipref ref, ptr unused)
{ const xrtownershippreparationv1* preparation; (void)unused; return resolve(ref, &preparation) != NULL; }
static void view(xfuture* output, xrtownershipref* slots, bool mapped, unsigned mode, bool rooted)
{
    xrtownershipscope freeze = {0}; xrtownershipsnapshot* snapshot = NULL; xrtownershipnode node;
    xrtownershipref anchor = xrtFutureOwnership(output), refs[16], group = {0};
    const xrtownershipadapterv1* adapters[16]; const xrtownershippreparationv1* preparations[16];
    const xfuturecombineownershipv1* maps[] = {&map_policy}; size_t n; unsigned token = 0, other = 1;
    testRequire(xrtOwnershipFreezeTryBegin(&freeze), "whole aggregate freeze");
    testRequire(xrtOwnershipSnapshotCreate(&anchor, 1, slots, 5, admit, NULL, &snapshot), "independent admission of every physical node");
    n = xrtOwnershipSnapshotNodeCount(snapshot); testRequire(n == (mapped ? 10u : 8u), "no alias or fictitious input nodes");
    for (size_t i = 0; i < n; ++i) {
        testRequire(xrtOwnershipSnapshotNode(snapshot, i, &node) && node.Reachable == rooted, "real external output alias controls reachability");
        refs[i] = node.Reference; adapters[i] = resolve(refs[i], &preparations[i]);
        if (xrtFutureCombineOwnershipAdapterV1(refs[i], maps, 1, &preparations[i])) group = refs[i];
        if (!rooted) testRequire(adapters[i]->Hold(refs[i].Data) && adapters[i]->Claim(refs[i].Data, &token), "whole unreachable graph pinned and claimed");
    }
    testRequire(group.Data != NULL, "actual output producer is in graph");
    if (!rooted) {
        xfuturecombinewaitv1 wait = {0}, unchanged;
        testRequire(xrtFutureCombineWaitV1(group, &token, &wait), "certified stable multi-input wait");
        testRequire(wait.size == sizeof(wait) && wait.Count == 3 && wait.Sources[0] == wait.Sources[1] &&
            xrtPromiseOwnership(wait.Output).Data == anchor.Data &&
            wait.Rule == (mode == 0 ? XFUTURE_WAIT_ALL_TERMINAL : XFUTURE_WAIT_ANY_TERMINAL) &&
            wait.CancelRemaining == (mode == 2), "exact output, all/any rule and Race-only cancellation");
        unchanged = wait; testRequire(!xrtFutureCombineWaitV1(group, &other, &wait) && !memcmp(&wait, &unchanged, sizeof(wait)), "foreign claim refusal is atomic");
        {
            const xrtownershippreparationv1* preparation = NULL;
            const xrtownershippreparationv1* sentinel = (const xrtownershippreparationv1*)(uintptr_t)1;
            testRequire(xrtFutureCombineOwnershipAdapterV1(group, maps, 1, &preparation) != NULL, "claimed group still admitted");
            if (mapped) testRequire(!xrtFutureCombineOwnershipAdapterV1(group, NULL, 0, &sentinel) &&
                sentinel == (const xrtownershippreparationv1*)(uintptr_t)1, "unrecognized mapper is not automatically certified");
            testRequire(xrtOwnershipScopeEnd(&freeze), "Prepare runs outside freeze");
            testRequire(preparation->Prepare(group.Data, &token) == XRT_OWNERSHIP_PREPARE_BUSY && !xrtFutureDone(output),
                "unreachable pending aggregate is not permission to cancel or skip accepted inputs");
            testRequire(xrtOwnershipFreezeTryBegin(&freeze), "restore freeze after semantic preparation");
        }
        for (size_t i = 0; i < n; ++i) adapters[i]->Restore(refs[i].Data, &token);
    }
    xrtOwnershipSnapshotDestroy(snapshot); testRequire(xrtOwnershipScopeEnd(&freeze), "restore complete graph before releasing pins");
    if (!rooted) for (size_t i = 0; i < n; ++i) adapters[i]->Drop(refs[i].Data);
}
static bool requested(xfuture* future)
{ xcancel* token = xrtFutureCancelToken(future); bool result = xrtCancelRequested(token); xrtCancelDestroy(token); return result; }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    testRequire(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount, "aggregate exact memory balance");
}
static xfuture* start(unsigned mode, bool mapped, xfuture** inputs, context* ctx)
{
    if (mapped) return mode == 0 ? xrtFutureAllMapOwnedPolicyV1(inputs, 3, ctx, &map_policy) :
        mode == 1 ? xrtFutureAnyMapOwnedPolicyV1(inputs, 3, ctx, &map_policy) : xrtFutureRaceMapOwnedPolicyV1(inputs, 3, ctx, &map_policy);
    return mode == 0 ? xrtFutureAll(inputs, 3) : mode == 1 ? xrtFutureAny(inputs, 3) : xrtFutureRace(inputs, 3);
}
static void round_trip(unsigned mode, bool mapped, bool cancel, bool ready)
{
    events counts = {0}; context* ctx = NULL; xmemdebugsnapshot before; xfuture *a, *b, *output, *alias;
    xpromise *pa, *pb; xfuture* inputs[3]; xrtownershipref slots[5];
    xrtMemDebugSnapshot(&before); pa = xrtPromiseCreate(&a, NULL); pb = xrtPromiseCreate(&b, NULL);
    testRequire(pa && pb, "input endpoints"); inputs[0] = a; inputs[1] = a; inputs[2] = b;
    if (ready) { testRequire(xrtPromiseResolve(pb, NULL), "ready winner"); if (!mode) testRequire(xrtPromiseResolve(pa, NULL), "ready All"); }
    if (mapped) ctx = create_context(&counts);
    output = start(mode, mapped, inputs, ctx); testRequire(output != NULL, "aggregate accepted");
    slots[0] = xrtFutureOwnership(a); slots[1] = xrtFutureOwnership(b); slots[2] = xrtPromiseOwnership(pa);
    slots[3] = xrtPromiseOwnership(pb); slots[4] = xrtFutureOwnership(output);
    if (!ready) {
        alias = xrtFutureRef(output); view(output, slots, mapped, mode, true); xrtFutureDestroy(alias);
        view(output, slots, mapped, mode, false);
        if (cancel) {
            testRequire(xrtFutureCancel(output) && xrtFutureState(output) == XFUTURE_CANCELLED, "real output cancellation");
            testRequire(requested(a) && requested(b) && !xrtFutureDone(a) && !xrtFutureDone(b), "cancel requests do not forge input terminal states");
        } else {
            testRequire(xrtPromiseResolve(pb, NULL), "actual source slot 2 completes first");
            if (!mode) { testRequire(!xrtFutureDone(output), "ALL remains pending"); view(output, slots, mapped, mode, false); }
            testRequire(xrtPromiseResolve(pa, NULL), "remaining duplicate source completes");
        }
    }
    if (!cancel || ready) {
        testRequire(xrtFutureState(output) == XFUTURE_RESOLVED, "successful aggregate outcome");
        if (!mapped) {
            if (!mode) { const xfutureall* all = xrtFutureValue(output); testRequire(all && all->Count == 3 && all->Futures[0] == all->Futures[1], "raw All payload ABI"); }
            else { const xfuturepick* pick = xrtFutureValue(output); testRequire(pick && pick->Index == 2 && pick->Future == b, "raw Pick payload ABI"); }
        }
        if (mode) testRequire(requested(a) == (mode == 2) && !requested(b), "only Race cancels loser");
    }
    testRequire(!mapped || (counts.maps == (unsigned)(!cancel || ready) && counts.drops == 1), "mapper and owned Data release exactly once");
    xrtPromiseDestroy(pa); xrtPromiseDestroy(pb); xrtFutureDestroy(a); xrtFutureDestroy(b); xrtFutureDestroy(output); balanced(&before);
}
static unsigned preparation_failures(void)
{
    unsigned failures = 0, successes = 0;
    for (unsigned mode = 0; mode < 3; ++mode) for (unsigned ready = 0; ready < 2; ++ready) for (unsigned point = 0; point < 16; ++point) {
        xmemdebugsnapshot before; events counts = {0}; xfuture *a, *b, *output; xpromise *pa, *pb; context* ctx; xfuture* inputs[3]; bool injected;
        xrtMemDebugSnapshot(&before); pa = xrtPromiseCreate(&a, NULL); pb = xrtPromiseCreate(&b, NULL);
        testRequire(pa && pb, "policy OOM inputs"); inputs[0] = inputs[1] = a; inputs[2] = b; ctx = create_context(&counts);
        if (ready) { testRequire(xrtPromiseResolve(pb, NULL), "precompleted winning input"); if (!mode) testRequire(xrtPromiseResolve(pa, NULL), "precompleted All"); }
        testRequire(xrtMemDebugFailAfter(point), "policy allocation budget"); output = start(mode, true, inputs, ctx);
        injected = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xrtClearError();
        if (!output) {
            xrtownershipref anchors[2] = {xrtFutureOwnership(a), xrtFutureOwnership(b)};
            xrtownershipref slots[4] = {anchors[0], anchors[1], xrtPromiseOwnership(pa), xrtPromiseOwnership(pb)};
            xrtownershipresult graph = {0}; ++failures;
            testRequire(injected && !counts.maps && !counts.drops && ctx->refs == 1, "failure returns untouched mapper ownership");
            testRequire(!requested(a) && !requested(b) && xrtFutureDone(a) == (bool)(ready && !mode) && xrtFutureDone(b) == (bool)ready,
                "failed launch preserves source completion and cancellation");
            testRequire(xrtOwnershipInspect(anchors, 2, slots, 4, &graph) && graph.NodeCount == 4 && graph.EdgeCount == 6 && !graph.ExternalRootCount,
                "failure leaves no group, source reference or observer behind");
            context_drop(ctx);
        } else {
            ++successes; if (!ready) testRequire(xrtFutureCancel(output), "accepted OOM frontier cancellation");
            testRequire(counts.maps == ready && counts.drops == 1, "accepted policy Data returned once"); xrtFutureDestroy(output);
        }
        xrtPromiseDestroy(pa); xrtPromiseDestroy(pb); xrtFutureDestroy(a); xrtFutureDestroy(b); balanced(&before);
    }
    testRequire(failures && successes, "both real allocation failure and accepted policy frontier"); return failures;
}
static void empty_and_invalid_policy(void)
{
    xmemdebugsnapshot before; events counts = {0}; context* ctx; xfuture* output;
    xrtMemDebugSnapshot(&before); ctx = create_context(&counts);
    for (unsigned i = 0; i < 4; ++i) {
        xfuturecombineownershipv1 invalid = map_policy;
        if (i == 0) invalid.size--; else if (i == 1) invalid.Drop = NULL; else if (i == 2) invalid.Ops = NULL; else invalid.AllMap = NULL;
        testRequire(!xrtFutureAllMapOwnedPolicyV1(NULL, 0, ctx, &invalid), "malformed mapper policy rejected"); xrtClearError();
    }
    testRequire(!xrtFutureAnyMapOwnedPolicyV1(NULL, 0, ctx, &map_policy), "Any still requires an input"); xrtClearError();
    testRequire(!xrtFutureRaceMapOwnedPolicyV1(NULL, 1, ctx, &map_policy), "Race rejects absent input array"); xrtClearError();
    testRequire(ctx->refs == 1 && !counts.maps && !counts.drops, "invalid calls do not consume map context");
    output = xrtFutureAllMapOwnedPolicyV1(NULL, 0, ctx, &map_policy);
    testRequire(output && xrtFutureState(output) == XFUTURE_RESOLVED && counts.maps == 1 && counts.drops == 1,
        "empty owned All synchronously accepts, maps and releases once"); xrtFutureDestroy(output); balanced(&before);
}
static void terminal_cycle(bool reverse, bool restore)
{
    xmemdebugsnapshot before; xfuture *a, *b, *output; xpromise *pa, *pb; xfuture* inputs[3];
    xrtownershipref anchors[1], slots[5], refs[16]; const xrtownershipadapterv1* adapters[16];
    const xrtownershippreparationv1* preparations[16]; xrtownershipsnapshot* snapshot = NULL;
    xrtownershipscope freeze = {0}; xrtownershipnode node; size_t n; unsigned token = 0;
    xrtMemDebugSnapshot(&before); pa = xrtPromiseCreate(&a, NULL); pb = xrtPromiseCreate(&b, NULL);
    testRequire(pa && pb, "cycle inputs"); inputs[0] = inputs[1] = a; inputs[2] = b;
    output = xrtFutureAll(inputs, 3); testRequire(output != NULL, "cycle producer");
    testRequire(xrtPromiseResolveOwnedPolicyV1(pa, xrtFutureRef(output), &future_policy), "real retained back edge through source payload");
    testRequire(xrtPromiseResolve(pb, NULL) && xrtFutureDone(output), "terminal aggregate cycle");
    anchors[0] = slots[4] = xrtFutureOwnership(output); slots[0] = xrtFutureOwnership(a); slots[1] = xrtFutureOwnership(b);
    slots[2] = xrtPromiseOwnership(pa); slots[3] = xrtPromiseOwnership(pb);
    for (unsigned attempt = 0; attempt < (restore ? 2u : 1u); ++attempt) {
        testRequire(xrtOwnershipFreezeTryBegin(&freeze), "terminal cycle freeze");
        testRequire(xrtOwnershipSnapshotCreate(anchors, 1, slots, 5, admit, NULL, &snapshot), "terminal full graph admission");
        n = xrtOwnershipSnapshotNodeCount(snapshot); testRequire(n == 7, "no terminal observer or phantom producer node");
        for (size_t i = 0; i < n; ++i) {
            testRequire(xrtOwnershipSnapshotNode(snapshot, i, &node) && !node.Reachable, "real terminal cycle unreachable");
            refs[i] = node.Reference; adapters[i] = resolve(refs[i], &preparations[i]);
            testRequire(adapters[i] && adapters[i]->Hold(refs[i].Data) && adapters[i]->Claim(refs[i].Data, &token), "whole physical graph hold and claim");
        }
        xrtOwnershipSnapshotDestroy(snapshot); snapshot = NULL;
        if (restore && attempt == 0) {
            for (size_t i = 0; i < n; ++i) adapters[i]->Restore(refs[i].Data, &token);
            testRequire(xrtOwnershipScopeEnd(&freeze), "restored whole terminal graph");
            for (size_t i = 0; i < n; ++i) adapters[i]->Drop(refs[i].Data);
            continue;
        }
        for (size_t i = 0; i < n; ++i) testRequire(!preparations[i] || preparations[i]->Ready(refs[i].Data), "terminal semantic preparation ready");
        testRequire(xrtOwnershipScopeEnd(&freeze), "terminal preparation outside freeze");
        for (size_t i = 0; i < n; ++i) testRequire(!preparations[i] || preparations[i]->Prepare(refs[i].Data, &token) == XRT_OWNERSHIP_PREPARE_READY, "normal completion settled every participant");
        testRequire(xrtOwnershipFreezeTryBegin(&freeze), "same-claim terminal revalidation");
        /* Every plan pin is an actual fixture-owned reference at revalidation. */
        {
            xrtownershipref internal[21]; memcpy(internal, slots, sizeof(slots)); memcpy(internal + 5, refs, n * sizeof(refs[0]));
            testRequire(xrtOwnershipSnapshotCreate(anchors, 1, internal, n + 5, admit, NULL, &snapshot), "rebuild graph with actual pins");
            testRequire(xrtOwnershipSnapshotNodeCount(snapshot) == n, "same physical terminal graph");
            for (size_t i = 0; i < n; ++i) testRequire(xrtOwnershipSnapshotNode(snapshot, i, &node) && !node.Reachable, "no resurrection before Clear");
            xrtOwnershipSnapshotDestroy(snapshot); snapshot = NULL;
        }
        for (size_t j = 0; j < n; ++j) { size_t i = reverse ? n - j - 1 : j; adapters[i]->Clear(refs[i].Data, &token); }
        testRequire(xrtOwnershipScopeEnd(&freeze), "whole graph cleared before physical release");
        for (unsigned again = 0; again < 2; ++again) for (size_t j = 0; j < n; ++j) {
            size_t i = reverse ? n - j - 1 : j; testRequire(!adapters[i]->Finish || adapters[i]->Finish(refs[i].Data, &token), "both Finish orders and idempotent exact slot return");
        }
        for (size_t i = 0; i < n; ++i) adapters[i]->Drop(refs[i].Data);
    }
    xrtPromiseDestroy(pa); xrtPromiseDestroy(pb); xrtFutureDestroy(a); xrtFutureDestroy(b); xrtFutureDestroy(output); balanced(&before);
}
int main(void)
{
    unsigned failures = 0;
    for (unsigned i = 0; i < 32; ++i) for (unsigned mode = 0; mode < 3; ++mode) for (unsigned mapped = 0; mapped < 2; ++mapped) {
        round_trip(mode, mapped != 0, false, false); round_trip(mode, mapped != 0, true, false); round_trip(mode, mapped != 0, false, true);
    }
    for (unsigned i = 0; i < 32; ++i) { terminal_cycle(false, false); terminal_cycle(true, false); terminal_cycle(false, true); terminal_cycle(true, true); }
    failures = preparation_failures();
    empty_and_invalid_policy();
    printf("Combine ownership: 576 lifetimes; 128 terminal cycle transactions; 96 policy allocation budgets (%u actual failures); pending BUSY, exact graphs and both Finish orders\n", failures);
    testMemoryDebugDrain("aggregate ownership drain"); return 0;
}
