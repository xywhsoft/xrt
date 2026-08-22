#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_TRANSPORT_CORE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Transport core 单头只组合 packet、状态和预算，不携带网络或系统时钟。 */
int main(void)
{
	xsshtransportcore Core;

	#if !defined(XSSH_FEATURE_TRANSPORT_CORE) || \
		!defined(XSSH_FEATURE_PACKET_CODEC) || \
		!defined(XSSH_FEATURE_TRANSPORT_REKEY) || \
		!defined(XSSH_FEATURE_TRANSPORT_STATE)
		#error "SSH transport core dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_TIME) || \
		defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH transport core unexpectedly enabled network, time or random"
	#endif

	if ( !xrtSshTransportCoreInit(
		&Core,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	) ) {
		return 1;
	}
	xrtSshTransportCoreClear(&Core);
	return 0;
}
