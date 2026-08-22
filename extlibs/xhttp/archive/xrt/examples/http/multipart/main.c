#include <xrt.h>

#include <stdio.h>



/* 逐项读取 multipart/form-data 正文。 */
int main(void)
{
	static const char Body[] =
		"--demo-boundary\r\n"
		"Content-Disposition: form-data; name=\"message\"\r\n"
		"\r\n"
		"hello\r\n"
		"--demo-boundary--\r\n";
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xmultipartpart Part;
	char Name[32];
	size_t iOffset = 0;
	size_t iSize;

	if ( !xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("demo-boundary"), &Boundary
	) ) {
		return 1;
	}
	while ( xrtMultipartNext(
		(xbytesview){
			(const uint8*)Body, sizeof(Body) - 1u
		}, &Boundary, &iOffset, &Part, &Error
	) == XHTTP_NEXT_ITEM ) {
		if ( xrtMultipartPartNameWrite(
			&Part, Name, sizeof(Name), &iSize
		) ) {
			printf("%.*s = %.*s\n",
				(int)iSize, Name,
				(int)Part.Body.Size, (cstr)Part.Body.Data);
		}
	}
	return (iOffset == (sizeof(Body) - 1u)) ? 0 : 1;
}
