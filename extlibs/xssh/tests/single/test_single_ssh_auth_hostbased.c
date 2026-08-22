#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_AUTH_HOSTBASED
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Hostbased 单头只闭包到公共认证消息和主机密钥格式层。 */
int main(void)
{
	#if !defined(XSSH_FEATURE_AUTH_HOSTBASED) || \
		!defined(XSSH_FEATURE_AUTH_MESSAGE) || !defined(XSSH_FEATURE_HOSTKEY)
		#error "SSH hostbased dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH hostbased unexpectedly enabled crypto, random or network"
	#endif

	return xrtSshAuthHostNameValid(
		XRT_STR_LITERAL("client.example.com")
	) ? 0 : 1;
}
