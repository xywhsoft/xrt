#include "../internal/xrt_http.h"

#include <xrt/http_connection.h>



#if defined(XRT_FEATURE_HTTP_CONNECTION)

/* 初始化重复 Connection 字段选项游标。 */
XRT_API void xrtHttpConnectionCursorInit(
	xhttpfieldtokencursor* pCursor
)
{
	xrtHttpFieldTokenCursorInit(pCursor);
}



/* 跨重复 Connection 字段行迭代连接选项。 */
XRT_API xhttpnext xrtHttpConnectionNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpfieldtokencursor* pCursor,
	xstrview* pOption
)
{
	return xrtHttpFieldTokenNext(
		pFields,
		iCount,
		XRT_STR_LITERAL("Connection"),
		pCursor,
		pOption
	);
}



/* 完整验证并统计重复 Connection 字段中的连接选项。 */
XRT_API bool xrtHttpConnectionCount(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pOptionCount
)
{
	return xrtHttpFieldTokenCount(
		pFields,
		iCount,
		XRT_STR_LITERAL("Connection"),
		pOptionCount
	);
}



/* 完整验证重复 Connection 字段并查找指定连接选项。 */
XRT_API xhttpnext xrtHttpConnectionFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Option
)
{
	return xrtHttpFieldTokenFind(
		pFields,
		iCount,
		XRT_STR_LITERAL("Connection"),
		Option
	);
}



/* 按协议版本、消息方向和接收角色判断 HTTP/1 连接持久性。 */
XRT_API xhttpconnectionstatus xrtHttpConnectionPersistence(
	xhttpversion Version,
	const xhttpfield* pFields,
	size_t iCount,
	uint32 iFlags
)
{
	xhttpnext Next;
	const uint32 iKnownFlags =
		(uint32)XHTTP_CONNECTION_RESPONSE |
		(uint32)XHTTP_CONNECTION_PROXY |
		(uint32)XHTTP_CONNECTION_ALLOW_HTTP10_KEEP_ALIVE;

	if ( ((Version != XHTTP_VERSION_1_0) &&
		 (Version != XHTTP_VERSION_1_1)) ||
		((iFlags & ~iKnownFlags) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CONNECTION_ERROR;
	}
	Next = xrtHttpConnectionFind(
		pFields, iCount, XRT_STR_LITERAL("close")
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return XHTTP_CONNECTION_ERROR;
	}
	if ( Next == XHTTP_NEXT_ITEM ) {
		return XHTTP_CONNECTION_CLOSE;
	}
	if ( Version == XHTTP_VERSION_1_1 ) {
		return XHTTP_CONNECTION_PERSIST;
	}
	if ( (iFlags &
		(uint32)XHTTP_CONNECTION_ALLOW_HTTP10_KEEP_ALIVE) == 0 ) {
		return XHTTP_CONNECTION_CLOSE;
	}
	Next = xrtHttpConnectionFind(
		pFields, iCount, XRT_STR_LITERAL("keep-alive")
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return XHTTP_CONNECTION_ERROR;
	}
	if ( (Next == XHTTP_NEXT_ITEM) &&
		(((iFlags & (uint32)XHTTP_CONNECTION_PROXY) == 0) ||
		 ((iFlags & (uint32)XHTTP_CONNECTION_RESPONSE) != 0)) ) {
		return XHTTP_CONNECTION_PERSIST;
	}
	return XHTTP_CONNECTION_CLOSE;
}

#endif
