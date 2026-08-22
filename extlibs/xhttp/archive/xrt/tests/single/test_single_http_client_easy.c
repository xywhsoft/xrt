#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 空完成过程只用于单头文件参数门槛。 */
static void testSingleHttpEasyDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	(void)pCall;
	(void)pResult;
	(void)pData;
}



/* 验证单头文件发布 callback 便利入口与结构化参数错误。 */
int main(void)
{
	xrtClearError();
	if ( (xrtHttpClientGet(
		NULL,
		XRT_STR_LITERAL("http://example.test/"),
		NULL,
		testSingleHttpEasyDone,
		NULL
	) != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 1;
	}
	return 0;
}
