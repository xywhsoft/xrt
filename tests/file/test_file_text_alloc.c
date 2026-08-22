#include "../test.h"



#define TEST_FILE_TEXT_SIZE (128u * 1024u)



/* 记录正文级底层分配，忽略路径和堆尺寸类的短分配。 */
typedef struct test_text_allocator {
	size_t LargeAllocations;
	size_t LargeReallocations;
} test_text_allocator;



/* 转发分配并记录大块请求。 */
static ptr testTextAlloc(ptr pContext, size_t iSize)
{
	test_text_allocator* pState = (test_text_allocator*)pContext;

	if ( iSize >= TEST_FILE_TEXT_SIZE ) {
		pState->LargeAllocations++;
	}
	return malloc(iSize);
}



/* 转发重分配并记录大块请求。 */
static ptr testTextRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	test_text_allocator* pState = (test_text_allocator*)pContext;

	if ( iSize >= TEST_FILE_TEXT_SIZE ) {
		pState->LargeReallocations++;
	}
	return realloc(pMemory, iSize);
}



/* 转发释放。 */
static void testTextFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 在安装 XRT 分配器前用标准库创建稳定的大 UTF-8 样本。 */
static void testTextFixture(cstr sPath)
{
	unsigned char arrChunk[4096];
	FILE* pFile;
	size_t iWritten = 0;

	memset(arrChunk, 'a', sizeof(arrChunk));
	pFile = fopen(sPath, "wb");
	testRequire(pFile != NULL, "text allocation fixture open failed");
	while ( iWritten < TEST_FILE_TEXT_SIZE ) {
		testRequire(fwrite(arrChunk, 1u, sizeof(arrChunk), pFile) ==
			sizeof(arrChunk), "text allocation fixture write failed");
		iWritten += sizeof(arrChunk);
	}
	testRequire(fclose(pFile) == 0, "text allocation fixture close failed");
}



/* 合法 UTF-8 读取只拥有原始正文，直写不复制整份正文。 */
int main(void)
{
	static const char sPath[] = "xrt-file-text-allocation.tmp";
	test_text_allocator State;
	xallocator Allocator;
	str sText;
	size_t iSize;

	testTextFixture(sPath);
	memset(&State, 0, sizeof(State));
	Allocator.Context = &State;
	Allocator.Alloc = testTextAlloc;
	Allocator.Realloc = testTextRealloc;
	Allocator.Free = testTextFree;
	testRequire(xrtSetAllocator(&Allocator),
		"text allocation counter install failed");

	/* 先让路径转换和小尺寸堆进入稳定状态。 */
	testRequire(xrtFileExists(sPath), "text allocation fixture is missing");
	State.LargeAllocations = 0u;
	State.LargeReallocations = 0u;

	sText = xrtFileReadText(sPath, XENCODING_UTF8, XUTF_STRICT, &iSize);
	testRequire((sText != NULL) && (iSize == TEST_FILE_TEXT_SIZE) &&
		(sText[0] == 'a') && (sText[iSize - 1u] == 'a') &&
		(sText[iSize] == '\0'), "large UTF-8 direct read failed");
	testRequire((State.LargeAllocations == 1u) &&
		(State.LargeReallocations == 0u),
		"UTF-8 read allocated or copied the whole body more than once");

	State.LargeAllocations = 0u;
	State.LargeReallocations = 0u;
	testRequire(xrtFileWriteText(sPath, (xstrview){ sText, iSize },
		XENCODING_UTF8, XUTF_STRICT, false),
		"large UTF-8 direct write failed");
	testRequire((State.LargeAllocations == 0u) &&
		(State.LargeReallocations == 0u),
		"UTF-8 write copied the whole borrowed body");

	xrtFree(sText);
	testRequire(xrtFileDelete(sPath), "text allocation fixture cleanup failed");
	return 0;
}
