#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_TRANSPORT_TCP_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 随机便利层必须同时闭合 TCP transport 与系统安全随机源。 */
int main(void)
{
	#if !defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM) || \
		!defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		!defined(XSSH_FEATURE_PACKET_RANDOM) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH transport TCP random dependency closure is incomplete"
	#endif

	return 0;
}
