#include <stdio.h>

#include <xrt/http_server.h>



/* 在服务端请求回调中解码 Digest Authorization。 */
static bool exampleHttpServerDigest(
	const xhttpserverrequest* pRequest
)
{
	xhttpdigestauth Digest;
	char Output[1024];
	size_t iSize;

	if ( xrtHttpServerRequestDigestAuth(
		pRequest,
		Output,
		sizeof(Output),
		&iSize,
		&Digest
	) != XHTTP_NEXT_ITEM ) {
		return false;
	}
	printf(
		"Digest user=%.*s realm=%.*s\n",
		(int)Digest.Username.Size,
		Digest.Username.Data,
		(int)Digest.Realm.Size,
		Digest.Realm.Data
	);
	return true;
}



/* Request 由真实 HTTP Server 请求回调提供。 */
int main(void)
{
	(void)exampleHttpServerDigest;
	return 0;
}
