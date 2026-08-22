#include "../test_allocator.h"

#include <xrt/http_retry.h>



/* 验证解析、字段读取和退避计算在失败分配器下仍可完成。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Retry-After"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	xhttpretryafter Retry;
	char Text[30];
	uint64 iDelay;
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"HTTP retry failure allocator install failed"
	);
	testRequire(
		xrtHttpRetryAfterParse(Fields[0].Value, &Retry) &&
		(xrtHttpRetryAfterFields(
			Fields,
			1,
			INT64_C(784111776000000),
			&iDelay
		 ) == XHTTP_NEXT_ITEM) &&
		(iDelay == UINT64_C(1000000)) &&
		xrtHttpRetryAfterWrite(
			&Retry, Text, sizeof(Text), &iSize
		) && (iSize == 29u) &&
		xrtHttpRetryBackoff(1000, 8000, 3, &iDelay) &&
		(iDelay == UINT64_C(8000)),
		"HTTP retry protocol allocated memory"
	);
	puts("[PASS] http_retry_noalloc");
	return 0;
}
