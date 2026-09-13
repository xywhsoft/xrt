#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRT_MODULE_VALUE_COLLECTION
#define XRT_MODULE_VALUE_COLLECTION
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifdef OWNERSHIP_SCOPE_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include "../test_thread.h"

static bool empty(const xrtownershipscope* pScope)
{
	return pScope->Self == NULL && pScope->Parent == NULL && pScope->Thread == 0 &&
		pScope->Children == 0 && pScope->Mode == 0;
}

typedef struct scope_worker {
	xrtownershipscope* Scope;
	xvalue* Weak;
	xvalue* Result;
	xatomic32 Stage;
	unsigned Kind;
} scope_worker;

static int worker(ptr pData)
{
	scope_worker* pWork = pData;
	xrtownershipscope Freeze = {0};
	if (pWork->Kind == 2) {
		testRequire(!xrtOwnershipScopeEnd(pWork->Scope), "wrong-thread end must fail");
		xrtClearError(); return 0;
	}
	testRequire(!xrtOwnershipFreezeTryBegin(&Freeze) && empty(&Freeze) && xrtGetError() == NULL,
		"another thread observes exclusive admission busy without diagnostic");
	xrtAtomic32Store(&pWork->Stage, 1, XMEMORY_RELEASE);
	pWork->Result = xrtValueWeakRefLock(pWork->Weak);
	testRequire(pWork->Result != NULL && xrtGetError() == NULL, "native weak lock completes normally");
	xrtAtomic32Store(&pWork->Stage, 2, XMEMORY_RELEASE);
	return 0;
}

static void admission(void)
{
	xrtownershipscope Mutation = {0}, Nested = {0}, Freeze = {0}, Copy;
	volatile int32 iCount = 1;
	testthread Thread = {0}; scope_worker Work = {0};
	testRequire(!xrtOwnershipMutationBegin(NULL), "NULL scope refused"); xrtClearError();
	testRequire(xrtOwnershipMutationBegin(&Mutation), "mutation admitted");
	Copy = Mutation;
	testRequire(!xrtOwnershipScopeEnd(&Copy), "copied live scope refused"); xrtClearError();
	testRequire(!xrtOwnershipMutationBegin(&Mutation), "reused live scope refused"); xrtClearError();
	xrtSetErrorKind(XERR_TIMEOUT);
	{ const xerror* pPrior = xrtGetError();
	  testRequire(!xrtOwnershipFreezeTryBegin(&Freeze) && empty(&Freeze) && xrtGetError() == pPrior,
		"self upgrade is nonblocking and preserves primary diagnostic"); }
	xrtClearError();
	testRequire(xrtOwnershipMutationBegin(&Nested) && xrtRefRetain(&iCount) == 2 &&
		xrtRefRelease(&iCount) == 1, "nested mutation makes progress without upgrade deadlock");
	Work.Scope = &Mutation; Work.Kind = 2; Thread.Proc = worker; Thread.Data = &Work;
	/* Only shared admission is held here: never join a worker under freeze. */
	testThreadsStart(&Thread, 1); testThreadsJoin(&Thread, 1);
	testRequire(Thread.Result == 0 && !empty(&Mutation), "wrong thread did not release mutation");
	testRequire(xrtOwnershipScopeEnd(&Mutation) && xrtOwnershipScopeEnd(&Nested),
		"independent mutations need not be LIFO");
	testRequire(xrtOwnershipFreezeTryBegin(&Freeze), "freeze after complete mutations");
	testRequire(xrtOwnershipMutationBegin(&Nested), "collector balanced cursor scope admitted");
	testRequire(!xrtOwnershipScopeEnd(&Freeze), "cannot end freeze with nested transition live"); xrtClearError();
	testRequire(!xrtOwnershipFreezeTryBegin(&Mutation) && empty(&Mutation), "recursive freeze is busy");
	testRequire(xrtOwnershipScopeEnd(&Nested) && xrtOwnershipScopeEnd(&Freeze), "nested then freeze end");
	testRequire(empty(&Freeze) && !xrtOwnershipScopeEnd(&Freeze), "double end refused"); xrtClearError();
}

/* The native worker is not wrapped in a language entry or root-registration
 * API. Its existing xrtValueWeakRefLock reaches the actual fenced strong CAS. */
