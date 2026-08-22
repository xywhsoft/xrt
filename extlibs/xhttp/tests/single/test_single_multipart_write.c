#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_MULTIPART_WRITE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证 multipart 字段可以直接写入调用方缓冲区。 */
int main(void)
{
	xmultipartboundary Boundary;
	char Output[256];
	size_t iSize = 0;

	return xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("x"), &Boundary
	) && xrtMultipartFieldWrite(
		&Boundary,
		XRT_STR_LITERAL("a"),
		(xbytesview){ (cbytes)"1", 1u },
		Output,
		sizeof(Output),
		&iSize
	) && (iSize != 0) ? 0 : 1;
}
