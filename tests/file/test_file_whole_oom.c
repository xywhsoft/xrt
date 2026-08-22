#include "../test_allocator.h"



/* 使用系统文件接口写入不受故障分配器影响的测试夹具。 */
static void testWholeOomWrite(cstr sPath, cstr sData)
{
	FILE* pFile = fopen(sPath, "wb");
	size_t iSize = strlen(sData);

	testRequire(pFile != NULL, "whole-file OOM fixture open failed");
	testRequire(fwrite(sData, 1u, iSize, pFile) == iSize,
		"whole-file OOM fixture write failed");
	testRequire(fclose(pFile) == 0,
		"whole-file OOM fixture close failed");
}



/* 比较磁盘内容，确保失败操作没有发布部分目标。 */
static bool testWholeOomMatches(cstr sPath, cstr sExpected)
{
	unsigned char arrBuffer[64];
	FILE* pFile = fopen(sPath, "rb");
	size_t iExpected = strlen(sExpected);
	size_t iRead;

	if ( pFile == NULL ) {
		return false;
	}
	iRead = fread(arrBuffer, 1u, sizeof(arrBuffer), pFile);
	if ( fclose(pFile) != 0 ) {
		return false;
	}
	return (iRead == iExpected) &&
		(memcmp(arrBuffer, sExpected, iExpected) == 0);
}



/* 全分配失败必须保留输出原子性、旧目标和源文件。 */
int main(void)
{
	static const char sRead[] = "xrt-file-whole-oom-read.tmp";
	static const char sSource[] = "xrt-file-whole-oom-source.tmp";
	static const char sTarget[] = "xrt-file-whole-oom-target.tmp";
	size_t iSize = 99u;

	testWholeOomWrite(sRead, "data");
	testWholeOomWrite(sSource, "source");
	testWholeOomWrite(sTarget, "old");
	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");

	testRequire(xrtFileReadAll(sRead, &iSize) == NULL,
		"whole-file read survived forced OOM");
	testRequire((iSize == 0u) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"whole-file read OOM reported the wrong result");
	xrtClearError();

	testRequire(!xrtFileWriteAtomic(sTarget, XRT_BYTES_LITERAL("new")) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		testWholeOomMatches(sTarget, "old"),
		"atomic write OOM changed the old target");
	xrtClearError();

	testRequire(!xrtFileCopy(sSource, sTarget, true) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		testWholeOomMatches(sSource, "source") &&
		testWholeOomMatches(sTarget, "old"),
		"copy OOM changed the source or old target");
	xrtClearError();

	testRequire(remove(sRead) == 0,
		"whole-file OOM read fixture cleanup failed");
	testRequire(remove(sSource) == 0,
		"whole-file OOM source fixture cleanup failed");
	testRequire(remove(sTarget) == 0,
		"whole-file OOM target fixture cleanup failed");
	return 0;
}
