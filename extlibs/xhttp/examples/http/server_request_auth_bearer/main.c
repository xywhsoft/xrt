#include <stdio.h>

#include <xrt/http_server.h>



/* 在服务端请求回调中读取借用型 Bearer token。 */
static bool exampleHttpServerBearer(
	const xhttpserverrequest* pRequest
)
{
	xstrview Token;

	if ( xrtHttpServerRequestBearerAuth(
		pRequest,
		&Token
	) != XHTTP_NEXT_ITEM ) {
		return false;
	}
	printf("Bearer token=%.*s\n", (int)Token.Size, Token.Data);
	return true;
}



/* Request 由真实 HTTP Server 请求回调提供。 */
int main(void)
{
	(void)exampleHttpServerBearer;
	return 0;
}
