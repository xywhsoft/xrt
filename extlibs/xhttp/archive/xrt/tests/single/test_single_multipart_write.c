#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



int main(void)
{
	xmultipartboundary Boundary;
	uint8 Output[128];
	size_t iSize;

	return (xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("single"), &Boundary
	) && xrtMultipartFieldWrite(
		&Boundary,
		XRT_STR_LITERAL("field"),
		(xbytesview){ (const uint8*)"value", 5u },
		Output, sizeof(Output), &iSize
	) && (iSize != 0)) ? 0 : 1;
}
