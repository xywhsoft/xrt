#include "../../src/internal/xrt_channel.h"
#include "../test.h"



/* 失败分配器拒绝 Select 等待节点分配。 */
static ptr testChannelSelectOomAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* 失败分配器拒绝重分配。 */
static ptr testChannelSelectOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 失败分配器不应收到可释放内存。 */
static void testChannelSelectOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 验证立即成功零分配，并验证阻塞装配 OOM 不留下注册。 */
int main(void)
{
	xallocator tAllocator = {
		NULL,
		testChannelSelectOomAlloc,
		testChannelSelectOomRealloc,
		testChannelSelectOomFree
	};
	xchannel tReady;
	xchannel arrBlocked[9];
	xchannelcase arrCase[9];
	xchannelselectresult tResult;
	ptr arrItems[1];
	ptr arrOutput[9];
	ptr pItem = (ptr)(uintptr_t)1u;

	testRequire(
		xrtChannelInitBuffer(&tReady, arrItems, 1u),
		"select OOM ready init failed"
	);
	for ( size_t i = 0; i < 9u; i++ ) {
		arrOutput[i] = (ptr)(uintptr_t)(i + 1u);
		testRequire(
			xrtChannelInit(&arrBlocked[i], 0),
			"select OOM blocked init failed"
		);
		arrCase[i] = xrtChannelCaseRecv(
			&arrBlocked[i],
			&arrOutput[i]
		);
	}
	testRequire(
		xrtChannelTrySend(
			&tReady,
			(ptr)(uintptr_t)77u
		) == XCHANNEL_OK,
		"select OOM ready setup failed"
	);
	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install select OOM allocator"
	);

	arrCase[0] = xrtChannelCaseRecv(&tReady, &pItem);
	tResult = xrtChannelSelect(arrCase, 1u);
	testRequire(
		(tResult.Wait == XWAIT_OK) &&
		((uintptr_t)pItem == 77u),
		"ready Select allocated under OOM"
	);

	arrCase[0] = xrtChannelCaseRecv(
		&arrBlocked[0],
		&arrOutput[0]
	);
	tResult = xrtChannelSelect(arrCase, 9u);
	testRequire(
		(tResult.Wait == XWAIT_ERROR) &&
		(tResult.Index == XCHANNEL_SELECT_NONE),
		"blocked Select OOM result mismatch"
	);
	for ( size_t i = 0; i < 9u; i++ ) {
		testRequire(
			(uintptr_t)arrOutput[i] == (uintptr_t)(i + 1u),
			"Select OOM modified output"
		);
		testRequire(
			((xrt_channel_impl*)&arrBlocked[i])->SelectWaiters == NULL,
			"Select OOM leaked registration"
		);
	}
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Select OOM error kind mismatch"
	);

	testRequire(xrtChannelUnit(&tReady), "select OOM ready unit failed");
	for ( size_t i = 0; i < 9u; i++ ) {
		testRequire(
			xrtChannelUnit(&arrBlocked[i]),
			"select OOM blocked unit failed"
		);
	}
	printf("[PASS] channel select OOM\n");
	return 0;
}
