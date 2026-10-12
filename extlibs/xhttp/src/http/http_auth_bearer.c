#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XHTTP_FEATURE_HTTP_AUTH_BEARER)

/* 判断 Bearer token。 */
XHTTP_API bool xrtHttpBearerTokenValid(xstrview Token)
{
	return xrtHttpAuthToken68Valid(Token);
}



/* 写出完整 Bearer 字段值。 */
XHTTP_API bool xrtHttpBearerWrite(
	xstrview Token,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !__xhttpViewValid(Token) ) {
		return false;
	}
	if ( !xrtHttpBearerTokenValid(Token) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bearer"),
		Token,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 解析完整 Bearer 字段值。 */
XHTTP_API bool xrtHttpBearerRead(
	xstrview Value,
	xstrview* pToken
)
{
	xhttpauth Auth;
	xstrview Token = { NULL, 0 };

	if ( !__xhttpViewValid(Value) ||
		!xrtMemRangeValid(pToken, sizeof(Token)) ||
		xrtMemRangesOverlap(
			pToken, sizeof(Token), Value.Data, Value.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pToken, &Token, sizeof(Token));
	if ( !xrtHttpAuthParse(Value, &Auth) ) {
		return false;
	}
	if ( !xrtHttpTokenEqual(
			Auth.Scheme, XRT_STR_LITERAL("Bearer")
		) || (Auth.Kind != XHTTP_AUTH_TOKEN68) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	Token = Auth.Data;
	memcpy(pToken, &Token, sizeof(Token));
	return true;
}

#endif
