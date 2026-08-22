#include <stdio.h>

#include <xrt.h>



/* 展示整文件写入、追加、原子替换、复制和移动。 */
int main(void)
{
	static const char sSource[] = "xrt-file-whole-source.tmp";
	static const char sCopy[] = "xrt-file-whole-copy.tmp";
	static const char sMoved[] = "xrt-file-whole-moved.tmp";
	bytes pData;
	size_t iSize;
	bool bResult = false;

	(void)xrtFileDelete(sSource);
	(void)xrtFileDelete(sCopy);
	(void)xrtFileDelete(sMoved);
	xrtClearError();
	if ( !xrtFileWriteAll(sSource, XRT_BYTES_LITERAL("first")) ||
		 !xrtFileAppend(sSource, XRT_BYTES_LITERAL(" second")) ||
		 !xrtFileWriteAtomic(sSource, XRT_BYTES_LITERAL("published")) ||
		 !xrtFileCopy(sSource, sCopy, false) ||
		 !xrtFileMove(sCopy, sMoved, false) ) {
		goto cleanup;
	}
	pData = xrtFileReadAll(sMoved, &iSize);
	if ( pData == NULL ) {
		goto cleanup;
	}
	printf("%.*s\n", (int)iSize, (const char*)pData);
	xrtFree(pData);
	bResult = true;

cleanup:
	if ( xrtFileExists(sSource) && !xrtFileDelete(sSource) ) {
		bResult = false;
	}
	if ( xrtFileExists(sCopy) && !xrtFileDelete(sCopy) ) {
		bResult = false;
	}
	if ( xrtFileExists(sMoved) && !xrtFileDelete(sMoved) ) {
		bResult = false;
	}
	return bResult ? 0 : 1;
}
