/* Offline storage ownership only: no handshake, records, keys or traffic. */
#ifdef TLS_SESSION_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#ifndef TLS_SESSION_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_tls_session.h"
#endif
#define CHECK(Condition) testRequire((Condition), #Condition)
typedef struct Edges {
	xrtownershipref Pool;
	xrtownershipref Bytes[4];
	size_t Pools, ByteOwners[4], StopAfter, Visits;
} Edges;
static bool visit(xrtownershipref ref, ptr raw)
{
	Edges* self = raw;
	if (ref.Ops == self->Pool.Ops && ref.Data == self->Pool.Data) ++self->Pools;
	else {
		size_t i = 0;
		while (i < 4 && (ref.Ops != self->Bytes[i].Ops || ref.Data != self->Bytes[i].Data)) ++i;
		CHECK(i < 4); ++self->ByteOwners[i];
	}
	++self->Visits;
	return !self->StopAfter || self->Visits < self->StopAfter;
}
static size_t count(xrtownershipref ref)
{
	size_t result = 0;
	CHECK(ref.Ops->Count(ref.Data, &result));
	return result;
}
static void release(ptr raw, cbytes data, size_t size)
{
	size_t* calls = raw;
	CHECK(data && size == 3);
	++*calls;
}
static void queues(xtlssession* session, xnetbuf** output)
{
	output[0] = &session->Feed; output[1] = &session->Send;
	output[2] = &session->Plain; output[3] = &session->Scratch;
}
static void actualEdges(void)
{
	xnetbufpool* pool = xrtNetBufPoolCreate(NULL);
	xtlscontext* context = xrtTlsContextCreate(NULL);
	xnetbytes* data[4];
	CHECK(pool && context);
	for (size_t i = 0; i < 4; ++i) {data[i] = xrtNetBytesCopy("a\0b", 3); CHECK(data[i]);}
	xtlssession* session = __xrtTlsSessionCreate(context, pool, XTLS_CLIENT);
	xrtTlsContextRelease(context); CHECK(session);
	xnetbuf* buffers[4]; queues(session, buffers);
	Edges edges = {0}; edges.Pool = xrtNetBufPoolOwnership(pool);
	for (size_t i = 0; i < 4; ++i) edges.Bytes[i] = xrtNetBytesOwnership(data[i]);
	xrtownershipscope freeze = {0}; CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	CHECK(xrtTlsSessionBufferOwnershipTrace(session, visit, &edges) && !edges.Visits);
	CHECK(count(edges.Pool) == 1);
	for (size_t i = 0; i < 4; ++i) CHECK(count(edges.Bytes[i]) == 1);
	CHECK(xrtOwnershipScopeEnd(&freeze));
	for (size_t i = 0; i < 4; ++i) {
		xnetwspan reserved;
		CHECK(xrtNetBufAppendBytes(buffers[i], data[i], 0, 3));
		CHECK(xrtNetBufReserve(buffers[i], 8192, &reserved));
	}
	/* An existing error must survive every trace, including failed enumeration. */
	CHECK(xrtTlsSessionRole(NULL) == 0); const xerror* prior = xrtGetError(); CHECK(prior);
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	CHECK(xrtMemDebugFailAfter(0));
#endif
	CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	size_t pools = count(edges.Pool); CHECK(pools == 9);
	for (size_t i = 0; i < 4; ++i) CHECK(count(edges.Bytes[i]) == 2);
	CHECK(xrtTlsSessionBufferOwnershipTrace(session, visit, &edges));
	CHECK(edges.Pools == 8 && edges.Visits == 12);
	CHECK(count(edges.Pool) == pools && xrtGetError() == prior);
	for (size_t i = 0; i < 4; ++i) CHECK(edges.ByteOwners[i] == 1 && count(edges.Bytes[i]) == 2);
	Edges stopped = edges; stopped.Pools = stopped.Visits = 0; stopped.StopAfter = 1;
	memset(stopped.ByteOwners, 0, sizeof(stopped.ByteOwners));
	CHECK(!xrtTlsSessionBufferOwnershipTrace(session, visit, &stopped) && stopped.Visits == 1);
	CHECK(!xrtTlsSessionBufferOwnershipTrace(NULL, visit, &edges));
	CHECK(!xrtTlsSessionBufferOwnershipTrace(session, NULL, &edges) && xrtGetError() == prior);
	CHECK(xrtOwnershipScopeEnd(&freeze));
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
#endif
	xrtClearError();
	/* Only actual block leases are visited; the queue's default pool is borrowed. */
	for (size_t i = 0; i < 4; ++i) {CHECK(xrtNetBufCancel(buffers[i])); xrtNetBufClear(buffers[i]);}
	CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	CHECK(count(edges.Pool) == 1);
	for (size_t i = 0; i < 4; ++i) CHECK(count(edges.Bytes[i]) == 1);
	CHECK(xrtOwnershipScopeEnd(&freeze));
	xrtTlsSessionDestroy(session); CHECK(xrtNetBufPoolDestroy(pool));
	for (size_t i = 0; i < 4; ++i) xrtNetBytesDestroy(data[i]);
}
static void heapEdges(void)
{
	xtlscontext* context = xrtTlsContextCreate(NULL); CHECK(context);
	xtlssession* session = __xrtTlsSessionCreate(context, NULL, XTLS_CLIENT);
	xrtTlsContextRelease(context); CHECK(session);
	char* taken = xrtMalloc(3); CHECK(taken); memcpy(taken, "a\0b", 3);
	CHECK(xrtTlsSessionFeed(session, "a\0b", 3) == XTLS_OK);
	CHECK(xrtTlsSessionFeedTake(session, taken, 3) == XTLS_OK);
	Edges edges = {0}; xrtownershipscope freeze = {0}; CHECK(xrtOwnershipFreezeTryBegin(&freeze));
	CHECK(xrtTlsSessionBufferOwnershipTrace(session, visit, &edges) && !edges.Visits);
	CHECK(xrtOwnershipScopeEnd(&freeze));
	CHECK(xrtTlsSessionFeedSize(session) == 6); xrtTlsSessionDestroy(session);
}
static void opaqueEdges(void)
{
	for (size_t i = 0; i < 4; ++i) {
		xtlscontext* context = xrtTlsContextCreate(NULL); CHECK(context);
		xtlssession* session = __xrtTlsSessionCreate(context, NULL, XTLS_SERVER);
		xrtTlsContextRelease(context); CHECK(session);
		xnetbuf* buffers[4]; queues(session, buffers); size_t calls = 0; Edges edges = {0};
		CHECK(xrtNetBufAppendRef(buffers[i], "abc", 3, release, &calls));
		xrtownershipscope freeze = {0}; CHECK(xrtOwnershipFreezeTryBegin(&freeze));
		CHECK(!xrtTlsSessionBufferOwnershipTrace(session, visit, &edges) && !calls && !edges.Visits);
		CHECK(xrtOwnershipScopeEnd(&freeze));
		xrtNetBufClear(buffers[i]); CHECK(calls == 1);
		CHECK(xrtNetBufAppendBorrow(buffers[i], "abc", 3));
		CHECK(xrtOwnershipFreezeTryBegin(&freeze));
		CHECK(!xrtTlsSessionBufferOwnershipTrace(session, visit, &edges) && !edges.Visits);
		CHECK(xrtOwnershipScopeEnd(&freeze));
		xrtNetBufClear(buffers[i]);
		CHECK(xrtOwnershipFreezeTryBegin(&freeze));
		CHECK(xrtTlsSessionBufferOwnershipTrace(session, visit, &edges) && !edges.Visits);
		CHECK(xrtOwnershipScopeEnd(&freeze));
		xrtTlsSessionDestroy(session); CHECK(calls == 1 && !xrtGetError());
	}
}
int main(void)
{
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	CHECK(xrtMemDebugEnable(true)); xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
#endif
	actualEdges(); heapEdges(); opaqueEdges();
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
		after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount &&
		before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
		before.UseAfterFreeCount == after.UseAfterFreeCount && before.OverflowCount == after.OverflowCount &&
		before.UnderflowCount == after.UnderflowCount);
	testMemoryDebugDrain("TLS queue ownership balance");
#endif
	puts("[PASS] TLS four queue ownership: 8 pool leases, 4 byte edges, reservations, stop, opaque refs, no allocation");
	return 0;
}
