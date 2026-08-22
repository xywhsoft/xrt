#include <stdio.h>
#include <xrt.h>



/*
 * 建立父子任务作用域，验证父级取消会传播到子组和叶 Future，
 * 同时仍由叶生产端确认最终取消状态。
 */
int main(void)
{
	int iResult = 1;
	xtaskgroup* pParent = NULL;
	xtaskgroup* pChild = NULL;
	xfuture* pLeaf = NULL;
	xfuture* pDone = NULL;
	xpromise* pLeafPromise = NULL;
	xcancel* pLeafCancel = NULL;
	xtaskgroupstats tStats;

	/* 子组作为父组中的一项，叶 Future 再由子组跟踪。 */
	pParent = xrtTaskGroupCreate(NULL);
	if ( pParent == NULL ) {
		goto cleanup;
	}
	pChild = xrtTaskGroupChild(pParent, NULL);
	pLeafPromise = xrtPromiseCreate(&pLeaf, NULL);
	if ( (pChild == NULL) || (pLeafPromise == NULL) ||
		!xrtTaskGroupAdd(pChild, pLeaf) ) {
		goto cleanup;
	}
	pDone = xrtTaskGroupFuture(pParent);
	pLeafCancel = xrtFutureCancelToken(pLeaf);
	if ( (pDone == NULL) || (pLeafCancel == NULL) ) {
		goto cleanup;
	}

	/* 父级只发出协作取消请求，叶生产端负责确认自己的终态。 */
	if ( !xrtTaskGroupCancel(pParent) ||
		!xrtCancelRequested(pLeafCancel) ||
		!xrtPromiseCancel(pLeafPromise) ||
		(xrtFutureWait(pDone) != XWAIT_OK) ) {
		goto cleanup;
	}
	if ( (xrtFutureState(pLeaf) != XFUTURE_CANCELLED) ||
		!xrtTaskGroupGet(pParent, &tStats) ) {
		goto cleanup;
	}
	printf(
		"parent completed = %llu, cancelled = %llu\n",
		(unsigned long long)tStats.Completed,
		(unsigned long long)tStats.Cancelled
	);
	iResult = 0;

cleanup:
	/* 子组和父组允许在仍有活动监听时延迟完成内部回收。 */
	xrtCancelDestroy(pLeafCancel);
	xrtFutureDestroy(pDone);
	xrtPromiseDestroy(pLeafPromise);
	xrtFutureDestroy(pLeaf);
	xrtTaskGroupDestroy(pChild);
	xrtTaskGroupDestroy(pParent);
	return iResult;
}
