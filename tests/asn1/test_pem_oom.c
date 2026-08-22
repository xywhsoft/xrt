#include "../test_allocator.h"



/* 验证 PEM 分配型编码和解码在 OOM 下失败且不发布长度。 */
int main(void)
{
	static const char Text[] =
		"-----BEGIN DATA-----\nTWFu\n-----END DATA-----\n";
	xpemcursor Cursor;
	xpemblock Block;
	size_t iSize = 77;

	testRequire(xrtPemInit(&Cursor, Text, sizeof(Text) - 1u) &&
		(xrtPemRead(&Cursor, &Block) == XPEM_BLOCK),
		"PEM OOM fixture parse failed");
	testRequire(testInstallFailAllocator(), "PEM failure allocator install failed");
	testRequire(xrtPemEncodeNew("DATA", "Man", 3) == NULL,
		"PEM allocated encode should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"PEM allocated encode OOM error mismatch");
	xrtClearError();
	testRequire((xrtPemDecodeNew(&Block, &iSize) == NULL) && (iSize == 77),
		"PEM allocated decode should fail atomically under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"PEM allocated decode OOM error mismatch");
	printf("[PASS] pem_oom\n");
	return 0;
}
