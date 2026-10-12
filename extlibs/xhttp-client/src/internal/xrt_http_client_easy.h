#ifndef XRT_INTERNAL_HTTP_CLIENT_EASY_H
#define XRT_INTERNAL_HTTP_CLIENT_EASY_H

#include <xrt/http_client_easy.h>



#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY)

/* 校验便利入口共有的 Client 与可选完成过程参数。 */
bool __xrtHttpClientEasyCheck(
	xhttpclient* pClient,
	bool bCompletion,
	cstr sOperation
);



/* 创建临时请求；Body 为空指针表示没有正文，非空则复制固定字节正文。 */
xhttprequest* __xrtHttpClientEasyRequest(
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType
);

#endif

#endif


