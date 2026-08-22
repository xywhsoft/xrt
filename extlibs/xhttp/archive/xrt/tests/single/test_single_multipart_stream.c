#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



int main(void)
{
	xmultipartboundary Boundary;
	xmultipartreader Reader;
	xmultipartpart Part;
	xmultiparterrorinfo Error;
	xbytesview Data;
	size_t iConsumed;

	return (xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("b"), &Boundary
	) && xrtMultipartReaderInit(
		&Reader, &Boundary, NULL
	) && (xrtMultipartReaderRead(
		&Reader,
		(xbytesview){
			(const uint8*)"--b\r\n\r\n", 7u
		}, false, &iConsumed, &Part, &Data, &Error
	) == XMULTIPART_READ_PART)) ? 0 : 1;
}
