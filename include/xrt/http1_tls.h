#ifndef XRT_HTTP1_TLS_H
#define XRT_HTTP1_TLS_H

#include <xrt/http1_net.h>
#include <xrt/tls_stream.h>



#if defined(XRT_FEATURE_HTTP1_TLS) && \
	(!defined(XRT_FEATURE_HTTP1_NET) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "XRT HTTP/1 TLS adapter requires HTTP/1 network parsing and TLS Stream"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP1_TLS)

/*
	从当前 TLS 明文块链严格解析请求 Header，只连续化实际 Header 前缀。
	函数不消费明文，Head 的视图借用 TLS Stream，随后可用 Head.Bytes 原子接管。
*/
XRT_API xhttp1status xrtHttp1RequestParseTls(
	xtlsstream* pStream,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);



/*
	从当前 TLS 明文块链严格解析响应 Header，完整保留 Upgrade 后明文余量。
	跨 TLS 记录时只按需合并 Header，不分配连接级固定缓冲。
*/
XRT_API xhttp1status xrtHttp1ResponseParseTls(
	xtlsstream* pStream,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);

#endif



#if defined(XRT_FEATURE_HTTP1_TLS)
XRT_EXTERN_C_END
#endif

#endif
