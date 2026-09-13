#ifdef FUTURE_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

static unsigned drops, traces, notifications;
static void drop_value(ptr value, ptr data)
{
	++drops; xrtValueRelease((xvalue*)value); xrtValueRelease((xvalue*)data);
}
static bool trace_value(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{
	++traces;
	return visit(xrtValueOwnership((const xvalue*)value), context) &&
		visit(xrtValueOwnership((const xvalue*)data), context);
}
static bool fail_trace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ (void)value; (void)data; (void)visit; (void)context; return false; }
static void notified(ptr data) { (void)data; ++notifications; }
static bool reject_visit(xrtownershipref ref, ptr context)
{ (void)ref; (void)context; return false; }

typedef struct future_loop { xfuture* Held; } future_loop;
static bool trace_loop(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ (void)data; return visit(xrtFutureOwnership(((const future_loop*)value)->Held), context); }
static void drop_loop(ptr value, ptr data)
{ future_loop* loop = (future_loop*)value; (void)data; xrtFutureDestroy(loop->Held); xrtFree(loop); }

static void expect_graph(xfuture* future, const xrtownershipref* slots, size_t count,
	size_t nodes, size_t edges, size_t roots, bool reachable)
{
	xrtownershipref anchor = xrtFutureOwnership(future);
	xrtownershipresult result = {0}; bool live = !reachable;
	/* Inspector includes the supplied enclosing owner's real slots in its
	 * edge statistic, in addition to edges enumerated by Trace. */
	edges += count;
	testRequire(xrtOwnershipInspectReachable(&anchor, 1, slots, count, &live, &result, NULL, NULL), "inspect future graph");
	if (result.NodeCount != nodes || result.EdgeCount != edges || result.ExternalRootCount != roots || live != reachable)
		fprintf(stderr, "slots=%zu graph=%zu/%zu/%zu live=%d expected=%zu/%zu/%zu live=%d\n",
			count, result.NodeCount, result.EdgeCount, result.ExternalRootCount, live, nodes, edges, roots, reachable);
	testRequire(result.NodeCount == nodes && result.EdgeCount == edges &&
		result.ExternalRootCount == roots && live == reachable, "exact graph and roots");
}

static void expect_rejected(xfuture* future)
{
	xrtownershipref anchor = xrtFutureOwnership(future);
	xrtownershipresult before = {11,22,33,44}, result = before; bool live = true;
	testRequire(!xrtOwnershipInspectReachable(&anchor, 1, NULL, 0, &live, &result, NULL, NULL), "unknown graph rejected");
	testRequire(memcmp(&result, &before, sizeof(result)) == 0 && live, "rejection output atomic");
	xrtClearError();
}

static void transaction(void)
{
	xfuture *future = NULL, *forward = NULL;
	xpromise *promise = xrtPromiseCreate(&future, NULL), *other;
	xrtownershipref slots[3]; xvalue* value; xfuturewatch watch;
	unsigned beforeDrops = drops, beforeTraces = traces;
	testRequire(promise != NULL, "create");
	slots[0] = xrtFutureOwnership(future); slots[1] = xrtPromiseOwnership(promise);
	testRequire(slots[0].Data == slots[1].Data && slots[0].Ops == slots[1].Ops, "canonical promise identity");
	expect_graph(future, slots, 1, 2, 1, 1, true); /* Promise still owns one root. */
	expect_graph(future, slots, 2, 2, 1, 0, false);
	testRequire(xrtPromiseRef(promise) == promise, "producer alias");
	expect_graph(future, slots, 2, 2, 1, 1, true);
	xrtPromiseDestroy(promise);
	testRequire(xrtFutureWatchInit(&watch, notified, NULL, NULL), "watch init");
	testRequire(xrtFutureWatchAdd(future, &watch) == XFUTURE_WATCH_PENDING, "watch add");
	expect_rejected(future); /* Never omit an opaque waiting frame. */
	testRequire(xrtFutureWatchDetach(future, &watch), "detach");
	value = xrtValueInt(42); testRequire(value != NULL, "payload");
	testRequire(xrtValueRetain(value) == value, "context owns same value separately");
	testRequire(xrtPromiseResolveOwnedTraced(promise, value, drop_value, value, trace_value), "atomic traced publication");
	testRequire(traces == beforeTraces && drops == beforeDrops, "publication does not run adapter");
	xrtPromiseDestroy(promise);
	expect_graph(future, slots, 1, 3, 3, 0, false); /* Two edges to the same payload. */
	testRequire(!slots[0].Ops->Trace(slots[0].Data, reject_visit, NULL), "visit failure propagated");
	value = xrtValueRetain(value);
	expect_graph(future, slots, 1, 3, 3, 1, false); /* Payload alias does not own its Future. */
	xrtValueRelease(value);
	other = xrtPromiseCreate(&forward, NULL); testRequire(other != NULL, "forward pair");
	testRequire(xrtPromiseForward(other, future), "forward"); xrtPromiseDestroy(other);
	slots[1] = xrtFutureOwnership(forward);
	expect_graph(forward, slots, 2, 5, 5, 0, false);
	xrtFutureDestroy(future);
	expect_graph(forward, &slots[1], 1, 5, 5, 0, false);
	testRequire(xrtFutureValue(forward) == value, "forward preserves payload identity");
	xrtFutureDestroy(forward); testRequire(drops == beforeDrops + 1, "exact drop once");

	promise = xrtPromiseCreate(&future, NULL); testRequire(promise != NULL, "unknown pair");
	value = xrtValueInt(8); testRequire(value != NULL, "unknown value");
	testRequire(!xrtPromiseResolveOwnedTraced(promise, value, drop_value, NULL, NULL), "trace required");
	xrtClearError(); testRequire(xrtFutureState(future) == XFUTURE_PENDING, "invalid publish atomic");
	testRequire(xrtPromiseResolveOwned(promise, value, drop_value, NULL), "legacy ownership unchanged");
	testRequire(!xrtPromiseResolveOwnedTraced(promise, value, drop_value, NULL, trace_value), "duplicate rejected without consuming");
	xrtClearError(); xrtPromiseDestroy(promise); expect_rejected(future); xrtFutureDestroy(future);

	promise = xrtPromiseCreate(&future, NULL); value = xrtValueInt(9);
	testRequire(promise != NULL && value != NULL && xrtPromiseResolveOwnedTraced(promise, value, drop_value, NULL, fail_trace), "failure adapter");
	xrtPromiseDestroy(promise); expect_rejected(future); xrtFutureDestroy(future);
}

