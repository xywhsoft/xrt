#include <xhttp.h>

#include <stdio.h>



/* 解析、修改并重新构建一组拥有型 URL 查询参数。 */
int main(void)
{
	xqueryparams* pParams;
	str sQuery;
	size_t iError;
	size_t iSize;

	pParams = xrtQueryParamsParse(
		XRT_STR_LITERAL("page=1&tag=c&tag=network"),
		NULL,
		&iError
	);
	if ( pParams == NULL ) {
		return 1;
	}
	if ( !xrtQueryParamsSet(
		pParams, XRT_STR_LITERAL("page"), XRT_STR_LITERAL("2")
	) || !xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("tag"), XRT_STR_LITERAL("xlang")
	) ) {
		xrtQueryParamsDestroy(pParams);
		return 1;
	}
	sQuery = xrtQueryParamsBuild(pParams, &iSize);
	if ( sQuery == NULL ) {
		xrtQueryParamsDestroy(pParams);
		return 1;
	}
	printf("%.*s\n", (int)iSize, sQuery);
	xrtFree(sQuery);
	xrtQueryParamsDestroy(pParams);
	return 0;
}
