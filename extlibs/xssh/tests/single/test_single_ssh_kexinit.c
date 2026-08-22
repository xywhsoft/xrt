#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEXINIT
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* KEXINIT 单头保持纯协议闭包。 */
int main(void)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE] = { 0u };
	unsigned char arrPayload[512];
	xsshkexinitconfig Config;
	xsshwriter Writer;
	xsshkexinit KexInit;

	#if !defined(XSSH_FEATURE_KEXINIT) || !defined(XSSH_FEATURE_WIRE)
		#error "SSH KEXINIT dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_RANDOM_SECURE) || defined(XRT_FEATURE_CRYPTO) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH KEXINIT unexpectedly enabled random, crypto or network"
	#endif

	return xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	) && xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) == XSSH_OK) && (xrtSshKexInitRead(
			(xbytesview){ arrPayload, Writer.Size },
			&KexInit
		) == XSSH_OK) ? 0 : 1;
}
