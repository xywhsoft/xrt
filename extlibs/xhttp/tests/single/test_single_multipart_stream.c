#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_MULTIPART_STREAM
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证无分配 multipart 流 Reader 可以初始化并复用。 */
int main(void)
{
	xmultipartboundary Boundary;
	xmultipartreader Reader;

	if ( !xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("x"), &Boundary
	) || !xrtMultipartReaderInit(&Reader, &Boundary, NULL) ) {
		return 1;
	}
	xrtMultipartReaderReset(&Reader);
	return xrtMultipartReaderDone(&Reader) ? 2 : 0;
}
