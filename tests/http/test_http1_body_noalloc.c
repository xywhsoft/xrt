#include "../test_allocator.h"



/* Body Plan、流式解码、trailer 和 chunk 写出必须完全不依赖堆分配。 */
int main(void)
{
	static const uint8 Wire[] =
		"4\r\nWiki\r\n5\r\npedia\r\n0\r\nDigest: ok\r\n\r\n";
	xhttpfield Trailers[1];
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1body Body;
	xbytesview Data;
	uint8 Output[64];
	size_t iOffset = 0;
	size_t iDecoded = 0;
	size_t iWritten;

	testRequire(testInstallFailAllocator(),
		"HTTP/1 Body failure allocator install failed");
	xrtHttp1BodyLimitsInit(&Limits);
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, Trailers, 1, &Limits
	), "HTTP/1 Body init allocated memory");
	while ( !xrtHttp1BodyDone(&Body) ) {
		size_t iConsumed = 0;
		xhttp1bodystatus Status = xrtHttp1BodyRead(
			&Body,
			(xbytesview){ Wire + iOffset, sizeof(Wire) - 1u - iOffset },
			false, &iConsumed, &Data, &Error
		);

		testRequire((Status == XHTTP1_BODY_DATA) ||
			(Status == XHTTP1_BODY_DONE),
			"HTTP/1 Body decode allocated memory");
		if ( Status == XHTTP1_BODY_DATA ) {
			memcpy(Output + iDecoded, Data.Data, Data.Size);
			iDecoded += Data.Size;
		}
		iOffset += iConsumed;
	}
	testRequire((iDecoded == 9) &&
		(memcmp(Output, "Wikipedia", 9) == 0) &&
		(Body.TrailerCount == 1),
		"HTTP/1 no-allocation decode result mismatch");
	testRequire(xrtHttp1ChunkWrite(
		(xbytesview){ (cbytes)"test", 4 },
		Output, sizeof(Output), &iWritten
	) && xrtHttp1ChunkEndWrite(
		Trailers, 1, Output + iWritten,
		sizeof(Output) - iWritten, &iOffset
	), "HTTP/1 Body writer allocated memory");
	printf("[PASS] http1_body_noalloc\n");
	return 0;
}
