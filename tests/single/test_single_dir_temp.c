#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通安全临时目录创建。 */
int main(void)
{
	str sPath = xrtDirTemp(".", "xrt-single-dir-temp-", NULL);
	int iResult = 1;

	if ( (sPath != NULL) && xrtDirRemove(sPath) ) {
		iResult = 0;
	}
	xrtFree(sPath);
	return iResult;
}
