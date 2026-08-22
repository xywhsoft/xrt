#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 在安全临时目录中原子写入并读回一份带时间的运行报告。 */
int main(void)
{
	char arrCreated[40];
	char arrFileName[64];
	char arrReport[512];
	str sDirectory = NULL;
	str sFilePath = NULL;
	str sReadBack = NULL;
	xtime iNow = xrtNow();
	uint64 iStart = xrtClock();
	size_t iCreatedSize;
	size_t iNameSize;
	size_t iReadSize = 0;
	int iReportSize;
	bool bValid = false;

	/* 临时目录由系统排他创建，不把可执行文件目录假设为可写。 */
	sDirectory = xrtDirTemp(NULL, "xrt-report-", NULL);
	if ( sDirectory == NULL ) {
		goto cleanup;
	}

	/* 人类可读时间进入正文，文件名使用不含平台禁用字符的 UTC 格式。 */
	iCreatedSize = xrtTimeWriteRFC3339(
		arrCreated, sizeof(arrCreated), iNow, 0
	);
	iNameSize = xrtTimeWrite(
		arrFileName,
		sizeof(arrFileName),
		iNow,
		0,
		XRT_STR_LITERAL("report_%Y%m%d_%H%M%S_%f.txt")
	);
	if ( (iCreatedSize == XRT_NPOS) ||
		 (iCreatedSize >= sizeof(arrCreated)) ||
		 (iNameSize == XRT_NPOS) ||
		 (iNameSize >= sizeof(arrFileName)) ) {
		goto cleanup;
	}
	sFilePath = xrtPathJoin(sDirectory, arrFileName);
	if ( sFilePath == NULL ) {
		goto cleanup;
	}

	/* 原子文本写入避免并发读者观察到半份报告。 */
	iReportSize = snprintf(
		arrReport,
		sizeof(arrReport),
		"created: %s\npath: %s\n",
		arrCreated,
		sFilePath
	);
	if ( (iReportSize < 0) ||
		 ((size_t)iReportSize >= sizeof(arrReport)) ||
		 !xrtFileWriteTextAtomic(
			sFilePath,
			(xstrview){ arrReport, (size_t)iReportSize },
			XENCODING_UTF8,
			XUTF_STRICT,
			false
		 ) ) {
		goto cleanup;
	}

	/* 对读回路径施加硬上限，并按返回长度验证 UTF-8 内容。 */
	sReadBack = xrtFileReadTextLimit(
		sFilePath, XENCODING_UTF8, XUTF_STRICT, 4096u, &iReadSize
	);
	if ( (sReadBack == NULL) || (iReadSize != (size_t)iReportSize) ||
		 (memcmp(sReadBack, arrReport, iReadSize) != 0) ) {
		goto cleanup;
	}
	printf("%.*s", (int)iReadSize, sReadBack);
	printf(
		"elapsed_us: %llu\n",
		(unsigned long long)(xrtClock() - iStart)
	);
	bValid = true;

cleanup:
	/* 示例不保留临时工件，先删除文件再删除空目录。 */
	if ( (sFilePath != NULL) && xrtFileExists(sFilePath) &&
		 !xrtFileDelete(sFilePath) ) {
		bValid = false;
	}
	if ( (sDirectory != NULL) && xrtDirExists(sDirectory) &&
		 !xrtDirRemove(sDirectory) ) {
		bValid = false;
	}
	xrtFree(sReadBack);
	xrtFree(sFilePath);
	xrtFree(sDirectory);
	return bValid ? 0 : 1;
}
