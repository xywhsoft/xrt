#include "../test_allocator.h"

#include <xrt/http_digest.h>



/* RFC 9530 单项直接写出和长度查询必须保持零分配。 */
int main(void)
{
	static const uint8 Digest[] = { 0u, 1u, 2u };
	char arrOutput[64];
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"HTTP digest writer failure allocator install failed"
	);
	testRequire(
		xrtHttpDigestWrite(
			XRT_STR_LITERAL("sha-256"),
			(xbytesview){ Digest, sizeof(Digest) },
			NULL, 0, &iSize
		) && xrtHttpDigestWrite(
			XRT_STR_LITERAL("sha-256"),
			(xbytesview){ Digest, sizeof(Digest) },
			arrOutput, sizeof(arrOutput), &iSize
		),
		"HTTP digest writer allocated"
	);
	printf("[PASS] http_digest_write_noalloc\n");
	return 0;
}
