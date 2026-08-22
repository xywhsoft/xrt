#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_AUTH_MESSAGE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 公共认证消息单头只闭包到 wire 和 Unicode 校验。 */
int main(void)
{
	unsigned char arrPayload[64];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_AUTH_MESSAGE) || \
		!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_UNICODE)
		#error "SSH auth message dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH auth message unexpectedly enabled crypto, random or network"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthNoneWrite(&Writer, XRT_STR_LITERAL("alice")) == XSSH_OK) ?
		0 : 1;
}
