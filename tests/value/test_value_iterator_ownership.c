/* Real stack/heap cursor ownership. Elements and keys are borrowed; a COW
 * source shell is not kept alive by the iterator's backing snapshot. */
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

static unsigned probes, actualFailures;
static bool insert(xvalue* container, unsigned kind, unsigned index, xvalue* value)
{
	if (kind == 0) return xrtValueArrayAppendNew(container, value);
	if (kind == 1) return xrtValueIntMapSetNew(container, index, value);
	if (kind == 2) return xrtValueSetAddNew(container, value);
	return xrtValueObjectSetNew(container,
		index == 0 ? XRT_STR_LITERAL("first") : index == 1 ? XRT_STR_LITERAL("second") : XRT_STR_LITERAL("third"), value);
}
static void inspect(const xrtownershipref* anchors, const xrtownershipref* slots, size_t slotCount,
	const xvalueiter* stack, const xvalueiter* heap, size_t nodes, size_t edges, bool itemLive)
{
	xvalueiter stackBefore = *stack, heapBefore = *heap;
	xrtownershipresult result = {0}; bool live[3] = {true, true, true};
	testRequire(xrtOwnershipInspectReachable(anchors, 3, slots, slotCount, live, &result, NULL, NULL), "iterator physical graph");
	testRequire(result.NodeCount == nodes && result.EdgeCount == edges, "exact cursor/backing/item graph");
	testRequire(result.ExternalRootCount == (size_t)itemLive && result.ReachableAnchorCount == (size_t)itemLive &&
		!live[0] && !live[1] && live[2] == itemLive, "backing alias never roots cursor backwards");
	testRequire(!memcmp(stack, &stackBefore, sizeof(*stack)) && !memcmp(heap, &heapBefore, sizeof(*heap)), "read-only cursors");
}
static void scenario(unsigned kind, bool reverse)
{
	xmemdebugsnapshot before, after;
	xvalueiter stack = {0}, *heap;
	xvaluekey key = {0};
	xvalue* container; xvalue* first; xvalue* alias;
	xrtownershipref anchors[3], slots[3];
	xrtMemDebugSnapshot(&before);
	container = kind == 0 ? xrtValueArray() : kind == 1 ? xrtValueIntMap() : kind == 2 ? xrtValueSet() : xrtValueObject();
	first = xrtValueInt(41);
	testRequire(container && first && insert(container, kind, 0, first), "first owning element");
	testRequire(insert(container, kind, 1, kind == 2 ? xrtValueInt(42) : xrtValueRetain(first)), "second real owning slot");
	testRequire(reverse ? xrtValueIterRBegin(container, &stack) : xrtValueIterBegin(container, &stack), "stack snapshot");
	heap = reverse ? xrtValueIterRCreate(container) : xrtValueIterCreate(container);
	testRequire(heap && xrtValueIterNext(&stack, &key) && xrtValueIterNext(heap, &key), "borrowed current items");
	anchors[0] = xrtValueIterOwnership(&stack); anchors[1] = xrtValueIterOwnership(heap); anchors[2] = xrtValueOwnership(first);
	slots[0] = anchors[0]; slots[1] = anchors[1]; slots[2] = xrtValueOwnership(container);
	inspect(anchors, slots, 3, &stack, heap, kind == 2 ? 6 : 5, 8, false);
	alias = xrtValueRetain(container);
	inspect(anchors, slots, 3, &stack, heap, kind == 2 ? 6 : 5, 8, true); xrtValueRelease(alias);
	alias = xrtValueClone(container);
	testRequire(alias && alias != container, "independent COW shell");
	inspect(anchors, slots, 3, &stack, heap, kind == 2 ? 6 : 5, 8, true); xrtValueRelease(alias);
	for (unsigned fault = 0; fault < 8; ++fault) {
		xrtownershipresult original = {91, 92, 93, 94}, result = original;
		bool live[3] = {true, false, true}, originalLive[3] = {true, false, true};
		xvalueiter stackBefore = stack, heapBefore = *heap;
		bool ok, triggered; xmemdebugsnapshot a, b;
		xrtMemDebugSnapshot(&a); testRequire(xrtMemDebugFailAfter(fault), "arm inspect failure");
		ok = xrtOwnershipInspectReachable(anchors, 3, slots, 3, live, &result, NULL, NULL);
		triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); ++probes;
		if (triggered) {
			++actualFailures;
			testRequire(!ok && !memcmp(&result, &original, sizeof(result)) && !memcmp(live, originalLive, sizeof(live)), "failure atomic outputs");
		} else testRequire(ok && result.EdgeCount == 8 && !result.ExternalRootCount, "successful allocation probe");
		xrtClearError(); xrtMemDebugSnapshot(&b);
		testRequire(a.LiveCount == b.LiveCount && a.LiveBytes == b.LiveBytes, "inspect scratch balance");
		testRequire(!memcmp(&stack, &stackBefore, sizeof(stack)) && !memcmp(heap, &heapBefore, sizeof(*heap)), "failure preserves cursor");
	}
	/* Snapshot isolation: source mutates onto a new backing while both cursors
	 * still own the original, including duplicate element slots. */
	testRequire(insert(container, kind, 2, xrtValueInt(43)), "COW mutation");
	inspect(anchors, slots, 3, &stack, heap, kind == 2 ? 8 : 7, 11, false);
	xrtValueRelease(container);
	inspect(anchors, slots, 2, &stack, heap, kind == 2 ? 5 : 4, 6, false);
	testRequire(xrtValueIterNext(&stack, &key) && xrtValueIterNext(heap, &key), "second snapshot element after source release");
	testRequire(!xrtValueIterNext(&stack, &key) && !xrtValueIterNext(heap, &key) && !xrtGetError(), "no new source element in snapshot");
	inspect(anchors, slots, 2, &stack, heap, kind == 2 ? 5 : 4, 6, false);
	{
		xrtownershipresult original = {91, 92, 93, 94}, result = original;
		int direction = stack.Direction; stack.Direction = 0;
		testRequire(!xrtOwnershipInspect(anchors, 3, slots, 2, &result) && !memcmp(&result, &original, sizeof(result)), "malformed cursor rejected atomically");
		stack.Direction = direction; xrtClearError();
	}
	xrtValueIterEnd(&stack); xrtValueIterDestroy(heap);
	{
		xrtownershipref empty = xrtValueIterOwnership(&stack), absent = xrtValueIterOwnership(NULL);
		xrtownershipresult result;
		testRequire(!absent.Data && !absent.Ops && xrtOwnershipInspect(&empty, 1, &empty, 1, &result) &&
			result.NodeCount == 1 && result.EdgeCount == 1 && !result.ExternalRootCount, "ended unique empty node");
	}
	xrtClearError(); xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
		after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
		before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
		before.UseAfterFreeCount == after.UseAfterFreeCount, "exact per-case memory balance");
}
int main(void)
{
	testRequire(xrtMemDebugEnable(true), "enable allocator checks");
	for (unsigned round = 0; round < 100; ++round)
		for (unsigned kind = 0; kind < 4; ++kind) for (unsigned direction = 0; direction < 2; ++direction)
			scenario(kind, direction != 0);
	testRequire(probes == 6400 && actualFailures == 3200, "all injection points and actual failures executed");
	printf("Value iterator ownership: 800 graphs, %u inspection probes, %u actual failures, exact COW/alias/borrowed-edge and memory checks\n", probes, actualFailures);
	return 0;
}
