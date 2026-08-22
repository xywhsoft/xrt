#ifndef XRT_HTTP_CONNECTION_H
#define XRT_HTTP_CONNECTION_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_CONNECTION) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP Connection support requires HTTP support"
#endif



#if defined(XRT_FEATURE_HTTP_CONNECTION)

/* 连接持久性结果区分协议错误、当前响应后关闭和继续复用。 */
typedef enum xhttpconnectionstatus {
	XHTTP_CONNECTION_ERROR = -1,
	XHTTP_CONNECTION_CLOSE = 0,
	XHTTP_CONNECTION_PERSIST = 1
} xhttpconnectionstatus;



/* HTTP/1.0 持久性判断所需的消息方向、接收角色和本地策略。 */
typedef enum xhttpconnectionflag {
	XHTTP_CONNECTION_RESPONSE = UINT32_C(0x00000001),
	XHTTP_CONNECTION_PROXY = UINT32_C(0x00000002),
	XHTTP_CONNECTION_ALLOW_HTTP10_KEEP_ALIVE = UINT32_C(0x00000004)
} xhttpconnectionflag;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_CONNECTION)

/* 初始化重复 Connection 字段选项游标。 */
XRT_API void xrtHttpConnectionCursorInit(
	xhttpfieldtokencursor* pCursor
);



/*
	跨重复 Connection 字段行迭代连接选项。
	第一次发布前完整验证所有 Connection 字段，选项借用原字段值。
*/
XRT_API xhttpnext xrtHttpConnectionNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpfieldtokencursor* pCursor,
	xstrview* pOption
);



/* 完整验证并统计全部重复 Connection 字段中的非空选项。 */
XRT_API bool xrtHttpConnectionCount(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pOptionCount
);



/*
	在全部重复 Connection 字段中查找大小写不敏感的连接选项。
	函数先验证所有 Connection 值，再返回 ITEM、END 或 ERROR。
*/
XRT_API xhttpnext xrtHttpConnectionFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Option
);



/*
	按 RFC 9112 判断 HTTP/1 连接是否能在当前响应后继续复用。
	HTTP/1.0 只有显式允许兼容机制、存在 keep-alive，且满足代理方向限制时才持久。
*/
XRT_API xhttpconnectionstatus xrtHttpConnectionPersistence(
	xhttpversion Version,
	const xhttpfield* pFields,
	size_t iCount,
	uint32 iFlags
);

#endif



XRT_EXTERN_C_END

#endif
