#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 已裁入验证后端时，完整握手仍必须显式提供验证器。 */
int main(void)
{
	xtlsclientconfig Config;
	xtlssession* pSession;

	xrtTlsClientConfigInit(&Config);
	Config.ServerName = XRT_STR_LITERAL("example.com");
	pSession = xrtTlsClientCreate(&Config, NULL);
	return (pSession == NULL) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_VERIFY) ? 0 : 1;
}
