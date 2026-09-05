#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/whole —— 整文件操作：写/追加/原子替换/复制/移动
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFileWriteAll    整文件写入（覆盖）
 *   xrtFileAppend      追加
 *   xrtFileWriteAtomic 原子替换（临时文件+改名，读者不见半文件）
 *   xrtFileCopy / Move 复制 / 移动（跨盘自动降级复制删源）
 *   xrtFileReadAll     整文件读入拥有式缓冲
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/whole/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   published
 *
 * WriteAtomic 是配置/状态持久化的关键原语：写临时文件、
 *   fsync、rename 三步——崩溃任意时刻磁盘上要么旧文件
 *   要么完整新文件。输出 "published" 证明 Append 的
 *   " second" 被原子替换覆盖。
 */


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
