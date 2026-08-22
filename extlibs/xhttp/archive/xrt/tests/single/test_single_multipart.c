#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



int main(void)
{
	static const char Body[] =
		"--b\r\n\r\nvalue\r\n--b--\r\n";
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xmultipartpart Part;
	size_t iOffset = 0;

	return (xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("b"), &Boundary
	) && (xrtMultipartNext(
		(xbytesview){
			(const uint8*)Body, sizeof(Body) - 1u
		}, &Boundary, &iOffset, &Part, &Error
	) == XHTTP_NEXT_ITEM) &&
		(Part.Body.Size == 5)) ? 0 : 1;
}
