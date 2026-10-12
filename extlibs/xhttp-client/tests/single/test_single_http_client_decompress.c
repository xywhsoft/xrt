#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_CLIENT_IMPLEMENTATION
#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_DECOMPRESS
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-client/features.h"
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-policy.h"
#include "../../../../single/extlibs/xhttp-client.h"


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
