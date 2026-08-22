#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_MULTIPART
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证 multipart boundary 和完整正文解析主路径。 */
int main(void)
{
	static const char Body[] =
		"--x\r\nContent-Disposition: form-data; name=a\r\n\r\n1\r\n--x--\r\n";
	xmultipartboundary Boundary;
	xmultipartpart Part;
	size_t iOffset = 0;

	return xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("x"), &Boundary
	) && (xrtMultipartNext(
		(xbytesview){ (cbytes)Body, sizeof(Body) - 1u },
		&Boundary, &iOffset, &Part, NULL
	) == XHTTP_NEXT_ITEM) && (Part.Body.Size == 1u) ? 0 : 1;
}
