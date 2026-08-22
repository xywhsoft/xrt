#ifndef XRT_HTTP1_NET_H
#define XRT_HTTP1_NET_H

#include <xrt/http1.h>
#include <xrt/net.h>



#if defined(XRT_FEATURE_HTTP1_NET) && \
	(!defined(XRT_FEATURE_HTTP1_HEAD) || \
	 !defined(XRT_FEATURE_NET_BUFFER))
	#error "XRT HTTP/1 network adapter requires HTTP/1 Head and network buffers"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP1_NET)

/*
	从网络缓冲链严格解析请求 Header；仅在 Header 跨块时合并实际前缀。
	函数不消费输入，Head 的视图借用 Buffer，调用方处理完成后消费 Head.Bytes。
*/
XRT_API xhttp1status xrtHttp1RequestParseBuffer(
	xnetbuf* pBuffer,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);



/*
	从网络缓冲链严格解析响应 Header；Upgrade 后的任何余量保持在 Buffer 中。
	函数不消费输入，Header 跨块时才按需分配连续前缀。
*/
XRT_API xhttp1status xrtHttp1ResponseParseBuffer(
	xnetbuf* pBuffer,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);

#endif



#if defined(XRT_FEATURE_HTTP1_NET)
XRT_EXTERN_C_END
#endif

#endif
