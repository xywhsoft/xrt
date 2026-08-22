#include "../../src/internal/xrt_channel.h"
#include "../test.h"



/* 可控分配器允许等待节点，随后拒绝取消监听。 */
typedef struct testchannelselectcancelallocator {
	size_t Remaining;
} testchannelselectcancelallocator;



/* 在剩余成功次数耗尽后拒绝分配。 */
static ptr testChannelSelectCancelAlloc(
	ptr pContext,
	size_t iSize
)
{
	testchannelselectcancelallocator* pState =
		(testchannelselectcancelallocator*)pContext;

	if ( pState->Remaining == 0 ) {
		return NULL;
	}
	pState->Remaining--;
	return malloc(iSize);
}



/* 在剩余成功次数耗尽后拒绝重分配。 */
static ptr testChannelSelectCancelRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testchannelselectcancelallocator* pState =
		(testchannelselectcancelallocator*)pContext;

	if ( pState->Remaining == 0 ) {
		return NULL;
	}
	pState->Remaining--;
	return realloc(pMemory, iSize);
}



/* 把释放请求转发给 C 堆。 */
static void testChannelSelectCancelFree(
	ptr pContext,
	ptr pMemory
)
{
	(void)pContext;
	free(pMemory);
}



/* 验证取消监听 OOM 不留下 Channel 注册。 */
int main(void)
{
	testchannelselectcancelallocator tState = { SIZE_MAX };
	xallocator tAllocator = {
		&tState,
		testChannelSelectCancelAlloc,
		testChannelSelectCancelRealloc,
		testChannelSelectCancelFree
	};
	xchannel tChannel;
	xcancel* pCancel;
	xchannelcase tCase;
	xchannelselectresult tResult;
	ptr pItem = (ptr)(uintptr_t)1u;

	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install select cancel OOM allocator"
	);
	testRequire(xrtChannelInit(&tChannel, 0), "select cancel OOM init failed");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "select cancel OOM token create failed");

	tState.Remaining = 0;
	tCase = xrtChannelCaseRecv(&tChannel, &pItem);
	tResult = xrtChannelSelectUntilCancel(
		&tCase,
		1u,
		XRT_DEADLINE_NEVER,
		pCancel
	);
	testRequire(
		(tResult.Wait == XWAIT_ERROR) &&
		(tResult.Index == XCHANNEL_SELECT_NONE),
		"select cancel watch OOM result mismatch"
	);
	testRequire((uintptr_t)pItem == 1u, "select cancel OOM modified output");
	testRequire(
		((xrt_channel_impl*)&tChannel)->SelectWaiters == NULL,
		"select cancel OOM leaked registration"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"select cancel OOM error kind mismatch"
	);

	tState.Remaining = SIZE_MAX;
	xrtCancelDestroy(pCancel);
	testRequire(xrtChannelUnit(&tChannel), "select cancel OOM unit failed");
	printf("[PASS] channel select cancel OOM\n");
	return 0;
}
