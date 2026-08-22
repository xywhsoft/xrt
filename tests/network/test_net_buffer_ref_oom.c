#include "../test_allocator.h"



/* 记录外部引用释放次数。 */
static void testNetRefRelease(ptr pContext, cbytes pData, size_t iSize)
{
	size_t* pCount = (size_t*)pContext;

	(void)pData;
	(void)iSize;
	(*pCount)++;
}



/* 引用块头分配失败时，缓冲不能取得外部数据所有权。 */
int main(void)
{
	xnetbuf Buffer;
	char sData[] = "borrowed";
	size_t iReleased = 0;

	testRequire(testInstallFailAllocator(),
		"reference OOM allocator install failed");
	testRequire(xrtNetBufInit(&Buffer, NULL),
		"reference OOM buffer init failed");
	testRequire(!xrtNetBufAppendRef(&Buffer, sData, strlen(sData),
		testNetRefRelease, &iReleased),
		"reference append unexpectedly survived OOM");
	testRequire((xrtNetBufSize(&Buffer) == 0) && (iReleased == 0),
		"failed reference append changed buffer or consumed ownership");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"reference append OOM error mismatch");
	xrtNetBufClear(&Buffer);
	testRequire(iReleased == 0,
		"unaccepted reference was released during clear");
	return 0;
}
