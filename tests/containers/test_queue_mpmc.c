#include "../test.h"



/* 排空上下文记录回调观察到的 FIFO 顺序。 */
typedef struct testmpmcdrainstate {
	ptr Items[8];
	size_t Count;
} testmpmcdrainstate;



/* 保存一个已经从 MPMC 队列移除的指针值。 */
static void testMPMCQueueDrain(ptr pItem, ptr pContext)
{
	testmpmcdrainstate* pState = (testmpmcdrainstate*)pContext;

	if ( pState->Count < 8u ) {
		pState->Items[pState->Count++] = pItem;
	}
}



/* 验证拥有型 MPMC 的容量、空值、批量、关闭和重置合同。 */
static void testMPMCBasic(void)
{
	xmpmcqueue tQueue;
	xmpmcqueue* pCreated;
	xqueuebatchresult Batch;
	testmpmcdrainstate tDrain = { { NULL }, 0 };
	int pValues[] = { 10, 20, 30, 40, 50, 60 };
	ptr pInput[] = { &pValues[2], &pValues[3], &pValues[4] };
	ptr pOutput[] = {
		(ptr)(uintptr_t)1u,
		(ptr)(uintptr_t)2u,
		(ptr)(uintptr_t)3u,
		(ptr)(uintptr_t)4u
	};
	ptr pItem = (ptr)(uintptr_t)1u;

	testRequire(sizeof(xqueuecursor32) == XRT_QUEUE_CACHE_SPAN, "MPMC cursor cache span mismatch");
	testRequire(xrtMPMCQueueInit(&tQueue, 1u), "minimum MPMC init failed");
	testRequire(tQueue.Capacity == 2u, "minimum MPMC capacity mismatch");
	testRequire(xrtMPMCQueueTryPush(&tQueue, &pValues[0]) == XQUEUE_OK, "minimum MPMC first push failed");
	testRequire(xrtMPMCQueueTryPush(&tQueue, &pValues[1]) == XQUEUE_OK, "minimum MPMC second push failed");
	testRequire(xrtMPMCQueueTryPush(&tQueue, &pValues[2]) == XQUEUE_FULL, "minimum MPMC full result mismatch");
	testRequire(xrtMPMCQueueTryPop(&tQueue, &pItem) == XQUEUE_OK, "minimum MPMC first pop failed");
	testRequire(pItem == &pValues[0], "minimum MPMC FIFO mismatch");
	xrtMPMCQueueUnit(&tQueue);

	testRequire(xrtMPMCQueueInit(&tQueue, 3u), "MPMC init failed");
	testRequire(
		(tQueue.Capacity == 4u) &&
		(tQueue.Mask == 3u) &&
		(tQueue.Allocation == tQueue.Slots),
		"MPMC initial state mismatch"
	);
	testRequire(!xrtMPMCQueueIsClosed(&tQueue), "MPMC should start open");
	testRequire(!xrtMPMCQueueIsDrained(&tQueue), "open empty MPMC should not be drained");
	testRequire(xrtMPMCQueueTryPop(&tQueue, &pItem) == XQUEUE_EMPTY, "empty MPMC result mismatch");
	testRequire(pItem == NULL, "empty MPMC did not clear output");

	testRequire(xrtMPMCQueueTryPush(&tQueue, &pValues[0]) == XQUEUE_OK, "MPMC first push failed");
	testRequire(xrtMPMCQueueTryPush(&tQueue, NULL) == XQUEUE_OK, "MPMC NULL push failed");
	Batch = xrtMPMCQueuePushBatch(&tQueue, pInput, 3u);
	testRequire((Batch.Result == XQUEUE_OK) && (Batch.Count == 2u), "MPMC partial push batch mismatch");
	testRequire(xrtMPMCQueueCount(&tQueue) == 4u, "MPMC full count mismatch");
	testRequire(xrtMPMCQueueTryPush(&tQueue, &pValues[5]) == XQUEUE_FULL, "full MPMC result mismatch");
	Batch = xrtMPMCQueuePushBatch(&tQueue, pInput, 1u);
	testRequire((Batch.Result == XQUEUE_FULL) && (Batch.Count == 0u), "full MPMC batch result mismatch");

	Batch = xrtMPMCQueuePopBatch(&tQueue, pOutput, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 3u) &&
		(pOutput[0] == &pValues[0]) &&
		(pOutput[1] == NULL) &&
		(pOutput[2] == &pValues[2]) &&
		(pOutput[3] == (ptr)(uintptr_t)4u),
		"MPMC pop batch FIFO mismatch"
	);
	testRequire(xrtMPMCQueueCount(&tQueue) == 1u, "MPMC count after batch mismatch");
	xrtClearError();
	testRequire(!xrtMPMCQueueReset(&tQueue), "busy MPMC reset should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN, "busy MPMC reset error mismatch");

	/* 关闭发生在全部生产者停止之后，消费者仍可并发排空。 */
	xrtMPMCQueueClose(&tQueue);
	testRequire(xrtMPMCQueueIsClosed(&tQueue), "MPMC close state mismatch");
	testRequire(xrtMPMCQueueTryPush(&tQueue, &pValues[5]) == XQUEUE_CLOSED, "closed MPMC push mismatch");
	Batch = xrtMPMCQueuePushBatch(&tQueue, pInput, 1u);
	testRequire((Batch.Result == XQUEUE_CLOSED) && (Batch.Count == 0u), "closed MPMC batch push mismatch");
	testRequire(xrtMPMCQueueDrain(&tQueue, testMPMCQueueDrain, &tDrain) == 1u, "MPMC drain count mismatch");
	testRequire(
		(tDrain.Count == 1u) && (tDrain.Items[0] == &pValues[3]),
		"MPMC drain order mismatch"
	);
	testRequire(xrtMPMCQueueIsDrained(&tQueue), "closed empty MPMC should be drained");
	testRequire(xrtMPMCQueueTryPop(&tQueue, &pItem) == XQUEUE_CLOSED, "drained MPMC pop mismatch");
	testRequire(xrtMPMCQueueReset(&tQueue), "drained MPMC reset failed");
	testRequire(!xrtMPMCQueueIsClosed(&tQueue), "MPMC reset did not reopen queue");

	testRequire(xrtMPMCQueueTryPush(&tQueue, &pValues[4]) == XQUEUE_OK, "MPMC discard setup failed");
	testRequire(xrtMPMCQueueDrain(&tQueue, NULL, NULL) == 1u, "MPMC discard drain mismatch");
	xrtMPMCQueueUnit(&tQueue);

	pCreated = xrtMPMCQueueCreate(8u);
	testRequire(pCreated != NULL, "MPMC create failed");
	xrtMPMCQueueDestroy(pCreated);
}



