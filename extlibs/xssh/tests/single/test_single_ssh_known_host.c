#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KNOWN_HOST
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* known_hosts 明文层不应带入 SHA-1、随机或网络。 */
int main(void)
{
	xsshknownhostmatch Match;

	#if !defined(XSSH_FEATURE_KNOWN_HOST) || \
		!defined(XSSH_FEATURE_KEY_TEXT)
		#error "SSH known-host dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO_SHA1) || \
		defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH plain known-host unexpectedly enabled hash, random or network"
	#endif

	return (xrtSshKnownHostPatternsMatch(
		XRT_STR_LITERAL("*.example.com"),
		XRT_STR_LITERAL("host.example.com"),
		22u,
		&Match
	) == XSSH_OK) && (Match == XSSH_KNOWN_HOST_MATCH) ? 0 : 1;
}
