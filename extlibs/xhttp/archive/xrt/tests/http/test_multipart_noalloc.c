#include "../test_allocator.h"



/* Multipart boundary、整包解析和字段读取必须保持零堆分配。 */
int main(void)
{
	static const char Body[] =
		"--b\r\n"
		"Content-Disposition: form-data; name=\"field\"\r\n"
		"\r\n"
		"value\r\n"
		"--b--\r\n";
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xmultipartpart Part;
	char Name[16];
	size_t iOffset = 0;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Multipart failure allocator install failed");
	testRequire(xrtMultipartBoundaryFromContentType(
		XRT_STR_LITERAL("multipart/form-data; boundary=b"),
		&Boundary
	), "Multipart boundary parse allocated");
	testRequire(xrtMultipartNext(
		(xbytesview){
			(const uint8*)Body, sizeof(Body) - 1u
		}, &Boundary, &iOffset, &Part, &Error
	) == XHTTP_NEXT_ITEM, "Multipart Part parse allocated");
	testRequire(xrtMultipartPartNameWrite(
		&Part, Name, sizeof(Name), &iSize
	) && (iSize == 5), "Multipart name read allocated");
	testRequire(xrtMultipartNext(
		(xbytesview){
			(const uint8*)Body, sizeof(Body) - 1u
		}, &Boundary, &iOffset, &Part, &Error
	) == XHTTP_NEXT_END, "Multipart close parse allocated");
	printf("[PASS] multipart_noalloc\n");
	return 0;
}
