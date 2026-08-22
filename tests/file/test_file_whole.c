#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../test.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <unistd.h>
#endif



/* 流式复制数据大于内部缓冲，避免测试退化成单次读写。 */
#define TEST_FILE_LARGE_SIZE (192u * 1024u + 37u)



#if defined(_WIN32) || defined(_WIN64)
	#define TEST_WHOLE_ATTRIBUTE_HIDDEN 0x02u
#endif



/* 在系统临时目录构造测试路径。 */
static str testWholePath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "whole-file test path allocation failed");
	return sPath;
}



/* 读取结果必须拥有内存、保留二进制零字节并额外补零。 */
static void testWholeReadWrite(cstr sPath)
{
	static const unsigned char arrFirst[] = { 1, 0, 2, 0, 3 };
	static const unsigned char arrTail[] = { 4, 0, 5 };
	bytes pData;
	size_t iSize;

	testRequire(xrtFileWriteAll(sPath,
		(xbytesview){ arrFirst, sizeof(arrFirst) }),
		"whole-file write failed");
	testRequire(xrtFileAppend(sPath,
		(xbytesview){ arrTail, sizeof(arrTail) }),
		"whole-file append failed");
	pData = xrtFileReadAll(sPath, &iSize);
	testRequire((pData != NULL) &&
		(iSize == (sizeof(arrFirst) + sizeof(arrTail))) &&
		(memcmp(pData, arrFirst, sizeof(arrFirst)) == 0) &&
		(memcmp(pData + sizeof(arrFirst), arrTail, sizeof(arrTail)) == 0) &&
		(pData[iSize] == 0u), "whole-file binary read is incorrect");
	xrtFree(pData);

	testRequire(xrtFileWriteAll(sPath, (xbytesview){ NULL, 0 }),
		"empty whole-file write failed");
	pData = xrtFileReadAll(sPath, &iSize);
	testRequire((pData != NULL) && (iSize == 0u) && (pData[0] == 0u),
		"empty file did not return an owned zero-terminated buffer");
	xrtFree(pData);
}



/* 受限整文件读取必须在分配前拒绝已知超限文件，并允许零上限空文件。 */
static void testWholeReadLimit(cstr sPath)
{
	static const unsigned char arrData[] = { 1, 2, 3, 4, 5 };
	bytes pData;
	size_t iSize = 99u;

	testRequire(xrtFileWriteAll(sPath,
		(xbytesview){ arrData, sizeof(arrData) }),
		"read-limit fixture write failed");
	testRequire(xrtFileReadAllLimit(sPath, sizeof(arrData) - 1u,
		&iSize) == NULL, "read limit accepted an oversized file");
	testRequire((iSize == 0u) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_LIMIT),
		"read limit reported the wrong failure");
	xrtClearError();
	pData = xrtFileReadAllLimit(sPath, sizeof(arrData), &iSize);
	testRequire((pData != NULL) && (iSize == sizeof(arrData)) &&
		(memcmp(pData, arrData, sizeof(arrData)) == 0) &&
		(pData[iSize] == 0u), "exact read limit failed");
	xrtFree(pData);

	testRequire(xrtFileWriteAll(sPath, (xbytesview){ NULL, 0 }),
		"empty read-limit fixture write failed");
	pData = xrtFileReadAllLimit(sPath, 0u, &iSize);
	testRequire((pData != NULL) && (iSize == 0u) && (pData[0] == 0u),
		"zero read limit rejected an empty file");
	xrtFree(pData);
}



