#include "../test_allocator.h"

#include <xrt/http_trailer.h>



/* Trailer 校验、查询和缓冲写入路径不得分配内存。 */
int main(void)
{
	static const xhttpfield Headers[] = {
		{
			XRT_STR_INIT("Trailer"),
			XRT_STR_INIT("Digest")
		},
		{
			XRT_STR_INIT("Trailer"),
			XRT_STR_INIT("X-Meta")
		}
	};
	static const xhttpfield Trailers[] = {
		{
			XRT_STR_INIT("Digest"),
			XRT_STR_INIT("one")
		},
		{
			XRT_STR_INIT("X-Meta"),
			XRT_STR_INIT("two")
		}
	};
	char Output[32];
	size_t iNames;
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"HTTP Trailer failure allocator install failed"
	);
	testRequire(
		xrtHttpTrailerCount(Headers, 2u, &iNames) &&
		(iNames == 2u) &&
		(xrtHttpTrailerFind(
			Headers, 2u, XRT_STR_LITERAL("digest")
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpTrailerSectionValid(Trailers, 2u) &&
		xrtHttpTrailerNamesWrite(
			Trailers, 2u, Output, sizeof(Output), &iSize
		) && (iSize == 14u),
		"HTTP Trailer no-allocation path allocated or failed"
	);
	printf("[PASS] http_trailer_noalloc\n");
	return 0;
}
