#include "../test_allocator.h"



/* 媒体类型解析、查找与直接写出必须保持零堆分配。 */
int main(void)
{
	xmediatype Type;
	xhttpparam Param;
	char Output[128];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"MIME failure allocator install failed");
	testRequire(xrtHttpMediaTypeParse(
		XRT_STR_LITERAL("application/json; charset=UTF-8"), &Type
	), "MIME media type parse allocated");
	testRequire(xrtHttpMediaTypeParam(
		&Type, XRT_STR_LITERAL("charset"), &Param
	) == XHTTP_NEXT_ITEM, "MIME parameter lookup allocated");
	testRequire(xrtHttpMediaTypeCompressible(&Type),
		"MIME compressibility check allocated");
	testRequire(!xrtHttpContentTypeCompressible(
		XRT_STR_LITERAL("font/woff2")
	), "MIME Content-Type compressibility check allocated");
	testRequire(xrtHttpMediaTypeWrite(
		&Type, Output, sizeof(Output), &iSize
	), "MIME media type write allocated");
	printf("[PASS] mime_type_noalloc\n");
	return 0;
}
