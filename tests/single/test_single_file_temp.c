#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通安全临时文件创建。 */
int main(void)
{
	str sPath = NULL;
	xfile File = xrtFileTemp(".", "xrt-single-file-temp-", ".tmp", &sPath);
	int iResult = 1;

	if ( (File != NULL) && (sPath != NULL) ) {
		bool bWritten = xrtWriteFull(File, "ok", 2u, NULL);
		bool bClosed = xrtClose(File);

		File = NULL;
		if ( bWritten && bClosed && xrtFileDelete(sPath) ) {
			iResult = 0;
		}
	}
	if ( File != NULL ) {
		(void)xrtClose(File);
	}
	if ( (sPath != NULL) && xrtFileExists(sPath) ) {
		(void)xrtFileDelete(sPath);
	}
	xrtFree(sPath);
	return iResult;
}
