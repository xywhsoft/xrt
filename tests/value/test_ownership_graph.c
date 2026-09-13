/* Complete physical graph regression, not the earlier flattened adapter. */
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

typedef struct Node {
	size_t Refs;
	xvalue* Value;
	struct Node* Other;
	unsigned Traces;
	bool SwallowFailure;
} Node;
static const xrtownershipops nodeOps;
static xrtownershipref reference(Node* node) { return (xrtownershipref){node, &nodeOps}; }
static bool countNode(const void* data, size_t* count)
{
	*count = ((const Node*)data)->Refs; return true;
}
static bool traceNode(const void* data, xrtownershipvisitor visit, ptr context)
{
	Node* node = (Node*)data;
	/* Test instrumentation only; production traces never mutate their graph. */
	++node->Traces;
	if (node->Value && !visit(xrtValueOwnership(node->Value), context)) return node->SwallowFailure;
	return !node->Other || visit(reference(node->Other), context);
}
static const xrtownershipops nodeOps = {countNode, traceNode};
static void handleDrop(ptr data, ptr user) { (void)user; --((Node*)data)->Refs; }
static bool handleClone(ptr data, ptr* copy, ptr user)
{
	(void)user; ++((Node*)data)->Refs; *copy = data; return true;
}
static const xvaluehandleops handleOps = {handleClone, handleDrop, NULL, NULL};
static bool handleTrace(const xvalue* value, xrtownershipvisitor visit, ptr context)
{
	ptr data, user; const xvaluehandleops* ops;
	return xrtValueGetHandle(value, &data, &ops, &user) && visit(reference((Node*)data), context);
}
static xvalue* wrap(Node* node)
{
	ptr data = node;
	xvalue* value = xrtValueHandleTake(&data, &handleOps, NULL);
	testRequire(value && !data, "handle take");
	++node->Refs;
	testRequire(xrtValueHandleOwnershipBind(value, handleTrace), "trace bind");
	return value;
}
static void requireInspect(Node* owner, Node* object, size_t reachable, size_t nodes, size_t edges)
{
	xrtownershipref anchor = reference(object), internal = reference(owner);
	xrtownershipresult result = {0};
	owner->Traces = object->Traces = 0;
	testRequire(xrtOwnershipInspect(&anchor, 1, &internal, 1, &result), "complete inspect");
	testRequire(result.ReachableAnchorCount == reachable && result.NodeCount == nodes && result.EdgeCount == edges,
		"physical identity and exact edges");
	testRequire(owner->Traces == 1 && object->Traces == 1, "one trace per node");
}
static void aliasCase(unsigned mode)
{
	Node owner = {1, NULL, NULL, 0, false}, object = {0, NULL, &owner, 0, false};
	xvalue* host = NULL;
	xvalue* wrapper = wrap(&object);
	bool array = mode >= 4;
	++owner.Refs; /* actual object -> owner strong slot */
	if (array) {
		owner.Value = xrtValueArray();
		testRequire(owner.Value && xrtValueArrayAppendNew(owner.Value, wrapper), "array owner");
	} else owner.Value = wrapper;
	if (mode == 1) host = xrtValueRetain(owner.Value);
	if (mode == 2) { host = xrtValueDeepClone(owner.Value); testRequire(host != owner.Value, "independent handle shell"); }
	if (mode == 3) ++object.Refs;
	if (mode == 5) host = xrtValueRetain(owner.Value);
	if (mode == 6) host = xrtValueClone(owner.Value);
	if (mode == 7) host = xrtValueRetain(xrtValueArrayGet(owner.Value, 0));
	requireInspect(&owner, &object, (mode != 0 && mode != 4) ? 1 : 0, array ? 5 : 3, array ? 6 : 4);
	xrtValueRelease(host);
	if (mode == 3) --object.Refs;
	requireInspect(&owner, &object, 0, array ? 5 : 3, array ? 6 : 4);
	xrtValueRelease(owner.Value); owner.Value = NULL;
	--owner.Refs; object.Other = NULL;
	testRequire(object.Refs == 0 && owner.Refs == 1, "balanced graph");
}
static void sharedCase(bool backing)
{
	Node owner = {1, NULL, NULL, 0, false}, second = {1, NULL, NULL, 0, false}, object = {0};
	xvalue* value = wrap(&object);
	xrtownershipref roots[2] = {reference(&owner), reference(&second)}, anchor = reference(&object);
	xrtownershipresult result;
	if (backing) {
		owner.Value = xrtValueArray();
		testRequire(owner.Value && xrtValueArrayAppendNew(owner.Value, value), "shared array");
		second.Value = xrtValueClone(owner.Value);
	} else { owner.Value = value; second.Value = xrtValueRetain(value); }
	testRequire(second.Value && xrtOwnershipInspect(&anchor, 1, roots, 2, &result), "shared inspect");
	testRequire(result.ReachableAnchorCount == 0 && result.NodeCount == (backing ? 7u : 4u), "shared edge identity");
	testRequire(object.Traces == 1 && object.Refs == 1, "one actual handle strong ref");
	xrtValueRelease(owner.Value); xrtValueRelease(second.Value);
	testRequire(object.Refs == 0, "shared cleanup");
}
static void failureCases(void)
{
	Node owner = {.Refs = 1}, object = {0};
	xrtownershipref anchor = reference(&object), roots[2] = {reference(&owner), reference(&owner)};
	xrtownershipresult before = {91,92,93,94}, result;
	const xrtownershipops inconsistent = {countNode, traceNode};
	xrtownershipref conflict[2] = {reference(&owner), {&owner, &inconsistent}};
	owner.Value = wrap(&object);
	result = before;
	testRequire(!xrtOwnershipInspect(&anchor, 1, roots, 2, &result), "overcount rejected");
	testRequire(!memcmp(&before, &result, sizeof(result)) && object.Refs == 1, "overcount atomicity");
	xrtClearError(); result = before;
	testRequire(!xrtOwnershipInspect(conflict, 2, NULL, 0, &result), "descriptor conflict rejected");
	testRequire(!memcmp(&before, &result, sizeof(result)), "conflict atomicity");
	xrtClearError();
	for (unsigned fault = 0; fault < 4; ++fault) {
		xmemdebugsnapshot a, b;
		result = before; xrtMemDebugSnapshot(&a);
		owner.SwallowFailure = true; /* ignored visitor failure must still fail */
		testRequire(xrtMemDebugFailAfter(fault), "arm allocation failure");
		testRequire(!xrtOwnershipInspect(&anchor, 1, roots, 1, &result), "allocation failure rejected");
		testRequire(xrtMemDebugFailTriggered(), "real allocation failure");
		xrtMemDebugFailClear(); xrtClearError(); xrtMemDebugSnapshot(&b);
		testRequire(a.LiveCount == b.LiveCount && a.LiveBytes == b.LiveBytes, "failure allocation balance");
		testRequire(!memcmp(&before, &result, sizeof(result)) && object.Refs == 1, "failure output atomicity");
	}
	xrtValueRelease(owner.Value);
}

