#include "../test_allocator.h"

#include <xrt/http_encoding.h>



/* 验证解析、查询和选择不触发任何堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip, identity")
		},
		{
			XRT_STR_INIT("content-encoding"),
			XRT_STR_INIT("deflate")
		}
	};
	xhttpacceptencoding Accept;
	xhttpcontentencodingplan Plan;
	xhttpcontentencodingcursor Cursor;
	xhttpcontentencodingitem Item;
	char Output[32];
	size_t iSize;
	bool bPass;

	testRequire(
		testInstallFailAllocator(),
		"HTTP encoding failure allocator install failed"
	);
	xrtHttpAcceptEncodingInit(&Accept);
	xrtHttpContentEncodingCursorInit(&Cursor);
	bPass = xrtHttpAcceptEncodingAdd(
		&Accept,
		XRT_STR_LITERAL(
			"gzip;q=0.8, deflate;q=1, identity;q=0"
		)
	) && (xrtHttpAcceptEncodingSelect(
		&Accept,
		XHTTP_CODING_IDENTITY |
			XHTTP_CODING_GZIP |
			XHTTP_CODING_DEFLATE,
		XHTTP_CODING_GZIP
	) == XHTTP_CODING_DEFLATE) &&
	xrtHttpContentEncodingPlan(
		Fields, 2, &Plan
	) &&
	(Plan.CodingCount == 3) &&
	xrtHttpContentEncodingWrite(
		Fields, 2, Output, sizeof(Output), &iSize
	) &&
	(iSize == 23) &&
	(xrtHttpContentEncodingNext(
		Fields, 2, &Cursor, &Item
	) == XHTTP_NEXT_ITEM) &&
	(Item.Coding == XHTTP_CODING_GZIP);
	testRequire(
		bPass,
		"HTTP encoding negotiation allocated memory"
	);
	printf("[PASS] http_encoding_noalloc\n");
	return 0;
}