/* 原子写必须替换完整旧内容，零长度也必须成功。 */
static void testWholeAtomic(cstr sPath)
{
	static const unsigned char arrOld[] = "old-content";
	static const unsigned char arrNew[] = "new";
	xfileinfo Info;
	bytes pData;
	size_t iSize;

	testRequire(xrtFileWriteAll(sPath,
		(xbytesview){ arrOld, sizeof(arrOld) - 1u }),
		"atomic fixture write failed");
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(xrtPathStat(sPath, false, &Info) &&
			xrtPathSetAttributes(sPath,
				Info.Attributes | TEST_WHOLE_ATTRIBUTE_HIDDEN),
			"atomic Windows attribute fixture setup failed");
	#else
		testRequire(xrtPathSetMode(sPath, true, 0640u),
			"atomic POSIX mode fixture setup failed");
	#endif
	testRequire(xrtFileWriteAtomic(sPath,
		(xbytesview){ arrNew, sizeof(arrNew) - 1u }),
		"atomic replacement failed");
	testRequire(xrtPathStat(sPath, true, &Info),
		"atomic replacement metadata query failed");
	#if defined(_WIN32) || defined(_WIN64)
		testRequire((Info.Attributes & TEST_WHOLE_ATTRIBUTE_HIDDEN) != 0u,
			"atomic replacement lost Windows target attributes");
	#else
		testRequire((Info.Mode & 0777u) == 0640u,
			"atomic replacement changed the POSIX target mode");
	#endif
	pData = xrtFileReadAll(sPath, &iSize);
	testRequire((pData != NULL) && (iSize == (sizeof(arrNew) - 1u)) &&
		(memcmp(pData, arrNew, iSize) == 0),
		"atomic replacement exposed incorrect content");
	xrtFree(pData);

	testRequire(xrtFileWriteAtomic(sPath, (xbytesview){ NULL, 0 }),
		"atomic empty replacement failed");
	pData = xrtFileReadAll(sPath, &iSize);
	testRequire((pData != NULL) && (iSize == 0u),
		"atomic empty replacement has the wrong size");
	xrtFree(pData);
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(xrtPathSetAttributes(sPath,
			Info.Attributes & ~TEST_WHOLE_ATTRIBUTE_HIDDEN),
			"atomic Windows attribute fixture restore failed");
	#endif
}



/* 复制必须流式处理、保护现有目标并拒绝同一文件。 */
static void testWholeCopy(cstr sSource, cstr sTarget)
{
	bytes pFixture = (bytes)xrtMalloc(TEST_FILE_LARGE_SIZE);
	bytes pCopied;
	size_t iSize;
	size_t i;

	testRequire(pFixture != NULL, "large copy fixture allocation failed");
	for ( i = 0; i < TEST_FILE_LARGE_SIZE; i++ ) {
		pFixture[i] = (unsigned char)((i * 29u) & 0xFFu);
	}
	testRequire(xrtFileWriteAll(sSource,
		(xbytesview){ pFixture, TEST_FILE_LARGE_SIZE }),
		"large copy fixture write failed");
	(void)xrtFileDelete(sTarget);
	xrtClearError();
	testRequire(xrtFileCopy(sSource, sTarget, false),
		"streaming file copy failed");
	pCopied = xrtFileReadAll(sTarget, &iSize);
	testRequire((pCopied != NULL) && (iSize == TEST_FILE_LARGE_SIZE) &&
		(memcmp(pCopied, pFixture, iSize) == 0),
		"streaming copy content mismatch");
	xrtFree(pCopied);

	testRequire(!xrtFileCopy(sSource, sTarget, false),
		"non-replacing copy overwrote an existing target");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
		"copy collision reported the wrong error");
	xrtClearError();
	testRequire(!xrtFileCopy(sSource, sSource, true),
		"copy accepted identical source and target");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"same-file copy reported the wrong error");
	xrtClearError();

	pFixture[0] ^= 0xFFu;
	testRequire(xrtFileWriteAll(sSource,
		(xbytesview){ pFixture, TEST_FILE_LARGE_SIZE }),
		"replacement copy fixture write failed");
	testRequire(xrtFileCopy(sSource, sTarget, true),
		"replacing copy failed");
	pCopied = xrtFileReadAll(sTarget, &iSize);
	testRequire((pCopied != NULL) && (iSize == TEST_FILE_LARGE_SIZE) &&
		(memcmp(pCopied, pFixture, iSize) == 0),
		"replacement copy content mismatch");
	xrtFree(pCopied);
	xrtFree(pFixture);
}



