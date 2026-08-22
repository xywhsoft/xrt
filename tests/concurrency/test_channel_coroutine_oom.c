#include "../../src/internal/xrt_channel.h"
#include "../test.h"



/* 可切换分配器用于证明常用 Channel Await 路径不分配。 */
typedef struct testchannelawaitoom {
	bool Fail;
} testchannelawaitoom;



/* 正常阶段转发到 C 分配器，失败阶段拒绝新分配。 */
static ptr testChannelAwaitOomAlloc(ptr pContext, size_t iSize)
{
	testchannelawaitoom* pState =
		(testchannelawaitoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 正常阶段允许调整内存，失败阶段保持原对象不变。 */
static ptr testChannelAwaitOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testchannelawaitoom* pState =
		(testchannelawaitoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testChannelAwaitOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* OOM 协程验证八个 case 的栈上路径和九个 case 的失败清理。 */
typedef struct testchannelawaitoomcase {
	testchannelawaitoom* Allocator;
	xchannel* Ready;
	xchannel* Blocked;
	xwaitresult ReadyResult;
	xchannelselectresult InlineResult;
	xchannelselectresult HeapResult;
	xerrkind ErrorKind;
	ptr ReadyValue;
	ptr Outputs[9];
} testchannelawaitoomcase;



/* 在失败分配器下执行三条不同的 Channel Await 路径。 */
static ptr testChannelAwaitOomProc(ptr pData)
{
	testchannelawaitoomcase* pContext =
		(testchannelawaitoomcase*)pData;
	xchannelcase arrCase[9];

	pContext->Allocator->Fail = true;
	pContext->ReadyResult = xrtChannelRecvAwait(
		pContext->Ready,
		&pContext->ReadyValue
	);
	for ( size_t i = 0; i < 9u; i++ ) {
		pContext->Outputs[i] = (ptr)(uintptr_t)(i + 1u);
		arrCase[i] = xrtChannelCaseRecv(
			&pContext->Blocked[i],
			&pContext->Outputs[i]
		);
	}
	pContext->InlineResult = xrtChannelSelectAwaitFor(
		arrCase,
		8u,
		UINT64_C(1000)
	);
	pContext->HeapResult = xrtChannelSelectAwait(
		arrCase,
		9u
	);
	pContext->ErrorKind = xrtErrorKind(xrtGetError());
	pContext->Allocator->Fail = false;
	return pContext;
}



/* 验证八 case 无堆分配，九 case OOM 不留下等待节点。 */
int main(void)
{
	testchannelawaitoom tAllocatorState;
	testchannelawaitoomcase tCase;
	xallocator tAllocator;
	xchannel tReady;
	xchannel arrBlocked[9];
	xcosched* pSched;
	xcoro* pCo;

	memset(&tAllocatorState, 0, sizeof(tAllocatorState));
	memset(&tCase, 0, sizeof(tCase));
	memset(&tAllocator, 0, sizeof(tAllocator));
	tAllocator.Context = &tAllocatorState;
	tAllocator.Alloc = testChannelAwaitOomAlloc;
	tAllocator.Realloc = testChannelAwaitOomRealloc;
	tAllocator.Free = testChannelAwaitOomFree;
	testRequire(
		xrtSetAllocator(&tAllocator),
		"channel await OOM allocator install failed"
	);
	testRequire(xrtChannelInit(&tReady, 1u), "await OOM ready init failed");
	for ( size_t i = 0; i < 9u; i++ ) {
		testRequire(
			xrtChannelInit(&arrBlocked[i], 0),
			"await OOM blocked init failed"
		);
	}
	testRequire(
		xrtChannelTrySend(
			&tReady,
			(ptr)(uintptr_t)57u
		) == XCHANNEL_OK,
		"await OOM ready setup failed"
	);
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "await OOM scheduler create failed");
	tCase.Allocator = &tAllocatorState;
	tCase.Ready = &tReady;
	tCase.Blocked = arrBlocked;
	pCo = xrtCoSpawn(pSched, testChannelAwaitOomProc, &tCase, NULL);
	testRequire(pCo != NULL, "await OOM coroutine spawn failed");
	testRequire(xrtCoSchedRun(pSched), "await OOM scheduler run failed");

	testRequire(
		(tCase.ReadyResult == XWAIT_OK) &&
		((uintptr_t)tCase.ReadyValue == 57u),
		"ready Channel Await allocated under OOM"
	);
	testRequire(
		(tCase.InlineResult.Wait == XWAIT_TIMEOUT) &&
		(tCase.InlineResult.Index == XCHANNEL_SELECT_NONE),
		"eight-case Channel Await allocated under OOM"
	);
	testRequire(
		(tCase.HeapResult.Wait == XWAIT_ERROR) &&
		(tCase.HeapResult.Index == XCHANNEL_SELECT_NONE) &&
		(tCase.ErrorKind == XERR_MEMORY),
		"nine-case Channel Await OOM result mismatch"
	);
	for ( size_t i = 0; i < 9u; i++ ) {
		testRequire(
			((xrt_channel_impl*)&arrBlocked[i])->SelectWaiters == NULL,
			"Channel Await OOM leaked registration"
		);
		testRequire(
			(uintptr_t)tCase.Outputs[i] == (uintptr_t)(i + 1u),
			"Channel Await OOM modified output"
		);
	}

	testRequire(xrtCoDestroy(pCo), "await OOM coroutine destroy failed");
	testRequire(xrtCoSchedDestroy(pSched), "await OOM scheduler destroy failed");
	testRequire(xrtCoThreadDetach(), "await OOM runtime detach failed");
	testRequire(xrtChannelUnit(&tReady), "await OOM ready unit failed");
	for ( size_t i = 0; i < 9u; i++ ) {
		testRequire(
			xrtChannelUnit(&arrBlocked[i]),
			"await OOM blocked unit failed"
		);
	}
	printf("[PASS] channel coroutine OOM\n");
	return 0;
}
