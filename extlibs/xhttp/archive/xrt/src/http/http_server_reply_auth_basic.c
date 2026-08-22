#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_BASIC)

/* Basic challenge 写出上下文借用 realm 并保存 UTF-8 声明。 */
typedef struct xrt_http_reply_basic_context {
	xstrview Realm;
	bool Utf8;
} xrt_http_reply_basic_context;



/* 把 Basic challenge 交给协议层安全写出器。 */
static bool __xrtHttpReplyBasicWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xrt_http_reply_basic_context* pBasic =
		(const xrt_http_reply_basic_context*)pContext;

	return xrtHttpBasicChallengeWrite(
		pBasic->Realm,
		pBasic->Utf8,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 使用指定字段名追加 Basic challenge。 */
static bool __xrtHttpReplyAddBasicChallenge(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Realm,
	bool bUtf8
)
{
	xrt_http_reply_basic_context Context;

	Context.Realm = Realm;
	Context.Utf8 = bUtf8;
	return __xrtHttpReplyAddWrittenAuth(
		pReply,
		Name,
		__xrtHttpReplyBasicWrite,
		&Context
	);
}



/* 追加源站 Basic challenge。 */
XRT_API bool xrtHttpReplyAddBasicChallenge(
	xhttpreply* pReply,
	xstrview Realm,
	bool bUtf8
)
{
	return __xrtHttpReplyAddBasicChallenge(
		pReply,
		XRT_STR_LITERAL("WWW-Authenticate"),
		Realm,
		bUtf8
	);
}



/* 追加代理 Basic challenge。 */
XRT_API bool xrtHttpReplyAddProxyBasicChallenge(
	xhttpreply* pReply,
	xstrview Realm,
	bool bUtf8
)
{
	return __xrtHttpReplyAddBasicChallenge(
		pReply,
		XRT_STR_LITERAL("Proxy-Authenticate"),
		Realm,
		bUtf8
	);
}

#endif