static void promotion(bool bRetire)
{
	xrtownershipscope Freeze = {0};
	scope_worker Work = {0}; testthread Thread = {0};
	xmemdebugsnapshot Before, After;
	xvalue* pValue;
	xrtMemDebugSnapshot(&Before);
	pValue = xrtValueObject(); Work.Weak = xrtValueWeakRef(pValue);
	testRequire(pValue != NULL && Work.Weak != NULL, "weak-promotion fixture");
	xrtAtomic32Init(&Work.Stage, 0); Thread.Proc = worker; Thread.Data = &Work;
	testRequire(xrtOwnershipFreezeTryBegin(&Freeze), "freeze before native promotion");
	testThreadsStart(&Thread, 1);
	while (xrtAtomic32Load(&Work.Stage, XMEMORY_ACQUIRE) == 0) testThreadYield();
	for (unsigned i = 0; i < 100; ++i) testThreadYield();
	testRequire(xrtAtomic32Load(&Work.Stage, XMEMORY_ACQUIRE) == 1,
		"weak promotion cannot cross an active freeze");
	if (bRetire) { xrtValueRelease(pValue); pValue = NULL; }
	testRequire(xrtOwnershipScopeEnd(&Freeze), "release exclusive admission before joining worker");
	testThreadsJoin(&Thread, 1); testRequire(Thread.Result == 0, "native promotion worker");
	testRequire(bRetire ? xrtValueIs(Work.Result, XVALUE_NULL) : Work.Result == pValue,
		"native promotion linearizes after retained-or-retired decision");
	xrtValueRelease(Work.Result); xrtValueRelease(Work.Weak); xrtValueRelease(pValue);
	xrtMemDebugSnapshot(&After);
	testRequire(Before.LiveCount == After.LiveCount && Before.LiveBytes == After.LiveBytes &&
		After.AllocCount-Before.AllocCount == After.FreeCount-Before.FreeCount, "promotion allocation balance");
}

typedef struct stress_worker { volatile int32* Count; xatomic32* Done; } stress_worker;
static int stress(ptr pData)
{
	stress_worker* pWork = pData;
	for (unsigned i = 0; i < 20000; ++i) {
		testRequire(xrtRefRetain(pWork->Count) >= 2, "concurrent retain");
		testRequire(xrtRefRelease(pWork->Count) >= 1, "concurrent release");
	}
	xrtAtomic32FetchAdd(pWork->Done, 1, XMEMORY_RELEASE); return 0;
}

static void concurrent_refs(void)
{
	volatile int32 Count = 1; xatomic32 Done;
	stress_worker Work = {&Count, &Done}; testthread Threads[4] = {0};
	unsigned freezes = 0;
	xrtAtomic32Init(&Done, 0);
	for (unsigned i = 0; i < 4; ++i) { Threads[i].Proc = stress; Threads[i].Data = &Work; }
	testThreadsStart(Threads, 4);
	do {
		xrtownershipscope Freeze = {0};
		if (xrtOwnershipFreezeTryBegin(&Freeze)) {
			int32 observed = Count;
			testRequire(observed >= 1 && observed <= 5, "freeze observes actual reference population");
			for (unsigned i = 0; i < 50; ++i) testRequire(Count == observed, "reference population stable until scope end");
			testRequire(xrtOwnershipScopeEnd(&Freeze), "stress freeze end"); ++freezes;
		}
		testThreadYield();
	} while (xrtAtomic32Load(&Done, XMEMORY_ACQUIRE) != 4 || freezes < 100);
	testThreadsJoin(Threads, 4);
	testRequire(Count == 1, "four-thread reference count balance");
	for (unsigned i = 0; i < 4; ++i) testRequire(Threads[i].Result == 0, "reference stress worker");
}

