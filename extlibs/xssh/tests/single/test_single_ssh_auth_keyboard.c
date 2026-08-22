#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_AUTH_KEYBOARD
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Keyboard-interactive 单头只闭包到公共认证消息和 Unicode。 */
int main(void)
{
	unsigned char arrPayload[96];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_AUTH_KEYBOARD) || \
		!defined(XSSH_FEATURE_AUTH_MESSAGE) || !defined(XRT_FEATURE_UNICODE)
		#error "SSH keyboard dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH keyboard unexpectedly enabled crypto, random or network"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL("otp")
		) == XSSH_OK) ? 0 : 1;
}
