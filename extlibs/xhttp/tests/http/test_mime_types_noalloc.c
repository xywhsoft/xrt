#include "../test_allocator.h"



/* 扩展名和路径 MIME 查询必须保持零堆分配。 */
int main(void)
{
	xstrview Type;

	testRequire(testInstallFailAllocator(),
		"MIME types failure allocator install failed");
	Type = xrtMimeByExt(XRT_STR_LITERAL(".WOFF2"));
	testRequire((Type.Size == 10) &&
		(memcmp(Type.Data, "font/woff2", 10) == 0),
		"MIME extension lookup allocated");
	Type = xrtMimeByPath(
		XRT_STR_LITERAL("assets/app.bundle.JS")
	);
	testRequire((Type.Size == 30) &&
		(memcmp(
			Type.Data,
			"text/javascript; charset=utf-8",
			30
		) == 0),
		"MIME path lookup allocated");
	testRequire(strcmp(
		xrtMime("assets/unknown.custom"),
		"application/octet-stream"
	) == 0, "MIME fallback allocated");
	printf("[PASS] mime_types_noalloc\n");
	return 0;
}

