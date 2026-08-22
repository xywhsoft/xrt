#include "../test.h"
#include "typed_queue_failure_fixture.h"



/* 验证 SPSC 在复制和移动失败后保留槽位、元素与所有权。 */
static void testTypedSPSCQueueFailure(void)
{
	xrttype Type = testTypedQueueValueType();
	xtypedspscqueue Queue;
	int iOwners = 1;
	testtypedqueuevalue Source = { 17, &iOwners };
	testtypedqueuevalue Output = { 0, NULL };

	testRequire(
		xrtTypedSPSCQueueInit(&Queue, &Type, 2u) &&
		(xrtTypedSPSCQueueTryPush(&Queue, &Source) == XQUEUE_OK) &&
		(iOwners == 2),
		"typed SPSC ownership fixture failed"
	);
	testTypedQueueValueFailures(true, false);
	testRequire(
		(xrtTypedSPSCQueueTryPush(&Queue, &Source) == XQUEUE_ERROR) &&
		(xrtTypedSPSCQueueCount(&Queue) == 1u) &&
		(iOwners == 2) && (xrtErrorCause(xrtGetError()) != NULL),
		"typed SPSC copy failure changed queue ownership"
	);
	testTypedQueueValueFailures(false, false);
	testRequire(
		(xrtTypedSPSCQueueTryPush(&Queue, &Source) == XQUEUE_OK) &&
		(xrtTypedSPSCQueueCount(&Queue) == 2u) && (iOwners == 3),
		"typed SPSC did not reuse a failed producer cell"
	);
	testTypedQueueValueFailures(false, true);
	testRequire(
		(xrtTypedSPSCQueueTryPop(&Queue, &Output) == XQUEUE_ERROR) &&
		(xrtTypedSPSCQueueCount(&Queue) == 2u) &&
		(Output.Owners == NULL) && (iOwners == 3),
		"typed SPSC move failure lost an item"
	);
	testTypedQueueValueFailures(false, false);
	testRequire(
		(xrtTypedSPSCQueueTryPop(&Queue, &Output) == XQUEUE_OK) &&
		(Output.Value == 17) && (xrtTypedSPSCQueueCount(&Queue) == 1u) &&
		(iOwners == 3),
		"typed SPSC failed item retry mismatch"
	);
	xrtTypeDropValue(&Type, &Output);
	testRequire(
		(xrtTypedSPSCQueueTryPop(&Queue, &Output) == XQUEUE_OK) &&
		(Output.Value == 17) && (xrtTypedSPSCQueueCount(&Queue) == 0u),
		"typed SPSC reused producer cell was not readable"
	);
	xrtTypeDropValue(&Type, &Output);
	xrtTypedSPSCQueueUnit(&Queue);
	xrtTypeDropValue(&Type, &Source);
	testRequire(iOwners == 0, "typed SPSC leaked an owned value");
}



