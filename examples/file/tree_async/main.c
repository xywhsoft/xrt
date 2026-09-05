#include <stdio.h>
#include <xrt.h>



/*
 * 范例：file/tree_async —— 异步目录树复制（带统计结果）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDirCopyAsync      异步递归复制
 *   Future 值 = xwalkstats   复制统计（条目/文件/字节）
 * 模块宏：XRT_MODULE_FILE_ASYNC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/tree_async/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   copied 1 items
 *
 * Future 不只带"完成"信号——结果是遍历统计的直接借用，
 *   进度汇报（"已复制 N 项 / M 字节"）不需要再扫一遍。
 *   大树后台复制 + 主线程汇报进度的完整拼图。
 */


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
