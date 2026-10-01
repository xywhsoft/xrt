#include "../test_allocator.h"



/* 验证三个数字展示分配便捷层完整传播内存不足。 */
int main(void)
{
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtIntFormat(42, XRT_STR_LITERAL("d")) == NULL,
		"signed integer format should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"signed integer format OOM mismatch");
	xrtClearError();
	size_t iSize = SIZE_MAX;
	testRequire(!xrtIntFormatSized(0, XRT_STR_LITERAL("c"), &iSize) && iSize == 0 &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY, "sized integer OOM contract");
	xrtClearError();
	iSize = SIZE_MAX;
	testRequire(!xrtUIntFormatSized(UINT64_MAX, XRT_STR_LITERAL("X"), &iSize) && iSize == 0 &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY, "sized unsigned OOM contract");
	xrtClearError();
	iSize = SIZE_MAX;
	testRequire(!xrtNumFormatSized(3.14, XRT_STR_LITERAL(".2f"), &iSize) && iSize == 0 &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY, "sized float OOM contract");
	xrtClearError();
	testRequire(xrtIntFormat(65, XRT_STR_LITERAL("c")) == NULL,
		"character format should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"character format OOM mismatch");
	xrtClearError();
	testRequire(xrtUIntFormat(42, XRT_STR_LITERAL("d")) == NULL,
		"unsigned integer format should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"unsigned integer format OOM mismatch");
	xrtClearError();
	testRequire(xrtNumFormat(3.14, XRT_STR_LITERAL(".2f")) == NULL,
		"floating-point format should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"floating-point format OOM mismatch");
	xrtClearError();
	printf("[PASS] number-format-oom\n");
	return 0;
}
