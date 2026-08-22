#include "../../src/internal/xrt_channel.h"
#include "../test.h"



/* 排空上下文记录回调观察到的 FIFO 顺序。 */
typedef struct testchanneldrain {
	ptr Items[8];
	size_t Count;
	xchannel* Channel;
	ptr Reentrant;
	xchannelresult ReentrantResult;
} testchanneldrain;



/* 保存一个已经从 Channel 移除的指针值。 */
static void testChannelDrain(ptr pItem, ptr pContext)
{
	testchanneldrain* pDrain = (testchanneldrain*)pContext;

	if ( pDrain->Count < 8u ) {
		pDrain->Items[pDrain->Count++] = pItem;
	}
	if (
		(pDrain->Channel != NULL) &&
		(pDrain->Count == 1u)
	) {
		pDrain->ReentrantResult = xrtChannelTrySend(
			pDrain->Channel,
			pDrain->Reentrant
		);
	}
}



/* 验证精确容量、空值、FIFO、关闭排空和重置。 */
static void testChannelBuffered(void)
{
	xchannel tChannel;
	xchannel* pCreated;
	int arrValue[] = { 10, 20, 30, 40 };
	ptr pItem = (ptr)(uintptr_t)1u;
	testchanneldrain tDrain = {
		{ NULL },
		0,
		NULL,
		NULL,
		XCHANNEL_ERROR
	};

	testRequire(xrtChannelInit(&tChannel, 1u), "capacity-one channel init failed");
	testRequire(xrtChannelCapacity(&tChannel) == 1u, "channel capacity was not exact");
	testRequire(xrtChannelCount(&tChannel) == 0u, "channel initial count mismatch");
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_EMPTY,
		"empty channel result mismatch"
	);
	testRequire(pItem == NULL, "empty channel did not clear output");

	testRequire(
		xrtChannelTrySend(&tChannel, NULL) == XCHANNEL_OK,
		"channel NULL send failed"
	);
	testRequire(xrtChannelCount(&tChannel) == 1u, "channel full count mismatch");
	testRequire(
		xrtChannelTrySend(&tChannel, &arrValue[0]) == XCHANNEL_FULL,
		"capacity-one channel did not report full"
	);
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_OK,
		"channel NULL receive failed"
	);
	testRequire(pItem == NULL, "channel NULL payload mismatch");
	testRequire(
		xrtChannelSendFor(&tChannel, &arrValue[0], 0) == XWAIT_OK,
		"ready zero-timeout send failed"
	);
	testRequire(
		xrtChannelSendFor(&tChannel, &arrValue[1], 0) == XWAIT_TIMEOUT,
		"full zero-timeout send result mismatch"
	);
	testRequire(
		xrtChannelRecvFor(&tChannel, &pItem, 0) == XWAIT_OK,
		"ready zero-timeout receive failed"
	);
	testRequire(pItem == &arrValue[0], "zero-timeout receive payload mismatch");
	testRequire(
		xrtChannelRecvFor(&tChannel, &pItem, 0) == XWAIT_TIMEOUT,
		"empty zero-timeout receive result mismatch"
	);

	testRequire(
		xrtChannelTrySend(&tChannel, &arrValue[1]) == XCHANNEL_OK,
		"channel close setup failed"
	);
	xrtChannelClose(&tChannel);
	xrtChannelClose(&tChannel);
	testRequire(xrtChannelIsClosed(&tChannel), "channel did not close");
	testRequire(!xrtChannelIsDrained(&tChannel), "closed channel drained too early");
	testRequire(
		xrtChannelTrySend(&tChannel, &arrValue[2]) == XCHANNEL_CLOSED,
		"closed channel accepted send"
	);
	testRequire(
		xrtChannelRecv(&tChannel, &pItem) == XWAIT_OK,
		"closed channel did not drain value"
	);
	testRequire(pItem == &arrValue[1], "closed channel drain payload mismatch");
	testRequire(xrtChannelIsDrained(&tChannel), "closed empty channel was not drained");
	testRequire(
		xrtChannelRecv(&tChannel, &pItem) == XWAIT_CLOSED,
		"drained channel receive result mismatch"
	);
	testRequire(pItem == NULL, "closed receive did not clear output");
	testRequire(xrtChannelReset(&tChannel), "channel reset failed");
	testRequire(!xrtChannelIsClosed(&tChannel), "channel reset did not reopen");
	testRequire(xrtChannelUnit(&tChannel), "channel unit failed");

	pCreated = xrtChannelCreate(3u);
	testRequire(pCreated != NULL, "channel create failed");
	testRequire(xrtChannelCapacity(pCreated) == 3u, "created channel capacity mismatch");
	testRequire(
		xrtChannelTrySend(pCreated, &arrValue[0]) == XCHANNEL_OK,
		"channel drain first send failed"
	);
	testRequire(
		xrtChannelTrySend(pCreated, &arrValue[1]) == XCHANNEL_OK,
		"channel drain second send failed"
	);
	tDrain.Channel = pCreated;
	tDrain.Reentrant = &arrValue[2];
	testRequire(
		xrtChannelDrain(pCreated, testChannelDrain, &tDrain) == 2u,
		"channel drain count mismatch"
	);
	testRequire(
		(tDrain.Count == 2u) &&
		(tDrain.Items[0] == &arrValue[0]) &&
		(tDrain.Items[1] == &arrValue[1]),
		"channel drain FIFO mismatch"
	);
	testRequire(
		tDrain.ReentrantResult == XCHANNEL_OK,
		"channel drain callback ran under internal lock"
	);
	testRequire(
		(xrtChannelTryRecv(pCreated, &pItem) == XCHANNEL_OK) &&
		(pItem == &arrValue[2]),
		"channel drain consumed callback insertion"
	);
	testRequire(xrtChannelDestroy(pCreated), "channel destroy failed");
	testRequire(xrtChannelDestroy(NULL), "null channel destroy failed");
}



