#ifndef XRT_HTTP_EXCHANGE_ASYNC_H
#define XRT_HTTP_EXCHANGE_ASYNC_H

#include <xrt/http_exchange.h>
#include <xrt/http_body.h>



#if defined(XRT_FEATURE_HTTP_EXCHANGE_ASYNC) && \
	(!defined(XRT_FEATURE_HTTP_EXCHANGE) || \
	 !defined(XRT_FEATURE_HTTP_BODY_ASYNC))
	#error "XRT async HTTP exchange support requires exchange and async body support"
#endif



#if defined(XRT_FEATURE_HTTP_EXCHANGE_ASYNC)

XRT_EXTERN_C_BEGIN



/*
	Output 返回 AGAIN 后取得一次正文可读性 Future。
	调用方拥有返回引用；Future 完成后再次调用 Output。
*/
XRT_API xfuture* xrtHttp1ExchangeOutputWait(
	xhttp1exchange* pExchange
);



XRT_EXTERN_C_END

#endif

#endif
