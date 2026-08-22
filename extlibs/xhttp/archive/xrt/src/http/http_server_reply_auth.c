#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH)

/* 通用 challenge 写出上下文借用方案和数据。 */
typedef struct xrt_http_reply_auth_write_context {
	xstrview Scheme;
	xstrview Data;
} xrt_http_reply_auth_write_context;



/* 把通用 challenge 交给协议层写出器。 */
static bool __xrtHttpReplyAuthWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xrt_http_reply_auth_write_context* pAuth =
		(const xrt_http_reply_auth_write_context*)pContext;

	return xrtHttpAuthWrite(
		pAuth->Scheme,
		pAuth->Data,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 回复字段消费上下文只在同步 challenge 构建期间有效。 */
typedef struct xrt_http_reply_auth_consume_context {
	xhttpreply* Reply;
	xstrview Name;
	bool Replace;
} xrt_http_reply_auth_consume_context;



/* 把临时 challenge 复制进回复拥有的 Header 容器。 */
static bool __xrtHttpReplyAuthConsume(
	void* pContext,
	xstrview Value
)
{
	xrt_http_reply_auth_consume_context* pAuth =
		(xrt_http_reply_auth_consume_context*)pContext;

	if ( pAuth->Replace ) {
		return xrtHttpReplySetHeader(
			pAuth->Reply, pAuth->Name, Value
		);
	}
	return xrtHttpReplyAddHeader(
		pAuth->Reply, pAuth->Name, Value
	);
}



/* 通过共享认证事务写出、提交并清零字段值。 */
static bool __xrtHttpReplyWrittenAuth(
	xhttpreply* pReply,
	xstrview Name,
	__xrtHttpAuthWriteFunction pWrite,
	const void* pContext,
	bool bReplace
)
{
	xrt_http_reply_auth_consume_context Consume;

	if ( (pReply == NULL) || (pWrite == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Consume.Reply = pReply;
	Consume.Name = Name;
	Consume.Replace = bReplace;
	return __xrtHttpAuthWriteTemporary(
		pWrite,
		pContext,
		__xrtHttpReplyAuthConsume,
		&Consume
	);
}



/* 写出并追加一条认证 challenge。 */
bool __xrtHttpReplyAddWrittenAuth(
	xhttpreply* pReply,
	xstrview Name,
	__xrtHttpAuthWriteFunction pWrite,
	const void* pContext
)
{
	return __xrtHttpReplyWrittenAuth(
		pReply, Name, pWrite, pContext, false
	);
}



/* 写出并设置唯一认证信息字段。 */
bool __xrtHttpReplySetWrittenAuth(
	xhttpreply* pReply,
	xstrview Name,
	__xrtHttpAuthWriteFunction pWrite,
	const void* pContext
)
{
	return __xrtHttpReplyWrittenAuth(
		pReply, Name, pWrite, pContext, true
	);
}



/* 使用指定字段名追加通用 challenge。 */
static bool __xrtHttpReplyAddChallenge(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Scheme,
	xstrview Data
)
{
	xrt_http_reply_auth_write_context Context;

	Context.Scheme = Scheme;
	Context.Data = Data;
	return __xrtHttpReplyAddWrittenAuth(
		pReply,
		Name,
		__xrtHttpReplyAuthWrite,
		&Context
	);
}



/* 追加源站 WWW-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddChallenge(
	xhttpreply* pReply,
	xstrview Scheme,
	xstrview Data
)
{
	return __xrtHttpReplyAddChallenge(
		pReply,
		XRT_STR_LITERAL("WWW-Authenticate"),
		Scheme,
		Data
	);
}



/* 追加代理 Proxy-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddProxyChallenge(
	xhttpreply* pReply,
	xstrview Scheme,
	xstrview Data
)
{
	return __xrtHttpReplyAddChallenge(
		pReply,
		XRT_STR_LITERAL("Proxy-Authenticate"),
		Scheme,
		Data
	);
}

#endif
