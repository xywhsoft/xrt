#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_AUTH_PASSWORD
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Password 单头只闭包到公共认证消息层。 */
int main(void)
{
	unsigned char arrPayload[64];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_AUTH_PASSWORD) || \
		!defined(XSSH_FEATURE_AUTH_MESSAGE)
		#error "SSH password dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH password unexpectedly enabled crypto, random or network"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPasswordWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL("secret")
		) == XSSH_OK) ? 0 : 1;
}
