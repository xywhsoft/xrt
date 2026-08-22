#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEX_EXCHANGE_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 随机便利闭包必须精确增加安全随机与 Curve25519 密钥对。 */
int main(void)
{
	#if !defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) || \
		!defined(XSSH_FEATURE_KEX_EXCHANGE) || \
		!defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH random KEX exchange dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH random KEX exchange pulled unrelated runtime dependencies"
	#endif

	return 0;
}
