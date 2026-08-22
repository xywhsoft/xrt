#include <stdio.h>
#include <xrt.h>



/* 异步复制目录树，并读取 Future 拥有的统计结果。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pCopy = NULL;
	xfuture* pRemove = NULL;
	xwalkstats* pStats;
	int iResult = 1;
	static const char sSource[] = "xrt-tree-async-example-source";
	static const char sTarget[] = "xrt-tree-async-example-target";

	if ( pPool == NULL ) {
		goto Exit;
	}
	(void)xrtDirRemoveAll(sSource);
	(void)xrtDirRemoveAll(sTarget);
	xrtClearError();
	if ( !xrtDirCreate(sSource) ) {
		goto Exit;
	}
	pCopy = xrtDirCopyAsync(pPool, sSource, sTarget, false);
	if ( (pCopy == NULL) ||
		(xrtFutureWait(pCopy) != XWAIT_OK) ||
		(xrtFutureState(pCopy) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pStats = (xwalkstats*)xrtFutureValue(pCopy);
	if ( pStats == NULL ) {
		goto Exit;
	}
	printf("copied %llu items\n", (unsigned long long)pStats->Items);
	pRemove = xrtDirRemoveAllAsync(pPool, sTarget);
	if ( (pRemove == NULL) ||
		(xrtFutureWait(pRemove) != XWAIT_OK) ||
		(xrtFutureState(pRemove) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	xrtFutureDestroy(pCopy);
	xrtFutureDestroy(pRemove);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtDirRemoveAll(sSource);
	(void)xrtDirRemoveAll(sTarget);
	return iResult;
}
