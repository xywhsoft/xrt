#include "../test_allocator.h"



/* Range 与 Content-Range 便捷构建失败必须保留内存不足错误。 */
int main(void)
{
	static const xhttprangespec Spec = {
		XHTTP_RANGE_SPEC_CLOSED,
		UINT64_C(10),
		UINT64_C(19)
	};
	static const xhttpcontentrange Content = {
		true,
		true,
		UINT64_C(10),
		UINT64_C(19),
		UINT64_C(100)
	};

	testRequire(testInstallFailAllocator(),
		"HTTP Range OOM allocator install failed");
	testRequire((xrtHttpRangeBuild(
		&Spec, 1, NULL
	) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Range build did not publish OOM");
	xrtClearError();
	testRequire((xrtHttpContentRangeBuild(
		&Content, NULL
	) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Content-Range build did not publish OOM");
	printf("[PASS] http_range_oom\n");
	return 0;
}
