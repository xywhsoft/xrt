#ifndef XRT_HTTP_SERVER_RESPONSE_ASYNC_H
#define XRT_HTTP_SERVER_RESPONSE_ASYNC_H

#include <xrt/http_server_exchange.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE_ASYNC) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE) || \
	 !defined(XHTTP_FEATURE_HTTP_BODY_ASYNC))
	#error "XRT async HTTP server response support requires response and async body support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE_ASYNC)

XRT_EXTERN_C_BEGIN



/*
	Output 返回 AGAIN 后取得一次正文可读性 Future。
	调用方拥有返回引用；Future 成功完成后再次调用 Output。
*/
XRT_API xfuture* xrtHttp1ServerResponseWait(
	xhttp1serverresponse* pResponse
);



XRT_EXTERN_C_END

#endif

#endif