typedef struct admission_group {
	xatomic32 Entered;
	xatomic32 Continue;
	xatomic32 Exited;
} admission_group;
typedef struct admission_member { admission_group* Group; unsigned Index; } admission_member;
static int grouped_admission(ptr data)
{
	admission_member* member = data; admission_group* group = member->Group;
	xrtownershipscope Mutation = {0};
	testRequire(xrtOwnershipMutationBegin(&Mutation), "group mutation admission");
	xrtAtomic32FetchAdd(&group->Entered, 1, XMEMORY_RELEASE);
	while (xrtAtomic32Load(&group->Continue, XMEMORY_ACQUIRE) <= member->Index) testThreadYield();
	testRequire(xrtOwnershipScopeEnd(&Mutation), "group mutation end");
	xrtAtomic32FetchAdd(&group->Exited, 1, XMEMORY_RELEASE); return 0;
}
static void colliding_admissions(void)
{
	/* 65 live native threads force a collision in the production 64 lanes,
	 * without accessing implementation state or registering native roots. */
	admission_group Group; admission_member Members[65]; testthread Threads[65] = {0};
	xrtownershipscope Freeze = {0};
	xrtAtomic32Init(&Group.Entered, 0); xrtAtomic32Init(&Group.Continue, 0); xrtAtomic32Init(&Group.Exited, 0);
	for (unsigned i = 0; i < 65; ++i) {
		Members[i].Group = &Group; Members[i].Index = i;
		Threads[i].Proc = grouped_admission; Threads[i].Data = &Members[i];
	}
	testThreadsStart(Threads, 65);
	while (xrtAtomic32Load(&Group.Entered, XMEMORY_ACQUIRE) != 65) testThreadYield();
	for (uint32 i = 0; i < 65; ++i) {
		testRequire(!xrtOwnershipFreezeTryBegin(&Freeze) && empty(&Freeze), "every remaining reader excludes freeze");
		xrtAtomic32Store(&Group.Continue, i + 1, XMEMORY_RELEASE);
		while (xrtAtomic32Load(&Group.Exited, XMEMORY_ACQUIRE) != i + 1) testThreadYield();
	}
	testThreadsJoin(Threads, 65);
	for (unsigned i = 0; i < 65; ++i) testRequire(Threads[i].Result == 0, "colliding admission worker");
	testRequire(xrtOwnershipFreezeTryBegin(&Freeze) && xrtOwnershipScopeEnd(&Freeze), "all colliding scopes returned");
}

typedef struct freeze_member { xatomic32* Active; unsigned Successes; } freeze_member;
static int competing_freeze(ptr data)
{
	freeze_member* member = data; unsigned attempts = 0;
	while (attempts++ < 2000 || member->Successes < 30) {
		xrtownershipscope Freeze = {0};
		if (xrtOwnershipFreezeTryBegin(&Freeze)) {
			volatile int32 refs = 1;
			testRequire(xrtAtomic32FetchAdd(member->Active, 1, XMEMORY_ACQ_REL) == 0, "at most one freeze owner");
			testRequire(xrtRefRetain(&refs) == 2 && xrtRefRelease(&refs) == 1, "freeze owner can use nested cursor");
			testRequire(xrtAtomic32FetchSub(member->Active, 1, XMEMORY_ACQ_REL) == 1, "freeze remains exclusive");
			testRequire(xrtOwnershipScopeEnd(&Freeze), "competing freeze returned"); ++member->Successes;
		} else testRequire(empty(&Freeze) && xrtGetError() == NULL, "contended admission leaves no partial freeze");
		testThreadYield();
	}
	return 0;
}
static void competing_collectors(void)
{
	xatomic32 Active; freeze_member Members[4] = {0}; testthread Threads[4] = {0};
	xrtAtomic32Init(&Active, 0);
	for (unsigned i = 0; i < 4; ++i) {
		Members[i].Active = &Active; Threads[i].Proc = competing_freeze; Threads[i].Data = &Members[i];
	}
	testThreadsStart(Threads, 4); testThreadsJoin(Threads, 4);
	for (unsigned i = 0; i < 4; ++i) testRequire(Threads[i].Result == 0 && Members[i].Successes >= 30, "each collector made progress");
	testRequire(xrtAtomic32Load(&Active, XMEMORY_ACQUIRE) == 0, "all competing freezes ended");
}

