#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通系统路径查询与绝对路径转换。 */
int main(void)
{
	str sCwd = xrtPathCwd();
	str sAbs = xrtPathAbs(".");
	str sReal = xrtPathReal(".");
	int iResult = (sCwd != NULL) && (sAbs != NULL) && (sReal != NULL) &&
		xrtPathIsAbs(sCwd) && xrtPathIsAbs(sAbs) &&
		xrtPathIsAbs(sReal) ? 0 : 1;

	xrtFree(sCwd);
	xrtFree(sAbs);
	xrtFree(sReal);
	return iResult;
}
