#include <stdio.h>

#include <xrt/http_server.h>



/* 在服务端请求回调中解码 Basic 凭据。 */
static bool exampleHttpServerBasic(
	const xhttpserverrequest* pRequest
)
{
	xhttpbasicauth Basic;
	char Output[512];
	size_t iSize;

	if ( xrtHttpServerRequestBasicAuth(
		pRequest,
		Output,
		sizeof(Output),
		&iSize,
		&Basic
	) != XHTTP_NEXT_ITEM ) {
		return false;
	}
	printf("Basic user=%.*s\n", (int)Basic.User.Size, Basic.User.Data);
	return true;
}



/* Request 由真实 HTTP Server 请求回调提供。 */
int main(void)
{
	(void)exampleHttpServerBasic;
	return 0;
}
