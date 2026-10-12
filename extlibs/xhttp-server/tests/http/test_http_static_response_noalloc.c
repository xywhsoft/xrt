#include "../test_allocator.h"



/* 静态响应的测量和字段构建不得依赖堆分配。 */
int main(void)
{
	xhttprepresentation Current;
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttpbyterange Range = { 5, 14 };
	xhttpstaticresponse Response;
	char Workspace[128];
	size_t iRequired;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	Current.HasETag = true;
	Current.ETag.Opaque = XRT_STR_LITERAL("asset");
	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Plan.SendBody = true;
	Plan.AcceptRanges = true;
	Plan.CompleteLength = 100;
	Plan.SelectedLength = 10;
	Plan.RangeCount = 1;
	xrtHttpStaticResponseConfigInit(&Config);
	Config.ContentType = XRT_STR_LITERAL("image/png");

	testRequire(testInstallFailAllocator(),
		"HTTP static response failure allocator install failed");
	testRequire(xrtHttpStaticResponseBuild(
		&Plan,
		&Range,
		&Current,
		&Config,
		NULL,
		0,
		&iRequired,
		NULL
	) && (iRequired <= sizeof(Workspace)),
		"HTTP static response measure allocated");
	testRequire(xrtHttpStaticResponseBuild(
		&Plan,
		&Range,
		&Current,
		&Config,
		Workspace,
		sizeof(Workspace),
		&iRequired,
		&Response
	) && (Response.FieldCount == 5),
		"HTTP static response build allocated");
	printf("[PASS] http_static_response_noalloc\n");
	return 0;
}
