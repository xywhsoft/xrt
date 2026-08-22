#define XRT_MODULE_JSON_ESCAPE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头 quote Writer 只统计输出长度。 */
static bool testSingleJsonEscapeWrite(xbytesview Data, ptr pUserData)
{
	size_t* pSize = (size_t*)pUserData;

	*pSize += Data.Size;
	return true;
}



/* 验证 JSON escape 不拉入 DOM、Buffer 或完整 Writer。 */
int main(void)
{
	size_t iSize = 0;
	size_t iWritten;

	#if !defined(XRT_FEATURE_JSON_ESCAPE) || \
		!defined(XRT_FEATURE_JSON_CORE) || \
		!defined(XRT_FEATURE_UNICODE) || \
		defined(XRT_FEATURE_JSON_WRITE) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_VALUE_CONTAINER)
		#error "XRT_MODULE_JSON_ESCAPE dependency closure is not minimal"
	#endif

	if (
		!xrtJsonQuoteWrite(
			XRT_STR_LITERAL("a\n"),
			0,
			testSingleJsonEscapeWrite,
			&iSize,
			&iWritten
		) ||
		(iSize != 5u) || (iWritten != 5u)
	) {
		return 1;
	}
	return 0;
}
