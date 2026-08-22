#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_KEX_ECDH
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* ECDH 报文单头只闭包到 wire。 */
int main(void)
{
	unsigned char arrPayload[40];
	xsshwriter Writer;
	xsshecdhinit Message;

	#if !defined(XSSH_FEATURE_KEX_ECDH) || !defined(XSSH_FEATURE_WIRE)
		#error "SSH ECDH dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH ECDH unexpectedly enabled crypto, random or network"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshEcdhInitWrite(
			&Writer,
			XRT_BYTES_LITERAL("public")
		) == XSSH_OK) && (xrtSshEcdhInitRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Message
		) == XSSH_OK) ? 0 : 1;
}
