#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_VALUE_COLLECTION
#define XRT_MODULE_VALUE_COLLECTION
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

typedef struct Lifetime {
	xvalue* edge;
	unsigned releases, fields, finalizers, finalizerContexts;
	unsigned expectedFields, expectedFinalizers, expectedContexts;
} Lifetime;
static bool trace(const void* data, xrtownershipvisitor visit, ptr context)
{ return visit(xrtValueOwnership(((const Lifetime*)data)->edge), context); }
static bool borrowed_trace(const void* data, xrtownershipvisitor visit, ptr context)
{ (void)data; (void)visit; (void)context; return true; }
static void release_lifetime(ptr data)
{
	Lifetime* state = (Lifetime*)data;
	testRequire(!state->releases++ && state->fields == state->expectedFields &&
		state->finalizers == state->expectedFinalizers && state->finalizerContexts == state->expectedContexts,
		"lifetime drops once, after fields and finalizer context");
	xrtValueRelease(state->edge); state->edge = NULL;
}
static xvalue* make(Lifetime* state)
{
	xvalue* object = xrtValueObjectLifo();
	state->edge = xrtValueString(XRT_STR_LITERAL("copyable capability edge"));
	testRequire(object && state->edge && xrtValueObjectLifetimeBindOwned(object, state, trace, release_lifetime), "owned lifetime binding");
	return object;
}
static void balanced(const xmemdebugsnapshot* before)
{
	xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
	testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
		after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
		before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
		before->UseAfterFreeCount == after.UseAfterFreeCount, "lifetime allocator balance");
}
static void graph(xvalue* a, xvalue* b, xvalue* c, size_t nodes, size_t edges)
{
	xrtownershipref anchors[3]; xrtownershipresult result;
	size_t count = 0;
	if (a) anchors[count++] = xrtValueOwnership(a);
	if (b) anchors[count++] = xrtValueOwnership(b);
	if (c) anchors[count++] = xrtValueOwnership(c);
	testRequire(xrtOwnershipInspect(anchors, count, NULL, 0, &result), "complete lifetime ownership graph");
	testRequire(result.NodeCount == nodes && result.EdgeCount == edges &&
		result.ExternalRootCount == count && result.ReachableAnchorCount == count,
		"shell/backing/shared lifetime/context edge counted once");
}
static void copy_families(void)
{
	for (unsigned round = 0; round < 50; ++round) for (unsigned mode = 0; mode < 4; ++mode) {
		Lifetime state = {0}; xmemdebugsnapshot before; xvalue *source, *copy, *deep; xvalueiter cursor;
		xrtMemDebugSnapshot(&before); source = make(&state);
		if (mode != 3) testRequire(xrtValueObjectConstructionPrepare(source), "prepare ordinary construction");
		if (mode == 0) testRequire(xrtValueObjectConstructionCommit(source), "commit before copying");
		graph(source, NULL, NULL, 4, 3);
		copy = xrtValueClone(source); testRequire(copy != NULL, "shallow shell clone");
		graph(source, copy, NULL, 5, 4);
		testRequire(xrtValueReserve(source, 4), "ordinary COW split remains legal");
		graph(source, copy, NULL, 6, 5);
		if (mode == 1) testRequire(xrtValueObjectConstructionCommit(source), "commit survives precommit COW");
		if (mode <= 1) {
			testRequire(!xrtValueObjectConstructionCommit(source), "repeat ordinary commit refuses"); xrtClearError();
		}
		deep = xrtValueDeepClone(source); testRequire(deep != NULL, "deep clone retains lifetime");
		graph(source, copy, deep, 8, 7);
		testRequire(xrtValueIterBegin(copy, &cursor), "cursor owns independent old backing");
		xrtValueRelease(source); xrtValueRelease(copy); xrtValueRelease(deep);
		testRequire(!state.releases, "cursor keeps capability after every shell ends");
		xrtValueIterEnd(&cursor); testRequire(state.releases == 1, "last cursor releases capability");
		balanced(&before);
	}
}
static void field_drop(ptr data, ptr unused)
{ Lifetime* s = (Lifetime*)data; (void)unused; testRequire(!s->releases && !s->fields++, "field before lifetime"); }
static void finalize(xvalue* value, ptr data)
{ Lifetime* s = (Lifetime*)data; testRequire(xrtValueCount(value) == 1 && !s->releases && !s->fields && !s->finalizers++, "unique finalizer before fields"); }
static void finalizer_release(ptr data)
{ Lifetime* s = (Lifetime*)data; testRequire(!s->releases && s->fields == 1 && !s->finalizerContexts++, "finalizer context before lifetime"); }
static void finalizer_identity(void)
{
	static const xvaluehandleops ops = {.Drop = field_drop};
	for (unsigned round = 0; round < 25; ++round) for (unsigned commit = 0; commit < 2; ++commit) {
		Lifetime s = {0}; xmemdebugsnapshot before; xvalue *object, *alias, *field; ptr owned = &s;
		xrtMemDebugSnapshot(&before); s.expectedFields = s.expectedContexts = 1; s.expectedFinalizers = commit;
		object = make(&s); field = xrtValueHandleTake(&owned, &ops, NULL);
		testRequire(field && !owned && xrtValueObjectSetNew(object, XRT_STR_LITERAL("field"), field), "unique finalizer field");
		testRequire(xrtValueObjectFinalizerPrepareOwned(object, finalize, &s, borrowed_trace, finalizer_release), "prepare separate finalizer duty");
		alias = xrtValueClone(object); testRequire(alias != NULL, "sharing unique finalizer is allowed");
		testRequire(!xrtValueReserve(object, 16), "copyable lifetime does not duplicate finalizer duty"); xrtClearError();
		if (commit) testRequire(xrtValueObjectConstructionCommit(object), "common commit arms real finalizer");
		xrtValueRelease(object); testRequire(!s.releases && !s.finalizers, "alias retains unique duty");
		xrtValueRelease(alias); testRequire(s.releases == 1, "finalizer lifetime release"); balanced(&before);
	}
}
static void empty_merge(void)
{
	for (unsigned round = 0; round < 50; ++round) {
		Lifetime a = {0}, b = {0}; xmemdebugsnapshot before; xvalue *target, *source;
		xrtMemDebugSnapshot(&before); target = make(&a); source = make(&b);
		testRequire(xrtValueObjectConstructionPrepare(target) &&
			xrtValueObjectSetNew(source, XRT_STR_LITERAL("field"), xrtValueInt(42)) &&
			xrtValueObjectMerge(target, source, XVALUE_MERGE_REPLACE), "empty merge keeps destination capability");
		testRequire(xrtValueObjectConstructionCommit(target), "empty merge keeps destination pending state");
		xrtValueRelease(source); testRequire(b.releases == 1 && !a.releases, "source capability not imported");
		xrtValueRelease(target); testRequire(a.releases == 1, "destination capability remains"); balanced(&before);
	}
}
static unsigned faults(void)
{
	unsigned refused = 0, allocations = 0;
	for (unsigned round = 0; round < 10; ++round) {
		Lifetime s = {0}; xmemdebugsnapshot before; xvalue* source; bool succeeded = false;
		xrtMemDebugSnapshot(&before); source = make(&s);
		testRequire(xrtValueObjectSetNew(source, XRT_STR_LITERAL("field"), xrtValueInt(42)), "fault source");
		for (unsigned point = 0; point < 64; ++point) {
			xmemdebugsnapshot copyBefore; xvalue* copy;
			xrtMemDebugSnapshot(&copyBefore); testRequire(xrtMemDebugFailAfter(point), "arm clone fault");
			copy = xrtValueDeepClone(source);
			if (copy) { testRequire(!xrtMemDebugFailTriggered(), "deep clone completes full allocation population"); succeeded = true; }
			else { testRequire(xrtMemDebugFailTriggered(), "actual deep clone allocation refusal"); ++refused; }
			xrtMemDebugFailClear(); xrtClearError(); xrtValueRelease(copy);
			testRequire(!s.releases, "failed/successful clone keeps original capability"); balanced(&copyBefore);
			if (succeeded) { if (!round) allocations = point; testRequire(point == allocations && point >= 4, "stable complete allocation denominator"); break; }
		}
		testRequire(succeeded, "clone allocation exhaustion completed");
		xrtValueRelease(source); testRequire(s.releases == 1, "fault source ends once"); balanced(&before);
	}
	testRequire(refused == allocations * 10, "every actual clone allocation injected"); return refused;
}
static void bind_failures(void)
{
	for (unsigned mode = 0; mode < 5; ++mode) {
		Lifetime s = {0}; xmemdebugsnapshot before; xvalue* object; xvalue* alias = NULL; xvalueiter cursor = {0};
		xrtMemDebugSnapshot(&before); object = xrtValueObject(); s.edge = xrtValueInt(42);
		testRequire(object && s.edge, "refusal setup");
		if (mode == 0) alias = xrtValueRetain(object);
		if (mode == 1) alias = xrtValueClone(object);
		if (mode == 2) testRequire(xrtValueIterBegin(object, &cursor), "bind cursor refusal");
		testRequire(xrtMemDebugFailAfter(0), "arm lifetime bind failure");
		testRequire(!xrtValueObjectLifetimeBindOwned(object, &s, mode == 3 ? NULL : trace, release_lifetime), "owned bind refusal");
		testRequire(xrtMemDebugFailTriggered() == (mode == 4), "only valid unique bind allocates");
		xrtMemDebugFailClear(); xrtClearError(); testRequire(!s.releases, "failure never consumes context");
		xrtValueRelease(alias); xrtValueIterEnd(&cursor);
		testRequire(xrtValueObjectLifetimeBindOwned(object, &s, trace, release_lifetime), "retry transfers original context");
		testRequire(!xrtValueObjectLifetimeBindOwned(object, &s, trace, release_lifetime), "repeat binding refuses"); xrtClearError();
		xrtValueRelease(object); testRequire(s.releases == 1, "binding retry exactly one release"); balanced(&before);
	}
}
int main(void)
{
	unsigned refused;
	testRequire(xrtMemDebugEnable(true), "enable lifetime debug");
	copy_families(); finalizer_identity(); empty_merge(); bind_failures(); refused = faults();
	printf("Value lifetime copy: 200 shallow/COW/deep/cursor families, 50 unique finalizer duties, 50 destination-preserving empty merges, 5 atomic bind refusals, %u actual deep-clone allocation refusals; graph and allocator balance\n", refused);
	testRequire(xrtMemDebugReset(), "reset lifetime debug"); return 0;
}
