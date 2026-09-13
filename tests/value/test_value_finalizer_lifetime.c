/* A cursor may be the last backing owner after every source shell is gone. */
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifdef OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

static unsigned finalized;
static void finalizer(xvalue* object, ptr context)
{
	(void)context;
	testRequire(xrtValueCount(object) == 1, "finalizer sees live fields");
	++finalized;
}
static void legacy_cursor(void)
{
	xvalue* object = xrtValueObject();
	xvalueiter iterator = {0};
	testRequire(object && xrtValueObjectSetNew(object, XRT_STR_LITERAL("item"), xrtValueInt(42)), "object setup");
	testRequire(xrtValueObjectFinalizerBind(object, finalizer, NULL), "bind finalizer");
	testRequire(xrtValueIterBegin(object, &iterator), "begin snapshot");
	xrtValueRelease(object);
	testRequire(finalized == 0 && xrtValueIterNext(&iterator, NULL), "cursor retains unfinalized backing");
	xrtValueIterEnd(&iterator);
	testRequire(finalized == 1, "last cursor release must run object finalizer exactly once");
}

typedef struct Lifetime Lifetime;
typedef struct Field { Lifetime* owner; unsigned index; } Field;
struct Lifetime {
	unsigned calls, releases, drops;
	bool deferred;
	xvalue* contextValue;
	xvalue* original;
	xvalueiter during;
	Field fields[2];
};
static unsigned probes, failures, refusals;
static bool leaf(const xvalue* value, xrtownershipvisitor visit, ptr context)
{ (void)value; (void)visit; (void)context; return true; }
static bool trace_context(const void* data, xrtownershipvisitor visit, ptr context)
{
	const Lifetime* state = (const Lifetime*)data;
	return state->contextValue == NULL || visit(xrtValueOwnership(state->contextValue), context);
}
static void drop_field(ptr data, ptr unused)
{
	Field* field = (Field*)data;
	(void)unused;
	testRequire(field->owner->calls == 1 && field->owner->releases == 0, "context retained throughout field Drop");
	testRequire(field->index == 2 - field->owner->drops, "reverse field destruction");
	++field->owner->drops;
}
static const xvaluehandleops field_ops = {.Drop = drop_field};
static void end_context(ptr data)
{
	Lifetime* state = (Lifetime*)data;
	testRequire(state->calls == 1 && state->drops == 2 && !state->releases, "owned context after finalizer AND fields");
	++state->releases; xrtValueRelease(state->contextValue); state->contextValue = NULL;
}
static void finalize_owned(xvalue* object, ptr data)
{
	Lifetime* state = (Lifetime*)data;
	xrtownershipresult initial = {91,92,93,94}, result = initial;
	xrtownershipref view;
	xvalueiter local = {0};
	testRequire(!state->calls && !state->drops && !state->releases && xrtValueCount(object) == 2, "live borrowed finalizer view");
	++state->calls;
	testRequire(xrtValueRetain(object) == NULL, "no resurrection by retain"); xrtClearError();
	testRequire(xrtValueClone(object) == NULL, "no resurrection by clone"); xrtClearError();
	xrtValueRelease(object);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "borrowed release rejected without refcount underflow"); xrtClearError();
	testRequire(!xrtValueObjectFinalizerBindOwned(object, finalize_owned, state, trace_context, end_context), "no rebind during finalize"); xrtClearError();
	testRequire(xrtValueIterBegin(object, &local) && local.FinalizerOwner == NULL, "local enumeration inside finalizer");
	view = xrtValueIterOwnership(&local);
	testRequire(!xrtOwnershipInspect(&view, 1, &view, 1, &result) && !memcmp(&result, &initial, sizeof(result)), "running finalizer graph refuses atomically");
	xrtClearError(); xrtValueIterEnd(&local);
	testRequire(xrtValueObjectSetNew(object, XRT_STR_LITERAL("extra"), xrtValueInt(43)), "documented field mutation in finalizer");
	if (state->deferred) testRequire(xrtValueIterBegin(object, &state->during), "callback-created backing snapshot");
}
static xvalue* make_object(Lifetime* state)
{
	xvalue* object = xrtValueObjectLifo();
	state->contextValue = xrtValueInt(99);
	testRequire(object && state->contextValue, "owned finalizer setup");
	for (unsigned i = 0; i < 2; ++i) {
		xvalue* field;
		ptr handle = &state->fields[i];
		state->fields[i].owner = state; state->fields[i].index = i + 1;
		field = xrtValueHandleTake(&handle, &field_ops, NULL);
		testRequire(field && xrtValueHandleOwnershipBind(field, leaf) && xrtValueObjectSetNew(object,
			i == 0 ? XRT_STR_LITERAL("first") : XRT_STR_LITERAL("second"), field), "traceable fields");
	}
	state->original = object; return object;
}
static void balanced(const xmemdebugsnapshot* before)
{
	xmemdebugsnapshot after;
	xrtClearError(); xrtMemDebugSnapshot(&after);
	testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
		after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
		before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
		before->UseAfterFreeCount == after.UseAfterFreeCount, "per-transaction exact memory balance");
}
static void scenario(unsigned mode)
{
	Lifetime state = {0};
	xmemdebugsnapshot before;
	xvalue *object, *clone = NULL, *alias;
	xvalueiter stack = {0}, *heap = NULL;
	xrtownershipref slots[4], anchor;
	xrtownershipresult result;
	size_t count = 0, cursors = 0;
	bool reverse = (mode & 1) != 0;
	xrtMemDebugSnapshot(&before); object = make_object(&state); state.deferred = mode == 8;
	/* All binding refusals leave the finalization duty/context untouched. */
	alias = xrtValueRetain(object);
	testRequire(!xrtValueObjectFinalizerBindOwned(object, finalize_owned, &state, trace_context, end_context), "shared shell bind rejected");
	++refusals; xrtClearError(); xrtValueRelease(alias);
	alias = xrtValueClone(object);
	testRequire(!xrtValueObjectFinalizerBindOwned(object, finalize_owned, &state, trace_context, end_context), "shared backing bind rejected");
	++refusals; xrtClearError(); xrtValueRelease(alias);
	testRequire(!xrtValueObjectFinalizerBindOwned(object, finalize_owned, &state, NULL, end_context) &&
		!xrtValueObjectFinalizerBindOwned(object, finalize_owned, &state, trace_context, NULL) &&
		!xrtValueObjectFinalizerBindOwned(object, NULL, &state, trace_context, end_context), "invalid callbacks rejected");
	refusals += 3; xrtClearError();
	testRequire(xrtMemDebugFailAfter(0), "arm allocation-free bind");
	testRequire(xrtValueObjectFinalizerBindOwned(object, finalize_owned, &state, trace_context, end_context) &&
		!xrtMemDebugFailTriggered(), "atomic bind requires no allocation"); xrtMemDebugFailClear();
	testRequire(!xrtValueObjectFinalizerBindOwned(object, finalize_owned, &state, trace_context, end_context), "duplicate binding unchanged");
	++refusals; xrtClearError();
	anchor = xrtValueOwnership(object); slots[count++] = anchor;
	if (mode & 1) { clone = xrtValueClone(object); testRequire(clone != NULL, "clone identity shell"); slots[count++] = xrtValueOwnership(clone); }
	if (mode & 2) {
		testRequire(reverse ? xrtValueIterRBegin(object, &stack) : xrtValueIterBegin(object, &stack), "stack cursor");
		testRequire(stack.FinalizerOwner == object, "cursor pins actual finalizer shell"); slots[count++] = xrtValueIterOwnership(&stack); ++cursors;
	}
	if (mode & 4) {
		heap = reverse ? xrtValueIterRCreate(object) : xrtValueIterCreate(object);
		testRequire(heap && heap->FinalizerOwner == object, "heap cursor pins actual shell"); slots[count++] = xrtValueIterOwnership(heap); ++cursors;
	}
	testRequire(xrtOwnershipInspect(&anchor, 1, slots, count, &result) &&
		result.NodeCount == 5 + (clone != NULL) + cursors &&
		result.EdgeCount == 5 + 2 * (clone != NULL) + 3 * cursors && !result.ExternalRootCount, "exact shell/backing/context/cursor graph");
	alias = xrtValueRetain(object);
	testRequire(xrtOwnershipInspect(&anchor, 1, slots, count, &result) && result.ExternalRootCount == 1, "native retained alias visible"); xrtValueRelease(alias);
	alias = xrtValueClone(object);
	testRequire(xrtOwnershipInspect(&anchor, 1, slots, count, &result) && result.ExternalRootCount == 1, "unregistered COW alias visible"); xrtValueRelease(alias);
	for (unsigned fault = 0; fault < 12; ++fault) {
		xrtownershipresult initial = {91,92,93,94}; bool ok, triggered;
		result = initial; testRequire(xrtMemDebugFailAfter(fault), "arm inspection failure");
		ok = xrtOwnershipInspect(&anchor, 1, slots, count, &result); triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); ++probes;
		if (triggered) { ++failures; testRequire(!ok && !memcmp(&result, &initial, sizeof(result)), "failed inspect is atomic"); }
		else testRequire(ok && !result.ExternalRootCount, "successful graph allocation probe");
		xrtClearError(); testRequire(!state.calls && !state.releases && !state.drops, "inspection never finalizes");
	}
	xrtValueRelease(object); xrtValueRelease(clone);
	if (cursors) testRequire(state.calls == 0, "all source owners released before cursors");
	xrtValueIterEnd(&stack); xrtValueIterDestroy(heap);
	testRequire(state.calls == 1, "one atomic finalization claimant");
	if (state.deferred) {
		testRequire(state.releases == 0 && state.drops == 0 && state.during.FinalizerOwner == NULL, "context survives callback-created snapshot");
		anchor = xrtValueIterOwnership(&state.during);
		testRequire(xrtOwnershipInspect(&anchor, 1, &anchor, 1, &result) && !result.ExternalRootCount, "post-finalize context still described");
		xrtValueIterEnd(&state.during); xrtValueIterEnd(&state.during);
	}
	testRequire(state.releases == 1 && state.drops == 2, "exact finalization/field/context duties"); balanced(&before);
}
typedef struct ReleaseJob { xvalue* value; xatomic32* start; } ReleaseJob;
static int32 release_worker(ptr data)
{
	ReleaseJob* job = (ReleaseJob*)data;
	while (!xrtAtomic32Load(job->start, XMEMORY_ACQUIRE)) xrtThreadYield();
	xrtValueRelease(job->value); return 0;
}
static void concurrent(void)
{
	Lifetime state = {0}; xatomic32 start; xthread* threads[4]; ReleaseJob jobs[4];
	xmemdebugsnapshot before; xvalue* object;
	xrtMemDebugSnapshot(&before); xrtAtomic32Init(&start, 0); object = make_object(&state);
	testRequire(xrtValueObjectFinalizerBindOwned(object, finalize_owned, &state, trace_context, end_context), "concurrent bind");
	for (unsigned i = 0; i < 4; ++i) {
		jobs[i].value = xrtValueClone(object); jobs[i].start = &start;
		testRequire(jobs[i].value != NULL, "concurrent independent shell");
		threads[i] = xrtThreadCreate(release_worker, &jobs[i], 0); testRequire(threads[i] != NULL, "release worker");
	}
	xrtValueRelease(object); testRequire(!state.calls, "all clone owners initially alive");
	xrtAtomic32Store(&start, 1, XMEMORY_RELEASE);
	for (unsigned i = 0; i < 4; ++i) { testRequire(xrtThreadWaitFor(threads[i], 5000000) == XWAIT_OK, "release thread join"); xrtThreadDestroy(threads[i]); }
	testRequire(state.calls == 1 && state.releases == 1 && state.drops == 2, "concurrent last backing finalizes exactly once"); balanced(&before);
}
int main(void)
{
	testRequire(xrtMemDebugEnable(true), "enable allocator"); legacy_cursor();
	for (unsigned round = 0; round < 40; ++round) for (unsigned mode = 0; mode < 10; ++mode) scenario(mode);
	for (unsigned round = 0; round < 100; ++round) concurrent();
	testRequire(probes == 4800 && failures == 1600 && refusals == 2400, "fixed batch denominator");
	printf("Value finalizer lifetime: 501 lifetimes, 100 four-thread releases, %u binding refusals, %u graph probes, %u actual failures\n", refusals, probes, failures);
	testMemoryDebugDrain("finalizer allocator drained"); return 0;
}