static void no_allocation_and_inspection(void)
{
	xrtownershipscope Freeze = {0}, Mutation = {0};
	xvalue* pValue = xrtValueObject(); xvalue* pAlias = xrtValueClone(pValue);
	xrtownershipref Anchor = xrtValueOwnership(pValue);
	xrtownershipresult Result = {0};
	volatile int32 Count = 1;
	testRequire(pValue != NULL && pAlias != NULL && xrtMemDebugFailAfter(0), "real OOM armed");
	testRequire(xrtOwnershipMutationBegin(&Mutation) && !xrtOwnershipFreezeTryBegin(&Freeze) &&
		xrtRefRetain(&Count) == 2 && xrtRefRelease(&Count) == 1 && xrtOwnershipScopeEnd(&Mutation) &&
		xrtOwnershipFreezeTryBegin(&Freeze), "allocation-free admission and reference updates");
	testRequire(!xrtMemDebugFailTriggered(), "scope needs no heap or TLS allocation");
	testRequire(!xrtOwnershipInspect(&Anchor, 1, NULL, 0, &Result) && xrtMemDebugFailTriggered(),
		"inspection actual allocation failure does not release enclosing freeze");
	xrtMemDebugFailClear(); xrtClearError();
	testRequire(!xrtOwnershipFreezeTryBegin(&Mutation) && empty(&Mutation), "freeze still owned after inspection error");
	/* Balanced Trace cursors can nest, without releasing the frozen parent. */
	testRequire(xrtOwnershipInspect(&Anchor, 1, NULL, 0, &Result) &&
		Result.NodeCount == 2 && Result.EdgeCount == 1 && Result.ExternalRootCount == 2,
		"balanced cursor inspection under a retained freeze");
	testRequire(xrtOwnershipScopeEnd(&Freeze), "inspection freeze end");
	xrtValueRelease(pAlias); xrtValueRelease(pValue);
}

typedef struct container_worker {
	xvalue* Target;
	xvalue* Source;
	xvalue* Item;
	xvalue* Query;
	xvalue* Taken;
	xatomic32 Stage;
	unsigned Kind;
} container_worker;

static int container_mutate(ptr pData)
{
	container_worker* pWork = pData;
	xrtownershipscope Freeze = {0}; bool ok = true;
	testRequire(!xrtOwnershipFreezeTryBegin(&Freeze) && empty(&Freeze), "container writer observes freeze");
	xrtAtomic32Store(&pWork->Stage, 1, XMEMORY_RELEASE);
	switch (pWork->Kind) {
		case 0: ok = xrtValueArrayAppend(pWork->Target, pWork->Item); break;
		case 1: ok = xrtValueArraySet(pWork->Target, 0, pWork->Item); break;
		case 2: ok = xrtValueArrayExtend(pWork->Target, pWork->Source); break;
		case 3: pWork->Taken = xrtValueArrayTake(pWork->Target, 0); break;
		case 4: ok = xrtValueObjectSet(pWork->Target, XRT_STR_LITERAL("second"), pWork->Item); break;
		case 5: ok = xrtValueObjectMerge(pWork->Target, pWork->Source, XVALUE_MERGE_REPLACE); break;
		case 6: pWork->Taken = xrtValueObjectTake(pWork->Target, XRT_STR_LITERAL("first")); break;
		case 7: ok = xrtValueClear(pWork->Target); break;
		case 8: ok = xrtValueIntMapSet(pWork->Target, 2, pWork->Item); break;
		case 9: ok = xrtValueIntMapMerge(pWork->Target, pWork->Source, XVALUE_MERGE_REPLACE); break;
		case 10: pWork->Taken = xrtValueIntMapTake(pWork->Target, 1); break;
		case 11: ok = xrtValueClear(pWork->Target); break;
		case 12: ok = xrtValueSetAdd(pWork->Target, pWork->Item); break;
		case 13: ok = xrtValueSetMerge(pWork->Target, pWork->Source); break;
		case 14: pWork->Taken = xrtValueSetTake(pWork->Target, pWork->Query); break;
		case 15: ok = xrtValueClear(pWork->Target); break;
		default: testRequire(false, "unexpected mutation kind");
	}
	if (pWork->Kind == 3 || pWork->Kind == 6 || pWork->Kind == 10 || pWork->Kind == 14)
		ok = pWork->Taken != NULL;
	testRequire(ok && xrtGetError() == NULL, "native container mutation completes");
	xrtAtomic32Store(&pWork->Stage, 2, XMEMORY_RELEASE); return 0;
}

static xvalue* container(unsigned kind, xvalue* item, bool second)
{
	xvalue* value = kind == 0 ? xrtValueArray() : kind == 1 ? xrtValueObject() :
		kind == 2 ? xrtValueIntMap() : xrtValueSet();
	bool ok = value != NULL;
	if (ok) switch (kind) {
		case 0: ok = xrtValueArrayAppend(value, item); break;
		case 1: ok = xrtValueObjectSet(value, second ? XRT_STR_LITERAL("second") : XRT_STR_LITERAL("first"), item); break;
		case 2: ok = xrtValueIntMapSet(value, second ? 2 : 1, item); break;
		case 3: ok = xrtValueSetAdd(value, item); break;
	}
	testRequire(ok, "create container fixture"); return value;
}

