#include "../test.h"



/* 验证非阻塞选择、关闭、轮转公平和参数边界。 */
static void testChannelSelectTry(void)
{
	xchannel tFirst;
	xchannel tSecond;
	xchannelcase arrCase[2];
	xchannelselectresult tResult;
	ptr pFirst = (ptr)(uintptr_t)101u;
	ptr pSecond = (ptr)(uintptr_t)202u;
	size_t arrChosen[2] = { 0, 0 };

	testRequire(xrtChannelInit(&tFirst, 1u), "first select channel init failed");
	testRequire(xrtChannelInit(&tSecond, 1u), "second select channel init failed");

	arrCase[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
	arrCase[1] = xrtChannelCaseRecv(&tSecond, &pSecond);
	tResult = xrtChannelSelectTry(arrCase, 2u);
	testRequire(
		(tResult.Wait == XWAIT_TIMEOUT) &&
		(tResult.Index == XCHANNEL_SELECT_NONE),
		"empty select try result mismatch"
	);
	testRequire(
		((uintptr_t)pFirst == 101u) &&
		((uintptr_t)pSecond == 202u),
		"empty select modified receive outputs"
	);

	/* 两个始终就绪的 case 必须都能被轮转起点选中。 */
	for ( size_t i = 0; i < 32u; i++ ) {
		ptr pDiscard = NULL;

		testRequire(
			xrtChannelTrySend(
				&tFirst,
				(ptr)(uintptr_t)(1000u + i)
			) == XCHANNEL_OK,
			"first fairness setup failed"
		);
		testRequire(
			xrtChannelTrySend(
				&tSecond,
				(ptr)(uintptr_t)(2000u + i)
			) == XCHANNEL_OK,
			"second fairness setup failed"
		);
		tResult = xrtChannelSelectTry(arrCase, 2u);
		testRequire(
			(tResult.Wait == XWAIT_OK) &&
			(tResult.Result == XCHANNEL_OK) &&
			(tResult.Index < 2u),
			"ready select try failed"
		);
		arrChosen[tResult.Index]++;
		testRequire(
			xrtChannelTryRecv(
				tResult.Index == 0 ? &tSecond : &tFirst,
				&pDiscard
			) == XCHANNEL_OK,
			"fairness loser cleanup failed"
		);
	}
	testRequire(
		(arrChosen[0] != 0) && (arrChosen[1] != 0),
		"select always favored one ready case"
	);

	/* 发送选择必须跳过满 Channel 并提交另一个 case。 */
	testRequire(
		xrtChannelTrySend(
			&tFirst,
			(ptr)(uintptr_t)7u
		) == XCHANNEL_OK,
		"send select prefill failed"
	);
	arrCase[0] = xrtChannelCaseSend(
		&tFirst,
		(ptr)(uintptr_t)8u
	);
	arrCase[1] = xrtChannelCaseSend(
		&tSecond,
		(ptr)(uintptr_t)9u
	);
	tResult = xrtChannelSelectTry(arrCase, 2u);
	testRequire(
		(tResult.Wait == XWAIT_OK) &&
		(tResult.Index == 1u) &&
		(tResult.Result == XCHANNEL_OK),
		"send select chose full case"
	);
	testRequire(
		xrtChannelTryRecv(&tFirst, &pFirst) == XCHANNEL_OK,
		"send select first cleanup failed"
	);
	testRequire(
		xrtChannelTryRecv(&tSecond, &pSecond) == XCHANNEL_OK,
		"send select second cleanup failed"
	);
	testRequire(
		((uintptr_t)pFirst == 7u) &&
		((uintptr_t)pSecond == 9u),
		"send select payload mismatch"
	);

	xrtChannelClose(&tFirst);
	arrCase[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
	tResult = xrtChannelSelectTry(arrCase, 1u);
	testRequire(
		(tResult.Wait == XWAIT_OK) &&
		(tResult.Index == 0) &&
		(tResult.Result == XCHANNEL_CLOSED) &&
		(pFirst == NULL),
		"closed receive was not selectable"
	);
	testRequire(xrtChannelUnit(&tFirst), "first select channel unit failed");
	testRequire(xrtChannelUnit(&tSecond), "second select channel unit failed");
}



/* 验证 Select 的数组、操作、对齐和跨 Channel 别名检查。 */
static void testChannelSelectErrors(void)
{
	xchannel tFirst;
	xchannel tSecond;
	xchannelcase arrCase[2];
	xchannelselectresult tResult;
	unsigned char arrUnaligned[(sizeof(ptr) * 2u) + 1u];
	ptr* pUnaligned;

	testRequire(xrtChannelInit(&tFirst, 1u), "select error first init failed");
	testRequire(xrtChannelInit(&tSecond, 1u), "select error second init failed");
	pUnaligned = (ptr*)(void*)(
		(((uintptr_t)arrUnaligned + sizeof(ptr) - 1u) &
		 ~((uintptr_t)sizeof(ptr) - 1u)) + 1u
	);

	xrtClearError();
	tResult = xrtChannelSelectTry(NULL, 0);
	testRequire(
		(tResult.Wait == XWAIT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null select cases error mismatch"
	);

	arrCase[0] = xrtChannelCaseRecv(
		&tFirst,
		pUnaligned
	);
	xrtClearError();
	tResult = xrtChannelSelectTry(arrCase, 1u);
	testRequire(
		(tResult.Wait == XWAIT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"unaligned select output error mismatch"
	);

	arrCase[0] = xrtChannelCaseRecv(
		&tFirst,
		(ptr*)(void*)&tSecond
	);
	arrCase[1] = xrtChannelCaseSend(
		&tSecond,
		(ptr)(uintptr_t)1u
	);
	xrtClearError();
	tResult = xrtChannelSelectTry(arrCase, 2u);
	testRequire(
		(tResult.Wait == XWAIT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"cross-channel output alias was accepted"
	);

	arrCase[0] = xrtChannelCaseSend(&tFirst, NULL);
	arrCase[0].Operation = (xchannelop)99;
	xrtClearError();
	tResult = xrtChannelSelectTry(arrCase, 1u);
	testRequire(
		(tResult.Wait == XWAIT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid select operation error mismatch"
	);

	testRequire(xrtChannelUnit(&tFirst), "select error first unit failed");
	testRequire(xrtChannelUnit(&tSecond), "select error second unit failed");
}



/* 执行 Channel Select 基础合同测试。 */
int main(void)
{
	testChannelSelectTry();
	testChannelSelectErrors();
	printf("[PASS] channel select\n");
	return 0;
}
