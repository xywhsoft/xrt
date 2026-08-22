#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_MULTIPART_RANDOM
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证安全随机 boundary 生成路径。 */
int main(void)
{
	xmultipartboundary Boundary;

	return xrtMultipartBoundaryRandom(&Boundary) &&
		(Boundary.Size != 0) ? 0 : 1;
}