/* 验证无缓冲 Channel 的非阻塞和关闭基础语义。 */
static void testChannelRendezvous(void)
{
	xchannel tChannel;
	ptr pItem = (ptr)(uintptr_t)1u;

	testRequire(xrtChannelInit(&tChannel, 0), "rendezvous channel init failed");
	testRequire(xrtChannelCapacity(&tChannel) == 0, "rendezvous capacity mismatch");
	testRequire(xrtChannelCount(&tChannel) == 0, "rendezvous count mismatch");
	testRequire(
		xrtChannelTrySend(&tChannel, pItem) == XCHANNEL_FULL,
		"rendezvous try send without receiver should not commit"
	);
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_EMPTY,
		"rendezvous try receive should be empty"
	);
	testRequire(pItem == NULL, "empty rendezvous receive did not clear output");
	testRequire(
		xrtChannelSendFor(
			&tChannel,
			(ptr)(uintptr_t)7u,
			0
		) == XWAIT_TIMEOUT,
		"rendezvous zero-timeout send mismatch"
	);
	xrtChannelClose(&tChannel);
	testRequire(
		xrtChannelTrySend(&tChannel, NULL) == XCHANNEL_CLOSED,
		"closed rendezvous accepted send"
	);
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_CLOSED,
		"closed rendezvous receive mismatch"
	);
	testRequire(xrtChannelIsDrained(&tChannel), "closed rendezvous not drained");
	testRequire(xrtChannelReset(&tChannel), "rendezvous reset failed");
	testRequire(!xrtChannelIsClosed(&tChannel), "rendezvous reset did not reopen");
	testRequire(xrtChannelUnit(&tChannel), "rendezvous unit failed");
}



