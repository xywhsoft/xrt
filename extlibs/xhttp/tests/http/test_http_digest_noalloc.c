#include "../test_allocator.h"

#include <xrt/http_digest.h>



/* RFC 9530 字段预校验、去重迭代与解码必须保持零分配。 */
int main(void)
{
	xhttpdigestcursor Cursor;
	xhttpdigest Digest;
	uint8 arrOutput[64];
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"HTTP digest failure allocator install failed"
	);
	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		(xrtHttpDigestNext(
			XRT_STR_LITERAL(
				"sha-256=:AA==:, sha-512=:AQI=:"
			), &Cursor, &Digest
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpDigestRead(
			&Digest, arrOutput, sizeof(arrOutput), &iSize
		),
		"HTTP digest parser allocated"
	);
	printf("[PASS] http_digest_noalloc\n");
	return 0;
}
