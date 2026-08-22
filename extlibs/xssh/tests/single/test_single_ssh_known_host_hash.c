#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KNOWN_HOST_HASH
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* hashed-host 单头只在明文层之上增加 SHA-1。 */
int main(void)
{
	static const char sHash[] =
		"|1|ICEiIyQlJicoKSorLC0uLzAxMjM=|xBQE+zEva59fZY5Xy11PuF9jNd8=";
	bool bMatch = false;

	#if !defined(XSSH_FEATURE_KNOWN_HOST_HASH) || \
		!defined(XSSH_FEATURE_KNOWN_HOST) || \
		!defined(XRT_FEATURE_CRYPTO_SHA1)
		#error "SSH known-host hash dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_NETWORK)
		#error "SSH known-host hash unexpectedly enabled random or network"
	#endif

	return (xrtSshKnownHostHashMatch(
		XRT_STR_LITERAL(sHash),
		XRT_STR_LITERAL("hashed.example"),
		22u,
		&bMatch
	) == XSSH_OK) && bMatch ? 0 : 1;
}
