#include <xrt.h>



/*
 * 范例：file/dir_async —— 异步目录创建与删除
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDirCreateAsync / xrtDirRemoveAsync   目录级异步 Helper
 * 模块宏：XRT_MODULE_FILE_ASYNC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/dir_async/main.c -lws2_32 -liphlpapi
 * 预期输出：（静默成功，退出码 0）
 *
 * 与文件 Helper 同族：路径进、Future 出——
 *   安装/迁移流程"建目录 → 拷文件 → 删旧目录"全链路异步化。
 */


/* 使用显式任务池创建并删除目录。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 8, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pCreate = NULL;
	xfuture* pRemove = NULL;
	int iResult = 1;
	static const char sPath[] = "xrt-dir-async-example";

	if ( pPool == NULL ) {
		goto Exit;
	}
	(void)xrtDirRemove(sPath);
	xrtClearError();
	pCreate = xrtDirCreateAsync(pPool, sPath);
	if ( (pCreate == NULL) ||
		(xrtFutureWait(pCreate) != XWAIT_OK) ||
		(xrtFutureState(pCreate) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pRemove = xrtDirRemoveAsync(pPool, sPath);
	if ( (pRemove == NULL) ||
		(xrtFutureWait(pRemove) != XWAIT_OK) ||
		(xrtFutureState(pRemove) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	xrtFutureDestroy(pCreate);
	xrtFutureDestroy(pRemove);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtDirRemove(sPath);
	return iResult;
}