static void containerCase(unsigned kind)
{
	Node owner = {.Refs = 1}, object = {0};
	xvalue* item = wrap(&object);
	xvalue* alias;
	xrtownershipref root = reference(&owner), anchor = reference(&object);
	xrtownershipresult result;
	if (kind == 0) {
		owner.Value = xrtValueArray();
		testRequire(owner.Value && xrtValueArrayAppendNew(owner.Value, item), "array slot");
	} else if (kind == 1) {
		owner.Value = xrtValueObject();
		testRequire(owner.Value && xrtValueObjectSetNew(owner.Value, XRT_STR_LITERAL("key"), item), "object slot");
	} else {
		owner.Value = xrtValueIntMap();
		testRequire(owner.Value && xrtValueIntMapSetNew(owner.Value, 7, item), "int map slot");
	}
	testRequire(xrtOwnershipInspect(&anchor, 1, &root, 1, &result) && result.ReachableAnchorCount == 0,
		"container internal only");
	alias = xrtValueClone(owner.Value); testRequire(alias && alias != owner.Value, "COW alias");
	testRequire(xrtOwnershipInspect(&anchor, 1, &root, 1, &result) && result.ReachableAnchorCount == 1,
		"COW backing external root");
	xrtValueRelease(alias); xrtValueRelease(owner.Value); testRequire(!object.Refs, "container balance");
}
static void largeGraph(void)
{
	Node* nodes = (Node*)xrtCalloc(4096, sizeof(Node));
	xrtownershipresult result;
	xrtownershipref root, anchor;
	testRequire(nodes != NULL, "large graph allocate");
	for (unsigned i = 0; i < 4096; ++i) {
		nodes[i].Refs = 1;
		if (i != 4095) nodes[i].Other = &nodes[i+1];
	}
	root = reference(&nodes[0]); anchor = reference(&nodes[4095]);
	testRequire(xrtOwnershipInspect(&anchor, 1, &root, 1, &result) &&
		result.NodeCount == 4096 && result.ReachableAnchorCount == 0, "iterative deep graph");
	++nodes[173].Refs;
	testRequire(xrtOwnershipInspect(&anchor, 1, &root, 1, &result) && result.ReachableAnchorCount == 1,
		"deep external root propagation");
	xrtFree(nodes);
}
static bool finalizerTrace(const void* data, xrtownershipvisitor visit, ptr context)
{ return visit(reference((Node*)data), context); }
static void finalizerDrop(xvalue* value, ptr data)
{
	Node* node = (Node*)data;
	testRequire(!xrtValueObjectFinalizerOwnershipBind(value, finalizerTrace), "finalizing bind rejected");
	xrtClearError(); --node->Refs;
}
static uint64 identityHash(const xvalue* value, ptr data)
{ (void)value; (void)data; return 7; }
static bool identityEqual(const xvalue* a, const xvalue* b, ptr data)
{ (void)data; return a == b; }
static void contextCases(void)
{
	Node native = {.Refs = 1};
	xvalue* object = xrtValueObject();
	xvalue* alias;
	xrtownershipref anchor = reference(&native), root = xrtValueOwnership(object);
	xrtownershipresult before = {71,72,73,74}, result = before;
	testRequire(object && xrtValueObjectFinalizerBind(object, finalizerDrop, &native), "finalizer context bind");
	testRequire(!xrtOwnershipInspect(&anchor, 1, &root, 1, &result) &&
		!memcmp(&before, &result, sizeof(result)), "opaque finalizer context fails atomically");
	xrtClearError();
	alias = xrtValueRetain(object);
	testRequire(!xrtValueObjectFinalizerOwnershipBind(object, finalizerTrace), "shared shell binding rejected");
	xrtClearError(); xrtValueRelease(alias);
	alias = xrtValueClone(object);
	testRequire(alias && !xrtValueObjectFinalizerOwnershipBind(object, finalizerTrace), "shared backing binding rejected");
	xrtClearError(); xrtValueRelease(alias);
	testRequire(xrtValueObjectFinalizerOwnershipBind(object, finalizerTrace), "complete context adapter bind");
	testRequire(!xrtValueObjectFinalizerOwnershipBind(object, finalizerTrace), "rebinding rejected");
	xrtClearError();
	testRequire(xrtOwnershipInspect(&anchor, 1, &root, 1, &result) &&
		result.NodeCount == 3 && result.EdgeCount == 3 && !result.ReachableAnchorCount,
		"finalizer context contributes one actual edge");
	alias = xrtValueClone(object);
	testRequire(alias && xrtOwnershipInspect(&anchor, 1, &root, 1, &result) &&
		result.ReachableAnchorCount == 1, "finalizer context follows COW external backing");
	xrtValueRelease(object); testRequire(native.Refs == 1, "COW shell still owns finalizer context");
	xrtValueRelease(alias); testRequire(native.Refs == 0, "one context finalization");
	object = xrtValueObject(); root = xrtValueOwnership(object); result = before;
	testRequire(object && xrtValueTypeIdBind(object, 7) &&
		xrtValueIdentityBind(object, identityHash, identityEqual, &native), "identity context bind");
	testRequire(!xrtOwnershipInspect(&root, 1, NULL, 0, &result) &&
		!memcmp(&before, &result, sizeof(result)), "opaque identity context fails atomically");
	xrtClearError(); xrtValueRelease(object);
	{ ptr empty = NULL;
		object = xrtValueHandleTake(&empty, &handleOps, &native); root = xrtValueOwnership(object); result = before;
		testRequire(object && !xrtOwnershipInspect(&root, 1, NULL, 0, &result) &&
			!memcmp(&before, &result, sizeof(result)), "null handle does not hide opaque UserData");
		xrtClearError(); xrtValueRelease(object);
	}
}
static void reachableCases(void)
{
	Node a = {.Refs = 1}, b = {.Refs = 1};
	xrtownershipref anchors[4] = {reference(&a), reference(&b), {0}, reference(&b)};
	xrtownershipref root = reference(&a);
	bool bits[4] = {true, false, true, false};
	xrtownershipresult result = {1,2,3,4};
	testRequire(xrtOwnershipInspectReachable(anchors, 4, &root, 1, bits, &result, NULL, NULL) &&
		!bits[0] && bits[1] && !bits[2] && bits[3] && result.ReachableAnchorCount == 1,
		"per-anchor bits preserve order, duplicate identity and null anchors");
	for (unsigned fault = 0; fault < 3; ++fault) {
		bool before[4]; xrtownershipresult original = result;
		memcpy(before, bits, sizeof(bits));
		testRequire(xrtMemDebugFailAfter(fault), "arm reachable scratch failure");
		testRequire(!xrtOwnershipInspectReachable(anchors, 4, &root, 1, bits, &result, NULL, NULL) &&
			xrtMemDebugFailTriggered(), "reachable real allocation fault");
		xrtMemDebugFailClear(); xrtClearError();
		testRequire(!memcmp(bits, before, sizeof(bits)) && !memcmp(&result, &original, sizeof(result)),
			"both reachable outputs failure atomic");
	}
}
int main(void)
{
	xmemdebugsnapshot before, after;
	testRequire(xrtMemDebugEnable(true), "debug enable");
	xrtMemDebugSnapshot(&before);
	for (unsigned round = 0; round < 100; ++round) {
		for (unsigned mode = 0; mode < 8; ++mode) aliasCase(mode);
		sharedCase(false); sharedCase(true); failureCases();
	}
	largeGraph();
	contextCases();
	reachableCases();
	for (unsigned kind = 0; kind < 3; ++kind) containerCase(kind);
	xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
		after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount, "final exact memory balance");
	puts("ownership graph: 800 alias cycles, 200 shared graphs, 400 allocation faults, 200 invalid graphs, depth 4096 passed");
	return 0;
}
