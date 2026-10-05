#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_TRANSPORT_MESSAGE
#define XSSH_IMPLEMENTATION
#include "../../include/xssh/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xssh.h"



/* Transport 消息单头只闭包到 wire。 */
int main(void)
{
	unsigned char arrPayload[8];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_TRANSPORT_MESSAGE) || \
		!defined(XSSH_FEATURE_WIRE)
		#error "SSH transport message dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_CRYPTO) || defined(XRT_FEATURE_RANDOM_SECURE) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH transport message unexpectedly enabled crypto, random or network"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshNewKeysWrite(&Writer) == XSSH_OK) ? 0 : 1;
}
