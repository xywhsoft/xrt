#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头 Exchange 能解析最小 HTTP/1.1 请求。 */
int main(void)
{
	static const char Wire[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"\r\n";
	xhttp1serverexchange* pExchange =
		xrtHttp1ServerExchangeCreate(NULL, NULL);
	size_t iAccepted = 0;
	bool bPassed;

	if ( pExchange == NULL ) {
		return 1;
	}
	bPassed = (xrtHttp1ServerExchangeFeed(
		pExchange,
		(xbytesview){
			(cbytes)Wire,
			sizeof(Wire) - 1u
		},
		false,
		&iAccepted
	) == XHTTP1_SERVER_FEED_COMPLETE) &&
		(iAccepted == (sizeof(Wire) - 1u));
	xrtHttp1ServerExchangeDestroy(pExchange);
	return bPassed ? 0 : 1;
}
