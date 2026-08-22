#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_QUERY_PARAMS
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的拥有型 QueryParams 主路径。 */
int main(void)
{
	xqueryparams* pParams;
	str sText;
	size_t iError;
	size_t iSize;

	pParams = xrtQueryParamsParse(
		XRT_STR_LITERAL("q=hello+world&q=two"), NULL, &iError
	);
	if ( (pParams == NULL) || (iError != 19u) ||
		!xrtQueryParamsSet(
			pParams, XRT_STR_LITERAL("q"), XRT_STR_LITERAL("final")
		) ) {
		xrtQueryParamsDestroy(pParams);
		return 1;
	}
	sText = xrtQueryParamsBuild(pParams, &iSize);
	if ( (sText == NULL) || (iSize != 7u) ||
		(memcmp(sText, "q=final", 8u) != 0) ) {
		xrtFree(sText);
		xrtQueryParamsDestroy(pParams);
		return 1;
	}
	xrtFree(sText);
	xrtQueryParamsDestroy(pParams);
	printf("[PASS] single-query-params\n");
	return 0;
}
