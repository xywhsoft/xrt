#ifdef XIMAP_MODULE_XIMAP
	#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_COMPRESS
#define XIMAP_IMPLEMENTATION
#include "../../single/ximap.h"



/* IMAP COMPRESS 单头仅闭包客户端、传输和 raw-DEFLATE 依赖。 */
int main(void)
{
	ximapcompressconfig Config;

	#if !defined(XIMAP_FEATURE_IMAP_COMPRESS) || \
		!defined(XIMAP_FEATURE_IMAP_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) || \
		!defined(XRT_FEATURE_DEFLATE) || \
		!defined(XRT_FEATURE_INFLATE)
		#error "XIMAP_MODULE_IMAP_COMPRESS dependency closure is incomplete"
	#endif
	#if defined(XIMAP_FEATURE_IMAP_AUTH) || \
		defined(XIMAP_FEATURE_IMAP_COMMAND) || \
		defined(XIMAP_FEATURE_IMAP_APPEND) || \
		defined(XIMAP_FEATURE_IMAP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS)
		#error "XIMAP_MODULE_IMAP_COMPRESS retained unrelated features"
	#endif

	xrtImapCompressConfigInit(&Config);
	return xrtImapCompressConfigValid(&Config) ? 0 : 1;
}
