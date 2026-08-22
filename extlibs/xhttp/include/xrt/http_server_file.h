#ifndef XRT_HTTP_SERVER_FILE_H
#define XRT_HTTP_SERVER_FILE_H

#include <xrt/http_body_file.h>
#include <xrt/http_server_future.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_FILE) && \
	(!defined(XHTTP_FEATURE_HTTP_BODY_FILE) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC))
	#error "XRT HTTP server file support requires file body, server Future and async body support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_FILE)

XRT_EXTERN_C_BEGIN



/*
	异步准备完整文件正文，并返回拥有 xhttpreply* 的 Future。
	ContentType 为空时不添加 Content-Type，线路层自动生成 Content-Length。
*/
XRT_API xfuture* xrtHttpReplyFileFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath
);



/*
	异步准备严格文件区间，并返回拥有 xhttpreply* 的 Future。
	区间越过准备时的文件大小会失败，不会静默截断。
*/
XRT_API xfuture* xrtHttpReplyFileRangeFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength
);



/*
	在 Connection Worker 上异步准备完整文件并绑定唯一最终响应。
	成功表示响应 Future 已受理，不表示文件已经打开或响应已经发送。
*/
XRT_API bool xrtHttpConnFile(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath
);



/*
	在 Connection Worker 上异步准备严格文件区间并绑定唯一最终响应。
	该函数不推断 Range、Content-Range、ETag 或缓存语义。
*/
XRT_API bool xrtHttpConnFileRange(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength
);



XRT_EXTERN_C_END

#endif

#endif
