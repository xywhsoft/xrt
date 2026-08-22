#include "../test.h"



/* 排空上下文记录回调观察到的 FIFO 顺序。 */
typedef struct testdrainstate {
	ptr Items[8];
	size_t Count;
} testdrainstate;



/* 保存一个已经从队列移除的指针值。 */
static void testQueueDrain(ptr pItem, ptr pContext)
{
	testdrainstate* pState = (testdrainstate*)pContext;

	if ( pState->Count < 8u ) {
		pState->Items[pState->Count++] = pItem;
	}
}



/* 验证拥有型 SPSC 的容量、空值、批量、关闭和重置合同。 */
static void testSPSCBasic(void)
{
	xspscqueue tQueue;
	xspscqueue* pCreated;
	xqueuebatchresult Batch;
	testdrainstate tDrain = { { NULL }, 0 };
	int pValues[] = { 10, 20, 30, 40, 50, 60 };
	ptr pInput[] = { &pValues[2], &pValues[3], &pValues[4] };
	ptr pOutput[] = {
		(ptr)(uintptr_t)1u,
		(ptr)(uintptr_t)2u,
		(ptr)(uintptr_t)3u,
		(ptr)(uintptr_t)4u
	};
	ptr pItem = (ptr)(uintptr_t)1u;

	testRequire(sizeof(xqueuecursor32) == XRT_QUEUE_CACHE_SPAN, "SPSC cursor cache span mismatch");
	testRequire(xrtSPSCQueueInit(&tQueue, 3u), "SPSC init failed");
	testRequire(
		(tQueue.Capacity == 4u) &&
		(tQueue.Mask == 3u) &&
		(tQueue.Allocation == tQueue.Items),
		"SPSC initial state mismatch"
	);
	testRequire(!xrtSPSCQueueIsClosed(&tQueue), "SPSC should start open");
	testRequire(!xrtSPSCQueueIsDrained(&tQueue), "open empty SPSC should not be drained");
	testRequire(xrtSPSCQueueTryPop(&tQueue, &pItem) == XQUEUE_EMPTY, "empty SPSC result mismatch");
	testRequire(pItem == NULL, "empty SPSC did not clear output");

	testRequire(xrtSPSCQueueTryPush(&tQueue, &pValues[0]) == XQUEUE_OK, "SPSC first push failed");
	testRequire(xrtSPSCQueueTryPush(&tQueue, NULL) == XQUEUE_OK, "SPSC NULL push failed");
	Batch = xrtSPSCQueuePushBatch(&tQueue, pInput, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 2u),
		"SPSC partial push batch mismatch"
	);
	testRequire(xrtSPSCQueueCount(&tQueue) == 4u, "SPSC full count mismatch");
	testRequire(xrtSPSCQueueTryPush(&tQueue, &pValues[5]) == XQUEUE_FULL, "full SPSC result mismatch");
	Batch = xrtSPSCQueuePushBatch(&tQueue, pInput, 1u);
	testRequire(
		(Batch.Result == XQUEUE_FULL) && (Batch.Count == 0u),
		"full SPSC batch result mismatch"
	);

	Batch = xrtSPSCQueuePopBatch(&tQueue, pOutput, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 3u) &&
		(pOutput[0] == &pValues[0]) &&
		(pOutput[1] == NULL) &&
		(pOutput[2] == &pValues[2]) &&
		(pOutput[3] == (ptr)(uintptr_t)4u),
		"SPSC pop batch FIFO mismatch"
	);
	testRequire(xrtSPSCQueueCount(&tQueue) == 1u, "SPSC count after batch mismatch");
	xrtClearError();
	testRequire(!xrtSPSCQueueReset(&tQueue), "busy SPSC reset should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN, "busy SPSC reset error mismatch");

	xrtSPSCQueueClose(&tQueue);
	testRequire(xrtSPSCQueueIsClosed(&tQueue), "SPSC close state mismatch");
	testRequire(xrtSPSCQueueTryPush(&tQueue, &pValues[5]) == XQUEUE_CLOSED, "closed SPSC push mismatch");
	testRequire(xrtSPSCQueueDrain(&tQueue, testQueueDrain, &tDrain) == 1u, "SPSC drain count mismatch");
	testRequire(
		(tDrain.Count == 1u) && (tDrain.Items[0] == &pValues[3]),
		"SPSC drain order mismatch"
	);
	testRequire(xrtSPSCQueueIsDrained(&tQueue), "closed empty SPSC should be drained");
	testRequire(xrtSPSCQueueTryPop(&tQueue, &pItem) == XQUEUE_CLOSED, "drained SPSC pop mismatch");
	testRequire(xrtSPSCQueueReset(&tQueue), "drained SPSC reset failed");
	testRequire(!xrtSPSCQueueIsClosed(&tQueue), "SPSC reset did not reopen queue");

	testRequire(xrtSPSCQueueTryPush(&tQueue, &pValues[4]) == XQUEUE_OK, "SPSC discard setup failed");
	testRequire(xrtSPSCQueueDrain(&tQueue, NULL, NULL) == 1u, "SPSC discard drain mismatch");
	xrtSPSCQueueUnit(&tQueue);

	pCreated = xrtSPSCQueueCreate(8u);
	testRequire(pCreated != NULL, "SPSC create failed");
	xrtSPSCQueueDestroy(pCreated);
}



