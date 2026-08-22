#include "../internal/xrt_cookie_jar.h"



#if defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)

/* 批量接收独立 Set-Cookie 字段并保留已经提交的结果。 */
XRT_API bool xrtCookieJarStoreHeaders(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	const xhttpheaders* pHeaders,
	xcookiestorereport* pReport
)
{
	xcookiestorereport Report;
	size_t i;

	memset(&Report, 0, sizeof(Report));
	if ( (pJar == NULL) || (pContext == NULL) || (pHeaders == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < xrtHttpHeadersCount(pHeaders); i++ ) {
		const xhttpfield* pField = xrtHttpHeadersAt(pHeaders, i);
		xcookiestorestatus Status;

		if ( (pField == NULL) || !xrtHttpFieldNameEqual(
			pField->Name, XRT_STR_LITERAL("Set-Cookie")
		) ) {
			continue;
		}
		Report.Fields++;
		Status = xrtCookieJarStore(
			pJar, pContext, pField->Value, NULL
		);
		if ( Status == XCOOKIE_STORE_ERROR ) {
			if ( pReport != NULL ) {
				*pReport = Report;
			}
			return false;
		}
		if ( Status == XCOOKIE_STORE_STORED ) {
			Report.Stored++;
		} else if ( Status == XCOOKIE_STORE_REMOVED ) {
			Report.Removed++;
		} else if ( Status == XCOOKIE_STORE_IGNORED ) {
			Report.Ignored++;
		} else {
			Report.Rejected++;
		}
	}
	if ( pReport != NULL ) {
		*pReport = Report;
	}
	return true;
}



/* 把 Jar 的选择结果设置为单个 Cookie 请求字段。 */
XRT_API bool xrtCookieJarApply(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	xhttpheaders* pHeaders
)
{
	str sValue;
	size_t iSize;
	bool bResult;

	if ( (pJar == NULL) || (pContext == NULL) || (pHeaders == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	sValue = xrtCookieJarBuild(pJar, pContext, &iSize);
	if ( sValue == NULL ) {
		return false;
	}
	if ( iSize == 0 ) {
		(void)xrtHttpHeadersRemove(
			pHeaders, XRT_STR_LITERAL("Cookie")
		);
		bResult = true;
	} else {
		bResult = xrtHttpHeadersSet(
			pHeaders, XRT_STR_LITERAL("Cookie"),
			(xstrview){ sValue, iSize }
		);
	}
	xrtFree(sValue);
	return bResult;
}

#endif

