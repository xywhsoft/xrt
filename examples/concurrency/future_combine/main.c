#include <stdio.h>
#include <xrt.h>



/*
 * 范例：concurrency/future_combine —— 组合子：Any / Race / All
 * ----------------------------------------------------------------
 * 演示 API：
 *   Any    任一完成即返回（索引 + 值）
 *   Race   首胜 + 请求败者取消（不伪造败者终态！）
 *   All    全部完成的值序列
 * 模块宏：XRT_MODULE_FUTURE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/future_combine/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   winner[1] = 22, values = 11, 22
 *
 * Race 的取消语义（本范例专门验证）：胜者出结果后
 *   只对败者"请求取消"，败者的最终状态由其生产端
 *   自行确认——框架不替它伪造 CANCELLED。
 */


/*
 * 同时观察 Any、Race 和 All 的结果，并验证 Race 只请求败者取消，
 * 不会替生产端伪造败者的最终状态。
 */
int main(void)
{
	int iResult = 1;
	int iFirst = 11;
	int iSecond = 22;
	xfuture* pFirst = NULL;
	xfuture* pSecond = NULL;
	xfuture* pAny = NULL;
	xfuture* pRace = NULL;
	xfuture* pAll = NULL;
	xfuture* arrFuture[2];
	xpromise* pFirstPromise = NULL;
	xpromise* pSecondPromise = NULL;
	xcancel* pFirstCancel = NULL;
	const xfuturepick* pAnyPick;
	const xfuturepick* pRacePick;
	const xfutureall* pAllValue;

	/* Promise 与 Future 成对创建，组合器只保留 Future 引用。 */
	pFirstPromise = xrtPromiseCreate(&pFirst, NULL);
	pSecondPromise = xrtPromiseCreate(&pSecond, NULL);
	if ( (pFirstPromise == NULL) || (pSecondPromise == NULL) ) {
		goto cleanup;
	}
	arrFuture[0] = pFirst;
	arrFuture[1] = pSecond;
	pAny = xrtFutureAny(arrFuture, 2);
	pRace = xrtFutureRace(arrFuture, 2);
	pAll = xrtFutureAll(arrFuture, 2);
	pFirstCancel = xrtFutureCancelToken(pFirst);
	if ( (pAny == NULL) || (pRace == NULL) || (pAll == NULL) ||
		(pFirstCancel == NULL) ) {
		goto cleanup;
	}

	/* 第二个源先完成，因此它同时成为 Any 与 Race 的胜出源。 */
	if ( !xrtPromiseResolve(pSecondPromise, &iSecond) ||
		(xrtFutureWait(pAny) != XWAIT_OK) ||
		(xrtFutureWait(pRace) != XWAIT_OK) ) {
		goto cleanup;
	}
	pAnyPick = (const xfuturepick*)xrtFutureValue(pAny);
	pRacePick = (const xfuturepick*)xrtFutureValue(pRace);
	if ( (pAnyPick == NULL) || (pRacePick == NULL) ||
		(pAnyPick->Index != 1) || (pRacePick->Index != 1) ||
		!xrtCancelRequested(pFirstCancel) ) {
		goto cleanup;
	}

	/* Race 只发出请求；生产端仍可选择正常完成，随后 All 才会完成。 */
	if ( !xrtPromiseResolve(pFirstPromise, &iFirst) ||
		(xrtFutureWait(pAll) != XWAIT_OK) ) {
		goto cleanup;
	}
	pAllValue = (const xfutureall*)xrtFutureValue(pAll);
	if ( (pAllValue == NULL) || (pAllValue->Count != 2) ) {
		goto cleanup;
	}
	printf(
		"winner[%u] = %d, values = %d, %d\n",
		(unsigned)pRacePick->Index,
		*(int*)xrtFutureValue(pRacePick->Future),
		*(int*)xrtFutureValue(pAllValue->Futures[0]),
		*(int*)xrtFutureValue(pAllValue->Futures[1])
	);
	iResult = 0;

cleanup:
	/* 按引用所有权逆序释放组合器、取消令牌和端点。 */
	xrtCancelDestroy(pFirstCancel);
	xrtFutureDestroy(pAll);
	xrtFutureDestroy(pRace);
	xrtFutureDestroy(pAny);
	xrtPromiseDestroy(pSecondPromise);
	xrtPromiseDestroy(pFirstPromise);
	xrtFutureDestroy(pSecond);
	xrtFutureDestroy(pFirst);
	return iResult;
}
