#include "../test.h"



/* 可切换故障点的分配器用于验证失败原子性。 */
typedef struct test_net_alloc {
	size_t Calls;
	size_t FailAt;
} test_net_alloc;



/* 在指定调用点失败，其余请求使用 C 运行库。 */
static ptr testNetAlloc(ptr pContext, size_t iSize)
{
	test_net_alloc* pState = (test_net_alloc*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配遵循同一个故障计数。 */
static ptr testNetRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	test_net_alloc* pState = (test_net_alloc*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放成功分配的测试内存。 */
static void testNetFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* OOM 必须保持追加和 Pullup 的逻辑内容不变。 */
int main(void)
{
	test_net_alloc State = { 0, SIZE_MAX };
	xallocator Allocator;
	xnetbuf Buffer;
	xnetspan Span;
	char sOutput[16] = { 0 };
	char aLarge[4096];
	char aLeft[800];
	char aRight[800];

	Allocator.Context = &State;
	Allocator.Alloc = testNetAlloc;
	Allocator.Realloc = testNetRealloc;
	Allocator.Free = testNetFree;
	testRequire(xrtSetAllocator(&Allocator),
		"network OOM allocator install failed");
	testRequire(xrtNetBufInit(&Buffer, NULL),
		"network OOM buffer init failed");
	testRequire(xrtNetBufAppend(&Buffer, "base", 4),
		"network OOM baseline append failed");

	memset(aLarge, 'x', sizeof(aLarge));
	State.FailAt = State.Calls + 1;
	testRequire(!xrtNetBufAppend(&Buffer, aLarge, sizeof(aLarge)),
		"network buffer append unexpectedly survived OOM");
	testRequire((xrtNetBufSize(&Buffer) == 4) &&
		(xrtNetBufPeek(&Buffer, 0, sOutput, sizeof(sOutput)) == 4) &&
		(memcmp(sOutput, "base", 4) == 0),
		"failed network buffer append changed contents");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"network buffer append OOM error mismatch");

	State.FailAt = SIZE_MAX;
	xrtNetBufClear(&Buffer);
	memset(aLeft, 'L', sizeof(aLeft));
	memset(aRight, 'R', sizeof(aRight));
	testRequire(xrtNetBufAppendBorrow(&Buffer, aLeft, sizeof(aLeft)) &&
		xrtNetBufAppendBorrow(&Buffer, aRight, sizeof(aRight)),
		"network pullup OOM setup failed");
	State.FailAt = State.Calls + 1;
	testRequire(!xrtNetBufPullup(&Buffer, 1200, &Span),
		"network buffer pullup unexpectedly survived OOM");
	testRequire((xrtNetBufSize(&Buffer) == 1600) &&
		(xrtNetBufPeek(&Buffer, 796, sOutput, sizeof(sOutput)) == sizeof(sOutput)) &&
		(memcmp(sOutput, "LLLLRRRRRRRRRRRR", sizeof(sOutput)) == 0),
		"failed network buffer pullup changed contents");
	State.FailAt = SIZE_MAX;
	xrtNetBufClear(&Buffer);
	return 0;
}
