#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端请求只读空值契约。 */
int main(void)
{
	return (xrtHttpServerRequestVersion(NULL) == 0) &&
		(xrtHttpServerRequestHeaderCount(NULL) == 0) &&
		(xrtHttpServerRequestHeaderData(NULL) == NULL) &&
		(xrtHttpServerRequestTrailerCount(NULL) == 0) &&
		(xrtHttpServerRequestTrailerData(NULL) == NULL) &&
		(xrtHttpServerRequestBodyBytes(NULL) == 0) &&
		!xrtHttpServerRequestAcceptsTrailers(NULL) ? 0 : 1;
}
