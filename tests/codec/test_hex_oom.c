#include "../test_allocator.h"



/* 验证 HEX 基础路径零分配，便捷路径正确报告 OOM。 */
int main(void)
{
	char arrText[8];
	uint8 arrData[4];
	size_t iSize;

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtHexEncode("ab", 2, arrText, sizeof(arrText), &iSize, 0) &&
		(strcmp(arrText, "6162") == 0), "HEX base encode allocated memory");
	testRequire(xrtHexDecode(XRT_STR_LITERAL("6162"), arrData,
		sizeof(arrData), &iSize, 0) && (memcmp(arrData, "ab", 2) == 0),
		"HEX base decode allocated memory");
	testRequire(xrtHexEncodeNew("ab", 2, 0) == NULL,
		"HEX allocated encode should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"HEX encode OOM error mismatch");
	xrtClearError();
	testRequire(xrtHexDecodeNew(XRT_STR_LITERAL("6162"), &iSize, 0) == NULL,
		"HEX allocated decode should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"HEX decode OOM error mismatch");
	xrtClearError();
	printf("[PASS] codec-hex-oom\n");
	return 0;
}