static void cause_and_cancel(void)
{
	xcancel* parent = xrtCancelCreate(); xfuture* future = NULL;
	xpromise* promise = xrtPromiseCreate(&future, parent);
	xerror *cause = xrtErrorCreate(XERR_STATE, "graph", 1, "cause"), *error;
	xrtownershipref slot;
	testRequire(parent != NULL && promise != NULL && cause != NULL, "error and cancellation allocation");
	error = xrtErrorWrap(cause, XERR_STATE, "graph", 2, "outer"); testRequire(error != NULL, "error wrapper");
	testRequire(xrtPromiseReject(promise, error), "failed future");
	xrtPromiseDestroy(promise); xrtErrorFree(error); xrtErrorFree(cause); xrtCancelDestroy(parent);
	slot = xrtFutureOwnership(future);
	expect_graph(future, &slot, 1, 5, 4, 0, false);
	xrtFutureDestroy(future);
}

static void cycle_and_oom(void)
{
	xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
	future_loop* loop = (future_loop*)xrtMalloc(sizeof(*loop));
	xrtownershipref slot; unsigned point, failed = 0;
	testRequire(promise != NULL && loop != NULL, "cycle allocation");
	loop->Held = xrtFutureRef(future); testRequire(loop->Held != NULL, "cycle owns Future");
	testRequire(xrtPromiseResolveOwnedTraced(promise, loop, drop_loop, NULL, trace_loop), "cyclic owned payload");
	xrtPromiseDestroy(promise); slot = xrtFutureOwnership(future);
	expect_graph(future, &slot, 1, 2, 2, 0, false);
	testRequire(xrtFutureRef(future) == future, "external cycle alias");
	expect_graph(future, &slot, 1, 2, 2, 1, true); xrtFutureDestroy(future);
	for (point = 0; point < 12; ++point) {
		xrtownershipresult before = {11,22,33,44}, result = before; bool live = true, ok, triggered;
		testRequire(xrtMemDebugFailAfter(point), "arm graph OOM");
		ok = xrtOwnershipInspectReachable(&slot, 1, &slot, 1, &live, &result, NULL, NULL);
		triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if (triggered) {
			++failed; testRequire(!ok && live && memcmp(&result, &before, sizeof(result)) == 0, "OOM leaves outputs unchanged");
		} else testRequire(ok && !live, "inspection after last allocation");
		xrtClearError(); testRequire(loop->Held == future && xrtFutureValue(future) == loop, "inspection never mutates cycle");
	}
	testRequire(failed != 0, "real graph allocation failures exercised");
	/* Inspection does not collect. The owning test explicitly severs its edge. */
	loop->Held = NULL; xrtFutureDestroy(future); xrtFutureDestroy(future);
}

int main(void)
{
	unsigned round; xmemdebugsnapshot before, after;
	testRequire(xrtMemDebugEnable(true), "memory debug");
	for (round = 0; round < 100; ++round) {
		xrtMemDebugSnapshot(&before); transaction(); cause_and_cancel(); cycle_and_oom();
		testRequire(xrtGetError() == NULL, "clean error slot"); xrtMemDebugSnapshot(&after);
		testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
			after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
			before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount,
			"exact transaction memory balance");
	}
	testRequire(notifications == 0, "detached watch never notified");
	printf("future ownership: 100 rounds, promise aliases, forwarding, payload/context multiplicity, error/cancel parents, cycles and 1200 allocation-failure probes\n");
	return 0;
}
