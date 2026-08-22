#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* 通用认证解析、迭代和直接写出不得依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Digest realm=\"api\", Basic abc==")
		}
	};
	xhttpauthcursor Cursor;
	xhttpauth Auth;
	xhttpparam Param;
	char Output[64];
	size_t iOffset = 0;
	size_t iParam = 0;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP auth failure allocator install failed");
	testRequire(xrtHttpChallengeNext(
		Fields[0].Value, &iOffset, &Auth
	) == XHTTP_NEXT_ITEM,
		"HTTP auth challenge iteration allocated");
	testRequire(xrtHttpAuthParamNext(
		Auth.Data, &iParam, &Param
	) == XHTTP_NEXT_ITEM,
		"HTTP auth parameter iteration allocated");
	testRequire(xrtHttpAuthParse(
		XRT_STR_LITERAL("Bearer token"), &Auth
	), "HTTP auth credential parsing allocated");
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(xrtHttpFieldChallengeNext(
		Fields,
		1u,
		XRT_STR_LITERAL("WWW-Authenticate"),
		&Cursor,
		&Auth
	) == XHTTP_NEXT_ITEM,
		"HTTP auth field iteration allocated");
	testRequire(xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bearer"), XRT_STR_LITERAL("token"),
		Output, sizeof(Output), &iSize
	), "HTTP auth direct writer allocated");
	puts("[PASS] HTTP authentication no allocation");
	return 0;
}
