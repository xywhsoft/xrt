#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



/* 验证单头文件 HTTPS Client 默认启用系统信任并发布 TLS 字段。 */
int main(void)
{
	xhttpclientconfig Config;

	xrtHttpClientConfigInit(&Config);
	if ( !Config.SystemTrust ||
		(Config.TlsContext != NULL) ||
		(Config.TlsVerifier != NULL) ) {
		return 1;
	}
	return 0;
}