/* 验证 MPSC 在复制和移动失败后保留槽位、元素与所有权。 */
static void testTypedMPSCQueueFailure(void)
{
	xrttype Type = testTypedQueueValueType();
	xtypedmpscqueue Queue;
	int iOwners = 1;
	testtypedqueuevalue Source = { 27, &iOwners };
	testtypedqueuevalue Output = { 0, NULL };

	testRequire(
		xrtTypedMPSCQueueInit(&Queue, &Type, 2u) &&
		(xrtTypedMPSCQueueTryPush(&Queue, &Source) == XQUEUE_OK),
		"typed MPSC ownership fixture failed"
	);
	testTypedQueueValueFailures(true, false);
	testRequire(
		(xrtTypedMPSCQueueTryPush(&Queue, &Source) == XQUEUE_ERROR) &&
		(xrtTypedMPSCQueueCount(&Queue) == 1u) && (iOwners == 2),
		"typed MPSC copy failure changed queue ownership"
	);
	testTypedQueueValueFailures(false, false);
	testRequire(
		(xrtTypedMPSCQueueTryPush(&Queue, &Source) == XQUEUE_OK) &&
		(xrtTypedMPSCQueueCount(&Queue) == 2u) && (iOwners == 3),
		"typed MPSC did not recycle a failed producer cell"
	);
	testTypedQueueValueFailures(false, true);
	testRequire(
		(xrtTypedMPSCQueueTryPop(&Queue, &Output) == XQUEUE_ERROR) &&
		(xrtTypedMPSCQueueCount(&Queue) == 2u) &&
		(Output.Owners == NULL) && (iOwners == 3),
		"typed MPSC move failure lost an item"
	);
	testTypedQueueValueFailures(false, false);
	testRequire(
		(xrtTypedMPSCQueueTryPop(&Queue, &Output) == XQUEUE_OK) &&
		(Output.Value == 27) && (xrtTypedMPSCQueueCount(&Queue) == 1u) &&
		(iOwners == 3),
		"typed MPSC failed item retry mismatch"
	);
	xrtTypeDropValue(&Type, &Output);
	testRequire(
		(xrtTypedMPSCQueueTryPop(&Queue, &Output) == XQUEUE_OK) &&
		(Output.Value == 27) && (xrtTypedMPSCQueueCount(&Queue) == 0u),
		"typed MPSC recycled producer cell was not readable"
	);
	xrtTypeDropValue(&Type, &Output);
	xrtTypedMPSCQueueUnit(&Queue);
	xrtTypeDropValue(&Type, &Source);
	testRequire(iOwners == 0, "typed MPSC leaked an owned value");
}



/* 验证 MPMC 在复制和移动失败后通过重试环保留元素。 */
static void testTypedMPMCQueueFailure(void)
{
	xrttype Type = testTypedQueueValueType();
	xtypedmpmcqueue Queue;
	int iOwners = 1;
	testtypedqueuevalue Source = { 37, &iOwners };
	testtypedqueuevalue Output = { 0, NULL };

	testRequire(
		xrtTypedMPMCQueueInit(&Queue, &Type, 2u) &&
		(xrtTypedMPMCQueueTryPush(&Queue, &Source) == XQUEUE_OK),
		"typed MPMC ownership fixture failed"
	);
	testTypedQueueValueFailures(true, false);
	testRequire(
		(xrtTypedMPMCQueueTryPush(&Queue, &Source) == XQUEUE_ERROR) &&
		(xrtTypedMPMCQueueCount(&Queue) == 1u) && (iOwners == 2),
		"typed MPMC copy failure changed queue ownership"
	);
	testTypedQueueValueFailures(false, true);
	testRequire(
		(xrtTypedMPMCQueueTryPushTake(&Queue, &Source) == XQUEUE_ERROR) &&
		(xrtTypedMPMCQueueCount(&Queue) == 1u) &&
		(Source.Owners == &iOwners) && (iOwners == 2),
		"typed MPMC move-in failure consumed the source"
	);
	testTypedQueueValueFailures(false, false);
	testRequire(
		(xrtTypedMPMCQueueTryPushTake(&Queue, &Source) == XQUEUE_OK) &&
		(xrtTypedMPMCQueueCount(&Queue) == 2u) &&
		(Source.Owners == NULL) && (iOwners == 2),
		"typed MPMC move-in retry mismatch"
	);
	testTypedQueueValueFailures(false, true);
	testRequire(
		(xrtTypedMPMCQueueTryPop(&Queue, &Output) == XQUEUE_ERROR) &&
		(xrtTypedMPMCQueueCount(&Queue) == 2u) &&
		(Output.Owners == NULL),
		"typed MPMC move failure lost an item"
	);
	testTypedQueueValueFailures(false, false);
	testRequire(
		(xrtTypedMPMCQueueTryPop(&Queue, &Output) == XQUEUE_OK) &&
		(Output.Value == 37) && (xrtTypedMPMCQueueCount(&Queue) == 1u) &&
		(iOwners == 2),
		"typed MPMC failed item retry mismatch"
	);
	xrtTypeDropValue(&Type, &Output);
	testRequire(
		(xrtTypedMPMCQueueTryPop(&Queue, &Output) == XQUEUE_OK) &&
		(Output.Value == 37) && (xrtTypedMPMCQueueCount(&Queue) == 0u),
		"typed MPMC moved producer value was not readable"
	);
	xrtTypeDropValue(&Type, &Output);
	xrtTypedMPMCQueueUnit(&Queue);
	xrtTypeDropValue(&Type, &Source);
	testRequire(iOwners == 0, "typed MPMC leaked an owned value");
}