/* 验证外部缓冲、批量别名和结构损坏检查。 */
static void testSPSCBufferAndAlias(void)
{
	xspscqueue tQueue;
	unsigned char arrUnaligned[(sizeof(ptr) * 5u) + 1u];
	ptr pStorage[4] = {
		(ptr)(uintptr_t)1u,
		(ptr)(uintptr_t)2u,
		(ptr)(uintptr_t)3u,
		(ptr)(uintptr_t)4u
	};
	ptr pValue = (ptr)(uintptr_t)5u;
	ptr* pUnaligned;
	ptr pSavedAllocation;
	ptr* pSavedItems;
	size_t iSavedMask;
	xqueuebatchresult Batch;

	pUnaligned = (ptr*)(void*)(
		(((uintptr_t)arrUnaligned + sizeof(ptr) - 1u) &
			~((uintptr_t)sizeof(ptr) - 1u)) + 1u
	);
	testRequire(
		xrtSPSCQueueInitBuffer(&tQueue, pStorage, 4u),
		"external SPSC init failed"
	);
	testRequire(
		(tQueue.Allocation == NULL) &&
		(pStorage[0] == NULL) &&
		(pStorage[3] == NULL),
		"external SPSC buffer state mismatch"
	);

	xrtClearError();
	Batch = xrtSPSCQueuePushBatch(
		&tQueue,
		(ptr const*)(const void*)&tQueue.Items,
		1u
	);
	testRequire(
		Batch.Result == XQUEUE_ERROR,
		"SPSC metadata push batch should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"SPSC metadata push alias error mismatch"
	);
	xrtClearError();
	Batch = xrtSPSCQueuePushBatch(&tQueue, pStorage, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "SPSC aliased push batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "SPSC push alias error mismatch");
	testRequire(xrtSPSCQueueTryPush(&tQueue, pValue) == XQUEUE_OK, "external SPSC push failed");

	xrtClearError();
	testRequire(
		xrtSPSCQueueTryPop(&tQueue, &pStorage[1]) == XQUEUE_ERROR,
		"SPSC aliased single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"SPSC single pop alias error mismatch"
	);
	testRequire(
		(xrtSPSCQueueCount(&tQueue) == 1u) && (pStorage[0] == pValue),
		"SPSC single pop alias changed queue"
	);

	xrtClearError();
	testRequire(
		xrtSPSCQueueTryPop(
			&tQueue,
			(ptr*)(void*)&tQueue.Items
		) == XQUEUE_ERROR,
		"SPSC metadata single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"SPSC metadata single pop error mismatch"
	);
	testRequire(
		(tQueue.Items == pStorage) && (xrtSPSCQueueCount(&tQueue) == 1u),
		"SPSC metadata single pop changed queue"
	);

	xrtClearError();
	testRequire(
		xrtSPSCQueueTryPop(&tQueue, pUnaligned) == XQUEUE_ERROR,
		"SPSC unaligned single pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"SPSC unaligned single pop error mismatch"
	);

	xrtClearError();
	Batch = xrtSPSCQueuePopBatch(&tQueue, pStorage, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "SPSC aliased pop batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "SPSC pop alias error mismatch");
	xrtClearError();
	Batch = xrtSPSCQueuePopBatch(
		&tQueue,
		(ptr*)(void*)&tQueue.Items,
		1u
	);
	testRequire(
		Batch.Result == XQUEUE_ERROR,
		"SPSC metadata pop batch should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"SPSC metadata pop alias error mismatch"
	);
	testRequire(xrtSPSCQueueTryPop(&tQueue, &pValue) == XQUEUE_OK, "external SPSC recovery pop failed");

	/* 静态摘要损坏必须报告状态错误且不能释放错误地址。 */
	pSavedAllocation = tQueue.Allocation;
	pSavedItems = tQueue.Items;
	iSavedMask = tQueue.Mask;
	tQueue.Items = (ptr*)(void*)&tQueue;
	xrtClearError();
	testRequire(
		xrtSPSCQueueTryPush(&tQueue, pValue) == XQUEUE_ERROR,
		"overlapping damaged SPSC storage should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"overlapping damaged SPSC storage error mismatch"
	);
	tQueue.Items = pSavedItems;
	tQueue.Mask = 0u;
	xrtClearError();
	testRequire(xrtSPSCQueueCount(&tQueue) == 0u, "corrupt SPSC count should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "corrupt SPSC error mismatch");
	tQueue.Allocation = pSavedAllocation;
	tQueue.Mask = iSavedMask;
	xrtSPSCQueueUnit(&tQueue);
	testRequire(pStorage[0] == NULL, "SPSC unit changed external buffer unexpectedly");

	xrtClearError();
	testRequire(
		!xrtSPSCQueueInitBuffer(&tQueue, pStorage, 3u),
		"non-power-of-two external SPSC should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external SPSC capacity error mismatch");

	xrtClearError();
	testRequire(
		!xrtSPSCQueueInitBuffer(
			&tQueue,
				pUnaligned,
				4u
		),
		"unaligned external SPSC should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external SPSC alignment error mismatch");

	xrtClearError();
	testRequire(
		!xrtSPSCQueueInitBuffer(
			&tQueue,
				(ptr*)(void*)&tQueue,
				4u
		),
		"SPSC structure and buffer overlap should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "external SPSC overlap error mismatch");
}



/* 验证损坏游标差不会被当作可用容量或普通满空状态。 */
static void testSPSCCorruptCursor(void)
{
	xspscqueue tQueue;
	ptr pStorage[4];
	ptr pInput[1] = { (ptr)(uintptr_t)7u };
	ptr pOutput[1] = { (ptr)(uintptr_t)9u };
	xqueuebatchresult Batch;

	testRequire(
		xrtSPSCQueueInitBuffer(&tQueue, pStorage, 4u),
		"corrupt-cursor SPSC init failed"
	);
	tQueue.Head.Position.Value = 0u;
	tQueue.Tail.Position.Value = 5u;

	xrtClearError();
	testRequire(
		xrtSPSCQueueTryPush(&tQueue, pInput[0]) == XQUEUE_ERROR,
		"corrupt-cursor SPSC push should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"corrupt-cursor SPSC push error mismatch"
	);

	xrtClearError();
	Batch = xrtSPSCQueuePushBatch(&tQueue, pInput, 1u);
	testRequire(
		(Batch.Result == XQUEUE_ERROR) && (Batch.Count == 0u),
		"corrupt-cursor SPSC push batch should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"corrupt-cursor SPSC push batch error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtSPSCQueueTryPop(&tQueue, &pOutput[0]) == XQUEUE_ERROR,
		"corrupt-cursor SPSC pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"corrupt-cursor SPSC pop error mismatch"
	);
	testRequire(
		pOutput[0] == (ptr)(uintptr_t)9u,
		"corrupt-cursor SPSC pop changed output"
	);

	xrtClearError();
	Batch = xrtSPSCQueuePopBatch(&tQueue, pOutput, 1u);
	testRequire(
		(Batch.Result == XQUEUE_ERROR) && (Batch.Count == 0u),
		"corrupt-cursor SPSC pop batch should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"corrupt-cursor SPSC pop batch error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtSPSCQueueReset(&tQueue),
		"corrupt-cursor SPSC reset should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"corrupt-cursor SPSC reset error mismatch"
	);

	tQueue.Head.Position.Value = 0u;
	tQueue.Tail.Position.Value = 0u;
	xrtSPSCQueueUnit(&tQueue);
}



/* 验证初始化失败留下零状态，空批量操作不要求数组地址。 */
static void testSPSCInvalid(void)
{
	xspscqueue tQueue;
	xqueuebatchresult Batch;
	ptr pStorage[4];

	memset(&tQueue, 0xaa, sizeof(tQueue));
	xrtClearError();
	testRequire(!xrtSPSCQueueInit(&tQueue, 0u), "zero SPSC init should fail");
	testRequire(tQueue.Items == NULL, "failed SPSC init did not leave zero state");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "zero SPSC init error mismatch");

	testRequire(xrtSPSCQueueInitBuffer(&tQueue, pStorage, 4u), "SPSC no-op batch setup failed");
	Batch = xrtSPSCQueuePushBatch(&tQueue, NULL, 0u);
	testRequire((Batch.Result == XQUEUE_OK) && (Batch.Count == 0u), "SPSC zero push batch mismatch");
	Batch = xrtSPSCQueuePopBatch(&tQueue, NULL, 0u);
	testRequire((Batch.Result == XQUEUE_OK) && (Batch.Count == 0u), "SPSC zero pop batch mismatch");

	xrtClearError();
	Batch = xrtSPSCQueuePushBatch(&tQueue, NULL, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "SPSC NULL push batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "SPSC NULL push error mismatch");
	xrtClearError();
	Batch = xrtSPSCQueuePopBatch(&tQueue, NULL, 1u);
	testRequire(Batch.Result == XQUEUE_ERROR, "SPSC NULL pop batch should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "SPSC NULL pop error mismatch");
	xrtSPSCQueueUnit(&tQueue);
}



/* 验证 32 位游标跨越最大值后仍保持 FIFO 和满空判断。 */
static void testSPSCWrap(void)
{
	xspscqueue tQueue;
	ptr pStorage[4];
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

	testRequire(xrtSPSCQueueInitBuffer(&tQueue, pStorage, 4u), "wrap SPSC init failed");
	tQueue.Head.Position.Value = iBase;
	tQueue.Tail.Position.Value = iBase;

	/* 先让批量发布、部分领取和再次发布连续跨越 32 位边界。 */
	Batch = xrtSPSCQueuePushBatch(&tQueue, pInput, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u),
		"wrap SPSC first push batch mismatch"
	);
	Batch = xrtSPSCQueuePopBatch(&tQueue, pOutput, 2u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 2u) &&
		(pOutput[0] == pInput[0]) &&
		(pOutput[1] == pInput[1]),
		"wrap SPSC first pop batch mismatch"
	);
	Batch = xrtSPSCQueuePushBatch(&tQueue, pInput + 3u, 3u);
	testRequire(
		(Batch.Result == XQUEUE_OK) && (Batch.Count == 3u),
		"wrap SPSC second push batch mismatch"
	);
	Batch = xrtSPSCQueuePopBatch(&tQueue, pOutput, 4u);
	testRequire(
		(Batch.Result == XQUEUE_OK) &&
		(Batch.Count == 4u) &&
		(pOutput[0] == pInput[2]) &&
		(pOutput[1] == pInput[3]) &&
		(pOutput[2] == pInput[4]) &&
		(pOutput[3] == pInput[5]),
		"wrap SPSC second pop batch mismatch"
	);

	/* 再以单元素路径覆盖多个环周期。 */
	for ( uintptr_t i = 7u; i <= 22u; i++ ) {
		testRequire(
			xrtSPSCQueueTryPush(&tQueue, (ptr)(uintptr_t)i) == XQUEUE_OK,
			"wrap SPSC push failed"
		);
		testRequire(xrtSPSCQueueTryPop(&tQueue, &pItem) == XQUEUE_OK, "wrap SPSC pop failed");
		testRequire((uintptr_t)pItem == i, "wrap SPSC FIFO mismatch");
	}
	testRequire(xrtSPSCQueueCount(&tQueue) == 0u, "wrap SPSC final count mismatch");
	xrtSPSCQueueUnit(&tQueue);
}



/* 运行 SPSC 单线程、外部缓冲和游标回绕测试。 */
int main(void)
{
	testSPSCBasic();
	testSPSCBufferAndAlias();
	testSPSCCorruptCursor();
	testSPSCInvalid();
	testSPSCWrap();
	printf("[PASS] queue_spsc\n");
	return 0;
}
