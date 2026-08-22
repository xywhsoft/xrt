#include "../test.h"



/* 分配探针可在初始化后拒绝全部新分配。 */
typedef struct testtypedqueueoom {
	bool Fail;
	size_t Calls;
} testtypedqueueoom;



/* 记录并按开关分配类型队列测试内存。 */
static ptr testTypedQueueOomAlloc(ptr pContext, size_t iSize)
{
	testtypedqueueoom* pState = (testtypedqueueoom*)pContext;

	pState->Calls++;
	return pState->Fail ? NULL : malloc(iSize);
}



/* 记录并按开关重分配类型队列测试内存。 */
static ptr testTypedQueueOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypedqueueoom* pState = (testtypedqueueoom*)pContext;

	pState->Calls++;
	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放类型队列测试内存。 */
static void testTypedQueueOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证初始化后标量入队出队不再调用分配器。 */
static void testTypedQueueNoHotAllocation(testtypedqueueoom* pState)
{
	xtypedspscqueue SPSC;
	xtypedmpscqueue MPSC;
	xtypedmpmcqueue MPMC;
	uint8 arrInput[sizeof(uint64) + 7u];
	uint8 arrOutput[sizeof(uint64) + 7u];
	uint64 iValue = 71u;
	ptr pInput;
	ptr pOutput;
	size_t iCalls;

	pInput = (ptr)(((uintptr_t)arrInput + 7u) & ~(uintptr_t)7u);
	pOutput = (ptr)(((uintptr_t)arrOutput + 7u) & ~(uintptr_t)7u);
	memcpy(pInput, &iValue, sizeof(iValue));
	memset(pOutput, 0, sizeof(iValue));

	testRequire(
		xrtTypedSPSCQueueInit(&SPSC, xrtTypeUInt64(), 8u) &&
		xrtTypedMPSCQueueInit(&MPSC, xrtTypeUInt64(), 8u) &&
		xrtTypedMPMCQueueInit(&MPMC, xrtTypeUInt64(), 8u),
		"typed queue OOM fixture initialization failed"
	);
	iCalls = pState->Calls;
	pState->Fail = true;
	testRequire(
		(xrtTypedSPSCQueueTryPush(&SPSC, pInput) == XQUEUE_OK) &&
		(xrtTypedSPSCQueueTryPop(&SPSC, pOutput) == XQUEUE_OK) &&
		(xrtTypedMPSCQueueTryPush(&MPSC, pInput) == XQUEUE_OK) &&
		(xrtTypedMPSCQueueTryPop(&MPSC, pOutput) == XQUEUE_OK) &&
		(xrtTypedMPMCQueueTryPush(&MPMC, pInput) == XQUEUE_OK) &&
		(xrtTypedMPMCQueueTryPop(&MPMC, pOutput) == XQUEUE_OK) &&
		(pState->Calls == iCalls),
		"typed queue scalar hot path allocated memory"
	);
	xrtTypedMPMCQueueUnit(&MPMC);
	xrtTypedMPSCQueueUnit(&MPSC);
	xrtTypedSPSCQueueUnit(&SPSC);
	pState->Fail = false;
}



/* 验证三种初始化 OOM 都回滚为可安全结束的零状态。 */
static void testTypedQueueInitOom(testtypedqueueoom* pState)
{
	xtypedspscqueue SPSC;
	xtypedmpscqueue MPSC;
	xtypedmpmcqueue MPMC;

	pState->Fail = true;
	memset(&SPSC, 0xA5, sizeof(SPSC));
	memset(&MPSC, 0xA5, sizeof(MPSC));
	memset(&MPMC, 0xA5, sizeof(MPMC));
	xrtClearError();
	testRequire(
		!xrtTypedSPSCQueueInit(
			&SPSC, xrtTypeUInt64(), 1024u * 1024u
		) &&
		(SPSC.Core.ItemType == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"typed SPSC init OOM did not leave zero state"
	);
	xrtClearError();
	testRequire(
		!xrtTypedMPSCQueueInit(
			&MPSC, xrtTypeUInt64(), 1024u * 1024u
		) &&
		(MPSC.Core.ItemType == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"typed MPSC init OOM did not leave zero state"
	);
	xrtClearError();
	testRequire(
		!xrtTypedMPMCQueueInit(
			&MPMC, xrtTypeUInt64(), 1024u * 1024u
		) &&
		(MPMC.Core.ItemType == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"typed MPMC init OOM did not leave zero state"
	);
	xrtTypedMPMCQueueUnit(&MPMC);
	xrtTypedMPSCQueueUnit(&MPSC);
	xrtTypedSPSCQueueUnit(&SPSC);
	pState->Fail = false;
}



/* 运行类型队列初始化 OOM 和热路径零分配测试。 */
int main(void)
{
	testtypedqueueoom State = { false, 0u };
	xallocator Allocator = {
		&State,
		testTypedQueueOomAlloc,
		testTypedQueueOomRealloc,
		testTypedQueueOomFree
	};

	testRequire(
		xrtSetAllocator(&Allocator),
		"typed queue OOM allocator install failed"
	);
	testTypedQueueNoHotAllocation(&State);
	testTypedQueueInitOom(&State);
	xrtClearError();
	printf("[PASS] typed queue OOM\n");
	return 0;
}
