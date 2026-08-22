#include "../test.h"



/* 排空上下文记录回调观察到的 FIFO 顺序。 */
typedef struct testmpscdrainstate {
	ptr Items[8];
	size_t Count;
} testmpscdrainstate;



/* 保存一个已经从 MPSC 队列移除的指针值。 */
static void testMPSCQueueDrain(ptr pItem, ptr pContext)
{
	testmpscdrainstate* pState = (testmpscdrainstate*)pContext;

	if ( pState->Count < 8u ) {
		pState->Items[pState->Count++] = pItem;
	}
}



/* 验证拥有型 MPSC 的容量、空值、批量、关闭和重置合同。 */
static void testMPSCBasic(void)
{
	xmpscqueue tQueue;
	xmpscqueue* pCreated;
	xqueuebatchresult Batch;
	testmpscdrainstate tDrain = { { NULL }, 0 };
	int pValues[] = { 10, 20, 30, 40, 50, 60 };
	ptr pInput[] = { &pValues[2], &pValues[3], &pValues[4] };
	ptr pOutput[] = {
		(ptr)(uintptr_t)1u,
		(ptr)(uintptr_t)2u,
		(ptr)(uintptr_t)3u,
		(ptr)(uintptr_t)4u
	};
	ptr pItem = (ptr)(uintptr_t)1u;

	testRequire(sizeof(xqueuecursor32) == XRT_QUEUE_CACHE_SPAN, "MPSC cursor cache span mismatch");
	testRequire(xrtMPSCQueueInit(&tQueue, 1u), "minimum MPSC init failed");
	testRequire(tQueue.Capacity == 2u, "minimum MPSC capacity mismatch");
	testRequire(xrtMPSCQueueTryPush(&tQueue, &pValues[0]) == XQUEUE_OK, "minimum MPSC first push failed");
	testRequire(xrtMPSCQueueTryPush(&tQueue, &pValues[1]) == XQUEUE_OK, "minimum MPSC second push failed");
	testRequire(xrtMPSCQueueTryPush(&tQueue, &pValues[2]) == XQUEUE_FULL, "minimum MPSC full result mismatch");
	testRequire(xrtMPSCQueueTryPop(&tQueue, &pItem) == XQUEUE_OK, "minimum MPSC first pop failed");
	testRequire(pItem == &pValues[0], "minimum MPSC FIFO mismatch");
	xrtMPSCQueueUnit(&tQueue);

	testRequire(xrtMPSCQueueInit(&tQueue, 3u), "MPSC init failed");
	testRequire(
		(tQueue.Capacity == 4u) &&
		(tQueue.Mask == 3u) &&
		(tQueue.Allocation == tQueue.Slots),
		"MPSC initial state mismatch"
	);
	testRequire(!xrtMPSCQueueIsClosed(&tQueue), "MPSC should start open");
	testRequire(!xrtMPSCQueueIsDrained(&tQueue), "open empty MPSC should not be drained");
	testRequire(xrtMPSCQueueTryPop(&tQueue, &pItem) == XQUEUE_EMPTY, "empty MPSC result mismatch");
	testRequire(pItem == NULL, "empty MPSC did not clear output");

	testRequire(xrtMPSCQueueTryPush(&tQueue, &pValues[0]) == XQUEUE_OK, "MPSC first push failed");
	testRequire(xrtMPSCQueueTryPush(&tQueue, NULL) == XQUEUE_OK, "MPSC NULL push failed");
	Batch = xrtMPSCQueuePushBatch(&tQueue, pInput, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 2u),
		"MPSC partial push batch mismatch"
	);
	testRequire(xrtMPSCQueueCount(&tQueue) == 4u, "MPSC full count mismatch");
	testRequire(xrtMPSCQueueTryPush(&tQueue, &pValues[5]) == XQUEUE_FULL, "full MPSC result mismatch");
	Batch = xrtMPSCQueuePushBatch(&tQueue, pInput, 1u);
	testRequire(
		(Batch.Result == XQUEUE_FULL) && (Batch.Count == 0u),
		"full MPSC batch result mismatch"
	);

	Batch = xrtMPSCQueuePopBatch(&tQueue, pOutput, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 3u) &&
		(pOutput[0] == &pValues[0]) &&
		(pOutput[1] == NULL) &&
		(pOutput[2] == &pValues[2]) &&
		(pOutput[3] == (ptr)(uintptr_t)4u),
		"MPSC pop batch FIFO mismatch"
	);
	testRequire(xrtMPSCQueueCount(&tQueue) == 1u, "MPSC count after batch mismatch");
	xrtClearError();
	testRequire(!xrtMPSCQueueReset(&tQueue), "busy MPSC reset should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN, "busy MPSC reset error mismatch");

	/* MPSC 只允许在所有生产者停止并返回后关闭。 */
	xrtMPSCQueueClose(&tQueue);
	testRequire(xrtMPSCQueueIsClosed(&tQueue), "MPSC close state mismatch");
	testRequire(xrtMPSCQueueTryPush(&tQueue, &pValues[5]) == XQUEUE_CLOSED, "closed MPSC push mismatch");
	Batch = xrtMPSCQueuePushBatch(&tQueue, pInput, 1u);
	testRequire(
		(Batch.Result == XQUEUE_CLOSED) && (Batch.Count == 0u),
		"closed MPSC batch push mismatch"
	);
	testRequire(xrtMPSCQueueDrain(&tQueue, testMPSCQueueDrain, &tDrain) == 1u, "MPSC drain count mismatch");
	testRequire(
		(tDrain.Count == 1u) && (tDrain.Items[0] == &pValues[3]),
		"MPSC drain order mismatch"
	);
	testRequire(xrtMPSCQueueIsDrained(&tQueue), "closed empty MPSC should be drained");
	testRequire(xrtMPSCQueueTryPop(&tQueue, &pItem) == XQUEUE_CLOSED, "drained MPSC pop mismatch");
	testRequire(xrtMPSCQueueReset(&tQueue), "drained MPSC reset failed");
	testRequire(!xrtMPSCQueueIsClosed(&tQueue), "MPSC reset did not reopen queue");

	testRequire(xrtMPSCQueueTryPush(&tQueue, &pValues[4]) == XQUEUE_OK, "MPSC discard setup failed");
	testRequire(xrtMPSCQueueDrain(&tQueue, NULL, NULL) == 1u, "MPSC discard drain mismatch");
	xrtMPSCQueueUnit(&tQueue);

	pCreated = xrtMPSCQueueCreate(8u);
	testRequire(pCreated != NULL, "MPSC create failed");
	xrtMPSCQueueDestroy(pCreated);
}



