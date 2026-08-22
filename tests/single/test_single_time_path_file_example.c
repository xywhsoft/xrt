#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头聚合能够组合时间、路径、临时目录和原子文本文件能力。 */
int main(void)
{
	char arrTime[40];
	str sDirectory = NULL;
	str sPath = NULL;
	str sText = NULL;
	uint64 iStart = xrtClock();
	size_t iSize = 0;
	size_t iTimeSize;
	bool bValid = false;

	iTimeSize = xrtTimeWriteRFC3339(
		arrTime, sizeof(arrTime), xrtNow(), 0
	);
	sDirectory = xrtDirTemp(NULL, "xrt-single-report-", NULL);
	if ( (iTimeSize == XRT_NPOS) ||
		 (iTimeSize >= sizeof(arrTime)) ||
		 (sDirectory == NULL) ) {
		goto cleanup;
	}
	sPath = xrtPathJoin(sDirectory, "report.txt");
	if ( (sPath == NULL) ||
		 !xrtFileWriteTextAtomic(
			sPath,
			(xstrview){ arrTime, iTimeSize },
			XENCODING_UTF8,
			XUTF_STRICT,
			false
		 ) ) {
		goto cleanup;
	}
	sText = xrtFileReadTextLimit(
		sPath, XENCODING_UTF8, XUTF_STRICT, 128u, &iSize
	);
	bValid = (sText != NULL) && (iSize == iTimeSize) &&
		(memcmp(sText, arrTime, iSize) == 0) &&
		(xrtClock() >= iStart);

cleanup:
	if ( (sPath != NULL) && xrtFileExists(sPath) &&
		 !xrtFileDelete(sPath) ) {
		bValid = false;
	}
	if ( (sDirectory != NULL) && xrtDirExists(sDirectory) &&
		 !xrtDirRemove(sDirectory) ) {
		bValid = false;
	}
	xrtFree(sText);
	xrtFree(sPath);
	xrtFree(sDirectory);
	return bValid ? 0 : 1;
}
