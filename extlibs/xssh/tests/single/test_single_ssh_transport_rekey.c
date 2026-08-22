#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_TRANSPORT_REKEY
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Rekey 状态单头不携带密码、时钟或网络实现。 */
int main(void)
{
	xsshrekeystate State;

	#if !defined(XSSH_FEATURE_TRANSPORT_REKEY) || \
		!defined(XSSH_FEATURE_WIRE)
		#error "SSH transport rekey dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_TIME) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH transport rekey unexpectedly enabled crypto, time or network"
	#endif

	return xrtSshRekeyInit(&State, NULL, 0u) ? 0 : 1;
}
