#include "../test.h"



/* 可控分配器在指定阶段拒绝分配，其余请求转发给 C 堆。 */
typedef struct testchannelcancelallocator {
	bool Fail;
} testchannelcancelallocator;



/* 按当前故障开关分配内存。 */
static ptr testChannelCancelAlloc(ptr pContext, size_t iSize)
{
	testchannelcancelallocator* pState =
		(testchannelcancelallocator*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 按当前故障开关重分配内存。 */
static ptr testChannelCancelRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testchannelcancelallocator* pState =
		(testchannelcancelallocator*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 把释放请求转发给 C 堆。 */
static void testChannelCancelFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证取消监听分配失败不会挂接或移动 Channel 状态。 */
int main(void)
{
	testchannelcancelallocator tState = { false };
	xallocator tAllocator = {
		&tState,
		testChannelCancelAlloc,
		testChannelCancelRealloc,
		testChannelCancelFree
	};
	xchannel tChannel;
	xcancel* pCancel;
	ptr pItem = (ptr)(uintptr_t)77u;

	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install channel cancel allocator"
	);
	testRequire(xrtChannelInit(&tChannel, 1u), "cancel OOM channel init failed");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "cancel OOM token create failed");

	tState.Fail = true;
	testRequire(
		xrtChannelRecvUntilCancel(
			&tChannel,
			&pItem,
			XRT_DEADLINE_NEVER,
			pCancel
		) == XWAIT_ERROR,
		"channel cancel watch OOM result mismatch"
	);
	testRequire(pItem == NULL, "channel cancel watch OOM output mismatch");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"channel cancel watch OOM error mismatch"
	);
	testRequire(xrtChannelCount(&tChannel) == 0, "cancel watch OOM changed channel");
	testRequire(
		xrtChannelSendCancel(
			&tChannel,
			(ptr)(uintptr_t)11u,
			pCancel
		) == XWAIT_OK,
		"ready SendCancel allocated under OOM"
	);
	testRequire(
		(xrtChannelRecvCancel(&tChannel, &pItem, pCancel) == XWAIT_OK) &&
		((uintptr_t)pItem == 11u),
		"ready RecvCancel allocated under OOM"
	);
	testRequire(
		xrtChannelTrySend(&tChannel, (ptr)(uintptr_t)22u) == XCHANNEL_OK,
		"channel cancel send OOM prefill failed"
	);
	testRequire(
		xrtChannelSendCancel(
			&tChannel,
			(ptr)(uintptr_t)33u,
			pCancel
		) == XWAIT_ERROR,
		"blocked SendCancel succeeded under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"blocked SendCancel OOM error mismatch"
	);
	testRequire(
		(xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_OK) &&
		((uintptr_t)pItem == 22u),
		"blocked SendCancel OOM changed buffered item"
	);
	testRequire(
		xrtChannelRecvForCancel(&tChannel, &pItem, 0, pCancel) ==
		XWAIT_TIMEOUT,
		"expired RecvForCancel allocated under OOM"
	);
	testRequire(
		xrtChannelRecvForCancel(&tChannel, &pItem, 0, NULL) ==
		XWAIT_TIMEOUT,
		"null-token RecvForCancel allocated under OOM"
	);

	tState.Fail = false;
	xrtCancelDestroy(pCancel);
	testRequire(xrtChannelUnit(&tChannel), "cancel OOM channel unit failed");
	printf("[PASS] channel cancel OOM\n");
	return 0;
}
