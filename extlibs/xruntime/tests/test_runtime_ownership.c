#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT
#define XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_WEAK
#define XRUNTIME_MODULE_RUNTIME_VALUE_WEAK
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_OBJECT_GRAPH
#define XRUNTIME_MODULE_RUNTIME_OBJECT_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifdef OWNERSHIP_SINGLE
#define XRUNTIME_IMPLEMENTATION
#include "../single/xruntime.h"
#endif
#include "../../../tests/test.h"
#include <xruntime.h>

typedef struct Environment { size_t References; xvalue* Value; } Environment;
static const xrtownershipops environmentOps;
static bool envCount(const void* data, size_t* count)
{ *count = ((const Environment*)data)->References; return true; }
static bool envTrace(const void* data, xrtownershipvisitor visit, ptr context)
{ return visit(xrtValueOwnership(((const Environment*)data)->Value), context); }
static const xrtownershipops environmentOps = {envCount, envTrace};
static bool callableTrace(const void* data, xrtownershipvisitor visit, ptr context)
{ return visit((xrtownershipref){data, &environmentOps}, context); }
static void drop(ptr data) { --((Environment*)data)->References; }
static bool invoke(ptr data, const xrtcallframe* frame, xrtcallresult* result)
{ (void)data; (void)frame; (void)result; return true; }
typedef struct Native { xvalue* Value; xrtobject* Other; } Native;
static unsigned nativeDrops;
static bool nativeTraceFails;
static bool nativeInit(ptr data, const xrttype* type)
{ (void)type; memset(data, 0, sizeof(Native)); return true; }
static void nativeDrop(ptr data, const xrttype* type)
{
	Native* value = (Native*)data; (void)type; ++nativeDrops;
	xrtValueRelease(value->Value); xrtObjectUnref(value->Other);
}
static bool nativeTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
	const Native* value = (const Native*)data;
	if (nativeTraceFails) return false;
	return (!value->Value || visit(xrtValueOwnership(value->Value), context)) &&
		(!value->Other || visit(xrtObjectOwnership(value->Other), context));
}
static xrttype nativeType(void)
{
	static const xrtinstanceops ops = {nativeInit, nativeDrop, NULL};
	xrttype type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.OwnedNative")), .Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("OwnedNative"), .AbiName = XRT_STR_INIT("tests.OwnedNative"),
		.Size = sizeof(ptr), .Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(Native), .InstanceAlign = TEST_ALIGNOF(Native),
		.Ops = xrtObjectValueOps(), .InstanceOps = &ops
	};
	return type;
}
static void nativeCase(unsigned mode)
{
	xrttype type = nativeType();
	xrtobject* owner = xrtObjectCreate(&type);
	xrtobject* object = xrtObjectCreate(&type);
	xrtownershipref anchor, root;
	xrtownershipresult result;
	Native* a; Native* b;
	xvalue* alias;
	testRequire(owner && object && xrtObjectOwnershipTraceBind(owner, nativeTrace) &&
		xrtObjectOwnershipTraceBind(object, nativeTrace), "native full graph bind");
	a = (Native*)xrtObjectData(owner); b = (Native*)xrtObjectData(object);
	b->Other = xrtObjectRef(owner);
	a->Value = xrtValueArray();
	testRequire(a->Value && xrtValueArrayAppendNew(a->Value, xrtValueRuntimeObject(object)), "native Value edge");
	anchor = xrtObjectOwnership(object); root = xrtObjectOwnership(owner);
	xrtObjectUnref(object); /* now owned only by the Value shell */
	alias = mode == 0 ? xrtValueRetain(a->Value) : mode == 1 ? xrtValueClone(a->Value) :
		xrtValueRetain(xrtValueArrayGet(a->Value, 0));
	testRequire(alias && xrtOwnershipInspect(&anchor, 1, &root, 1, &result) && result.ReachableAnchorCount == 1,
		"native Value alias prevents retirement");
	xrtValueRelease(alias);
	testRequire(xrtOwnershipInspect(&anchor, 1, &root, 1, &result) && result.ReachableAnchorCount == 0,
		"native internal cycle is not an external root");
	/* Inspector is read-only: deliberately break this test's own cycle. */
	{ xvalue* slots = a->Value; a->Value = NULL; xrtValueRelease(slots); }
	xrtObjectUnref(owner);
}
static void weakCase(void)
{
	xrttype type = nativeType();
	xrtobject* object = xrtObjectCreate(&type);
	xrtweak weak = {0};
	xvalue* value; xvalue* copy;
	xrtownershipref anchor;
	xrtownershipresult result;
	testRequire(object && xrtWeakInit(&weak, object), "weak source");
	value = xrtValueWeakTake(&weak); testRequire(value && !weak.Control, "weak take");
	copy = xrtValueDeepClone(value); testRequire(copy && copy != value, "independent weak shell");
	anchor = xrtValueOwnership(copy);
	testRequire(xrtOwnershipInspect(&anchor, 1, NULL, 0, &result) && result.NodeCount == 1 &&
		result.EdgeCount == 0 && result.ReachableAnchorCount == 1, "weak target is not a strong edge");
	testRequire(xrtObjectRefCount(object) == 1, "weak trace does not retain target");
	xrtObjectUnref(object);
	testRequire(xrtValueWeakExpired(value) && xrtValueWeakExpired(copy), "target can expire while weak shells live");
	testRequire(xrtOwnershipInspect(&anchor, 1, &anchor, 1, &result) &&
		result.NodeCount == 1 && !result.ReachableAnchorCount, "expired weak shell is still a leaf");
	xrtValueRelease(copy); xrtValueRelease(value);
}
static void collectedCase(unsigned mode)
{
	xrttype type = nativeType();
	xrtobjectgraph* graph = xrtObjectGraphCreate();
	xrtobject* a = xrtObjectCreate(&type); xrtobject* b = xrtObjectCreate(&type);
	Native* payload;
	xvalue* alias = NULL;
	xrtobject* nativeAlias = NULL;
	xrtweak weakA = {0}, weakB = {0};
	xrtobjectgraphownedresult result;
	unsigned drops = nativeDrops;
	testRequire(graph && a && b && xrtObjectOwnershipTraceBind(a, nativeTrace) &&
		xrtObjectOwnershipTraceBind(b, nativeTrace), "collect-owned setup");
	payload = (Native*)xrtObjectData(a); payload->Value = xrtValueArray();
	testRequire(payload->Value && xrtValueArrayAppendNew(payload->Value, xrtValueRuntimeObject(b)), "collect-owned Value edge");
	((Native*)xrtObjectData(b))->Other = xrtObjectRef(a);
	testRequire(xrtObjectGraphTrack(graph, a) && xrtObjectGraphTrack(graph, b) &&
		xrtWeakInit(&weakA, a) && xrtWeakInit(&weakB, b), "collect-owned tracking");
	if (mode == 1) alias = xrtValueRetain(payload->Value);
	if (mode == 2) alias = xrtValueClone(payload->Value);
	if (mode == 3) alias = xrtValueRetain(xrtValueArrayGet(payload->Value, 0));
	if (mode == 4) alias = xrtValueDeepClone(xrtValueArrayGet(payload->Value, 0));
	if (mode == 5) nativeAlias = xrtObjectRef(b);
	if (mode > 0 && mode < 5) testRequire(alias != NULL, "collect-owned external alias");
	xrtObjectUnref(a); xrtObjectUnref(b); /* only actual cycle and optional host remain */
	if (mode != 0) {
		testRequire(xrtObjectGraphCollectOwned(graph, &result) && result.TrackedCount == 2 &&
			result.CollectedCount == 0 && result.Ownership.ReachableAnchorCount == 2,
			"real collector preserves both objects reachable through host alias");
		testRequire(nativeDrops == drops && !xrtWeakExpired(&weakA) && !xrtWeakExpired(&weakB), "no premature finalization");
		xrtValueRelease(alias); xrtObjectUnref(nativeAlias);
	}
	if (mode == 0) {
		xrtobjectgraphownedresult unchanged;
		memset(&unchanged, 0x5a, sizeof(unchanged));
		for (unsigned fault = 0; fault < 7; ++fault) {
			xmemdebugsnapshot before, after;
			result = unchanged; xrtMemDebugSnapshot(&before);
			testRequire(xrtMemDebugFailAfter(fault), "arm collection allocation fault");
			testRequire(!xrtObjectGraphCollectOwned(graph, &result) && xrtMemDebugFailTriggered(), "real collection allocation failure");
			xrtMemDebugFailClear(); xrtClearError(); xrtMemDebugSnapshot(&after);
			testRequire(!memcmp(&result, &unchanged, sizeof(result)) && xrtObjectGraphCount(graph) == 2 &&
				nativeDrops == drops && !xrtWeakExpired(&weakA) && !xrtWeakExpired(&weakB), "failed collection leaves objects and membership unchanged");
			testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes, "failed collection releases all scratch and pins");
		}
		result = unchanged; nativeTraceFails = true;
		testRequire(!xrtObjectGraphCollectOwned(graph, &result) && !memcmp(&result, &unchanged, sizeof(result)), "trace failure has no partial commit");
		nativeTraceFails = false; xrtClearError();
		testRequire(nativeDrops == drops && xrtObjectGraphCount(graph) == 2, "trace failure did not finalize");
	}
	testRequire(xrtObjectGraphCollectOwned(graph, &result) && result.TrackedCount == 2 &&
		result.CollectedCount == 2 && !result.Ownership.ReachableAnchorCount, "host-free internal cycle actually collected");
	testRequire(nativeDrops == drops + 2 && xrtObjectGraphCount(graph) == 0 &&
		xrtWeakExpired(&weakA) && xrtWeakExpired(&weakB), "exactly once native finalization after host release");
	testRequire(xrtObjectGraphCollectOwned(graph, &result) && result.CollectedCount == 0, "empty collection is idempotent");
	xrtWeakUnit(&weakA); xrtWeakUnit(&weakB); xrtObjectGraphDestroy(graph);
}
static void collectedSharedCase(bool cow)
{
	xrttype type = nativeType();
	xrtobjectgraph* graph = xrtObjectGraphCreate();
	xrtobject* a = xrtObjectCreate(&type); xrtobject* b = xrtObjectCreate(&type); xrtobject* c = xrtObjectCreate(&type);
	Native *pa, *pb, *pc;
	xrtobjectgraphownedresult result;
	unsigned drops = nativeDrops;
	testRequire(graph && a && b && c && xrtObjectOwnershipTraceBind(a, nativeTrace) &&
		xrtObjectOwnershipTraceBind(b, nativeTrace) && xrtObjectOwnershipTraceBind(c, nativeTrace), "shared collection setup");
	pa = (Native*)xrtObjectData(a); pb = (Native*)xrtObjectData(b); pc = (Native*)xrtObjectData(c);
	pa->Value = xrtValueArray();
	testRequire(pa->Value && xrtValueArrayAppendNew(pa->Value, xrtValueRuntimeObject(c)), "shared physical target");
	pb->Value = cow ? xrtValueClone(pa->Value) : xrtValueRetain(pa->Value);
	pc->Other = xrtObjectRef(a); pc->Value = xrtValueRuntimeObject(b);
	testRequire(pb->Value && pc->Value && xrtObjectGraphTrack(graph, a) &&
		xrtObjectGraphTrack(graph, b) && xrtObjectGraphTrack(graph, c), "shared closed graph");
	xrtObjectUnref(a); xrtObjectUnref(b); xrtObjectUnref(c);
	testRequire(xrtObjectGraphCollectOwned(graph, &result) && result.CollectedCount == 3 &&
		nativeDrops == drops + 3 && xrtObjectGraphCount(graph) == 0,
		"shared shell/backing edges are not overcounted by actual collector");
	xrtObjectGraphDestroy(graph);
}
static void collectionBoundaryCase(void)
{
	xrttype type = nativeType();
	xrtobjectgraph* graph = xrtObjectGraphCreate();
	xrtobject* a = xrtObjectCreate(&type); xrtobject* b = xrtObjectCreate(&type);
	xrtobject* external = xrtObjectCreate(&type);
	xrtweak weak = {0};
	xrtobjectgraphownedresult result;
	unsigned drops = nativeDrops;
	testRequire(graph && a && b && external && xrtObjectOwnershipTraceBind(a, nativeTrace) &&
		xrtObjectOwnershipTraceBind(b, nativeTrace) && xrtObjectOwnershipTraceBind(external, nativeTrace), "boundary setup");
	((Native*)xrtObjectData(a))->Other = xrtObjectRef(b);
	((Native*)xrtObjectData(b))->Other = xrtObjectRef(a);
	testRequire(xrtObjectGraphTrack(graph, a) && xrtObjectGraphTrack(graph, external) &&
		xrtWeakInit(&weak, b), "boundary tracking");
	xrtObjectUnref(a); xrtObjectUnref(b);
	testRequire(xrtObjectGraphCollectOwned(graph, &result) && !result.CollectedCount &&
		nativeDrops == drops, "untracked native cycle is a collection boundary root");
	b = xrtWeakLock(&weak);
	testRequire(b && xrtObjectData(((Native*)xrtObjectData(b))->Other), "weak lock outside domain sees no finalized peer");
	/* Explicitly bring the peer into the domain; drop the temporary root. */
	testRequire(xrtObjectGraphTrack(graph, b), "complete the collection domain");
	xrtObjectUnref(b);
	testRequire(xrtObjectGraphCollectOwned(graph, &result) && result.TrackedCount == 3 &&
		result.CollectedCount == 2 && result.Ownership.ReachableAnchorCount == 1 &&
		xrtObjectGraphCount(graph) == 1 && nativeDrops == drops + 2,
		"collect only the unreachable subset, preserve unrelated native root");
	testRequire(xrtWeakExpired(&weak), "collected weak target expired");
	xrtWeakUnit(&weak); xrtObjectUnref(external);
	testRequire(nativeDrops == drops + 3 && !xrtObjectGraphCount(graph), "boundary object balance");
	xrtObjectGraphDestroy(graph);
}
int main(void)
{
	xmemdebugsnapshot before, after;
	testRequire(xrtMemDebugEnable(true), "enable tracking"); xrtMemDebugSnapshot(&before);
	for (unsigned mode = 0; mode < 5; ++mode) {
		Environment env = {1, NULL};
		xrtcallable* callable = xrtCallableCreate(NULL, invoke, &env, drop);
		xvalue* owner;
		xvalue* alias;
		xrtownershipref anchor = {&env, &environmentOps}, root;
		xrtownershipresult result = {91,92,93,94}, unchanged = result;
		testRequire(callable != NULL, "create callable");
		/* Callable -> environment -> scalar Value is complete, not opaque. */
		env.Value = xrtValueInt(42); testRequire(env.Value != NULL, "env value");
		if (mode != 0) testRequire(xrtCallableOwnershipTraceBind(callable, callableTrace), "bind environment graph");
		owner = xrtValueCallableTake(&callable); testRequire(owner && !callable, "take callable");
		root = xrtValueOwnership(owner);
		if (mode == 0) {
			testRequire(!xrtOwnershipInspect(&anchor, 1, &root, 1, &result), "opaque environment rejected");
			testRequire(!memcmp(&result, &unchanged, sizeof(result)), "opaque result unchanged");
			xrtClearError();
		} else {
			testRequire(xrtOwnershipInspect(&anchor, 1, &root, 1, &result) && !result.ReachableAnchorCount,
				"callable internal ownership exact");
			alias = mode == 1 ? xrtValueRetain(owner) : xrtValueDeepClone(owner);
			testRequire(alias != NULL, "callable alias");
			if (mode != 1) testRequire(alias != owner, "independent callable shell");
			testRequire(xrtOwnershipInspect(&anchor, 1, &root, 1, &result) && result.ReachableAnchorCount == 1,
				"external callable owner detected");
			if (mode == 3) {
				xrtownershipref slots[2] = {root, xrtValueOwnership(alias)};
				testRequire(xrtOwnershipInspect(&anchor, 1, slots, 2, &result) && !result.ReachableAnchorCount,
					"independent handle clones share one callable environment edge");
			}
			xrtValueRelease(alias);
		}
		xrtValueRelease(owner); testRequire(env.References == 0, "one environment drop");
		xrtValueRelease(env.Value);
	}
	xrtMemDebugSnapshot(&after);
	for (unsigned mode = 0; mode < 3; ++mode) nativeCase(mode);
	weakCase();
	testRequire(nativeDrops == 7, "native objects released exactly once");
	for (unsigned round = 0; round < 100; ++round) {
		for (unsigned mode = 0; mode < 6; ++mode) collectedCase(mode);
		collectedSharedCase(false); collectedSharedCase(true);
		collectionBoundaryCase();
	}
	xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes, "runtime graph memory balance");
	puts("runtime callable and native object ownership graph passed");
	puts("physical collector: 600 alias/cycle cases, 200 shared graphs, 700 allocation faults and 100 trace faults passed");
	puts("physical collector: 100 cross-domain weak-lock and partial-collection cases passed");
	return 0;
}
