#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEXINIT_RANDOM
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 安全 KEXINIT 单头只在纯协议层上增加 random_secure。 */
int main(void)
{
	unsigned char arrPayload[512];
	xsshkexinitconfig Config;
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_KEXINIT_RANDOM) || \
		!defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH secure KEXINIT dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_NETWORK)
		#error "SSH secure KEXINIT unexpectedly enabled crypto or network"
	#endif

	return xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	) && xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexInitWriteSecure(&Writer, &Config) == XSSH_OK) ? 0 : 1;
}
