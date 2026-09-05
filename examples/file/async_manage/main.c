#include <xrt.h>



/*
 * 范例：file/async_manage —— 管理类异步 Helper：复制与删除
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFileCopyAsync / xrtFileDeleteAsync   整文件级异步操作
 * 模块宏：XRT_MODULE_FILE_ASYNC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/async_manage/main.c -lws2_32 -liphlpapi
 * 预期输出：（无输出，操作成功即静默退出 0）
 *
 * Helper 层定位：不需要句柄、直接给路径的整操作——
 *   安装器/更新器的"后台复制一批文件再删旧版"主线。
 *   bReplace 参数（true）允许覆盖已存在目标。
 */


/* 使用文件管理异步 Helper 复制并删除文件。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 8, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pCopy = NULL;
	xfuture* pDelete = NULL;
	int iResult = 1;
	static const char sSource[] = "xrt-async-manage-source.txt";
	static const char sTarget[] = "xrt-async-manage-target.txt";

	if ( (pPool == NULL) ||
		!xrtFileWriteAll(sSource, XRT_BYTES_LITERAL("copy me")) ) {
		goto Exit;
	}
	pCopy = xrtFileCopyAsync(pPool, sSource, sTarget, true);
	if ( (pCopy == NULL) ||
		(xrtFutureWait(pCopy) != XWAIT_OK) ||
		(xrtFutureState(pCopy) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pDelete = xrtFileDeleteAsync(pPool, sTarget);
	if ( (pDelete == NULL) ||
		(xrtFutureWait(pDelete) != XWAIT_OK) ||
		(xrtFutureState(pDelete) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	xrtFutureDestroy(pCopy);
	xrtFutureDestroy(pDelete);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtFileDelete(sSource);
	(void)xrtFileDelete(sTarget);
	return iResult;
}
