#ifndef XRT_HTTP_SERVER_RAW_H
#define XRT_HTTP_SERVER_RAW_H

#include <xrt/http_server_runtime.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW) && \
	!defined(XHTTP_FEATURE_HTTP_SERVER)
	#error "XRT raw HTTP server response support requires HTTP server support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW)

/*
	原始响应默认在完整报文排空后关闭连接。
	KEEP_ALIVE 只是一项请求；运行时仍会服从请求版本、Connection、半关闭和排空事实。
*/
typedef enum xhttpserverrawflag {
	XHTTP_SERVER_RAW_NONE = 0,
	XHTTP_SERVER_RAW_KEEP_ALIVE = UINT32_C(0x00000001)
} xhttpserverrawflag;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW)

/*
	在 Connection Worker 上复制一条已经完整封包的 HTTP/1 响应并提交。

	运行时不会解析、改写或补全报文；调用方负责状态行、Header、分帧与请求方法语义。
	默认关闭连接，显式请求 KEEP_ALIVE 时也只有请求本身允许复用才会进入下一条请求。
	Response 必须覆盖完整且不回绕的非空字节范围。
*/
XRT_API xnetresult xrtHttpConnRespondRaw(
	xhttpconn* pConnection,
	xbytesview Response,
	uint32 iFlags
);



/*
	零复制提交一条完整 HTTP/1 线缆响应。

	成功后转移 Ref 的释放责任，数据离开发送队列时执行一次 Release；失败时全部责任
	仍归调用方。空引用不转移责任，非空引用必须覆盖完整字节并提供 Release。
*/
XRT_API xnetresult xrtHttpConnRespondRawRef(
	xhttpconn* pConnection,
	const xnetref* pResponse,
	uint32 iFlags
);



/*
	原子提交一组顺序组成完整 HTTP/1 线缆响应的零复制引用。

	成功后转移全部非空引用，失败时一个也不转移。总长度必须已知、非零且不溢出，
	描述符数组在函数返回后不再使用。
*/
XRT_API xnetresult xrtHttpConnRespondRawRefs(
	xhttpconn* pConnection,
	const xnetref* pResponses,
	size_t iCount,
	uint32 iFlags
);



/*
	成功时接管由 xrtMalloc 分配的完整 HTTP/1 线缆响应，失败时所有权仍归调用方。
	非空数据必须具有非零长度。
*/
XRT_API xnetresult xrtHttpConnRespondRawTake(
	xhttpconn* pConnection,
	ptr pResponse,
	size_t iSize,
	uint32 iFlags
);



/*
	在 Connection Worker 上以已知非零长度 Body 作为完整 HTTP/1 线缆响应。

	提交过程保留 Body 引用，调用方可以在函数返回后立即销毁自己的引用。Body 的
	Length 必须已知，实际输出必须严格等于该长度。它可以使用 Borrow、Take 或
	Reference 建立零复制和自定义所有权；异步 AGAIN 需要服务端异步正文组合层。
*/
XRT_API xnetresult xrtHttpConnRespondRawBody(
	xhttpconn* pConnection,
	xhttpbody* pResponse,
	uint32 iFlags
);

#endif



XRT_EXTERN_C_END

#endif
