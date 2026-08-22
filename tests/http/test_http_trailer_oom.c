#include "../test_allocator.h"

#include <xrt/http_trailer.h>



/* Trailer Build 必须传播分配失败且不修改长度输出。 */
int main(void)
{
	static const xhttpfield Trailer = {
		XRT_STR_INIT("Digest"),
		XRT_STR_INIT("value")
	};
	size_t iSize = 71u;

	testRequire(
		testInstallFailAllocator(),
		"HTTP Trailer Build failure allocator install failed"
	);
	testRequire(
		(xrtHttpTrailerNamesBuild(
			&Trailer, 1u, &iSize
		) == NULL) &&
		(iSize == 71u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Trailer Build did not propagate OOM"
	);
	printf("[PASS] http_trailer_oom\n");
	return 0;
}
