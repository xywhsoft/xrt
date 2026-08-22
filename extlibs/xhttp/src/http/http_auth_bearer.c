#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XHTTP_FEATURE_HTTP_AUTH_BEARER)

/* 判断 Bearer token。 */
XRT_API bool xrtHttpBearerTokenValid(xstrview Token)
{
	return xrtHttpAuthToken68Valid(Token);
}



/* 写出完整 Bearer 字段值。 */
XRT_API bool xrtHttpBearerWrite(
	xstrview Token,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !__xrtHttpViewValid(Token) ) {
		return false;
	}
	if ( !xrtHttpBearerTokenValid(Token) ) {
		__xrtErrorSetValue();
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
XRT_API bool xrtHttpBearerRead(
	xstrview Value,
	xstrview* pToken
)
{
	xhttpauth Auth;
	xstrview Token = { NULL, 0 };

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pToken, sizeof(Token)) ||
		__xrtRangesOverlap(
			pToken, sizeof(Token), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pToken, &Token, sizeof(Token));
	if ( !xrtHttpAuthParse(Value, &Auth) ) {
		return false;
	}
	if ( !xrtHttpTokenEqual(
			Auth.Scheme, XRT_STR_LITERAL("Bearer")
		) || (Auth.Kind != XHTTP_AUTH_TOKEN68) ) {
		__xrtErrorSetValue();
		return false;
	}
	Token = Auth.Data;
	memcpy(pToken, &Token, sizeof(Token));
	return true;
}

#endif