/* 验证外部槽环、批量别名、对齐、重叠和结构损坏检查。 */
static void testMPSCBufferAndAlias(void)
{
	xmpscqueue tQueue;
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
	testRequire(
		xrtMPSCQueueInitBuffer(&tQueue, pStorage, 4u),
		"external MPSC init failed"
	);
	testRequire(
		(tQueue.Allocation == NULL) &&
		(pStorage[0].Sequence.Value == 0u) &&
		(pStorage[3].Sequence.Value == 3u) &&
		(pStorage[0].Item == NULL),
		"external MPSC buffer state mismatch"
	);

	xrtClearError();
	Batch = xrtMPSCQueuePushBatch(
		&tQueue,
		(ptr const*)(const void*)&tQueue.Slots,
		1u
	);
	testRequire(
		Batch.Result == XQUEUE_ERROR,
		"MPSC metadata push batch should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPSC metadata push alias error mismatch"
	);
	xrtClearError();
	Batch = xrtMPSCQueuePushBatch(&tQueue, &pStorage[0].Item, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "MPSC aliased push batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPSC push alias error mismatch");
	testRequire(xrtMPSCQueueTryPush(&tQueue, pValue) == XQUEUE_OK, "external MPSC push failed");

	xrtClearError();
	testRequire(
		xrtMPSCQueueTryPop(&tQueue, &pStorage[0].Item) == XQUEUE_ERROR,
		"MPSC aliased single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPSC single pop alias error mismatch"
	);
	testRequire(
		(xrtMPSCQueueCount(&tQueue) == 1u) &&
		(pStorage[0].Item == pValue),
		"MPSC single pop alias changed queue"
	);

	xrtClearError();
	testRequire(
		xrtMPSCQueueTryPop(
			&tQueue,
			(ptr*)(void*)&tQueue.Slots
		) == XQUEUE_ERROR,
		"MPSC metadata single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPSC metadata single pop error mismatch"
	);
	testRequire(
		(tQueue.Slots == pStorage) && (xrtMPSCQueueCount(&tQueue) == 1u),
		"MPSC metadata single pop changed queue"
	);

	xrtClearError();
	testRequire(
		xrtMPSCQueueTryPop(&tQueue, pUnalignedItems) == XQUEUE_ERROR,
		"MPSC unaligned single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPSC unaligned single pop error mismatch"
	);

	xrtClearError();
	Batch = xrtMPSCQueuePopBatch(&tQueue, &pStorage[0].Item, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "MPSC aliased pop batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPSC pop alias error mismatch");
	xrtClearError();
	Batch = xrtMPSCQueuePopBatch(
		&tQueue,
		(ptr*)(void*)&tQueue.Slots,
		1u
	);
	testRequire(
		Batch.Result == XQUEUE_ERROR,
		"MPSC metadata pop batch should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"MPSC metadata pop alias error mismatch"
	);
	testRequire(xrtMPSCQueueTryPop(&tQueue, &pOutput) == XQUEUE_OK, "external MPSC recovery pop failed");
	testRequire(pOutput == pValue, "external MPSC recovery value mismatch");

	/* 静态摘要损坏必须报告状态错误。 */
	pSavedSlots = tQueue.Slots;
	iSavedMask = tQueue.Mask;
	tQueue.Slots = (xqueueslot*)(void*)&tQueue;
	xrtClearError();
	testRequire(
		xrtMPSCQueueTryPush(&tQueue, pValue) == XQUEUE_ERROR,
		"overlapping damaged MPSC slots should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"overlapping damaged MPSC slots error mismatch"
	);
	tQueue.Slots = pSavedSlots;
	tQueue.Mask = 0u;
	xrtClearError();
	testRequire(xrtMPSCQueueCount(&tQueue) == 0u, "corrupt MPSC count should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "corrupt MPSC error mismatch");
	tQueue.Mask = iSavedMask;
	pStorage[0].Item = pValue;
	xrtMPSCQueueUnit(&tQueue);
	testRequire(pStorage[0].Item == pValue, "MPSC unit changed external slots unexpectedly");

	xrtClearError();
	testRequire(
		!xrtMPSCQueueInitBuffer(&tQueue, pStorage, 1u),
		"one-slot external MPSC should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external MPSC minimum capacity error mismatch");
	xrtClearError();
	testRequire(
		!xrtMPSCQueueInitBuffer(&tQueue, pStorage, 3u),
		"non-power-of-two external MPSC should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external MPSC capacity error mismatch");

	/* 在一个自然对齐地址后偏移一字节，构造确定的不对齐槽环。 */
	xrtClearError();
	testRequire(
		!xrtMPSCQueueInitBuffer(&tQueue, pUnaligned, 4u),
		"unaligned external MPSC should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external MPSC alignment error mismatch");

	xrtClearError();
	testRequire(
		!xrtMPSCQueueInitBuffer(
			&tQueue,
			(xqueueslot*)(void*)&tQueue,
			2u
		),
		"MPSC structure and buffer overlap should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external MPSC overlap error mismatch");
}



/* 验证初始化失败零状态、空批量和参数错误合同。 */
static void testMPSCInvalid(void)
{
	xmpscqueue tQueue;
	xqueueslot pStorage[4];
	xqueuebatchresult Batch;

	memset(&tQueue, 0xaa, sizeof(tQueue));
	xrtClearError();
	testRequire(!xrtMPSCQueueInit(&tQueue, 0u), "zero MPSC init should fail");
	testRequire(tQueue.Slots == NULL, "failed MPSC init did not leave zero state");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "zero MPSC init error mismatch");

	testRequire(xrtMPSCQueueInitBuffer(&tQueue, pStorage, 4u), "MPSC no-op batch setup failed");
	Batch = xrtMPSCQueuePushBatch(&tQueue, NULL, 0u);
	testRequire((Batch.Result == XQUEUE_OK) && (Batch.Count == 0u), "MPSC zero push batch mismatch");
	Batch = xrtMPSCQueuePopBatch(&tQueue, NULL, 0u);
	testRequire((Batch.Result == XQUEUE_OK) && (Batch.Count == 0u), "MPSC zero pop batch mismatch");

	xrtClearError();
	Batch = xrtMPSCQueuePushBatch(&tQueue, NULL, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "MPSC NULL push batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPSC NULL push error mismatch");
	xrtClearError();
	Batch = xrtMPSCQueuePopBatch(&tQueue, NULL, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "MPSC NULL pop batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPSC NULL pop error mismatch");
	xrtClearError();
	testRequire(xrtMPSCQueueTryPop(&tQueue, NULL) == XQUEUE_ERROR, "MPSC NULL pop output should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "MPSC NULL pop output error mismatch");
	xrtMPSCQueueUnit(&tQueue);
}



/* 验证序号槽和 32 位游标跨越最大值后仍保持 FIFO 与满空判断。 */
static void testMPSCWrap(void)
{
	xmpscqueue tQueue;
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

	testRequire(xrtMPSCQueueInitBuffer(&tQueue, pStorage, 4u), "wrap MPSC init failed");
	for ( uint32 i = 0u; i < 4u; i++ ) {
		uint32 iPosition = iBase + i;
		xqueueslot* pSlot = &pStorage[iPosition & 3u];

		pSlot->Sequence.Value = iPosition;
		pSlot->Item = NULL;
	}
	tQueue.Head.Position.Value = iBase;
	tQueue.Tail.Position.Value = iBase;

	/* 验证批量预留、发布和释放在回绕边界保持完整序列代次。 */
	Batch = xrtMPSCQueuePushBatch(&tQueue, pInput, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u),
		"wrap MPSC first push batch mismatch"
	);
	Batch = xrtMPSCQueuePopBatch(&tQueue, pOutput, 2u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 2u) &&
		(pOutput[0] == pInput[0]) &&
		(pOutput[1] == pInput[1]),
		"wrap MPSC first pop batch mismatch"
	);
	Batch = xrtMPSCQueuePushBatch(&tQueue, pInput + 3u, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u),
		"wrap MPSC second push batch mismatch"
	);
	Batch = xrtMPSCQueuePopBatch(&tQueue, pOutput, 4u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 4u) &&
		(pOutput[0] == pInput[2]) &&
		(pOutput[1] == pInput[3]) &&
		(pOutput[2] == pInput[4]) &&
		(pOutput[3] == pInput[5]),
		"wrap MPSC second pop batch mismatch"
	);

	/* 再以单元素路径覆盖多个完整环周期。 */
	for ( uintptr_t i = 7u; i <= 22u; i++ ) {
		testRequire(
			xrtMPSCQueueTryPush(&tQueue, (ptr)(uintptr_t)i) == XQUEUE_OK,
			"wrap MPSC push failed"
		);
		testRequire(xrtMPSCQueueTryPop(&tQueue, &pItem) == XQUEUE_OK, "wrap MPSC pop failed");
		testRequire((uintptr_t)pItem == i, "wrap MPSC FIFO mismatch");
	}
	testRequire(xrtMPSCQueueCount(&tQueue) == 0u, "wrap MPSC final count mismatch");
	xrtMPSCQueueUnit(&tQueue);
}



/* 运行 MPSC 单线程合同、外部槽环和游标回绕测试。 */
int main(void)
{
	testMPSCBasic();
	testMPSCBufferAndAlias();
	testMPSCInvalid();
	testMPSCWrap();
	printf("[PASS] queue_mpsc\n");
	return 0;
}
