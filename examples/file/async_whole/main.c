#include <stdio.h>
#include <xrt.h>



/*
 * 范例：file/async_whole —— 异步整文件读写：原子写 + 全量读
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFileWriteAtomicAsync   异步原子替换
 *   xrtFileReadAllAsync       异步整读（结果 xfiledata）
 * 模块宏：XRT_MODULE_FILE_ASYNC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/async_whole/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hello async whole file
 *
 * 同步版（whole 范例）语义完全一致，只是执行搬进池线程——
 *   事件循环里保存状态文件、加载配置不阻塞主线的标准姿势。
 */


/* 使用显式任务池完成原子写入和整文件读取。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pWrite = NULL;
	xfuture* pRead = NULL;
	xfiledata* pData;
	int iResult = 1;
	static const char sPath[] = "xrt-async-whole-example.txt";

	if ( pPool == NULL ) {
		goto Exit;
	}
	pWrite = xrtFileWriteAtomicAsync(
		pPool,
		sPath,
		XRT_BYTES_LITERAL("hello async whole file")
	);
	if ( (pWrite == NULL) ||
		(xrtFutureWait(pWrite) != XWAIT_OK) ||
		(xrtFutureState(pWrite) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pRead = xrtFileReadAllAsync(pPool, sPath);
	if ( (pRead == NULL) ||
		(xrtFutureWait(pRead) != XWAIT_OK) ||
		(xrtFutureState(pRead) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pData = (xfiledata*)xrtFutureValue(pRead);
	if ( pData == NULL ) {
		goto Exit;
	}
	printf("%.*s\n", (int)pData->Size, (const char*)pData->Data);
	iResult = 0;

Exit:
	xrtFutureDestroy(pWrite);
	xrtFutureDestroy(pRead);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	return iResult;
}
