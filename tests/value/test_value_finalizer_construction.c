#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

typedef struct Construction {
	unsigned finalized, fields, released;
	xvalue* edge;
	xvalueiter afterFinalize;
	bool keepAfterFinalize;
} Construction;

static void finalize(xvalue* value, ptr data)
{
	Construction* state = (Construction*)data;
	testRequire(state->finalized++ == 0 && !state->fields && !state->released, "one armed finalizer before fields/context");
	testRequire(xrtValueCount(value) == 1, "finalizer sees initialized fields");
	testRequire(!xrtValueObjectFinalizerCommit(value), "reentrant finalizing commit refuses"); xrtClearError();
	if (state->keepAfterFinalize) testRequire(xrtValueIterBegin(value, &state->afterFinalize), "finalizer cursor prolongs fields/context");
}
static void field_drop(ptr data, ptr unused)
{
	Construction* state = (Construction*)data; (void)unused;
	testRequire(!state->fields++ && !state->released, "fields precede context release");
}
static bool trace(const void* data, xrtownershipvisitor visit, void* context)
{
	const Construction* state = (const Construction*)data;
	return visit(xrtValueOwnership(state->edge), context);
}
static void release_context(ptr data)
{
	Construction* state = (Construction*)data;
	testRequire(state->fields == 1 && !state->released++, "one context release after fields");
	xrtValueRelease(state->edge); state->edge = NULL;
}
static xvalue* make(Construction* state)
{
	static const xvaluehandleops ops = {.Drop=field_drop};
	xvalue* object = xrtValueObjectLifo(); ptr owned = state;
	xvalue* field = xrtValueHandleTake(&owned, &ops, NULL);
	state->edge = xrtValueString(XRT_STR_LITERAL("owned context edge"));
	testRequire(object && field && !owned && state->edge &&
		xrtValueObjectSetNew(object, XRT_STR_LITERAL("field"), field), "construction setup");
	return object;
}
static void balanced(const xmemdebugsnapshot* before)
{
	xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
	testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
		after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
		before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
		before->UseAfterFreeCount == after.UseAfterFreeCount, "construction allocator balance");
}
static void lifetime(unsigned mode, bool commit)
{
	Construction state = {0}; xmemdebugsnapshot before;
	xvalue *object, *alias = NULL; xvalueiter cursor = {0};
	xrtMemDebugSnapshot(&before); object = make(&state);
	testRequire(xrtMemDebugFailAfter(0), "arm prepare allocation detector");
	testRequire(xrtValueObjectFinalizerPrepareOwned(object, finalize, &state, trace, release_context), "prepare before publication");
	testRequire(!xrtMemDebugFailTriggered(), "prepare allocates nothing"); xrtMemDebugFailClear();
	if (mode == 0) alias = xrtValueRetain(object);
	if (mode == 1) alias = xrtValueClone(object);
	if (mode == 2) testRequire(xrtValueIterBegin(object, &cursor), "cursor before commit");
	if (mode == 3) { alias = xrtValueRetain(object); state.keepAfterFinalize = commit; }
	if (mode != 2) testRequire(alias != NULL, "native alias owns prepared backing");
	testRequire(xrtMemDebugFailAfter(0), "arm commit allocation detector");
	if (commit) testRequire(xrtValueObjectFinalizerCommit(object), "commit permits native aliases/cursors");
	testRequire(!xrtMemDebugFailTriggered(), "commit allocates nothing"); xrtMemDebugFailClear();
	if (commit) {
		testRequire(!xrtValueObjectFinalizerCommit(object) && xrtErrorKind(xrtGetError()) == XERR_STATE,
			"duplicate commit fails without changing the armed duty"); xrtClearError();
	}
	xrtValueRelease(object);
	testRequire(!state.finalized && !state.fields && !state.released, "native owner keeps pending or armed lifecycle alive");
	xrtValueRelease(alias); xrtValueIterEnd(&cursor);
	if (state.keepAfterFinalize) {
		testRequire(state.finalized == 1 && !state.fields && !state.released, "finalizer-return tail is still owned");
		xrtValueIterEnd(&state.afterFinalize);
	}
	testRequire(state.finalized == (unsigned)commit && state.fields == 1 && state.released == 1 && !xrtGetError(),
		"failed construction never runs user finalizer; all owners end once");
	balanced(&before);
}
static void refusal(unsigned mode)
{
	Construction state = {0}; xmemdebugsnapshot before;
	xvalue *object, *alias = NULL; xvalueiter cursor = {0};
	xrtMemDebugSnapshot(&before); object = make(&state);
	if (mode == 0) alias = xrtValueRetain(object);
	if (mode == 1) alias = xrtValueClone(object);
	if (mode == 2) testRequire(xrtValueIterBegin(object, &cursor), "unprepared snapshot");
	testRequire(!xrtValueObjectFinalizerPrepareOwned(object, finalize, &state,
		mode == 3 ? NULL : trace, release_context), "invalid/nonunique prepare refuses atomically");
	xrtClearError();
	testRequire(!state.finalized && !state.fields && !state.released && state.edge, "failed prepare does not transfer context");
	testRequire(!xrtValueObjectFinalizerCommit(object), "failed prepare installed no pending duty"); xrtClearError();
	xrtValueRelease(object); xrtValueRelease(alias); xrtValueIterEnd(&cursor);
	testRequire(state.fields == 1 && !state.finalized && !state.released, "only original field ownership ended");
	release_context(&state); balanced(&before);
}
int main(void)
{
	testRequire(xrtMemDebugEnable(true), "enable allocator diagnostics");
	for (unsigned round = 0; round < 50; ++round) {
		for (unsigned mode = 0; mode < 4; ++mode) { lifetime(mode, false); lifetime(mode, true); refusal(mode); }
	}
	testRequire(!xrtValueObjectFinalizerCommit(NULL) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "NULL commit refuses"); xrtClearError();
	puts("Prepared finalizers: 400 alias/cursor lifetimes, 200 commits, 200 abandoned constructions, 200 atomic prepare refusals, allocation-free prepare/commit, field/context and allocator balance");
	testMemoryDebugDrain("prepared finalizer allocator drained"); return 0;
}
