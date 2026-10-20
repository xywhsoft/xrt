/* Offline physical pool leases and immutable bytes. No sockets or traffic. */
#ifdef NET_BUFFER_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#ifndef NET_BUFFER_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_net_buffer.h"
#endif
#define CHECK(Condition) testRequire((Condition), #Condition)

typedef struct Leases {
	xrtownershipref slots[16];
	size_t count;
} Leases;
static bool collect(xrtownershipref ref, ptr context)
{
	Leases* leases = context;
	CHECK(leases->count < 16); leases->slots[leases->count++] = ref; return true;
}
static size_t count(xrtownershipref ref)
{
	xrtownershipscope freeze = {0}; CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	size_t value = 0; CHECK(ref.Ops->Count(ref.Data, &value));
	CHECK(xrtOwnershipScopeEnd(&freeze)); return value;
}
static void balanced(const xmemdebugsnapshot* before)
{
	xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
	CHECK(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
	CHECK(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
		before->UseAfterFreeCount == after.UseAfterFreeCount && before->OverflowCount == after.OverflowCount &&
		before->UnderflowCount == after.UnderflowCount);
}
static void transfers(void)
{
	xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
	xnetbufpool* pool = xrtNetBufPoolCreate(NULL); CHECK(pool);
	xrtownershipref ref = xrtNetBufPoolOwnership(pool); CHECK(count(ref) == 1);
	xnetbuf source, target; CHECK(xrtNetBufInit(&source, pool) && xrtNetBufInit(&target, NULL));
	xnetbytes* bytes = xrtNetBytesCopy("a\0b", 3); CHECK(bytes);
	xrtownershipref owner = xrtNetBytesOwnership(bytes); CHECK(count(owner) == 1);
	CHECK(xrtNetBufAppend(&source, "abc", 3) && xrtNetBufPrepend(&source, "def", 3));
	CHECK(count(ref) == 3);
	xnetwspan span; CHECK(xrtNetBufReserve(&source, 8192, &span)); CHECK(count(ref) == 4);
	CHECK(xrtNetBufCancel(&source) && count(ref) == 3);
	CHECK(xrtNetBufAppendBytes(&source, bytes, 0, 3)); CHECK(count(ref) == 4 && count(owner) == 2);
	CHECK(!xrtNetBufPoolDestroy(pool)); xrtClearError();
	CHECK(xrtNetBufMove(&target, &source)); CHECK(!source.Head && count(ref) == 4 && count(owner) == 2);
	xrtownershipscope freeze = {0}; CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	Leases leases = {0}; leases.slots[leases.count++] = ref; leases.slots[leases.count++] = owner;
	CHECK(__xrtNetBufOwnershipWalk(&target, NULL, NULL, collect, &leases));
	CHECK(leases.count == 6); xrtownershipref anchors[2] = {ref,owner}; xrtownershipresult graph;
	CHECK(xrtOwnershipInspect(anchors, 2, leases.slots, leases.count, &graph));
	CHECK(graph.NodeCount == 2 && graph.EdgeCount == 6 && !graph.ExternalRootCount);
	CHECK(xrtOwnershipScopeEnd(&freeze));
	xrtNetBufClear(&target); CHECK(count(ref) == 1 && count(owner) == 1);
	CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	const xrtownershippreparationv1* preparation = NULL;
	const xrtownershipadapterv1* adapter = xrtNetBufPoolOwnershipAdapterV1(ref, &preparation);
	const xrtownershipadapterv1* byte_adapter = xrtNetBytesOwnershipAdapterV1(owner);
	CHECK(adapter && preparation && byte_adapter && adapter->Hold(pool) && byte_adapter->Hold(bytes));
	CHECK(adapter->Claim(pool, &source) && !adapter->Claim(pool, &target));
	adapter->Restore(pool, &source); CHECK(adapter->Claim(pool, &source));
	CHECK(xrtOwnershipScopeEnd(&freeze));
	CHECK(!xrtNetBufAppend(&source, "blocked", 7)); xrtClearError();
	CHECK(count(ref) == 2 && count(owner) == 2);
	CHECK(preparation->Prepare(pool, &source) == XRT_OWNERSHIP_PREPARE_READY);
	CHECK(xrtOwnershipFreezeTryBegin(&freeze)); CHECK(preparation->Ready(pool));
	adapter->Clear(pool, &source); CHECK(xrtOwnershipScopeEnd(&freeze));
	/* The checked creator is consumed independently of the actual plan pin. */
	CHECK(xrtNetBufPoolDestroy(pool));
	xrtNetBytesDestroy(bytes); CHECK(xrtNetBytesView(bytes).Size == 3);
	CHECK(memcmp(xrtNetBytesView(bytes).Data, "a\0b", 3) == 0);
	adapter->Drop(pool); byte_adapter->Drop(bytes); balanced(&before);
}
typedef struct ReleaseCase { xnetbufpool* pool; unsigned calls; } ReleaseCase;
static void release_probe(ptr context, cbytes data, size_t size)
{
	ReleaseCase* test = context; CHECK(data && size == 3); ++test->calls;
	CHECK(!xrtNetBufPoolDestroy(test->pool)); xrtClearError();
	xrtownershipscope freeze = {0}; CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	xrtownershipref ref = xrtNetBufPoolOwnership(test->pool); size_t unchanged = 123;
	CHECK(xrtNetBufPoolOwnershipQuiescenceV1Get()->Busy(test->pool));
	CHECK(!ref.Ops->Count(ref.Data, &unchanged) && unchanged == 123);
	CHECK(xrtOwnershipScopeEnd(&freeze));
}
static void callbacks(void)
{
	xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
	ReleaseCase test = {xrtNetBufPoolCreate(NULL),0}; CHECK(test.pool);
	xnetbuf buffer; CHECK(xrtNetBufInit(&buffer, test.pool));
	CHECK(xrtNetBufAppendRef(&buffer, "abc", 3, release_probe, &test));
	xrtownershipscope freeze = {0}; CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	CHECK(!__xrtNetBufOwnershipWalk(&buffer, NULL, NULL, NULL, NULL) && !test.calls);
	CHECK(xrtOwnershipScopeEnd(&freeze));
	xrtNetBufClear(&buffer); CHECK(test.calls == 1 && count(xrtNetBufPoolOwnership(test.pool)) == 1);
	CHECK(xrtNetBufPoolDestroy(test.pool)); balanced(&before);
}
static void failures(void)
{
	for (unsigned budget = 0; budget < 4; ++budget) {
		xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
		xnetbufpool* pool = xrtNetBufPoolCreate(NULL); CHECK(pool);
		xnetbytes* bytes = xrtNetBytesCopy("a\0b", 3); CHECK(bytes);
		xnetbuf buffer; CHECK(xrtNetBufInit(&buffer, pool));
		CHECK(xrtMemDebugFailAfter(budget));
		bool accepted = xrtNetBufAppendBytes(&buffer, bytes, 0, 3);
		bool failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		CHECK(accepted != failed); xrtClearError();
		CHECK(count(xrtNetBufPoolOwnership(pool)) == (accepted ? 2u : 1u));
		CHECK(count(xrtNetBytesOwnership(bytes)) == (accepted ? 2u : 1u));
		CHECK(!xrtNetBufAppendBytes(&buffer, bytes, 4, 0)); xrtClearError();
		CHECK(!xrtNetBufAppendBytes(&buffer, bytes, 1, SIZE_MAX)); xrtClearError();
		xrtNetBufClear(&buffer); CHECK(xrtNetBufPoolDestroy(pool)); xrtNetBytesDestroy(bytes); balanced(&before);
	}
}
int main(void)
{
	CHECK(xrtMemDebugEnable(true)); transfers(); callbacks(); failures();
	testMemoryDebugDrain("pool/bytes ownership balance");
	puts("[PASS] offline actual pool leases, immutable bytes, callback retirement and four OOM prefixes");
	return 0;
}
