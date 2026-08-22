#include "../test_allocator.h"



/* 验证调用方缓冲路径在全局分配器失效时仍然完全可用。 */
int main(void)
{
	char Text[32];
	uint8 Data[16];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"percent failure allocator install failed");
	testRequire(xrtPercentEncode(
		"a b", 3, XRT_STR_LITERAL(""), Text, sizeof(Text), &iSize
	) && (strcmp(Text, "a%20b") == 0),
		"percent direct encode unexpectedly allocated");
	testRequire(xrtPercentWrite(
		"a b", 3, XRT_STR_LITERAL(""), Text, 5, &iSize
	) && (iSize == 5) && (memcmp(Text, "a%20b", 5) == 0),
		"percent fragment write unexpectedly allocated");
	testRequire(xrtPercentDecode(
		(xstrview){ Text, iSize }, Data, sizeof(Data), &iSize
	) && (iSize == 3) && (memcmp(Data, "a b", 3) == 0),
		"percent direct decode unexpectedly allocated");
	printf("[PASS] codec_percent_noalloc\n");
	return 0;
}