/* 验证内部值槽不能作为公开输入或输出覆盖队列所有权。 */
static void testTypedQueueAliases(void)
{
	xtypedspscqueue SPSC;
	xtypedmpscqueue MPSC;
	xtypedmpmcqueue MPMC;
	int32 iValue = 9;

	testRequire(
		xrtTypedSPSCQueueInit(&SPSC, xrtTypeInt32(), 2u) &&
		xrtTypedMPSCQueueInit(&MPSC, xrtTypeInt32(), 2u) &&
		xrtTypedMPMCQueueInit(&MPMC, xrtTypeInt32(), 2u),
		"typed queue alias fixture failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedSPSCQueueTryPush(&SPSC, SPSC.Core.Values) == XQUEUE_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"typed SPSC accepted an internal source"
	);
	testRequire(
		(xrtTypedMPSCQueueTryPush(&MPSC, &iValue) == XQUEUE_OK) &&
		(xrtTypedMPSCQueueTryPop(&MPSC, MPSC.Core.Values) == XQUEUE_ERROR),
		"typed MPSC accepted an internal output"
	);
	testRequire(
		(xrtTypedMPMCQueueTryPush(&MPMC, &iValue) == XQUEUE_OK) &&
		(xrtTypedMPMCQueueTryPop(&MPMC, MPMC.Core.Values) == XQUEUE_ERROR),
		"typed MPMC accepted an internal output"
	);
	xrtTypedMPMCQueueUnit(&MPMC);
	xrtTypedMPSCQueueUnit(&MPSC);
	xrtTypedSPSCQueueUnit(&SPSC);
}



/* 验证类型能力和对象描述必须完整匹配具体并发队列。 */
static void testTypedQueueTypes(void)
{
	xrttype Type = testTypedQueueValueType();
	xrttype MissingMove = Type;
	xrttypeops MissingMoveOps = *Type.Ops;
	const xrttype* arrArguments[] = { &Type };
	xtypedqueuemeta Meta = { 4u };
	xrttype QueueType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-queue.SPSC")),
		.Kind = XRT_TYPE_LIST,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("SPSC"),
		.AbiName = XRT_STR_INIT("tests.typed-queue.SPSC"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(xtypedspscqueue),
		.InstanceAlign = TEST_ALIGNOF(xtypedspscqueue),
		.InstanceOps = xrtTypedSPSCQueueInstanceOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments,
		.Metadata = &Meta
	};
	xtypedspscqueue Queue;

	MissingMoveOps.Move = NULL;
	MissingMove.Ops = &MissingMoveOps;
	testRequire(
		!xrtTypedSPSCQueueInit(&Queue, xrtTypeNull(), 2u),
		"typed queue accepted a zero-size item"
	);
	testRequire(
		!xrtTypedSPSCQueueInit(&Queue, &MissingMove, 2u),
		"typed queue accepted an item without move support"
	);
	testRequire(
		xrtTypedSPSCQueueTypeValidate(&QueueType),
		"valid typed SPSC object type was rejected"
	);
	QueueType.Metadata = NULL;
	testRequire(
		!xrtTypedSPSCQueueTypeValidate(&QueueType),
		"typed queue object type accepted missing capacity metadata"
	);
}



/* 运行三种类型队列的所有权与失败恢复合同。 */
int main(void)
{
	testTypedSPSCQueueFailure();
	testTypedMPSCQueueFailure();
	testTypedMPMCQueueFailure();
	testTypedQueueAliases();
	testTypedQueueTypes();
	testTypedQueueValueFailures(false, false);
	xrtClearError();
	printf("[PASS] typed queue contract\n");
	return 0;
}
