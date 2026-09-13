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

typedef struct Publication {
	unsigned finalized, proposed, fields;
	xerror* rollbackError;
} Publication;
static void finalize(xvalue* value, ptr data)
{
	Publication* state = (Publication*)data;
	testRequire(xrtValueCount(value) == 1, "finalizer still observes field");
	++state->finalized;
	xrtSetErrorTake(xrtErrorRef(state->rollbackError));
}
static void proposed(xvalue* value, ptr data)
{
	Publication* state = (Publication*)data;
	(void)value; ++state->proposed;
}
static void field_drop(ptr data, ptr unused)
{
	Publication* state = (Publication*)data;
	(void)unused; ++state->fields;
	xrtSetErrorTake(xrtErrorRef(state->rollbackError));
}
static xvalue* make(Publication* state)
{
	static const xvaluehandleops fields = {.Drop = field_drop}; ptr owned = state;
	xvalue* object = xrtValueObjectLifo(); xvalue* field;
	field = xrtValueHandleTake(&owned, &fields, NULL);
	testRequire(object && field && !owned && xrtValueObjectSetNew(object, XRT_STR_LITERAL("field"), field), "publication setup");
	return object;
}
static void run(unsigned mode, bool primary)
{
	Publication state = {0}; xmemdebugsnapshot before, after;
	xvalue *object, *alias = NULL; xvalueiter cursor = {0};
	xerror* prior; xvalue* result;
	xrtClearError(); xrtMemDebugSnapshot(&before);
	state.rollbackError = xrtErrorCreate(XERR_VALUE, "rollback", 1, "field/finalizer cleanup");
	prior = primary ? xrtErrorCreate(XERR_IO, "primary", 2, "earlier error") : NULL;
	testRequire(state.rollbackError && (!primary || prior), "diagnostics setup");
	object = make(&state);
	if (mode == 1) { alias = xrtValueClone(object); testRequire(alias != NULL, "shared backing"); }
	if (mode == 2) testRequire(xrtValueIterBegin(object, &cursor), "snapshot before publication");
	if (mode == 3) testRequire(xrtValueObjectFinalizerBind(object, finalize, &state), "earlier finalizer duty");
	xrtSetErrorTake(prior);
	if (mode == 0) testRequire(xrtMemDebugFailAfter(0), "arm allocation failure");
	result = xrtValueObjectFinalizerBindTake(object, mode == 4 ? NULL : proposed, &state);
	if (mode == 0) {
		testRequire(result == object && !xrtMemDebugFailTriggered(), "allocation-free successful publication");
		xrtMemDebugFailClear();
		testRequire(xrtGetError() == prior, "successful publication preserves primary");
		xrtClearError(); xrtValueRelease(result);
		testRequire(state.proposed == 1 && !state.finalized, "new duty runs once");
	} else {
		testRequire(!result && !state.proposed, "failed publication never installs proposed duty");
		testRequire(prior ? xrtGetError() == prior : xrtErrorKind(xrtGetError()) == (mode == 4 ? XERR_ARGUMENT : XERR_STATE), "binding error survives rollback callbacks");
		testRequire(state.fields == (unsigned)(mode >= 3), "one input owner consumed, aliases preserved");
		testRequire(state.finalized == (unsigned)(mode == 3), "only the earlier finalizer can run on rollback");
	}
	xrtClearError(); xrtValueRelease(alias); xrtValueIterEnd(&cursor); xrtClearError();
	testRequire(state.fields == 1, "one final field release");
	xrtErrorFree(state.rollbackError); xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
		after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
		before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
		before.UseAfterFreeCount == after.UseAfterFreeCount, "publication memory balance");
}
int main(void)
{
	testRequire(xrtMemDebugEnable(true), "enable allocation diagnostics");
	for (unsigned round = 0; round < 100; ++round)
		for (unsigned mode = 0; mode < 5; ++mode) run(mode, (round & 1) != 0);
	testRequire(!xrtValueObjectFinalizerBindTake(NULL, proposed, NULL) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "NULL input outcome"); xrtClearError();
	puts("Value finalizer publication: 500 lifetimes, 400 real binding refusals, 100 allocation-free commits, original-error and field-release balance");
	testMemoryDebugDrain("publication allocator drained"); return 0;
}
