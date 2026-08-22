#include <stdio.h>

#include <xrt.h>



/* 展示当前目录、用户目录、临时目录和程序目录查询。 */
int main(void)
{
	str sCwd = NULL;
	str sHome = NULL;
	str sTemp = NULL;
	str sApp = NULL;
	str sReal = NULL;
	int iResult = 1;

	/* 查询各类系统路径并统一处理部分失败。 */
	sCwd = xrtPathCwd();
	sHome = xrtPathHome();
	sTemp = xrtPathTemp();
	sApp = xrtPathAppDir();
	sReal = xrtPathReal(".");
	if ( (sCwd == NULL) || (sHome == NULL) ||
		 (sTemp == NULL) || (sApp == NULL) || (sReal == NULL) ) {
		goto cleanup;
	}
	printf("cwd=%s\nreal=%s\nhome=%s\ntemp=%s\napp=%s\n",
		sCwd, sReal, sHome, sTemp, sApp);
	iResult = 0;

cleanup:
	xrtFree(sCwd);
	xrtFree(sHome);
	xrtFree(sTemp);
	xrtFree(sApp);
	xrtFree(sReal);
	return iResult;
}
