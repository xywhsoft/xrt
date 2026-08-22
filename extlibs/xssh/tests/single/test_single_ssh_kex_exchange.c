#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEX_EXCHANGE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 确定性交换只应携带动态缓冲、transport core 和 KEX 会话。 */
int main(void)
{
	xsshkexexchange Exchange;

	#if !defined(XSSH_FEATURE_KEX_EXCHANGE) || \
		!defined(XSSH_FEATURE_KEX_SESSION) || \
		!defined(XRT_FEATURE_NET_BUFFER)
		#error "SSH KEX exchange dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		defined(XRT_FEATURE_TASK) || \
		defined(XRT_FEATURE_COROUTINE)
		#error "SSH KEX exchange core pulled unrelated convenience dependencies"
	#endif

	if ( !xrtSshKexExchangeInit(
		&Exchange,
		NULL,
		XSSH_ROLE_CLIENT
	) ) {
		return 1;
	}
	xrtSshKexExchangeClear(&Exchange);
	return 0;
}