static void container_transition(unsigned kind)
{
	static const size_t counts[16] = {2,1,2,0,2,2,0,0,2,2,0,0,2,2,0,0};
	container_worker Work = {0}; testthread Thread = {0};
	xrtownershipscope Freeze = {0}; xrtownershipresult BeforeGraph = {0}, During = {0};
	xrtownershipref Anchor; xvalue* Alias; xmemdebugsnapshot Before, After;
	xrtMemDebugSnapshot(&Before);
	Work.Kind = kind; Work.Item = xrtValueInt(2); Work.Query = xrtValueInt(1);
	Work.Target = container(kind / 4, Work.Query, false);
	Work.Source = container(kind / 4, Work.Item, true);
	Alias = xrtValueClone(Work.Target); testRequire(Alias != NULL, "native COW alias");
	Anchor = xrtValueOwnership(Work.Target); xrtAtomic32Init(&Work.Stage, 0);
	Thread.Proc = container_mutate; Thread.Data = &Work;
	testRequire(xrtOwnershipFreezeTryBegin(&Freeze) && xrtOwnershipInspect(&Anchor, 1, NULL, 0, &BeforeGraph), "freeze container graph");
	testThreadsStart(&Thread, 1);
	while (xrtAtomic32Load(&Work.Stage, XMEMORY_ACQUIRE) == 0) testThreadYield();
	for (unsigned i = 0; i < 100; ++i) testThreadYield();
	testRequire(xrtAtomic32Load(&Work.Stage, XMEMORY_ACQUIRE) == 1 && xrtValueCount(Work.Target) == 1 &&
		xrtOwnershipInspect(&Anchor, 1, NULL, 0, &During) && !memcmp(&BeforeGraph, &During, sizeof(During)),
		"complete COW/field/merge/take transition waits outside frozen graph");
	testRequire(xrtOwnershipScopeEnd(&Freeze), "release graph before native mutation");
	testThreadsJoin(&Thread, 1);
	testRequire(Thread.Result == 0 && xrtValueCount(Work.Target) == counts[kind] && xrtValueCount(Alias) == 1,
		"mutation committed without changing old COW backing");
	xrtValueRelease(Work.Taken); xrtValueRelease(Work.Item); xrtValueRelease(Work.Query);
	xrtValueRelease(Work.Target); xrtValueRelease(Work.Source); xrtValueRelease(Alias);
	xrtMemDebugSnapshot(&After);
	testRequire(Before.LiveCount == After.LiveCount && Before.LiveBytes == After.LiveBytes &&
		After.AllocCount-Before.AllocCount == After.FreeCount-Before.FreeCount, "container transition allocation balance");
}

typedef struct drop_worker { xvalue* Value; xatomic32 Entered; xatomic32 Continue; } drop_worker;
static void in_flight_drop(xvalue* value, ptr data)
{
	drop_worker* work = data; volatile int32 refs = 1; (void)value;
	xrtAtomic32Store(&work->Entered, 1, XMEMORY_RELEASE);
	while (!xrtAtomic32Load(&work->Continue, XMEMORY_ACQUIRE)) testThreadYield();
	testRequire(xrtRefRetain(&refs) == 2 && xrtRefRelease(&refs) == 1,
		"nested mutation progresses after another thread's refused freeze");
}
static int clear_while_dropping(ptr data)
{
	drop_worker* work = data;
	testRequire(xrtValueClear(work->Value) && xrtGetError() == NULL, "clear returns after finalizer tail"); return 0;
}
static void unfinished_transition(void)
{
	drop_worker Work = {0}; testthread Thread = {0}; xrtownershipscope Freeze = {0};
	xvalue* child = xrtValueObject(); Work.Value = xrtValueArray();
	xrtAtomic32Init(&Work.Entered, 0); xrtAtomic32Init(&Work.Continue, 0);
	testRequire(child != NULL && Work.Value != NULL && xrtValueObjectFinalizerBind(child, in_flight_drop, &Work) &&
		xrtValueArrayAppendNew(Work.Value, child), "in-flight field drop fixture");
	Thread.Proc = clear_while_dropping; Thread.Data = &Work; testThreadsStart(&Thread, 1);
	while (!xrtAtomic32Load(&Work.Entered, XMEMORY_ACQUIRE)) testThreadYield();
	for (unsigned i = 0; i < 100; ++i)
		testRequire(!xrtOwnershipFreezeTryBegin(&Freeze) && empty(&Freeze) && xrtGetError() == NULL,
			"no inspection admission in the middle of a field destructor");
	xrtAtomic32Store(&Work.Continue, 1, XMEMORY_RELEASE); testThreadsJoin(&Thread, 1);
	testRequire(Thread.Result == 0 && xrtOwnershipFreezeTryBegin(&Freeze), "admit only after complete field transition");
	testRequire(xrtOwnershipScopeEnd(&Freeze) && xrtValueCount(Work.Value) == 0, "field transition ended");
	xrtValueRelease(Work.Value);
}

