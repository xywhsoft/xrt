#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_CLIENT_IMPLEMENTATION
#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_EASY
	#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_EASY
#endif
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-client/features.h"
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-policy.h"
#include "../../../../single/extlibs/xhttp-client.h"




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


