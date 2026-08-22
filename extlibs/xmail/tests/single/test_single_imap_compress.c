#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_IMAP_COMPRESS
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* IMAP COMPRESS 单头仅闭包客户端、传输和 raw-DEFLATE 依赖。 */
int main(void)
{
	ximapcompressconfig Config;

	#if !defined(XMAIL_FEATURE_IMAP_COMPRESS) || \
		!defined(XMAIL_FEATURE_IMAP_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) || \
		!defined(XRT_FEATURE_DEFLATE) || \
		!defined(XRT_FEATURE_INFLATE)
		#error "XMAIL_MODULE_IMAP_COMPRESS dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_IMAP_AUTH) || \
		defined(XMAIL_FEATURE_IMAP_COMMAND) || \
		defined(XMAIL_FEATURE_IMAP_APPEND) || \
		defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) || \
		defined(XMAIL_FEATURE_MAIL_NET_TLS)
		#error "XMAIL_MODULE_IMAP_COMPRESS retained unrelated features"
	#endif

	xrtImapCompressConfigInit(&Config);
	return xrtImapCompressConfigValid(&Config) ? 0 : 1;
}