static uint64 in_flight_hash(const xvalue* value, ptr data)
{
	(void)value; in_flight_drop(NULL, data); return 7;
}
static bool identity_equal(const xvalue* left, const xvalue* right, ptr data)
{
	(void)data; return left == right;
}
static int hash_while_waiting(ptr data)
{
	drop_worker* work = data; uint64 hash = 0;
	/* Public identity hashes include the TypeId domain, not just callback bits. */
	const uint64 expected = UINT64_C(7) ^ (UINT64_C(42) + UINT64_C(0x9E3779B97F4A7C15) +
		(UINT64_C(7) << 6) + (UINT64_C(7) >> 2));
	testRequire(xrtValueHash(work->Value, &hash) && hash == expected && xrtGetError() == NULL,
		"direct identity hash returns after callback tail"); return 0;
}
static void unfinished_hash(void)
{
	drop_worker Work = {0}; testthread Thread = {0}; xrtownershipscope Freeze = {0};
	Work.Value = xrtValueObject();
	xrtAtomic32Init(&Work.Entered, 0); xrtAtomic32Init(&Work.Continue, 0);
	testRequire(Work.Value != NULL && xrtValueTypeIdBind(Work.Value, 42) &&
		xrtValueIdentityBind(Work.Value, in_flight_hash, identity_equal, &Work), "in-flight identity hash fixture");
	Thread.Proc = hash_while_waiting; Thread.Data = &Work; testThreadsStart(&Thread, 1);
	while (!xrtAtomic32Load(&Work.Entered, XMEMORY_ACQUIRE)) testThreadYield();
	for (unsigned i = 0; i < 100; ++i)
		testRequire(!xrtOwnershipFreezeTryBegin(&Freeze) && empty(&Freeze) && xrtGetError() == NULL,
			"no inspection admission in the middle of a direct identity callback");
	xrtAtomic32Store(&Work.Continue, 1, XMEMORY_RELEASE); testThreadsJoin(&Thread, 1);
	testRequire(Thread.Result == 0 && xrtOwnershipFreezeTryBegin(&Freeze), "admit only after complete direct hash");
	testRequire(xrtOwnershipScopeEnd(&Freeze), "direct hash transition ended"); xrtValueRelease(Work.Value);
}

int main(void)
{
	testRequire(xrtMemDebugEnable(true), "enable memory accounting");
	admission(); no_allocation_and_inspection(); concurrent_refs(); colliding_admissions(); competing_collectors();
	for (unsigned round = 0; round < 50; ++round) { promotion(false); promotion(true); }
	for (unsigned round = 0; round < 5; ++round) for (unsigned kind = 0; kind < 16; ++kind) container_transition(kind);
	for (unsigned round = 0; round < 25; ++round) unfinished_transition();
	for (unsigned round = 0; round < 25; ++round) unfinished_hash();
	testRequire(xrtGetError() == NULL, "no residual diagnostic");
	testMemoryDebugDrain("scope tests balanced");
	puts("Ownership scopes: 100 native weak promotions, 80 COW/container transitions, 25 in-flight field finalizers, 25 in-flight identity hashes, four threads with 80000 retain/release pairs, at least 100 stable count freezes, 65 simultaneous native admissions, four competing collectors with at least 120 exclusive freezes, nested/busy/copy/wrong-thread/end refusals, actual inspection OOM, allocation-free scope admission; not whole-graph collection");
	return 0;
}
