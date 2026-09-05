#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 等待 Future 成功并返回借用值。 */
static ptr waitValue(xfuture* pFuture)
{
	if ( (pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		return NULL;
	}
	return xrtFutureValue(pFuture);
}



/*
 * 范例：file/async —— 异步文件句柄：任务池上的偏移读写
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTaskPoolCreate({线程数, 队列深度, 标志})   有界任务池
 *   xrtAsyncOpen        打开异步句柄（绑定任务池）
 *   WriteAtAsync / ReadAtAsync   绝对偏移异步读写
 *   xrtFutureWait / Value        等待并取结果（xfiledata）
 * 模块宏：XRT_MODULE_FILE_ASYNC（依赖 TASK/FUTURE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/async/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hello async file
 *
 * 异步的分工：IO 在池线程执行，调用线程拿 Future 继续；
 *   有界队列（32）提供背压——池满时提交失败而非无限堆积。
 *   read/write 顺序经 Future 链保证，close 也异步且等待完成。
 */


/* 使用有界任务池完成异步绝对偏移读写。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 32, 0 };
	xfileoptions Options;
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xasyncfile* pFile = NULL;
	xfuture* pWrite = NULL;
	xfuture* pRead = NULL;
	xfuture* pClose = NULL;
	xfiledata* pData;
	int iResult = 1;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE;
	if ( pPool == NULL ) {
		goto Exit;
	}
	pFile = xrtAsyncFileOpen(
		pPool,
		"xrt-async-example.txt",
		&Options
	);
	if ( pFile == NULL ) {
		goto Exit;
	}
	pWrite = xrtAsyncFileWriteAt(
		pFile,
		0,
		XRT_BYTES_LITERAL("hello async file")
	);
	if ( waitValue(pWrite) == NULL ) {
		goto Exit;
	}
	pRead = xrtAsyncFileReadAt(pFile, 0, 64);
	pData = (xfiledata*)waitValue(pRead);
	if ( pData == NULL ) {
		goto Exit;
	}
	printf(
		"%.*s\n",
		(int)pData->Size,
		(const char*)pData->Data
	);
	pClose = xrtAsyncFileClose(pFile);
	pFile = NULL;
	if ( (pClose == NULL) ||
		(xrtFutureWait(pClose) != XWAIT_OK) ||
		(xrtFutureState(pClose) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	if ( pFile != NULL ) {
		xfuture* pPending = xrtAsyncFileClose(pFile);

		if ( pPending != NULL ) {
			(void)xrtFutureWait(pPending);
			xrtFutureDestroy(pPending);
		}
	}
	xrtFutureDestroy(pWrite);
	xrtFutureDestroy(pRead);
	xrtFutureDestroy(pClose);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	return iResult;
}
