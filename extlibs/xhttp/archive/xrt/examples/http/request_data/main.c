#include <xrt/http_server.h>



/* 展示完整请求上按需选择 Query、Cookie 和两类表单辅助器。 */
void inspectRequestData(const xhttpserverrequest* pRequest)
{
	xqueryparams* pQuery;
	xcookiepair Session;
	xcookiepair Cookies[16];
	xmediatype Type;
	size_t iCookieCount;

	pQuery = xrtHttpServerRequestQueryParams(
		pRequest,
		NULL,
		NULL
	);
	if ( pQuery != NULL ) {
		xrtQueryParamsDestroy(pQuery);
	}
	if ( xrtHttpServerRequestCookie(
		pRequest,
		XRT_STR_LITERAL("session"),
		&Session
	) == XCOOKIE_NEXT_ERROR ) {
		xrtClearError();
	}
	/* 一次请求需要读取多个 Cookie 时，批量借用路径只解析字段一次。 */
	if ( !xrtHttpServerRequestCookies(
		pRequest,
		Cookies,
		sizeof(Cookies) / sizeof(Cookies[0]),
		&iCookieCount
	) ) {
		xrtClearError();
	}
	if ( xrtHttpServerRequestContentType(
		pRequest,
		&Type
	) != XHTTP_NEXT_ITEM ) {
		xrtClearError();
		return;
	}
	if ( xrtHttpMediaTypeEqual(
		&Type,
		XRT_STR_LITERAL("application"),
		XRT_STR_LITERAL("x-www-form-urlencoded")
	) ) {
		xqueryparams* pForm = xrtHttpServerRequestForm(
			pRequest,
			NULL,
			NULL
		);

		xrtQueryParamsDestroy(pForm);
	} else if ( xrtHttpMediaTypeEqual(
		&Type,
		XRT_STR_LITERAL("multipart"),
		XRT_STR_LITERAL("form-data")
	) ) {
		xformdata* pForm = xrtHttpServerRequestFormData(
			pRequest,
			NULL,
			NULL,
			NULL
		);

		xrtFormDataDestroy(pForm);
	}
}



/* 示例只编译请求处理函数，不启动网络服务。 */
int main(void)
{
	return 0;
}