/* 验证外部槽环、批量别名、对齐、重叠和结构损坏检查。 */
static void testMPMCBufferAndAlias(void)
{
	xmpmcqueue tQueue;
	xqueueslot pStorage[4];
	unsigned char arrUnaligned[(sizeof(xqueueslot) * 5u) + sizeof(ptr)];
	xqueueslot* pUnaligned;
	ptr* pUnalignedItems;
	ptr pValue = (ptr)(uintptr_t)5u;
	ptr pOutput = NULL;
	xqueueslot* pSavedSlots;
	size_t iSavedMask;
	xqueuebatchresult Batch;

	pUnaligned = (xqueueslot*)(void*)(
		(((uintptr_t)arrUnaligned + sizeof(ptr) - 1u) &
			~((uintptr_t)sizeof(ptr) - 1u)) + 1u
	);
	pUnalignedItems = (ptr*)(void*)pUnaligned;
	memset(pStorage, 0xaa, sizeof(pStorage));
	testRequire(xrtMPMCQueueInitBuffer(&tQueue, pStorage, 4u), "external MPMC init failed");
	testRequire(
		(tQueue.Allocation == NULL) &&
		(pStorage[0].Sequence.Value == 0u) &&
		(pStorage[3].Sequence.Value == 3u) &&
		(pStorage[0].Item == NULL),
		"external MPMC buffer state mismatch"
	);

	xrtClearError();
	Batch = xrtMPMCQueuePushBatch(
		&tQueue,
		(ptr const*)(const void*)&tQueue.Slots,
		1u
	);
	testRequire(
		Batch.Result == XQUEUE_ERROR,
		"MPMC metadata push batch should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPMC metadata push alias error mismatch"
	);
	xrtClearError();
	Batch = xrtMPMCQueuePushBatch(&tQueue, &pStorage[0].Item, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "MPMC aliased push batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPMC push alias error mismatch");
	testRequire(xrtMPMCQueueTryPush(&tQueue, pValue) == XQUEUE_OK, "external MPMC push failed");

	xrtClearError();
	testRequire(
		xrtMPMCQueueTryPop(&tQueue, &pStorage[0].Item) == XQUEUE_ERROR,
		"MPMC aliased single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPMC single pop alias error mismatch"
	);
	testRequire(
		(xrtMPMCQueueCount(&tQueue) == 1u) &&
		(pStorage[0].Item == pValue),
		"MPMC single pop alias changed queue"
	);

	xrtClearError();
	testRequire(
		xrtMPMCQueueTryPop(
			&tQueue,
			(ptr*)(void*)&tQueue.Slots
		) == XQUEUE_ERROR,
		"MPMC metadata single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPMC metadata single pop error mismatch"
	);
	testRequire(
		(tQueue.Slots == pStorage) && (xrtMPMCQueueCount(&tQueue) == 1u),
		"MPMC metadata single pop changed queue"
	);

	xrtClearError();
	testRequire(
		xrtMPMCQueueTryPop(&tQueue, pUnalignedItems) == XQUEUE_ERROR,
		"MPMC unaligned single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPMC unaligned single pop error mismatch"
	);

	xrtClearError();
	Batch = xrtMPMCQueuePopBatch(&tQueue, &pStorage[0].Item, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "MPMC aliased pop batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPMC pop alias error mismatch");
	xrtClearError();
	Batch = xrtMPMCQueuePopBatch(
		&tQueue,
		(ptr*)(void*)&tQueue.Slots,
		1u
	);
	testRequire(
		Batch.Result == XQUEUE_ERROR,
		"MPMC metadata pop batch should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPMC metadata pop alias error mismatch"
	);
	testRequire(xrtMPMCQueueTryPop(&tQueue, &pOutput) == XQUEUE_OK, "external MPMC recovery pop failed");
	testRequire(pOutput == pValue, "external MPMC recovery value mismatch");

	pSavedSlots = tQueue.Slots;
	iSavedMask = tQueue.Mask;
	tQueue.Slots = (xqueueslot*)(void*)&tQueue;
	xrtClearError();
	testRequire(
		xrtMPMCQueueTryPush(&tQueue, pValue) == XQUEUE_ERROR,
		"overlapping damaged MPMC slots should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"overlapping damaged MPMC slots error mismatch"
	);
	tQueue.Slots = pSavedSlots;
	tQueue.Mask = 0u;
	xrtClearError();
	testRequire(xrtMPMCQueueCount(&tQueue) == 0u, "corrupt MPMC count should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "corrupt MPMC error mismatch");
	tQueue.Mask = iSavedMask;
	pStorage[0].Item = pValue;
	xrtMPMCQueueUnit(&tQueue);
	testRequire(pStorage[0].Item == pValue, "MPMC unit changed external slots unexpectedly");

	xrtClearError();
	testRequire(!xrtMPMCQueueInitBuffer(&tQueue, pStorage, 1u), "one-slot external MPMC should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external MPMC minimum capacity error mismatch");
	xrtClearError();
	testRequire(!xrtMPMCQueueInitBuffer(&tQueue, pStorage, 3u), "non-power-of-two external MPMC should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external MPMC capacity error mismatch");

	xrtClearError();
	testRequire(!xrtMPMCQueueInitBuffer(&tQueue, pUnaligned, 4u), "unaligned external MPMC should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external MPMC alignment error mismatch");

	xrtClearError();
	testRequire(
		!xrtMPMCQueueInitBuffer(&tQueue, (xqueueslot*)(void*)&tQueue, 2u),
		"MPMC structure and buffer overlap should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external MPMC overlap error mismatch");
}



/* 验证初始化失败零状态、空批量和参数错误合同。 */
static void testMPMCInvalid(void)
{
	xmpmcqueue tQueue;
	xqueueslot pStorage[4];
	xqueuebatchresult Batch;

	memset(&tQueue, 0xaa, sizeof(tQueue));
	xrtClearError();
	testRequire(!xrtMPMCQueueInit(&tQueue, 0u), "zero MPMC init should fail");
	testRequire(tQueue.Slots == NULL, "failed MPMC init did not leave zero state");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "zero MPMC init error mismatch");

	testRequire(xrtMPMCQueueInitBuffer(&tQueue, pStorage, 4u), "MPMC no-op batch setup failed");
	Batch = xrtMPMCQueuePushBatch(&tQueue, NULL, 0u);
	testRequire((Batch.Result == XQUEUE_OK) && (Batch.Count == 0u), "MPMC zero push batch mismatch");
	Batch = xrtMPMCQueuePopBatch(&tQueue, NULL, 0u);
	testRequire((Batch.Result == XQUEUE_OK) && (Batch.Count == 0u), "MPMC zero pop batch mismatch");

	xrtClearError();
	Batch = xrtMPMCQueuePushBatch(&tQueue, NULL, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "MPMC NULL push batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPMC NULL push error mismatch");
	xrtClearError();
	Batch = xrtMPMCQueuePopBatch(&tQueue, NULL, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "MPMC NULL pop batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPMC NULL pop error mismatch");
	xrtClearError();
	testRequire(xrtMPMCQueueTryPop(&tQueue, NULL) == XQUEUE_ERROR, "MPMC NULL pop output should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPMC NULL pop output error mismatch");
	xrtMPMCQueueUnit(&tQueue);
}



