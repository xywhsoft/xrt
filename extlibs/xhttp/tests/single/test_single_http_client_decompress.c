#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_DECOMPRESS
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头文件公开自动解码默认值和 raw 覆盖模式。 */
int main(void)
{
	xhttpclientconfig Client;
	xhttpcalloptions Call;

	xrtHttpClientConfigInit(&Client);
	xrtHttpCallOptionsInit(&Call);
	if ( !Client.Decompress.Enabled ||
		(Client.Decompress.MaxBody !=
		 XHTTP_DECOMPRESS_BODY_DEFAULT) ||
		(Client.Decompress.MaxCodings !=
		 XHTTP_DECOMPRESS_CODINGS_DEFAULT) ||
		(Call.Decompress !=
		 XHTTP_DECOMPRESS_DEFAULT) ) {
		return 1;
	}
	Call.Decompress = XHTTP_DECOMPRESS_RAW;
	printf("[PASS] single-http-client-decompress\n");
	return 0;
}
