#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通文件创建、完整读写、定位和删除。 */
int main(void)
{
	str sDirectory = xrtPathTemp();
	str sPath;
	xfile File;
	char arrBuffer[3];
	uint64 iPosition;
	int iResult = 1;

	if ( sDirectory == NULL ) {
		return 1;
	}
	sPath = xrtPathJoin(sDirectory, "xrt-single-file.tmp");
	xrtFree(sDirectory);
	if ( sPath == NULL ) {
		return 1;
	}
	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_EXCLUSIVE);
	if ( (File != NULL) && xrtWriteFull(File, "abc", 3, NULL) &&
		xrtSeek(File, 1, XSEEK_START, NULL) &&
		xrtReadAtFull(File, 0, arrBuffer, sizeof(arrBuffer), NULL) &&
		xrtTell(File, &iPosition) && (iPosition == 1u) &&
		xrtWriteAtFull(File, 1, "B", 1, NULL) &&
		xrtTell(File, &iPosition) && (iPosition == 1u) &&
		xrtSeek(File, 0, XSEEK_START, NULL) &&
		xrtReadFull(File, arrBuffer, sizeof(arrBuffer), NULL) &&
		(memcmp(arrBuffer, "aBc", 3) == 0) && xrtClose(File) &&
		xrtFileDelete(sPath) ) {
		iResult = 0;
	}
	xrtFree(sPath);
	return iResult;
}
