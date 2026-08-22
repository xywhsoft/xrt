#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CHANNEL_MESSAGE
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* Channel message 单头只依赖 wire 与 UTF-8 校验。 */
int main(void)
{
	unsigned char arrPayload[8];
	xsshwriter Writer;
	uint32 iRecipient;

	#if !defined(XSSH_FEATURE_CHANNEL_MESSAGE) || \
		!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_UNICODE)
		#error "SSH channel message dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_CRYPTO) || \
		defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH channel message unexpectedly enabled network or crypto"
	#endif

	return xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelEofWrite(&Writer, 7u) == XSSH_OK) &&
		(xrtSshChannelEofRead(
			(xbytesview){ arrPayload, Writer.Size },
			&iRecipient
		) == XSSH_OK) && (iRecipient == 7u) ? 0 : 1;
}
