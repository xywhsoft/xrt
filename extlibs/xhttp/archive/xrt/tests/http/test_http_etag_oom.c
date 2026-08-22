#include "../test_allocator.h"



/* 实体标签便捷构建失败必须保留内存不足错误。 */
int main(void)
{
	static const xhttpetag Tag = {
		XRT_STR_INIT("revision-7"),
		false
	};

	testRequire(testInstallFailAllocator(),
		"HTTP entity-tag OOM allocator install failed");
	testRequire((xrtHttpETagBuild(&Tag, NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP entity-tag build did not publish OOM");
	printf("[PASS] http_etag_oom\n");
	return 0;
}
