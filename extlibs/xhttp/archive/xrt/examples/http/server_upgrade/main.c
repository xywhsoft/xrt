#include <xrt.h>
#include <stdio.h>



/* 接管结果必须先安装新协议事件，再显式处理缓冲余量并恢复读取。 */
static void onUpgrade(
	xhttpconn* pConnection,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
)
{
	(void)pConnection;
	(void)pError;
	(void)pData;
	if ( Result != XNET_RESULT_OK ) {
		return;
	}
	xrtHttpUpgradeAbort(&Upgrade);
}



/* 示例只展示 Upgrade 回调的所有权清理。 */
int main(void)
{
	(void)onUpgrade;
	printf("HTTP server Upgrade callback is ready\n");
	return 0;
}