/* 验证序列槽和 32 位游标跨越最大值后仍保持 FIFO 与满空判断。 */
static void testMPMCWrap(void)
{
	xmpmcqueue tQueue;
	xqueueslot pStorage[4];
	ptr pInput[6] = {
		(ptr)(uintptr_t)1u,
		(ptr)(uintptr_t)2u,
		(ptr)(uintptr_t)3u,
		(ptr)(uintptr_t)4u,
		(ptr)(uintptr_t)5u,
		(ptr)(uintptr_t)6u
	};
	ptr pOutput[4];
	ptr pItem;
	xqueuebatchresult Batch;
	uint32 iBase = UINT32_MAX - 2u;

	testRequire(xrtMPMCQueueInitBuffer(&tQueue, pStorage, 4u), "wrap MPMC init failed");
	for ( uint32 i = 0u; i < 4u; i++ ) {
		uint32 iPosition = iBase + i;
		xqueueslot* pSlot = &pStorage[iPosition & 3u];

		pSlot->Sequence.Value = iPosition;
		pSlot->Item = NULL;
	}
	tQueue.Head.Position.Value = iBase;
	tQueue.Tail.Position.Value = iBase;

	/* 验证批量预留和并发槽代次在 32 位回绕处保持一致。 */
	Batch = xrtMPMCQueuePushBatch(&tQueue, pInput, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u),
		"wrap MPMC first push batch mismatch"
	);
	Batch = xrtMPMCQueuePopBatch(&tQueue, pOutput, 2u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 2u) &&
		(pOutput[0] == pInput[0]) &&
		(pOutput[1] == pInput[1]),
		"wrap MPMC first pop batch mismatch"
	);
	Batch = xrtMPMCQueuePushBatch(&tQueue, pInput + 3u, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u),
		"wrap MPMC second push batch mismatch"
	);
	Batch = xrtMPMCQueuePopBatch(&tQueue, pOutput, 4u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 4u) &&
		(pOutput[0] == pInput[2]) &&
		(pOutput[1] == pInput[3]) &&
		(pOutput[2] == pInput[4]) &&
		(pOutput[3] == pInput[5]),
		"wrap MPMC second pop batch mismatch"
	);

	/* 再以单元素路径覆盖多个完整环周期。 */
	for ( uintptr_t i = 7u; i <= 22u; i++ ) {
		testRequire(xrtMPMCQueueTryPush(&tQueue, (ptr)i) == XQUEUE_OK, "wrap MPMC push failed");
		testRequire(xrtMPMCQueueTryPop(&tQueue, &pItem) == XQUEUE_OK, "wrap MPMC pop failed");
		testRequire((uintptr_t)pItem == i, "wrap MPMC FIFO mismatch");
	}
	testRequire(xrtMPMCQueueCount(&tQueue) == 0u, "wrap MPMC final count mismatch");
	xrtMPMCQueueUnit(&tQueue);
}



/* 运行 MPMC 单线程合同、外部槽环和游标回绕测试。 */
int main(void)
{
	testMPMCBasic();
	testMPMCBufferAndAlias();
	testMPMCInvalid();
	testMPMCWrap();
	printf("[PASS] queue_mpmc\n");
	return 0;
}
