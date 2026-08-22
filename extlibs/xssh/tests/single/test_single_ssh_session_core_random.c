#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_SESSION_CORE_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 随机会话闭包只额外携带系统安全随机和 Curve25519 密钥对。 */
int main(void)
{
	#if !defined(XSSH_FEATURE_SESSION_CORE_RANDOM) || \
		!defined(XSSH_FEATURE_SESSION_CORE) || \
		!defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH random session core dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		defined(XRT_FEATURE_TASK) || defined(XRT_FEATURE_COROUTINE)
		#error "SSH random session core pulled unrelated runtime dependencies"
	#endif

	return 0;
}
