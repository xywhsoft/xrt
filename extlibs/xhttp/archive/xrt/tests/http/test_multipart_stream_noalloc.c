#include "../test_allocator.h"



/* Multipart Reader 在分块、事件和结束路径上不得分配堆内存。 */
int main(void)
{
	static const char Body[] =
		"--b\r\n\r\nvalue\r\n--b--\r\n";
	xmultipartboundary Boundary;
	xmultipartreader Reader;
	xmultipartpart Part;
	xmultiparterrorinfo Error;
	xbytesview Data;
	size_t iConsumed;
	size_t iOffset = 0;
	size_t iData = 0;
	bool bDone = false;

	testRequire(testInstallFailAllocator(),
		"Multipart Reader failure allocator install failed");
	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("b"), &Boundary
	) && xrtMultipartReaderInit(
		&Reader, &Boundary, NULL
	), "Multipart Reader initialization allocated");
	while ( !bDone ) {
		xmultipartreadstatus Status =
			xrtMultipartReaderRead(
				&Reader,
				(xbytesview){
					(const uint8*)Body + iOffset,
					(sizeof(Body) - 1u) - iOffset
				}, true, &iConsumed,
				&Part, &Data, &Error
			);

		testRequire(Status != XMULTIPART_READ_ERROR,
			"Multipart Reader allocated or failed");
		iOffset += iConsumed;
		if ( Status == XMULTIPART_READ_DATA ) {
			iData += Data.Size;
		} else if ( Status == XMULTIPART_READ_DONE ) {
			bDone = true;
		}
	}
	testRequire((iData == 5) &&
		(iOffset == (sizeof(Body) - 1u)),
		"Multipart Reader noalloc result mismatch");
	printf("[PASS] multipart_stream_noalloc\n");
	return 0;
}
