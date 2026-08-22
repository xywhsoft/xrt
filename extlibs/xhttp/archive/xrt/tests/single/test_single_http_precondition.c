#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留完整条件请求评估顺序。 */
int main(void)
{
	xhttprepresentation Current;
	xhttpfield Field = {
		XRT_STR_INIT("If-None-Match"),
		XRT_STR_INIT("W/\"v1\"")
	};

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	Current.HasETag = true;
	Current.ETag.Opaque = XRT_STR_LITERAL("v1");
	if ( xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"), &Field, 1, &Current
	) != XHTTP_PRECONDITION_NOT_MODIFIED ) {
		return 1;
	}
	if ( xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), &Field, 1, &Current
	) != XHTTP_PRECONDITION_FAILED ) {
		return 2;
	}
	return 0;
}