/* 同卷移动必须直接改名，并保持目标内容。 */
static void testWholeMove(cstr sSource, cstr sTarget)
{
	static const unsigned char arrData[] = "move-data";
	bytes pData;
	size_t iSize;

	testRequire(xrtFileWriteAll(sSource,
		(xbytesview){ arrData, sizeof(arrData) - 1u }),
		"move fixture write failed");
	(void)xrtFileDelete(sTarget);
	xrtClearError();
	testRequire(xrtFileMove(sSource, sTarget, false),
		"same-volume move failed");
	testRequire(!xrtPathExists(sSource) && xrtFileExists(sTarget),
		"move left the source path behind");
	pData = xrtFileReadAll(sTarget, &iSize);
	testRequire((pData != NULL) && (iSize == (sizeof(arrData) - 1u)) &&
		(memcmp(pData, arrData, iSize) == 0), "moved file content mismatch");
	xrtFree(pData);
}



/* 文件移动必须拒绝目录、链接和同一对象，不能因是否跨卷而改变对象类别。 */
static void testWholeMoveTypes(cstr sSource, cstr sTarget)
{
	str sDirectory = xrtPathTemp();

	testRequire(sDirectory != NULL,
		"move type test temporary directory query failed");
	testRequire(!xrtFileMove(sSource, sDirectory, true) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE) &&
		xrtFileExists(sSource),
		"move accepted a directory target");
	xrtClearError();
	testRequire(!xrtFileMove(sDirectory, sTarget, true) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE) &&
		xrtDirExists(sDirectory),
		"move accepted a directory source");
	xrtClearError();
	testRequire(!xrtFileMove(sSource, sSource, true) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		xrtFileExists(sSource),
		"move accepted identical source and target files");
	xrtClearError();
	xrtFree(sDirectory);
}



#if !defined(_WIN32) && !defined(_WIN64)

/* POSIX 普通文件 helper 必须拒绝源链接和目标链接，不得静默改为另一种语义。 */
static void testWholeLinks(cstr sSource, cstr sTarget)
{
	str sLink = testWholePath("xrt-file-whole-link.tmp");
	xfileinfo Info;

	(void)xrtFileDelete(sLink);
	xrtClearError();
	testRequire(symlink(sTarget, sLink) == 0,
		"whole-file target link creation failed");
	testRequire(!xrtFileCopy(sSource, sLink, true) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE) &&
		xrtPathStat(sLink, false, &Info) &&
		(Info.Type == XFILE_TYPE_LINK),
		"copy replaced a symbolic-link target");
	xrtClearError();
	testRequire(!xrtFileMove(sSource, sLink, true) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE) &&
		xrtFileExists(sSource),
		"move replaced a symbolic-link target");
	xrtClearError();
	testRequire(xrtFileDelete(sLink),
		"whole-file target link cleanup failed");
	testRequire(symlink(sSource, sLink) == 0,
		"whole-file source link creation failed");
	testRequire(!xrtFileCopy(sLink, sTarget, true) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE) &&
		xrtPathStat(sLink, false, &Info) &&
		(Info.Type == XFILE_TYPE_LINK),
		"copy followed a symbolic-link source");
	xrtClearError();
	testRequire(!xrtFileMove(sLink, sTarget, true) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE) &&
		xrtPathExists(sLink),
		"move accepted a symbolic-link source");
	xrtClearError();
	testRequire(xrtFileDelete(sLink),
		"whole-file source link cleanup failed");
	xrtFree(sLink);
}

#endif



/* 整文件 Helper 回归入口。 */
int main(void)
{
	str sFirst = testWholePath("xrt-file-whole-first.tmp");
	str sSecond = testWholePath("xrt-file-whole-second.tmp");

	(void)xrtFileDelete(sFirst);
	(void)xrtFileDelete(sSecond);
	xrtClearError();
	testWholeReadWrite(sFirst);
	testWholeReadLimit(sFirst);
	testWholeAtomic(sFirst);
	testWholeCopy(sFirst, sSecond);
	testWholeMoveTypes(sFirst, sSecond);
	#if !defined(_WIN32) && !defined(_WIN64)
		testWholeLinks(sFirst, sSecond);
	#endif
	testWholeMove(sFirst, sSecond);
	testRequire(xrtFileDelete(sSecond), "whole-file final cleanup failed");
	xrtFree(sFirst);
	xrtFree(sSecond);
	return 0;
}
