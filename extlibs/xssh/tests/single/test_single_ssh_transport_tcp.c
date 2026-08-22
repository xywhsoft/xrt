#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_TRANSPORT_TCP
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* TCP transport 单头只增加动态 xnetbuf，不嵌入固定 packet 数组。 */
int main(void)
{
	xsshtransporttcpconfig Config;
	xsshtransporttcp Transport;

	#if !defined(XSSH_FEATURE_TRANSPORT_TCP) || \
		!defined(XSSH_FEATURE_TRANSPORT_CORE) || \
		!defined(XRT_FEATURE_NET_TCP)
		#error "SSH transport TCP dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_PACKET_RANDOM) || defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH transport TCP core must not pull the secure-random convenience layer"
	#endif

	if ( !xrtSshTransportTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshTransportTcpInit(
		&Transport,
		NULL,
		&Config,
		0u
	) ) {
		return 1;
	}
	xrtSshTransportTcpClear(&Transport);
	return 0;
}
