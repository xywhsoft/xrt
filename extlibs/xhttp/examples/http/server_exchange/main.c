#include <stdio.h>
#include <string.h>
#include <xhttp.h>



/* 在请求完整后打印已经稳定拥有的请求事实。 */
static bool exampleComplete(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xstrview Method = xrtHttpServerRequestMethod(pRequest);
	xstrview Target = xrtHttpServerRequestTarget(pRequest);

	(void)pExchange;
	(void)pData;
	printf(
		"%.*s %.*s body=%llu\n",
		(int)Method.Size,
		Method.Data,
		(int)Target.Size,
		Target.Data,
		(unsigned long long)xrtHttpServerRequestBodyBytes(
			pRequest
		)
	);
	return true;
}



/* 演示把任意网络接收片段交给无 I/O 服务端协议核心。 */
int main(void)
{
	static const char Wire[] =
		"POST /items HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 5\r\n"
		"\r\n"
		"hello";
	xhttp1serverevents Events = {
		NULL,
		NULL,
		exampleComplete,
		NULL
	};
	xhttp1serverexchange* pExchange =
		xrtHttp1ServerExchangeCreate(NULL, &Events);
	size_t iAccepted = 0;
	xhttp1serverfeedstatus Status;

	if ( pExchange == NULL ) {
		return 1;
	}
	Status = xrtHttp1ServerExchangeFeed(
		pExchange,
		(xbytesview){
			(cbytes)Wire,
			strlen(Wire)
		},
		false,
		&iAccepted
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
	return (Status == XHTTP1_SERVER_FEED_COMPLETE) &&
		(iAccepted == strlen(Wire)) ? 0 : 1;
}