/* 验证外部缓冲、输出别名和公开状态损坏检查。 */
static void testChannelBufferAndDamage(void)
{
	xchannel tChannel;
	xrt_channel_impl* pImpl;
	ptr arrItems[3];
	unsigned char arrUnaligned[(sizeof(ptr) * 2u) + 1u];
	ptr* pUnaligned;
	ptr pValue = (ptr)(uintptr_t)41u;
	size_t iSavedCount;
	size_t iSavedTail;

	pUnaligned = (ptr*)(void*)(
		(((uintptr_t)arrUnaligned + sizeof(ptr) - 1u) &
		 ~((uintptr_t)sizeof(ptr) - 1u)) + 1u
	);
	testRequire(
		xrtChannelInitBuffer(&tChannel, arrItems, 3u),
		"external channel init failed"
	);
	pImpl = (xrt_channel_impl*)&tChannel;
	testRequire(pImpl->Allocation == NULL, "external channel owns caller buffer");
	testRequire(
		(arrItems[0] == NULL) && (arrItems[2] == NULL),
		"external channel buffer was not cleared"
	);
	testRequire(
		xrtChannelTrySend(&tChannel, pValue) == XCHANNEL_OK,
		"external channel send failed"
	);

	xrtClearError();
	testRequire(
		xrtChannelTryRecv(&tChannel, &arrItems[1]) == XCHANNEL_ERROR,
		"channel accepted output inside internal ring"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"channel ring output alias error mismatch"
	);
	testRequire(xrtChannelCount(&tChannel) == 1u, "alias failure changed channel count");

	xrtClearError();
	testRequire(
		xrtChannelTryRecv(
			&tChannel,
			(ptr*)(void*)&pImpl->Capacity
		) == XCHANNEL_ERROR,
		"channel accepted output inside object"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"channel object output alias error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtChannelTryRecv(&tChannel, pUnaligned) == XCHANNEL_ERROR,
		"channel accepted unaligned output"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"channel unaligned output error mismatch"
	);

	iSavedCount = pImpl->Count;
	pImpl->Count = pImpl->Capacity + 1u;
	xrtClearError();
	testRequire(
		xrtChannelTrySend(&tChannel, NULL) == XCHANNEL_ERROR,
		"channel accepted damaged count"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"channel damaged count error mismatch"
	);
	pImpl->Count = iSavedCount;

	iSavedTail = pImpl->Tail;
	pImpl->Tail = (pImpl->Tail + 1u) % pImpl->Capacity;
	xrtClearError();
	testRequire(
		xrtChannelCount(&tChannel) == 0,
		"channel accepted damaged cursor relation"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"channel damaged cursor error mismatch"
	);
	pImpl->Tail = iSavedTail;

	testRequire(
		xrtChannelTryRecv(&tChannel, &pValue) == XCHANNEL_OK,
		"external channel recovery receive failed"
	);
	testRequire(
		pValue == (ptr)(uintptr_t)41u,
		"external channel recovery payload mismatch"
	);
	testRequire(xrtChannelUnit(&tChannel), "external channel unit failed");

	xrtClearError();
	testRequire(
		!xrtChannelInitBuffer(
			&tChannel,
			(ptr*)(void*)&tChannel,
			1u
		),
		"channel accepted overlapping external buffer"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"channel overlapping buffer error mismatch"
	);
}



/* 验证空参数、溢出和未初始化对象的错误口径。 */
static void testChannelErrors(void)
{
	xchannel tChannel;
	unsigned char arrUnaligned[sizeof(xchannel) + sizeof(uint64)];
	xchannel* pUnaligned;
	ptr pItem = (ptr)(uintptr_t)99u;

	pUnaligned = (xchannel*)(void*)(
		(((uintptr_t)arrUnaligned + sizeof(uint64) - 1u) &
		 ~((uintptr_t)sizeof(uint64) - 1u)) + 1u
	);
	memset(&tChannel, 0, sizeof(tChannel));
	xrtClearError();
	testRequire(
		xrtChannelTrySend(NULL, NULL) == XCHANNEL_ERROR,
		"null channel send result mismatch"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null channel send error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_ERROR,
		"uninitialized channel receive result mismatch"
	);
	testRequire(
		(pItem == (ptr)(uintptr_t)99u) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"uninitialized channel receive contract mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtChannelInit(
			&tChannel,
			(SIZE_MAX / sizeof(ptr)) + 1u
		),
		"channel accepted capacity byte overflow"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"channel capacity overflow error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtChannelInit(pUnaligned, 0),
		"channel accepted unaligned object"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"channel unaligned object error mismatch"
	);
}



/* 执行 Channel 单线程合同测试。 */
int main(void)
{
	testChannelBuffered();
	testChannelRendezvous();
	testChannelBufferAndDamage();
	testChannelErrors();
	printf("[PASS] channel\n");
	return 0;
}
