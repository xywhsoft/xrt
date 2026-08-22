#include "../test_allocator.h"



/* 验证两个分配型便捷函数在 OOM 下返回空指针并保留结构化内存错误。 */
int main(void)
{
	size_t iSize = 77;

	testRequire(testInstallFailAllocator(), "Base64 failure allocator install failed");
	testRequire(xrtBase64EncodeNew("foo", 3, NULL) == NULL,
		"Base64 allocated encode should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Base64 allocated encode OOM error mismatch");
	xrtClearError();
	testRequire(xrtBase64DecodeNew("Zm9v", 4, &iSize, NULL) == NULL &&
		(iSize == 77), "Base64 allocated decode should fail atomically under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Base64 allocated decode OOM error mismatch");
	printf("[PASS] codec_base64_oom\n");
	return 0;
}
