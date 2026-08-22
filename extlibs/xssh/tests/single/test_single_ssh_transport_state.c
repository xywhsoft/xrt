#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_TRANSPORT_STATE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Transport 状态单头不携带 packet codec、密码、网络或运行时。 */
int main(void)
{
	xsshtransportstate State;
	xsshtransportkexrules Rules;

	#if !defined(XSSH_FEATURE_TRANSPORT_STATE) || \
		!defined(XSSH_FEATURE_KEXINIT) || \
		!defined(XSSH_FEATURE_TRANSPORT_MESSAGE)
		#error "SSH transport state dependency closure is incomplete"
	#endif
	#if defined(XSSH_FEATURE_PACKET_CODEC) || \
		defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_NETWORK) || \
		defined(XRT_FEATURE_TASK)
		#error "SSH transport state unexpectedly enabled codec, crypto or runtime"
	#endif

	return xrtSshTransportStateInit(&State, XSSH_ROLE_CLIENT) &&
		xrtSshTransportKexRulesInit(&Rules) &&
		xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_LOCAL,
			30u,
			1u
		) &&
		(xrtSshTransportIdentificationCommit(
			&State,
			XSSH_TRANSPORT_LOCAL
		) == XSSH_OK) && (xrtSshTransportIdentificationCommit(
			&State,
			XSSH_TRANSPORT_PEER
		) == XSSH_OK) ? 0 : 1;
}
