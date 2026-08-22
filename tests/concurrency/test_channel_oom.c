#include "../test.h"



/* 失败分配器拒绝 Channel 拥有式存储。 */
static ptr testChannelOomAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* 失败分配器拒绝重分配。 */
static ptr testChannelOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 失败分配器不应收到可释放内存。 */
static void testChannelOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 验证拥有式失败和零分配初始化路径。 */
int main(void)
{
	xallocator tAllocator = {
		NULL,
		testChannelOomAlloc,
		testChannelOomRealloc,
		testChannelOomFree
	};
	xchannel tChannel;
	ptr arrItems[2];

	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install channel OOM allocator"
	);
	testRequire(
		!xrtChannelInit(&tChannel, 1u),
		"buffered channel init succeeded under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"buffered channel OOM error mismatch"
	);
	testRequire(
		xrtChannelCreate(1u) == NULL,
		"channel create succeeded under OOM"
	);

	/* rendezvous 与外部缓冲初始化不依赖 XRT 堆。 */
	testRequire(
		xrtChannelInit(&tChannel, 0),
		"rendezvous channel allocated under OOM"
	);
	testRequire(xrtChannelUnit(&tChannel), "rendezvous channel unit failed");
	testRequire(
		xrtChannelInitBuffer(&tChannel, arrItems, 2u),
		"external channel allocated under OOM"
	);
	testRequire(xrtChannelUnit(&tChannel), "external channel unit failed");

	printf("[PASS] channel OOM\n");
	return 0;
}
