#include <xrt.h>

#include <stdio.h>



/* 直接从分块输入读取 Part 和正文，不为 Reader 配置私有正文缓冲区。 */
int main(void)
{
	static const char Body[] =
		"--demo\r\n"
		"Content-Disposition: form-data; name=\"message\"\r\n"
		"\r\n"
		"hello\r\n"
		"--demo--\r\n";
	xmultipartboundary Boundary;
	xmultipartreader Reader;
	xmultipartpart Part;
	xmultiparterrorinfo Error;
	xbytesview Data;
	size_t iOffset = 0;

	if ( !xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("demo"), &Boundary
	) || !xrtMultipartReaderInit(
		&Reader, &Boundary, NULL
	) ) {
		return 1;
	}
	while ( !xrtMultipartReaderDone(&Reader) ) {
		xmultipartreadstatus Status;
		size_t iConsumed;

		Status = xrtMultipartReaderRead(
			&Reader,
			(xbytesview){
				(const uint8*)Body + iOffset,
				(sizeof(Body) - 1u) - iOffset
			}, true, &iConsumed,
			&Part, &Data, &Error
		);
		if ( Status == XMULTIPART_READ_ERROR ) {
			return 1;
		}
		iOffset += iConsumed;
		if ( Status == XMULTIPART_READ_DATA ) {
			printf("%.*s", (int)Data.Size, (cstr)Data.Data);
		}
	}
	return 0;
}
