#define XSSH_MODULE_XSSH
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* xssh 聚合单头当前闭合已经迁移完成的协议底层。 */
int main(void)
{
	xstrview Match;

	#if !defined(XSSH_FEATURE_SSH) || !defined(XSSH_FEATURE_PACKET) || \
		!defined(XSSH_FEATURE_WIRE) || \
		!defined(XSSH_FEATURE_PACKET_RANDOM) || \
		!defined(XSSH_FEATURE_PACKET_AES_GCM) || \
		!defined(XSSH_FEATURE_KEXINIT) || \
		!defined(XSSH_FEATURE_KEXINIT_RANDOM) || \
		!defined(XSSH_FEATURE_KEX_ECDH) || \
		!defined(XSSH_FEATURE_KEX_SHA256) || \
		!defined(XSSH_FEATURE_KEX_CURVE25519) || \
		!defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM) || \
		!defined(XSSH_FEATURE_AUTH_MESSAGE) || \
		!defined(XSSH_FEATURE_AUTH_PASSWORD) || \
		!defined(XSSH_FEATURE_AUTH_PUBLICKEY) || \
		!defined(XSSH_FEATURE_AUTH_KEYBOARD) || \
		!defined(XSSH_FEATURE_AUTH_HOSTBASED) || \
		!defined(XSSH_FEATURE_AUTH_GUARD) || \
		!defined(XSSH_FEATURE_CONNECTION_MESSAGE)
		#error "XSSH_MODULE_XSSH dependency closure is incomplete"
	#endif

	return (xrtSshNameListFirstMatch(
		XRT_STR_LITERAL("curve25519-sha256,diffie-hellman-group14-sha256"),
		XRT_STR_LITERAL("diffie-hellman-group14-sha256"),
		&Match
	) == XSSH_OK) &&
		(Match.Size == sizeof("diffie-hellman-group14-sha256") - 1u) ? 0 : 1;
}
